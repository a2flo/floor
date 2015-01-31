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
	// TODO: !
}

cuda_buffer::~cuda_buffer() {
	// TODO: kill the buffer
	//if(buffer == nullptr) return;
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

void* __attribute__((aligned(128))) cuda_buffer::map(shared_ptr<compute_queue> cqueue,
													 const COMPUTE_BUFFER_MAP_FLAG flags_,
													 const size_t size_, const size_t offset) {
	if(buffer == 0) return nullptr;
	// TODO: !
	return nullptr;
}

void cuda_buffer::unmap(shared_ptr<compute_queue> cqueue, void* __attribute__((aligned(128))) mapped_ptr) {
	if(buffer == 0) return;
	if(mapped_ptr == nullptr) return;
	// TODO: !
}

#endif
