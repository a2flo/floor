/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2024 Florian Ziesche
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

#ifndef __FLOOR_HOST_BUFFER_HPP__
#define __FLOOR_HOST_BUFFER_HPP__

#include <floor/compute/host/host_common.hpp>

#if !defined(FLOOR_NO_HOST_COMPUTE)

#include <floor/compute/compute_buffer.hpp>
#include <floor/core/aligned_ptr.hpp>

class host_device;
class host_buffer final : public compute_buffer {
public:
	host_buffer(const compute_queue& cqueue,
				const size_t& size_,
				std::span<uint8_t> host_data_,
				const COMPUTE_MEMORY_FLAG flags_ = (COMPUTE_MEMORY_FLAG::READ_WRITE |
													COMPUTE_MEMORY_FLAG::HOST_READ_WRITE),
				const uint32_t opengl_type_ = 0,
				const uint32_t external_gl_object_ = 0,
				compute_buffer* shared_buffer_ = nullptr);
	
	host_buffer(const compute_queue& cqueue,
				const size_t& size_,
				const COMPUTE_MEMORY_FLAG flags_ = (COMPUTE_MEMORY_FLAG::READ_WRITE |
													COMPUTE_MEMORY_FLAG::HOST_READ_WRITE),
				const uint32_t opengl_type_ = 0,
				compute_buffer* shared_buffer_ = nullptr) :
	host_buffer(cqueue, size_, {}, flags_, opengl_type_, 0, shared_buffer_) {}
	
	~host_buffer() override;

	void read(const compute_queue& cqueue, const size_t size = 0, const size_t offset = 0) override;
	void read(const compute_queue& cqueue, void* dst, const size_t size = 0, const size_t offset = 0) override;

	void write(const compute_queue& cqueue, const size_t size = 0, const size_t offset = 0) override;
	void write(const compute_queue& cqueue, const void* src, const size_t size = 0, const size_t offset = 0) override;

	void copy(const compute_queue& cqueue, const compute_buffer& src,
			  const size_t size = 0, const size_t src_offset = 0, const size_t dst_offset = 0) override;

	bool fill(const compute_queue& cqueue,
			  const void* pattern, const size_t& pattern_size,
			  const size_t size = 0, const size_t offset = 0) override;

	bool zero(const compute_queue& cqueue) override;

	void* __attribute__((aligned(128))) map(const compute_queue& cqueue,
											const COMPUTE_MEMORY_MAP_FLAG flags = (COMPUTE_MEMORY_MAP_FLAG::READ_WRITE | COMPUTE_MEMORY_MAP_FLAG::BLOCK),
											const size_t size = 0, const size_t offset = 0) override;

	bool unmap(const compute_queue& cqueue, void* __attribute__((aligned(128))) mapped_ptr) override;

	bool acquire_opengl_object(const compute_queue* cqueue) override;
	bool release_opengl_object(const compute_queue* cqueue) override;
	
	bool acquire_metal_buffer(const compute_queue* cqueue, const metal_queue* mtl_queue) const override;
	bool release_metal_buffer(const compute_queue* cqueue, const metal_queue* mtl_queue) const override;
	bool sync_metal_buffer(const compute_queue* cqueue, const metal_queue* mtl_queue) const override;
	
	bool acquire_vulkan_buffer(const compute_queue* cqueue, const vulkan_queue* vk_queue) const override;
	bool release_vulkan_buffer(const compute_queue* cqueue, const vulkan_queue* vk_queue) const override;
	bool sync_vulkan_buffer(const compute_queue* cqueue, const vulkan_queue* vk_queue) const override;
	
	//! returns a direct pointer to the internal host buffer
	auto get_host_buffer_ptr() const {
		return buffer.get();
	}
	
	//! returns a direct pointer to the internal host buffer and synchronizes buffer contents if synchronization flags are set
	decltype(aligned_ptr<uint8_t>{}.get()) get_host_buffer_ptr_with_sync() const;

protected:
	mutable aligned_ptr<uint8_t> buffer;
	
	//! separate create buffer function, b/c it's called by the constructor and resize
	bool create_internal(const bool copy_host_data, const compute_queue& cqueue);
	
	// internal Metal/Vulkan buffer when using Metal/Vulkan memory sharing (and not wrapping an existing buffer)
	shared_ptr<compute_buffer> host_shared_buffer;
	// creates the internal Metal/Vulkan buffer, or deals with the wrapped external one
	bool create_shared_buffer(const bool copy_host_data);

};

#endif

#endif
