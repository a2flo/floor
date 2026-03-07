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

#include <floor/device/vulkan/vulkan_queue.hpp>

#if !defined(FLOOR_NO_VULKAN)
#include "internal/vulkan_headers.hpp"
#include <floor/device/vulkan/vulkan_device.hpp>
#include <floor/device/vulkan/vulkan_context.hpp>
#include <floor/device/vulkan/vulkan_indirect_command.hpp>
#include <floor/device/vulkan/vulkan_fence.hpp>
#include <floor/device/generic_indirect_command.hpp>
#include <floor/device/utility.hpp>
#include "internal/vulkan_debug.hpp"
#include <floor/core/logger.hpp>
#include <floor/threading/task.hpp>
#include <floor/threading/thread_base.hpp>
#include <floor/threading/thread_helpers.hpp>
#include <floor/floor.hpp>
#include <chrono>

namespace fl {
using namespace std::literals;

//! returns the debug name of the specified buffer or "unknown"
static const char* cmd_buffer_name(const vulkan_command_buffer& cmd_buffer) {
	return (cmd_buffer.name != nullptr ? cmd_buffer.name : "unknown");
}

static VkPipelineStageFlagBits2 sync_stage_to_vulkan_pipeline_stage(const SYNC_STAGE stage) {
	VkPipelineStageFlagBits2 vk_stages { 0u };
	if (has_flag<SYNC_STAGE::VERTEX>(stage)) {
		vk_stages |= VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT;
	}
	if (has_flag<SYNC_STAGE::TESSELLATION>(stage)) {
		vk_stages |= VK_PIPELINE_STAGE_2_TESSELLATION_EVALUATION_SHADER_BIT;
	}
	if (has_flag<SYNC_STAGE::FRAGMENT>(stage)) {
		vk_stages |= VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
	}
	if (has_flag<SYNC_STAGE::COLOR_ATTACHMENT_OUTPUT>(stage)) {
		vk_stages |= VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
	}
	if (has_flag<SYNC_STAGE::BOTTOM_OF_PIPE>(stage)) {
		vk_stages |= VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT;
	}
	if (has_flag<SYNC_STAGE::TOP_OF_PIPE>(stage)) {
		vk_stages |= VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT;
	}
	return vk_stages;
}

//! completes the specified cmd buffer (blocking) + performs all the completion handling and clean up
static void vulkan_complete_cmd_buffer(vulkan_command_pool_t& pool,
									   const vulkan_device& vk_dev,
									   vulkan_command_buffer&& cmd_buffer,
									   VkSemaphore work_sema,
									   const uint64_t work_sema_signal_value,
									   std::function<void(const vulkan_command_buffer&)>&& completion_handler);

struct vulkan_command_buffer_completion_impl;

//! single completion command used in "vulkan_cmd_completion_handler"
struct vulkan_completion_cmd_t {
	vulkan_command_pool_t* pool { nullptr };
	const vulkan_device* vk_dev { nullptr };
	vulkan_command_buffer cmd_buffer {};
	VkSemaphore work_sema { nullptr };
	uint64_t work_sema_signal_value { 0u };
	std::function<void(const vulkan_command_buffer&)> completion_handler;
};

//! the command completion handler/pool implementation
using vulkan_cmd_completion_handler = generic_cmd_completion_handler<vulkan_completion_cmd_t, vulkan_command_buffer_completion_impl>;

//! called to complete (wait for) a single "vulkan_completion_cmd_t"
struct vulkan_command_buffer_completion_impl {
	static void complete(vulkan_cmd_completion_handler::cmd_t& cmd) {
		vulkan_complete_cmd_buffer(*cmd.pool, *cmd.vk_dev, std::move(cmd.cmd_buffer), cmd.work_sema, cmd.work_sema_signal_value,
								   std::move(cmd.completion_handler));
		cmd.cmd_buffer.reset();
	}
};

//! the command completion handler instance
static std::unique_ptr<vulkan_cmd_completion_handler> vk_cmd_completion_handler;

//! per-thread Vulkan command pool and command buffer management
//! NOTE: since Vulkan is *not* thread-safe, we need to manage this on our own
struct vulkan_command_pool_t {
	VkCommandPool cmd_pool { nullptr };
	const vulkan_device& dev;
	const vulkan_queue& queue;
	const bool is_secondary { false };
	const bool no_blocking { false };
	const bool sema_wait_polling { false };
	static inline std::atomic<bool> is_ctx_shutdown { false };
	
