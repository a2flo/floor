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

#include <floor/compute/metal/metal_image.hpp>

#if !defined(FLOOR_NO_METAL)

#include <floor/core/logger.hpp>
#include <floor/compute/metal/metal_queue.hpp>
#include <floor/compute/metal/metal_device.hpp>

// TODO: proper error (return) value handling everywhere

metal_image::metal_image(const metal_device* device,
						 const uint4 image_dim_,
						 const COMPUTE_IMAGE_TYPE image_type_,
						 void* host_ptr_,
						 const COMPUTE_MEMORY_FLAG flags_,
						 const uint32_t opengl_type_) :
compute_image(device, image_dim_, image_type_, host_ptr_, flags_, opengl_type_) {
	// TODO: implement this
	
	// actually create the image
	if(!create_internal(true)) {
		return; // can't do much else
	}
}

bool metal_image::create_internal(const bool copy_host_data floor_unused) {
	// TODO: implement this
	return false;
}

metal_image::~metal_image() {
	// TODO: kill the image
}

void* __attribute__((aligned(128))) metal_image::map(shared_ptr<compute_queue> cqueue floor_unused,
													 const COMPUTE_MEMORY_MAP_FLAG flags_ floor_unused) {
	if(image == nullptr) return nullptr;
	
	// TODO: implement this
	return nullptr;
}

void metal_image::unmap(shared_ptr<compute_queue> cqueue floor_unused, void* __attribute__((aligned(128))) mapped_ptr) {
	if(image == nullptr) return;
	if(mapped_ptr == nullptr) return;
	
	// TODO: implement this
}

bool metal_image::acquire_opengl_object(shared_ptr<compute_queue> cqueue floor_unused) {
	if(gl_object == 0) return false;
	if(image == nullptr) return false;
	if(gl_object_state) {
		log_warn("opengl image has already been acquired for opengl use!");
		return true;
	}
	
	// TODO: implement this
	gl_object_state = true;
	return true;
}

bool metal_image::release_opengl_object(shared_ptr<compute_queue> cqueue floor_unused) {
	if(gl_object == 0) return false;
	if(!gl_object_state) {
		log_warn("opengl image has already been released to opencl!");
		return true;
	}
	
	// TODO: implement this
	gl_object_state = false;
	return true;
}

#endif
