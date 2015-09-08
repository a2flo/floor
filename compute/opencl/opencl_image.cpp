/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2015 Florian Ziesche
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

#include <floor/compute/opencl/opencl_image.hpp>

#if !defined(FLOOR_NO_OPENCL)

#include <floor/core/logger.hpp>
#include <floor/compute/opencl/opencl_queue.hpp>
#include <floor/compute/opencl/opencl_device.hpp>
#include <floor/compute/opencl/opencl_compute.hpp>

// TODO: proper error (return) value handling everywhere

opencl_image::opencl_image(const opencl_device* device,
						   const uint4 image_dim_,
						   const COMPUTE_IMAGE_TYPE image_type_,
						   void* host_ptr_,
						   const COMPUTE_MEMORY_FLAG flags_,
						   const uint32_t opengl_type_,
						   const uint32_t external_gl_object_,
						   const opengl_image_info* gl_image_info) :
compute_image(device, image_dim_, image_type_, host_ptr_, flags_,
			  opengl_type_, external_gl_object_, gl_image_info) {
	switch(flags & COMPUTE_MEMORY_FLAG::READ_WRITE) {
		case COMPUTE_MEMORY_FLAG::READ:
			cl_flags |= CL_MEM_READ_ONLY;
			break;
		case COMPUTE_MEMORY_FLAG::WRITE:
			cl_flags |= CL_MEM_WRITE_ONLY;
			break;
		case COMPUTE_MEMORY_FLAG::READ_WRITE:
			cl_flags |= CL_MEM_READ_WRITE;
			break;
			// all possible cases handled
		default: floor_unreachable();
	}
	
	switch(flags & COMPUTE_MEMORY_FLAG::HOST_READ_WRITE) {
		case COMPUTE_MEMORY_FLAG::HOST_READ:
			cl_flags |= CL_MEM_HOST_READ_ONLY;
			break;
		case COMPUTE_MEMORY_FLAG::HOST_WRITE:
			cl_flags |= CL_MEM_HOST_WRITE_ONLY;
			break;
		case COMPUTE_MEMORY_FLAG::HOST_READ_WRITE:
			// both - this is the default
			break;
		case COMPUTE_MEMORY_FLAG::NONE:
			cl_flags |= CL_MEM_HOST_NO_ACCESS;
			break;
			// all possible cases handled
		default: floor_unreachable();
	}
	
	// TODO: handle the remaining flags + host ptr
	if(host_ptr_ != nullptr && !has_flag<COMPUTE_MEMORY_FLAG::NO_INITIAL_COPY>(flags)) {
		cl_flags |= CL_MEM_COPY_HOST_PTR;
	}
	
	// actually create the image
	if(!create_internal(true, nullptr)) {
		return; // can't do much else
	}
}

