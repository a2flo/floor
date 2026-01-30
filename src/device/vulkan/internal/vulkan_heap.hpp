/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2026 Florian Ziesche
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
#include <floor/device/vulkan/vulkan_device.hpp>
#include "device/vulkan/internal/vulkan_headers.hpp"
#include <floor/device/device_memory_flags.hpp>
#include <floor/device/backend/image_types.hpp>

VK_DEFINE_HANDLE(VmaAllocator)

namespace fl {

class vulkan_heap {
public:
	vulkan_heap(const vulkan_device& dev_);
	
	//! return value of create_buffer(), check is_valid() if this is a valid allocation
	struct buffer_allocation_t {
		VkBuffer buffer { nullptr };
		VmaAllocation allocation { nullptr };
		VkDeviceMemory memory { nullptr };
		VkDeviceSize allocation_size { 0u };
		bool is_host_visible { false };
		
		bool is_valid() const {
			return (buffer != nullptr && allocation != nullptr);
		}
	};
	
	//! creates a buffer allocation in the heap using the specified memory flags and Vulkan create info
	[[nodiscard]] buffer_allocation_t create_buffer(const VkBufferCreateInfo& create_info,
													const MEMORY_FLAG flags);
	
	//! return value of create_image(), check is_valid() if this is a valid allocation
	struct image_allocation_t {
		VkImage image { nullptr };
		VmaAllocation allocation { nullptr };
		VkDeviceMemory memory { nullptr };
		VkDeviceSize allocation_size { 0u };
		bool is_host_visible { false };
		
		bool is_valid() const {
			return (image != nullptr && allocation != nullptr);
		}
	};
	
	//! creates an image allocation in the heap using the specified memory flags and Vulkan create info
	[[nodiscard]] image_allocation_t create_image(const VkImageCreateInfo& create_info,
												  const MEMORY_FLAG flags, const IMAGE_TYPE image_type);
	
	//! destroys a previously made buffer/image allocation
	void destroy_allocation(VmaAllocation allocation, VkBuffer buffer);
	void destroy_allocation(VmaAllocation allocation, VkImage image);
	
	//! maps the memory allocation in CPU-accessible memory
	void* map_memory(VmaAllocation allocation);
	//! unmaps a previously made CPU-accessible memory mapping
	void unmap_memory(VmaAllocation allocation);
	
	//! copies "copy_size" bytes of host memory from "host_ptr" to the device memory specified by "allocation" to byte offset "alloc_offset"
	//! NOTE: "allocation" must be host-visible
	bool host_to_device_copy(const void* host_ptr, VmaAllocation allocation, const uint64_t alloc_offset, const uint64_t copy_size);
	//! copies "copy_size" bytes of device memory specified by "allocation" from byte offset "alloc_offset" to host memory specified by "host_ptr"
	//! NOTE: "allocation" must be host-visible
	bool device_to_host_copy(VmaAllocation allocation, void* host_ptr, const uint64_t alloc_offset, const uint64_t copy_size);
	
	//! returns the total amount of bytes that are currently allocated through this heap
	//! NOTE: we don't report a max/total heap allocation size here, because it is generally the same as the device memory size
	uint64_t query_total_usage();
	
protected:
	const vulkan_device& dev;
	VmaAllocator allocator { nullptr };
	
};

} // namespace fl

#endif
