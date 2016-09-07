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

#include <floor/compute/vulkan/vulkan_image.hpp>

#if !defined(FLOOR_NO_VULKAN)

#include <floor/core/logger.hpp>
#include <floor/compute/vulkan/vulkan_queue.hpp>
#include <floor/compute/vulkan/vulkan_device.hpp>
#include <floor/compute/vulkan/vulkan_compute.hpp>

// TODO: proper error (return) value handling everywhere

vulkan_image::vulkan_image(const vulkan_device* device,
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
	if(host_ptr_ != nullptr && !has_flag<COMPUTE_MEMORY_FLAG::NO_INITIAL_COPY>(flags)) {
		// TODO: flag?
	}
	
	// actually create the image
	if(!create_internal(true, nullptr)) {
		return; // can't do much else
	}
}

bool vulkan_image::create_internal(const bool copy_host_data floor_unused, shared_ptr<compute_queue> cqueue floor_unused) {
	// TODO: implement this
	return false;
}

vulkan_image::~vulkan_image() {
	// TODO: implement this
}

void vulkan_image::zero(shared_ptr<compute_queue> cqueue floor_unused) {
	if(image == nullptr) return;
	// TODO: implement this
}

void* __attribute__((aligned(128))) vulkan_image::map(shared_ptr<compute_queue> cqueue floor_unused,
													  const COMPUTE_MEMORY_MAP_FLAG flags_ floor_unused) {
	if(image == nullptr) return nullptr;
	
	// TODO: implement this
	return nullptr;
}

void vulkan_image::unmap(shared_ptr<compute_queue> cqueue floor_unused,
						 void* __attribute__((aligned(128))) mapped_ptr) {
	if(image == nullptr) return;
	if(mapped_ptr == nullptr) return;
	
	// TODO: implement this
}

bool vulkan_image::acquire_opengl_object(shared_ptr<compute_queue>) {
	log_error("not supported by vulkan");
	return false;
}

bool vulkan_image::release_opengl_object(shared_ptr<compute_queue>) {
	log_error("not supported by vulkan");
	return false;
}

#endif
