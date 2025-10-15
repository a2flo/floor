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

#include <floor/device/host/host_common.hpp>

#if !defined(FLOOR_NO_HOST_COMPUTE)

#include <floor/device/device_buffer.hpp>
#include <floor/core/aligned_ptr.hpp>

namespace fl {

class host_device;
class host_buffer final : public device_buffer {
public:
	host_buffer(const device_queue& cqueue,
				const size_t& size_,
				std::span<uint8_t> host_data_,
				const MEMORY_FLAG flags_ = (MEMORY_FLAG::READ_WRITE |
													MEMORY_FLAG::HOST_READ_WRITE),
				device_buffer* shared_buffer_ = nullptr);
	
	host_buffer(const device_queue& cqueue,
				const size_t& size_,
				const MEMORY_FLAG flags_ = (MEMORY_FLAG::READ_WRITE |
													MEMORY_FLAG::HOST_READ_WRITE),
				device_buffer* shared_buffer_ = nullptr) :
	host_buffer(cqueue, size_, {}, flags_, shared_buffer_) {}
	
	~host_buffer() override;
	
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
	
	bool acquire_metal_buffer(const device_queue* cqueue, const metal_queue* mtl_queue) const override;
	bool release_metal_buffer(const device_queue* cqueue, const metal_queue* mtl_queue) const override;
	bool sync_metal_buffer(const device_queue* cqueue, const metal_queue* mtl_queue) const override;
	
	bool acquire_vulkan_buffer(const device_queue* cqueue, const vulkan_queue* vk_queue) const override;
	bool release_vulkan_buffer(const device_queue* cqueue, const vulkan_queue* vk_queue) const override;
	bool sync_vulkan_buffer(const device_queue* cqueue, const vulkan_queue* vk_queue) const override;
	
	//! returns a direct pointer to the internal host buffer
	auto get_host_buffer_ptr() const {
		return buffer.get();
	}
	
	//! returns a direct pointer to the internal host buffer and synchronizes buffer contents if synchronization flags are set
	decltype(aligned_ptr<uint8_t>{}.get()) get_host_buffer_ptr_with_sync() const;
	
protected:
	mutable aligned_ptr<uint8_t> buffer;
	
	//! separate create buffer function, b/c it's called by the constructor and resize
	bool create_internal(const bool copy_host_data, const device_queue& cqueue);
	
	// internal Metal/Vulkan buffer when using Metal/Vulkan memory sharing (and not wrapping an existing buffer)
	std::shared_ptr<device_buffer> host_shared_buffer;
	// creates the internal Metal/Vulkan buffer, or deals with the wrapped external one
	bool create_shared_buffer(const bool copy_host_data);
	
};

} // namespace fl

#endif
