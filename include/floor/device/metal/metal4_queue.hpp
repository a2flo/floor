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

#pragma once

#include <floor/device/metal/metal_common.hpp>

#if !defined(FLOOR_NO_METAL)

#include <floor/device/device_queue.hpp>
#include <floor/threading/thread_safety.hpp>
#include <floor/threading/atomic_spin_lock.hpp>
#include <floor/threading/resource_slot_handler.hpp>
#include <floor/core/small_ring_buffer.hpp>
#if defined(__OBJC__)
#include <Metal/Metal.h>
#endif

namespace fl {

class metal4_command_block;
struct metal4_command_pool;
struct metal4_command_buffer_completion_impl;
class metal_context;

//! per-thread command resource count
//! NOTE: since these are *per-thread* we are probably never going to need more than this
static constexpr const uint32_t metal_thread_resource_count { 64u };

struct metal4_command_buffer {
#if defined(__OBJC__)
	using allocator_t = id <MTL4CommandAllocator>;
	using cmd_buffer_t = id <MTL4CommandBuffer>;
	using residency_set_t = id <MTLResidencySet>;
#else
	using allocator_t = void*;
	using cmd_buffer_t = void*;
	using residency_set_t = void*;
#endif
	
	allocator_t floor_null_unspecified allocator { nullptr };
	cmd_buffer_t floor_null_unspecified cmd_buffer { nullptr };
	residency_set_t floor_null_unspecified residency_set { nullptr };
	const char* floor_nullable name { nullptr };
	uint8_t index { resource_slot_handler<metal_thread_resource_count>::invalid_slot_idx };
	
	metal4_command_buffer() = default;
	metal4_command_buffer(metal4_command_buffer&& other) :
	allocator(other.allocator), cmd_buffer(other.cmd_buffer), residency_set(other.residency_set),
	name(other.name), index(other.index) {
		other.reset();
	}
	metal4_command_buffer& operator=(metal4_command_buffer&& other) {
		assert(allocator == nullptr && cmd_buffer == nullptr && residency_set == nullptr &&
			   index == resource_slot_handler<metal_thread_resource_count>::invalid_slot_idx);
		allocator = other.allocator;
		cmd_buffer = other.cmd_buffer;
		residency_set = other.residency_set;
		index = other.index;
		name = other.name;
		other.reset();
		return *this;
	}
	~metal4_command_buffer() {
		@autoreleasepool {
			allocator = nil;
			cmd_buffer = nil;
			residency_set = nil;
		}
	}
	
	explicit operator bool() const {
		return (allocator != nullptr && cmd_buffer != nullptr && residency_set != nullptr &&
				index != resource_slot_handler<metal_thread_resource_count>::invalid_slot_idx);
	}
	
protected:
	friend metal4_command_buffer_completion_impl;
	
	void reset() {
		allocator = nullptr;
		cmd_buffer = nullptr;
		residency_set = nullptr;
		index = resource_slot_handler<metal_thread_resource_count>::invalid_slot_idx;
	}
};

struct submission_number_t {
	//! value of invalid submission numbers that should never be encountered
	static constexpr const uint64_t invalid_submission_number { 0u };
	//! used to mark a submission number as complete
	static constexpr const uint64_t submission_complete_flag { 1ull << 63ull };
	
	constexpr submission_number_t() noexcept = default;
	constexpr submission_number_t(const uint64_t sn) : value(sn) {
		assert(value < submission_complete_flag);
		assert(sn != invalid_submission_number);
	}
	
	//! returns the underlying submission number w/o the "submission_complete_flag"
	//! NOTE: this makes handling easier in situations where we're only interested in the number
	operator uint64_t() const {
		const auto ret = (value & ~submission_complete_flag);
		assert(ret != invalid_submission_number);
		return ret;
	}
	
	bool is_complete() const {
		return (value & submission_complete_flag) != 0u;
	}
	
	bool is_valid() const {
		return (value != invalid_submission_number);
	}
	
	//! marks this submission number as complete
	void complete() {
		assert(!is_complete());
		assert(is_valid());
		value |= submission_complete_flag;
	}
	
protected:
	//! the full underlying submission number
	//! NOTE: this contains the "submission_complete_flag"
	uint64_t value { invalid_submission_number };
	
};
static_assert(sizeof(submission_number_t) == sizeof(uint64_t));

class metal4_queue final : public device_queue {
public:
#if defined(__OBJC__)
	using queue_t = id <MTL4CommandQueue>;
	using cmd_buffer_t = id <MTL4CommandBuffer>;
#else
	using queue_t = void*;
	using cmd_buffer_t = void*;
#endif
	
#if defined(__OBJC__)
	explicit metal4_queue(const device& dev, id <MTL4CommandQueue> floor_nonnull queue);
#endif
	~metal4_queue() override;
	
	void finish() const override REQUIRES(!queue_lock);
	void flush() const override { /* nop */ }
	
	void execute_indirect(const indirect_command_pipeline& indirect_cmd,
						  const indirect_execution_parameters_t& params,
						  kernel_completion_handler_f&& completion_handler) const override;
	
