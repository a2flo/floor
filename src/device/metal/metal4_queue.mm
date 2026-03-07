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

#include <floor/device/metal/metal4_queue.hpp>

#if !defined(FLOOR_NO_METAL)

#include <floor/darwin/darwin_helper.hpp>
#include <floor/device/device_context.hpp>
#include <floor/device/soft_printf.hpp>
#include <floor/device/generic_indirect_command.hpp>
#include <floor/device/utility.hpp>
#include <floor/device/metal/metal4_indirect_command.hpp>
#include <floor/device/metal/metal4_soft_indirect_command.hpp>
#include <floor/device/metal/metal_fence.hpp>
#include <floor/device/metal/metal_device.hpp>
#include <floor/threading/task.hpp>
#include <floor/threading/thread_base.hpp>
#include <floor/threading/thread_helpers.hpp>
#include <floor/core/core.hpp>
#include <floor/floor.hpp>
#include <chrono>

namespace fl {
using namespace std::literals;

//! completes the specified cmd buffer (blocking) + performs all the completion handling and clean up
static void metal4_complete_cmd_buffer(metal4_command_pool& pool,
									   metal4_command_buffer&& cmd_buffer,
									   metal4_queue::command_buffer_completion_handler_f&& completion_handler);

struct metal4_command_buffer_completion_impl;

//! single completion command used in "metal4_cmd_completion_handler"
struct metal4_completion_cmd_t {
	metal4_command_pool* pool { nullptr };
	const metal_device* mtl_dev { nullptr };
	metal4_command_buffer cmd_buffer {};
	metal4_queue::command_buffer_completion_handler_f completion_handler;
};

//! the command completion handler/pool implementation
using metal4_cmd_completion_handler = generic_cmd_completion_handler<metal4_completion_cmd_t, metal4_command_buffer_completion_impl>;

//! called to complete (wait for) a single "metal4_completion_cmd_t"
struct metal4_command_buffer_completion_impl {
	static void complete(metal4_cmd_completion_handler::cmd_t& cmd) {
		metal4_complete_cmd_buffer(*cmd.pool, std::move(cmd.cmd_buffer), std::move(cmd.completion_handler));
		cmd.cmd_buffer.reset();
	}
};

//! the command completion handler instance
static std::unique_ptr<metal4_cmd_completion_handler> mtl_cmd_completion_handler;

//! per-thread Metal command pool and command buffer management
//! NOTE: since command buffers, allocators and residency sets in Metal4 are *not* thread-safe, we need to manage this on our own
struct metal4_command_pool {
	const metal_device& dev;
	const bool no_blocking { false };
	static inline std::atomic<bool> is_ctx_shutdown { false };
	
	metal4_command_pool(const metal_device& dev_) :
	dev(dev_), no_blocking(!has_flag<DEVICE_CONTEXT_FLAGS::METAL_BLOCKING>(dev.context->get_context_flags())) {}
	
	~metal4_command_pool() {
		// if the Metal context has already been shut down, don't do anything in here
		if (is_ctx_shutdown) {
			return;
		}
		
		// NOTE: this is called via metal4_command_pool_destructor_t on thread exit
		@autoreleasepool {
			// if this is still in use, try to wait for it (but no longer than 5s)
			const auto start_time = core::unix_timestamp_ms();
			bool abort_wait = false;
			for (auto& res : cmd_resources.resources) {
				while (!res.completed) {
					std::this_thread::sleep_for(250ms);
					if (is_ctx_shutdown || core::unix_timestamp_ms() - start_time >= 5'000) {
						abort_wait = true;
						break;
					}
				}
				if (abort_wait) {
					break;
				}
			}
			
			for (auto& res : cmd_resources.resources) {
				res.allocator = nil;
				res.cmd_buffer = nil;
				res.residency_set = nil;
				res.completion_cb = nil;
			}
		}
	}
	
	struct cmd_buffer_state_t {
		id <MTL4CommandAllocator> allocator { nil };
		id <MTL4CommandBuffer> cmd_buffer { nil };
		id <MTLResidencySet> residency_set { nil };
		MTL4CommitOptions* commit_op { nil };
		
		//! associated queue for this command buffer submission
		const metal4_queue* queue { nullptr };
		
