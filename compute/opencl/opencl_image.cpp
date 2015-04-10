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

// TODO: proper error (return) value handling everywhere

opencl_image::opencl_image(const opencl_device* device,
					   const uint4 image_dim_,
					   const COMPUTE_IMAGE_TYPE image_type_,
					   const COMPUTE_IMAGE_STORAGE_TYPE storage_type_,
					   const uint32_t channel_count_,
							 void* host_ptr_,
							 const COMPUTE_MEMORY_FLAG flags_,
							 const uint32_t opengl_type_) :
compute_image(device, image_dim_, image_type_, storage_type_, channel_count_, host_ptr_, flags_, opengl_type_) {
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
	
	// actually create the image
	if(!create_internal(true, nullptr)) {
		return; // can't do much else
	}
}

bool opencl_image::create_internal(const bool copy_host_data, shared_ptr<compute_queue> cqueue) {
	// TODO: implement this
	return true;
}

opencl_image::~opencl_image() {
	// kill the image
	if(image != nullptr) {
		clReleaseMemObject(image);
	}
	if(gl_object != 0) {
		if(gl_object_state) {
			log_warn("image still acquired for opengl use - release before destructing a compute image!");
		}
		if(!gl_object_state) acquire_opengl_object(nullptr); // -> release to opengl
		delete_gl_image();
	}
}

bool opencl_image::acquire_opengl_object(shared_ptr<compute_queue> cqueue) {
	if(gl_object == 0) return false;
	if(image == 0) return false;
	if(gl_object_state) {
		log_warn("opengl image has already been acquired for opengl use!");
		return true;
	}
	
	CL_CALL_RET(clEnqueueReleaseGLObjects((cl_command_queue)cqueue->get_queue_ptr(), 1, &image, 0, nullptr, nullptr),
				"failed to acquire opengl image - opencl gl object release failed", false);
	gl_object_state = true;
	return true;
}

bool opencl_image::release_opengl_object(shared_ptr<compute_queue> cqueue) {
	if(gl_object == 0) return false;
	if(!gl_object_state) {
		log_warn("opengl image has already been released to opencl!");
		return true;
	}
	
	CL_CALL_RET(clEnqueueAcquireGLObjects((cl_command_queue)cqueue->get_queue_ptr(), 1, &image, 0, nullptr, nullptr),
				"failed to release opengl image - opencl gl object acquire failed", false);
	gl_object_state = false;
	return true;
}

#endif