	// this is synchronized elsewhere
	const void* floor_nonnull get_queue_ptr() const override;
	void* floor_nonnull get_queue_ptr() override;
	
#if defined(__OBJC__)
	id <MTL4CommandQueue> floor_nonnull get_queue() const;
#endif
	
	//! NOTE: this will block until a free command buffer can be acquired
	metal4_command_block make_command_block(const char* floor_nonnull name, bool& error_signal, const bool is_blocking) const;
	
	//! NOTE: this will block until a free command buffer can be acquired
	metal4_command_buffer make_command_buffer(const char* floor_nullable name = nullptr) const;
	
	//! free an unsubmitted command buffer
	//! NOTE: caller is responsible for calling endCommandBuffer if beginCommandBufferWithAllocator was called
	void free_command_buffer(metal4_command_buffer&& cmd_buffer) const;
	
	//! completion handler type for "submit_command_buffer"
	using command_buffer_completion_handler_f = std::function<void(const metal4_command_buffer& /* command_buffer */,
																   const bool /* is_error */, const std::string_view /* err_string */)>;
	
	//! submits the specified "cmd_buffer" to this queue,
	//! "completion_handler" will be called once the cmd buffer fully completed execution,
	//! "blocking" signals if the function should not return until the cmd buffer has fully completed execution
	//! NOTE: ownership of "cmd_buffer" and "completion_handler" are transferred to this function
	//! NOTE: do not rely on the address of the cmd buffer parameter in "completion_handler", this may not be the same as the initial one
	void submit_command_buffer(metal4_command_buffer&& cmd_buffer,
							   command_buffer_completion_handler_f&& completion_handler,
							   const bool blocking = true) const;
	
	//! completion handler type for "add_completion_handler"
	using metal4_completion_handler_f = std::function<void()>;
	
	//! adds a completion handler to the specified command buffer that is called once the command buffer has finished execution
	//! NOTE: must be called before submit_command_buffer, otherwise this has no effect
	void add_completion_handler(const metal4_command_buffer& cmd_buffer,
								metal4_completion_handler_f&& completion_handler) const;
	
	//! adds a printf completion handler of the specified "printf_buffer" to the specified command buffer
	void printf_completion(metal4_command_buffer& cmd_buffer, std::shared_ptr<device_buffer> printf_buffer) const;
	
	bool has_profiling_support() const override {
		return true;
	}
	void start_profiling() const override;
	uint64_t stop_profiling() const override REQUIRES(!queue_lock);
	
	void completed_submission(const uint64_t submission_number) const REQUIRES(!queue_lock);
	
protected:
	queue_t floor_null_unspecified queue { nullptr };
	
	mutable atomic_spin_lock queue_lock;
	mutable std::atomic<uint64_t> submission_counter { 1u };
	mutable small_ring_buffer<submission_number_t, metal_thread_resource_count * 4u> pending_submissions GUARDED_BY(queue_lock);
	//! dequeues all front elements in "pending_submissions" that were flagged as complete
	void dequeue_completed_submissions() const REQUIRES(queue_lock);
	
	friend metal4_command_pool;
	friend metal4_command_block;
	
	mutable std::atomic<bool> is_profiling { false };
	mutable std::atomic<uint64_t> profiling_sum { 0 };
	
	friend metal_context;
	static void init();
	static void destroy();
	
	//! required lock for "finish_cv"
	mutable std::mutex finish_cv_lock;
	//! will be signaled every time a queue submission has completed
	mutable std::condition_variable finish_cv;
	
};

//! command buffer block that will automatically start the cmd buffer on construction and end + submit it on destruction
class metal4_command_block {
public:
	metal4_command_buffer cmd_buffer {};
	
	metal4_command_block(const metal4_queue& mtl_queue, const char* floor_nullable name, bool& error_signal, const bool is_blocking);
	~metal4_command_block();
	
	bool valid { false };
	explicit operator bool() const {
		return valid;
	}
	
protected:
	const metal4_queue& mtl_queue;
	bool& error_signal;
	const bool is_blocking { true };
	const bool has_name { false };
};

} // namespace fl

//! creates a "command block", i.e. creates a command buffer, starts it, runs the code specified as "...", and finally submits the buffer,
//! returns "ret" on error
#define MTL4_CMD_BLOCK_RET(mtl_queue, name, code, ret, is_blocking, ...) \
do { \
	@autoreleasepool { \
		bool error_signal_ = false; \
		{ \
			auto cmd_block_ = (mtl_queue).make_command_block(name, error_signal_, is_blocking, ##__VA_ARGS__); \
			if (!cmd_block_ || error_signal_) { \
				return ret; \
			} \
			auto& block_cmd_buffer = cmd_block_.cmd_buffer; \
			code; \
		} \
		if (error_signal_) { \
			return ret; \
		} \
	} \
} while (false)
//! creates a "command block", i.e. creates a command buffer, starts it, runs the code specified as "...", and finally submits the buffer
#define MTL4_CMD_BLOCK(mtl_queue, name, code, is_blocking, ...) MTL4_CMD_BLOCK_RET(mtl_queue, name, code, {}, is_blocking, ##__VA_ARGS__)

#endif
