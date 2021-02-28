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

#include <floor/compute/vulkan/vulkan_queue.hpp>

#if !defined(FLOOR_NO_VULKAN)
#include <floor/compute/vulkan/vulkan_device.hpp>
#include <floor/compute/vulkan/vulkan_compute.hpp>
#include <floor/core/logger.hpp>
#include <floor/core/core.hpp>
#include <floor/threading/task.hpp>

//! returns the debug name of the specified buffer or "unknown"
static inline const char* cmd_buffer_name(const vulkan_command_buffer& cmd_buffer) {
	return (cmd_buffer.name != nullptr ? cmd_buffer.name : "unknown");
}

//! per-thread Vulkan command pool and command buffer management
//! NOTE: since Vulkan is *not* thread-safe, we need to manage this on our own
struct vulkan_command_pool_t {
	VkCommandPool cmd_pool;
	const vulkan_device& dev;
	const vulkan_queue& queue;
	
	vulkan_command_pool_t(const vulkan_device& dev_, const vulkan_queue& queue_) : dev(dev_), queue(queue_) {}
	// TODO: destructor (manually + on thread exit)
	
	struct command_buffer_internal {
		vector<shared_ptr<compute_buffer>> retained_buffers;
		vector<vulkan_queue::vulkan_completion_handler_t> completion_handlers;
	};

	safe_mutex cmd_buffers_lock;
	static constexpr const uint32_t cmd_buffer_count {
		// make use of optimized bitset
		64
	};
	array<VkCommandBuffer, cmd_buffer_count> cmd_buffers GUARDED_BY(cmd_buffers_lock);
	array<command_buffer_internal, cmd_buffer_count> cmd_buffer_internals GUARDED_BY(cmd_buffers_lock);
	bitset<cmd_buffer_count> cmd_buffers_in_use GUARDED_BY(cmd_buffers_lock);
	
	static constexpr const uint32_t fence_count { 32 };
	safe_mutex fence_lock;
	array<VkFence, fence_count> fences GUARDED_BY(fence_lock);
	bitset<fence_count> fences_in_use GUARDED_BY(fence_lock);
	
	//! acquire an unused fence
	pair<VkFence, uint32_t> acquire_fence() REQUIRES(!fence_lock) {
		for (uint32_t trial = 0, limiter = 10; trial < limiter; ++trial) {
			{
				GUARD(fence_lock);
				if (!fences_in_use.all()) {
					for (uint32_t i = 0; i < fence_count; ++i) {
						if (!fences_in_use[i]) {
							fences_in_use.set(i);
							return { fences[i], i };
						}
					}
					floor_unreachable();
				}
			}
			this_thread::yield();
		}
		log_error("failed to acquire a fence");
		return { nullptr, ~0u };
	}
	
	//! release a used fence again
	void release_fence(const pair<VkFence, uint32_t>& fence) REQUIRES(!fence_lock) {
		VK_CALL_RET(vkResetFences(dev.device, 1, &fence.first), "failed to reset fence")
		
		GUARD(fence_lock);
		fences_in_use.reset(fence.second);
	}
	
	//! acquires an unused command buffer (resets an old unused one)
	vulkan_command_buffer make_command_buffer(const char* name = nullptr) REQUIRES(!cmd_buffers_lock) {
		GUARD(cmd_buffers_lock);
		if (!cmd_buffers_in_use.all()) {
			for (uint32_t i = 0; i < cmd_buffer_count; ++i) {
				if (!cmd_buffers_in_use[i]) {
					// TODO: don't block
					VK_CALL_RET(vkResetCommandBuffer(cmd_buffers[i], VK_COMMAND_BUFFER_RESET_RELEASE_RESOURCES_BIT),
								"failed to reset command buffer ("s + (name != nullptr ? name : "unknown") + ")",
								{ nullptr, ~0u, nullptr })
					cmd_buffers_in_use.set(i);
					cmd_buffer_internals[i].retained_buffers.clear();
					cmd_buffer_internals[i].completion_handlers.clear();
					return { cmd_buffers[i], i, name };
				}
			}
			// shouldn't get here if .all() check fails
			floor_unreachable();
		}
		log_error("all command buffers are currently in use (implementation limitation right now)");
		return {};
	}
	
