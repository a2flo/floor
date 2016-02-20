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
	// create command pool for this queue + device
	const VkCommandPoolCreateInfo cmd_pool_info {
		.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
		.pNext = nullptr,
		// always short-lived + need individual reset
		.flags = (VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT),
		.queueFamilyIndex = family_index,
	};
	VK_CALL_RET(vkCreateCommandPool(((vulkan_device*)device.get())->device, &cmd_pool_info, nullptr, &cmd_pool), "failed to create command pool");
	// TODO: vkDestroyCommandPool necessary in destructor? this will live as long as the context/instance does
	
	// allocate initial command buffers
	// TODO: manage this dynamically (for now: 32 must be enough)
	const VkCommandBufferAllocateInfo cmd_buffer_info {
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
		.pNext = nullptr,
		.commandPool = cmd_pool,
		// always create primary for now
		.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
		.commandBufferCount = cmd_buffer_count,
	};
	VK_CALL_RET(vkAllocateCommandBuffers(((vulkan_device*)device.get())->device, &cmd_buffer_info, &cmd_buffers[0]), "failed to create command buffers");
	cmd_buffers_in_use.reset();
}

void vulkan_queue::finish() const {
	GUARD(queue_lock);
	VK_CALL_RET(vkQueueWaitIdle(queue), "queue finish failed");
}

void vulkan_queue::flush() const {
	// nop
}

const void* vulkan_queue::get_queue_ptr() const {
	return queue;
}

vulkan_queue::command_buffer vulkan_queue::make_command_buffer() {
	GUARD(cmd_buffers_lock);
	if(!cmd_buffers_in_use.all()) {
		for(uint32_t i = 0; i < cmd_buffer_count; ++i) {
			if(!cmd_buffers_in_use[i]) {
				VK_CALL_RET(vkResetCommandBuffer(cmd_buffers[i], VK_COMMAND_BUFFER_RESET_RELEASE_RESOURCES_BIT),
							"failed to reset command buffer", { nullptr, ~0u });
				cmd_buffers_in_use.set(i);
				return { cmd_buffers[i], i };
			}
		}
		// shouldn't get here if .all() check fails
		floor_unreachable();
	}
	log_error("all command buffers are currently in use (implementation limitation right now)");
	return { nullptr, ~0u };
}

void vulkan_queue::submit_command_buffer(vulkan_queue::command_buffer cmd_buffer) {
	// must sync/lock queue
	{
		GUARD(queue_lock);
		const VkSubmitInfo submit_info {
			.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
			.pNext = nullptr,
			.waitSemaphoreCount = 0,
			.pWaitSemaphores = nullptr,
			.pWaitDstStageMask = nullptr,
			.commandBufferCount = 1,
			.pCommandBuffers = &cmd_buffer.cmd_buffer,
			.signalSemaphoreCount = 0,
			.pSignalSemaphores = nullptr,
		};
		const auto submit_err = vkQueueSubmit(queue, 1, &submit_info, VK_NULL_HANDLE);
		if(submit_err != VK_SUCCESS) {
			log_error("failed to submit queue: %u: %s", submit_err, vulkan_error_to_string(submit_err));
			// still continue here to free the cmd buffer
		}
	}
	
	// mark cmd buffer as free again
	// TODO: this needs to be handled _after_ the cmd buffer has been executed, not here
	{
		GUARD(cmd_buffers_lock);
		cmd_buffers_in_use.reset(cmd_buffer.index);
	}
}

#endif
