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
#include <floor/compute/cuda/cuda_device.hpp>

// TODO: proper error (return) value handling everywhere

cuda_buffer::cuda_buffer(const cuda_device* device,
						 const size_t& size_,
						 void* host_ptr_,
						 const COMPUTE_MEMORY_FLAG flags_,
						 const uint32_t opengl_type_) :
compute_buffer(device, size_, host_ptr_, flags_, opengl_type_) {
	if(size < min_multiple()) return;
	
	switch(flags & COMPUTE_MEMORY_FLAG::READ_WRITE) {
		case COMPUTE_MEMORY_FLAG::READ:
		case COMPUTE_MEMORY_FLAG::WRITE:
		case COMPUTE_MEMORY_FLAG::READ_WRITE:
			// no special handling for cuda
			break;
		// all possible cases handled
		default: floor_unreachable();
	}
	
	switch(flags & COMPUTE_MEMORY_FLAG::HOST_READ_WRITE) {
		case COMPUTE_MEMORY_FLAG::HOST_READ:
		case COMPUTE_MEMORY_FLAG::HOST_WRITE:
		case COMPUTE_MEMORY_FLAG::NONE:
			// no special handling for cuda
			break;
		case COMPUTE_MEMORY_FLAG::HOST_READ_WRITE:
			// both - this is the default
			break;
		// all possible cases handled
		default: floor_unreachable();
	}
	
	// TODO: handle the remaining flags + host ptr
	
	// need to allocate the buffer on the correct device, if a context was specified,
	// else: assume the correct context is already active
	if(device->ctx != nullptr) {
		CU_CALL_RET(cuCtxSetCurrent(device->ctx),
					"failed to make cuda context current");
	}
	
	// actually create the buffer
	if(!create_internal(true, nullptr)) {
		return; // can't do much else
	}
}

bool cuda_buffer::create_internal(const bool copy_host_data, shared_ptr<compute_queue> cqueue) {
	// -> use host memory
	if((flags & COMPUTE_MEMORY_FLAG::USE_HOST_MEMORY) != COMPUTE_MEMORY_FLAG::NONE) {
		CU_CALL_RET(cuMemHostRegister(host_ptr, size, CU_MEMHOSTALLOC_DEVICEMAP | CU_MEMHOSTREGISTER_PORTABLE),
					"failed to register host pointer", false);
		CU_CALL_RET(cuMemHostGetDevicePointer(&buffer, host_ptr, 0),
					"failed to get device pointer for mapped host memory", false);
	}
	// -> alloc and use device memory
	else {
		// -> plain old cuda buffer
		if((flags & COMPUTE_MEMORY_FLAG::OPENGL_SHARING) == COMPUTE_MEMORY_FLAG::NONE) {
			CU_CALL_RET(cuMemAlloc(&buffer, size),
						"failed to allocate device memory", false);
			
			// copy host memory to device if it is non-null and NO_INITIAL_COPY is not specified
			if(copy_host_data &&
			   host_ptr != nullptr &&
			   (flags & COMPUTE_MEMORY_FLAG::NO_INITIAL_COPY) != COMPUTE_MEMORY_FLAG::NONE) {
				CU_CALL_RET(cuMemcpyHtoD(buffer, host_ptr, size),
							"failed to copy initial host data to device", false);
			}
		}
		// -> opengl buffer
		else {
			if(!create_gl_buffer(copy_host_data)) return false;
			
			// register the cuda object
			uint32_t cuda_gl_flags = 0;
			switch(flags & COMPUTE_MEMORY_FLAG::OPENGL_READ_WRITE) {
				case COMPUTE_MEMORY_FLAG::OPENGL_READ:
					cuda_gl_flags = CU_GRAPHICS_REGISTER_FLAGS_READ_ONLY;
					break;
				case COMPUTE_MEMORY_FLAG::OPENGL_WRITE:
					cuda_gl_flags = CU_GRAPHICS_REGISTER_FLAGS_WRITE_DISCARD;
					break;
				default:
				case COMPUTE_MEMORY_FLAG::OPENGL_READ_WRITE:
					cuda_gl_flags = CU_GRAPHICS_REGISTER_FLAGS_NONE;
					break;
			}
			CU_CALL_RET(cuGraphicsGLRegisterBuffer(&rsrc, gl_object, cuda_gl_flags),
						"failed to register opengl buffer with cuda", false);
			if(rsrc == nullptr) {
				log_error("created cuda gl graphics resource is invalid!");
				return false;
			}
			// release -> acquire for use with cuda
			release_opengl_object(cqueue);
		}
	}
	return true;
}