	vulkan_command_pool_t(const vulkan_device& dev_, const vulkan_queue& queue_, const bool is_secondary_) :
	dev(dev_), queue(queue_), is_secondary(is_secondary_),
	no_blocking(has_flag<DEVICE_CONTEXT_FLAGS::VULKAN_NO_BLOCKING>(dev.context->get_context_flags())),
	sema_wait_polling(floor::get_vulkan_sema_wait_polling()) {}
	
	~vulkan_command_pool_t() {
		// if the Vulkan context has already been shut down, don't do anything in here
		if (is_ctx_shutdown) {
			return;
		}
		
		// NOTE: this is called via vulkan_command_pool_destructor_t on thread exit
		{
			// if the work sema is still in use, try to wait for (but no longer than 5s)
			const auto last_work_sema_signal_value = work_sema_signal_counter.load();
			uint64_t sema_value = 0u;
			if (vkGetSemaphoreCounterValue(dev.device, work_sema, &sema_value) == VK_SUCCESS &&
				sema_value < last_work_sema_signal_value) {
				log_warn("queue work sema still in use, waiting for completion ...");
				const VkSemaphoreWaitInfo wait_info {
					.sType = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO,
					.pNext = nullptr,
					.flags = 0u,
					.semaphoreCount = 1u,
					.pSemaphores = &work_sema,
					.pValues = &last_work_sema_signal_value,
				};
				(void)vkWaitSemaphores(dev.device, &wait_info, 5'000'000'000ull);
			}
			vkDestroySemaphore(dev.device, work_sema, nullptr);
		}
		if (cmd_pool) {
			vkDestroyCommandPool(dev.device, cmd_pool, nullptr);
		}
	}
	
	struct cmd_buffer_state_t {
		VkCommandBuffer cmd_buffer { nullptr };
		std::vector<vulkan_queue::vulkan_completion_handler_t> completion_handlers;
	};
	resource_slot_container<cmd_buffer_state_t, vulkan_thread_resource_count> cmd_resources;
	
	//! timeline semaphore that is used to synchronize / wait for work completion in this queue
	VkSemaphore work_sema { nullptr };
	std::atomic<uint64_t> work_sema_signal_counter { 0u };
	
	//! acquires an unused command buffer (resets an old unused one)
	vulkan_command_buffer make_command_buffer(const char* name = nullptr) {
		for (uint32_t iter = 0u, attempt = 0u; !is_ctx_shutdown; ++iter, ++attempt) {
			if (auto res = cmd_resources.try_acquire_resource_no_auto_release(); res) [[likely]] {
				VK_CALL_RET(vkResetCommandBuffer(res->cmd_buffer, VK_COMMAND_BUFFER_RESET_RELEASE_RESOURCES_BIT),
							"failed to reset command buffer (for "s + (name != nullptr ? name : "unknown") + ")", {})
				res->completion_handlers.clear();
				
				vulkan_command_buffer ret {};
				ret.cmd_buffer = res->cmd_buffer;
				ret.index = res.index();
				ret.name = name;
				ret.is_secondary = is_secondary;
				return ret;
			}
			
			spin_wait_or_yield(attempt);
			
			if (iter > 0 && (iter % 1'000'000u) == 0u) {
				log_warn("long wait: all command resources are currently in use");
			}
		}
		return {};
	}
	
	//! release an unsubmitted command buffer
	void release_unsubmitted_command_buffer(vulkan_command_buffer&& cmd_buffer) {
		// we still need to call all registered completion handlers
		auto& cmd = cmd_resources.resources[cmd_buffer.index];
		std::vector<vulkan_queue::vulkan_completion_handler_t> completion_handlers;
		completion_handlers.swap(cmd.completion_handlers);
		for (const auto& compl_handler : completion_handlers) {
			if (compl_handler) {
				compl_handler();
			}
		}
		
		cmd_resources.release_resource(cmd_buffer.index);
	}
	
	//! for internal use only: release a secondary command buffer again specified by the given resource index
	void release_secondary_command_buffer(const uint8_t sec_cmd_buffer_idx) {
		assert(is_secondary);
		cmd_resources.release_resource(sec_cmd_buffer_idx);
	}
	
	//! submits a command buffer to the device queue
	void submit_command_buffer(vulkan_command_buffer&& cmd_buffer,
							   std::function<void(const vulkan_command_buffer&)>&& completion_handler,
							   const bool blocking,
							   std::vector<vulkan_queue::wait_fence_t>&& wait_fences,
							   std::vector<vulkan_queue::signal_fence_t>&& signal_fences) {
		std::vector<VkSemaphoreSubmitInfo> wait_sema_info;
		const auto wait_fences_count = uint32_t(wait_fences.size());
		if (wait_fences_count > 0) {
			wait_sema_info.reserve(wait_fences_count);
			for (const auto& wait_fence : wait_fences) {
				wait_sema_info.emplace_back(VkSemaphoreSubmitInfo {
					.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
					.pNext = nullptr,
					.semaphore = ((const vulkan_fence*)wait_fence.fence)->get_vulkan_fence(),
					.value = wait_fence.signaled_value,
					.stageMask = sync_stage_to_vulkan_pipeline_stage(wait_fence.stage),
					.deviceIndex = 0,
				});
			}
		}
		
		// get next work sema signal value in line + always add work sema
		const auto work_sema_signal_value = ++work_sema_signal_counter;
		std::vector<VkSemaphoreSubmitInfo> signal_sema_info {{
			.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
			.pNext = nullptr,
			.semaphore = work_sema,
			.value = work_sema_signal_value,
			.stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
			.deviceIndex = 0,
		}};
		
		if (!signal_fences.empty()) {
			signal_sema_info.reserve(signal_fences.size() + 1u);
			for (const auto& signal_fence : signal_fences) {
				signal_sema_info.emplace_back(VkSemaphoreSubmitInfo {
					.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
					.pNext = nullptr,
					.semaphore = ((const vulkan_fence*)signal_fence.fence)->get_vulkan_fence(),
					.value = signal_fence.signaled_value,
					.stageMask = sync_stage_to_vulkan_pipeline_stage(signal_fence.stage),
					.deviceIndex = 0,
				});
			}
		}
		
		const VkCommandBufferSubmitInfo cmd_buf_info {
			.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
			.pNext = nullptr,
			.commandBuffer = cmd_buffer.cmd_buffer,
			.deviceMask = 0,
		};
		
		const VkSubmitInfo2 submit_info {
			.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
			.pNext = nullptr,
			.flags = 0,
			.waitSemaphoreInfoCount = wait_fences_count,
			.pWaitSemaphoreInfos = (wait_fences_count > 0 ? &wait_sema_info[0] : nullptr),
			.commandBufferInfoCount = 1,
			.pCommandBufferInfos = &cmd_buf_info,
			.signalSemaphoreInfoCount = uint32_t(signal_sema_info.size()),
			.pSignalSemaphoreInfos = &signal_sema_info[0],
		};
		VkResult submit_err { VK_SUCCESS };
		{
			GUARD(queue.queue_lock);
			submit_err = vkQueueSubmit2(queue.vk_queue, 1, &submit_info, nullptr);
		}
		if (submit_err != VK_SUCCESS) {
			log_error("failed to submit queue ($): $: $",
					  cmd_buffer_name(cmd_buffer), submit_err, vulkan_error_to_string(submit_err));
			// still continue here to free the cmd buffer
		}
		
		// if blocking: wait until completion in here (in this thread),
		// otherwise offload to a completion handler thread
		if (blocking || !no_blocking) {
			vulkan_complete_cmd_buffer(*this, dev, std::move(cmd_buffer), work_sema, work_sema_signal_value, std::move(completion_handler));
		} else {
			// -> offload
			vulkan_cmd_completion_handler::cmd_t cmd {
				.pool = this,
				.vk_dev = &dev,
				.cmd_buffer = std::move(cmd_buffer),
				.work_sema = work_sema,
				.work_sema_signal_value = work_sema_signal_value,
				.completion_handler = std::move(completion_handler),
			};
			vk_cmd_completion_handler->add_cmd_completion(std::move(cmd));
		}
	}
	
	//! adds a completion handler to the specified command buffer (called once the command buffer has completed execution, successfully or not)
	void add_completion_handler(const vulkan_command_buffer& cmd_buffer,
								vulkan_queue::vulkan_completion_handler_t&& completion_handler) {
		// NOTE: thread-safe, b/c this is only called from a single (the owning/originating) thread
		cmd_resources.resources[cmd_buffer.index].completion_handlers.emplace_back(std::move(completion_handler));
	}
};

void vulkan_complete_cmd_buffer(vulkan_command_pool_t& pool,
								const vulkan_device& vk_dev,
								vulkan_command_buffer&& cmd_buffer,
								VkSemaphore work_sema,
								const uint64_t work_sema_signal_value,
								std::function<void(const vulkan_command_buffer&)>&& completion_handler) {
	// TODO/NOTE: at this point, I am not sure what the better/faster approach is (one would think vkWaitSemaphores, but apparently not)
	// -> Linux: polling seems to be a lot faster, with vkWaitSemaphores sometimes having multi-millisecond delays
	// -> Windows: not much of a difference between these, with the polling being slightly faster
	if (!pool.sema_wait_polling) {
		// -> wait on sema until completion
		const VkSemaphoreWaitInfo wait_info {
			.sType = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO,
			.pNext = nullptr,
			.flags = 0u,
			.semaphoreCount = 1u,
			.pSemaphores = &work_sema,
			.pValues = &work_sema_signal_value,
		};
		const auto wait_ret = vkWaitSemaphores(vk_dev.device, &wait_info, ~0ull);
		if (wait_ret != VK_SUCCESS) {
			if (wait_ret == VK_TIMEOUT) {
				log_error("waiting for work sema timed out");
			} else if (wait_ret == VK_ERROR_DEVICE_LOST) {
				log_error("device lost during command buffer execution/wait (probably program error)$!",
						  cmd_buffer.name != nullptr ? ": "s + cmd_buffer.name : ""s);
				logger::flush();
				throw std::runtime_error("Vulkan device lost");
			} else {
				log_error("waiting for work sema failed: $ ($)", vulkan_error_to_string(wait_ret), wait_ret);
			}
		}
	} else {
		// -> poll work sema status until completion
		for (;;) {
			uint64_t sema_value = 0u;
			const auto status = vkGetSemaphoreCounterValue(vk_dev.device, work_sema, &sema_value);
			if (status == VK_SUCCESS) {
				if (sema_value >= work_sema_signal_value) {
					break;
				}
				// else: continue waiting
			} else if (status == VK_ERROR_DEVICE_LOST) {
				log_error("device lost during command buffer execution/wait (probably program error)$!",
						  cmd_buffer.name != nullptr ? ": "s + cmd_buffer.name : ""s);
				logger::flush();
				throw std::runtime_error("Vulkan device lost");
			} else if (status != VK_NOT_READY) {
				log_error("waiting for work sema failed: $ ($)", vulkan_error_to_string(status), status);
			}
		}
	}
	
	// call user-specified handler
	if (completion_handler) {
		completion_handler(cmd_buffer);
	}
	
	auto& cmd = pool.cmd_resources.resources[cmd_buffer.index];
	
	// call internal completion handlers and free retained buffers
	std::vector<vulkan_queue::vulkan_completion_handler_t> completion_handlers;
	completion_handlers.swap(cmd.completion_handlers);
	for (const auto& compl_handler : completion_handlers) {
		if (compl_handler) {
			compl_handler();
		}
	}
	
	// mark resource + internals as free again
	pool.cmd_resources.release_resource(cmd_buffer.index);
}

//! stores all Vulkan command pool instances
struct vulkan_command_pool_storage {
	//! access to "cmd_pools" must be thread-safe
	static inline safe_mutex cmd_pools_lock;
	//! contains all currently active/allocated command pools
	static inline fl::flat_map<vulkan_command_pool_t*, std::unique_ptr<vulkan_command_pool_t>> cmd_pools GUARDED_BY(cmd_pools_lock);
	
