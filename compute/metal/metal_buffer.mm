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

#include <floor/compute/metal/metal_buffer.hpp>

#if !defined(FLOOR_NO_METAL)

#include <floor/core/logger.hpp>
#include <floor/compute/metal/metal_queue.hpp>
#include <floor/compute/metal/metal_device.hpp>

// TODO: proper error (return) value handling everywhere

metal_buffer::metal_buffer(const metal_device* device,
						   const size_t& size_,
						   void* host_ptr_,
						   const COMPUTE_MEMORY_FLAG flags_,
						   const uint32_t opengl_type_,
						   const uint32_t external_gl_object_) :
compute_buffer(device, size_, host_ptr_, flags_, opengl_type_, external_gl_object_) {
	if(size < min_multiple()) return;
	
	switch(flags & COMPUTE_MEMORY_FLAG::READ_WRITE) {
		case COMPUTE_MEMORY_FLAG::READ:
		case COMPUTE_MEMORY_FLAG::WRITE:
		case COMPUTE_MEMORY_FLAG::READ_WRITE:
			// no special handling for metal
			break;
		// all possible cases handled
		default: floor_unreachable();
	}
	
	switch(flags & COMPUTE_MEMORY_FLAG::HOST_READ_WRITE) {
		case COMPUTE_MEMORY_FLAG::HOST_READ:
		case COMPUTE_MEMORY_FLAG::HOST_READ_WRITE:
			// keep the default MTLCPUCacheModeDefaultCache
			break;
		case COMPUTE_MEMORY_FLAG::NONE:
		case COMPUTE_MEMORY_FLAG::HOST_WRITE:
			// host will only write or not read/write at all -> can use write combined
			options = MTLCPUCacheModeWriteCombined;
			break;
		// all possible cases handled
		default: floor_unreachable();
	}
	
	// TODO: handle the remaining flags + host ptr
	
	// actually create the buffer
	if(!create_internal(true)) {
		return; // can't do much else
	}
}

bool metal_buffer::create_internal(const bool copy_host_data) {
	// -> use host memory
	if((flags & COMPUTE_MEMORY_FLAG::USE_HOST_MEMORY) != COMPUTE_MEMORY_FLAG::NONE) {
		buffer = [((metal_device*)dev)->device newBufferWithBytesNoCopy:host_ptr length:size options:options
															deallocator:^(void*, NSUInteger) { /* nop */ }];
	}
	// -> alloc and use device memory
	else {
		// copy host memory to device if it is non-null and NO_INITIAL_COPY is not specified
		if(copy_host_data &&
		   host_ptr != nullptr &&
		   !has_flag<COMPUTE_MEMORY_FLAG::NO_INITIAL_COPY>(flags)) {
			buffer = [((metal_device*)dev)->device newBufferWithBytes:host_ptr length:size options:options];
		}
		// else: just create a buffer of the specified size
		else {
			buffer = [((metal_device*)dev)->device newBufferWithLength:size options:options];
		}
	}
	log_debug("created metal buffer: %X", buffer);
	return true;
}

metal_buffer::~metal_buffer() {
	// kill the buffer / auto-release
	buffer = nullptr;
}

void metal_buffer::read(shared_ptr<compute_queue> cqueue, const size_t size_, const size_t offset) {
	read(cqueue, host_ptr, size_, offset);
}

void metal_buffer::read(shared_ptr<compute_queue> cqueue, void* dst, const size_t size_, const size_t offset) {
	if(buffer == nullptr) return;
	
	const size_t read_size = (size_ == 0 ? size : size_);
	if(!read_check(size, read_size, offset)) return;
	
	// TODO: !
	GUARD(lock);
	memcpy(dst, (uint8_t*)[buffer contents] + offset, size_);
}

void metal_buffer::write(shared_ptr<compute_queue> cqueue, const size_t size_, const size_t offset) {
	write(cqueue, host_ptr, size_, offset);
}

