/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2014 Florian Ziesche
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

#ifndef __FLOOR_OPENCL_BUFFER_HPP__
#define __FLOOR_OPENCL_BUFFER_HPP__

#include <floor/compute/opencl/opencl_common.hpp>

#if !defined(FLOOR_NO_OPENCL)

#include <floor/compute/compute_buffer.hpp>

class opencl_buffer final : public compute_buffer {
public:
	opencl_buffer(const cl_context ctx_ptr_,
				  const size_t& size_,
				  const void* data,
				  const COMPUTE_BUFFER_FLAG flags_ = (COMPUTE_BUFFER_FLAG::READ_WRITE |
													  COMPUTE_BUFFER_FLAG::HOST_READ_WRITE));
	
	opencl_buffer(const cl_context ctx_ptr_,
				  const size_t& size_,
				  const COMPUTE_BUFFER_FLAG flags_ = (COMPUTE_BUFFER_FLAG::READ_WRITE |
													  COMPUTE_BUFFER_FLAG::HOST_READ_WRITE)) :
	opencl_buffer(ctx_ptr_, size_, nullptr, flags_) {}
	
	template <typename data_type>
	opencl_buffer(const cl_context ctx_ptr_,
				  const vector<data_type>& data,
				  const COMPUTE_BUFFER_FLAG flags_ = (COMPUTE_BUFFER_FLAG::READ_WRITE |
													  COMPUTE_BUFFER_FLAG::HOST_READ_WRITE)) :
	opencl_buffer(ctx_ptr_, sizeof(data_type) * data.size(), &data[0], flags_) {}
	
	template <typename data_type, size_t n>
	opencl_buffer(const cl_context ctx_ptr_,
				  const array<data_type, n>& data,
				  const COMPUTE_BUFFER_FLAG flags_ = (COMPUTE_BUFFER_FLAG::READ_WRITE |
													  COMPUTE_BUFFER_FLAG::HOST_READ_WRITE)) :
	opencl_buffer(ctx_ptr_, sizeof(data_type) * n, &data[0], flags_) {}
	
	~opencl_buffer() override;
	
	//! reads "size" bytes (or the complete buffer if 0) from "offset" onwards
	//! back to the previously specified host pointer
	void read(const size_t size = 0, const size_t offset = 0) override;
	//! reads "size" bytes (or the complete buffer if 0) from "offset" onwards
	//! back to the specified "dst" pointer
	void read(void* dst, const size_t size = 0, const size_t offset = 0) override;
	
	//! writes "size" bytes (or the complete buffer if 0) from "offset" onwards
	//! from the previously specified host pointer to this buffer
	void write(const size_t size = 0, const size_t offset = 0) override;
	//! writes "size" bytes (or the complete buffer if 0) from "offset" onwards
	//! from the specified "src" pointer to this buffer
	void write(const void* src, const size_t size = 0, const size_t offset = 0) override;
	
	//!
	void copy(shared_ptr<compute_buffer> src,
			  const size_t src_size = 0, const size_t src_offset = 0,
			  const size_t dst_size = 0, const size_t dst_offset = 0) override;
	
	//!
	void fill(const void* pattern, const size_t& pattern_size,
			  const size_t size = 0, const size_t offset = 0) override;
	
	//! zeros/clears the complete buffer
	void zero() override;
	
	//!
	bool resize(const size_t& size, const bool copy_old_data = false) override;
	
	//!
	void* __attribute__((aligned(128))) map(const COMPUTE_BUFFER_MAP_FLAG flags =
											(COMPUTE_BUFFER_MAP_FLAG::READ_WRITE |
											 COMPUTE_BUFFER_MAP_FLAG::BLOCK),
											const size_t size = 0, const size_t offset = 0) override;
	
	//!
	void unmap(void* __attribute__((aligned(128))) mapped_ptr) override;
	
protected:
	
};

#endif

#endif
