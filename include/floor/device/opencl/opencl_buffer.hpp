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

#pragma once

#include <floor/device/opencl/opencl_common.hpp>

#if !defined(FLOOR_NO_OPENCL)

#include <floor/device/device_buffer.hpp>

namespace fl {

class opencl_device;
class opencl_buffer final : public device_buffer {
public:
	opencl_buffer(const device_queue& cqueue,
				  const size_t& size_,
				  std::span<uint8_t> host_data_,
				  const MEMORY_FLAG flags_ = (MEMORY_FLAG::READ_WRITE |
													  MEMORY_FLAG::HOST_READ_WRITE));
	
	opencl_buffer(const device_queue& cqueue,
				  const size_t& size_,
				  const MEMORY_FLAG flags_ = (MEMORY_FLAG::READ_WRITE |
													  MEMORY_FLAG::HOST_READ_WRITE)) :
	opencl_buffer(cqueue, size_, {}, flags_) {}
	
	~opencl_buffer() override;
	
	void read(const device_queue& cqueue, const size_t size = 0, const size_t offset = 0) const override;
	void read(const device_queue& cqueue, void* dst, const size_t size = 0, const size_t offset = 0) const override;
	
	void write(const device_queue& cqueue, const size_t size = 0, const size_t offset = 0) override;
	void write(const device_queue& cqueue, const void* src, const size_t size = 0, const size_t offset = 0) override;
	
	void copy(const device_queue& cqueue, const device_buffer& src,
			  const size_t size = 0, const size_t src_offset = 0, const size_t dst_offset = 0) override;
	
	bool fill(const device_queue& cqueue,
			  const void* pattern, const size_t& pattern_size,
			  const size_t size = 0, const size_t offset = 0) override;
	
	bool zero(const device_queue& cqueue) override;
	
	void* __attribute__((aligned(128))) map(const device_queue& cqueue,
											const MEMORY_MAP_FLAG flags = (MEMORY_MAP_FLAG::READ_WRITE | MEMORY_MAP_FLAG::BLOCK),
											const size_t size = 0, const size_t offset = 0) override;
	
	bool unmap(const device_queue& cqueue, void* __attribute__((aligned(128))) mapped_ptr) override;
	
	//! returns the OpenCL specific buffer object/pointer
	const cl_mem& get_cl_buffer() const { return buffer; }
	
protected:
	cl_mem buffer { nullptr };
	cl_mem_flags cl_flags { 0 };
	
	//! separate create buffer function, b/c it's called by the constructor and resize
	bool create_internal(const bool copy_host_data, const device_queue& cqueue);
	
	//! if cqueue isn't nullptr, returns its cl_command_queue, otherwise returns the devices default queue
	cl_command_queue queue_or_default_queue(const device_queue* cqueue) const;
	
};

} // namespace fl

#endif
