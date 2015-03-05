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

#include <floor/compute/opencl/opencl_buffer.hpp>

#if !defined(FLOOR_NO_OPENCL)

#include <floor/core/logger.hpp>
#include <floor/compute/opencl/opencl_queue.hpp>
#include <floor/compute/opencl/opencl_device.hpp>

// TODO: remove the || 1 again
#if defined(FLOOR_DEBUG) || 1
#define FLOOR_DEBUG_COMPUTE_BUFFER 1
#endif

// TODO: proper error (return) value handling everywhere

opencl_buffer::opencl_buffer(const opencl_device* device,
							 const size_t& size_,
							 void* host_ptr_,
							 const COMPUTE_BUFFER_FLAG flags_) :
compute_buffer(device, size_, host_ptr_, flags_) {
	if(size < min_multiple()) return;
	
	switch(flags & COMPUTE_BUFFER_FLAG::READ_WRITE) {
		case COMPUTE_BUFFER_FLAG::READ:
			cl_flags |= CL_MEM_READ_ONLY;
			break;
		case COMPUTE_BUFFER_FLAG::WRITE:
			cl_flags |= CL_MEM_WRITE_ONLY;
			break;
		case COMPUTE_BUFFER_FLAG::READ_WRITE:
			cl_flags |= CL_MEM_READ_WRITE;
			break;
		default:
			log_error("buffer must be read-only, write-only or read-write!");
			return;
	}
	
	switch(flags & COMPUTE_BUFFER_FLAG::HOST_READ_WRITE) {
		case COMPUTE_BUFFER_FLAG::HOST_READ:
			cl_flags |= CL_MEM_HOST_READ_ONLY;
			break;
		case COMPUTE_BUFFER_FLAG::HOST_WRITE:
			cl_flags |= CL_MEM_HOST_WRITE_ONLY;
			break;
		case COMPUTE_BUFFER_FLAG::HOST_READ_WRITE:
			// both - this is the default
			break;
		case COMPUTE_BUFFER_FLAG::NONE:
			cl_flags |= CL_MEM_HOST_NO_ACCESS;
			break;
		// all possible cases handled
		default: floor_unreachable();
	}
	
	// TODO: handle the remaining flags + host ptr
	
	cl_int create_err = CL_SUCCESS;
	buffer = clCreateBuffer(device->ctx, cl_flags, size, host_ptr, &create_err);
	if(create_err != CL_SUCCESS) {
		log_error("failed to create buffer: %u", create_err);
		buffer = nullptr;
	}
}

opencl_buffer::~opencl_buffer() {
	// kill the buffer
	if(buffer == nullptr) return;
	clReleaseMemObject(buffer);
}

void opencl_buffer::read(shared_ptr<compute_queue> cqueue, const size_t size_, const size_t offset) {
	read(cqueue, host_ptr, size_, offset);
}

void opencl_buffer::read(shared_ptr<compute_queue> cqueue, void* dst, const size_t size_, const size_t offset) {
	if(buffer == nullptr) return;
	
	const size_t read_size = (size_ == 0 ? size : size_);
	
#if defined(FLOOR_DEBUG_COMPUTE_BUFFER)
	if(read_size == 0) {
		log_warn("trying to read 0 bytes!");
	}
	if(offset >= size) {
		log_error("invalid offset (>= size): offset: %X, size: %X", offset, size);
		return;
	}
	if(offset + read_size > size) {
		log_error("invalid offset/read size (offset + read size > buffer size): offset: %X, read size: %X, size: %X",
				  offset, read_size, size);
		return;
	}
#endif
	
	// TODO: blocking flag
	clEnqueueReadBuffer((cl_command_queue)cqueue->get_queue_ptr(), buffer, false, offset, read_size, dst,
						0, nullptr, nullptr);
}

void opencl_buffer::write(shared_ptr<compute_queue> cqueue, const size_t size_, const size_t offset) {
	write(cqueue, host_ptr, size_, offset);
}

void opencl_buffer::write(shared_ptr<compute_queue> cqueue, const void* src, const size_t size_, const size_t offset) {
	if(buffer == nullptr) return;
	
	const size_t write_size = (size_ == 0 ? size : size_);
	
#if defined(FLOOR_DEBUG_COMPUTE_BUFFER)
	if(write_size == 0) {
		log_warn("trying to write 0 bytes!");
	}
	if(offset >= size) {
		log_error("invalid offset (>= size): offset: %X, size: %X", offset, size);
		return;
	}
	if(offset + write_size > size) {
		log_error("invalid offset/write size (offset + write size > buffer size): offset: %X, write size: %X, size: %X",
				  offset, write_size, size);
		return;
	}
#endif
	
	// TODO: blocking flag
	clEnqueueWriteBuffer((cl_command_queue)cqueue->get_queue_ptr(), buffer, false, offset, write_size, src,
						 0, nullptr, nullptr);
}

void opencl_buffer::copy(shared_ptr<compute_queue> cqueue,
						 shared_ptr<compute_buffer> src,
						 const size_t size_, const size_t src_offset, const size_t dst_offset) {
	if(buffer == nullptr) return;
	
	// use min(src size, dst size) as the default size if no size is specified
	const size_t src_size = src->get_size();
	const size_t copy_size = (size_ == 0 ? std::min(src_size, size) : size_);
	
#if defined(FLOOR_DEBUG_COMPUTE_BUFFER)
	if(copy_size == 0) {
		log_warn("trying to copy 0 bytes!");
	}
	if(src_offset >= src_size) {
		log_error("invalid src offset (>= size): offset: %X, size: %X", src_offset, src_size);
		return;
	}
	if(dst_offset >= size) {
		log_error("invalid dst offset (>= size): offset: %X, size: %X", dst_offset, size);
		return;
	}
	if(src_offset + copy_size > src_size) {
		log_error("invalid src offset/copy size (offset + copy size > buffer size): offset: %X, copy size: %X, size: %X",
				  src_offset, copy_size, src_size);
		return;
	}
	if(dst_offset + copy_size > size) {
		log_error("invalid dst offset/copy size (offset + copy size > buffer size): offset: %X, copy size: %X, size: %X",
				  dst_offset, copy_size, size);
		return;
	}
#endif
	
	// TODO: blocking flag?
	clEnqueueCopyBuffer((cl_command_queue)cqueue->get_queue_ptr(),
						((shared_ptr<opencl_buffer>&)src)->get_cl_buffer(), buffer, src_offset, dst_offset, copy_size,
						0, nullptr, nullptr);
}

