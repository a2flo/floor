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

#include <floor/compute/cuda/cuda_buffer.hpp>

#if !defined(FLOOR_NO_CUDA)

#include <floor/core/logger.hpp>
#include <floor/compute/cuda/cuda_queue.hpp>

// TODO: remove the || 1 again
#if defined(FLOOR_DEBUG) || 1
#define FLOOR_DEBUG_COMPUTE_BUFFER 1
#endif

// TODO: proper error (return) value handling everywhere

cuda_buffer::cuda_buffer(const CUcontext ctx_ptr_,
						 const size_t& size_,
						 void* data,
						 const COMPUTE_BUFFER_FLAG flags_) :
compute_buffer(ctx_ptr_, size_, data, flags_) {
	switch(flags & COMPUTE_BUFFER_FLAG::READ_WRITE) {
		case COMPUTE_BUFFER_FLAG::READ:
		case COMPUTE_BUFFER_FLAG::WRITE:
		case COMPUTE_BUFFER_FLAG::READ_WRITE:
			// no special handling for cuda
			break;
		default:
			log_error("buffer must be read-only, write-only or read-write!");
			return;
	}
	
	switch(flags & COMPUTE_BUFFER_FLAG::HOST_READ_WRITE) {
		case COMPUTE_BUFFER_FLAG::HOST_READ:
		case COMPUTE_BUFFER_FLAG::HOST_WRITE:
		case COMPUTE_BUFFER_FLAG::NONE:
			// no special handling for cuda
			break;
		case COMPUTE_BUFFER_FLAG::HOST_READ_WRITE:
			// both - this is the default
			break;
		// all possible cases handled
		default: floor_unreachable();
	}
	
	// TODO: handle the remaining flags + host ptr
	
	// need to allocate the buffer on the correct device, if a context was specified,
	// else: assume the correct context is already active
	if(ctx_ptr_ != nullptr) {
		CU_CALL_RET(cuCtxSetCurrent(ctx_ptr_),
					"failed to make cuda context current");
	}
	
	// -> use host memory
	if((flags & COMPUTE_BUFFER_FLAG::USE_HOST_MEMORY) != COMPUTE_BUFFER_FLAG::NONE) {
		CU_CALL_RET(cuMemHostRegister(data, size, CU_MEMHOSTALLOC_DEVICEMAP | CU_MEMHOSTREGISTER_PORTABLE),
					"failed to register host pointer");
		CU_CALL_RET(cuMemHostGetDevicePointer(&buffer, data, 0),
					"failed to get device pointer for mapped host memory");
	}
	// -> alloc and use device memory
	else {
		CU_CALL_RET(cuMemAlloc(&buffer, size),
					"failed to allocate device memory");
		
		// copy host memory to device if it is non-null and NO_INITIAL_COPY is not specified
		if(data != nullptr &&
		   (flags & COMPUTE_BUFFER_FLAG::NO_INITIAL_COPY) != COMPUTE_BUFFER_FLAG::NONE) {
			CU_CALL_RET(cuMemcpyHtoD(buffer, data, size),
						"failed to copy initial host data to device");
		}
	}
}

cuda_buffer::~cuda_buffer() {
	// kill the buffer
	if(buffer == 0) return;
	
	// -> host memory
	if((flags & COMPUTE_BUFFER_FLAG::USE_HOST_MEMORY) != COMPUTE_BUFFER_FLAG::NONE) {
		CU_CALL_RET(cuMemHostUnregister(host_ptr),
					"failed to unregister mapped host memory");
	}
	// -> device memory
	else {
		CU_CALL_RET(cuMemFree(buffer),
					"failed to free device memory");
	}
}

void cuda_buffer::read(shared_ptr<compute_queue> cqueue, const size_t size_, const size_t offset) {
	read(cqueue, host_ptr, size_, offset);
}

void cuda_buffer::read(shared_ptr<compute_queue> cqueue, void* dst, const size_t size_, const size_t offset) {
	if(buffer == 0) return;
	// TODO: !
}

void cuda_buffer::write(shared_ptr<compute_queue> cqueue, const size_t size_, const size_t offset) {
	write(cqueue, host_ptr, size_, offset);
}

