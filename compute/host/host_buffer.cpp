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

#include <floor/compute/host/host_buffer.hpp>

#if !defined(FLOOR_NO_HOST_COMPUTE)

#include <floor/core/logger.hpp>
#include <floor/compute/host/host_queue.hpp>
#include <floor/compute/host/host_device.hpp>
#include <floor/compute/host/host_compute.hpp>

host_buffer::host_buffer(const host_device* device,
						 const size_t& size_,
						 void* host_ptr_,
						 const COMPUTE_MEMORY_FLAG flags_,
						 const uint32_t opengl_type_,
						 const uint32_t external_gl_object_) :
compute_buffer(device, size_, host_ptr_, flags_, opengl_type_, external_gl_object_) {
	if(size < min_multiple()) return;

	// actually create the buffer
	if(!create_internal(true, nullptr)) {
		return; // can't do much else
	}
}

bool host_buffer::create_internal(const bool copy_host_data, shared_ptr<compute_queue> cqueue) {
	// TODO: handle the remaining flags + host ptr
	
	// always allocate host memory (even with opengl, memory needs to be copied somewhere)
	buffer = new uint8_t[size] alignas(1024);

	// -> normal host buffer
	if(!has_flag<COMPUTE_MEMORY_FLAG::OPENGL_SHARING>(flags)) {
		// copy host memory to "device" if it is non-null and NO_INITIAL_COPY is not specified
		if(copy_host_data &&
		   host_ptr != nullptr &&
		   !has_flag<COMPUTE_MEMORY_FLAG::NO_INITIAL_COPY>(flags)) {
			memcpy(buffer, host_ptr, size);
		}
	}
	// -> shared host/opengl buffer
	else {
		if(!create_gl_buffer(copy_host_data)) return false;
		
		// acquire for use with the host
		acquire_opengl_object(cqueue);
	}

	return true;
}

host_buffer::~host_buffer() {
	// first, release and kill the opengl buffer
	if(gl_object != 0) {
		if(gl_object_state) {
			log_warn("buffer still registered for opengl use - acquire before destructing a compute buffer!");
		}
		if(!gl_object_state) release_opengl_object(nullptr); // -> release to opengl
		delete_gl_buffer();
	}
	// then, also kill the host buffer
	if(buffer != nullptr) {
		delete [] buffer;
		buffer = nullptr;
	}
}

void host_buffer::read(shared_ptr<compute_queue> cqueue, const size_t size_, const size_t offset) {
	read(cqueue, host_ptr, size_, offset);
}

void host_buffer::read(shared_ptr<compute_queue> cqueue floor_unused, void* dst, const size_t size_, const size_t offset) {
	if(buffer == nullptr) return;

	const size_t read_size = (size_ == 0 ? size : size_);
	if(!read_check(size, read_size, offset)) return;

	GUARD(lock);
	memcpy(dst, buffer + offset, read_size);
}

void host_buffer::write(shared_ptr<compute_queue> cqueue, const size_t size_, const size_t offset) {
	write(cqueue, host_ptr, size_, offset);
}

void host_buffer::write(shared_ptr<compute_queue> cqueue floor_unused, const void* src, const size_t size_, const size_t offset) {
	if(buffer == nullptr) return;

	const size_t write_size = (size_ == 0 ? size : size_);
	if(!write_check(size, write_size, offset)) return;
	
	GUARD(lock);
	memcpy(buffer + offset, src, write_size);
}

void host_buffer::copy(shared_ptr<compute_queue> cqueue floor_unused,
					   shared_ptr<compute_buffer> src,
					   const size_t size_, const size_t src_offset, const size_t dst_offset) {
	if(buffer == nullptr) return;

	// use min(src size, dst size) as the default size if no size is specified
	const size_t src_size = src->get_size();
	const size_t copy_size = (size_ == 0 ? std::min(src_size, size) : size_);
	if(!copy_check(size, src_size, copy_size, dst_offset, src_offset)) return;
	
	src->_lock();
	_lock();
	
	memcpy(buffer + dst_offset, ((host_buffer*)src.get())->get_host_buffer_ptr() + src_offset, copy_size);
	
	_unlock();
	src->_unlock();
}

void host_buffer::fill(shared_ptr<compute_queue> cqueue floor_unused,
					   const void* pattern, const size_t& pattern_size,
					   const size_t size_, const size_t offset) {
	if(buffer == nullptr) return;

	const size_t fill_size = (size_ == 0 ? size : size_);
	if(!fill_check(size, fill_size, pattern_size, offset)) return;
	
	switch(pattern_size) {
		case 1:
			memset(buffer + offset, *(uint8_t*)pattern, fill_size);
			break;
#if defined(__APPLE__) // TODO: check for availability on linux, *bsd, windows
		// NOTE: memset_pattern* will simple truncate any overspill, so size checking is not necessary
		// NOTE: 3rd parameter is the fill/buffer size and has nothing to do with the pattern count
		case 4:
			memset_pattern4(buffer + offset, (const void*)pattern, fill_size);
			break;
		case 8:
			memset_pattern8(buffer + offset, (const void*)pattern, fill_size);
			break;
		case 16:
			memset_pattern16(buffer + offset, (const void*)pattern, fill_size);
			break;
#endif
		default: {
			// not a pattern size that allows a fast memset
			// -> copy pattern manually in a loop
			const size_t pattern_count = fill_size / pattern_size;
			uint8_t* write_ptr = buffer + offset;
			for(size_t i = 0; i < pattern_count; ++i) {
				memcpy(write_ptr, pattern, pattern_size);
				write_ptr += pattern_size;
			}
			break;
		}
	}
}