void opencl_buffer::fill(shared_ptr<compute_queue> cqueue,
						 const void* pattern, const size_t& pattern_size,
						 const size_t size_, const size_t offset) {
	if(buffer == nullptr) return;
	
	const size_t fill_size = (size_ == 0 ? size : size_);
	
	// TODO: debug checks!
	
	// NOTE: opencl spec says that this ignores kernel/host read/write flags
	clEnqueueFillBuffer((cl_command_queue)cqueue->get_queue_ptr(), buffer, pattern, pattern_size, offset, fill_size,
						0, nullptr, nullptr);
}

void opencl_buffer::zero(shared_ptr<compute_queue> cqueue) {
	if(buffer == nullptr) return;
	
	// TODO: figure out the fastest way to do this here (write 8-bit, 16-bit, 32-bit, ...?)
	static constexpr const uint32_t zero_pattern { 0u };
	clEnqueueFillBuffer((cl_command_queue)cqueue->get_queue_ptr(), buffer, &zero_pattern, sizeof(zero_pattern),
						0, size, 0, nullptr, nullptr);
}

bool opencl_buffer::resize(shared_ptr<compute_queue> cqueue, const size_t& new_size_,
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
	
	const bool is_host_buffer = ((flags & COMPUTE_BUFFER_FLAG::USE_HOST_MEMORY) != COMPUTE_BUFFER_FLAG::NONE);
	
	// create the new buffer
	cl_mem new_buffer = nullptr;
	CL_CALL_ERR_PARAM_RET(new_buffer = clCreateBuffer(((opencl_device*)dev)->ctx, cl_flags, new_size, new_host_ptr, &create_err),
						  create_err, "failed to create resized buffer", false);
	
	// copy old data if specified
	if(copy_old_data) {
		// can only copy as many bytes as there are bytes
		const size_t copy_size = std::min(size, new_size); // >= 4, established above
		
		clEnqueueCopyBuffer((cl_command_queue)cqueue->get_queue_ptr(), buffer, new_buffer, 0, 0, copy_size,
							0, nullptr, nullptr);
	}
	else if(!copy_old_data && copy_host_data && is_host_buffer && new_host_ptr != nullptr) {
		// TODO: blocking flag
		clEnqueueWriteBuffer((cl_command_queue)cqueue->get_queue_ptr(), buffer, false, 0, new_size, new_host_ptr,
							 0, nullptr, nullptr);
	}
	
	// kill the old buffer and assign the new one
	if(buffer != nullptr) {
		clReleaseMemObject(buffer);
	}
	buffer = new_buffer;
	host_ptr = new_host_ptr;
	
	return true;
}

void* __attribute__((aligned(128))) opencl_buffer::map(shared_ptr<compute_queue> cqueue,
													   const COMPUTE_BUFFER_MAP_FLAG flags_,
													   const size_t size_, const size_t offset) {
	if(buffer == nullptr) return nullptr;
	
	const size_t map_size = (size_ == 0 ? size : size_);
	const bool blocking_map = ((flags_ & COMPUTE_BUFFER_MAP_FLAG::BLOCK) != COMPUTE_BUFFER_MAP_FLAG::NONE);
	
	cl_map_flags map_flags = 0;
	if((flags_ & COMPUTE_BUFFER_MAP_FLAG::WRITE_INVALIDATE) != COMPUTE_BUFFER_MAP_FLAG::NONE) {
		map_flags |= CL_MAP_WRITE_INVALIDATE_REGION;
		
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
				map_flags |= CL_MAP_READ;
				break;
			case COMPUTE_BUFFER_MAP_FLAG::WRITE:
				map_flags |= CL_MAP_WRITE;
				break;
			case COMPUTE_BUFFER_MAP_FLAG::READ_WRITE:
				map_flags |= CL_MAP_READ | CL_MAP_WRITE;
				break;
			case COMPUTE_BUFFER_MAP_FLAG::NONE:
			default:
				log_error("neither read nor write flag set for buffer mapping!");
				return nullptr;
		}
	}
	
	// TODO: debug checks! check size/offset + flag combinations (host flags)
	
	//
	cl_int map_err = CL_SUCCESS;
	auto ret_ptr = clEnqueueMapBuffer((cl_command_queue)cqueue->get_queue_ptr(),
									  buffer, blocking_map, map_flags, offset, map_size,
									  0, nullptr, nullptr, &map_err);
	if(map_err != CL_SUCCESS) {
		log_error("failed to map buffer: %u!", map_err);
		return nullptr;
	}
	return ret_ptr;
}

void opencl_buffer::unmap(shared_ptr<compute_queue> cqueue, void* __attribute__((aligned(128))) mapped_ptr) {
	if(buffer == nullptr) return;
	if(mapped_ptr == nullptr) return;
	
	CL_CALL_RET(clEnqueueUnmapMemObject((cl_command_queue)cqueue->get_queue_ptr(), buffer, mapped_ptr, 0, nullptr, nullptr),
				"failed to unmap buffer")
}

#endif
