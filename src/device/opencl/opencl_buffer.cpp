/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2025 Florian Ziesche
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

#include <floor/device/opencl/opencl_buffer.hpp>

#if !defined(FLOOR_NO_OPENCL)

#include <floor/core/logger.hpp>
#include <floor/device/opencl/opencl_queue.hpp>
#include <floor/device/opencl/opencl_device.hpp>
#include <floor/device/opencl/opencl_context.hpp>

namespace fl {

// TODO: proper error (return) value handling everywhere

opencl_buffer::opencl_buffer(const device_queue& cqueue,
							 const size_t& size_,
							 std::span<uint8_t> host_data_,
							 const MEMORY_FLAG flags_) :
device_buffer(cqueue, size_, host_data_, flags_) {
	if(size < min_multiple()) return;
	
	switch(flags & MEMORY_FLAG::READ_WRITE) {
		case MEMORY_FLAG::READ:
			cl_flags |= CL_MEM_READ_ONLY;
			break;
		case MEMORY_FLAG::WRITE:
			cl_flags |= CL_MEM_WRITE_ONLY;
			break;
		case MEMORY_FLAG::READ_WRITE:
			cl_flags |= CL_MEM_READ_WRITE;
			break;
		// all possible cases handled
		default: floor_unreachable();
	}
	
	switch(flags & MEMORY_FLAG::HOST_READ_WRITE) {
		case MEMORY_FLAG::HOST_READ:
			cl_flags |= CL_MEM_HOST_READ_ONLY;
			break;
		case MEMORY_FLAG::HOST_WRITE:
			cl_flags |= CL_MEM_HOST_WRITE_ONLY;
			break;
		case MEMORY_FLAG::HOST_READ_WRITE:
			// both - this is the default
			break;
		case MEMORY_FLAG::NONE:
			cl_flags |= CL_MEM_HOST_NO_ACCESS;
			break;
		// all possible cases handled
		default: floor_unreachable();
	}
	
	// TODO: handle the remaining flags + host ptr
	if (host_data.data() != nullptr && !has_flag<MEMORY_FLAG::NO_INITIAL_COPY>(flags)) {
		cl_flags |= CL_MEM_COPY_HOST_PTR;
	}
	
	// actually create the buffer
	if(!create_internal(true, cqueue)) {
		return; // can't do much else
	}
}

bool opencl_buffer::create_internal([[maybe_unused]] const bool copy_host_data, const device_queue& cqueue) {
	// TODO: handle the remaining flags + host ptr
	const auto& cl_dev = (const opencl_device&)cqueue.get_device();
	cl_int create_err = CL_SUCCESS;
	
	// -> normal OpenCL buffer
	buffer = clCreateBuffer(cl_dev.ctx, cl_flags, size, host_data.data(), &create_err);
	if(create_err != CL_SUCCESS) {
		log_error("failed to create buffer: $: $", create_err, cl_error_to_string(create_err));
		buffer = nullptr;
		return false;
	}
	
	return true;
}

opencl_buffer::~opencl_buffer() {
	if (buffer != nullptr) {
		clReleaseMemObject(buffer);
	}
}

void opencl_buffer::read(const device_queue& cqueue, const size_t size_, const size_t offset) const {
	read(cqueue, host_data.data(), size_, offset);
}

void opencl_buffer::read(const device_queue& cqueue, void* dst, const size_t size_, const size_t offset) const {
	if(buffer == nullptr) return;
	
	const size_t read_size = (size_ == 0 ? size : size_);
	if(!read_check(size, read_size, offset, flags)) return;
	
	// TODO: blocking flag
	clEnqueueReadBuffer((cl_command_queue)const_cast<void*>(cqueue.get_queue_ptr()), buffer, true, offset, read_size, dst,
						0, nullptr, nullptr);
}

void opencl_buffer::write(const device_queue& cqueue, const size_t size_, const size_t offset) {
	write(cqueue, host_data.data(), size_, offset);
}

void opencl_buffer::write(const device_queue& cqueue, const void* src, const size_t size_, const size_t offset) {
	if(buffer == nullptr) return;
	
	const size_t write_size = (size_ == 0 ? size : size_);
	if(!write_check(size, write_size, offset, flags)) return;
	
	// TODO: blocking flag
	clEnqueueWriteBuffer((cl_command_queue)const_cast<void*>(cqueue.get_queue_ptr()), buffer, true, offset, write_size, src,
						 0, nullptr, nullptr);
}

void opencl_buffer::copy(const device_queue& cqueue, const device_buffer& src,
						 const size_t size_, const size_t src_offset, const size_t dst_offset) {
	if(buffer == nullptr) return;
	
	// use min(src size, dst size) as the default size if no size is specified
	const size_t src_size = src.get_size();
	const size_t copy_size = (size_ == 0 ? std::min(src_size, size) : size_);
	if(!copy_check(size, src_size, copy_size, dst_offset, src_offset)) return;
	
	// TODO: blocking flag?
	clEnqueueCopyBuffer((cl_command_queue)const_cast<void*>(cqueue.get_queue_ptr()),
						((const opencl_buffer&)src).get_cl_buffer(), buffer, src_offset, dst_offset, copy_size,
						0, nullptr, nullptr);
}

bool opencl_buffer::fill(const device_queue& cqueue,
						 const void* pattern, const size_t& pattern_size,
						 const size_t size_, const size_t offset) {
	if(buffer == nullptr) return false;
	
	const size_t fill_size = (size_ == 0 ? size : size_);
	if(!fill_check(size, fill_size, pattern_size, offset)) return false;
	
	// NOTE: OpenCL spec says that this ignores kernel/host read/write flags
	return (clEnqueueFillBuffer((cl_command_queue)const_cast<void*>(cqueue.get_queue_ptr()), buffer, pattern, pattern_size, offset, fill_size,
								0, nullptr, nullptr) == CL_SUCCESS);
}

bool opencl_buffer::zero(const device_queue& cqueue) {
	if(buffer == nullptr) return false;
	
	// TODO: figure out the fastest way to do this here (write 8-bit, 16-bit, 32-bit, ...?)
	static constexpr const uint32_t zero_pattern { 0u };
	return (clEnqueueFillBuffer((cl_command_queue)const_cast<void*>(cqueue.get_queue_ptr()), buffer, &zero_pattern, sizeof(zero_pattern),
								0, size, 0, nullptr, nullptr) == CL_SUCCESS);
}

void* __attribute__((aligned(128))) opencl_buffer::map(const device_queue& cqueue,
													   const MEMORY_MAP_FLAG flags_,
													   const size_t size_, const size_t offset) {
	if(buffer == nullptr) return nullptr;
	
	const size_t map_size = (size_ == 0 ? size : size_);
	const bool blocking_map = has_flag<MEMORY_MAP_FLAG::BLOCK>(flags_);
	if(!map_check(size, map_size, flags, flags_, offset)) return nullptr;
	
	cl_map_flags map_flags = 0;
	if(has_flag<MEMORY_MAP_FLAG::WRITE_INVALIDATE>(flags_)) {
		map_flags |= CL_MAP_WRITE_INVALIDATE_REGION;
	}
	else {
		switch(flags_ & MEMORY_MAP_FLAG::READ_WRITE) {
			case MEMORY_MAP_FLAG::READ:
				map_flags |= CL_MAP_READ;
				break;
			case MEMORY_MAP_FLAG::WRITE:
				map_flags |= CL_MAP_WRITE;
				break;
			case MEMORY_MAP_FLAG::READ_WRITE:
				map_flags |= CL_MAP_READ | CL_MAP_WRITE;
				break;
			case MEMORY_MAP_FLAG::NONE:
			default:
				log_error("neither read nor write flag set for buffer mapping!");
				return nullptr;
		}
	}
	
	//
	cl_int map_err = CL_SUCCESS;
	auto ret_ptr = clEnqueueMapBuffer((cl_command_queue)const_cast<void*>(cqueue.get_queue_ptr()),
									  buffer, blocking_map, map_flags, offset, map_size,
									  0, nullptr, nullptr, &map_err);
	if(map_err != CL_SUCCESS) {
		log_error("failed to map buffer: $: $!", map_err, cl_error_to_string(map_err));
		return nullptr;
	}
	return ret_ptr;
}

bool opencl_buffer::unmap(const device_queue& cqueue, void* __attribute__((aligned(128))) mapped_ptr) {
	if(buffer == nullptr) return false;
	if(mapped_ptr == nullptr) return false;
	
	CL_CALL_RET(clEnqueueUnmapMemObject((cl_command_queue)const_cast<void*>(cqueue.get_queue_ptr()), buffer, mapped_ptr, 0, nullptr, nullptr),
				"failed to unmap buffer", false)
	return true;
}

cl_command_queue opencl_buffer::queue_or_default_queue(const device_queue* cqueue) const {
	if(cqueue != nullptr) return (cl_command_queue)const_cast<void*>(cqueue->get_queue_ptr());
	return (cl_command_queue)const_cast<void*>(((const opencl_context&)*dev.context).get_device_default_queue((const opencl_device&)dev)->get_queue_ptr());
}

} // namespace fl

#endif
