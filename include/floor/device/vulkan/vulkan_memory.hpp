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

#include <floor/device/device_memory.hpp>

namespace fl {

class vulkan_device;
class vulkan_queue;
class device_queue;

//! helper class for common code between vulkan_buffer and vulkan_image
class vulkan_memory {
public:
	virtual ~vulkan_memory() noexcept;
	
protected:
	vulkan_memory(const vulkan_device& vk_dev,
				  const uint64_t* object,
				  // if false: is buffer
				  const bool is_image,
				  const MEMORY_FLAG memory_flags) noexcept;
	
	vulkan_memory(const vulkan_device& vk_dev_,
				  const VkBuffer* buffer_,
				  const MEMORY_FLAG memory_flags_) noexcept :
	vulkan_memory(vk_dev_, (const uint64_t*)buffer_, false, memory_flags_) {}
	
	vulkan_memory(const vulkan_device& vk_dev_,
				  const VkImage* image,
				  const MEMORY_FLAG memory_flags_) noexcept :
	vulkan_memory(vk_dev_, (const uint64_t*)image, true, memory_flags_) {}
	
	const vulkan_device& vk_dev;
	const uint64_t* object { nullptr };
	VkDeviceMemory mem { nullptr };
	const bool is_image { false };
	const MEMORY_FLAG memory_flags { MEMORY_FLAG::NONE };
	
	// heap allocation vars
	VmaAllocation heap_allocation { nullptr };
	bool is_heap_allocation { false };
	bool is_heap_allocation_host_visible { false };
	
	struct vulkan_mapping {
		void* base_address { nullptr };
		VkBuffer buffer { nullptr };
		VkDeviceMemory mem { nullptr };
		VmaAllocation staging_allocation { nullptr };
		const size_t size { 0u };
		const size_t offset { 0u };
		const MEMORY_MAP_FLAG flags {};
	};
	// stores all mapped pointers and the mapped buffer
	std::unordered_map<void*, vulkan_mapping> mappings;
	
	//! overwrites memory data with the host data pointed to by data, with the specified size/offset
	bool write_memory_data(const device_queue& cqueue,
						   const std::span<const uint8_t> data, const size_t& offset,
						   const size_t shim_input_size = 0,
						   const char* error_msg_on_failure = nullptr);
	//! reads memory from the device with the specified size/offset and writes it to the specified host pointer
	bool read_memory_data(const device_queue& cqueue,
						  std::span<uint8_t> data, const size_t& offset,
						  const size_t shim_input_size = 0,
						  const char* error_msg_on_failure = nullptr) const;
	
	void* __attribute__((aligned(128))) map(const device_queue& cqueue,
											const MEMORY_MAP_FLAG flags,
											const size_t size = 0, const size_t offset = 0);
	
	bool unmap(const device_queue& cqueue, void* __attribute__((aligned(128))) mapped_ptr);
	
	virtual void image_copy_dev_to_host(const device_queue&, VkCommandBuffer, VkBuffer) {}
	virtual void image_copy_host_to_dev(const device_queue&, VkCommandBuffer, VkBuffer, std::span<uint8_t>) {}
	
	//! based on the specified/supported memory type bits and "wants device memory" flag,
	//! this tries to find the best matching memory type index (heap / location)
	uint32_t find_memory_type_index(const uint32_t memory_type_bits,
									const bool want_device_memory,
									const bool requires_device_memory = false,
									const bool requires_host_coherent = false) const;
	
};

} // namespace fl

#endif