	//! creates a new command pool, returning a *non-owning* reference to it
	static vulkan_command_pool_t* create_cmd_pool(const vulkan_device& dev, const vulkan_queue& queue, const bool is_secondary) {
		auto cmd_pool = std::make_unique<vulkan_command_pool_t>(dev, queue, is_secondary);
		auto cmd_pool_ret = cmd_pool.get();
		{
			GUARD(cmd_pools_lock);
			vulkan_command_pool_t* cmd_pool_ptr = cmd_pool.get();
			cmd_pools.emplace(std::move(cmd_pool_ptr), std::move(cmd_pool));
		}
		return cmd_pool_ret;
	}
	
	//! destroys the specified command pool, returns true on success
	static bool destroy_cmd_pool(vulkan_command_pool_t* cmd_pool) {
		GUARD(cmd_pools_lock);
		if (auto cmd_pool_iter = cmd_pools.find(cmd_pool); cmd_pool_iter != cmd_pools.end()) {
			cmd_pools.erase(cmd_pool_iter);
			return true;
		}
		return false;
	}
};

//! since command pools are created per-thread and we don't necessarily have a clean direct way of destructing command pool resources,
//! we do this via a static thread-local RAII class instead, with the destructor in it being called once the thread exits
static thread_local struct vulkan_command_pool_destructor_t {
	vulkan_command_pool_destructor_t() = default;
	~vulkan_command_pool_destructor_t() {
		if (primary_pool) {
			vulkan_command_pool_storage::destroy_cmd_pool(primary_pool);
		}
		if (secondary_pool) {
			vulkan_command_pool_storage::destroy_cmd_pool(secondary_pool);
		}
	}
	
	vulkan_command_pool_t* primary_pool { nullptr };
	vulkan_command_pool_t* secondary_pool { nullptr };
} vulkan_command_pool_destructor {};

//! internal Vulkan device queue implementation
struct vulkan_queue_impl {
	const vulkan_device& dev;
	const vulkan_queue& queue;
	const uint32_t family_index;
	//! per-thread/thread-local Vulkan command pool/buffers
	static inline thread_local vulkan_command_pool_t* thread_primary_cmd_pool { nullptr };
	//! per-thread/thread-local Vulkan secondary command pool/buffers
	static inline thread_local vulkan_command_pool_t* thread_secondary_cmd_pool { nullptr };
	
