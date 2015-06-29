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

	// actually create the buffer
	if(!create_internal(true, nullptr)) {
		return; // can't do much else
	}
}

bool host_buffer::create_internal(const bool copy_host_data, shared_ptr<compute_queue> cqueue) {
	// TODO: handle the remaining flags + host ptr

	// -> normal host buffer
	if(!has_flag<COMPUTE_MEMORY_FLAG::OPENGL_SHARING>(flags)) {
		// TODO: implement this!
	}
	// -> shared host/opengl buffer
	else {
		if(!create_gl_buffer(copy_host_data)) return false;
		
		// TODO: implement this!
		
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
		// TODO: implement this!
	}
}

void host_buffer::read(shared_ptr<compute_queue> cqueue, const size_t size_, const size_t offset) {
	read(cqueue, host_ptr, size_, offset);
}

void host_buffer::read(shared_ptr<compute_queue> cqueue, void* dst, const size_t size_, const size_t offset) {
	if(buffer == nullptr) return;

	const size_t read_size = (size_ == 0 ? size : size_);
	if(!read_check(size, read_size, offset)) return;

	// TODO: implement this!
}

void host_buffer::write(shared_ptr<compute_queue> cqueue, const size_t size_, const size_t offset) {
	write(cqueue, host_ptr, size_, offset);
}

void host_buffer::write(shared_ptr<compute_queue> cqueue, const void* src, const size_t size_, const size_t offset) {
	if(buffer == nullptr) return;

	const size_t write_size = (size_ == 0 ? size : size_);
	if(!write_check(size, write_size, offset)) return;

	// TODO: implement this!
}

void host_buffer::copy(shared_ptr<compute_queue> cqueue,
					   shared_ptr<compute_buffer> src,
					   const size_t size_, const size_t src_offset, const size_t dst_offset) {
	if(buffer == nullptr) return;

	// use min(src size, dst size) as the default size if no size is specified
	const size_t src_size = src->get_size();
	const size_t copy_size = (size_ == 0 ? std::min(src_size, size) : size_);
	if(!copy_check(size, src_size, copy_size, dst_offset, src_offset)) return;

	// TODO: implement this!
}

void host_buffer::fill(shared_ptr<compute_queue> cqueue,
					   const void* pattern, const size_t& pattern_size,
					   const size_t size_, const size_t offset) {
	if(buffer == nullptr) return;

	const size_t fill_size = (size_ == 0 ? size : size_);
	if(!fill_check(size, fill_size, pattern_size, offset)) return;

	// TODO: implement this!
}

void host_buffer::zero(shared_ptr<compute_queue> cqueue) {
	if(buffer == nullptr) return;

	// TODO: implement this!
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

	// TODO: implement this!
	return false;
}

void* __attribute__((aligned(128))) host_buffer::map(shared_ptr<compute_queue> cqueue,
													 const COMPUTE_MEMORY_MAP_FLAG flags_,
													 const size_t size_, const size_t offset) {
	if(buffer == nullptr) return nullptr;

	// TODO: implement this!
	return nullptr;
}

void host_buffer::unmap(shared_ptr<compute_queue> cqueue, void* __attribute__((aligned(128))) mapped_ptr) {
	if(buffer == nullptr) return;
	if(mapped_ptr == nullptr) return;

	// TODO: implement this!
}

bool host_buffer::acquire_opengl_object(shared_ptr<compute_queue> cqueue) {
	if(gl_object == 0) return false;
	if(!gl_object_state) {
		log_warn("opengl buffer has already been acquired for use with the host!");
		return true;
	}

	// TODO: implement this!
	gl_object_state = false;
	return true;
}

bool host_buffer::release_opengl_object(shared_ptr<compute_queue> cqueue) {
	if(gl_object == 0) return false;
	if(buffer == 0) return false;
	if(gl_object_state) {
		log_warn("opengl buffer has already been released for opengl use!");
		return true;
	}

	// TODO: implement this!
	gl_object_state = true;
	return true;
}

#endif
