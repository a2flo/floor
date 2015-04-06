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

static constexpr COMPUTE_BUFFER_FLAG handle_buffer_flags(COMPUTE_BUFFER_FLAG flags) {
	// opengl sharing handling
	if((flags & COMPUTE_BUFFER_FLAG::OPENGL_SHARING) != COMPUTE_BUFFER_FLAG::NONE) {
		// need to make sure that opengl read/write flags are set (it isn't a good idea to
		// set OPENGL_READ_WRITE as the default parameter, because opengl sharing might
		// not be enabled/set)
		if((flags & COMPUTE_BUFFER_FLAG::OPENGL_READ_WRITE) == COMPUTE_BUFFER_FLAG::NONE) {
			// neither read nor write is set -> set read/write
			flags |= COMPUTE_BUFFER_FLAG::OPENGL_READ_WRITE;
		}
		
		// check if specified opengl buffer type is valid
		if((flags & COMPUTE_BUFFER_FLAG::__OPENGL_BUFFER_TYPE_MASK) > COMPUTE_BUFFER_FLAG::__OPENGL_MAX_BUFFER_TYPE) {
			// invalid type -> default to array buffer
			flags &= (COMPUTE_BUFFER_FLAG)~uint32_t(COMPUTE_BUFFER_FLAG::__OPENGL_BUFFER_TYPE_MASK);
			flags |= COMPUTE_BUFFER_FLAG::OPENGL_ARRAY_BUFFER;
		}
		else if((flags & COMPUTE_BUFFER_FLAG::__OPENGL_BUFFER_TYPE_MASK) == COMPUTE_BUFFER_FLAG::NONE) {
			// default to array buffer
			flags |= COMPUTE_BUFFER_FLAG::OPENGL_ARRAY_BUFFER;
		}
		
		// clear out USE_HOST_MEMORY flag if it is set
		if((flags & COMPUTE_BUFFER_FLAG::USE_HOST_MEMORY) != COMPUTE_BUFFER_FLAG::NONE) {
			flags &= (COMPUTE_BUFFER_FLAG)~uint32_t(COMPUTE_BUFFER_FLAG::USE_HOST_MEMORY);
		}
		
#if defined(FLOOR_IOS)
		// there are no opengl texture buffers in opengl es 3.0
		if((flags & COMPUTE_BUFFER_FLAG::OPENGL_TEXTURE_BUFFER) != COMPUTE_BUFFER_FLAG::NONE) {
			flags &= (COMPUTE_BUFFER_FLAG)~uint32_t(COMPUTE_BUFFER_FLAG::OPENGL_TEXTURE_BUFFER);
		}
#endif
	}
	else {
		// clear out opengl flags, just in case
		flags &= (COMPUTE_BUFFER_FLAG)~uint32_t(COMPUTE_BUFFER_FLAG::OPENGL_SHARING |
												COMPUTE_BUFFER_FLAG::OPENGL_READ_WRITE |
												COMPUTE_BUFFER_FLAG::__OPENGL_BUFFER_TYPE_MASK);
	}
	
	// handle read/write flags
	if((flags & COMPUTE_BUFFER_FLAG::READ_WRITE) == COMPUTE_BUFFER_FLAG::NONE) {
		// neither read nor write is set -> set read/write
		flags |= COMPUTE_BUFFER_FLAG::READ_WRITE;
	}
	
	return flags;
}

compute_buffer::compute_buffer(const void* device,
							   const size_t& size_,
							   void* host_ptr_,
							   const COMPUTE_BUFFER_FLAG flags_) :
dev(device), size(align_size(size_)), host_ptr(host_ptr_), flags(handle_buffer_flags(flags_)) {
	if(size == 0) {
		log_error("can't allocate a buffer of size 0!");
	}
	else if(size_ != size) {
		log_error("buffer size must always be a multiple of %u! - using size of %u instead of %u now",
				  min_multiple(), size, size_);
	}
	
	if((flags_ & COMPUTE_BUFFER_FLAG::READ_WRITE) == COMPUTE_BUFFER_FLAG::NONE) {
		log_error("buffer must be read-only, write-only or read-write!");
	}
	if((flags_ & COMPUTE_BUFFER_FLAG::USE_HOST_MEMORY) != COMPUTE_BUFFER_FLAG::NONE &
	   (flags_ & COMPUTE_BUFFER_FLAG::OPENGL_SHARING) != COMPUTE_BUFFER_FLAG::NONE) {
		log_error("USE_HOST_MEMORY and OPENGL_SHARING are mutually exclusive!");
	}
	if((flags & COMPUTE_BUFFER_FLAG::__OPENGL_BUFFER_TYPE_MASK) > COMPUTE_BUFFER_FLAG::__OPENGL_MAX_BUFFER_TYPE) {
		log_error("invalid opengl buffer type!");
	}
	
#if defined(FLOOR_IOS)
	if((flags & COMPUTE_BUFFER_FLAG::OPENGL_TEXTURE_BUFFER) != COMPUTE_BUFFER_FLAG::NONE) {
		log_error("opengl texture buffers are not supported by opengl es 3.0 on iOS!");
	}
#endif
}

compute_buffer::~compute_buffer() {}

void compute_buffer::_lock() {
	lock.lock();
}

void compute_buffer::_unlock() {
	lock.unlock();
}

void compute_buffer::delete_gl_buffer() {
	if(gl_buffer == 0) return;
	glDeleteBuffers(1, &gl_buffer);
	gl_buffer = 0;
}

bool compute_buffer::create_gl_buffer(const bool copy_host_data) {
	// clean up previous gl buffer that might still exist
	delete_gl_buffer();
	
	// create and init the opengl buffer
	const auto gl_buffer_type = get_opengl_buffer_type();
	glGenBuffers(1, &gl_buffer);
	glBindBuffer(gl_buffer_type, gl_buffer);
	if(gl_buffer == 0 || !glIsBuffer(gl_buffer)) {
		log_error("created opengl buffer %u is invalid!", gl_buffer);
		return false;
	}
	
	if(copy_host_data &&
	   host_ptr != nullptr &&
	   (flags & COMPUTE_BUFFER_FLAG::NO_INITIAL_COPY) != COMPUTE_BUFFER_FLAG::NONE) {
		glBufferData(gl_buffer_type, (GLsizeiptr)size, host_ptr, GL_DYNAMIC_DRAW);
	}
	else {
		glBufferData(gl_buffer_type, (GLsizeiptr)size, nullptr, GL_DYNAMIC_DRAW);
	}
	
	return true;
}