	vulkan_queue_impl(const vulkan_queue& queue_, const vulkan_device& dev_, const uint32_t& family_index_) :
	dev(dev_), queue(queue_), family_index(family_index_) {}
	
	//! creates and initializes the per-thread/thread-local primary command pool/buffers
	bool create_thread_primary_command_pool() {
		return create_thread_command_pool(false);
	}
	
	//! creates and initializes the per-thread/thread-local secondary command pool/buffers
	bool create_thread_secondary_command_pool() {
		return create_thread_command_pool(true);
	}
	
	bool create_thread_command_pool(const bool is_secondary) {
		auto& cmd_pool = (!is_secondary ? thread_primary_cmd_pool : thread_secondary_cmd_pool);
		if (cmd_pool) {
			return true;
		}
		cmd_pool = vulkan_command_pool_storage::create_cmd_pool(dev, queue, is_secondary);
		
		// register in per-thread destructor
		if (!is_secondary) {
			vulkan_command_pool_destructor.primary_pool = cmd_pool;
		} else {
			vulkan_command_pool_destructor.secondary_pool = cmd_pool;
		}
		
		// create command pool for this queue + device
		const VkCommandPoolCreateInfo cmd_pool_info {
			.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
			.pNext = nullptr,
			// always short-lived + need individual reset
			.flags = (VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT),
			.queueFamilyIndex = family_index,
		};
		VK_CALL_RET(vkCreateCommandPool(dev.device, &cmd_pool_info, nullptr, &cmd_pool->cmd_pool),
					"failed to create command pool", false)
		
#if defined(FLOOR_DEBUG)
		auto thread_name = get_current_thread_name();
		if (thread_name.empty()) {
			std::stringstream sstr;
			sstr << std::this_thread::get_id();
			thread_name = sstr.str();
		}
		set_vulkan_debug_label(dev, VK_OBJECT_TYPE_COMMAND_POOL, uint64_t(cmd_pool->cmd_pool), "command_pool:" + thread_name);
#endif
		
		// allocate initial command buffers
		const VkCommandBufferAllocateInfo cmd_buffer_info {
			.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
			.pNext = nullptr,
			.commandPool = cmd_pool->cmd_pool,
			.level = (!is_secondary ? VK_COMMAND_BUFFER_LEVEL_PRIMARY : VK_COMMAND_BUFFER_LEVEL_SECONDARY),
			.commandBufferCount = vulkan_thread_resource_count,
		};
		VkCommandBuffer cmd_buffers[vulkan_thread_resource_count];
		VK_CALL_RET(vkAllocateCommandBuffers(dev.device, &cmd_buffer_info, cmd_buffers),
					"failed to create command buffers", false)
		for (uint32_t res_idx = 0; res_idx < vulkan_thread_resource_count; ++res_idx) {
			cmd_pool->cmd_resources.resources[res_idx].cmd_buffer = cmd_buffers[res_idx];
		}
		
#if defined(FLOOR_DEBUG)
		const auto cmd_buf_prefix = (is_secondary ? "sec_"s : ""s) + "command_buffer:" + thread_name + ":";
		for (uint32_t res_idx = 0; res_idx < vulkan_thread_resource_count; ++res_idx) {
			auto& cmd = cmd_pool->cmd_resources.resources[res_idx];
			set_vulkan_debug_label(dev, VK_OBJECT_TYPE_COMMAND_BUFFER, uint64_t(cmd.cmd_buffer),
								   cmd_buf_prefix + std::to_string(res_idx));
		}
#endif
		
		// create work semaphore
		const VkSemaphoreTypeCreateInfo sema_type_create_info {
			.sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO,
			.pNext = nullptr,
			.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE,
			.initialValue = 0u,
		};
		const VkSemaphoreCreateInfo sema_create_info {
			.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
			.pNext = &sema_type_create_info,
			.flags = 0,
		};
		VK_CALL_RET(vkCreateSemaphore(dev.device, &sema_create_info, nullptr, &cmd_pool->work_sema),
					"failed to create queue work semaphore", false)
#if defined(FLOOR_DEBUG)
		const auto sema_prefix = (is_secondary ? "sec_"s : ""s) + "sema:" + thread_name;
		set_vulkan_debug_label(dev, VK_OBJECT_TYPE_SEMAPHORE, uint64_t(cmd_pool->work_sema), sema_prefix);
#endif
		
		return true;
	}
	