		//! used between the Metal feedback handler and our completion handler,
		//! both as the predicate for "completed_cv", and to indicate any failure/non-completion on the Metal side
		//! NOTE: the Metal feedback handler and command submission mechanisms are rather rudimentary,
		//!       so we can't properly handle ABA situations or abort commands here
		std::atomic<bool> completed { true };
		std::mutex completed_cv_lock;
		//! waited on by our completion handler, notified from the Metal feedback handler
		std::condition_variable completed_cv;
		MTL4CommitFeedbackHandler completion_cb { nil };
		
		//! used by metal4_queue finish() to identify which submission to wait for
		uint64_t submission_number { 0u };
		
		std::vector<metal4_queue::metal4_completion_handler_f> completion_handlers;
		
		//! set in Metal feedback handler if there was any error
		std::string error_str;
		bool is_error { false };
		
		//! set during submission so that GPU execution times can be accumulated
		std::atomic<uint64_t>* profiling_sum { nullptr };
		bool is_profiling { false };
	};
	resource_slot_container<cmd_buffer_state_t, metal_thread_resource_count> cmd_resources;
	
	//! acquires an unused command buffer (resets an old unused one)
	metal4_command_buffer make_command_buffer(const char* name = nullptr) {
		@autoreleasepool {
			for (uint32_t iter = 0u, attempt = 0u; !is_ctx_shutdown; ++iter, ++attempt) {
				if (auto res = cmd_resources.try_acquire_resource_no_auto_release(); res) [[likely]] {
					assert(res->completed);
					[res->allocator reset];
					[res->residency_set removeAllAllocations];
					[res->residency_set commit];
					res->completion_handlers.clear();
					
#if FLOOR_METAL_DEBUG_RS
					dev.commit_debug_residency_set();
#endif
					metal4_command_buffer ret {};
					ret.allocator = res->allocator;
					ret.cmd_buffer = res->cmd_buffer;
					ret.residency_set = res->residency_set;
					ret.index = res.index();
					ret.name = name;
					return ret;
				}
				
				spin_wait_or_yield(attempt);
				
				if (iter > 0 && (iter % 1'000'000u) == 0u) {
					log_warn("long wait: all command resources are currently in use");
				}
			}
			return {};
		}
	}
	
	//! release an unsubmitted command buffer
	void release_unsubmitted_command_buffer(metal4_command_buffer&& cmd_buffer) {
		// we still need to call all registered completion handlers
		auto& cmd = cmd_resources.resources[cmd_buffer.index];
		std::vector<metal4_queue::metal4_completion_handler_f> completion_handlers;
		completion_handlers.swap(cmd.completion_handlers);
		for (const auto& compl_handler : completion_handlers) {
			if (compl_handler) {
				compl_handler();
			}
		}
		
		cmd_resources.release_resource(cmd_buffer.index);
	}
	
	//! submits a command buffer to the device queue
	void submit_command_buffer(metal4_command_buffer&& cmd_buffer,
							   const metal4_queue& queue,
							   metal4_queue::command_buffer_completion_handler_f&& completion_handler,
							   const bool blocking) {
		@autoreleasepool {
#if FLOOR_METAL_DEBUG_RS
			dev.commit_debug_residency_set();
#endif
			
			// NOTE: completion state and commit op are intentionally not lock-guarded, since we know they can only used by a single thread
			auto& cmd = cmd_resources.resources[cmd_buffer.index];
			cmd.queue = &queue;
			cmd.completed = false;
			cmd.is_error = false;
			cmd.error_str.clear();
			cmd.is_profiling = queue.is_profiling;
			cmd.profiling_sum = &queue.profiling_sum;
			// NOTE: needs to be set every time
			[cmd.commit_op addFeedbackHandler:cmd.completion_cb];
			{
				// get + enqueue submission_number
				for (uint32_t iter = 0u, attempt = 0u; ; ++iter, ++attempt) {
					if (is_ctx_shutdown) [[unlikely]] {
						return;
					}
					{
						GUARD(queue.queue_lock);
						if (queue.pending_submissions.writable_elements() > 0) [[likely]] {
							// must only increase the submission_counter if we can actually enqueue, otherwise order is not guaranteed
							cmd.submission_number = queue.submission_counter++;
							[[maybe_unused]] const auto did_enqueue = queue.pending_submissions.enqueue(cmd.submission_number);
							assert(did_enqueue);
							break;
						}
					}
					spin_wait_or_yield(attempt);
					if (iter > 0 && (iter % 100u) == 0u) {
						log_warn("long wait for submission");
					}
				}
			}
			[queue.queue commit:&cmd_buffer.cmd_buffer count:1 options:cmd.commit_op];
			
			// if blocking: wait until completion in here (in this thread),
			// otherwise offload to a completion handler thread
			if (blocking || !no_blocking) {
				metal4_complete_cmd_buffer(*this, std::move(cmd_buffer), std::move(completion_handler));
			} else {
				// -> offload
				metal4_cmd_completion_handler::cmd_t cmd_compl {
					.pool = this,
					.mtl_dev = &dev,
					.cmd_buffer = std::move(cmd_buffer),
					.completion_handler = std::move(completion_handler),
				};
				mtl_cmd_completion_handler->add_cmd_completion(std::move(cmd_compl));
			}
		}
	}
	
