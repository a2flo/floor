/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2026 Florian Ziesche
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

#pragma once

#include <floor/device/toolchain.hpp>
#include <floor/device/metal/metal4_buffer.hpp>
#include <floor/device/metal/metal4_image.hpp>
#include <floor/device/metal/metal4_argument_buffer.hpp>
#include <floor/device/metal/metal4_function_entry.hpp>
#include <floor/device/metal/metal4_resource_tracking.hpp>
#include <Metal/MTL4ArgumentTable.h>
#include <Metal/MTL4ComputeCommandEncoder.h>
#include <Metal/MTL4RenderCommandEncoder.h>
#include <Metal/MTLArgumentEncoder.h>
#include <span>

//! Metal argument handling
//! NOTE: do not include manually
namespace fl::metal4_args {
using namespace toolchain;

enum class ENCODER_TYPE {
	ARGUMENT_BUFFER,
	ARGUMENT_TABLE,
	INDIRECT_SHADER,
	INDIRECT_COMPUTE,
};

struct argument_table_encoder_t {
	id <MTL4ArgumentTable> arg_table { nil };
	device_buffer* constants_buffer { nullptr };
	const fl::flat_map<uint32_t, metal4_constant_buffer_info_t>* constant_buffer_info { nullptr };
};

template <uint32_t count>
struct argument_table_encoders_t {
	std::array<argument_table_encoder_t, count> encoders;
};

template <ENCODER_TYPE enc_type>
using encoder_selector_t =
	std::conditional_t<(enc_type == ENCODER_TYPE::ARGUMENT_TABLE), argument_table_encoder_t&,
	std::conditional_t<(enc_type == ENCODER_TYPE::ARGUMENT_BUFFER), id <MTLArgumentEncoder>,
	std::conditional_t<(enc_type == ENCODER_TYPE::INDIRECT_COMPUTE), id <MTLIndirectComputeCommand>,
	std::conditional_t<(enc_type == ENCODER_TYPE::INDIRECT_SHADER), id <MTLIndirectRenderCommand>,
					   void>>>>;

struct idx_handler {
	//! actual argument index (directly corresponding to the C++ source code)
	uint32_t arg { 0 };
	//! flag if this is an implicit arg
	bool is_implicit { false };
	//! current implicit argument index
	uint32_t implicit { 0 };
	//! current buffer index
	uint32_t buffer_idx { 0 };
	//! current texture index
	uint32_t texture_idx { 0 };
	//! current function entry
	uint32_t entry { 0 };
};

//! return the argument buffer index for the specified buffer index
static inline uint32_t arg_buffer_index(const idx_handler& idx, const std::vector<uint32_t>* arg_buffer_indices) {
	if (arg_buffer_indices) {
		if (idx.arg < arg_buffer_indices->size()) {
			return (*arg_buffer_indices)[idx.arg];
		}
#if defined(FLOOR_DEBUG)
		log_error("arg index $ > size of arg buffer indices $", idx.arg, arg_buffer_indices->size());
#endif
	}
	return idx.buffer_idx;
}

//! actual function argument setters
template <ENCODER_TYPE enc_type>
requires (enc_type == ENCODER_TYPE::ARGUMENT_BUFFER)
static inline void set_argument(const idx_handler& idx,
								encoder_selector_t<enc_type> encoder,
								const void* ptr, const size_t& size,
								const std::vector<uint32_t>* arg_buffer_indices) {
	memcpy([encoder constantDataAtIndex:arg_buffer_index(idx, arg_buffer_indices)], ptr, size);
}
template <ENCODER_TYPE enc_type>
requires (enc_type == ENCODER_TYPE::ARGUMENT_TABLE)
static inline void set_argument(const idx_handler& idx,
								encoder_selector_t<enc_type> encoder,
								const void* ptr, const size_t& size,
								[[maybe_unused]] const std::vector<uint32_t>* arg_buffer_indices) {
	// update constants buffer memory in range of constant
	assert(encoder.constants_buffer);
	const auto& const_buffer_info = encoder.constant_buffer_info->at(idx.arg);
	assert(const_buffer_info.size == size);
#if defined(FLOOR_DEBUG)
	if (const_buffer_info.offset + const_buffer_info.size > encoder.constants_buffer->get_size()) {
		throw std::runtime_error("out-of-bounds constants buffer write");
	}
#endif
	const metal4_buffer* mtl_buffer = encoder.constants_buffer->get_underlying_metal4_buffer_safe();
	auto mtl_buffer_obj = mtl_buffer->get_metal_buffer();
	memcpy((uint8_t*)[mtl_buffer_obj contents] + const_buffer_info.offset, ptr, size);
	// NOTE: arg table info itself has already been set on init
	assert(idx.buffer_idx == const_buffer_info.buffer_idx);
}

template <ENCODER_TYPE enc_type>
static inline void set_argument(const idx_handler& idx,
								encoder_selector_t<enc_type> encoder,
								const function_info& entry,
								const device_buffer* arg,
								const std::vector<uint32_t>* arg_buffer_indices,
								metal4_resource_container_t& res_info) {
	const auto mtl_buffer = arg->get_underlying_metal4_buffer_safe();
	auto mtl_buffer_obj = mtl_buffer->get_metal_buffer();
	
	if constexpr (enc_type == ENCODER_TYPE::ARGUMENT_TABLE) {
		[encoder.arg_table setAddress:mtl_buffer_obj.gpuAddress
							  atIndex:idx.buffer_idx];
	} else if constexpr (enc_type == ENCODER_TYPE::ARGUMENT_BUFFER) {
		[encoder setBuffer:mtl_buffer_obj
					offset:0
				   atIndex:arg_buffer_index(idx, arg_buffer_indices)];
	} else if constexpr (enc_type == ENCODER_TYPE::INDIRECT_COMPUTE) {
		[encoder setKernelBuffer:mtl_buffer_obj
						  offset:0
						 atIndex:idx.buffer_idx];
	} else if constexpr (enc_type == ENCODER_TYPE::INDIRECT_SHADER) {
		if (entry.type == FUNCTION_TYPE::VERTEX) {
			[encoder setVertexBuffer:mtl_buffer_obj
							  offset:0
							 atIndex:idx.buffer_idx];
		} else {
			[encoder setFragmentBuffer:mtl_buffer_obj
								offset:0
							   atIndex:idx.buffer_idx];
		}
	}
	
	if (!mtl_buffer->is_heap_allocated()) {
		res_info.add_resource(mtl_buffer_obj);
	}
}

template <ENCODER_TYPE enc_type, typename buffer_array_elem_type>
requires (enc_type == ENCODER_TYPE::ARGUMENT_BUFFER)
static inline void set_buffer_array(const idx_handler& idx,
									encoder_selector_t<enc_type> encoder,
									const std::span<buffer_array_elem_type> arg,
									[[maybe_unused]] const device& dev,
									const std::vector<uint32_t>* arg_buffer_indices,
									metal4_resource_container_t& res_info) {
	const auto count = arg.size();
	if (count < 1) {
		return;
	}
	
	std::vector<id <MTLBuffer>> mtl_buf_array(count, nil);
	std::vector<id <MTLAllocation> __unsafe_unretained> mtl_buf_array_for_res_info;
	mtl_buf_array_for_res_info.reserve(count);
	std::vector<NSUInteger> offsets(count, 0);
	for (size_t i = 0; i < count; ++i) {
		if (arg[i]) {
			const auto mtl_buffer = arg[i]->get_underlying_metal4_buffer_safe();
			mtl_buf_array[i] = mtl_buffer->get_metal_buffer();
			if (!mtl_buffer->is_heap_allocated()) {
				mtl_buf_array_for_res_info.emplace_back(mtl_buf_array[i]);
			}
		}
	}
	
	if constexpr (enc_type == ENCODER_TYPE::ARGUMENT_TABLE) {
		[encoder.arg_table setBuffers:mtl_buf_array.data()
							  offsets:offsets.data()
							withRange:NSRange { arg_buffer_index(idx, arg_buffer_indices), count }];
	} else {
		[encoder setBuffers:mtl_buf_array.data()
					offsets:offsets.data()
				  withRange:NSRange { arg_buffer_index(idx, arg_buffer_indices), count }];
	}
	
	if (!mtl_buf_array_for_res_info.empty()) {
		res_info.add_resources(mtl_buf_array_for_res_info);
	}
}

template <ENCODER_TYPE enc_type>
requires (enc_type == ENCODER_TYPE::ARGUMENT_BUFFER)
static inline void set_argument(const idx_handler& idx,
								encoder_selector_t<enc_type> encoder,
								const std::span<const std::shared_ptr<device_buffer>> arg,
								const device& dev,
								const std::vector<uint32_t>* arg_buffer_indices,
								metal4_resource_container_t& res_info) {
	set_buffer_array<enc_type, const std::shared_ptr<device_buffer>>(idx, encoder, arg, dev, arg_buffer_indices, res_info);
}

template <ENCODER_TYPE enc_type>
requires (enc_type == ENCODER_TYPE::ARGUMENT_BUFFER)
static inline void set_argument(const idx_handler& idx,
								encoder_selector_t<enc_type> encoder,
								const std::span<const device_buffer* const> arg,
								const device& dev,
								const std::vector<uint32_t>* arg_buffer_indices,
								metal4_resource_container_t& res_info) {
	set_buffer_array<enc_type, const device_buffer* const>(idx, encoder, arg, dev, arg_buffer_indices, res_info);
}

template <ENCODER_TYPE enc_type>
static inline void set_argument(const idx_handler& idx,
								encoder_selector_t<enc_type> encoder,
								const function_info& entry,
								const metal4_argument_buffer* arg_buf,
								const std::vector<uint32_t>* arg_buffer_indices,
								metal4_resource_container_t& res_info,
								const bool include_arg_buffer_resources) {
	const auto buf = arg_buf->get_storage_buffer();
	const metal4_buffer* mtl_buffer = buf->get_underlying_metal4_buffer_safe();
	auto mtl_arg_buffer_obj = mtl_buffer->get_metal_buffer();
	if constexpr (enc_type == ENCODER_TYPE::ARGUMENT_TABLE) {
		[encoder.arg_table setAddress:mtl_arg_buffer_obj.gpuAddress
							  atIndex:idx.buffer_idx];
	} else if constexpr (enc_type == ENCODER_TYPE::ARGUMENT_BUFFER) {
		[encoder setBuffer:mtl_arg_buffer_obj
					offset:0
				   atIndex:arg_buffer_index(idx, arg_buffer_indices)];
	} else if constexpr (enc_type == ENCODER_TYPE::INDIRECT_COMPUTE) {
		[encoder setKernelBuffer:mtl_arg_buffer_obj
						  offset:0
						 atIndex:idx.buffer_idx];
	} else if constexpr (enc_type == ENCODER_TYPE::INDIRECT_SHADER) {
		if (entry.type == FUNCTION_TYPE::VERTEX) {
			[encoder setVertexBuffer:mtl_arg_buffer_obj
							  offset:0
							 atIndex:idx.buffer_idx];
		} else {
			[encoder setFragmentBuffer:mtl_arg_buffer_obj
								offset:0
							   atIndex:idx.buffer_idx];
		}
	}
	
	if (!mtl_buffer->is_heap_allocated()) {
		res_info.add_resource(mtl_arg_buffer_obj);
	}
	
	// include all argument buffer resources if specified
	if constexpr (enc_type == ENCODER_TYPE::ARGUMENT_TABLE ||
				  enc_type == ENCODER_TYPE::INDIRECT_COMPUTE ||
				  enc_type == ENCODER_TYPE::INDIRECT_SHADER) {
		if (include_arg_buffer_resources) {
			res_info.add_resources(arg_buf->get_resources());
		}
	}
}

static constexpr bool image_type_match(floor_unused_if_release const ARG_IMAGE_TYPE ct_image_type,
									   floor_unused_if_release const IMAGE_TYPE rt_image_type) {
#if defined(FLOOR_DEBUG)
	switch (ct_image_type) {
		case ARG_IMAGE_TYPE::NONE:
			return false;
		case ARG_IMAGE_TYPE::IMAGE_1D:
			return is_image_1d(rt_image_type);
		case ARG_IMAGE_TYPE::IMAGE_1D_ARRAY:
			return is_image_1d_array(rt_image_type);
		case ARG_IMAGE_TYPE::IMAGE_1D_BUFFER:
			return is_image_1d_buffer(rt_image_type);
		case ARG_IMAGE_TYPE::IMAGE_2D:
			return is_image_2d(rt_image_type);
		case ARG_IMAGE_TYPE::IMAGE_2D_ARRAY:
			return is_image_2d_array(rt_image_type);
		case ARG_IMAGE_TYPE::IMAGE_2D_DEPTH:
			return is_image_depth(rt_image_type);
		case ARG_IMAGE_TYPE::IMAGE_2D_ARRAY_DEPTH:
			return is_image_depth_array(rt_image_type);
		case ARG_IMAGE_TYPE::IMAGE_2D_MSAA:
			return is_image_2d_msaa(rt_image_type);
		case ARG_IMAGE_TYPE::IMAGE_2D_ARRAY_MSAA:
			return is_image_2d_msaa_array(rt_image_type);
		case ARG_IMAGE_TYPE::IMAGE_2D_MSAA_DEPTH:
			return is_image_depth_msaa(rt_image_type);
		case ARG_IMAGE_TYPE::IMAGE_2D_ARRAY_MSAA_DEPTH:
			return is_image_depth_msaa_array(rt_image_type);
		case ARG_IMAGE_TYPE::IMAGE_3D:
			return is_image_3d(rt_image_type);
		case ARG_IMAGE_TYPE::IMAGE_CUBE:
			return is_image_cube(rt_image_type);
		case ARG_IMAGE_TYPE::IMAGE_CUBE_ARRAY:
			return is_image_cube_array(rt_image_type);
		case ARG_IMAGE_TYPE::IMAGE_CUBE_DEPTH:
			return is_image_depth_cube(rt_image_type);
		case ARG_IMAGE_TYPE::IMAGE_CUBE_ARRAY_DEPTH:
			return is_image_depth_cube_array(rt_image_type);
	}
	floor_unreachable();
#else
	return true;
#endif
}

template <ENCODER_TYPE enc_type>
static inline void set_argument(const idx_handler& idx,
								encoder_selector_t<enc_type> encoder,
								const function_info& entry,
								const device_image* arg,
								metal4_resource_container_t& res_info) {
	if constexpr (enc_type == ENCODER_TYPE::INDIRECT_COMPUTE || enc_type == ENCODER_TYPE::INDIRECT_SHADER) {
#if defined(FLOOR_DEBUG)
		log_error("can not encode an image into an indirect compute/render command");
#endif
		return;
	}
	
	const auto mtl_image = arg->get_underlying_metal4_image_safe();
	assert(image_type_match(entry.args[idx.arg].image_type, arg->get_image_type()));
	auto mtl_image_obj = mtl_image->get_metal_image();
	if constexpr (enc_type == ENCODER_TYPE::ARGUMENT_TABLE) {
		[encoder.arg_table setTexture:mtl_image_obj.gpuResourceID
							  atIndex:idx.texture_idx];
	} else if constexpr (enc_type == ENCODER_TYPE::ARGUMENT_BUFFER) {
		[encoder setTexture:mtl_image_obj
					atIndex:idx.texture_idx];
	}
	
	// if this is a read/write image, add it again (one is read-only, the other is write-only)
	if (entry.args[idx.arg].access == ARG_ACCESS::READ_WRITE) {
		if constexpr (enc_type == ENCODER_TYPE::ARGUMENT_TABLE) {
			[encoder.arg_table setTexture:mtl_image_obj.gpuResourceID
								  atIndex:(idx.texture_idx + 1)];
		} else if constexpr (enc_type == ENCODER_TYPE::ARGUMENT_BUFFER) {
			[encoder setTexture:mtl_image_obj
						atIndex:(idx.texture_idx + 1)];
		}
	}
	
	if (!mtl_image->is_heap_allocated()) {
		res_info.add_resource(mtl_image_obj);
	}
}

template <ENCODER_TYPE enc_type, typename image_array_elem_type>
static inline void set_argument_image_array(const idx_handler& idx,
											encoder_selector_t<enc_type> encoder,
											const function_info& entry,
											const std::span<image_array_elem_type> arg,
											metal4_resource_container_t& res_info) {
	if constexpr (enc_type == ENCODER_TYPE::INDIRECT_COMPUTE || enc_type == ENCODER_TYPE::INDIRECT_SHADER) {
#if defined(FLOOR_DEBUG)
		log_error("can not encode images into an indirect compute/render command");
#endif
		return;
	}
	
	const auto count = arg.size();
	if (count < 1) {
		return;
	}
	
	std::vector<id <MTLTexture>> mtl_img_array(count, nil);
	std::vector<id <MTLAllocation> __unsafe_unretained> mtl_img_array_for_res_info;
	mtl_img_array_for_res_info.reserve(count);
	for (size_t i = 0; i < count; ++i) {
		const metal4_image* mtl_image = (arg[i] ? arg[i]->get_underlying_metal4_image_safe() : nullptr);
		assert(!arg[i] || (arg[i] && image_type_match(entry.args[idx.arg].image_type, arg[i]->get_image_type())));
		mtl_img_array[i] = (mtl_image ? mtl_image->get_metal_image() : nil);
		if (mtl_image && !mtl_image->is_heap_allocated()) {
			mtl_img_array_for_res_info.emplace_back(mtl_img_array[i]);
		}
	}
	
	if constexpr (enc_type == ENCODER_TYPE::ARGUMENT_BUFFER) {
		[encoder setTextures:mtl_img_array.data()
				   withRange:NSRange { idx.texture_idx, count }];
	} else if constexpr (enc_type == ENCODER_TYPE::ARGUMENT_TABLE) {
		// NOTE: no setTextures available in MTL4ArgumentTable
		for (uint32_t tex_idx = idx.texture_idx, i = 0; i < count; ++i, ++tex_idx) {
			[encoder.arg_table setTexture:mtl_img_array[i].gpuResourceID
								  atIndex:tex_idx];
		}
	}

	if (!mtl_img_array_for_res_info.empty()) {
		res_info.add_resources(mtl_img_array_for_res_info);
	}
}

template <ENCODER_TYPE enc_type>
static inline void set_argument(const idx_handler& idx,
								encoder_selector_t<enc_type> encoder,
								const function_info& entry,
								const std::span<const std::shared_ptr<device_image>> arg,
								metal4_resource_container_t& res_info) {
	set_argument_image_array<enc_type, const std::shared_ptr<device_image>>(idx, encoder, entry, arg, res_info);
}

template <ENCODER_TYPE enc_type>
static inline void set_argument(const idx_handler& idx,
								encoder_selector_t<enc_type> encoder,
								const function_info& entry,
								const std::span<const device_image* const> arg,
								metal4_resource_container_t& res_info) {
	set_argument_image_array<enc_type, const device_image* const>(idx, encoder, entry, arg, res_info);
}

//! returns the entry for the current indices and makes sure that stage_input args are ignored
//! NOTE: for normal use, "print_error_on_failure" is true and prints and error when going out-of-bounds,
//!       however, there may also be a valid use case (set_buffer_mutability), so this can be set to false
template <bool print_error_on_failure = true>
static inline const function_info* arg_pre_handler(const std::span<const function_info* const>& entries, idx_handler& idx) {
	// make sure we have a usable entry
	const function_info* entry = nullptr;
	for (;;) {
		if (idx.entry >= entries.size()) {
			if constexpr (print_error_on_failure) {
				log_error("function entry is out-of-bounds");
			}
			return nullptr;
		}
		
		// get the next non-nullptr entry or use the current one if it's valid
		while (entries[idx.entry] == nullptr) {
			++idx.entry;
			if (idx.entry >= entries.size()) {
				if constexpr (print_error_on_failure) {
					log_error("function entry is out-of-bounds");
				}
				return nullptr;
			}
		}
		entry = entries[idx.entry];
		
		// ignore any stage input args
		while (idx.arg < entry->args.size() && has_flag<ARG_FLAG::STAGE_INPUT>(entry->args[idx.arg].flags)) {
			++idx.arg;
		}
		
		// have all args been specified for this entry?
		if (idx.arg >= entry->args.size()) {
			// implicit args at the end
			const auto implicit_arg_count = (has_flag<FUNCTION_FLAGS::USES_SOFT_PRINTF>(entry->flags) ? 1u : 0u);
			if (idx.arg < entry->args.size() + implicit_arg_count) {
				idx.is_implicit = true;
			} else { // actual end
				// get the next entry
				++idx.entry;
				// reset
				idx.arg = 0;
				idx.is_implicit = false;
				idx.implicit = 0;
				idx.buffer_idx = 0;
				idx.texture_idx = 0;
				continue;
			}
		}
		break;
	}
	return entry;
}

//! increments indicies dependent on the arg
static inline void arg_post_handler(const function_info& entry, idx_handler& idx) {
	// advance all indices
	if (idx.is_implicit) {
		++idx.implicit;
		// always a buffer for now
		++idx.buffer_idx;
	} else {
		if (entry.args[idx.arg].image_type == ARG_IMAGE_TYPE::NONE) {
			// buffer
			const auto buf_idx_count = (entry.args[idx.arg].is_array() ? entry.args[idx.arg].array_extent : 1u);
			assert(buf_idx_count > 0u);
			idx.buffer_idx += buf_idx_count;
		} else {
			// texture
			const auto tex_arg_count = (entry.args[idx.arg].is_array() ? entry.args[idx.arg].array_extent : 1u);
			assert(tex_arg_count > 0u);
			
			idx.texture_idx += tex_arg_count;
			if (entry.args[idx.arg].access == ARG_ACCESS::READ_WRITE) {
				// read/write images are implemented as two images -> add twice
				idx.texture_idx += tex_arg_count;
			}
		}
	}
	// finally
	++idx.arg;
}

//! handles a single argument from set_and_handle_arguments()
template <ENCODER_TYPE enc_type>
static inline bool handle_argument(const device& dev,
								   idx_handler& idx,
								   const device_function_arg& arg,
								   const function_info& entry,
								   encoder_selector_t<enc_type> arg_encoder,
								   metal4_resource_container_t& res_info,
								   const std::vector<uint32_t>* arg_buffer_indices,
								   std::vector<const metal4_argument_buffer*>* gathered_arg_buffers,
								   const bool include_arg_buffer_resources) {
	if (auto buf_ptr = get_if<const device_buffer*>(&arg.var)) {
		set_argument<enc_type>(idx, arg_encoder, entry, *buf_ptr, arg_buffer_indices, res_info);
	} else if (auto vec_buf_ptrs = get_if<std::span<const device_buffer* const>>(&arg.var)) {
		if constexpr (enc_type == ENCODER_TYPE::ARGUMENT_BUFFER) {
			set_argument<enc_type>(idx, arg_encoder, *vec_buf_ptrs, dev, arg_buffer_indices, res_info);
		} else {
			log_error("buffer arrays are only supported for argument buffers");
			return false;
		}
	} else if (auto vec_buf_sptrs = get_if<std::span<const std::shared_ptr<device_buffer>>>(&arg.var)) {
		if constexpr (enc_type == ENCODER_TYPE::ARGUMENT_BUFFER) {
			set_argument<enc_type>(idx, arg_encoder, *vec_buf_sptrs, dev, arg_buffer_indices, res_info);
		} else {
			log_error("buffer arrays are only supported for argument buffers");
			return false;
		}
	} else if (auto img_ptr = get_if<const device_image*>(&arg.var)) {
		set_argument<enc_type>(idx, arg_encoder, entry, *img_ptr, res_info);
	} else if (auto vec_img_ptrs = get_if<std::span<const device_image* const>>(&arg.var)) {
		set_argument<enc_type>(idx, arg_encoder, entry, *vec_img_ptrs, res_info);
	} else if (auto vec_img_sptrs = get_if<std::span<const std::shared_ptr<device_image>>>(&arg.var)) {
		set_argument<enc_type>(idx, arg_encoder, entry, *vec_img_sptrs, res_info);
	} else if (auto arg_buf_ptr = get_if<const argument_buffer*>(&arg.var)) {
		assert(dynamic_cast<const metal4_argument_buffer*>(*arg_buf_ptr));
		const auto mtl_arg_buf_ptr = (const metal4_argument_buffer*)*arg_buf_ptr;
		set_argument<enc_type>(idx, arg_encoder, entry, mtl_arg_buf_ptr, arg_buffer_indices, res_info, include_arg_buffer_resources);
		if (gathered_arg_buffers) {
			gathered_arg_buffers->emplace_back(mtl_arg_buf_ptr);
		}
	} else if (auto generic_arg_ptr = get_if<const void*>(&arg.var)) {
		if constexpr (enc_type == ENCODER_TYPE::ARGUMENT_BUFFER || enc_type == ENCODER_TYPE::ARGUMENT_TABLE) {
			set_argument<enc_type>(idx, arg_encoder, *generic_arg_ptr, arg.size, arg_buffer_indices);
		} else {
			log_error("can not encode a raw value into an indirect compute/render command");
			return false;
		}
	} else {
		log_error("encountered invalid arg");
		return false;
	}
	return true;
}

//! sets and handles all arguments in argument buffers/tables and indirect functions
//! NOTE: ensure this is enclosed in an @autoreleasepool when called!
template <ENCODER_TYPE enc_type, bool multi_encoder = false, uint32_t multi_encoder_count = 0u>
static inline bool set_and_handle_arguments(const device& dev,
											std::conditional_t<!multi_encoder, encoder_selector_t<enc_type>,
															   argument_table_encoders_t<multi_encoder_count>&> encoder,
											const std::span<const function_info* const> entries,
											const std::vector<device_function_arg>& args,
											const std::vector<device_function_arg>& implicit_args,
											std::conditional_t<!multi_encoder, metal4_resource_container_t&,
															   std::array<metal4_resource_container_t*, multi_encoder_count>> res_info,
											const std::vector<uint32_t>* arg_buffer_indices = nullptr,
											std::vector<const metal4_argument_buffer*>* gathered_arg_buffers = nullptr,
											const bool include_arg_buffer_resources = true) {
	const size_t arg_count = args.size() + implicit_args.size();
	idx_handler idx;
	size_t explicit_idx = 0, implicit_idx = 0;
	for (size_t i = 0; i < arg_count; ++i) {
		auto entry = arg_pre_handler(entries, idx);
		if (entry == nullptr) {
			return false;
		}
		const auto& arg = (!idx.is_implicit ? args[explicit_idx++] : implicit_args[implicit_idx++]);
		
		if constexpr (!multi_encoder) {
			if (!handle_argument<enc_type>(dev, idx, arg, *entry, encoder, res_info, arg_buffer_indices,
										   gathered_arg_buffers, include_arg_buffer_resources)) {
				return false;
			}
		} else {
			auto& arg_encoder = encoder.encoders[idx.entry];
			auto& arg_res_info = res_info[idx.entry];
			assert(arg_res_info);
			if (!handle_argument<enc_type>(dev, idx, arg, *entry, arg_encoder, *arg_res_info, arg_buffer_indices,
										   gathered_arg_buffers, include_arg_buffer_resources)) {
				return false;
			}
		}
		
		arg_post_handler(*entry, idx);
	}
	return true;
}

} // namespace fl::metal4_args
