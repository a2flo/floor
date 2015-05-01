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

#include <floor/compute/compute_buffer.hpp>
#include <floor/core/logger.hpp>

compute_buffer::compute_buffer(const void* device,
							   const size_t& size_,
							   void* host_ptr_,
							   const COMPUTE_MEMORY_FLAG flags_,
							   const uint32_t opengl_type_) :
compute_memory(device, host_ptr_, flags_, opengl_type_), size(align_size(size_)) {
	if(size == 0) {
		log_error("can't allocate a buffer of size 0!");
	}
	else if(size_ != size) {
		log_error("buffer size must always be a multiple of %u! - using size of %u instead of %u now",
				  min_multiple(), size, size_);
	}
#if defined(PLATFORM_X64)
	else if(size > 0xFFFFFFFFu && (flags & COMPUTE_MEMORY_FLAG::OPENGL_SHARING) != COMPUTE_MEMORY_FLAG::NONE) {
		log_error("using a buffer larger than 4GiB is not supported when using OpenGL sharing!");
		size = 0xFFFFFFFFu;
	}
#endif
}

compute_buffer::~compute_buffer() {}

void compute_buffer::delete_gl_buffer() {
	if(has_external_gl_object) return;
	if(gl_object == 0) return;
	glDeleteBuffers(1, &gl_object);
	gl_object = 0;
}

bool compute_buffer::create_gl_buffer(const bool copy_host_data) {
	if(has_external_gl_object) return true;
	
	// clean up previous gl buffer that might still exist
	delete_gl_buffer();
	
	// create and init the opengl buffer
	glGenBuffers(1, &gl_object);
	glBindBuffer(opengl_type, gl_object);
	if(gl_object == 0 || !glIsBuffer(gl_object)) {
		log_error("created opengl buffer %u is invalid!", gl_object);
		return false;
	}
	
	if(copy_host_data &&
	   host_ptr != nullptr &&
	   (flags & COMPUTE_MEMORY_FLAG::NO_INITIAL_COPY) != COMPUTE_MEMORY_FLAG::NONE) {
		glBufferData(opengl_type, (GLsizeiptr)size, host_ptr, GL_DYNAMIC_DRAW);
	}
	else {
		glBufferData(opengl_type, (GLsizeiptr)size, nullptr, GL_DYNAMIC_DRAW);
	}
	
	return true;
}
