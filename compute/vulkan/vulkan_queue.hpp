/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2023 Florian Ziesche
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
	bool is_secondary { false };
	
	explicit operator bool() const { return (cmd_buffer != nullptr); }
};

class vulkan_queue;
class vulkan_command_block;
struct vulkan_queue_impl;
struct vulkan_command_pool_t;

class vulkan_queue final : public compute_queue {
public:
	explicit vulkan_queue(const compute_device& device, VkQueue queue, const uint32_t family_index);
	~vulkan_queue() override;
	
	static void init();
	static void destroy();
	
	void finish() const override REQUIRES(!queue_lock);
	void flush() const override;
	
	void execute_indirect(const indirect_command_pipeline& indirect_cmd,
						  const indirect_execution_parameters_t& params,
						  kernel_completion_handler_f&& completion_handler,
						  const uint32_t command_offset,
						  const uint32_t command_count) const override REQUIRES(!queue_lock);
	
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
	
	//! definition for a fence that should be waited on before command buffer submission
	struct wait_fence_t {
		const compute_fence* fence { nullptr };
		uint64_t signaled_value { 0 };
		compute_fence::SYNC_STAGE stage { compute_fence::SYNC_STAGE::NONE };
	};
	
	//! definition for a fence that should be signaled after command buffer execution
	struct signal_fence_t {
		compute_fence* fence { nullptr };
		uint64_t unsignaled_value { 0 };
		uint64_t signaled_value { 0 };
		compute_fence::SYNC_STAGE stage { compute_fence::SYNC_STAGE::NONE };
	};
	
	vulkan_command_block make_command_block(const char* name, bool& error_signal, const bool is_blocking,
											vector<wait_fence_t>&& wait_fences = {},
											vector<signal_fence_t>&& signal_fences = {}) const;
	
	vulkan_command_buffer make_command_buffer(const char* name = nullptr) const;
	
	//! submits the specified "cmd_buffer" to this queue,
	//! execution will wait until all "wait_fences" requirements are fulfilled,
	//! "signal_fences" will be signaled once their requirements are fulfilled (the cmd buffer has completed execution up to sync stage),
	//! "completion_handler" will be called once the cmd buffer fully completed execution,
	//! "blocking" signals if the function should not return until the cmd buffer has fully completed execution
	//! NOTE: ownership of "cmd_buffer", "wait_fences", "signal_fences" and "completion_handler" are transferred to this function
	//! NOTE: do not rely on the address of the cmd buffer parameter in "completion_handler", this may not be the same as the initial one
	void submit_command_buffer(vulkan_command_buffer&& cmd_buffer,
							   vector<wait_fence_t>&& wait_fences,
							   vector<signal_fence_t>&& signal_fences,
							   function<void(const vulkan_command_buffer&)>&& completion_handler,
							   const bool blocking = true) const REQUIRES(!queue_lock);
	
	//! creates a secondary command buffer (e.g. for use during rendering)
	vulkan_command_buffer make_secondary_command_buffer(const char* name = nullptr) const;
	
	//! executes the specified secondary command buffer within the specified primary command buffer
	//! NOTE: this will automatically hold onto the secondary command buffer until the primary has completed execution
	bool execute_secondary_command_buffer(const vulkan_command_buffer& primary_cmd_buffer,
										  const vulkan_command_buffer& secondary_cmd_buffer) const;
	
	//! attaches buffers to the specified command buffer that will be retained until the command buffer has finished execution
	//! NOTE: must be called before submit_command_buffer, otherwise this has no effect
	void add_retained_buffers(const vulkan_command_buffer& cmd_buffer,
							  const vector<shared_ptr<compute_buffer>>& buffers) const;
	
	//! completion handler type for "add_completion_handler"
	using vulkan_completion_handler_t = function<void()>;
	
	//! adds a completion handler to the specified command buffer that is called once the command buffer has finished execution
	//! NOTE: must be called before submit_command_buffer, otherwise this has no effect
	void add_completion_handler(const vulkan_command_buffer& cmd_buffer,
								vulkan_completion_handler_t&& completion_handler) const;
	
	void set_debug_label(const string& label) override REQUIRES(!queue_lock);
	
protected:
	VkQueue vk_queue GUARDED_BY(queue_lock);
	mutable safe_mutex queue_lock;
	const uint32_t family_index;
	unique_ptr<vulkan_queue_impl> impl;
	
	friend vulkan_command_pool_t;
	friend vulkan_queue_impl;
	
	//! internal queue submit
	VkResult queue_submit(const VkSubmitInfo2& submit_info, VkFence& fence) const REQUIRES(!queue_lock);
	
};

//! command buffer block that will automatically start the cmd buffer on construction and end + submit it on destruction
class vulkan_command_block {
public:
	vulkan_command_buffer cmd_buffer {};
	
	vulkan_command_block(const vulkan_queue& vk_queue, const char* name, bool& error_signal, const bool is_blocking,
						 vector<vulkan_queue::wait_fence_t>&& wait_fences = {}, vector<vulkan_queue::signal_fence_t>&& signal_fences = {});
	~vulkan_command_block();
	
	bool valid { false };
	explicit operator bool() const {
		return valid;
	}
	
protected:
	const vulkan_queue& vk_queue;
	bool& error_signal;
	const bool is_blocking { true };

	vector<vulkan_queue::wait_fence_t> wait_fences;
	vector<vulkan_queue::signal_fence_t> signal_fences;
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
		auto& block_cmd_buffer = cmd_block_.cmd_buffer; \
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