	//! adds a completion handler to the specified command buffer (called once the command buffer has completed execution, successfully or not)
	void add_completion_handler(const metal4_command_buffer& cmd_buffer,
								metal4_queue::metal4_completion_handler_f&& completion_handler) {
		// NOTE: thread-safe, b/c this is only called from a single (the owning/originating) thread
		cmd_resources.resources[cmd_buffer.index].completion_handlers.emplace_back(std::move(completion_handler));
	}
};

//! per-thread/thread-local Metal command pool/resources
static inline thread_local metal4_command_pool* metal4_thread_command_pool { nullptr };

//! creates and initializes the per-thread/thread-local primary command pool if it doesn't exist yet,
//! otherwise returns the existing one, returns nullptr on failure
static metal4_command_pool* create_or_get_thread_command_pool(const metal_device& dev);

void metal4_complete_cmd_buffer(metal4_command_pool& pool,
								metal4_command_buffer&& cmd_buffer,
								metal4_queue::command_buffer_completion_handler_f&& completion_handler) {
	assert(cmd_buffer.index < metal_thread_resource_count);
	auto& cmd = pool.cmd_resources.resources[cmd_buffer.index];
	
	// NOTE: while I would prefer to simply wait for the CV here, we may run into a race-ish condition, where the command gets completed
	//       and the CV gets notified, while we're in the process of setting up the CV wait
	//       -> CV may never get notified and we would have to wait for a long CV timeout even if the command is already complete
	if (!cmd.completed) {
		// instead: do a spin-wait and only wait on the CV if the command doesn't get completed fast enough
		const auto start_time = core::unix_timestamp_ms();
		for (uint32_t iter = 0u, attempt = 0u; ; ++iter, ++attempt) {
			if (cmd.completed) {
				break;
			}
			spin_wait_or_yield(attempt);
			
			if (iter > 0 && (iter % 1024u) == 0u) [[unlikely]] {
				const auto cur_time = core::unix_timestamp_ms();
				if (cur_time - start_time >= 60'000u || metal4_command_pool::is_ctx_shutdown) {
					log_error("command buffer (\"$\") timeout", safe_string(cmd_buffer.name));
					cmd.completed = true;
					// NOTE: not returning here, still need to properly complete/clean up the command buffer even if there was an error
					break;
				}
				
				// only now back off and wait for the CV
				std::unique_lock<std::mutex> completed_lock_guard(cmd.completed_cv_lock);
				const auto wait_time = 5ms * std::clamp(iter / 1024u, 1u, 10u);
				if (cmd.completed_cv.wait_for(completed_lock_guard, wait_time, [&cmd] { return cmd.completed.load(); })) {
					assert(cmd.completed.load());
					break;
				}
				// else: continue waiting
			}
		}
	}
	
	// call internal completion handlers
	std::vector<metal4_queue::metal4_completion_handler_f> completion_handlers;
	completion_handlers.swap(cmd.completion_handlers);
	for (const auto& compl_handler : completion_handlers) {
		if (compl_handler) {
			compl_handler();
		}
	}
	
	// call submit-specified handler (this must be the final handler)
	if (completion_handler) {
		completion_handler(cmd_buffer, cmd.is_error, cmd.error_str);
	}
	
	// inform queue that the submission has completed
	cmd.queue->completed_submission(cmd.submission_number);
	
	// mark resource + internals as free again
	pool.cmd_resources.release_resource(cmd_buffer.index);
}

//! stores all Metal command pool instances
struct metal4_command_pool_storage {
	//! access to "command_pools" must be thread-safe
	static inline safe_mutex command_pools_lock;
	//! contains all currently active/allocated command pools
	static inline fl::flat_map<metal4_command_pool*, std::unique_ptr<metal4_command_pool>>
	command_pools GUARDED_BY(command_pools_lock);
	
