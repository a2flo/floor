/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2016 Florian Ziesche
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

#ifndef __FLOOR_VULKAN_DEVICE_HPP__
#define __FLOOR_VULKAN_DEVICE_HPP__

#include <floor/compute/vulkan/vulkan_common.hpp>
#include <floor/compute/compute_device.hpp>

FLOOR_PUSH_WARNINGS()
FLOOR_IGNORE_WARNING(weak-vtables)

#if !defined(FLOOR_NO_VULKAN)
class vulkan_compute;
#endif

class vulkan_device final : public compute_device {
public:
	vulkan_device();
	~vulkan_device() override {}
	
#if !defined(FLOOR_NO_VULKAN)
	//! physical vulkan device
	VkPhysicalDevice physical_device { nullptr };
	
	//! logical vulkan device
	VkDevice device { nullptr };
	
	//! memory properties of the device/implementation/host
	shared_ptr<VkPhysicalDeviceMemoryProperties> mem_props;
#else
	void* _physical_device { nullptr };
	void* _device { nullptr };
	shared_ptr<void*> _mem_props;
#endif
	
	//! queue count per queue family
	//! (obviously also stores the queue family count)
	vector<uint32_t> queue_counts {};
	
	//! for internal purposes, do not change this
	uint32_t cur_queue_idx { 0 };
	
	//! max push constants size
	uint32_t max_push_constants_size { 0u };
	
	//! memory type index for device memory allocation
	uint32_t device_mem_index { ~0u };
	
	//! memory type index for (potentially cached) host + device-visible memory allocation
	uint32_t host_mem_cached_index { ~0u };
	
	//! memory type index for (potentially uncached) host + device-visible memory allocation
	uint32_t host_mem_uncached_index { ~0u };
	
};

FLOOR_POP_WARNINGS()

#endif
