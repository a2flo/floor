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

#include <floor/compute/host/host_image.hpp>

#if !defined(FLOOR_NO_HOST_COMPUTE)

#include <floor/core/logger.hpp>
#include <floor/compute/host/host_queue.hpp>
#include <floor/compute/host/host_device.hpp>
#include <floor/compute/host/host_compute.hpp>

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
	switch(flags & COMPUTE_MEMORY_FLAG::READ_WRITE) {
		case COMPUTE_MEMORY_FLAG::READ:
			break;
		case COMPUTE_MEMORY_FLAG::WRITE:
			break;
		case COMPUTE_MEMORY_FLAG::READ_WRITE:
			break;
			// all possible cases handled
		default: floor_unreachable();
	}
	
	switch(flags & COMPUTE_MEMORY_FLAG::HOST_READ_WRITE) {
		case COMPUTE_MEMORY_FLAG::HOST_READ:
			break;
		case COMPUTE_MEMORY_FLAG::HOST_WRITE:
			break;
		case COMPUTE_MEMORY_FLAG::HOST_READ_WRITE:
			// both - this is the default
			break;
		case COMPUTE_MEMORY_FLAG::NONE:
			break;
			// all possible cases handled
		default: floor_unreachable();
	}
	
	// TODO: handle the remaining flags + host ptr
	if(host_ptr_ != nullptr && !has_flag<COMPUTE_MEMORY_FLAG::NO_INITIAL_COPY>(flags)) {
		// "CL_MEM_COPY_HOST_PTR"
	}
	
	// actually create the image
	if(!create_internal(true, nullptr)) {
		return; // can't do much else
	}
}

bool host_image::create_internal(const bool copy_host_data, shared_ptr<compute_queue> cqueue) {
	// -> normal host image
	if(!has_flag<COMPUTE_MEMORY_FLAG::OPENGL_SHARING>(flags)) {
		// TODO: implement this!
	}
	// -> shared host/opengl image
	else {
		if(!create_gl_image(copy_host_data)) return false;
		
		// TODO: implement this!
		
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
		delete_gl_image();
	}
	// then, also kill the host image
	if(image != nullptr) {
		// TODO: implement this!
	}
}

void* __attribute__((aligned(128))) host_image::map(shared_ptr<compute_queue> cqueue, const COMPUTE_MEMORY_MAP_FLAG flags_) {
	if(image == nullptr) return nullptr;
	
	// TODO: implement this!
	return nullptr;
}

void host_image::unmap(shared_ptr<compute_queue> cqueue, void* __attribute__((aligned(128))) mapped_ptr) {
	if(image == nullptr) return;
	if(mapped_ptr == nullptr) return;
	
	// TODO: implement this!
}

bool host_image::acquire_opengl_object(shared_ptr<compute_queue> cqueue) {
	if(gl_object == 0) return false;
	if(!gl_object_state) {
		log_warn("opengl image has already been acquired for use with the host!");
		return true;
	}
	
	// TODO: implement this!
	gl_object_state = false;
	return true;
}

bool host_image::release_opengl_object(shared_ptr<compute_queue> cqueue) {
	if(gl_object == 0) return false;
	if(image == 0) return false;
	if(gl_object_state) {
		log_warn("opengl image has already been released for opengl use!");
		return true;
	}
	
	// TODO: implement this!
	gl_object_state = true;
	return true;
}

#endif