	//! submits a command buffer to the device queue
	void submit_command_buffer(const vulkan_command_buffer& cmd_buffer, function<void(const vulkan_command_buffer&)> completion_handler,
							   const bool blocking = true, const VkSemaphore* wait_semas = nullptr, const uint32_t wait_sema_count = 0,
							   const VkPipelineStageFlags wait_stage_flags = 0) REQUIRES(!cmd_buffers_lock) {
		const auto submit_func = [this, cmd_buffer, completion_handler,
								  wait_semas, wait_sema_count, wait_stage_flags]() {
			// must sync/lock queue
			auto fence = acquire_fence();
			if (fence.first == nullptr) {
				return;
			}
			
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
			const auto submit_err = queue.queue_submit(submit_info, fence.first);
			if(submit_err != VK_SUCCESS) {
				log_error("failed to submit queue (%s): %u: %s",
						  cmd_buffer_name(cmd_buffer), submit_err, vulkan_error_to_string(submit_err));
				// still continue here to free the cmd buffer
			}
			
			// TODO: connect a fence to a cmd buffer allocation, this way they don't need to be created + destroyed every time
			// TODO: instead of creating a completion handler thread every time, it's probably better to have just one (or two) threads to handle this (+vkGetFenceStatus)
			
			// NOTE: vkWaitForFences is faster + more efficient than a vkGetFenceStatus loop
			const auto wait_ret = vkWaitForFences(dev.device, 1, &fence.first, true, ~0ull);
			if (wait_ret != VK_SUCCESS) {
				if (wait_ret == VK_TIMEOUT) {
					log_error("waiting for fence timed out");
				} else {
					log_error("waiting for fence failed: %s (%u)", vulkan_error_to_string(wait_ret), wait_ret);
				}
			}
			
			// reset + release fence
			release_fence(fence);
			
			// call user-specified handler
			if (completion_handler) {
				completion_handler(cmd_buffer);
			}
			
			// call internal completion handlers and free retained buffers
			vector<shared_ptr<compute_buffer>> retained_buffers;
			vector<vulkan_queue::vulkan_completion_handler_t> completion_handlers;
			{
				GUARD(cmd_buffers_lock);
				auto& internal_cmd_buffer = cmd_buffer_internals[cmd_buffer.index];
				retained_buffers.swap(internal_cmd_buffer.retained_buffers);
				completion_handlers.swap(internal_cmd_buffer.completion_handlers);
			}
			for (const auto& compl_handler : completion_handlers) {
				if (compl_handler) {
					compl_handler();
				}
			}
			retained_buffers.clear();
			
			// mark cmd buffer as free again
			{
				GUARD(cmd_buffers_lock);
				cmd_buffers_in_use.reset(cmd_buffer.index);
			}
		};
		
		// TODO/NOTE: we currently can't handle non-blocking submits (this would mean moving this to a different handler thread,
		// while various things in the command pool/buffers are in use by this thread, which is not supported by Vulkan)
		if (!blocking) {
			log_warn("non-blocking submit is currently not supported");
		}
		
		// block until done or timeout
		submit_func();
	}
	
	//! attach the specified buffer(s) to the specified command buffer (keep them alive while the command buffer is in use)
	void add_retained_buffers(const vulkan_command_buffer& cmd_buffer,
							  const vector<shared_ptr<compute_buffer>>& buffers) REQUIRES(!cmd_buffers_lock) {
		GUARD(cmd_buffers_lock);
		auto& internal_cmd_buffer = cmd_buffer_internals[cmd_buffer.index];
		internal_cmd_buffer.retained_buffers.insert(internal_cmd_buffer.retained_buffers.end(), buffers.begin(), buffers.end());
	}
	
	//! adds a completion handler to the specified command buffer (called once the command buffer has completed execution, successfully or not)
	void add_completion_handler(const vulkan_command_buffer& cmd_buffer,
								vulkan_queue::vulkan_completion_handler_t completion_handler) REQUIRES(!cmd_buffers_lock) {
		GUARD(cmd_buffers_lock);
		cmd_buffer_internals[cmd_buffer.index].completion_handlers.emplace_back(completion_handler);
	}
};

//! internal Vulkan device queue implementation
struct vulkan_queue_impl {
	const vulkan_device& dev;
	const vulkan_queue& queue;
	const uint32_t family_index;
	//! per-thread/thread-local Vulkan command pool/buffers
	static inline thread_local unique_ptr<vulkan_command_pool_t> thread_cmd_pool;

	vulkan_queue_impl(const vulkan_queue& queue_, const vulkan_device& dev_, const uint32_t& family_index_) :
	dev(dev_), queue(queue_), family_index(family_index_) {}
	
