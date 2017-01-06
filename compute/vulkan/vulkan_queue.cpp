/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2017 Florian Ziesche
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
#include <floor/threading/task.hpp>

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
	// TODO: manage this dynamically (for now: 32/64 must be enough)
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
	
	// create fences
	static constexpr const VkFenceCreateInfo fence_info {
		.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
		.pNext = nullptr,
		.flags = 0,
	};
	for(uint32_t i = 0; i < fence_count; ++i) {
		VK_CALL_RET(vkCreateFence(((vulkan_device*)device.get())->device, &fence_info, nullptr, &fences[i]),
					"failed to create fence #" + to_string(i));
	}
	fences_in_use.reset();
}

void vulkan_queue::finish() const {
	GUARD(queue_lock);
	VK_CALL_RET(vkQueueWaitIdle(queue), "queue finish failed");
}

void vulkan_queue::flush() const {
	// nop
}

static const char* cmd_buffer_name(const vulkan_queue::command_buffer& cmd_buffer) {
	return (cmd_buffer.name != nullptr ? cmd_buffer.name : "unknown");
}

vulkan_queue::command_buffer vulkan_queue::make_command_buffer(const char* name) {
	GUARD(cmd_buffers_lock);
	if(!cmd_buffers_in_use.all()) {
		for(uint32_t i = 0; i < cmd_buffer_count; ++i) {
			if(!cmd_buffers_in_use[i]) {
				// TODO: don't block
				VK_CALL_RET(vkResetCommandBuffer(cmd_buffers[i], VK_COMMAND_BUFFER_RESET_RELEASE_RESOURCES_BIT),
							"failed to reset command buffer ("s + (name != nullptr ? name : "unknown") + ")",
							{ nullptr, ~0u, nullptr });
				cmd_buffers_in_use.set(i);
				return { cmd_buffers[i], i, name };
			}
		}
		// shouldn't get here if .all() check fails
		floor_unreachable();
	}
	log_error("all command buffers are currently in use (implementation limitation right now)");
	return {};
}

pair<VkFence, uint32_t> vulkan_queue::acquire_fence() {
	for(uint32_t trial = 0, limiter = 10; trial < limiter; ++trial) {
		{
			GUARD(fence_lock);
			if(!fences_in_use.all()) {
				for(uint32_t i = 0; i < fence_count; ++i) {
					if(!fences_in_use[i]) {
						fences_in_use.set(i);
						return { fences[i], i };
					}
				}
				floor_unreachable();
			}
		}
		this_thread::sleep_for(1ms);
	}
	log_error("failed to acquire a fence");
	return { nullptr, ~0u };
}

void vulkan_queue::release_fence(VkDevice dev, const pair<VkFence, uint32_t>& fence) {
	VK_CALL_RET(vkResetFences(dev, 1, &fence.first),
				"failed to reset fence");
	
	GUARD(fence_lock);
	fences_in_use.reset(fence.second);
}

void vulkan_queue::submit_command_buffer(command_buffer cmd_buffer,
										 const bool blocking,
										 const VkSemaphore* wait_semas,
										 const uint32_t wait_sema_count,
										 const VkPipelineStageFlags wait_stage_flags) {
	submit_command_buffer(cmd_buffer, [](const command_buffer&){}, blocking,
						  wait_semas, wait_sema_count, wait_stage_flags);
}

void vulkan_queue::submit_command_buffer(vulkan_queue::command_buffer cmd_buffer,
										 function<void(const command_buffer&)> completion_handler,
										 const bool blocking,
										 const VkSemaphore* wait_semas,
										 const uint32_t wait_sema_count,
										 const VkPipelineStageFlags wait_stage_flags) {
	const auto submit_func = [this, cmd_buffer, completion_handler,
							  wait_semas, wait_sema_count, wait_stage_flags,
							  dev = ((vulkan_device*)device.get())->device]() {
		// must sync/lock queue
		auto fence = acquire_fence();
		if(fence.first == nullptr) return;
		{
			GUARD(queue_lock);
			const VkSubmitInfo submit_info {
				.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
				.pNext = nullptr,
				.waitSemaphoreCount = wait_sema_count,
				.pWaitSemaphores = wait_semas,
				.pWaitDstStageMask = (wait_stage_flags != 0 ? &wait_stage_flags : nullptr),
				.commandBufferCount = 1,
				.pCommandBuffers = &cmd_buffer.cmd_buffer,
				.signalSemaphoreCount = 0,
				.pSignalSemaphores = nullptr,
			};
			const auto submit_err = vkQueueSubmit(queue, 1, &submit_info, fence.first);
			if(submit_err != VK_SUCCESS) {
				log_error("failed to submit queue (%s): %u: %s",
						  cmd_buffer_name(cmd_buffer), submit_err, vulkan_error_to_string(submit_err));
				// still continue here to free the cmd buffer
			}
		}
		
		// TODO: it might make sense to sleep+wait+vkGetFenceStatus instead of vkWaitForFences
		// TODO: connect a fence to a cmd buffer allocation, this way they don't need to be created + destroyed every time
		// TODO: instead of creating a completion handler thread every time, it's probably better to have just one (or two) threads to handle this (+vkGetFenceStatus)
		
		for(;;) {
			auto status = vkGetFenceStatus(dev, fence.first);
			//log_debug(">> fence %X status: %X", fence.first, status);
			if(status == VK_SUCCESS) break;
			this_thread::sleep_for(1ms);
		}
		
		// reset + release fence
		release_fence(dev, fence);
		
		// call user-specified handler
		completion_handler(cmd_buffer);
		
		// mark cmd buffer as free again
		{
			GUARD(cmd_buffers_lock);
			cmd_buffers_in_use.reset(cmd_buffer.index);
		}
	};
	
	if(!blocking) {
		// spawn handler thread
		task::spawn(submit_func, "q_submit_hndlr");
	}
	else {
		// block until done or timeout
		submit_func();
	}
}

#endif