	//! creates a new command pool, returning a *non-owning* reference to it
	static metal4_command_pool* create_command_pool(const metal_device& dev) {
		@autoreleasepool {
			auto cmd_pool = std::make_unique<metal4_command_pool>(dev);
			auto cmd_pool_ret = cmd_pool.get();
			{
				GUARD(command_pools_lock);
				metal4_command_pool* cmd_pool_ptr = cmd_pool.get();
				command_pools.emplace(std::move(cmd_pool_ptr), std::move(cmd_pool));
			}
			return cmd_pool_ret;
		}
	}
	
	//! destroys the specified command pool, returns true on success
	static bool destroy_command_pool(metal4_command_pool* cmd_pool) {
		@autoreleasepool {
			GUARD(command_pools_lock);
			if (auto pool_iter = command_pools.find(cmd_pool); pool_iter != command_pools.end()) {
				command_pools.erase(pool_iter);
				return true;
			}
			return false;
		}
	}
};

//! since command pools are created per-thread and we don't necessarily have a clean direct way of destructing command pool resources,
//! we do this via a static thread-local RAII class instead, with the destructor in it being called once the thread exits
static thread_local struct metal4_command_pool_destructor_t {
	metal4_command_pool_destructor_t() = default;
	~metal4_command_pool_destructor_t() {
		metal4_command_pool_storage::destroy_command_pool(pool);
	}
	
	metal4_command_pool* pool { nullptr };
} metal4_command_pool_destructor {};

// NOTE/TODO: right now, this assumes we only ever have a single device (currently true for all Apple GPUs, but may change in the futurue)
metal4_command_pool* create_or_get_thread_command_pool(const metal_device& dev) {
	if (metal4_thread_command_pool) {
		return metal4_thread_command_pool;
	}
	
	@autoreleasepool {
		metal4_thread_command_pool = metal4_command_pool_storage::create_command_pool(dev);
		assert(metal4_thread_command_pool);
		
		// register in per-thread destructor
		metal4_command_pool_destructor.pool = metal4_thread_command_pool;
		
		auto thread_name = get_current_thread_name();
		if (thread_name.empty()) {
			std::stringstream sstr;
			sstr << std::this_thread::get_id();
			thread_name = sstr.str();
		}
		
		// allocate initial command resources
		for (uint32_t res_idx = 0; res_idx < metal_thread_resource_count; ++res_idx) {
			auto& cmd = metal4_thread_command_pool->cmd_resources.resources[res_idx];
			NSError* err = nil;
			
			// allocator
			MTL4CommandAllocatorDescriptor* desc = [MTL4CommandAllocatorDescriptor new];
			const auto label = "cmd_allocator:" + thread_name + ":" + std::to_string(res_idx);
			desc.label = [NSString stringWithUTF8String:label.c_str()];
			cmd.allocator = [dev.device newCommandAllocatorWithDescriptor:desc error:&err];
			if (!cmd.allocator || err) {
				log_error("failed to create thread command allocator @$: $", res_idx,
						  err ? [[err localizedDescription] UTF8String] : "<no-error-msg>");
				return nullptr;
			}
			
			// command buffer
			cmd.cmd_buffer = [dev.device newCommandBuffer];
			if (!cmd.cmd_buffer) {
				log_error("failed to create thread command buffer @$", res_idx);
				return nullptr;
			}
			
			// residency set
			MTLResidencySetDescriptor* rs_desc = [MTLResidencySetDescriptor new];
			const auto rs_label = "residency_set:" + thread_name + ":" + std::to_string(res_idx);
			desc.label = [NSString stringWithUTF8String:rs_label.c_str()];
			cmd.residency_set = [dev.device newResidencySetWithDescriptor:rs_desc
																	error:&err];
			if (!cmd.residency_set || err) {
				log_error("failed to create thread residency set @$: $", res_idx,
						  err ? [[err localizedDescription] UTF8String] : "<no-error-msg>");
				return nullptr;
			}
			
			// init commit op
			cmd.commit_op = [MTL4CommitOptions new];
			// NOTE: feedback handler will be called from a different thread -> must to call the origin command pool
			// NOTE: this needs to readded to the commits *every* time
			metal4_command_pool* origin_thread_command_pool = metal4_thread_command_pool;
			cmd.completion_cb = ^(id <MTL4CommitFeedback> feedback) {
				auto& compl_state = origin_thread_command_pool->cmd_resources.resources[res_idx];
				compl_state.is_error = (feedback.error != nil);
				if (compl_state.is_error) [[unlikely]] {
					@autoreleasepool {
						compl_state.error_str = safe_string([[feedback.error localizedDescription] UTF8String]);
						log_error("command buffer error: $", compl_state.error_str);
					}
				}
				if (compl_state.is_profiling) [[unlikely]] {
					const auto elapsed_time = ([feedback GPUEndTime] - [feedback GPUStartTime]);
					*compl_state.profiling_sum += uint64_t(elapsed_time * 1000000.0);
				}
				
				compl_state.completed = true;
				compl_state.completed_cv.notify_one();
			};
		}
		
		return metal4_thread_command_pool;
	}
}

static bool did_init_metal4_queue { false };
void metal4_queue::init() {
	if (!did_init_metal4_queue) {
		did_init_metal4_queue = true;
		mtl_cmd_completion_handler = std::make_unique<metal4_cmd_completion_handler>();
	}
}
void metal4_queue::destroy() {
	if (did_init_metal4_queue) {
		mtl_cmd_completion_handler = nullptr;
		metal4_command_pool::is_ctx_shutdown = true;
	}
}

metal4_queue::metal4_queue(const device& dev_, id <MTL4CommandQueue> queue_) : device_queue(dev_, QUEUE_TYPE::ALL), queue(queue_) {
#if defined(FLOOR_DEBUG)
	// zero-init "pending_submissions" (we don't actually need this to be the zero'ed out, but it makes debugging easier)
	const auto pending_submissions_range = pending_submissions.get();
	memset(pending_submissions_range.data(), 0, pending_submissions_range.size_bytes());
#endif
}

metal4_queue::~metal4_queue() {
	@autoreleasepool {
		finish();
		queue = nil;
	}
}

const void* metal4_queue::get_queue_ptr() const {
	return (__bridge const void*)queue;
}

void* metal4_queue::get_queue_ptr() {
	return (__bridge void*)queue;
}

id <MTL4CommandQueue> metal4_queue::get_queue() const {
	return queue;
}

void metal4_queue::dequeue_completed_submissions() const {
	for (;;) {
		const auto [sn, has_value] = pending_submissions.peek();
		if (!has_value || !sn.is_complete()) {
			break;
		}
		(void)pending_submissions.dequeue();
	}
}

void metal4_queue::completed_submission(const uint64_t submission_number) const {
	bool success = false;
	{
		GUARD(queue_lock);
		if (!pending_submissions.empty()) {
			dequeue_completed_submissions();
			
			// check if the front element is our submission_number
			if (const auto [sn, has_value] = pending_submissions.peek(); sn == submission_number) [[likely]] {
				// if so, dequeue, and we're almost done here
				(void)pending_submissions.dequeue();
				success = true;
				
				// clean up / remove all completed submissions after ours as well
				// NTOE: this is important, because if any completion went into the else-case below,
				//       there may be multiple completed submissions after our current completed submission that
				//       a finish() may wait on, but there may also not come another completion that cleans up
				//       these completed submissions for a while after ours, so a finish() might block indefinitely
				dequeue_completed_submissions();
			} else {
				// if our submission_number was not at the front, find it in the enqueued elements + flag it as complete
				pending_submissions.peek_iterate([submission_number, &success](submission_number_t& elem) {
					if (elem == submission_number) {
						elem.complete();
						success = true;
						return true;
					}
					return false;
				});
			}
		}
	}
	if (!success) [[unlikely]] {
		log_error("invalid submission number: $'", submission_number);
	}
	finish_cv.notify_all();
}

void metal4_queue::finish() const {
	uint64_t last_submission_number = 0u;
	{
		GUARD(queue_lock);
		// submission numbers are implicitly stored in ascending order
		// -> just need to grab the last one and wait for its completion
		const auto [sn_back, non_empty] = pending_submissions.peek_back();
		if (!non_empty) {
			// if there are no pending submissions, we can just exit here
			return;
		}
		assert(sn_back != submission_number_t::invalid_submission_number);
		last_submission_number = sn_back;
	}
	
	while (!metal4_command_pool::is_ctx_shutdown) {
		{
			std::unique_lock<std::mutex> finish_lock_guard(finish_cv_lock);
			finish_cv.wait_for(finish_lock_guard, 250ms);
		}
		{
			GUARD(queue_lock);
			const auto [sn_front, non_empty] = pending_submissions.peek();
			if (!non_empty || sn_front > last_submission_number) {
				return;
			}
		}
	}
}

template <typename mtl_indirect_cmd_type>
requires (std::is_same_v<mtl_indirect_cmd_type, metal4_soft_indirect_command_pipeline> ||
		  std::is_same_v<mtl_indirect_cmd_type, metal4_indirect_command_pipeline>)
static inline void handle_metal_indirect_compute(const mtl_indirect_cmd_type& mtl_indirect_cmd,
												 const metal4_queue& cqueue, const metal_device& dev,
												 const device_queue::indirect_execution_parameters_t& params,
												 kernel_completion_handler_f&& completion_handler,
												 const uint32_t command_count) {
	@autoreleasepool {
		auto mtl_indirect_pipeline_entry = mtl_indirect_cmd.get_metal_pipeline_entry(dev);
		if (!mtl_indirect_pipeline_entry) {
			log_error("no indirect command pipeline state for device \"$\" in indirect command pipeline \"$\"",
					  dev.name, mtl_indirect_cmd.get_description().debug_label);
			return;
		}
		
		if constexpr (std::is_same_v<mtl_indirect_cmd_type, metal4_soft_indirect_command_pipeline>) {
			mtl_indirect_cmd.update_resources(dev, *mtl_indirect_pipeline_entry);
		} else if constexpr (std::is_same_v<mtl_indirect_cmd_type, metal4_indirect_command_pipeline>) {
			mtl_indirect_cmd.update_resources(dev, *mtl_indirect_pipeline_entry);
		}
		
		// create and setup the compute encoder
		auto cmd_buffer = cqueue.make_command_buffer();
		assert(cmd_buffer);
		[cmd_buffer.cmd_buffer beginCommandBufferWithAllocator:cmd_buffer.allocator];
		
		id <MTL4ComputeCommandEncoder> encoder = [cmd_buffer.cmd_buffer computeCommandEncoder];
#if defined(FLOOR_DEBUG)
		if (params.debug_label) {
			[encoder setLabel:[NSString stringWithUTF8String:params.debug_label]];
		}
#endif
		
		for (const auto& fence : params.wait_fences) {
			[encoder waitForFence:((const metal_fence*)fence)->get_metal_fence()
			  beforeEncoderStages:MTLStageDispatch];
		}
		
		// declare all used resources
		const auto has_resources = mtl_indirect_pipeline_entry->has_resources();
		if (has_resources) {
			[cmd_buffer.cmd_buffer useResidencySet:mtl_indirect_pipeline_entry->residency_set];
		}
		
		if (mtl_indirect_pipeline_entry->printf_buffer) {
			mtl_indirect_pipeline_entry->printf_init(cqueue);
		}
		
		if constexpr (std::is_same_v<mtl_indirect_cmd_type, metal4_indirect_command_pipeline>) {
			[encoder executeCommandsInBuffer:mtl_indirect_pipeline_entry->icb
								   withRange:NSRange { .location = 0u, .length = command_count }];
		} else {
			id <MTLComputePipelineState> cur_pipeline_state = nil;
			for (uint32_t cmd_idx = 0u; cmd_idx < command_count; ++cmd_idx) {
				const auto& cmd_encoder = mtl_indirect_cmd.get_compute_command(cmd_idx);
				const auto& cmd_params = mtl_indirect_pipeline_entry->compute_commands[cmd_idx];
				
				if (cur_pipeline_state != cmd_encoder.pipeline_state) {
					[encoder setComputePipelineState:cmd_encoder.pipeline_state];
					cur_pipeline_state = cmd_encoder.pipeline_state;
				}
				[encoder setArgumentTable:cmd_encoder.arg_table];
				[encoder dispatchThreadgroups:cmd_params.grid_dim threadsPerThreadgroup:cmd_params.block_dim];
				
				if (cmd_params.wait_until_completion) {
					[encoder barrierAfterEncoderStages:MTLStageDispatch
								   beforeEncoderStages:MTLStageDispatch
									 visibilityOptions:MTL4VisibilityOptionNone];
				}
			}
		}
		
		for (const auto& fence : params.signal_fences) {
			[encoder updateFence:((const metal_fence*)fence)->get_metal_fence()
			  afterEncoderStages:MTLStageDispatch];
		}
		[encoder endEncoding];
		
		if (mtl_indirect_pipeline_entry->printf_buffer) {
			cqueue.printf_completion(cmd_buffer, mtl_indirect_pipeline_entry->printf_buffer);
		}
		
		[cmd_buffer.cmd_buffer endCommandBuffer];
		
		cqueue.submit_command_buffer(std::move(cmd_buffer),
									 [handler_ = std::move(completion_handler)](const metal4_command_buffer& cmd_buffer_,
																				const bool is_error,
																				const std::string_view err_string) {
			if (is_error) {
				log_error("compute command buffer error (in \"$\"): $", cmd_buffer_.name ? cmd_buffer_.name : "<unnamed>", err_string);
			}
			
			if (handler_) {
				handler_();
			}
		}, params.wait_until_completion);
	}
}

void metal4_queue::execute_indirect(const indirect_command_pipeline& indirect_cmd,
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
	} else if (const auto mtl_soft_indirect_cmd = dynamic_cast<const metal4_soft_indirect_command_pipeline*>(&indirect_cmd);
			   mtl_soft_indirect_cmd) {
		handle_metal_indirect_compute(*mtl_soft_indirect_cmd, *this, (const metal_device&)dev, params,
									  std::move(completion_handler), command_count);
		return;
	} else if (const auto mtl_indirect_cmd = dynamic_cast<const metal4_indirect_command_pipeline*>(&indirect_cmd);
			   mtl_indirect_cmd) {
		handle_metal_indirect_compute(*mtl_indirect_cmd, *this, (const metal_device&)dev, params,
									  std::move(completion_handler), command_count);
		return;
	} else {
		log_error("invalid indirect command pipeline \"$\"", indirect_cmd.get_description().debug_label);
		return;
	}
}

