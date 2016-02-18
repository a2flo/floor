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

#include <floor/compute/vulkan/vulkan_queue.hpp>

#if !defined(FLOOR_NO_VULKAN)
#include <floor/compute/vulkan/vulkan_device.hpp>

vulkan_queue::vulkan_queue(shared_ptr<compute_device> device_, const VkQueue queue_, const uint32_t family_index_) :
compute_queue(device_), queue(queue_), family_index(family_index_) {
	const VkCommandPoolCreateInfo cmd_pool_info {
		.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
		.pNext = nullptr,
		// always short-lived + need individual reset
		.flags = (VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT),
		.queueFamilyIndex = family_index,
	};
	VK_CALL_RET(vkCreateCommandPool(((vulkan_device*)device_.get())->device, &cmd_pool_info, nullptr, &cmd_pool), "failed to create command pool");
	// TODO: vkDestroyCommandPool necessary in destructor? this will live as long as the context/instance does
}

void vulkan_queue::finish() const {
	VK_CALL_RET(vkQueueWaitIdle(queue), "queue finish failed");
}

void vulkan_queue::flush() const {
	// nop
}

const void* vulkan_queue::get_queue_ptr() const {
	return queue;
}

#endif
