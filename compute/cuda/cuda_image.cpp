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

#include <floor/compute/cuda/cuda_image.hpp>

#if !defined(FLOOR_NO_CUDA)

#include <floor/core/logger.hpp>
#include <floor/compute/cuda/cuda_queue.hpp>
#include <floor/compute/cuda/cuda_device.hpp>

// TODO: proper error (return) value handling everywhere

cuda_image::cuda_image(const cuda_device* device,
					   const uint4 image_dim_,
					   const COMPUTE_IMAGE_TYPE image_type_,
					   const COMPUTE_IMAGE_STORAGE_TYPE storage_type_,
					   const uint32_t channel_count_,
					   void* host_ptr_,
					   const COMPUTE_MEMORY_FLAG flags_,
					   const uint32_t opengl_type_) :
compute_image(device, image_dim_, image_type_, storage_type_, channel_count_, host_ptr_, flags_, opengl_type_) {
	// TODO: handle the remaining flags + host ptr
	
	// need to allocate the buffer on the correct device, if a context was specified,
	// else: assume the correct context is already active
	if(device->ctx != nullptr) {
		CU_CALL_RET(cuCtxSetCurrent(device->ctx),
					"failed to make cuda context current");
	}
	
	// actually create the image
	if(!create_internal(true, nullptr)) {
		return; // can't do much else
	}
}

