/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2016 Florian Ziesche
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

#include <floor/compute/host/host_image.hpp>

#if !defined(FLOOR_NO_HOST_COMPUTE)

#include <floor/core/logger.hpp>
#include <floor/compute/host/host_queue.hpp>
#include <floor/compute/host/host_device.hpp>
#include <floor/compute/host/host_compute.hpp>

#if defined(FLOOR_DEBUG)
static constexpr const size_t protection_size { 1024u };
static constexpr const uint8_t protection_byte { 0xA5 };
#else
static constexpr const size_t protection_size { 0u };
#endif

host_image::host_image(const host_device* device,
					   const uint4 image_dim_,
					   const COMPUTE_IMAGE_TYPE image_type_,
					   void* host_ptr_,
					   const COMPUTE_MEMORY_FLAG flags_,
					   const uint32_t opengl_type_,
					   const uint32_t external_gl_object_,
					   const opengl_image_info* gl_image_info) :
compute_image(device, image_dim_, image_type_, host_ptr_, flags_,
			  opengl_type_, external_gl_object_, gl_image_info) {
	// actually create the image
	if(!create_internal(true, nullptr)) {
		return; // can't do much else
	}
}

bool host_image::create_internal(const bool copy_host_data, shared_ptr<compute_queue> cqueue) {
	image = new uint8_t[image_data_size + protection_size] alignas(1024);
	program_info.buffer = image;
	program_info.runtime_image_type = image_type;
	program_info.image_dim = image_dim;
	program_info.image_clamp_dim.int_dim = image_dim - 1;
	program_info.image_clamp_dim.float_dim = program_info.image_clamp_dim.int_dim;
	
#if defined(FLOOR_DEBUG)
	// set protection bytes
	memset(image + image_data_size, protection_byte, protection_size);
#endif
	
	// -> normal host image
	if(!has_flag<COMPUTE_MEMORY_FLAG::OPENGL_SHARING>(flags)) {
		// copy host memory to "device" if it is non-null and NO_INITIAL_COPY is not specified
		if(copy_host_data &&
		   host_ptr != nullptr &&
		   !has_flag<COMPUTE_MEMORY_FLAG::NO_INITIAL_COPY>(flags)) {
			memcpy(image, host_ptr, image_data_size);
		}
	}
	// -> shared host/opengl image
	else {
#if !defined(FLOOR_IOS)
		if(!create_gl_image(copy_host_data)) return false;
#endif
		
		// acquire for use with the host
		acquire_opengl_object(cqueue);
	}
	
	return true;
}

host_image::~host_image() {
	// first, release and kill the opengl image
	if(gl_object != 0) {
		if(gl_object_state) {
			log_warn("image still registered for opengl use - acquire before destructing a compute image!");
		}
		if(!gl_object_state) release_opengl_object(nullptr); // -> release to opengl
#if !defined(FLOOR_IOS)
		delete_gl_image();
#endif
	}
	// then, also kill the host image
	if(image != nullptr) {
		delete [] image;
	}
}

void host_image::zero(shared_ptr<compute_queue> cqueue) {
	if(image == nullptr) return;
	
	cqueue->finish();
	memset(image, 0, image_data_size);
}

void* __attribute__((aligned(128))) host_image::map(shared_ptr<compute_queue> cqueue,
													const COMPUTE_MEMORY_MAP_FLAG flags_) {
	if(image == nullptr) return nullptr;
	
	const bool blocking_map = has_flag<COMPUTE_MEMORY_MAP_FLAG::BLOCK>(flags_);
	if(blocking_map) {
		cqueue->finish();
	}
	return image;
}

void host_image::unmap(shared_ptr<compute_queue> cqueue floor_unused, void* __attribute__((aligned(128))) mapped_ptr) {
	if(image == nullptr) return;
	if(mapped_ptr == nullptr) return;
	
	// nop
}

bool host_image::acquire_opengl_object(shared_ptr<compute_queue> cqueue floor_unused) {
#if !defined(FLOOR_IOS)
	if(gl_object == 0) return false;
	if(!gl_object_state) {
#if defined(FLOOR_DEBUG) && 0
		log_warn("opengl image has already been acquired for use with the host!");
#endif
		return true;
	}
	
	// copy gl image data to host memory (if read access is set)
	if(has_flag<COMPUTE_MEMORY_FLAG::READ>(flags)) {
		glBindTexture(opengl_type, gl_object);
		if(!has_flag<COMPUTE_IMAGE_TYPE::FLAG_CUBE>(image_type) ||
		   // contrary to GL_TEXTURE_CUBE_MAP, GL_TEXTURE_CUBE_MAP_ARRAY can be copied directly
		   (has_flag<COMPUTE_IMAGE_TYPE::FLAG_CUBE>(image_type) && has_flag<COMPUTE_IMAGE_TYPE::FLAG_ARRAY>(image_type))) {
			glGetTexImage(opengl_type, 0, gl_format, gl_type, image);
		}
		else {
			// must copy cube faces individually
			const auto face_size = image_slice_data_size_from_types(image_dim, image_type);
			auto img_ptr = image;
			for(uint32_t i = 0; i < 6; ++i) {
				glGetTexImage(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, gl_format, gl_type, img_ptr);
				img_ptr += face_size;
			}
		}
		glBindTexture(opengl_type, 0);
		
#if defined(FLOOR_DEBUG)
		// check memory protection strip
		for(size_t i = 0; i < protection_size; ++i) {
			if(*(image + image_data_size + i) != protection_byte) {
				log_error("DO PANIC: opengl wrote too many bytes: image: %X, gl object: %u, @ protection byte #%u, expected data size %u",
						  this, gl_object, i, image_data_size);
				logger::flush();
				break;
			}
		}
#endif
	}
	
	gl_object_state = false;
	return true;
#else
	log_error("this is not supported in iOS!");
	return false;
#endif
}

bool host_image::release_opengl_object(shared_ptr<compute_queue> cqueue floor_unused) {
#if !defined(FLOOR_IOS)
	if(gl_object == 0) return false;
	if(image == 0) return false;
	if(gl_object_state) {
#if defined(FLOOR_DEBUG) && 0
		log_warn("opengl image has already been released for opengl use!");
#endif
		return true;
	}
	
	// copy the host data back to the gl buffer (if write access is set)
	if(has_flag<COMPUTE_MEMORY_FLAG::WRITE>(flags)) {
		glBindTexture(opengl_type, gl_object);
		update_gl_image_data(image);
		glBindTexture(opengl_type, 0);
	}
	
	gl_object_state = true;
	return true;
#else
	log_error("this is not supported in iOS!");
	return false;
#endif
}

#endif
