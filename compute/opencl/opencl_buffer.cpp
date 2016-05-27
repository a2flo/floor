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

#include <floor/compute/opencl/opencl_buffer.hpp>

#if !defined(FLOOR_NO_OPENCL)

#include <floor/core/logger.hpp>
#include <floor/compute/opencl/opencl_queue.hpp>
#include <floor/compute/opencl/opencl_device.hpp>
#include <floor/compute/opencl/opencl_compute.hpp>

// TODO: proper error (return) value handling everywhere

opencl_buffer::opencl_buffer(const opencl_device* device,
							 const size_t& size_,
							 void* host_ptr_,
							 const COMPUTE_MEMORY_FLAG flags_,
							 const uint32_t opengl_type_,
							 const uint32_t external_gl_object_) :
compute_buffer(device, size_, host_ptr_, flags_, opengl_type_, external_gl_object_) {
	if(size < min_multiple()) return;
	
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
	if(host_ptr_ != nullptr && !has_flag<COMPUTE_MEMORY_FLAG::NO_INITIAL_COPY>(flags)) {
		cl_flags |= CL_MEM_COPY_HOST_PTR;
	}
	
	// actually create the buffer
	if(!create_internal(true, nullptr)) {
		return; // can't do much else
	}
}

bool opencl_buffer::create_internal(const bool copy_host_data, shared_ptr<compute_queue> cqueue) {
	// TODO: handle the remaining flags + host ptr
	cl_int create_err = CL_SUCCESS;
	
	// -> normal opencl buffer
	if(!has_flag<COMPUTE_MEMORY_FLAG::OPENGL_SHARING>(flags)) {
		buffer = clCreateBuffer(((opencl_device*)dev)->ctx, cl_flags, size, host_ptr, &create_err);
		if(create_err != CL_SUCCESS) {
			log_error("failed to create buffer: %u: %s", create_err, cl_error_to_string(create_err));
			buffer = nullptr;
			return false;
		}
	}
	// -> shared opencl/opengl buffer
	else {
		if(!create_gl_buffer(copy_host_data)) return false;
		
		// "Only CL_MEM_READ_ONLY, CL_MEM_WRITE_ONLY and CL_MEM_READ_WRITE values specified in table 5.3 can be used"
		cl_flags &= (CL_MEM_READ_ONLY | CL_MEM_WRITE_ONLY | CL_MEM_READ_WRITE); // be lenient on other flag use
		buffer = clCreateFromGLBuffer(((opencl_device*)dev)->ctx, cl_flags, gl_object, &create_err);
		if(create_err != CL_SUCCESS) {
			log_error("failed to create shared opengl/opencl buffer: %u: %s", create_err, cl_error_to_string(create_err));
			buffer = nullptr;
			return false;
		}
		// acquire for use with opencl
		acquire_opengl_object(cqueue);
	}
	
	return true;
}

opencl_buffer::~opencl_buffer() {
	// first, release and kill the opengl buffer
	if(gl_object != 0) {
		if(gl_object_state) {
			log_warn("buffer still registered for opengl use - acquire before destructing a compute buffer!");
		}
		if(!gl_object_state) release_opengl_object(nullptr); // -> release to opengl
		delete_gl_buffer();
	}
	// then, also kill the opencl buffer
	if(buffer != nullptr) {
		clReleaseMemObject(buffer);
	}
}

void opencl_buffer::read(shared_ptr<compute_queue> cqueue, const size_t size_, const size_t offset) {
	read(cqueue, host_ptr, size_, offset);
}

void opencl_buffer::read(shared_ptr<compute_queue> cqueue, void* dst, const size_t size_, const size_t offset) {
	if(buffer == nullptr) return;
	
	const size_t read_size = (size_ == 0 ? size : size_);
	if(!read_check(size, read_size, offset)) return;
	
	// TODO: blocking flag
	clEnqueueReadBuffer(queue_or_default_queue(cqueue), buffer, true, offset, read_size, dst,
						0, nullptr, nullptr);
}

void opencl_buffer::write(shared_ptr<compute_queue> cqueue, const size_t size_, const size_t offset) {
	write(cqueue, host_ptr, size_, offset);
}

void opencl_buffer::write(shared_ptr<compute_queue> cqueue, const void* src, const size_t size_, const size_t offset) {
	if(buffer == nullptr) return;
	
	const size_t write_size = (size_ == 0 ? size : size_);
	if(!write_check(size, write_size, offset)) return;
	
	// TODO: blocking flag
	clEnqueueWriteBuffer(queue_or_default_queue(cqueue), buffer, true, offset, write_size, src,
						 0, nullptr, nullptr);
}

void opencl_buffer::copy(shared_ptr<compute_queue> cqueue,
						 shared_ptr<compute_buffer> src,
						 const size_t size_, const size_t src_offset, const size_t dst_offset) {
	if(buffer == nullptr) return;
	
	// use min(src size, dst size) as the default size if no size is specified
	const size_t src_size = src->get_size();
	const size_t copy_size = (size_ == 0 ? std::min(src_size, size) : size_);
	if(!copy_check(size, src_size, copy_size, dst_offset, src_offset)) return;
	
	// TODO: blocking flag?
	clEnqueueCopyBuffer(queue_or_default_queue(cqueue),
						((shared_ptr<opencl_buffer>&)src)->get_cl_buffer(), buffer, src_offset, dst_offset, copy_size,
						0, nullptr, nullptr);
}