cuda_buffer::~cuda_buffer() {
	// kill the buffer
	if(buffer == 0) return;
	
	// -> host memory
	if((flags & COMPUTE_MEMORY_FLAG::USE_HOST_MEMORY) != COMPUTE_MEMORY_FLAG::NONE) {
		CU_CALL_RET(cuMemHostUnregister(host_ptr),
					"failed to unregister mapped host memory");
	}
	// -> device memory
	else {
		// -> plain old cuda buffer
		if((flags & COMPUTE_MEMORY_FLAG::OPENGL_SHARING) == COMPUTE_MEMORY_FLAG::NONE) {
			CU_CALL_RET(cuMemFree(buffer),
						"failed to free device memory");
		}
		// -> opengl buffer
		else {
			if(gl_object == 0) {
				log_error("invalid opengl buffer!");
			}
			else {
				if(buffer == 0 || gl_object_state) {
					log_warn("buffer still acquired for opengl use - release before destructing a compute buffer!");
				}
				// kill opengl buffer
				if(!gl_object_state) acquire_opengl_object(nullptr); // -> release to opengl
				delete_gl_buffer();
			}
		}
	}
}

void cuda_buffer::read(shared_ptr<compute_queue> cqueue, const size_t size_, const size_t offset) {
	read(cqueue, host_ptr, size_, offset);
}

void cuda_buffer::read(shared_ptr<compute_queue> cqueue, void* dst, const size_t size_, const size_t offset) {
	if(buffer == 0) return;
	
	const size_t read_size = (size_ == 0 ? size : size_);
	if(!read_check(size, read_size, offset)) return;
	
	// TODO: blocking flag
	CU_CALL_RET(cuMemcpyDtoHAsync(dst, buffer + offset, read_size, (CUstream)cqueue->get_queue_ptr()),
				"failed to read memory from device");
}

void cuda_buffer::write(shared_ptr<compute_queue> cqueue, const size_t size_, const size_t offset) {
	write(cqueue, host_ptr, size_, offset);
}

void cuda_buffer::write(shared_ptr<compute_queue> cqueue, const void* src, const size_t size_, const size_t offset) {
	if(buffer == 0) return;
	
	const size_t write_size = (size_ == 0 ? size : size_);
	if(!write_check(size, write_size, offset)) return;
	
	// TODO: blocking flag
	CU_CALL_RET(cuMemcpyHtoDAsync(buffer + offset, src, write_size, (CUstream)cqueue->get_queue_ptr()),
				"failed to write memory to device");
}

void cuda_buffer::copy(shared_ptr<compute_queue> cqueue,
					   shared_ptr<compute_buffer> src,
					   const size_t size_, const size_t src_offset, const size_t dst_offset) {
	if(buffer == 0) return;
	
	// use min(src size, dst size) as the default size if no size is specified
	const size_t src_size = src->get_size();
	const size_t copy_size = (size_ == 0 ? std::min(src_size, size) : size_);
	if(!copy_check(size, src_size, copy_size, dst_offset, src_offset)) return;
	
	// TODO: blocking flag
	CU_CALL_RET(cuMemcpyDtoDAsync(buffer + dst_offset,
								  ((shared_ptr<cuda_buffer>&)src)->get_cuda_buffer() + src_offset,
								  copy_size, (CUstream)cqueue->get_queue_ptr()),
				"failed to copy memory on device");
}

void cuda_buffer::fill(shared_ptr<compute_queue> cqueue,
					   const void* pattern, const size_t& pattern_size,
					   const size_t size_, const size_t offset) {
	if(buffer == 0) return;
	
	const size_t fill_size = (size_ == 0 ? size : size_);
	if(!fill_check(size, fill_size, pattern_size, offset)) return;
	
	// TODO: blocking flag
	const size_t pattern_count = fill_size / pattern_size;
	switch(pattern_size) {
		case 1:
			CU_CALL_RET(cuMemsetD8Async(buffer + offset, *(uint8_t*)pattern, pattern_count, (CUstream)cqueue->get_queue_ptr()),
						"failed to fill device memory (8-bit memset)");
			break;
		case 2:
			CU_CALL_RET(cuMemsetD16Async(buffer + offset, *(uint16_t*)pattern, pattern_count, (CUstream)cqueue->get_queue_ptr()),
						"failed to fill device memory (16-bit memset)");
			break;
		case 4:
			CU_CALL_RET(cuMemsetD32Async(buffer + offset, *(uint32_t*)pattern, pattern_count, (CUstream)cqueue->get_queue_ptr()),
						"failed to fill device memory (32-bit memset)");
			break;
		default:
			// not a pattern size that allows a fast memset
			// -> create a host buffer with the pattern and upload it
			unsigned char* pattern_buffer = new unsigned char[fill_size];
			unsigned char* write_ptr = pattern_buffer;
			for(size_t i = 0; i < pattern_count; i++) {
				memcpy(write_ptr, pattern, pattern_size);
				write_ptr += pattern_size;
			}
			CU_CALL_NO_ACTION(cuMemcpyHtoD(buffer + offset, pattern_buffer, fill_size),
							  "failed to fill device memory (arbitrary memcpy)");
			delete [] pattern_buffer;
			break;
	}
}