bool cuda_image::create_internal(const bool copy_host_data, shared_ptr<compute_queue> cqueue) {
	// image handling in cuda/ptx is rather complicated:
	// when using a texture reference (sm_20+) or object (sm_30+), you can only read from it, but with sampler support,
	// when using a surface reference (sm_20+) or object (sm_30+), you can read _and_ write from/to it, but without sampler support.
	// this is further complicated by the fact that I currently can't generate texture/surface reference code, thus no sm_20 support
	// right now + even then, setting and using a texture/surface reference is rather tricky as it is a global module symbol.
	// if "no sampler" flag is set, only use surfaces
	const bool no_sampler { has_flag<COMPUTE_IMAGE_TYPE::FLAG_NO_SAMPLER>(image_type) };
	const bool need_tex { has_flag<COMPUTE_MEMORY_FLAG::READ>(flags) && !no_sampler };
	const bool need_surf { has_flag<COMPUTE_MEMORY_FLAG::WRITE>(flags) || no_sampler };
	
	// -> cuda array
	if(!has_flag<COMPUTE_MEMORY_FLAG::OPENGL_SHARING>(flags)) {
		CUarray_format format;
		switch(storage_type) {
			case COMPUTE_IMAGE_STORAGE_TYPE::INT_8:
			case COMPUTE_IMAGE_STORAGE_TYPE::NORM_INT_8:
				format = CU_AD_FORMAT_SIGNED_INT8;
				break;
			case COMPUTE_IMAGE_STORAGE_TYPE::INT_16:
			case COMPUTE_IMAGE_STORAGE_TYPE::NORM_INT_16:
				format = CU_AD_FORMAT_SIGNED_INT16;
				break;
			case COMPUTE_IMAGE_STORAGE_TYPE::INT_32:
			case COMPUTE_IMAGE_STORAGE_TYPE::NORM_INT_32:
				format = CU_AD_FORMAT_SIGNED_INT32;
				break;
			case COMPUTE_IMAGE_STORAGE_TYPE::UINT_8:
			case COMPUTE_IMAGE_STORAGE_TYPE::NORM_UINT_8:
				format = CU_AD_FORMAT_UNSIGNED_INT8;
				break;
			case COMPUTE_IMAGE_STORAGE_TYPE::UINT_16:
			case COMPUTE_IMAGE_STORAGE_TYPE::NORM_UINT_16:
				format = CU_AD_FORMAT_UNSIGNED_INT16;
				break;
			case COMPUTE_IMAGE_STORAGE_TYPE::UINT_32:
			case COMPUTE_IMAGE_STORAGE_TYPE::NORM_UINT_32:
				format = CU_AD_FORMAT_UNSIGNED_INT32;
				break;
			case COMPUTE_IMAGE_STORAGE_TYPE::FLOAT_16:
				format = CU_AD_FORMAT_HALF;
				break;
			case COMPUTE_IMAGE_STORAGE_TYPE::FLOAT_32:
				format = CU_AD_FORMAT_FLOAT;
				break;
			default:
				log_error("unsupported storage format: %u", storage_type);
				return false;
		}
		const uint32_t dim { compute_image::dim_count(image_type) };
		
		size_t depth = 0;
		if(dim >= 3) {
			depth = image_dim.z;
		}
		else {
			// check array first ...
			if(has_flag<COMPUTE_IMAGE_TYPE::FLAG_ARRAY>(image_type)) {
				if(dim == 1) depth = image_dim.y;
				else if(dim >= 2) depth = image_dim.z;
			}
			// ... and check cube map second
			if(has_flag<COMPUTE_IMAGE_TYPE::FLAG_CUBE>(image_type)) {
				// if FLAG_ARRAY is also present, .z/depth has been specified by the user -> multiply by 6,
				// else, just specify 6 directly
				depth = (depth != 0 ? depth * 6 : 6);
			}
		}
		// TODO: cube map: make sure width == height
		// TODO: handle channel count == 3
		
		const CUDA_ARRAY3D_DESCRIPTOR desc {
			.Width = image_dim.x,
			.Height = (dim >= 2 ? image_dim.y : 0),
			.Depth = depth,
			.Format = format,
			.NumChannels = channel_count,
			.Flags = (
				(has_flag<COMPUTE_IMAGE_TYPE::FLAG_ARRAY>(image_type) ? CUDA_ARRAY3D_LAYERED : 0u) |
				(has_flag<COMPUTE_IMAGE_TYPE::FLAG_CUBE>(image_type) ? CUDA_ARRAY3D_CUBEMAP : 0u) |
				(has_flag<COMPUTE_IMAGE_TYPE::FLAG_DEPTH>(image_type) ? CUDA_ARRAY3D_DEPTH_TEXTURE : 0u) |
				(has_flag<COMPUTE_IMAGE_TYPE::FLAG_GATHER>(image_type) ? CUDA_ARRAY3D_TEXTURE_GATHER : 0u) |
				(need_surf ? CUDA_ARRAY3D_SURFACE_LDST : 0u)
			)
		};
		CU_CALL_RET(cuArray3DCreate(&image, &desc),
					"failed to create cuda array/image", false);
		
		// copy host memory to device if it is non-null and NO_INITIAL_COPY is not specified
		if(copy_host_data &&
		   host_ptr != nullptr &&
		   (flags & COMPUTE_MEMORY_FLAG::NO_INITIAL_COPY) != COMPUTE_MEMORY_FLAG::NONE) {
			CU_CALL_RET(cuMemcpyHtoA(image, 0, host_ptr, image_data_size),
						"failed to copy initial host data to device", false);
		}
	}
	// -> opengl image
	else {
		if(!create_gl_image(copy_host_data)) return false;
		
		// register the cuda object
		uint32_t cuda_gl_flags = 0;
		switch(flags & COMPUTE_MEMORY_FLAG::OPENGL_READ_WRITE) {
			case COMPUTE_MEMORY_FLAG::OPENGL_READ:
				cuda_gl_flags = CU_GRAPHICS_REGISTER_FLAGS_READ_ONLY;
				break;
			case COMPUTE_MEMORY_FLAG::OPENGL_WRITE:
				cuda_gl_flags = CU_GRAPHICS_REGISTER_FLAGS_WRITE_DISCARD;
				break;
			default:
			case COMPUTE_MEMORY_FLAG::OPENGL_READ_WRITE:
				cuda_gl_flags = CU_GRAPHICS_REGISTER_FLAGS_NONE;
				break;
		}
		CU_CALL_RET(cuGraphicsGLRegisterImage(&rsrc, gl_object, opengl_type, cuda_gl_flags),
					"failed to register opengl image with cuda", false);
		if(rsrc == nullptr) {
			log_error("created cuda gl graphics resource is invalid!");
			return false;
		}
		// release -> acquire for use with cuda
		release_opengl_object(cqueue);
	}
	
	// create texture/surface objects, depending on read/write flags and sampler support (TODO: and sm_xy)
	CUDA_RESOURCE_DESC rsrc_desc;
	CUDA_RESOURCE_VIEW_DESC rsrc_view_desc;
	CUDA_TEXTURE_DESC tex_desc;
	memset(&rsrc_desc, 0, sizeof(CUDA_RESOURCE_DESC));
	memset(&rsrc_view_desc, 0, sizeof(CUDA_RESOURCE_VIEW_DESC));
	memset(&tex_desc, 0, sizeof(CUDA_TEXTURE_DESC));
	
	// TODO: support CU_RESOURCE_TYPE_MIPMAPPED_ARRAY + CU_RESOURCE_TYPE_LINEAR
	rsrc_desc.resType = CU_RESOURCE_TYPE_ARRAY;
	rsrc_desc.res.array.hArray = image;
	rsrc_desc.flags = 0; // must be 0
	
	if(need_tex) {
		CU_CALL_RET(cuTexObjectCreate(&texture, &rsrc_desc, &tex_desc, &rsrc_view_desc),
					"failed to create texture object", false);
	}
	if(need_surf) {
		CU_CALL_RET(cuSurfObjectCreate(&surface, &rsrc_desc),
					"failed to create surface object", false);
	}
	
	return true;
}