void metal4_queue::start_profiling() const {
	// signal that we're profiling now
	is_profiling = true;
}

uint64_t metal4_queue::stop_profiling() const {
	// wait on all buffers
	finish();
	
	const uint64_t ret = profiling_sum;
	
	// clear
	profiling_sum = 0;
	is_profiling = false;
	
	return ret;
}

metal4_command_buffer metal4_queue::make_command_buffer(const char* name) const {
	const auto thread_command_pool = create_or_get_thread_command_pool((const metal_device&)dev);
	if (!thread_command_pool) [[unlikely]] {
		throw std::runtime_error("no command pool could be created for the current thread");
	}
	return thread_command_pool->make_command_buffer(name);
}

void metal4_queue::free_command_buffer(metal4_command_buffer&& cmd_buffer) const {
	const auto thread_command_pool = create_or_get_thread_command_pool((const metal_device&)dev);
	if (!thread_command_pool) [[unlikely]] {
		throw std::runtime_error("no command pool could be created for the current thread");
	}
	return thread_command_pool->release_unsubmitted_command_buffer(std::forward<metal4_command_buffer>(cmd_buffer));
}

metal4_command_block metal4_queue::make_command_block(const char* name, bool& error_signal, const bool is_blocking) const {
	return metal4_command_block { *this, name, error_signal, is_blocking };
}

