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

#include <floor/device/opencl/opencl_image.hpp>

#if !defined(FLOOR_NO_OPENCL)

#include <numeric>
#include <floor/core/logger.hpp>
#include <floor/device/opencl/opencl_queue.hpp>
#include <floor/device/opencl/opencl_device.hpp>
#include <floor/device/opencl/opencl_context.hpp>

namespace fl {

// TODO: proper error (return) value handling everywhere

opencl_image::opencl_image(const device_queue& cqueue,
						   const uint4 image_dim_,
						   const IMAGE_TYPE image_type_,
						   std::span<uint8_t> host_data_,
						   const MEMORY_FLAG flags_,
						   const uint32_t mip_level_limit_) :
device_image(cqueue, image_dim_, image_type_, host_data_, flags_, nullptr, false, mip_level_limit_),
mip_origin_idx(!is_mip_mapped ? 0 :
			   (image_dim_count(image_type) + (has_flag<IMAGE_TYPE::FLAG_ARRAY>(image_type) ? 1 : 0))) {
	switch(flags & MEMORY_FLAG::READ_WRITE) {
		case MEMORY_FLAG::READ:
			cl_flags |= CL_MEM_READ_ONLY;
			break;
		case MEMORY_FLAG::WRITE:
			cl_flags |= CL_MEM_WRITE_ONLY;
			break;
		case MEMORY_FLAG::READ_WRITE:
			cl_flags |= CL_MEM_READ_WRITE;
			break;
			// all possible cases handled
		default: floor_unreachable();
	}
	
	switch(flags & MEMORY_FLAG::HOST_READ_WRITE) {
		case MEMORY_FLAG::HOST_READ:
			cl_flags |= CL_MEM_HOST_READ_ONLY;
			break;
		case MEMORY_FLAG::HOST_WRITE:
			cl_flags |= CL_MEM_HOST_WRITE_ONLY;
			break;
		case MEMORY_FLAG::HOST_READ_WRITE:
			// both - this is the default
			break;
		case MEMORY_FLAG::NONE:
			cl_flags |= CL_MEM_HOST_NO_ACCESS;
			break;
			// all possible cases handled
		default: floor_unreachable();
	}
	
	// TODO: handle the remaining flags + host ptr
	if (host_data.data() != nullptr && !has_flag<MEMORY_FLAG::NO_INITIAL_COPY>(flags) && !is_mip_mapped) {
		cl_flags |= CL_MEM_COPY_HOST_PTR;
	}
	
	// actually create the image
	if(!create_internal(true, cqueue)) {
		return; // can't do much else
	}
}

bool opencl_image::create_internal(const bool copy_host_data, const device_queue& cqueue) {
	if (has_flag<IMAGE_TYPE::FLAG_STENCIL>(image_type)) {
		throw std::runtime_error("stencil images are not supported");
	}
	
	const auto& cl_dev = (const opencl_device&)cqueue.get_device();
	cl_int create_err = CL_SUCCESS;
	
	cl_image_format cl_img_format;
	const bool is_depth = has_flag<IMAGE_TYPE::FLAG_DEPTH>(image_type);
	const bool is_array = has_flag<IMAGE_TYPE::FLAG_ARRAY>(image_type);
	// TODO: handle other channel orders (CL_*x formats if supported/prefered, reverse order)
	switch(image_type & IMAGE_TYPE::__CHANNELS_MASK) {
		case IMAGE_TYPE::CHANNELS_1:
			cl_img_format.image_channel_order = (is_depth ? CL_DEPTH : CL_R);
			break;
		case IMAGE_TYPE::CHANNELS_2:
			cl_img_format.image_channel_order = CL_RG;
			break;
		case IMAGE_TYPE::CHANNELS_3:
			cl_img_format.image_channel_order = CL_RGB;
			break;
		case IMAGE_TYPE::CHANNELS_4:
			cl_img_format.image_channel_order = (image_layout_bgra(image_type) ? CL_BGRA :
												 image_layout_argb(image_type) ? CL_ARGB : CL_RGBA);
			break;
		default: floor_unreachable();
	}
	static const std::unordered_map<IMAGE_TYPE, cl_channel_type> format_lut {
		{ IMAGE_TYPE::INT | IMAGE_TYPE::FORMAT_8 | IMAGE_TYPE::FLAG_NORMALIZED, CL_SNORM_INT8 },
		{ IMAGE_TYPE::UINT | IMAGE_TYPE::FORMAT_8 | IMAGE_TYPE::FLAG_NORMALIZED, CL_UNORM_INT8 },
		{ IMAGE_TYPE::INT | IMAGE_TYPE::FORMAT_8, CL_SIGNED_INT8 },
		{ IMAGE_TYPE::UINT | IMAGE_TYPE::FORMAT_8, CL_UNSIGNED_INT8 },
		{ IMAGE_TYPE::INT | IMAGE_TYPE::FORMAT_16 | IMAGE_TYPE::FLAG_NORMALIZED, CL_SNORM_INT16 },
		{ IMAGE_TYPE::UINT | IMAGE_TYPE::FORMAT_16 | IMAGE_TYPE::FLAG_NORMALIZED, CL_UNORM_INT16 },
		{ IMAGE_TYPE::INT | IMAGE_TYPE::FORMAT_16, CL_SIGNED_INT16 },
		{ IMAGE_TYPE::UINT | IMAGE_TYPE::FORMAT_16, CL_UNSIGNED_INT32 },
		{ IMAGE_TYPE::INT | IMAGE_TYPE::FORMAT_32, CL_SIGNED_INT32 },
		{ IMAGE_TYPE::UINT | IMAGE_TYPE::FORMAT_32, CL_UNSIGNED_INT32 },
		{ IMAGE_TYPE::FLOAT | IMAGE_TYPE::FORMAT_16, CL_HALF_FLOAT },
		{ IMAGE_TYPE::FLOAT | IMAGE_TYPE::FORMAT_32, CL_FLOAT },
	};
	const auto cl_format = format_lut.find(image_type & (IMAGE_TYPE::__DATA_TYPE_MASK |
														 IMAGE_TYPE::__FORMAT_MASK |
														 IMAGE_TYPE::FLAG_NORMALIZED));
	if(cl_format == end(format_lut)) {
		log_error("unsupported image format: $ ($X)", image_type_to_string(image_type), image_type);
		return false;
	}
	cl_img_format.image_channel_data_type = cl_format->second;
	
	//
	cl_image_desc cl_img_desc;
	memset(&cl_img_desc, 0, sizeof(cl_image_desc));
	
	const auto dim_count = image_dim_count(image_type);
	if(is_array) {
		if(dim_count < 1 || dim_count > 2) {
			log_error("buffer format is only supported for 1D and 2D images!");
			return false;
		}
		cl_img_desc.image_type = (dim_count == 1 ? CL_MEM_OBJECT_IMAGE1D_ARRAY : CL_MEM_OBJECT_IMAGE2D_ARRAY);
		cl_img_desc.image_array_size = layer_count;
	}
	else if(has_flag<IMAGE_TYPE::FLAG_BUFFER>(image_type)) {
		if(dim_count != 1) {
			log_error("buffer format is only supported for 1D images!");
			return false;
		}
		cl_img_desc.image_type = CL_MEM_OBJECT_IMAGE1D_BUFFER;
		// TODO: must set cl_img_desc.buffer to the resp. buffer
		log_error("1D buffer not supported yet!");
		return false;
	}
	else {
		cl_img_desc.image_type = (dim_count == 1 ? CL_MEM_OBJECT_IMAGE1D :
								  (dim_count == 2 ? CL_MEM_OBJECT_IMAGE2D : CL_MEM_OBJECT_IMAGE3D));
	}
	
	cl_img_desc.image_width = image_dim.x;
	if(dim_count > 1) cl_img_desc.image_height = image_dim.y;
	if(dim_count > 2) cl_img_desc.image_depth = image_dim.z;
	
	// TODO: support this
	//cl_img_desc.num_samples = ;
	
	if(has_flag<IMAGE_TYPE::FLAG_MIPMAPPED>(image_type) &&
	   // spec says this must be > 1
	   mip_level_count > 1) {
		cl_img_desc.num_mip_levels = mip_level_count;
	}
	
	// NOTE: image_row_pitch, image_slice_pitch are optional, but should be set when constructing from an image descriptor (which is TODO)
	
	// -> normal OpenCL image
	image = clCreateImage(cl_dev.ctx, cl_flags, &cl_img_format, &cl_img_desc,
						  (copy_host_data && !is_mip_mapped ? host_data.data() : nullptr), &create_err);
	if(create_err != CL_SUCCESS) {
		log_error("failed to create image: $", cl_error_to_string(create_err));
		image = nullptr;
		return false;
	}
	
	// host_ptr must be nullptr in clCreateImage when using mip-mapping
	// -> must copy/write this afterwards
	if (is_mip_mapped && copy_host_data && host_data.data() != nullptr &&
		!has_flag<MEMORY_FLAG::NO_INITIAL_COPY>(flags)) {
		auto cpy_host_data = host_data;
		apply_on_levels([this, &cpy_host_data, &cqueue](const uint32_t& level,
														const uint4& mip_image_dim,
														const uint32_t&,
														const uint32_t& level_data_size) {
			const size4 level_region { mip_image_dim.xyz.maxed(1), 1 };
			size4 level_origin;
			level_origin[mip_origin_idx] = level;
			CL_CALL_RET(clEnqueueWriteImage((cl_command_queue)const_cast<void*>(cqueue.get_queue_ptr()), image, false,
											level_origin.data(), level_region.data(), 0, 0, cpy_host_data.data(),
											0, nullptr, nullptr),
						"failed to copy initial host data to device (mip-level #" + std::to_string(level) + ")", false)
			cpy_host_data = cpy_host_data.subspan(level_data_size, cpy_host_data.size_bytes() - level_data_size);
			return true;
		});
		cqueue.finish();
	}
	
	// manually create mip-map chain
	if(generate_mip_maps) {
		generate_mip_map_chain(cqueue);
	}
	
	return true;
}

opencl_image::~opencl_image() {
	if (image != nullptr) {
		clReleaseMemObject(image);
	}
}

bool opencl_image::zero(const device_queue& cqueue) {
	if(image == nullptr) return false;
	
	const float4 black { 0.0f }; // bit identical to uint4(0) and int4(0), so format doesn't matter here
	const size4 origin { 0, 0, 0, 0 };
	const size4 region { image_dim.xyz.maxed(1), 1 };
	CL_CALL_RET(clEnqueueFillImage((cl_command_queue)const_cast<void*>(cqueue.get_queue_ptr()), image,
								   (const void*)&black, origin.data(), region.data(),
								   0, nullptr, nullptr),
				"failed to zero image", false)
	
	// NOTE: clEnqueueFillImage is not listed as supporting mip-mapping by cl_khr_mipmap_image
	// -> create a 0-buffer for all mip-levels > 0
	if(is_mip_mapped) {
		std::unique_ptr<uint8_t[]> zero_buffer;
		const auto success = apply_on_levels<true>([this, &cqueue, &zero_buffer](const uint32_t& level,
																				 const uint4& mip_image_dim,
																				 const uint32_t&,
																				 const uint32_t& level_data_size) {
			// level #0 already handled
			if(level == 0) return true;
			if(level == 1) {
				// level #1 is the largest, simply reuse for later levels
				zero_buffer = std::make_unique<uint8_t[]>(level_data_size);
				memset(zero_buffer.get(), 0, level_data_size);
			}
			
			const size4 level_region { mip_image_dim.xyz.maxed(1), 1 };
			size4 level_origin;
			level_origin[mip_origin_idx] = level;
			CL_CALL_RET(clEnqueueWriteImage((cl_command_queue)const_cast<void*>(cqueue.get_queue_ptr()), image, false,
											level_origin.data(), level_region.data(), 0, 0, zero_buffer.get(),
											0, nullptr, nullptr),
						"failed to zero image (mip-level #" + std::to_string(level) + ")", false)
			return true;
		});
		
		// block until all have been written
		cqueue.finish();
		
		return success;
	}
	return true;
}

void* __attribute__((aligned(128))) opencl_image::map(const device_queue& cqueue, const MEMORY_MAP_FLAG flags_) {
	if(image == nullptr) return nullptr;
	
	const bool blocking_map = has_flag<MEMORY_MAP_FLAG::BLOCK>(flags_) || generate_mip_maps;
	// TODO: image map check
	
	cl_map_flags map_flags = 0;
	if(has_flag<MEMORY_MAP_FLAG::WRITE_INVALIDATE>(flags_)) {
		map_flags |= CL_MAP_WRITE_INVALIDATE_REGION;
	}
	else {
		switch(flags_ & MEMORY_MAP_FLAG::READ_WRITE) {
			case MEMORY_MAP_FLAG::READ:
				map_flags |= CL_MAP_READ;
				break;
			case MEMORY_MAP_FLAG::WRITE:
				map_flags |= CL_MAP_WRITE;
				break;
			case MEMORY_MAP_FLAG::READ_WRITE:
				map_flags |= CL_MAP_READ | CL_MAP_WRITE;
				break;
			case MEMORY_MAP_FLAG::NONE:
			default:
				log_error("neither read nor write flag set for image mapping!");
				return nullptr;
		}
	}
	
	// NOTE: for non-mip-mapped images and automatically mip-mapped images, this will only map level #0
	//       for manually mip-mapped images, this will map all mip-levels
	std::vector<void*> mapped_ptrs;
	std::vector<size_t> level_sizes;
	if(!apply_on_levels([this, &mapped_ptrs, &level_sizes,
						 &blocking_map, &map_flags, &cqueue](const uint32_t& level,
															 const uint4& mip_image_dim,
															 const uint32_t&,
															 const uint32_t& level_data_size) {
		// TODO: parameter origin + region
		const size4 region { mip_image_dim.xyz.maxed(1), 1 }; // complete image(s) + "The values in region cannot be 0."
		size4 origin;
		origin[mip_origin_idx] = level;
		
		size_t image_row_pitch = 0, image_slice_pitch = 0; // must not be nullptr
		cl_int map_err = CL_SUCCESS;
		auto mapped_ptr = clEnqueueMapImage((cl_command_queue)const_cast<void*>(cqueue.get_queue_ptr()),
											image, blocking_map, map_flags,
											origin.data(), region.data(),
											&image_row_pitch, &image_slice_pitch,
											0, nullptr, nullptr, &map_err);
		if(map_err != CL_SUCCESS) {
			log_error("failed to map image$: $!",
					  (generate_mip_maps ? " (level #" + std::to_string(level) + ")" : ""),
					  cl_error_to_string(map_err));
			return false;
		}
		
		mapped_ptrs.emplace_back(mapped_ptr);
		level_sizes.emplace_back(level_data_size);
		return true;
	})) {
		return nullptr;
	}
	
	auto ret_ptr = mapped_ptrs[0];
	aligned_ptr<uint8_t> host_buffer;
	if(!generate_mip_maps && is_mip_mapped) { // -> manual mip-mapping
		// since each mip-level is mapped individually, we must create a contiguous buffer manually
		// and copy each mip-level into it (if using read mapping, for write/write_invalidate this doesn't matter)
		const auto total_size = std::accumulate(std::begin(level_sizes), std::end(level_sizes), size_t(0));
		host_buffer = make_aligned_ptr<uint8_t>(total_size);
		ret_ptr = host_buffer.get();
		
		if(has_flag<MEMORY_MAP_FLAG::READ>(flags_)) {
			auto cpy_ptr = (uint8_t*)ret_ptr;
			for(size_t i = 0; i < mapped_ptrs.size(); ++i) {
				memcpy(cpy_ptr, mapped_ptrs[i], level_sizes[i]);
				cpy_ptr += level_sizes[i];
			}
		}
	}
	
	mappings.emplace(ret_ptr, opencl_mapping { std::move(host_buffer), flags_, std::move(mapped_ptrs), std::move(level_sizes) });
	return ret_ptr;
}

bool opencl_image::unmap(const device_queue& cqueue, void* __attribute__((aligned(128))) mapped_ptr) {
	if(image == nullptr) return false;
	if(mapped_ptr == nullptr) return false;
	
	// check if this is actually a mapped pointer
	const auto iter = mappings.find(mapped_ptr);
	if(iter == mappings.end()) {
		log_error("invalid mapped pointer: $X", mapped_ptr);
		return false;
	}
	
	// when using manual mip-mapping and write/write_invalidate mapping,
	// data must be copied back from the contiguous buffer to each mapped mip-level
	if(!generate_mip_maps && is_mip_mapped &&
	   (has_flag<MEMORY_MAP_FLAG::WRITE>(iter->second.flags) ||
		has_flag<MEMORY_MAP_FLAG::WRITE_INVALIDATE>(iter->second.flags))) {
		auto cpy_ptr = (uint8_t*)mapped_ptr;
		for(size_t i = 0; i < iter->second.mapped_ptrs.size(); ++i) {
			memcpy(iter->second.mapped_ptrs[i], cpy_ptr, iter->second.level_sizes[i]);
			cpy_ptr += iter->second.level_sizes[i];
		}
	}
	
	for(const auto& mptr : iter->second.mapped_ptrs) {
		CL_CALL_RET(clEnqueueUnmapMemObject((cl_command_queue)const_cast<void*>(cqueue.get_queue_ptr()), image, mptr, 0, nullptr, nullptr),
					"failed to unmap buffer", false)
	}
	mappings.erase(mapped_ptr);
	
	// manually create mip-map chain (only if mapping was write/write_invalidate)
	if(generate_mip_maps &&
	   (has_flag<MEMORY_MAP_FLAG::WRITE>(iter->second.flags) ||
		has_flag<MEMORY_MAP_FLAG::WRITE_INVALIDATE>(iter->second.flags))) {
		generate_mip_map_chain(cqueue);
	}
	
	return true;
}

cl_command_queue opencl_image::queue_or_default_queue(const device_queue* cqueue) const {
	if(cqueue != nullptr) return (cl_command_queue)const_cast<void*>(cqueue->get_queue_ptr());
	return (cl_command_queue)const_cast<void*>(((const opencl_context&)*dev.context).get_device_default_queue((const opencl_device&)dev)->get_queue_ptr());
}

} // namespace fl

#endif