void host_buffer::zero(shared_ptr<compute_queue> cqueue floor_unused) {
	if(buffer == nullptr) return;

	GUARD(lock);
	memset(buffer, 0, size);
}

bool host_buffer::resize(shared_ptr<compute_queue> cqueue, const size_t& new_size_,
						 const bool copy_old_data, const bool copy_host_data,
						 void* new_host_ptr) {
	if(buffer == nullptr) return false;
	if(new_size_ == 0) {
		log_error("can't allocate a buffer of size 0!");
		return false;
	}
	if(copy_old_data && copy_host_data) {
		log_error("can't copy data both from the old buffer and the host pointer!");
		// still continue though, but assume just copy_old_data!
	}

	const size_t new_size = align_size(new_size_);
	if(new_size_ != new_size) {
		log_error("buffer size must always be a multiple of %u! - using size of %u instead of %u now",
				  min_multiple(), new_size, new_size_);
	}
	
	// store old buffer, size and host pointer for possible restore + cleanup later on
	const auto old_buffer = buffer;
	const auto old_size = size;
	const auto old_host_ptr = host_ptr;
	const auto restore_old_buffer = [this, &old_buffer, &old_size, &old_host_ptr] {
		buffer = old_buffer;
		size = old_size;
		host_ptr = old_host_ptr;
	};
	const bool is_host_buffer = has_flag<COMPUTE_MEMORY_FLAG::USE_HOST_MEMORY>(flags);
	
	// create the new buffer
	buffer = nullptr;
	size = new_size;
	host_ptr = new_host_ptr;
	if(!create_internal(copy_host_data, cqueue)) {
		// much fail, restore old buffer
		log_error("failed to create resized buffer");
		
		// restore old buffer and re-register when using host memory
		restore_old_buffer();
		
		return false;
	}
	
	// copy old data if specified
	if(copy_old_data) {
		// can only copy as many bytes as there are bytes
		const size_t copy_size = std::min(size, new_size); // >= 4, established above
		memcpy(buffer, old_buffer, copy_size);
	}
	else if(!copy_old_data && copy_host_data && is_host_buffer && host_ptr != nullptr) {
		memcpy(buffer, host_ptr, size);
	}
	
	// kill the old buffer
	if(old_buffer != nullptr) {
		delete [] old_buffer;
	}
	
	return true;
}

void* __attribute__((aligned(128))) host_buffer::map(shared_ptr<compute_queue> cqueue,
													 const COMPUTE_MEMORY_MAP_FLAG flags_,
													 const size_t size_, const size_t offset) {
	if(buffer == nullptr) return nullptr;
	
	const size_t map_size = (size_ == 0 ? size : size_);
	const bool blocking_map = has_flag<COMPUTE_MEMORY_MAP_FLAG::BLOCK>(flags_);
	if(!map_check(size, map_size, flags, flags_, offset)) return nullptr;

	if(blocking_map) {
		cqueue->finish();
	}
	
	// NOTE: this is returning a raw pointer to the internal buffer memory and specifically not creating+copying a new buffer
	// -> the user is always responsible for proper sync when mapping a buffer multiple times and this way, it should be
	// easier to detect any problems (race conditions, etc.)
	return buffer + offset;
}

void host_buffer::unmap(shared_ptr<compute_queue> cqueue floor_unused, void* __attribute__((aligned(128))) mapped_ptr) {
	if(buffer == nullptr) return;
	if(mapped_ptr == nullptr) return;

	// nop
}

bool host_buffer::acquire_opengl_object(shared_ptr<compute_queue> cqueue floor_unused) {
	if(gl_object == 0) return false;
	if(!gl_object_state) {
#if defined(FLOOR_DEBUG) && 0
		log_warn("opengl buffer has already been acquired for use with the host!");
#endif
		return true;
	}
	
	// copy gl buffer data to host memory (through r/o map)
	glBindBuffer(opengl_type, gl_object);
#if !defined(FLOOR_IOS)
	void* gl_data = glMapBuffer(opengl_type, GL_READ_ONLY);
#else
	void* gl_data = glMapBufferRange(opengl_type, 0, (GLsizeiptr)size, GL_MAP_READ_BIT);
#endif
	if(gl_data == nullptr) {
		log_error("failed to acquire opengl buffer - opengl buffer mapping failed");
		return false;
	}
	
	memcpy(buffer, gl_data, size);
	
	if(!glUnmapBuffer(opengl_type)) {
		log_error("opengl buffer unmapping failed");
	}
	glBindBuffer(opengl_type, 0);
	
	gl_object_state = false;
	return true;
}

bool host_buffer::release_opengl_object(shared_ptr<compute_queue> cqueue floor_unused) {
	if(gl_object == 0) return false;
	if(buffer == 0) return false;
	if(gl_object_state) {
#if defined(FLOOR_DEBUG) && 0
		log_warn("opengl buffer has already been released for opengl use!");
#endif
		return true;
	}
	
	// copy the host data to the gl buffer
	glBindBuffer(opengl_type, gl_object);
	glBufferSubData(opengl_type, 0, (GLsizeiptr)size, buffer);
	glBindBuffer(opengl_type, 0);
	
	gl_object_state = true;
	return true;
}

#endif