void metal4_queue::submit_command_buffer(metal4_command_buffer&& cmd_buffer,
										 metal4_queue::command_buffer_completion_handler_f&& completion_handler,
										 const bool blocking) const {
	const auto thread_command_pool = create_or_get_thread_command_pool((const metal_device&)dev);
	if (!thread_command_pool) [[unlikely]] {
		throw std::runtime_error("no command pool could be created for the current thread");
	}
	thread_command_pool->submit_command_buffer(std::move(cmd_buffer), *this, std::move(completion_handler), blocking);
}

void metal4_queue::add_completion_handler(const metal4_command_buffer& cmd_buffer,
										  metal4_completion_handler_f&& completion_handler) const {
	const auto thread_command_pool = create_or_get_thread_command_pool((const metal_device&)dev);
	if (!thread_command_pool) [[unlikely]] {
		throw std::runtime_error("no command pool could be created for the current thread");
	}
	thread_command_pool->add_completion_handler(cmd_buffer, std::move(completion_handler));
}

void metal4_queue::printf_completion(metal4_command_buffer& cmd_buffer, std::shared_ptr<device_buffer> printf_buffer) const {
	auto internal_dev_queue = get_context().get_device_default_queue(dev);
	add_completion_handler(cmd_buffer, [printf_buffer, internal_dev_queue] {
		auto cpu_printf_buffer = std::make_unique<uint32_t[]>(printf_buffer_size / 4);
		printf_buffer->read(*internal_dev_queue, cpu_printf_buffer.get());
		handle_printf_buffer(std::span { cpu_printf_buffer.get(), printf_buffer_size / 4 });
	});
}