void metal_buffer::write(shared_ptr<compute_queue> cqueue, const void* src, const size_t size_, const size_t offset) {
	if(buffer == nullptr) return;
	
	const size_t write_size = (size_ == 0 ? size : size_);
	if(!write_check(size, write_size, offset)) return;
	
	GUARD(lock);
	memcpy((uint8_t*)[buffer contents] + offset, src, write_size);
}

void metal_buffer::copy(shared_ptr<compute_queue> cqueue,
						shared_ptr<compute_buffer> src,
						const size_t size_, const size_t src_offset, const size_t dst_offset) {
	if(buffer == nullptr) return;
	
	const size_t src_size = src->get_size();
	const size_t copy_size = (size_ == 0 ? std::min(src_size, size) : size_);
	if(!copy_check(size, src_size, copy_size, dst_offset, src_offset)) return;
	
	// TODO: !
	GUARD(lock);
	memcpy((uint8_t*)[buffer contents] + dst_offset,
		   (uint8_t*)[((metal_buffer*)src.get())->get_metal_buffer() contents] + src_offset,
		   size_);
}

void metal_buffer::fill(shared_ptr<compute_queue> cqueue,
						const void* pattern, const size_t& pattern_size,
						const size_t size_, const size_t offset) {
	if(buffer == nullptr) return;
	
	const size_t fill_size = (size_ == 0 ? size : size_);
	if(!fill_check(size, fill_size, pattern_size, offset)) return;
	
	// TODO: !
	GUARD(lock);
	const size_t pattern_count = fill_size / pattern_size;
	switch(pattern_size) {
		case 1:
			fill_n((uint8_t*)[buffer contents] + offset, pattern_count, *(uint8_t*)pattern);
			break;
		case 2:
			fill_n((uint8_t*)[buffer contents] + offset, pattern_count, *(uint16_t*)pattern);
			break;
		case 4:
			fill_n((uint8_t*)[buffer contents] + offset, pattern_count, *(uint32_t*)pattern);
			break;
		default:
			// not a pattern size that allows a fast memset
			uint8_t* write_ptr = ((uint8_t*)[buffer contents]) + offset;
			for(size_t i = 0; i < pattern_count; i++) {
				memcpy(write_ptr, pattern, pattern_size);
				write_ptr += pattern_size;
			}
			break;
	}
}

void metal_buffer::zero(shared_ptr<compute_queue> cqueue) {
	if(buffer == nullptr) return;
	
	// TODO: !
	GUARD(lock);
	memset([buffer contents], 0, size);
}

bool metal_buffer::resize(shared_ptr<compute_queue> cqueue, const size_t& new_size_,
						  const bool copy_old_data, const bool copy_host_data,
						  void* new_host_ptr) {
	if(buffer == nullptr) return false;
	
	// TODO: !
	return false;
}

void* __attribute__((aligned(128))) metal_buffer::map(shared_ptr<compute_queue> cqueue floor_unused,
													  const COMPUTE_MEMORY_MAP_FLAG flags_,
													  const size_t size_, const size_t offset) {
	if(buffer == nullptr) return nullptr;
	
	const size_t map_size = (size_ == 0 ? size : size_);
	const bool blocking_map = ((flags_ & COMPUTE_MEMORY_MAP_FLAG::BLOCK) != COMPUTE_MEMORY_MAP_FLAG::NONE);
	if(!map_check(size, map_size, flags, flags_, offset)) return nullptr;
	
	_lock();
	return (void*)(((uint8_t*)[buffer contents]) + offset);
}

void metal_buffer::unmap(shared_ptr<compute_queue> cqueue, void* __attribute__((aligned(128))) mapped_ptr floor_unused) {
	//if(buffer == nullptr) return;
	//if(mapped_ptr == nullptr) return;
	_unlock();
}

bool metal_buffer::acquire_opengl_object(shared_ptr<compute_queue> cqueue) {
	if(gl_object == 0) return false;
	if(buffer == 0) return false;
	// TODO: implement this
	return true;
}

bool metal_buffer::release_opengl_object(shared_ptr<compute_queue> cqueue) {
	if(gl_object == 0) return false;
	// TODO: implement this
	return true;
}

#endif