	//! creates and initializes the per-thread/thread-local command pool/buffers
	bool create_thread_command_pool() {
		if (thread_cmd_pool) {
			return true;
		}
		thread_cmd_pool = make_unique<vulkan_command_pool_t>(dev, queue);
		MULTI_GUARD(thread_cmd_pool->cmd_buffers_lock, thread_cmd_pool->fence_lock);
		
		// create command pool for this queue + device
		const VkCommandPoolCreateInfo cmd_pool_info {
			.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
			.pNext = nullptr,
			// always short-lived + need individual reset
			.flags = (VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT),
			.queueFamilyIndex = family_index,
		};
		VK_CALL_RET(vkCreateCommandPool(dev.device, &cmd_pool_info, nullptr, &thread_cmd_pool->cmd_pool),
					"failed to create command pool", false)
		// TODO: vkDestroyCommandPool in destructor! this will live as long as the context/instance does
		
#if defined(FLOOR_DEBUG)
		auto thread_name = core::get_current_thread_name();
		if (thread_name.empty()) {
			stringstream sstr;
			sstr << this_thread::get_id();
			thread_name = sstr.str();
		}
		((const vulkan_compute*)dev.context)->set_vulkan_debug_label(dev, VK_OBJECT_TYPE_COMMAND_POOL, uint64_t(thread_cmd_pool->cmd_pool),
																	 "command_pool:" + thread_name);
#endif
		
		// allocate initial command buffers
		// TODO: manage this dynamically (for now: 64 must be enough)
		const VkCommandBufferAllocateInfo cmd_buffer_info {
			.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
			.pNext = nullptr,
			.commandPool = thread_cmd_pool->cmd_pool,
			// always create primary for now
			.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
			.commandBufferCount = vulkan_command_pool_t::cmd_buffer_count,
		};
		VK_CALL_RET(vkAllocateCommandBuffers(dev.device, &cmd_buffer_info, &thread_cmd_pool->cmd_buffers[0]),
					"failed to create command buffers", false)
		thread_cmd_pool->cmd_buffers_in_use.reset();
		
#if defined(FLOOR_DEBUG)
		const auto cmd_buf_prefix = "command_buffer:" + thread_name + ":";
		for (uint32_t cmd_buf_idx = 0; cmd_buf_idx < vulkan_command_pool_t::cmd_buffer_count; ++cmd_buf_idx) {
			((const vulkan_compute*)dev.context)->set_vulkan_debug_label(dev, VK_OBJECT_TYPE_COMMAND_BUFFER, uint64_t(thread_cmd_pool->cmd_buffers[cmd_buf_idx]),
																		 cmd_buf_prefix + to_string(cmd_buf_idx));
		}
#endif
		
		// create fences
		static constexpr const VkFenceCreateInfo fence_info {
			.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
			.pNext = nullptr,
			.flags = 0,
		};
		for (uint32_t i = 0; i < vulkan_command_pool_t::fence_count; ++i) {
			VK_CALL_RET(vkCreateFence(dev.device, &fence_info, nullptr, &thread_cmd_pool->fences[i]),
						"failed to create fence #" + to_string(i), false)
		}
		thread_cmd_pool->fences_in_use.reset();
		
#if defined(FLOOR_DEBUG)
		const auto fence_prefix = "fence:" + thread_name + ":";
		for (uint32_t fence_idx = 0; fence_idx < vulkan_command_pool_t::fence_count; ++fence_idx) {
			((const vulkan_compute*)dev.context)->set_vulkan_debug_label(dev, VK_OBJECT_TYPE_FENCE, uint64_t(thread_cmd_pool->fences[fence_idx]),
																		 fence_prefix + to_string(fence_idx));
		}
#endif
		
		return true;
	}
};

vulkan_queue::vulkan_queue(const compute_device& device_, const VkQueue queue_, const uint32_t family_index_) :
compute_queue(device_), vk_queue(queue_), family_index(family_index_) {
	// create impl
	impl = make_unique<vulkan_queue_impl>(*this, (const vulkan_device&)device_, family_index);
}

vulkan_queue::~vulkan_queue() {
	// TODO: clean up all thread pools/buffers
	finish();
	impl = nullptr;
}

void vulkan_queue::finish() const {
	GUARD(queue_lock);
	VK_CALL_RET(vkQueueWaitIdle(vk_queue), "queue finish failed")
}