metal4_command_block::metal4_command_block(const metal4_queue& mtl_queue_, const char* name,
										   bool& error_signal_, const bool is_blocking_) :
mtl_queue(mtl_queue_), error_signal(error_signal_), is_blocking(is_blocking_), has_name(name && name[0] != '\0') {
	@autoreleasepool {
		// create new command buffer + begin
		cmd_buffer = mtl_queue.make_command_buffer(name);
		if (!cmd_buffer) {
			// this can only fail on ctx shutdown, so just return here + signal failure
			error_signal_ = true;
			return;
		}
		[cmd_buffer.cmd_buffer beginCommandBufferWithAllocator:cmd_buffer.allocator];
		
#if defined(FLOOR_DEBUG)
		if (has_name) {
			NSString* label = [NSString stringWithUTF8String:floor_force_nonnull(name)];
			cmd_buffer.cmd_buffer.label = label;
			[cmd_buffer.cmd_buffer pushDebugGroup:label];
		}
#endif
		
		// done
		valid = true;
	}
}

metal4_command_block::~metal4_command_block() {
	if (!valid || !cmd_buffer) {
		error_signal = true;
		return;
	}
	
	@autoreleasepool {
#if defined(FLOOR_DEBUG)
		if (has_name) {
			[cmd_buffer.cmd_buffer popDebugGroup];
		}
#endif
		
		if ([cmd_buffer.residency_set allocationCount]) {
			[cmd_buffer.cmd_buffer useResidencySet:cmd_buffer.residency_set];
			[cmd_buffer.residency_set commit];
		}
		[cmd_buffer.cmd_buffer endCommandBuffer];
		mtl_queue.submit_command_buffer(std::move(cmd_buffer),
										[error_signal_ = &error_signal, is_blocking_ = is_blocking](const metal4_command_buffer& cmd_buffer_,
																									const bool is_error,
																									const std::string_view err_string) {
			if (is_error) {
				log_error("command buffer error (in \"$\"): $", cmd_buffer_.name ? cmd_buffer_.name : "<unnamed>", err_string);
				
				// signal outside block that there was an error
				// NOTE: can only do this if "this" still exists, i.e. this was a blocking submit
				if (is_blocking_) {
					*error_signal_ = true;
				}
			}
		}, is_blocking);
	}
}

} // namespace fl

#endif

