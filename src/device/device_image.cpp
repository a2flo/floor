/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2025 Florian Ziesche
 *  
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; version 2 of the License only.
 *  
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *  
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <floor/floor.hpp>
#include <floor/device/device_image.hpp>
#include <floor/device/device.hpp>
#include <floor/device/device_context.hpp>
#include <floor/device/toolchain.hpp>
#include <floor/core/logger.hpp>
#include <floor/threading/task.hpp>
#include <chrono>
#include <cassert>

#if !defined(FLOOR_NO_METAL)
#include <floor/device/metal/metal_image.hpp>
#endif
#if !defined(FLOOR_NO_VULKAN)
#include <floor/device/vulkan/vulkan_image.hpp>
#endif

namespace fl {
using namespace std::literals;

safe_mutex device_image::minify_programs_mtx;
std::unordered_map<device_context*, std::unique_ptr<device_image::minify_program>> device_image::minify_programs;

std::pair<std::unique_ptr<uint8_t[]>, size_t> device_image::rgb_to_rgba(const IMAGE_TYPE& rgb_type,
																		const IMAGE_TYPE& rgba_type,
																		const std::span<const uint8_t> rgb_data,
																		const bool ignore_mip_levels) {
	// need to copy/convert the RGB host data to RGBA
	const auto rgba_size = image_data_size_from_types(image_dim, rgba_type, ignore_mip_levels);
	const auto rgb_bytes_per_pixel = image_bytes_per_pixel(rgb_type);
	const auto rgba_bytes_per_pixel = image_bytes_per_pixel(rgba_type);
	const auto pixel_count = rgba_size / rgba_bytes_per_pixel;
	assert(rgb_data.size_bytes() >= pixel_count * rgb_bytes_per_pixel);
	
	auto rgba_data_ptr = std::make_unique<uint8_t[]>(rgba_size);
	memset(rgba_data_ptr.get(), 0xFF, rgba_size); // opaque
	for (size_t i = 0; i < pixel_count; ++i) {
		memcpy(&rgba_data_ptr[i * rgba_bytes_per_pixel],
			   &rgb_data[i * rgb_bytes_per_pixel],
			   rgb_bytes_per_pixel);
	}
	return { std::move(rgba_data_ptr), rgba_size };
}

void device_image::rgb_to_rgba_inplace(const IMAGE_TYPE& rgb_type,
									   const IMAGE_TYPE& rgba_type,
									   std::span<uint8_t> rgb_to_rgba_data,
									   const bool ignore_mip_levels) {
	// need to copy/convert the RGB host data to RGBA
	const auto rgba_size = image_data_size_from_types(image_dim, rgba_type, ignore_mip_levels);
	const auto rgb_bytes_per_pixel = image_bytes_per_pixel(rgb_type);
	const auto rgba_bytes_per_pixel = image_bytes_per_pixel(rgba_type);
	const auto alpha_size = rgba_bytes_per_pixel / 4;
	const auto pixel_count = rgba_size / rgba_bytes_per_pixel;
	assert(rgb_to_rgba_data.size_bytes() >= pixel_count * rgba_bytes_per_pixel);
	
	// this needs to happen in reverse, otherwise we'd be overwriting the following RGB data
	for (size_t i = pixel_count - 1; ; --i) {
		for (size_t j = 0; j < rgb_bytes_per_pixel; ++j) {
			rgb_to_rgba_data[i * rgba_bytes_per_pixel + j] = rgb_to_rgba_data[i * rgb_bytes_per_pixel + j];
		}
		memset(&rgb_to_rgba_data[i * rgba_bytes_per_pixel] + rgba_bytes_per_pixel - alpha_size, 0xFF, alpha_size); // opaque
		
		if (i == 0) {
			break;
		}
	}
}

std::pair<std::unique_ptr<uint8_t[]>, size_t> device_image::rgba_to_rgb(const IMAGE_TYPE& rgba_type,
																		const IMAGE_TYPE& rgb_type,
																		const std::span<const uint8_t> rgba_data,
																		std::span<uint8_t> dst_rgb_data,
																		const bool ignore_mip_levels) {
	// need to copy/convert the RGB host data to RGBA
	const auto rgba_size = image_data_size_from_types(image_dim, rgba_type, ignore_mip_levels);
	const auto rgb_size = image_data_size_from_types(image_dim, rgb_type, ignore_mip_levels);
	const auto rgb_bytes_per_pixel = image_bytes_per_pixel(rgb_type);
	const auto rgba_bytes_per_pixel = image_bytes_per_pixel(rgba_type);
	const auto pixel_count = rgba_size / rgba_bytes_per_pixel;
	assert(rgb_size == pixel_count * rgb_bytes_per_pixel);
	assert(dst_rgb_data.data() == nullptr ||
		   dst_rgb_data.size_bytes() >= pixel_count * rgb_bytes_per_pixel);
	
	uint8_t* rgb_data_ptr = dst_rgb_data.data();
	std::unique_ptr<uint8_t[]> alloc_data_ptr;
	if (dst_rgb_data.data() == nullptr) {
		alloc_data_ptr = std::make_unique<uint8_t[]>(rgb_size);
		rgb_data_ptr = alloc_data_ptr.get();
	}
	for (size_t i = 0; i < pixel_count; ++i) {
		memcpy(&rgb_data_ptr[i * rgb_bytes_per_pixel],
			   &rgba_data[i * rgba_bytes_per_pixel],
			   rgb_bytes_per_pixel);
	}
	return { std::move(alloc_data_ptr), rgb_size };
}

} // namespace fl