void cuda_buffer::zero(shared_ptr<compute_queue> cqueue) {
	if(buffer == 0) return;
	static constexpr const uint32_t zero_pattern { 0u };
	fill(cqueue, &zero_pattern, sizeof(zero_pattern), 0, 0);
}

bool cuda_buffer::resize(shared_ptr<compute_queue> cqueue, const size_t& new_size_,
						 const bool copy_old_data, const bool copy_host_data,
						 void* new_host_ptr) {
	if(buffer == 0) return false;
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
	const bool is_host_buffer = ((flags & COMPUTE_MEMORY_FLAG::USE_HOST_MEMORY) != COMPUTE_MEMORY_FLAG::NONE);
	
	// unregister old host pointer if host memory is being used
	if(is_host_buffer) {
		CU_CALL_ERROR_EXEC(cuMemHostUnregister(host_ptr),
						   "failed to unregister mapped host memory",
						   restore_old_buffer();
						   return false;);
	}
	
	// create the new buffer
	buffer = 0;
	size = new_size;
	host_ptr = new_host_ptr;
	if(!create_internal(copy_host_data, cqueue)) {
		// much fail, restore old buffer
		log_error("failed to create resized buffer");
		
		// restore old buffer and re-register when using host memory
		restore_old_buffer();
		if(is_host_buffer) {
			// note that this can fail, leaving this buffer in a completely broken state
			CU_CALL_RET(cuMemHostRegister(host_ptr, size, CU_MEMHOSTALLOC_DEVICEMAP | CU_MEMHOSTREGISTER_PORTABLE),
						"failed to register host pointer", false);
			CU_CALL_RET(cuMemHostGetDevicePointer(&buffer, host_ptr, 0),
						"failed to get device pointer for mapped host memory", false);
		}
		return false;
	}
	
	// copy old data if specified
	if(copy_old_data) {
		// can only copy as many bytes as there are bytes
		const size_t copy_size = std::min(size, new_size); // >= 4, established above
		
		// must be blocking, because we're going to delete the old buffer in here
		CU_CALL_NO_ACTION(cuMemcpyDtoD(buffer, old_buffer, copy_size),
						  "failed to copy old data to new buffer while resizing buffer");
		// hard to decide what to do here, use new buffer with invalid data, or continue using the old one?
		// -> continue with new buffer as it has the correct/expected size
	}
	else if(!copy_old_data && copy_host_data && is_host_buffer && host_ptr != nullptr) {
		// can be done async, because the new host pointer continues to exist
		CU_CALL_RET(cuMemcpyHtoDAsync(buffer, host_ptr, size, (CUstream)cqueue->get_queue_ptr()),
					"failed to copy host data to new buffer while resizing buffer", false);
	}
	
	// kill the old buffer
	if(old_buffer != 0) {
		// -> device memory
		if(!is_host_buffer) {
			CU_CALL_RET(cuMemFree(old_buffer),
						"failed to free device memory", false); // can't do much if this fails
		}
		// else: -> host memory: nop, already unregistered earlier
	}
	
	return true;
}

