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

#ifndef __FLOOR_METAL_BUFFER_HPP__
#define __FLOOR_METAL_BUFFER_HPP__

#include <floor/compute/metal/metal_common.hpp>

#if !defined(FLOOR_NO_METAL)

#include <floor/threading/atomic_spin_lock.hpp>
#include <floor/compute/compute_buffer.hpp>
#include <Metal/Metal.h>

class metal_device;
class metal_buffer final : public compute_buffer {
public:
	metal_buffer(const metal_device* device,
				 const size_t& size_,
				 void* host_ptr,
				 const COMPUTE_BUFFER_FLAG flags_ = (COMPUTE_BUFFER_FLAG::READ_WRITE |
													 COMPUTE_BUFFER_FLAG::HOST_READ_WRITE));
	
	metal_buffer(const metal_device* device,
				 const size_t& size_,
				 const COMPUTE_BUFFER_FLAG flags_ = (COMPUTE_BUFFER_FLAG::READ_WRITE |
													 COMPUTE_BUFFER_FLAG::HOST_READ_WRITE)) :
	metal_buffer(device, size_, nullptr, flags_) {}
	
	template <typename data_type>
	metal_buffer(const metal_device* device,
				 const vector<data_type>& data,
				 const COMPUTE_BUFFER_FLAG flags_ = (COMPUTE_BUFFER_FLAG::READ_WRITE |
													 COMPUTE_BUFFER_FLAG::HOST_READ_WRITE)) :
	metal_buffer(device, sizeof(data_type) * data.size(), (void*)&data[0], flags_) {}
	
	template <typename data_type, size_t n>
	metal_buffer(const metal_device* device,
				 const array<data_type, n>& data,
				 const COMPUTE_BUFFER_FLAG flags_ = (COMPUTE_BUFFER_FLAG::READ_WRITE |
													 COMPUTE_BUFFER_FLAG::HOST_READ_WRITE)) :
	metal_buffer(device, sizeof(data_type) * n, (void*)&data[0], flags_) {}
	
	~metal_buffer() override;
	
	void read(shared_ptr<compute_queue> cqueue, const size_t size = 0, const size_t offset = 0) override;
	void read(shared_ptr<compute_queue> cqueue, void* dst, const size_t size = 0, const size_t offset = 0) override;
	
	void write(shared_ptr<compute_queue> cqueue, const size_t size = 0, const size_t offset = 0) override;
	void write(shared_ptr<compute_queue> cqueue, const void* src, const size_t size = 0, const size_t offset = 0) override;
	
	void copy(shared_ptr<compute_queue> cqueue,
			  shared_ptr<compute_buffer> src,
			  const size_t size = 0, const size_t src_offset = 0, const size_t dst_offset = 0) override;
	
	void fill(shared_ptr<compute_queue> cqueue,
			  const void* pattern, const size_t& pattern_size,
			  const size_t size = 0, const size_t offset = 0) override;
	
	void zero(shared_ptr<compute_queue> cqueue) override;
	
	bool resize(shared_ptr<compute_queue> cqueue,
				const size_t& size,
				const bool copy_old_data = false,
				const bool copy_host_data = false,
				void* new_host_ptr = nullptr) override;
	
	void* __attribute__((aligned(128))) map(shared_ptr<compute_queue> cqueue,
											const COMPUTE_BUFFER_MAP_FLAG flags =
											(COMPUTE_BUFFER_MAP_FLAG::READ_WRITE |
											 COMPUTE_BUFFER_MAP_FLAG::BLOCK),
											const size_t size = 0,
											const size_t offset = 0) override ACQUIRE(lock) REQUIRES(!lock);
	
	void unmap(shared_ptr<compute_queue> cqueue, void* __attribute__((aligned(128))) mapped_ptr) override RELEASE(lock);
	
	bool acquire_opengl_buffer(shared_ptr<compute_queue> cqueue) override;
	bool release_opengl_buffer(shared_ptr<compute_queue> cqueue) override;
	
	//! returns the metal specific buffer object
	id <MTLBuffer> get_metal_buffer() const { return buffer; }
	
protected:
	id <MTLBuffer> buffer { nullptr };
	
	MTLResourceOptions options { MTLCPUCacheModeDefaultCache };
	
	// separate create buffer function, b/c it's called by the constructor and resize
	bool create_internal(const bool copy_host_data);
	
};

#endif

#endif
