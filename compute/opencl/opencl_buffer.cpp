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

#include <floor/compute/opencl/opencl_buffer.hpp>

#if !defined(FLOOR_NO_OPENCL)

opencl_buffer::opencl_buffer(const cl_context ctx_ptr_,
							 const size_t& size_,
							 const void* data,
							 const COMPUTE_BUFFER_FLAG flags_) :
compute_buffer(ctx_ptr_, size_, data, flags_) {
	// TODO: create the buffer
}

opencl_buffer::~opencl_buffer() {
	// TODO: kill the buffer
}

void opencl_buffer::read(const size_t size, const size_t offset) {
	// TODO: !
}

void opencl_buffer::read(void* dst, const size_t size, const size_t offset)  {
	// TODO: !
}

void opencl_buffer::write(const size_t size, const size_t offset)  {
	// TODO: !
}

void opencl_buffer::write(const void* src, const size_t size, const size_t offset)  {
	// TODO: !
}

void opencl_buffer::copy(shared_ptr<compute_buffer> src,
						 const size_t src_size, const size_t src_offset,
						 const size_t dst_size, const size_t dst_offset)  {
	// TODO: !
}

void opencl_buffer::fill(const void* pattern, const size_t& pattern_size,
						 const size_t size, const size_t offset)  {
	// TODO: !
}

void opencl_buffer::zero()  {
	// TODO: !
}

bool opencl_buffer::resize(const size_t& size, const bool copy_old_data)  {
	// TODO: !
	return false;
}

void* __attribute__((aligned(128))) opencl_buffer::map(const COMPUTE_BUFFER_MAP_FLAG flags,
													   const size_t size, const size_t offset)  {
	// TODO: !
	return nullptr;
}

void opencl_buffer::unmap(void* __attribute__((aligned(128))) mapped_ptr)  {
	// TODO: !
}

#endif