// something about dog food
#if !defined(FLOOR_NO_HOST_COMPUTE)
#include <floor/device/backend/common.hpp>
#define FLOOR_DEVICE_HOST_COMPUTE_MINIFY 1 // needed now so that kernel code will actually be included
#else // when not using Host-Compute, set these two defines so depth images are actually supported on other backends
#define FLOOR_DEVICE_INFO_HAS_IMAGE_DEPTH_SUPPORT_1
#define FLOOR_DEVICE_INFO_HAS_IMAGE_DEPTH_WRITE_SUPPORT_1
#endif
#include <floor/device/backend/mip_map_minify.hpp>

namespace fl {

// embed the compiled mip-map-minify FUBAR file if it is available
#if __has_embed("../etc/mip_map_minify/mmm.fubar")
static constexpr const uint8_t mmm_fubar[] {
#embed "../etc/mip_map_minify/mmm.fubar"
};
#define FLOOR_HAS_EMBEDDED_MMM_FUBAR 1
#endif

bool device_image::add_embedded_minify_program([[maybe_unused]] device_context& ctx) {
#if defined(FLOOR_HAS_EMBEDDED_MMM_FUBAR)
	// try to load embedded mip-map minify programs/functions
	static_assert(std::size(mmm_fubar) > 0, "mmm.fubar is missing or empty");
	const std::span<const uint8_t> mmm_fubar_data{ mmm_fubar, std::size(mmm_fubar) };
	auto embedded_mmm_program = ctx.add_universal_binary(mmm_fubar_data);
	if (!embedded_mmm_program) {
		return false;
	}
	log_msg("using embedded mip-map minify FUBAR");
	
	// create the minify program for this context
	return device_image::provide_minify_program(ctx, embedded_mmm_program);
#else
	return false;
#endif
}

bool device_image::provide_minify_program(device_context& ctx, std::shared_ptr<device_program> prog) {
	auto minify_prog = std::make_unique<minify_program>();
	minify_prog->program = prog;
	
	// get all minification functions
	std::unordered_map<IMAGE_TYPE, std::pair<std::string, std::shared_ptr<device_function>>> minify_functions {
#define FLOOR_MINIFY_ENTRY(function_type, image_type, sample_type) \
		{ \
			IMAGE_TYPE::image_type | IMAGE_TYPE::sample_type, \
			{ "libfloor_mip_map_minify_" #image_type "_" #sample_type , {} } \
		},
		
		FLOOR_MINIFY_IMAGE_TYPES(FLOOR_MINIFY_ENTRY)
	};
	
	// drop depth image function (handling) if there is no depth image support
	for (const auto& dev : ctx.get_devices()) {
		if (!dev->image_depth_support ||
			!dev->image_depth_write_support ||
			// TODO: Vulkan support
			ctx.get_platform_type() == PLATFORM_TYPE::VULKAN) {
			minify_functions.erase(IMAGE_TYPE::IMAGE_DEPTH | IMAGE_TYPE::FLOAT);
			minify_functions.erase(IMAGE_TYPE::IMAGE_DEPTH_ARRAY | IMAGE_TYPE::FLOAT);
		}
	}
	
	for (auto& entry : minify_functions) {
		entry.second.second = minify_prog->program->get_function(entry.second.first);
		if (entry.second.second == nullptr) {
			log_error("failed to retrieve function \"$\" from minify program", entry.second.first);
			return false;
		}
	}
	minify_functions.swap(minify_prog->functions);
	
	// done, set programs for this context and return
	GUARD(minify_programs_mtx);
	minify_programs[&ctx] = std::move(minify_prog);
	return true;
}

void device_image::build_mip_map_minification_program() const {
	// build mip-map minify functions (do so in a separate thread so that we don't hold up anything)
	task::spawn([ctx = dev.context]() {
		const toolchain::compile_options options {
			// suppress any debug output for this, we only want to have console/log output if something goes wrong
			.silence_debug_output = true
		};
		
		std::string base_path;
		switch (ctx->get_platform_type()) {
			case PLATFORM_TYPE::CUDA:
				base_path = floor::get_cuda_base_path();
				break;
			case PLATFORM_TYPE::OPENCL:
				base_path = floor::get_opencl_base_path();
				break;
			case PLATFORM_TYPE::VULKAN:
				base_path = floor::get_vulkan_base_path();
				break;
			case PLATFORM_TYPE::HOST:
				// doesn't matter
				break;
			default:
				log_error("backend does not support (or need) mip-map minification functions");
				return;
		}
		
		auto program = ctx->add_program_file(base_path + "include/floor/device/backend/mip_map_minify.hpp", options);
		if (!program) {
			log_error("failed to build minify functions");
			return;
		}
		
		provide_minify_program(*ctx, program);
		
		// TODO: safely/cleanly hold up device_context destruction if the program exits before this is built
	}, "minify build");
}

void device_image::generate_mip_map_chain(const device_queue& cqueue) {
#if defined(FLOOR_HAS_EMBEDDED_MMM_FUBAR)
	// load the embedded mip-map minify program/FUBAR for each context that gets here
	static safe_mutex did_add_embedded_program_lock;
	// this signals whether we have already tried loading the FUBAR and if it was successful or not
	static std::unordered_map<device_context*, bool> did_add_embedded_program GUARDED_BY(did_add_embedded_program_lock);
	{
		auto ctx = cqueue.get_device().context;
		GUARD(did_add_embedded_program_lock);
		if (!did_add_embedded_program.contains(ctx)) {
			did_add_embedded_program[ctx] = add_embedded_minify_program(*ctx);
		}
	}
#endif
	
	const device_function* minify_function = nullptr;
	for (uint32_t try_out = 0; ; ++try_out) {
		if (try_out == 100) {
			log_error("mip-map minify function compilation is stuck?");
			return;
		}
		if (try_out > 0) {
			std::this_thread::sleep_for(50ms);
		}
		
		// get the compiled program for this context
		minify_programs_mtx.lock();
		const auto iter = minify_programs.find(dev.context);
		if (iter == minify_programs.end()) {
			// kick off build + insert nullptr value to signal build has started
			build_mip_map_minification_program();
			minify_programs.emplace(dev.context, nullptr);
			
			minify_programs_mtx.unlock();
			continue;
		}
		if (iter->second == nullptr) {
			// still building
			minify_programs_mtx.unlock();
			continue;
		}
		const auto prog = iter->second.get();
		minify_programs_mtx.unlock();
		
		// find the appropriate function for this image type
		const auto image_base_type = minify_image_base_type(image_type);
		const auto function_iter = prog->functions.find(image_base_type);
		if (function_iter == prog->functions.end()) {
			log_error("no minification function for this image type exists: $ ($X)", image_type_to_string(image_type), image_type);
			return;
		}
		minify_function = function_iter->second.second.get();
		break;
	}
	assert(minify_function);
	
	// run the function for this image
	const auto dim_count = image_dim_count(image_type);
	uint3 lsize;
	switch (dim_count) {
		case 1:
			lsize = { dev.max_total_local_size, 1, 1 };
			break;
		case 2:
			lsize = { (dev.max_total_local_size > 256 ? 32 : 16), (dev.max_total_local_size > 512 ? 32 : 16), 1 };
			break;
		default:
		case 3:
			lsize = { (dev.max_total_local_size > 512 ? 32 : 16), (dev.max_total_local_size > 256 ? 16 : 8), 2 };
			break;
	}
	for (uint32_t layer = 0; layer < layer_count; ++layer) {
		uint3 level_size {
			image_dim.x,
			dim_count >= 2 ? image_dim.y : 0u,
			dim_count >= 3 ? image_dim.z : 0u,
		};
		float3 inv_prev_level_size;
		for (uint32_t level = 0; level < mip_level_count;
			 ++level, inv_prev_level_size = 1.0f / float3(level_size), level_size >>= 1) {
			if (level == 0) {
				continue;
			}
			device_queue::execution_parameters_t exec_params {
				.execution_dim = dim_count,
				.global_work_size = level_size.rounded_next_multiple(lsize),
				.local_work_size = lsize,
				.args = { (device_image*)this, level_size, inv_prev_level_size, level, layer },
				// NOTE: must be blocking, because each level depends on the previous level
				.wait_until_completion = true,
				.debug_label = "mip_map_minify",
			};
			cqueue.execute_with_parameters(*minify_function, exec_params);
		}
	}
}

std::string device_image::image_type_to_string(const IMAGE_TYPE& type) {
	std::stringstream ret;
	
	// base type
	const auto dim = image_dim_count(type);
	const auto is_array = has_flag<IMAGE_TYPE::FLAG_ARRAY>(type);
	const auto is_buffer = has_flag<IMAGE_TYPE::FLAG_BUFFER>(type);
	const auto is_msaa = has_flag<IMAGE_TYPE::FLAG_MSAA>(type);
	const auto is_cube = has_flag<IMAGE_TYPE::FLAG_CUBE>(type);
	const auto is_depth = has_flag<IMAGE_TYPE::FLAG_DEPTH>(type);
	const auto is_stencil = has_flag<IMAGE_TYPE::FLAG_STENCIL>(type);
	
	if (!is_cube) {
		if (dim > 0) {
			ret << dim << "D ";
		}
	} else {
		ret << "Cube ";
	}
	
	if(is_depth) ret << "Depth ";
	if(is_stencil) ret << "Stencil ";
	if(is_msaa) {
		ret << image_sample_count(type) << "xMSAA ";
	}
	if(is_array) ret << "Array ";
	if(is_buffer) ret << "Buffer ";
	
	const auto aniso = image_anisotropy(type);
	if (aniso > 1) {
		ret << aniso << "xAniso";
	}
	
	// channel count and layout
	const auto channel_count = image_channel_count(type);
	const auto layout_idx = (uint32_t(type & IMAGE_TYPE::__LAYOUT_MASK) >> uint32_t(IMAGE_TYPE::__LAYOUT_SHIFT));
	static const char layout_strs[4][5] {
		"RGBA",
		"BGRA",
		"ABGR",
		"ARGB",
	};
	for(uint32_t i = 0; i < channel_count; ++i) {
		ret << layout_strs[layout_idx][i];
	}
	ret << " ";
	if(has_flag<IMAGE_TYPE::FLAG_SRGB>(type)) ret << "sRGB ";
	
	// data type
	if(has_flag<IMAGE_TYPE::FLAG_NORMALIZED>(type)) ret << "normalized ";
	switch(type & IMAGE_TYPE::__DATA_TYPE_MASK) {
		case IMAGE_TYPE::INT: ret << "int "; break;
		case IMAGE_TYPE::UINT: ret << "uint "; break;
		case IMAGE_TYPE::FLOAT: ret << "float "; break;
		default: ret << "INVALID-DATA-TYPE "; break;
	}
	
	// access
	if(has_flag<IMAGE_TYPE::READ_WRITE>(type)) ret << "read/write ";
	else if(has_flag<IMAGE_TYPE::READ>(type)) ret << "read-only ";
	else if(has_flag<IMAGE_TYPE::WRITE>(type)) ret << "write-only ";
	
	// compression
	if(image_compressed(type)) {
		switch(type & IMAGE_TYPE::__COMPRESSION_MASK) {
			case IMAGE_TYPE::BC1: ret << "BC1 "; break;
			case IMAGE_TYPE::BC2: ret << "BC2 "; break;
			case IMAGE_TYPE::BC3: ret << "BC3 "; break;
			case IMAGE_TYPE::RGTC: ret << "RGTC/BC4/BC5 "; break;
			case IMAGE_TYPE::BPTC: ret << "BPTC/BC6/BC7 "; break;
			case IMAGE_TYPE::PVRTC: ret << "PVRTC "; break;
			case IMAGE_TYPE::PVRTC2: ret << "PVRTC2 "; break;
			case IMAGE_TYPE::EAC: ret << "EAC "; break;
			case IMAGE_TYPE::ETC2: ret << "ETC2 "; break;
			case IMAGE_TYPE::ASTC: ret << "ASTC "; break;
			default: ret << "INVALID-COMPRESSION-TYPE "; break;
		}
	}
	
	// format
	switch(type & IMAGE_TYPE::__FORMAT_MASK) {
		case IMAGE_TYPE::FORMAT_1: ret << "1bpc"; break;
		case IMAGE_TYPE::FORMAT_2: ret << "2bpc"; break;
		case IMAGE_TYPE::FORMAT_3_3_2: ret << "3/3/2"; break;
		case IMAGE_TYPE::FORMAT_4: ret << "4bpc"; break;
		case IMAGE_TYPE::FORMAT_4_2_0: ret << "4/2/0"; break;
		case IMAGE_TYPE::FORMAT_4_1_1: ret << "4/1/1"; break;
		case IMAGE_TYPE::FORMAT_4_2_2: ret << "4/2/2"; break;
		case IMAGE_TYPE::FORMAT_5_5_5: ret << "5/5/5"; break;
		case IMAGE_TYPE::FORMAT_5_5_5_ALPHA_1: ret << "5/5/5/A1"; break;
		case IMAGE_TYPE::FORMAT_5_6_5: ret << "5/6/5"; break;
		case IMAGE_TYPE::FORMAT_8: ret << "8bpc"; break;
		case IMAGE_TYPE::FORMAT_9_9_9_EXP_5: ret << "9/9/9/E5"; break;
		case IMAGE_TYPE::FORMAT_10: ret << "10/10/10"; break;
		case IMAGE_TYPE::FORMAT_10_10_10_ALPHA_2: ret << "10/10/10/A2"; break;
		case IMAGE_TYPE::FORMAT_11_11_10: ret << "11/11/10"; break;
		case IMAGE_TYPE::FORMAT_12_12_12: ret << "12/12/12"; break;
		case IMAGE_TYPE::FORMAT_12_12_12_12: ret << "12/12/12/12"; break;
		case IMAGE_TYPE::FORMAT_16: ret << "16bpc"; break;
		case IMAGE_TYPE::FORMAT_16_8: ret << "16/8"; break;
		case IMAGE_TYPE::FORMAT_24: ret << "24"; break;
		case IMAGE_TYPE::FORMAT_24_8: ret << "24/8"; break;
		case IMAGE_TYPE::FORMAT_32: ret << "32bpc"; break;
		case IMAGE_TYPE::FORMAT_32_8: ret << "32/8"; break;
		case IMAGE_TYPE::FORMAT_64: ret << "64bpc"; break;
		default: ret << "INVALID-FORMAT"; break;
	}
	
	// other
	if(has_flag<IMAGE_TYPE::FLAG_MIPMAPPED>(type)) ret << " mip-mapped";
	if(has_flag<IMAGE_TYPE::FLAG_TRANSIENT>(type)) ret << " transient";
	if(has_flag<IMAGE_TYPE::FLAG_RENDER_TARGET>(type)) ret << " render-target";
	
	return ret.str();
}

std::shared_ptr<device_image> device_image::clone(const device_queue& cqueue, const bool copy_contents,
											   const MEMORY_FLAG flags_override,
											   const IMAGE_TYPE image_type_override) {
	if (dev.context == nullptr) {
		log_error("invalid image/device state");
		return {};
	}
	
	auto clone_flags = (flags_override != MEMORY_FLAG::NONE ? flags_override : flags);
	if (host_data.data() != nullptr) {
		// never copy host data on the newly created image
		clone_flags |= MEMORY_FLAG::NO_INITIAL_COPY;
	}
	
	auto ret = dev.context->create_image(cqueue, image_dim, (image_type_override == IMAGE_TYPE::NONE ? image_type : image_type_override),
										 host_data, clone_flags);
	if (ret == nullptr) {
		return {};
	}
	
	if (copy_contents) {
		ret->blit(cqueue, *this);
	}
	
	return ret;
}

bool device_image::blit_check(const device_queue&, const device_image& src) {
	const auto src_image_dim = src.get_image_dim();
	if ((src_image_dim != image_dim).any()) {
		log_error("blit: dim mismatch: src $ != dst $", src_image_dim, image_dim);
		return false;
	}
	
	const auto src_layer_count = src.get_layer_count();
	if (src_layer_count != layer_count) {
		log_error("blit: layer count mismatch: src $ != dst $", src_layer_count, layer_count);
		return false;
	}
	
	const auto src_data_size = src.get_image_data_size();
	if (src_data_size != image_data_size) {
		log_error("blit: size mismatch: src $ != dst $", src_data_size, image_data_size);
		return false;
	}
	
	const auto src_format = image_format(src.get_image_type());
	const auto dst_format = image_format(image_type);
	if (src_format != dst_format) {
		log_error("blit: format mismatch ($ != $)", src_format, dst_format);
		return false;
	}
	
	if (image_compressed(image_type) || image_compressed(src.get_image_type())) {
		log_error("blit: blitting of compressed formats is not supported");
		return false;
	}
	
	return true;
}

void device_image::destroy_minify_programs() {
	GUARD(minify_programs_mtx);
	minify_programs.clear();
}

metal_image* device_image::get_underlying_metal_image_safe() {
	if (has_flag<MEMORY_FLAG::METAL_SHARING>(flags)) {
		metal_image* ret = get_shared_metal_image();
		if (ret) {
			if (has_flag<MEMORY_FLAG::SHARING_SYNC>(flags)) {
				// -> release from compute use, acquire for Metal use
				release_metal_image(nullptr, nullptr);
			} else if (has_flag<MEMORY_FLAG::METAL_SHARING_SYNC_SHARED>(flags)) {
				sync_metal_image(nullptr, nullptr);
			}
		} else {
			ret = (metal_image*)this;
		}
#if defined(FLOOR_DEBUG) && !defined(FLOOR_NO_METAL)
		if (auto test_cast_mtl_image = dynamic_cast<metal_image*>(ret); !test_cast_mtl_image) {
			throw std::runtime_error("specified image is neither a Metal image nor a shared Metal image");
		}
#endif
		return ret;
	}
	return (metal_image*)this;
}

const metal_image* device_image::get_underlying_metal_image_safe() const {
	if (has_flag<MEMORY_FLAG::METAL_SHARING>(flags)) {
		const metal_image* ret = get_shared_metal_image();
		if (ret) {
			if (has_flag<MEMORY_FLAG::SHARING_SYNC>(flags)) {
				// -> release from compute use, acquire for Metal use
				release_metal_image(nullptr, nullptr);
			} else if (has_flag<MEMORY_FLAG::METAL_SHARING_SYNC_SHARED>(flags)) {
				sync_metal_image(nullptr, nullptr);
			}
		} else {
			ret = (const metal_image*)this;
		}
#if defined(FLOOR_DEBUG) && !defined(FLOOR_NO_METAL)
		if (auto test_cast_mtl_image = dynamic_cast<const metal_image*>(ret); !test_cast_mtl_image) {
			throw std::runtime_error("specified image is neither a Metal image nor a shared Metal image");
		}
#endif
		return ret;
	}
	return (const metal_image*)this;
}

vulkan_image* device_image::get_underlying_vulkan_image_safe() {
	if (has_flag<MEMORY_FLAG::VULKAN_SHARING>(flags)) {
		vulkan_image* ret = get_shared_vulkan_image();
		if (ret) {
			if (has_flag<MEMORY_FLAG::SHARING_SYNC>(flags)) {
				// -> release from compute use, acquire for Vulkan use
				release_vulkan_image(nullptr, nullptr);
			} else if (has_flag<MEMORY_FLAG::VULKAN_SHARING_SYNC_SHARED>(flags)) {
				sync_vulkan_image(nullptr, nullptr);
			}
		} else {
			ret = (vulkan_image*)this;
		}
#if defined(FLOOR_DEBUG) && !defined(FLOOR_NO_VULKAN)
		if (auto test_cast_vk_image = dynamic_cast<vulkan_image*>(ret); !test_cast_vk_image) {
			throw std::runtime_error("specified image is neither a Vulkan image nor a shared Vulkan image");
		}
#endif
		return ret;
	}
	return (vulkan_image*)this;
}

const vulkan_image* device_image::get_underlying_vulkan_image_safe() const {
	if (has_flag<MEMORY_FLAG::VULKAN_SHARING>(flags)) {
		const vulkan_image* ret = get_shared_vulkan_image();
		if (ret) {
			if (has_flag<MEMORY_FLAG::SHARING_SYNC>(flags)) {
				// -> release from compute use, acquire for Vulkan use
				release_vulkan_image(nullptr, nullptr);
			} else if (has_flag<MEMORY_FLAG::VULKAN_SHARING_SYNC_SHARED>(flags)) {
				sync_vulkan_image(nullptr, nullptr);
			}
		} else {
			ret = (const vulkan_image*)this;
		}
#if defined(FLOOR_DEBUG) && !defined(FLOOR_NO_VULKAN)
		if (auto test_cast_vk_image = dynamic_cast<const vulkan_image*>(ret); !test_cast_vk_image) {
			throw std::runtime_error("specified image is neither a Vulkan image nor a shared Vulkan image");
		}
#endif
		return ret;
	}
	return (const vulkan_image*)this;
}

} // namespace fl
