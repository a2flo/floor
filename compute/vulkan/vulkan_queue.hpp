/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2021 Florian Ziesche
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
#include <floor/threading/thread_safety.hpp>
#include <bitset>

struct vulkan_command_buffer {
	VkCommandBuffer cmd_buffer { nullptr };
	uint32_t index { ~0u };
	const char* name { nullptr };
	
	explicit operator bool() const { return (cmd_buffer != nullptr); }
};

class vulkan_queue;

//! command buffer block that will automatically start the cmd buffer on construction and end + submit it on destruction
struct vulkan_command_block {
	vulkan_command_buffer cmd_buffer {};
	
	vulkan_command_block(const vulkan_queue& vk_queue, const char* name, bool& error_signal, const bool is_blocking,
						 const VkSemaphore* wait_semas = nullptr, const uint32_t wait_sema_count = 0,
						 const VkPipelineStageFlags wait_stage_flags = 0);
	~vulkan_command_block();
	
	bool valid { false };
	explicit operator bool() const {
		return valid;
	}
	
protected:
	const vulkan_queue& vk_queue;
	bool& error_signal;
	const bool is_blocking { true };
	
	const VkSemaphore* wait_semas { nullptr };
	const uint32_t wait_sema_count { 0u };
	const VkPipelineStageFlags wait_stage_flags { 0 };
};

struct vulkan_queue_impl;
struct vulkan_command_pool_t;

class vulkan_queue final : public compute_queue {
public:
	explicit vulkan_queue(const compute_device& device, VkQueue queue, const uint32_t family_index);
	~vulkan_queue() override;
	
	void finish() const override REQUIRES(!queue_lock);
	void flush() const override;
	
	// this is synchronized elsewhere
	const void* get_queue_ptr() const override NO_THREAD_SAFETY_ANALYSIS {
		return vk_queue;
	}
	void* get_queue_ptr() override NO_THREAD_SAFETY_ANALYSIS {
		return vk_queue;
	}
	
	uint32_t get_family_index() const {
		return family_index;
	}
	
	vulkan_command_block make_command_block(const char* name, bool& error_signal, const bool is_blocking,
											const VkSemaphore* wait_semas = nullptr, const uint32_t wait_sema_count = 0,
											const VkPipelineStageFlags wait_stage_flags = 0) const;
	
	vulkan_command_buffer make_command_buffer(const char* name = nullptr) const;
	
	// TODO: manual cmd buffer creation (+optional start)
	
	void submit_command_buffer(const vulkan_command_buffer& cmd_buffer,
							   const bool blocking = true,
							   const VkSemaphore* wait_semas = nullptr,
							   const uint32_t wait_sema_count = 0,
							   const VkPipelineStageFlags wait_stage_flags = 0) const REQUIRES(!queue_lock);
	void submit_command_buffer(const vulkan_command_buffer& cmd_buffer,
							   function<void(const vulkan_command_buffer&)> completion_handler,
							   const bool blocking = true,
							   const VkSemaphore* wait_semas = nullptr,
							   const uint32_t wait_sema_count = 0,
							   const VkPipelineStageFlags wait_stage_flags = 0) const REQUIRES(!queue_lock);
	
	//! attaches buffers to the specified command buffer that will be retained until the command buffer has finished execution
	//! NOTE: must be called before submit_command_buffer, otherwise this has no effect
	void add_retained_buffers(const vulkan_command_buffer& cmd_buffer,
							  const vector<shared_ptr<compute_buffer>>& buffers) const;
	
	//! completion handler type for "add_completion_handler"
	using vulkan_completion_handler_t = function<void()>;
	
	//! adds a completion handler to the specified command buffer that is called once the command buffer has finished execution
	//! NOTE: must be called before submit_command_buffer, otherwise this has no effect
	void add_completion_handler(const vulkan_command_buffer& cmd_buffer,
								vulkan_completion_handler_t completion_handler) const;
	
protected:
	VkQueue vk_queue GUARDED_BY(queue_lock);
	mutable safe_mutex queue_lock;
	const uint32_t family_index;
	unique_ptr<vulkan_queue_impl> impl;
	
	friend vulkan_command_pool_t;
	friend vulkan_queue_impl;
	
	//! internal queue submit
	VkResult queue_submit(const VkSubmitInfo& submit_info, VkFence& fence) const REQUIRES(!queue_lock);
	
};

//! creates a "command block", i.e. creates a command buffer, starts it, runs the code specified as "...", and finally submits the buffer,
//! returns "ret" on error
#define VK_CMD_BLOCK_RET(vk_queue, name, code, ret, is_blocking, ...) \
do { \
	bool error_signal_ = false; \
	{ \
		auto cmd_block_ = vk_queue.make_command_block(name, error_signal_, is_blocking, ##__VA_ARGS__); \
		if (!cmd_block_ || error_signal_) { \
			return ret; \
		} \
		auto& cmd_buffer = cmd_block_.cmd_buffer; \
		code; \
	} \
	if (error_signal_) { \
		return ret; \
	} \
} while (false)
//! creates a "command block", i.e. creates a command buffer, starts it, runs the code specified as "...", and finally submits the buffer
#define VK_CMD_BLOCK(vk_queue, name, code, is_blocking, ...) VK_CMD_BLOCK_RET(vk_queue, name, code, {}, is_blocking, ##__VA_ARGS__)

#endif

#endif