bool opencl_image::create_internal(const bool copy_host_data, shared_ptr<compute_queue> cqueue) {
	cl_int create_err = CL_SUCCESS;
	
	cl_image_format cl_img_format;
	const bool is_depth = has_flag<COMPUTE_IMAGE_TYPE::FLAG_DEPTH>(image_type);
	const bool is_stencil = has_flag<COMPUTE_IMAGE_TYPE::FLAG_STENCIL>(image_type);
	// TODO: handle other channel orders (CL_*x formats if supported/prefered, reverse order)
	switch(image_type & COMPUTE_IMAGE_TYPE::__CHANNELS_MASK) {
		case COMPUTE_IMAGE_TYPE::CHANNELS_1:
			cl_img_format.image_channel_order = (is_depth ? CL_DEPTH : CL_R);
			break;
		case COMPUTE_IMAGE_TYPE::CHANNELS_2:
			cl_img_format.image_channel_order = (is_depth && is_stencil ? CL_DEPTH_STENCIL : CL_RG);
			break;
		case COMPUTE_IMAGE_TYPE::CHANNELS_3:
			cl_img_format.image_channel_order = CL_RGB;
			break;
		case COMPUTE_IMAGE_TYPE::CHANNELS_4:
			cl_img_format.image_channel_order = (has_flag<COMPUTE_IMAGE_TYPE::FLAG_REVERSE>(image_type) ? CL_BGRA : CL_RGBA);
			break;
		default: floor_unreachable();
	}
	static const unordered_map<COMPUTE_IMAGE_TYPE, cl_channel_type> format_lut {
		{ COMPUTE_IMAGE_TYPE::INT | COMPUTE_IMAGE_TYPE::FORMAT_8 | COMPUTE_IMAGE_TYPE::FLAG_NORMALIZED, CL_SNORM_INT8 },
		{ COMPUTE_IMAGE_TYPE::UINT | COMPUTE_IMAGE_TYPE::FORMAT_8 | COMPUTE_IMAGE_TYPE::FLAG_NORMALIZED, CL_UNORM_INT8 },
		{ COMPUTE_IMAGE_TYPE::INT | COMPUTE_IMAGE_TYPE::FORMAT_8, CL_SIGNED_INT8 },
		{ COMPUTE_IMAGE_TYPE::UINT | COMPUTE_IMAGE_TYPE::FORMAT_8, CL_UNSIGNED_INT8 },
		{ COMPUTE_IMAGE_TYPE::INT | COMPUTE_IMAGE_TYPE::FORMAT_16 | COMPUTE_IMAGE_TYPE::FLAG_NORMALIZED, CL_SNORM_INT16 },
		{ COMPUTE_IMAGE_TYPE::UINT | COMPUTE_IMAGE_TYPE::FORMAT_16 | COMPUTE_IMAGE_TYPE::FLAG_NORMALIZED, CL_UNORM_INT16 },
		{ COMPUTE_IMAGE_TYPE::INT | COMPUTE_IMAGE_TYPE::FORMAT_16, CL_SIGNED_INT16 },
		{ COMPUTE_IMAGE_TYPE::UINT | COMPUTE_IMAGE_TYPE::FORMAT_16, CL_UNSIGNED_INT32 },
		{ COMPUTE_IMAGE_TYPE::INT | COMPUTE_IMAGE_TYPE::FORMAT_32, CL_SIGNED_INT32 },
		{ COMPUTE_IMAGE_TYPE::UINT | COMPUTE_IMAGE_TYPE::FORMAT_32, CL_UNSIGNED_INT32 },
		{ COMPUTE_IMAGE_TYPE::FLOAT | COMPUTE_IMAGE_TYPE::FORMAT_16, CL_HALF_FLOAT },
		{ COMPUTE_IMAGE_TYPE::FLOAT | COMPUTE_IMAGE_TYPE::FORMAT_32, CL_FLOAT },
	};
	const auto cl_format = format_lut.find(image_type & (COMPUTE_IMAGE_TYPE::__DATA_TYPE_MASK |
														 COMPUTE_IMAGE_TYPE::__FORMAT_MASK |
														 COMPUTE_IMAGE_TYPE::FLAG_NORMALIZED));
	if(cl_format == end(format_lut)) {
		log_error("unsupported image format: %X", image_type);
		return false;
	}
	cl_img_format.image_channel_data_type = cl_format->second;
	
	//
	cl_image_desc cl_img_desc;
	memset(&cl_img_desc, 0, sizeof(cl_image_desc));
	
	const auto dim_count = image_dim_count(image_type);
	if(has_flag<COMPUTE_IMAGE_TYPE::FLAG_ARRAY>(image_type)) {
		if(dim_count < 1 || dim_count > 2) {
			log_error("buffer format is only supported for 1D and 2D images!");
			return false;
		}
		cl_img_desc.image_type = (dim_count == 1 ? CL_MEM_OBJECT_IMAGE1D_ARRAY : CL_MEM_OBJECT_IMAGE2D_ARRAY);
		cl_img_desc.image_array_size = (dim_count == 1 ? image_dim.y : image_dim.z);
	}
	else if(has_flag<COMPUTE_IMAGE_TYPE::FLAG_BUFFER>(image_type)) {
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
	
	// TODO: cube map?
	cl_img_desc.image_width = image_dim.x;
	if(dim_count > 1) cl_img_desc.image_height = image_dim.y;
	if(dim_count > 2) cl_img_desc.image_depth = image_dim.z;
	
	// TODO: support these
	//cl_img_desc.num_mip_levels = ;
	//cl_img_desc.num_samples = ;
	
	// NOTE: image_row_pitch, image_slice_pitch are optional, but should be set when constructing from an image descriptor (which is TODO)
	
	// -> normal opencl image
	if(!has_flag<COMPUTE_MEMORY_FLAG::OPENGL_SHARING>(flags)) {
		image = clCreateImage(((opencl_device*)dev)->ctx, cl_flags, &cl_img_format, &cl_img_desc,
							  (copy_host_data ? host_ptr : nullptr), &create_err);
		if(create_err != CL_SUCCESS) {
			log_error("failed to create image: %s", cl_error_to_string(create_err));
			image = nullptr;
			return false;
		}
	}
	// -> shared opencl/opengl image
	else {
		if(!create_gl_image(copy_host_data)) return false;
		
		// "Only CL_MEM_READ_ONLY, CL_MEM_WRITE_ONLY and CL_MEM_READ_WRITE values specified in table 5.3 can be used"
		cl_flags &= (CL_MEM_READ_ONLY | CL_MEM_WRITE_ONLY | CL_MEM_READ_WRITE); // be lenient on other flag use
		if(!has_flag<COMPUTE_IMAGE_TYPE::FLAG_RENDERBUFFER>(image_type)) {
			image = clCreateFromGLTexture(((opencl_device*)dev)->ctx, cl_flags, opengl_type,
										  0 /* TODO: mip level */, gl_object, &create_err);
		}
		else {
			image = clCreateFromGLRenderbuffer(((opencl_device*)dev)->ctx, cl_flags, gl_object, &create_err);
		}
		if(create_err != CL_SUCCESS) {
			log_error("failed to create image from opengl object: %s", cl_error_to_string(create_err));
			image = nullptr;
			return false;
		}
		// acquire for use with opencl
		acquire_opengl_object(cqueue);
	}
	
	return true;
}

opencl_image::~opencl_image() {
	// first, release and kill the opengl image
	if(gl_object != 0) {
		if(gl_object_state) {
			log_warn("image still registered for opengl use - acquire before destructing a compute image!");
		}
		if(!gl_object_state) release_opengl_object(nullptr); // -> release to opengl
		delete_gl_image();
	}
	// then, also kill the opencl image
	if(image != nullptr) {
		clReleaseMemObject(image);
	}
}

void opencl_image::zero(shared_ptr<compute_queue> cqueue) {
	if(image == nullptr) return;
	
	const float4 black { 0.0f }; // bit identical to uint4(0) and int4(0), so format doesn't matter here
	const size3 origin { 0, 0, 0 };
	const size3 region { image_dim.xyz.maxed(1) };
	CL_CALL_RET(clEnqueueFillImage(queue_or_default_queue(cqueue), image,
								   (const void*)&black, origin.data(), region.data(),
								   0, nullptr, nullptr),
				"failed to zero image")
}

void* __attribute__((aligned(128))) opencl_image::map(shared_ptr<compute_queue> cqueue, const COMPUTE_MEMORY_MAP_FLAG flags_) {
	if(image == nullptr) return nullptr;
	
	// TODO: parameter origin + region
	const size3 origin { 0, 0, 0 };
	const size3 region { image_dim.xyz.maxed(1) }; // complete image(s) + "The values in region cannot be 0."
	
	const bool blocking_map = has_flag<COMPUTE_MEMORY_MAP_FLAG::BLOCK>(flags_);
	// TODO: image map check
	
	cl_map_flags map_flags = 0;
	if(has_flag<COMPUTE_MEMORY_MAP_FLAG::WRITE_INVALIDATE>(flags_)) {
		map_flags |= CL_MAP_WRITE_INVALIDATE_REGION;
	}
	else {
		switch(flags_ & COMPUTE_MEMORY_MAP_FLAG::READ_WRITE) {
			case COMPUTE_MEMORY_MAP_FLAG::READ:
				map_flags |= CL_MAP_READ;
				break;
			case COMPUTE_MEMORY_MAP_FLAG::WRITE:
				map_flags |= CL_MAP_WRITE;
				break;
			case COMPUTE_MEMORY_MAP_FLAG::READ_WRITE:
				map_flags |= CL_MAP_READ | CL_MAP_WRITE;
				break;
			case COMPUTE_MEMORY_MAP_FLAG::NONE:
			default:
				log_error("neither read nor write flag set for image mapping!");
				return nullptr;
		}
	}
	
	//
	size_t image_row_pitch = 0, image_slice_pitch = 0; // must not be nullptr (TODO: return these)
	cl_int map_err = CL_SUCCESS;
	auto ret_ptr = clEnqueueMapImage(queue_or_default_queue(cqueue),
									 image, blocking_map, map_flags,
									 origin.data(), region.data(),
									 &image_row_pitch, &image_slice_pitch,
									 0, nullptr, nullptr, &map_err);
	if(map_err != CL_SUCCESS) {
		log_error("failed to map image: %s!", cl_error_to_string(map_err));
		return nullptr;
	}
	return ret_ptr;
}

void opencl_image::unmap(shared_ptr<compute_queue> cqueue, void* __attribute__((aligned(128))) mapped_ptr) {
	if(image == nullptr) return;
	if(mapped_ptr == nullptr) return;
	
	CL_CALL_RET(clEnqueueUnmapMemObject(queue_or_default_queue(cqueue), image, mapped_ptr, 0, nullptr, nullptr),
				"failed to unmap buffer");
}

bool opencl_image::acquire_opengl_object(shared_ptr<compute_queue> cqueue) {
	if(gl_object == 0) return false;
	if(!gl_object_state) {
		log_warn("opengl image has already been acquired for use with opencl!");
		return true;
	}
	
	CL_CALL_RET(clEnqueueAcquireGLObjects(queue_or_default_queue(cqueue), 1, &image, 0, nullptr, nullptr),
				"failed to acquire opengl image - opencl gl object acquire failed", false);
	gl_object_state = false;
	return true;
}

bool opencl_image::release_opengl_object(shared_ptr<compute_queue> cqueue) {
	if(gl_object == 0) return false;
	if(image == 0) return false;
	if(gl_object_state) {
		log_warn("opengl image has already been released for opengl use!");
		return true;
	}
	
	CL_CALL_RET(clEnqueueReleaseGLObjects(queue_or_default_queue(cqueue), 1, &image, 0, nullptr, nullptr),
				"failed to release opengl image - opencl gl object release failed", false);
	gl_object_state = true;
	return true;
}

cl_command_queue opencl_image::queue_or_default_queue(shared_ptr<compute_queue> cqueue) const {
	if(cqueue != nullptr) return (cl_command_queue)cqueue->get_queue_ptr();
	return (cl_command_queue)((opencl_device*)dev)->compute_ctx->get_device_default_queue((const opencl_device*)dev)->get_queue_ptr();
}

#endif