cuda_image::~cuda_image() {
	// kill the image
	if(image == nullptr) return;
	
	if(texture != 0) {
		CU_CALL_NO_ACTION(cuTexObjectDestroy(texture),
						  "failed to destroy texture object");
	}
	if(surface != 0) {
		CU_CALL_NO_ACTION(cuSurfObjectDestroy(surface),
						  "failed to destroy surface object");
	}
	
	// -> cuda array
	if(has_flag<COMPUTE_MEMORY_FLAG::OPENGL_SHARING>(flags)) {
		CU_CALL_RET(cuArrayDestroy(image),
					"failed to free device memory");
	}
	// -> opengl image
	else {
		if(gl_object == 0) {
			log_error("invalid opengl image!");
		}
		else {
			if(image == nullptr || gl_object_state) {
				log_warn("image still acquired for opengl use - release before destructing a compute image!");
			}
			// kill opengl image
			if(!gl_object_state) acquire_opengl_object(nullptr); // -> release to opengl
			delete_gl_image();
		}
	}
}

// TODO: merge with cuda_buffer
bool cuda_image::acquire_opengl_object(shared_ptr<compute_queue> cqueue) {
	if(gl_object == 0) return false;
	if(image == nullptr) return false;
	if(rsrc == nullptr) return false;
	if(gl_object_state) {
		log_warn("opengl image has already been acquired for opengl use!");
		return true;
	}
	
	image = 0; // reset buffer pointer, this is no longer valid
	CU_CALL_RET(cuGraphicsUnmapResources(1, &rsrc,
										 (cqueue != nullptr ? (CUstream)cqueue->get_queue_ptr() : nullptr)),
				"failed to acquire opengl image - cuda resource unmapping failed!", false);
	gl_object_state = true;
	
	return true;
}

// TODO: merge with cuda_buffer
bool cuda_image::release_opengl_object(shared_ptr<compute_queue> cqueue) {
	if(gl_object == 0) return false;
	if(rsrc == nullptr) return false;
	if(!gl_object_state) {
		log_warn("opengl image has already been released to cuda!");
		return true;
	}
	
	CU_CALL_RET(cuGraphicsMapResources(1, &rsrc,
									   (cqueue != nullptr ? (CUstream)cqueue->get_queue_ptr() : nullptr)),
				"failed to release opengl image - cuda resource mapping failed!", false);
	gl_object_state = false;
	
	// TODO: handle opengl array/layers and mip-mapping
	CU_CALL_RET(cuGraphicsSubResourceGetMappedArray(&image, rsrc, 0, 0),
				"failed to retrieve mapped cuda image from opengl buffer!", false);
	
	if(image == nullptr) {
		log_error("mapped cuda image (from a graphics resource) is invalid!");
		return false;
	}
	
	return true;
}

#endif