	vulkan_command_pool_t& get_thread_command_pool(const bool is_secondary) {
		return (!is_secondary ? *thread_primary_cmd_pool : *thread_secondary_cmd_pool);
	}
};

static bool did_init_vulkan_queue { false };
void vulkan_queue::init() {
	if (!did_init_vulkan_queue) {
		did_init_vulkan_queue = true;
		vk_cmd_completion_handler = std::make_unique<vulkan_cmd_completion_handler>();
	}
}
void vulkan_queue::destroy() {
	if (did_init_vulkan_queue) {
		vk_cmd_completion_handler = nullptr;
		vulkan_command_pool_t::is_ctx_shutdown = true;
	}
}

vulkan_queue::vulkan_queue(const device& dev_, const VkQueue queue_, const uint32_t family_index_,
						   const uint32_t queue_index_, const QUEUE_TYPE queue_type_) :
device_queue(dev_, queue_type_), vk_queue(queue_), family_index(family_index_), queue_index(queue_index_) {
	// create impl
	impl = std::make_unique<vulkan_queue_impl>(*this, (const vulkan_device&)dev_, family_index);
}

vulkan_queue::~vulkan_queue() {
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

void vulkan_queue::execute_indirect(const indirect_command_pipeline& indirect_cmd,
									const indirect_execution_parameters_t& params,
									kernel_completion_handler_f&& completion_handler) const {
	const auto command_count = indirect_cmd.get_command_count();
	if (command_count == 0) {
		return;
	}
	
#if defined(FLOOR_DEBUG)
	if (indirect_cmd.get_description().command_type != indirect_command_description::COMMAND_TYPE::COMPUTE) {
		log_error("specified indirect command pipeline \"$\" must be a compute pipeline",
				  indirect_cmd.get_description().debug_label);
		return;
	}
#endif
	
	if (const auto generic_ind_pipeline = dynamic_cast<const generic_indirect_command_pipeline*>(&indirect_cmd); generic_ind_pipeline) {
		device_queue::execute_indirect(indirect_cmd, params, std::move(completion_handler));
		return;
	}
	
	const auto& vk_indirect_cmd = (const vulkan_indirect_command_pipeline&)indirect_cmd;
	const auto vk_indirect_pipeline_entry = vk_indirect_cmd.get_vulkan_pipeline_entry(dev);
	if (!vk_indirect_pipeline_entry) {
		log_error("no indirect command pipeline state for device \"$\" in indirect command pipeline \"$\"",
				  dev.name, indirect_cmd.get_description().debug_label);
		return;
	}
	
	// create and setup the compute encoder (primary command buffer)
	const auto encoder_label = params.debug_label ? params.debug_label : "indirect_encoder";
	auto cmd_buffer = make_command_buffer(encoder_label);
	if (!cmd_buffer) {
		return;
	}
	const VkCommandBufferBeginInfo begin_info {
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
		.pNext = nullptr,
		.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
		.pInheritanceInfo = nullptr,
	};
	VK_CALL_RET(vkBeginCommandBuffer(cmd_buffer.cmd_buffer, &begin_info),
				"failed to begin command buffer")
	
#if defined(FLOOR_DEBUG)
	vulkan_insert_cmd_debug_label(cmd_buffer.cmd_buffer, encoder_label);
#endif
	
	if (vk_indirect_pipeline_entry->printf_buffer) {
		vk_indirect_pipeline_entry->printf_init(*this);
	}
	
	const auto queue_data_index = (queue_type == QUEUE_TYPE::ALL ? 0u : 1u);
	vkCmdExecuteCommands(cmd_buffer.cmd_buffer, command_count,
						 &vk_indirect_pipeline_entry->per_queue_data[queue_data_index].cmd_buffers[0]);
	
	// all done here, end + submit
	VK_CALL_RET(vkEndCommandBuffer(cmd_buffer.cmd_buffer), "failed to end command buffer")
	
	if (vk_indirect_pipeline_entry->printf_buffer) {
		vk_indirect_pipeline_entry->printf_completion(*this, cmd_buffer);
	}
	
	// add completion handler if required
	if (completion_handler) {
		add_completion_handler(cmd_buffer, [handler = std::move(completion_handler)]() {
			handler();
		});
	}
	
	auto wait_fences = encode_wait_fences(params.wait_fences);
	auto signal_fences = encode_signal_fences(params.signal_fences);
	submit_command_buffer(std::move(cmd_buffer), std::move(wait_fences), std::move(signal_fences), {},
						  params.wait_until_completion ||
						  vk_indirect_pipeline_entry->printf_buffer /* must block when soft-print is used */);
}

std::vector<vulkan_queue::wait_fence_t> vulkan_queue::encode_wait_fences(const std::vector<const device_fence*>& wait_fences) {
	std::vector<wait_fence_t> vk_wait_fences;
	for (const auto& fence : wait_fences) {
		if (!fence) {
			continue;
		}
		const auto& vk_fence = (const vulkan_fence&)*fence;
		vk_wait_fences.emplace_back(vulkan_queue::wait_fence_t {
			.fence = fence,
			.signaled_value = vk_fence.get_signaled_value(),
			.stage = SYNC_STAGE::NONE,
		});
	}
	return vk_wait_fences;
}

std::vector<vulkan_queue::signal_fence_t> vulkan_queue::encode_signal_fences(const std::vector<device_fence*>& signal_fences) {
	std::vector<signal_fence_t> vk_signal_fences;
	for (auto& fence : signal_fences) {
		if (!fence) {
			continue;
		}
		auto& vk_fence = (vulkan_fence&)*fence;
		if (!vk_fence.next_signal_value()) {
			throw std::runtime_error("failed to set next signal value on fence");
		}
		vk_signal_fences.emplace_back(vulkan_queue::signal_fence_t {
			.fence = fence,
			.unsignaled_value = vk_fence.get_unsignaled_value(),
			.signaled_value = vk_fence.get_signaled_value(),
			.stage = SYNC_STAGE::NONE,
		});
	}
	return vk_signal_fences;
}

vulkan_command_buffer vulkan_queue::make_command_buffer(const char* name) const {
	impl->create_thread_primary_command_pool();
	return impl->thread_primary_cmd_pool->make_command_buffer(name);
}

vulkan_command_buffer vulkan_queue::make_secondary_command_buffer(const char* name) const {
	impl->create_thread_secondary_command_pool();
	return impl->thread_secondary_cmd_pool->make_command_buffer(name);
}

void vulkan_queue::free_command_buffer(vulkan_command_buffer&& cmd_buffer) const {
	if (!cmd_buffer) {
		return; // shouldn't be here in the first place
	}
	
	if (!cmd_buffer.is_secondary) {
		impl->create_thread_primary_command_pool();
		impl->thread_primary_cmd_pool->release_unsubmitted_command_buffer(std::move(cmd_buffer));
	} else {
		impl->create_thread_secondary_command_pool();
		impl->thread_secondary_cmd_pool->release_unsubmitted_command_buffer(std::move(cmd_buffer));
	}
}

void vulkan_queue::submit_command_buffer(vulkan_command_buffer&& cmd_buffer,
										 std::vector<wait_fence_t>&& wait_fences,
										 std::vector<signal_fence_t>&& signal_fences,
										 std::function<void(const vulkan_command_buffer&)>&& completion_handler,
										 const bool blocking) const {
	impl->create_thread_command_pool(cmd_buffer.is_secondary);
	vulkan_command_pool_t& pool = impl->get_thread_command_pool(cmd_buffer.is_secondary);
	pool.submit_command_buffer(std::move(cmd_buffer), std::move(completion_handler),
							   blocking, std::move(wait_fences),
							   std::move(signal_fences));
}

bool vulkan_queue::execute_secondary_command_buffer(const vulkan_command_buffer& primary_cmd_buffer,
													vulkan_command_buffer&& secondary_cmd_buffer) const {
	if (primary_cmd_buffer.is_secondary) {
		log_error("specified primary cmd buffer is not actually a primary cmd buffer!");
		return false;
	}
	if (!secondary_cmd_buffer.is_secondary) {
		log_error("specified secondary cmd buffer is not actually a secondary cmd buffer!");
		return false;
	}
	assert(primary_cmd_buffer);
	assert(secondary_cmd_buffer);
	
	vkCmdExecuteCommands(primary_cmd_buffer.cmd_buffer, 1, &secondary_cmd_buffer.cmd_buffer);
	
	// we need to hold onto the secondary cmd buffer until the primary cmd buffer has completed
	add_completion_handler(primary_cmd_buffer, [this, sec_cmd_buffer_idx = secondary_cmd_buffer.index]() {
		impl->thread_secondary_cmd_pool->release_secondary_command_buffer(sec_cmd_buffer_idx);
	});
	secondary_cmd_buffer.reset();
	
	return true;
}

void vulkan_queue::add_completion_handler(const vulkan_command_buffer& cmd_buffer,
										  vulkan_completion_handler_t&& completion_handler) const {
	impl->create_thread_command_pool(cmd_buffer.is_secondary);
	impl->get_thread_command_pool(cmd_buffer.is_secondary).add_completion_handler(cmd_buffer, std::move(completion_handler));
}

void vulkan_queue::set_debug_label(const std::string& label) {
	device_queue::set_debug_label(label);
	
	GUARD(queue_lock);
	if (vk_queue) {
		set_vulkan_debug_label((const vulkan_device&)dev, VK_OBJECT_TYPE_QUEUE, uint64_t(vk_queue), label);
	}
}

vulkan_command_block vulkan_queue::make_command_block(const char* name, bool& error_signal, const bool is_blocking,
													  std::vector<wait_fence_t>&& wait_fences,
													  std::vector<signal_fence_t>&& signal_fences) const {
	return vulkan_command_block { *this, name, error_signal, is_blocking, std::move(wait_fences), std::move(signal_fences) };
}

vulkan_command_block::vulkan_command_block(const vulkan_queue& vk_queue_, const char* name, bool& error_signal_, const bool is_blocking_,
										   std::vector<vulkan_queue::wait_fence_t>&& wait_fences_, std::vector<vulkan_queue::signal_fence_t>&& signal_fences_) :
vk_queue(vk_queue_), error_signal(error_signal_), is_blocking(is_blocking_),
wait_fences(std::move(wait_fences_)), signal_fences(std::move(signal_fences_)) {
	// create new command buffer + begin
	cmd_buffer = vk_queue.make_command_buffer(name);
	if (!cmd_buffer) {
		// this can only fail on ctx shutdown, so just return here + signal failure
		error_signal_ = true;
		return;
	}
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
	vulkan_begin_cmd_debug_label(cmd_buffer.cmd_buffer, name);
#endif
	
	// done
	valid = true;
}

vulkan_command_block::~vulkan_command_block() {
	if (!valid || !cmd_buffer.cmd_buffer) {
		error_signal = true;
		return;
	}
	
#if defined(FLOOR_DEBUG)
	vulkan_end_cmd_debug_label(cmd_buffer.cmd_buffer);
#endif
	
	VK_CALL_ERR_EXEC(vkEndCommandBuffer(cmd_buffer.cmd_buffer),
					 "failed to end command buffer",
					 { error_signal = true; return; })
	
	vk_queue.submit_command_buffer(std::move(cmd_buffer), std::move(wait_fences), std::move(signal_fences), {}, is_blocking);
}

} // namespace fl

#endif
