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

#include <floor/device/vulkan/vulkan_common.hpp>

#if !defined(FLOOR_NO_VULKAN)

#include <floor/device/device_buffer.hpp>
#include <floor/device/vulkan/vulkan_memory.hpp>

namespace fl {

class vulkan_device;
class vulkan_buffer final : public device_buffer, vulkan_memory {
public:
	vulkan_buffer(const device_queue& cqueue,
				  const size_t& size_,
				  std::span<uint8_t> host_data_,
				  const MEMORY_FLAG flags_ = (MEMORY_FLAG::READ_WRITE |
													  MEMORY_FLAG::HOST_READ_WRITE));
	
	vulkan_buffer(const device_queue& cqueue,
				  const size_t& size_,
				  const MEMORY_FLAG flags_ = (MEMORY_FLAG::READ_WRITE |
													  MEMORY_FLAG::HOST_READ_WRITE)) :
	vulkan_buffer(cqueue, size_, {}, flags_) {}
	
	~vulkan_buffer() override;
	
	void read(const device_queue& cqueue, const size_t size = 0, const size_t offset = 0) override;
	void read(const device_queue& cqueue, void* dst, const size_t size = 0, const size_t offset = 0) override;
	
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
	
	//! returns the Vulkan specific buffer object/pointer
	const VkBuffer& get_vulkan_buffer() const { return buffer; }
	const VkDeviceAddress& get_vulkan_buffer_device_address() const { return buffer_device_address; }
	
	//! returns the Vulkan shared memory handle (nullptr/0 if !shared)
	const auto& get_vulkan_shared_handle() const {
		return shared_handle;
	}
	
	//! returns the actual allocation size in bytes this buffer has been created with
	//! NOTE: allocation size has been computed via vkGetBufferMemoryRequirements2 and can differ from "size"
	const VkDeviceSize& get_vulkan_allocation_size() const {
		return allocation_size;
	}
	
	void set_debug_label(const std::string& label) override;
	
	bool is_heap_allocated() const override {
		return is_heap_allocation;
	}
	
	//! max size of an SSBO descriptor
	static constexpr const uint32_t max_ssbo_descriptor_size { 16u };
	
	//! returns the descriptor data for this buffer (for use in descriptor buffers)
	const auto& get_vulkan_descriptor_data() const {
		return descriptor_data;
	}
	
	//! returns the usage flags that this Vulkan buffer was created with
	VkBufferUsageFlags2 get_vulkan_buffer_usage() const {
		return buffer_usage;
	}
	
protected:
	VkBuffer buffer { nullptr };
	VkDeviceSize allocation_size { 0 };
	VkDeviceAddress buffer_device_address { 0 };
	VkBufferUsageFlags2 buffer_usage { 0 };
	
	//! when using descriptor buffers, this contains the descriptor data (as a SSBO descriptor)
	std::array<uint8_t, max_ssbo_descriptor_size> descriptor_data {{
		0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u,
		0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u,
	}};
	
	// shared memory handle when the buffer has been created with VULKAN_SHARING
#if defined(__WINDOWS__)
	void* /* HANDLE */ shared_handle { nullptr };
#else
	int shared_handle { 0 };
#endif
	
	//! separate create buffer function, b/c it's called by the constructor and resize
	bool create_internal(const bool copy_host_data, const device_queue& cqueue);
	
};

} // namespace fl

#endif