void opencl_buffer::fill(shared_ptr<compute_queue> cqueue,
						 const void* pattern, const size_t& pattern_size,
						 const size_t size_, const size_t offset) {
	if(buffer == nullptr) return;
	
	const size_t fill_size = (size_ == 0 ? size : size_);
	if(!fill_check(size, fill_size, pattern_size, offset)) return;
	
	// NOTE: opencl spec says that this ignores kernel/host read/write flags
	clEnqueueFillBuffer(queue_or_default_queue(cqueue), buffer, pattern, pattern_size, offset, fill_size,
						0, nullptr, nullptr);
}

void opencl_buffer::zero(shared_ptr<compute_queue> cqueue) {
	if(buffer == nullptr) return;
	
	// TODO: figure out the fastest way to do this here (write 8-bit, 16-bit, 32-bit, ...?)
	static constexpr const uint32_t zero_pattern { 0u };
	clEnqueueFillBuffer(queue_or_default_queue(cqueue), buffer, &zero_pattern, sizeof(zero_pattern),
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
		
		// restore old buffer
		restore_old_buffer();
		return false;
	}
	
	// copy old data if specified
	if(copy_old_data) {
		// can only copy as many bytes as there are bytes
		const size_t copy_size = std::min(size, new_size); // >= 4, established above
		
		clEnqueueCopyBuffer(queue_or_default_queue(cqueue), old_buffer, buffer, 0, 0, copy_size,
							0, nullptr, nullptr);
	}
	else if(!copy_old_data && copy_host_data && is_host_buffer && host_ptr != nullptr) {
		// TODO: blocking flag
		clEnqueueWriteBuffer(queue_or_default_queue(cqueue), buffer, true, 0, size, host_ptr,
							 0, nullptr, nullptr);
	}
	
	// kill the old buffer
	if(old_buffer != nullptr) {
		clReleaseMemObject(old_buffer);
	}
	
	return true;
}

void* __attribute__((aligned(128))) opencl_buffer::map(shared_ptr<compute_queue> cqueue,
													   const COMPUTE_MEMORY_MAP_FLAG flags_,
													   const size_t size_, const size_t offset) {
	if(buffer == nullptr) return nullptr;
	
	const size_t map_size = (size_ == 0 ? size : size_);
	const bool blocking_map = has_flag<COMPUTE_MEMORY_MAP_FLAG::BLOCK>(flags_);
	if(!map_check(size, map_size, flags, flags_, offset)) return nullptr;
	
	cl_map_flags map_flags = 0;
	if(has_flag<COMPUTE_MEMORY_MAP_FLAG::WRITE_INVALIDATE>(flags_)) {
		map_flags |= CL_MAP_WRITE_INVALIDATE_REGION;
	}
	else {
		switch(flags_ & COMPUTE_MEMORY_MAP_FLAG::READ_WRITE) {
			case COMPUTE_MEMORY_MAP_FLAG::READ:
				map_flags |= CL_MAP_READ;
				break;
			case COMPUTE_MEMORY_MAP_FLAG::WRITE:
				map_flags |= CL_MAP_WRITE;
				break;
			case COMPUTE_MEMORY_MAP_FLAG::READ_WRITE:
				map_flags |= CL_MAP_READ | CL_MAP_WRITE;
				break;
			case COMPUTE_MEMORY_MAP_FLAG::NONE:
			default:
				log_error("neither read nor write flag set for buffer mapping!");
				return nullptr;
		}
	}
	
	//
	cl_int map_err = CL_SUCCESS;
	auto ret_ptr = clEnqueueMapBuffer(queue_or_default_queue(cqueue),
									  buffer, blocking_map, map_flags, offset, map_size,
									  0, nullptr, nullptr, &map_err);
	if(map_err != CL_SUCCESS) {
		log_error("failed to map buffer: %u: %s!", map_err, cl_error_to_string(map_err));
		return nullptr;
	}
	return ret_ptr;
}

void opencl_buffer::unmap(shared_ptr<compute_queue> cqueue, void* __attribute__((aligned(128))) mapped_ptr) {
	if(buffer == nullptr) return;
	if(mapped_ptr == nullptr) return;
	
	CL_CALL_RET(clEnqueueUnmapMemObject(queue_or_default_queue(cqueue), buffer, mapped_ptr, 0, nullptr, nullptr),
				"failed to unmap buffer")
}

bool opencl_buffer::acquire_opengl_object(shared_ptr<compute_queue> cqueue) {
	if(gl_object == 0) return false;
	if(!gl_object_state) {
#if defined(FLOOR_DEBUG) && 0
		log_warn("opengl buffer has already been acquired for use with opencl!");
#endif
		return true;
	}
	
	CL_CALL_RET(clEnqueueAcquireGLObjects(queue_or_default_queue(cqueue), 1, &buffer, 0, nullptr, nullptr),
				"failed to acquire opengl buffer - opencl gl object acquire failed", false);
	gl_object_state = false;
	return true;
}

bool opencl_buffer::release_opengl_object(shared_ptr<compute_queue> cqueue) {
	if(gl_object == 0) return false;
	if(buffer == 0) return false;
	if(gl_object_state) {
#if defined(FLOOR_DEBUG) && 0
		log_warn("opengl buffer has already been released for opengl use!");
#endif
		return true;
	}
	
	CL_CALL_RET(clEnqueueReleaseGLObjects(queue_or_default_queue(cqueue), 1, &buffer, 0, nullptr, nullptr),
				"failed to release opengl buffer - opencl gl object release failed", false);
	gl_object_state = true;
	return true;
}

cl_command_queue opencl_buffer::queue_or_default_queue(shared_ptr<compute_queue> cqueue) const {
	if(cqueue != nullptr) return (cl_command_queue)cqueue->get_queue_ptr();
	return (cl_command_queue)((opencl_compute*)dev->context)->get_device_default_queue((const opencl_device*)dev)->get_queue_ptr();
}

#endif
