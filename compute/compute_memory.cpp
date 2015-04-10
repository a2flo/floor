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

#include <floor/compute/compute_memory.hpp>
#include <floor/core/logger.hpp>

static constexpr COMPUTE_MEMORY_FLAG handle_memory_flags(COMPUTE_MEMORY_FLAG flags, const uint32_t opengl_type) {
	// opengl sharing handling
	if((flags & COMPUTE_MEMORY_FLAG::OPENGL_SHARING) != COMPUTE_MEMORY_FLAG::NONE) {
		// need to make sure that opengl read/write flags are set (it isn't a good idea to
		// set OPENGL_READ_WRITE as the default parameter, because opengl sharing might
		// not be enabled/set)
		if((flags & COMPUTE_MEMORY_FLAG::OPENGL_READ_WRITE) == COMPUTE_MEMORY_FLAG::NONE) {
			// neither read nor write is set -> set read/write
			flags |= COMPUTE_MEMORY_FLAG::OPENGL_READ_WRITE;
		}
		
		// check if specified opengl type is valid
		if(opengl_type == 0) {
			log_error("OpenGL sharing has been set, but no OpenGL object type has been specified!");
		}
		// TODO: check for known opengl types? how to deal with unknown ones?
		
		// clear out USE_HOST_MEMORY flag if it is set
		if((flags & COMPUTE_MEMORY_FLAG::USE_HOST_MEMORY) != COMPUTE_MEMORY_FLAG::NONE) {
			flags &= (COMPUTE_MEMORY_FLAG)~uint32_t(COMPUTE_MEMORY_FLAG::USE_HOST_MEMORY);
		}
	}
	else {
		// clear out opengl flags, just in case
		flags &= (COMPUTE_MEMORY_FLAG)~uint32_t(COMPUTE_MEMORY_FLAG::OPENGL_SHARING |
												COMPUTE_MEMORY_FLAG::OPENGL_READ_WRITE);
	}
	
	// handle read/write flags
	if((flags & COMPUTE_MEMORY_FLAG::READ_WRITE) == COMPUTE_MEMORY_FLAG::NONE) {
		// neither read nor write is set -> set read/write
		flags |= COMPUTE_MEMORY_FLAG::READ_WRITE;
	}
	
	return flags;
}

compute_memory::compute_memory(const void* device,
							   void* host_ptr_,
							   const COMPUTE_MEMORY_FLAG flags_,
							   const uint32_t opengl_type_) :
dev(device), host_ptr(host_ptr_), flags(handle_memory_flags(flags_, opengl_type_)), opengl_type(opengl_type_) {
	if((flags_ & COMPUTE_MEMORY_FLAG::READ_WRITE) == COMPUTE_MEMORY_FLAG::NONE) {
		log_error("memory must be read-only, write-only or read-write!");
	}
	if((flags_ & COMPUTE_MEMORY_FLAG::USE_HOST_MEMORY) != COMPUTE_MEMORY_FLAG::NONE &
	   (flags_ & COMPUTE_MEMORY_FLAG::OPENGL_SHARING) != COMPUTE_MEMORY_FLAG::NONE) {
		log_error("USE_HOST_MEMORY and OPENGL_SHARING are mutually exclusive!");
	}
}

compute_memory::~compute_memory() {}

void compute_memory::_lock() {
	lock.lock();
}

void compute_memory::_unlock() {
	lock.unlock();
}