void* __attribute__((aligned(128))) cuda_buffer::map(shared_ptr<compute_queue> cqueue,
													 const COMPUTE_MEMORY_MAP_FLAG flags_,
													 const size_t size_, const size_t offset) {
	if(buffer == 0) return nullptr;
	
	const size_t map_size = (size_ == 0 ? size : size_);
	const bool blocking_map = ((flags_ & COMPUTE_MEMORY_MAP_FLAG::BLOCK) != COMPUTE_MEMORY_MAP_FLAG::NONE);
	if(!map_check(size, map_size, flags, flags_, offset)) return nullptr;
	
	bool write_only = false;
	if((flags_ & COMPUTE_MEMORY_MAP_FLAG::WRITE_INVALIDATE) != COMPUTE_MEMORY_MAP_FLAG::NONE) {
		write_only = true;
	}
	else {
		switch(flags_ & COMPUTE_MEMORY_MAP_FLAG::READ_WRITE) {
			case COMPUTE_MEMORY_MAP_FLAG::READ:
				write_only = false;
				break;
			case COMPUTE_MEMORY_MAP_FLAG::WRITE:
				write_only = true;
				break;
			case COMPUTE_MEMORY_MAP_FLAG::READ_WRITE:
				write_only = false;
				break;
			case COMPUTE_MEMORY_MAP_FLAG::NONE:
			default:
				log_error("neither read nor write flag set for buffer mapping!");
				return nullptr;
		}
	}
	
	// alloc host memory (NOTE: not going to use pinned memory here, b/c it has restrictions)
	alignas(128) unsigned char* host_buffer = new unsigned char[map_size] alignas(128);
	
	// check if we need to copy the buffer from the device (in case READ was specified)
	if(!write_only) {
		if(blocking_map) {
			// must finish up all current work before we can properly read from the current buffer
			cqueue->finish();
			
			CU_CALL_NO_ACTION(cuMemcpyDtoH(host_buffer, buffer + offset, map_size),
							  "failed to copy device memory to host");
		}
		else {
			CU_CALL_NO_ACTION(cuMemcpyDtoHAsync(host_buffer, buffer + offset, map_size, (CUstream)cqueue->get_queue_ptr()),
							  "failed to copy device memory to host");
		}
	}
	
	// need to remember how much we mapped and where (so the host->device copy copies the right amount of bytes)
	mappings.emplace(host_buffer, cuda_mapping { map_size, offset, flags_ });
	
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
	
	// check if we need to actually copy data back to the device (not the case if read-only mapping)
	if((iter->second.flags & COMPUTE_MEMORY_MAP_FLAG::WRITE) != COMPUTE_MEMORY_MAP_FLAG::NONE ||
	   (iter->second.flags & COMPUTE_MEMORY_MAP_FLAG::WRITE_INVALIDATE) != COMPUTE_MEMORY_MAP_FLAG::NONE) {
		CU_CALL_NO_ACTION(cuMemcpyHtoD(buffer + iter->second.offset, mapped_ptr, iter->second.size),
						  "failed to copy host memory to device");
	}
	
	// free host memory again and remove the mapping
	delete [] (unsigned char*)mapped_ptr;
	mappings.erase(mapped_ptr);
}

bool cuda_buffer::acquire_opengl_object(shared_ptr<compute_queue> cqueue) {
	if(gl_object == 0) return false;
	if(buffer == 0) return false;
	if(rsrc == nullptr) return false;
	if(gl_object_state) {
		log_warn("opengl buffer has already been acquired for opengl use!");
		return true;
	}
	
	buffer = 0; // reset buffer pointer, this is no longer valid
	CU_CALL_RET(cuGraphicsUnmapResources(1, &rsrc,
										 (cqueue != nullptr ? (CUstream)cqueue->get_queue_ptr() : nullptr)),
				"failed to acquire opengl buffer - cuda resource unmapping failed!", false);
	gl_object_state = true;
	
	return true;
}

bool cuda_buffer::release_opengl_object(shared_ptr<compute_queue> cqueue) {
	if(gl_object == 0) return false;
	if(rsrc == nullptr) return false;
	if(!gl_object_state) {
		log_warn("opengl buffer has already been released to cuda!");
		return true;
	}
	
	CU_CALL_RET(cuGraphicsMapResources(1, &rsrc,
									   (cqueue != nullptr ? (CUstream)cqueue->get_queue_ptr() : nullptr)),
				"failed to release opengl buffer - cuda resource mapping failed!", false);
	gl_object_state = false;
	
	size_t ret_size { 0u };
	CU_CALL_RET(cuGraphicsResourceGetMappedPointer(&buffer, &ret_size, rsrc),
				"failed to retrieve mapped cuda buffer pointer from opengl buffer!", false);
	
	if(ret_size != size) {
		log_warn("size mismatch between shared opengl buffer and mapped cuda buffer: expected %u, got %u!",
				 size, ret_size);
	}
	if(buffer == 0) {
		log_error("mapped cuda buffer pointer (from a graphics resource) is invalid!");
		return false;
	}
	
	return true;
}

#endif