void cuda_buffer::write(shared_ptr<compute_queue> cqueue, const void* src, const size_t size_, const size_t offset) {
	if(buffer == 0) return;
	// TODO: !
}

void cuda_buffer::copy(shared_ptr<compute_queue> cqueue,
					   shared_ptr<compute_buffer> src,
					   const size_t size_, const size_t src_offset, const size_t dst_offset) {
	if(buffer == 0) return;
	// TODO: !
}

void cuda_buffer::fill(shared_ptr<compute_queue> cqueue,
					   const void* pattern, const size_t& pattern_size,
					   const size_t size_, const size_t offset) {
	if(buffer == 0) return;
	// TODO: !
}

void cuda_buffer::zero(shared_ptr<compute_queue> cqueue) {
	if(buffer == 0) return;
	// TODO: !
}

bool cuda_buffer::resize(shared_ptr<compute_queue> cqueue, const size_t& new_size, const bool copy_old_data) {
	if(buffer == 0) return false;
	// TODO: !
	return true;
}

void* __attribute__((aligned(128))) cuda_buffer::map(shared_ptr<compute_queue> cqueue floor_unused,
													 const COMPUTE_BUFFER_MAP_FLAG flags_,
													 const size_t size_, const size_t offset) {
	if(buffer == 0) return nullptr;
	
	const size_t map_size = (size_ == 0 ? size : size_);
	
	bool write_only = false;
	if((flags_ & COMPUTE_BUFFER_MAP_FLAG::WRITE_INVALIDATE) != COMPUTE_BUFFER_MAP_FLAG::NONE) {
		write_only = true;
		
#if defined(FLOOR_DEBUG_COMPUTE_BUFFER)
		if((flags_ & COMPUTE_BUFFER_MAP_FLAG::READ_WRITE) != COMPUTE_BUFFER_MAP_FLAG::NONE) {
			log_error("WRITE_INVALIDATE map flag is mutually exclusive with the READ and WRITE flags!");
			return nullptr;
		}
#endif
	}
	else {
		switch(flags_ & COMPUTE_BUFFER_MAP_FLAG::READ_WRITE) {
			case COMPUTE_BUFFER_MAP_FLAG::READ:
				write_only = false;
				break;
			case COMPUTE_BUFFER_MAP_FLAG::WRITE:
				write_only = true;
				break;
			case COMPUTE_BUFFER_MAP_FLAG::READ_WRITE:
				write_only = false;
				break;
			case COMPUTE_BUFFER_MAP_FLAG::NONE:
			default:
				log_error("neither read nor write flag set for buffer mapping!");
				return nullptr;
		}
	}
	
	// TODO: debug checks! check size/offset + flag combinations (host flags)
	
	// alloc host memory (NOTE: not going to use pinned memory here, b/c it has restrictions)
	alignas(128) unsigned char* host_buffer = new unsigned char[map_size] alignas(128);
	
	// check if we need to copy the buffer from the device (in case READ was specified)
	if(!write_only) {
		CU_CALL_NO_ACTION(cuMemcpyDtoH(host_buffer, buffer + offset, map_size),
						  "failed to copy device memory to host");
	}
	
	// need to remember how much we mapped and where (so the host->device copy copies the right amount of bytes)
	mappings.emplace(host_buffer, size2 { map_size, offset });
	
	return host_buffer;
}

void cuda_buffer::unmap(shared_ptr<compute_queue> cqueue floor_unused,
						void* __attribute__((aligned(128))) mapped_ptr) {
	if(buffer == 0) return;
	if(mapped_ptr == nullptr) return;
	
	// check if this is actually a mapped pointer (+get the mapped size)
	const auto iter = mappings.find(mapped_ptr);
	if(iter == mappings.end()) {
		log_error("invalid mapped pointer: %X", mapped_ptr);
		return;
	}
	
	CU_CALL_NO_ACTION(cuMemcpyHtoD(buffer + iter->second.y, mapped_ptr, iter->second.x),
					  "failed to copy host memory to device");
	
	// free host memory again and remove the mapping
	delete [] (unsigned char*)mapped_ptr;
	mappings.erase(mapped_ptr);
}

#endif