void vulkan_queue::flush() const {
	// nop
}

vulkan_command_buffer vulkan_queue::make_command_buffer(const char* name) const {
	impl->create_thread_command_pool();
	return impl->thread_cmd_pool->make_command_buffer(name);
}

void vulkan_queue::submit_command_buffer(const vulkan_command_buffer& cmd_buffer,
										 const bool blocking,
										 const VkSemaphore* wait_semas,
										 const uint32_t wait_sema_count,
										 const VkPipelineStageFlags wait_stage_flags) const {
	submit_command_buffer(cmd_buffer, {}, blocking, wait_semas, wait_sema_count, wait_stage_flags);
}

VkResult vulkan_queue::queue_submit(const VkSubmitInfo& submit_info, VkFence& fence) const {
	GUARD(queue_lock);
	return vkQueueSubmit(vk_queue, 1, &submit_info, fence);
}

void vulkan_queue::submit_command_buffer(const vulkan_command_buffer& cmd_buffer,
										 function<void(const vulkan_command_buffer&)> completion_handler,
										 const bool blocking,
										 const VkSemaphore* wait_semas,
										 const uint32_t wait_sema_count,
										 const VkPipelineStageFlags wait_stage_flags) const {
	impl->create_thread_command_pool();
	impl->thread_cmd_pool->submit_command_buffer(cmd_buffer, completion_handler, blocking, wait_semas, wait_sema_count, wait_stage_flags);
}

void vulkan_queue::add_retained_buffers(const vulkan_command_buffer& cmd_buffer,
										const vector<shared_ptr<compute_buffer>>& buffers) const {
	impl->create_thread_command_pool();
	impl->thread_cmd_pool->add_retained_buffers(cmd_buffer, buffers);
}

void vulkan_queue::add_completion_handler(const vulkan_command_buffer& cmd_buffer,
										  vulkan_completion_handler_t completion_handler) const {
	impl->create_thread_command_pool();
	impl->thread_cmd_pool->add_completion_handler(cmd_buffer, completion_handler);
}

vulkan_command_block vulkan_queue::make_command_block(const char* name, bool& error_signal, const bool is_blocking,
													  const VkSemaphore* wait_semas, const uint32_t wait_sema_count,
													  const VkPipelineStageFlags wait_stage_flags) const {
	return vulkan_command_block { *this, name, error_signal, is_blocking, wait_semas, wait_sema_count, wait_stage_flags };
}

vulkan_command_block::vulkan_command_block(const vulkan_queue& vk_queue_, const char* name, bool& error_signal_, const bool is_blocking_,
										   const VkSemaphore* wait_semas_, const uint32_t wait_sema_count_,
										   const VkPipelineStageFlags wait_stage_flags_) :
vk_queue(vk_queue_), error_signal(error_signal_), is_blocking(is_blocking_),
wait_semas(wait_semas_), wait_sema_count(wait_sema_count_), wait_stage_flags(wait_stage_flags_) {
	// create new command buffer + begin
	cmd_buffer = vk_queue.make_command_buffer(name);
	const VkCommandBufferBeginInfo begin_info {
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
		.pNext = nullptr,
		.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
		.pInheritanceInfo = nullptr,
	};
	VK_CALL_ERR_EXEC(vkBeginCommandBuffer(cmd_buffer.cmd_buffer, &begin_info),
					 "failed to begin command buffer",
					 { error_signal = true; return; })
	
#if defined(FLOOR_DEBUG)
	((const vulkan_compute*)vk_queue.get_device().context)->vulkan_begin_cmd_debug_label(cmd_buffer.cmd_buffer, name);
#endif
	
	// done
	valid = true;
}

vulkan_command_block::~vulkan_command_block() {
	if (!valid || !cmd_buffer.cmd_buffer) {
		error_signal = true;
		return;
	}
	
	VK_CALL_ERR_EXEC(vkEndCommandBuffer(cmd_buffer.cmd_buffer),
					 "failed to end command buffer",
					 { error_signal = true; return; })
	
#if defined(FLOOR_DEBUG)
	((const vulkan_compute*)vk_queue.get_device().context)->vulkan_end_cmd_debug_label(cmd_buffer.cmd_buffer);
#endif
	
	vk_queue.submit_command_buffer(cmd_buffer, is_blocking, wait_semas, wait_sema_count, wait_stage_flags);
}

#endif
