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

#include <floor/floor/floor.hpp>
#include <floor/core/logger.hpp>
#include <floor/compute/cuda/cuda_queue.hpp>
#include <floor/compute/cuda/cuda_device.hpp>

// TODO: proper error (return) value handling everywhere

cuda_image::cuda_image(const cuda_device* device,
					   const uint4 image_dim_,
					   const COMPUTE_IMAGE_TYPE image_type_,
					   void* host_ptr_,
					   const COMPUTE_MEMORY_FLAG flags_,
					   const uint32_t opengl_type_,
					   const uint32_t external_gl_object_,
					   const opengl_image_info* gl_image_info) :
compute_image(device, image_dim_, image_type_, host_ptr_, flags_,
			  opengl_type_, external_gl_object_, gl_image_info) {
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
	
	//
	const auto dim_count = image_dim_count(image_type);
	auto channel_count = image_channel_count(image_type);
	if(channel_count == 3) {
		log_error("3-channel images are unsupported with cuda - using 4 channels instead now!");
		channel_count = 4;
		// TODO: make this work transparently by using an empty alpha channel (pun not intended ;)),
		// this will certainly segfault when using a host pointer that only points to 3-channel data
		// -> on the device: can also make sure it only returns a <type>3 vector
		return false;
	}
	
	size_t depth = 0;
	if(dim_count >= 3) {
		depth = image_dim.z;
	}
	else {
		// check array first ...
		if(has_flag<COMPUTE_IMAGE_TYPE::FLAG_ARRAY>(image_type)) {
			if(dim_count == 1) depth = image_dim.y;
			else if(dim_count >= 2) depth = image_dim.z;
		}
		// ... and check cube map second
		if(has_flag<COMPUTE_IMAGE_TYPE::FLAG_CUBE>(image_type)) {
			// if FLAG_ARRAY is also present, .z/depth has been specified by the user -> multiply by 6,
			// else, just specify 6 directly
			depth = (depth != 0 ? depth * 6 : 6);
		}
	}
	// TODO: cube map: make sure width == height
	
	//
	CUarray_format format;
	CUresourceViewFormat rsrc_view_format;
	
	static const unordered_map<COMPUTE_IMAGE_TYPE, pair<CUarray_format, CUresourceViewFormat>> format_lut {
		{ COMPUTE_IMAGE_TYPE::INT | COMPUTE_IMAGE_TYPE::FORMAT_8, { CU_AD_FORMAT_SIGNED_INT8, CU_RES_VIEW_FORMAT_SINT_1X8 } },
		{ COMPUTE_IMAGE_TYPE::INT | COMPUTE_IMAGE_TYPE::FORMAT_16, { CU_AD_FORMAT_SIGNED_INT16, CU_RES_VIEW_FORMAT_SINT_1X16 } },
		{ COMPUTE_IMAGE_TYPE::INT | COMPUTE_IMAGE_TYPE::FORMAT_32, { CU_AD_FORMAT_SIGNED_INT32, CU_RES_VIEW_FORMAT_SINT_1X32 } },
		{ COMPUTE_IMAGE_TYPE::UINT | COMPUTE_IMAGE_TYPE::FORMAT_8, { CU_AD_FORMAT_UNSIGNED_INT8, CU_RES_VIEW_FORMAT_UINT_1X8 } },
		{ COMPUTE_IMAGE_TYPE::UINT | COMPUTE_IMAGE_TYPE::FORMAT_16, { CU_AD_FORMAT_UNSIGNED_INT16, CU_RES_VIEW_FORMAT_UINT_1X16 } },
		{ COMPUTE_IMAGE_TYPE::UINT | COMPUTE_IMAGE_TYPE::FORMAT_24, { CU_AD_FORMAT_UNSIGNED_INT32, CU_RES_VIEW_FORMAT_UINT_1X32 } },
		{ COMPUTE_IMAGE_TYPE::UINT | COMPUTE_IMAGE_TYPE::FORMAT_32, { CU_AD_FORMAT_UNSIGNED_INT32, CU_RES_VIEW_FORMAT_UINT_1X32 } },
		{ COMPUTE_IMAGE_TYPE::FLOAT | COMPUTE_IMAGE_TYPE::FORMAT_16, { CU_AD_FORMAT_HALF, CU_RES_VIEW_FORMAT_FLOAT_1X16 } },
		{ COMPUTE_IMAGE_TYPE::FLOAT | COMPUTE_IMAGE_TYPE::FORMAT_32, { CU_AD_FORMAT_FLOAT, CU_RES_VIEW_FORMAT_FLOAT_1X32 } },
	};
	const auto cuda_format = format_lut.find(image_type & (COMPUTE_IMAGE_TYPE::__DATA_TYPE_MASK | COMPUTE_IMAGE_TYPE::__FORMAT_MASK));
	if(cuda_format == end(format_lut)) {
		log_error("unsupported image format: %X", image_type);
		return false;
	}
	format = cuda_format->second.first;
	rsrc_view_format = (CUresourceViewFormat)(cuda_format->second.second + (channel_count == 1 ? 0 :
																			(channel_count == 2 ? 1 : 2 /* 4 channels */)));
	
	// -> cuda array
	if(!has_flag<COMPUTE_MEMORY_FLAG::OPENGL_SHARING>(flags)) {
		const CUDA_ARRAY3D_DESCRIPTOR desc {
			.Width = image_dim.x,
			.Height = (dim_count >= 2 ? image_dim.y : 0),
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
		log_debug("surf/tex %u/%u; dim %u; %u %u %u; channels %u; flags %u; format: %X",
				  need_surf, need_tex, dim_count, desc.Width, desc.Height, desc.Depth, desc.NumChannels, desc.Flags, desc.Format);
		CU_CALL_RET(cuArray3DCreate(&image, &desc),
					"failed to create cuda array/image", false);
		
		// copy host memory to device if it is non-null and NO_INITIAL_COPY is not specified
		if(copy_host_data && host_ptr != nullptr && !has_flag<COMPUTE_MEMORY_FLAG::NO_INITIAL_COPY>(flags)) {
			log_debug("copying %u bytes from %X to array %X",
					  image_data_size, host_ptr, image);
			CUDA_MEMCPY3D mcpy3d;
			memset(&mcpy3d, 0, sizeof(CUDA_MEMCPY3D));
			mcpy3d.srcMemoryType = CU_MEMORYTYPE_HOST;
			mcpy3d.srcHost = host_ptr;
			mcpy3d.srcPitch = image_data_size / (std::max(desc.Height, size_t(1)) * std::max(desc.Depth, size_t(1)));
			mcpy3d.srcHeight = desc.Height;
			mcpy3d.dstMemoryType = CU_MEMORYTYPE_ARRAY;
			mcpy3d.dstArray = image;
			mcpy3d.dstXInBytes = 0;
			mcpy3d.WidthInBytes = mcpy3d.srcPitch;
			mcpy3d.Height = mcpy3d.srcHeight;
			mcpy3d.Depth = std::max(depth, size_t(1));
			CU_CALL_RET(cuMemcpy3D(&mcpy3d),
						"failed to copy initial host data to device", false);
		}
	}
	// -> opengl image
	else {
		if(!create_gl_image(copy_host_data)) return false;
		log_debug("surf/tex %u/%u", need_surf, need_tex);
		
		// cuda doesn't support depth textures
		// -> need to create a compatible texture and copy it on the gpu
		if(has_flag<COMPUTE_IMAGE_TYPE::FLAG_DEPTH>(image_type)) {
			// remove old
			if(depth_compat_tex != 0) {
				glDeleteTextures(1, &depth_compat_tex);
			}
			
			// check if the format can be used
			switch(gl_internal_format) {
				case GL_DEPTH_COMPONENT16:
					depth_compat_format = GL_R16UI;
					break;
				case GL_DEPTH_COMPONENT24:
				case GL_DEPTH_COMPONENT32:
					depth_compat_format = GL_R32UI;
					break;
				case GL_DEPTH_COMPONENT32F:
					depth_compat_format = GL_R32F;
					break;
				case GL_DEPTH32F_STENCIL8:
					depth_compat_format = GL_R32F;
					
					// correct view format, since stencil isn't supported
					rsrc_view_format = CU_RES_VIEW_FORMAT_FLOAT_1X32;
					break;
				default:
					log_error("can't share opengl depth format %X with cuda", gl_internal_format);
					return false;
			}
			
			//
			glGenTextures(1, &depth_compat_tex);
			glBindTexture(opengl_type, depth_compat_tex);
			glTexParameteri(opengl_type, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
			glTexParameteri(opengl_type, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
			glTexParameteri(opengl_type, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			glTexParameteri(opengl_type, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
			if(dim_count == 2) {
				glTexImage2D(opengl_type, 0, (GLint)depth_compat_format, (int)image_dim.x, (int)image_dim.y,
							 0, GL_RED, gl_type, nullptr);
			}
			else {
				glTexParameteri(opengl_type, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
				glTexImage3D(opengl_type, 0, (GLint)depth_compat_format, (int)image_dim.x, (int)image_dim.y, (int)image_dim.z,
							 0, GL_RED, gl_type, nullptr);
			}
			glBindTexture(opengl_type, 0);
		}
		
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
		CU_CALL_RET(cuGraphicsGLRegisterImage(&rsrc,
											  (depth_compat_tex == 0 ? gl_object : depth_compat_tex),
											  opengl_type, cuda_gl_flags),
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
		tex_desc.addressMode[0] = CU_TR_ADDRESS_MODE_CLAMP;
		if(dim_count >= 2) tex_desc.addressMode[1] = CU_TR_ADDRESS_MODE_CLAMP;
		if(dim_count >= 3) tex_desc.addressMode[2] = CU_TR_ADDRESS_MODE_CLAMP;
		tex_desc.filterMode = CU_TR_FILTER_MODE_POINT;
		tex_desc.flags = 0;
		tex_desc.maxAnisotropy = 1;
		tex_desc.mipmapFilterMode = CU_TR_FILTER_MODE_POINT;
		tex_desc.mipmapLevelBias = 0.0f;
		tex_desc.minMipmapLevelClamp = 0;
		tex_desc.maxMipmapLevelClamp = 0;
		
		rsrc_view_desc.format = rsrc_view_format;
		rsrc_view_desc.width = image_dim.x;
		rsrc_view_desc.height = (dim_count >= 2 ? image_dim.y : 0);
		rsrc_view_desc.depth = depth;
		// rest is already 0
		
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
	if(!has_flag<COMPUTE_MEMORY_FLAG::OPENGL_SHARING>(flags)) {
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
	
	// clean up depth compat object
	if(depth_compat_tex != 0) {
		glDeleteTextures(1, &depth_compat_tex);
	}
}

void* __attribute__((aligned(128))) cuda_image::map(shared_ptr<compute_queue> cqueue, const COMPUTE_MEMORY_MAP_FLAG flags_) {
	if(image == nullptr) return nullptr;
	
	// TODO: parameter origin + region
	const size_t dim_count = image_dim_count(image_type);
	size_t depth = 0;
	if(dim_count >= 3) {
		depth = image_dim.z;
	}
	else {
		// check array first ...
		if(has_flag<COMPUTE_IMAGE_TYPE::FLAG_ARRAY>(image_type)) {
			if(dim_count == 1) depth = image_dim.y;
			else if(dim_count >= 2) depth = image_dim.z;
		}
		// ... and check cube map second
		if(has_flag<COMPUTE_IMAGE_TYPE::FLAG_CUBE>(image_type)) {
			depth = (depth != 0 ? depth * 6 : 6);
		}
	}
	const size_t height = (dim_count >= 2 ? image_dim.y : 1);

	const size3 origin { 0, 0, 0 };
	const size3 region { size3(image_dim.x, height, depth).maxed(1) }; // complete image(s) + "The values in region cannot be 0."
	const auto map_size = region.x * region.y * region.z * image_bytes_per_pixel(image_type);
	
	const bool blocking_map = has_flag<COMPUTE_MEMORY_MAP_FLAG::BLOCK>(flags_);
	// TODO: image map check
	
	bool write_only = false;
	if(has_flag<COMPUTE_MEMORY_MAP_FLAG::WRITE_INVALIDATE>(flags_)) {
		write_only = true;
	}
	else {
		switch(flags_ & COMPUTE_MEMORY_MAP_FLAG::READ_WRITE) {
			case COMPUTE_MEMORY_MAP_FLAG::READ:
				write_only = false;
				break;
			case COMPUTE_MEMORY_MAP_FLAG::WRITE:
				write_only = true;
				break;
			case COMPUTE_MEMORY_MAP_FLAG::READ_WRITE:
				write_only = false;
				break;
			case COMPUTE_MEMORY_MAP_FLAG::NONE:
			default:
				log_error("neither read nor write flag set for image mapping!");
				return nullptr;
		}
	}
	
	// alloc host memory (NOTE: not going to use pinned memory here, b/c it has restrictions)
	alignas(128) unsigned char* host_buffer = new unsigned char[map_size] alignas(128);
	
	// check if we need to copy the image from the device (in case READ was specified)
	if(!write_only) {
		CUDA_MEMCPY3D mcpy3d;
		memset(&mcpy3d, 0, sizeof(CUDA_MEMCPY3D));
		mcpy3d.dstMemoryType = CU_MEMORYTYPE_HOST;
		mcpy3d.dstHost = host_buffer;
		mcpy3d.dstPitch = image_data_size / (region.y * region.z);
		mcpy3d.dstHeight = region.y;
		mcpy3d.srcMemoryType = CU_MEMORYTYPE_ARRAY;
		mcpy3d.srcArray = image;
		mcpy3d.srcXInBytes = 0;
		mcpy3d.WidthInBytes = mcpy3d.dstPitch;
		mcpy3d.Height = mcpy3d.dstHeight;
		mcpy3d.Depth = region.z;
	
		if(blocking_map) {
			// must finish up all current work before we can properly read from the current buffer
			cqueue->finish();
			
			CU_CALL_NO_ACTION(cuMemcpy3D(&mcpy3d),
							  "failed to copy device memory to host");
		}
		else {
			CU_CALL_NO_ACTION(cuMemcpy3DAsync(&mcpy3d, (CUstream)cqueue->get_queue_ptr()),
							  "failed to copy device memory to host");
		}
	}
	
	// need to remember how much we mapped and where (so the host->device copy copies the right amount of bytes)
	mappings.emplace(host_buffer, cuda_mapping { origin, region, flags_ });
	
	return host_buffer;
}

void cuda_image::unmap(shared_ptr<compute_queue> cqueue floor_unused,
					   void* __attribute__((aligned(128))) mapped_ptr) {
	if(image == nullptr) return;
	if(mapped_ptr == nullptr) return;
	
	// check if this is actually a mapped pointer (+get the mapped size)
	const auto iter = mappings.find(mapped_ptr);
	if(iter == mappings.end()) {
		log_error("invalid mapped pointer: %X", mapped_ptr);
		return;
	}
	
	// check if we need to actually copy data back to the device (not the case if read-only mapping)
	if(has_flag<COMPUTE_MEMORY_MAP_FLAG::WRITE>(iter->second.flags) ||
	   has_flag<COMPUTE_MEMORY_MAP_FLAG::WRITE_INVALIDATE>(iter->second.flags)) {
		CUDA_MEMCPY3D mcpy3d;
		memset(&mcpy3d, 0, sizeof(CUDA_MEMCPY3D));
		mcpy3d.srcMemoryType = CU_MEMORYTYPE_HOST;
		mcpy3d.srcHost = mapped_ptr;
		mcpy3d.srcPitch = image_data_size / (iter->second.region.y * iter->second.region.z);
		mcpy3d.srcHeight = iter->second.region.y;
		mcpy3d.dstMemoryType = CU_MEMORYTYPE_ARRAY;
		mcpy3d.dstArray = image;
		mcpy3d.dstXInBytes = 0;
		mcpy3d.WidthInBytes = mcpy3d.srcPitch;
		mcpy3d.Height = mcpy3d.srcHeight;
		mcpy3d.Depth = iter->second.region.z;
		CU_CALL_NO_ACTION(cuMemcpy3D(&mcpy3d),
						  "failed to copy host memory to device");
	}
	
	// free host memory again and remove the mapping
	delete [] (unsigned char*)mapped_ptr;
	mappings.erase(mapped_ptr);
}

bool cuda_image::acquire_opengl_object(shared_ptr<compute_queue> cqueue) {
	if(gl_object == 0) return false;
	if(image == nullptr) return false;
	if(rsrc == nullptr) return false;
	if(gl_object_state) {
		log_warn("opengl image has already been acquired for opengl use!");
		return true;
	}
	
	// if a depth compat texture is used, the cuda image must be copied to the opengl depth texture
	if(depth_compat_tex != 0 && has_flag<COMPUTE_MEMORY_FLAG::WRITE>(flags)) {
		if(floor::has_opengl_extension("GL_ARB_copy_image")) {
			glCopyImageSubData(depth_compat_tex, opengl_type, 0, 0, 0, 0,
							   gl_object, opengl_type, 0, 0, 0, 0,
							   (int)image_dim.x, (int)image_dim.y, std::max((int)image_dim.z, 1));
		}
		else {
			// TODO: shader copy?
		}
	}
	
	image = 0; // reset buffer pointer, this is no longer valid
	CU_CALL_RET(cuGraphicsUnmapResources(1, &rsrc,
										 (cqueue != nullptr ? (CUstream)cqueue->get_queue_ptr() : nullptr)),
				"failed to acquire opengl image - cuda resource unmapping failed!", false);
	gl_object_state = true;
	
	return true;
}

bool cuda_image::release_opengl_object(shared_ptr<compute_queue> cqueue) {
	if(gl_object == 0) return false;
	if(rsrc == nullptr) return false;
	if(!gl_object_state) {
		log_warn("opengl image has already been released to cuda!");
		return true;
	}
	
	// if a depth compat texture is used, the original opengl texture must by copied into it
	if(depth_compat_tex != 0 && has_flag<COMPUTE_MEMORY_FLAG::READ>(flags)) {
		if(floor::has_opengl_extension("GL_ARB_copy_image")) {
			glCopyImageSubData(gl_object, opengl_type, 0, 0, 0, 0,
							   depth_compat_tex, opengl_type, 0, 0, 0, 0,
							   (int)image_dim.x, (int)image_dim.y, std::max((int)image_dim.z, 1));
		}
		else {
			// TODO: shader copy
		}
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
