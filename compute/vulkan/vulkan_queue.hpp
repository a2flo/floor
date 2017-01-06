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

#ifndef __FLOOR_VULKAN_QUEUE_HPP__
#define __FLOOR_VULKAN_QUEUE_HPP__

#include <floor/compute/vulkan/vulkan_common.hpp>

#if !defined(FLOOR_NO_VULKAN)

#include <floor/compute/compute_queue.hpp>
#include <floor/compute/vulkan/vulkan_kernel.hpp>
#include <bitset>

class vulkan_queue final : public compute_queue {
public:
	vulkan_queue(shared_ptr<compute_device> device, VkQueue queue, const uint32_t family_index);
	
	void finish() const override REQUIRES(!queue_lock);
	void flush() const override;
	
	// this is synchronized elsewhere
	const void* get_queue_ptr() const override NO_THREAD_SAFETY_ANALYSIS {
		return queue;
	}
	
	uint32_t get_family_index() const {
		return family_index;
	}
	
	struct command_buffer {
		VkCommandBuffer cmd_buffer { nullptr };
		uint32_t index { ~0u };
		const char* name { nullptr };
	};
	command_buffer make_command_buffer(const char* name = nullptr) REQUIRES(!cmd_buffers_lock);
	
	void submit_command_buffer(command_buffer cmd_buffer,
							   const bool blocking = true,
							   const VkSemaphore* wait_semas = nullptr,
							   const uint32_t wait_sema_count = 0,
							   const VkPipelineStageFlags wait_stage_flags = 0) REQUIRES(!cmd_buffers_lock, !queue_lock);
	void submit_command_buffer(command_buffer cmd_buffer,
							   function<void(const command_buffer&)> completion_handler,
							   const bool blocking = true,
							   const VkSemaphore* wait_semas = nullptr,
							   const uint32_t wait_sema_count = 0,
							   const VkPipelineStageFlags wait_stage_flags = 0) REQUIRES(!cmd_buffers_lock, !queue_lock);
	
protected:
	const VkQueue queue GUARDED_BY(queue_lock);
	mutable safe_mutex queue_lock;
	const uint32_t family_index;
	VkCommandPool cmd_pool;

	mutable safe_mutex cmd_buffers_lock;
	static constexpr const uint32_t cmd_buffer_count {
		// make use of optimized bitset
#if defined(PLATFORM_X64)
		64
#else
		32
#endif
	};
	array<VkCommandBuffer, cmd_buffer_count> cmd_buffers GUARDED_BY(cmd_buffers_lock);
	bitset<cmd_buffer_count> cmd_buffers_in_use GUARDED_BY(cmd_buffers_lock);
	
	static constexpr const uint32_t fence_count { 32 };
	mutable safe_mutex fence_lock;
	array<VkFence, fence_count> fences GUARDED_BY(fence_lock);
	bitset<fence_count> fences_in_use GUARDED_BY(fence_lock);
	pair<VkFence, uint32_t> acquire_fence() REQUIRES(!fence_lock);
	void release_fence(VkDevice dev, const pair<VkFence, uint32_t>& fence) REQUIRES(!fence_lock);
	
};

#endif

#endif
