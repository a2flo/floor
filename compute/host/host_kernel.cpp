/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2015 Florian Ziesche
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

// need to include this before all else
#if defined(__APPLE__) && !defined(_XOPEN_SOURCE) && !defined(_STRUCT_UCONTEXT)
#define _XOPEN_SOURCE
#endif
#include <ucontext.h>

#include <floor/compute/host/host_kernel.hpp>

#if !defined(FLOOR_NO_HOST_COMPUTE)

#include <floor/compute/compute_queue.hpp>
#include <floor/compute/host/host_buffer.hpp>
#include <floor/compute/host/host_image.hpp>
#include <floor/compute/host/host_queue.hpp>

#include <floor/core/timer.hpp>
//#define FLOOR_HOST_KERNEL_ENABLE_TIMING 1

#if defined(__APPLE__)
#include <mach/thread_policy.h>
#include <mach/thread_act.h>
#elif defined(__linux__) || defined(__FreeBSD__)
#include <pthread.h>
#if defined(__FreeBSD__)
#include <pthread_np.h>
#endif
#endif

#if defined(__clang__) // ignore warnings about deprecated functions
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#endif

//
static const function<void()>* cur_kernel_function { nullptr };
extern "C" void run_mt_group_item(const uint32_t local_linear_idx);

// NOTE: due to rather fragile stack handling (rsp), this is completely done in asm, so that the compiler can't do anything wrong
#if defined(PLATFORM_X64) && !defined(FLOOR_IOS) && !defined(__WINDOWS__)
asm("floor_get_context_sysv_x86_64:"
	// store all registers in fiber_context*
	"movq %rbp, 0x0(%rdi);"
	"movq %rbx, 0x8(%rdi);"
	"movq %r12, 0x10(%rdi);"
	"movq %r13, 0x18(%rdi);"
	"movq %r14, 0x20(%rdi);"
	"movq %r15, 0x28(%rdi);"
	"movq %rsp, %rcx;"
	"addq $0x8, %rcx;"
	"movq %rcx, 0x30(%rdi);" // rsp
	"movq (%rsp), %rcx;"
	"movq %rcx, 0x38(%rdi);" // rip
	"retq;");
asm("floor_set_context_sysv_x86_64:"
	// restore all registers from fiber_context*
	"movq 0x0(%rdi), %rbp;"
	"movq 0x8(%rdi), %rbx;"
	"movq 0x10(%rdi), %r12;"
	"movq 0x18(%rdi), %r13;"
	"movq 0x20(%rdi), %r14;"
	"movq 0x28(%rdi), %r15;"
	"movq 0x30(%rdi), %rsp;"
	"movq 0x38(%rdi), %rcx;"
	// and jump to rip (rcx)
	"jmp *%rcx;");
asm(".extern exit;"
	"floor_enter_context_sysv_x86_64:"
	// retrieve fiber_context*
	"movq 0x8(%rsp), %rax;"
	// fiber_context->init_func
	"movq 0x50(%rax), %rcx;"
	// fiber_context->init_arg
	"movq 0x60(%rax), %rdi;"
	// call init_func(init_arg)
	"callq *%rcx;"
	// context is done, -> exit to set exit context, or exit(0)
	// retrieve fiber_context* again
	"movq 0x8(%rsp), %rax;"
	// exit fiber_context*
	"movq 0x58(%rax), %rdi;"
	// TODO: cmp 0, -> exit(0)
	// set_context(exit_context)
	"callq floor_set_context_sysv_x86_64;"
	// it's a trap!
	"ud2");
extern "C" void floor_get_context(void* ctx) asm("floor_get_context_sysv_x86_64");
extern "C" void floor_set_context(void* ctx) asm("floor_set_context_sysv_x86_64");
extern "C" void floor_enter_context() asm("floor_enter_context_sysv_x86_64");
#endif

struct fiber_context {
	typedef void (*init_func_type)(const uint32_t);
	
#if defined(PLATFORM_X64) && !defined(FLOOR_IOS) && !defined(__WINDOWS__)
	// sysv x86-64 abi compliant implementation
	static constexpr const size_t min_stack_size { 4096 };
	
	// callee-saved registers
	uint64_t rbp { 0 };
	uint64_t rbx { 0 };
	uint64_t r12 { 0 };
	uint64_t r13 { 0 };
	uint64_t r14 { 0 };
	uint64_t r15 { 0 };
	// stack pointer
	uint64_t rsp { 0 };
	// return address / instruction pointer
	uint64_t rip { 0 };
	
	void init(void* stack_ptr_, const size_t& stack_size_,
			  init_func_type init_func_, const uint32_t& init_arg_,
			  fiber_context* exit_ctx_) {
		init_common(stack_ptr_, stack_size_, init_func_, init_arg_, exit_ctx_);
	}
	
	void reset() {
		// reset registers, set rip to enter_context and reset rsp
		rbp = 0;
		rbx = 0;
		r12 = 0;
		r13 = 0;
		r14 = 0;
		r15 = 0;
		// we're pushing two 64-bit values here + needs to be 16-byte aligned
		rsp = ((size_t)stack_ptr) + stack_size - 16u;
		rip = (uint64_t)floor_enter_context;
		// set first 64-bit value on stack to this context
		*(uint64_t*)(rsp + 8u) = (uint64_t)this;
		// for stack protection (well, corruption detection ...) purposes
		// TODO: check this on exit (in debug mode or when manually enabled)
		*(uint64_t*)(rsp) = 0x0123456789ABCDEF;
	}
	
	void get_context() {
		floor_get_context(this);
	}
	
	[[noreturn]] void set_context() {
		floor_set_context(this);
		floor_unreachable();
	}
	
	void swap_context(fiber_context* next_ctx) {
		volatile bool swapped = false;
		get_context();
		if(!swapped) {
			swapped = true;
			next_ctx->set_context();
		}
	}
	
//#elif defined(PLATFORM_X64) && defined(FLOOR_IOS)
	// TODO: aarch64/armv8 implementation
//#elif defined(PLATFORM_X32) && defined(FLOOR_IOS)
	// TODO: armv7 implementation
#elif defined(__WINDOWS__)
	// TODO: windows implementation (-> use CreateFiber/etc)
#error "not implemented yet"
#else
	// fallback to posix ucontext_t/etc
	static constexpr const size_t min_stack_size { 32768 };
	
	ucontext_t ctx;
	
	void init(void* stack_ptr_, const size_t& stack_size_,
			  init_func_type init_func_, const uint32_t& init_arg_,
			  fiber_context* exit_ctx_) {
		init_common(stack_ptr_, stack_size_, init_func_, init_arg_, exit_ctx_);
		
		memset(&ctx, 0, sizeof(ucontext_t));
		getcontext(&ctx);
		
		// unknown context vars -> query external
		if(stack_ptr == nullptr) {
			stack_ptr = ctx.uc_stack.ss_sp;
			stack_size = ctx.uc_stack.ss_size;
		}
	}
	
	void reset() {
		if(exit_ctx != nullptr) {
			ctx.uc_link = &exit_ctx->ctx;
		}
		else ctx.uc_link = nullptr;
		ctx.uc_stack.ss_sp = stack_ptr;
		ctx.uc_stack.ss_size = stack_size;
		makecontext(&ctx, (void (*)())init_func, 1, init_arg);
	}
	
	void get_context() {
		getcontext(&ctx);
	}
	
	void set_context() {
		setcontext(&ctx);
	}
	
	void swap_context(fiber_context* next_ctx) {
		swapcontext(&ctx, &next_ctx->ctx);
	}
#endif
	void init_common(void* stack_ptr_, const size_t& stack_size_,
					 init_func_type init_func_, const uint32_t& init_arg_,
					 fiber_context* exit_ctx_) {
		stack_ptr = stack_ptr_;
		stack_size = stack_size_;
		init_func = init_func_;
		exit_ctx = exit_ctx_;
		init_arg = init_arg_;
	}
	
	// ctx vars
	void* stack_ptr { nullptr };
	size_t stack_size { 0 };
	
	//
	init_func_type init_func { nullptr };
	
	//
	fiber_context* exit_ctx { nullptr };
	
	//
	uint32_t init_arg { 0 };
};

// thread affinity handling
static void floor_set_thread_affinity(const uint32_t& affinity) {
#if defined(__APPLE__)
	thread_port_t thread_port = pthread_mach_thread_np(pthread_self());
	thread_affinity_policy thread_affinity { int(affinity) };
	thread_policy_set(thread_port, THREAD_AFFINITY_POLICY, (thread_policy_t)&thread_affinity, THREAD_AFFINITY_POLICY_COUNT);
#elif defined(__linux__) || defined(__FreeBSD__)
	// use gnu extension
	cpu_set_t cpu_set;
	CPU_ZERO(&cpu_set);
	CPU_SET(affinity - 1, &cpu_set);
	pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpu_set);
#elif defined(__OpenBSD__)
	// TODO: pthread gnu extension not available here
#elif defined(__WINDOWS__)
	// TODO: !
#endif
}

// id handling vars
static uint32_t floor_work_dim { 1u };
static uint3 floor_global_work_size;
static uint32_t floor_linear_global_work_size;
static uint3 floor_local_work_size;
static uint32_t floor_linear_local_work_size;
static uint3 floor_group_size;
static uint32_t floor_linear_group_size;
static _Thread_local uint3 floor_global_idx;
static _Thread_local uint3 floor_local_idx;
static _Thread_local uint3 floor_group_idx;

// barrier handling vars
// -> mt-item
static atomic<uint32_t> barrier_counter { 0 };
static atomic<uint32_t> barrier_gen { 0 };
static uint32_t barrier_users { 0 };
// -> mt-group
static _Thread_local uint32_t item_local_linear_idx { 0 };
static _Thread_local fiber_context* item_contexts { nullptr };

//
host_kernel::host_kernel(const void* kernel_, const string& func_name_) :
kernel((kernel_func_type)kernel_), func_name(func_name_) {
}

host_kernel::~host_kernel() {}

void* host_kernel::handle_kernel_arg(shared_ptr<compute_buffer> buffer) {
	return ((host_buffer*)buffer.get())->get_host_buffer_ptr();
}

void* host_kernel::handle_kernel_arg(shared_ptr<compute_image> image) {
	return ((host_image*)image.get())->get_host_image_buffer_ptr();
}

void host_kernel::execute_internal(compute_queue* queue,
								   const uint32_t work_dim,
								   const size3 global_work_size,
								   const size3 local_work_size,
								   const function<void()>& kernel_func) {
	//
	cur_kernel_function = &kernel_func;
	const auto cpu_count = ((host_queue*)queue)->get_device()->units;
	const uint3 local_dim { local_work_size.maxed(1u) };
	const uint3 group_dim_overflow {
		global_work_size.x > 0 ? std::min(uint32_t(global_work_size.x % local_dim.x), 1u) : 0u,
		global_work_size.y > 0 ? std::min(uint32_t(global_work_size.y % local_dim.y), 1u) : 0u,
		global_work_size.z > 0 ? std::min(uint32_t(global_work_size.z % local_dim.z), 1u) : 0u
	};
	uint3 group_dim { (global_work_size / local_dim) + group_dim_overflow };
	group_dim.max(1u);
	
	// setup id handling
	floor_work_dim = work_dim;
	floor_global_work_size = global_work_size;
	floor_local_work_size = local_dim;
	
	const auto mod_groups = floor_global_work_size % floor_local_work_size;
	floor_group_size = floor_global_work_size / floor_local_work_size;
	if(mod_groups.x > 0) ++floor_group_size.x;
	if(mod_groups.y > 0) ++floor_group_size.y;
	if(mod_groups.z > 0) ++floor_group_size.z;
	
	floor_linear_global_work_size = floor_global_work_size.x * floor_global_work_size.y * floor_global_work_size.z;
	floor_linear_local_work_size = local_dim.x * local_dim.y * local_dim.z;
	floor_linear_group_size = floor_group_size.x * floor_group_size.y * floor_group_size.z;
	
#if defined(FLOOR_HOST_COMPUTE_ST) // single-threaded
	// it's usually best to go from largest to smallest loop count (usually: X > Y > Z)
	size3& global_idx = floor_global_idx;
	size3& local_idx = floor_local_idx;
	size3& group_idx = floor_group_idx;
	for(uint32_t group_x = 0; group_x < group_dim.x; ++group_x) {
		for(uint32_t group_y = 0; group_y < group_dim.y; ++group_y) {
			for(uint32_t group_z = 0; group_z < group_dim.z; ++group_z) {
				group_idx.set(group_x, group_y, group_z);
				global_idx.set(group_x * local_dim.x,
							   group_y * local_dim.y,
							   group_z * local_dim.z);
				local_idx.set(0, 0, 0);
				
				// this time, go from potentially smallest to largest (it's better to execute this in X, Y, Z order)
				for(; local_idx.z < local_dim.z; ++local_idx.z, ++global_idx.z) {
					local_idx.y = 0;
					global_idx.y = group_y * local_dim.y;
					for(; local_idx.y < local_dim.y; ++local_idx.y, ++global_idx.y) {
						local_idx.x = 0;
						global_idx.x = group_x * local_dim.x;
						for(; local_idx.x < local_dim.x; ++local_idx.x, ++global_idx.x) {
							kernel_func();
						}
					}
				}
			}
		}
	}
#elif defined(FLOOR_HOST_COMPUTE_MT_ITEM)
	// #work-items per group
	const uint32_t local_size = local_dim.x * local_dim.y * local_dim.z;
	// amount of work-items (in a group) in-flight (0 when group is done, then reset for every group)
	atomic<uint32_t> items_in_flight { 0 };
	// for group syncing purposes, waited on until all work-items in a group are done
	atomic<uint32_t> group_id { ~0u };
	
	// init barrier vars
	barrier_counter = local_size;
	barrier_gen = 0;
	barrier_users = local_size;
	
	// start worker threads
	vector<unique_ptr<thread>> worker_threads(local_size);
	for(uint32_t local_linear_idx = 0; local_linear_idx < local_size; ++local_linear_idx) {
		worker_threads[local_linear_idx] = make_unique<thread>([&items_in_flight, &group_id,
																local_linear_idx, local_size,
																local_dim, group_dim,
																this, &kernel_func] {
			// local id is fixed for all execution
			const uint3 local_id {
				local_linear_idx % local_dim.x,
				(local_linear_idx / local_dim.x) % local_dim.y,
				local_linear_idx / (local_dim.x * local_dim.y)
			};
			floor_local_idx = local_id;
			
#if defined(FLOOR_DEBUG)
			// set thread name for debugging purposes, shortened as far as possible
			// note that thread name max size is 15 (-2 commas -> 13)
			if((const_math::int_width(local_dim.x - 1) +
				const_math::int_width(local_dim.y - 1) +
				const_math::int_width(local_dim.z - 1)) <= 13) {
				core::set_current_thread_name(to_string(local_id.x) + "," +
											  to_string(local_id.y) + "," +
											  to_string(local_id.z));
			}
			else core::set_current_thread_name("#" + to_string(local_linear_idx));
#endif
			
			// iterate over groups - note that the group id is always identical for all threads,
			// as a single group item is worked by all worker threads (before continuing)
			uint32_t linear_group_id = 0;
			for(uint32_t group_x = 0; group_x < group_dim.x; ++group_x) {
				for(uint32_t group_y = 0; group_y < group_dim.y; ++group_y) {
					for(uint32_t group_z = 0; group_z < group_dim.z; ++group_z, ++linear_group_id) {
						// last thread is responsible for sync
						if(local_linear_idx == local_size - 1) {
							// wait until all prior work-items are done
							while(items_in_flight != 0) {
								this_thread::yield();
							}
							
							// reset + signal that group is ready for execution
							items_in_flight = local_size;
							group_id = linear_group_id;
						}
						else {
							// wait until group init is done
							while(group_id != linear_group_id) {
								this_thread::yield();
							}
						}
						
						// setup group
						floor_group_idx = { group_x, group_y, group_z };
						
						// compute global id for this work-item
						const uint3 global_id {
							group_x * local_dim.x + local_id.x,
							group_y * local_dim.y + local_id.y,
							group_z * local_dim.z + local_id.z
						};
						floor_global_idx = global_id;
						
						// finally: execute work-item
						kernel_func();
						
						// work-item done
						--items_in_flight;
					}
				}
			}
		});
	}
	// wait for worker threads to finish
	for(auto& item : worker_threads) {
		item->join();
	}
#elif defined(FLOOR_HOST_COMPUTE_MT_GROUP)
	// #work-groups
	const auto group_count = group_dim.x * group_dim.y * group_dim.z;
	// #work-items per group
	const uint32_t local_size = local_dim.x * local_dim.y * local_dim.z;
	// group ticketing system, each worker thread will grab a new group id, once it's done with one group
	atomic<uint32_t> group_idx { 0 };
	
	// start worker threads
#if defined(FLOOR_HOST_KERNEL_ENABLE_TIMING)
	const auto time_start = floor_timer2::start();
#endif
	vector<unique_ptr<thread>> worker_threads(cpu_count);
	for(uint32_t cpu_idx = 0; cpu_idx < cpu_count; ++cpu_idx) {
		worker_threads[cpu_idx] = make_unique<thread>([cpu_idx, &kernel_func,
													   &group_idx, group_count, group_dim,
													   local_size, local_dim] {
			// sanity check (mostly necessary on os x where some fool had the idea to make the size of ucontext_t define dependent)
			static_assert(sizeof(ucontext_t) > 64, "ucontext_t should not be this small, something is wrong!");
			
			// set cpu affinity for this thread to a particular cpu to prevent this thread from being constantly moved/scheduled
			// on different cpus (starting at index 1, with 0 representing no affinity)
			floor_set_thread_affinity(cpu_idx + 1);
			
			// init contexts (aka fibers)
			fiber_context main_ctx;
			main_ctx.init(nullptr, 0, nullptr, ~0u, nullptr);
			auto items = make_unique<fiber_context[]>(local_size);
			item_contexts = items.get();
			
			// 4k stack should be enough, considering this runs on gpus
			// TODO: stack protection?
			static constexpr const size_t item_stack_size { fiber_context::min_stack_size };
			uint8_t* item_stacks = new uint8_t[item_stack_size * local_size] alignas(128);
			
			// init fibers
			for(uint32_t i = 0; i < local_size; ++i) {
				items[i].init(&item_stacks[i * item_stack_size], item_stack_size,
							  run_mt_group_item, i,
							  // continue with next on return, or return to main ctx when the last item returns
							  // TODO: add option to use randomized order?
							  (i + 1 < local_size ? &items[i + 1] : &main_ctx));
			}
			
			for(;;) {
				// assign a new group to this thread/cpu and check if we're done
				const auto group_linear_idx = group_idx++;
				if(group_linear_idx >= group_count) break;
				
				// setup group
				const uint3 group_id {
					group_linear_idx % group_dim.x,
					(group_linear_idx / group_dim.x) % group_dim.y,
					group_linear_idx / (group_dim.x * group_dim.y)
				};
				floor_group_idx = group_id;
				
				// reset fibers
				for(uint32_t i = 0; i < local_size; ++i) {
					items[i].reset();
				}
				
				// run fibers/work-items for this group
				static _Thread_local volatile bool done;
				done = false;
				main_ctx.get_context();
				if(!done) {
					done = true;
					
					// start first fiber
					items[0].set_context();
				}
			}
			
			delete [] item_stacks; // TODO: better use unique_ptr
		});
	}
	// wait for worker threads to finish
	for(auto& item : worker_threads) {
		item->join();
	}
#if defined(FLOOR_HOST_KERNEL_ENABLE_TIMING)
	log_debug("kernel time: %ums", double(floor_timer2::stop<chrono::microseconds>(time_start)) / 1000.0);
#endif
#endif
}

extern "C" void run_mt_group_item(const uint32_t local_linear_idx) {
	// set local + global id
	const uint3 local_id {
		local_linear_idx % floor_local_work_size.x,
		(local_linear_idx / floor_local_work_size.x) % floor_local_work_size.y,
		local_linear_idx / (floor_local_work_size.x * floor_local_work_size.y)
	};
	floor_local_idx = local_id;
	item_local_linear_idx = local_linear_idx;
	
	const uint3 global_id {
		floor_group_idx.x * floor_local_work_size.x + local_id.x,
		floor_group_idx.y * floor_local_work_size.y + local_id.y,
		floor_group_idx.z * floor_local_work_size.z + local_id.z
	};
	floor_global_idx = global_id;
	
	// execute work-item / kernel function
	(*cur_kernel_function)();
}

// id handling implementation
#include <floor/compute/device/host.hpp>
size_t get_global_id(uint32_t dimindx) {
	if(dimindx >= floor_work_dim) return 0;
	return floor_global_idx[dimindx];
}
size_t get_global_size(uint32_t dimindx) {
	if(dimindx >= floor_work_dim) return 1;
	return floor_global_work_size[dimindx];
}
size_t get_local_id(uint32_t dimindx) {
	if(dimindx >= floor_work_dim) return 0;
	return floor_local_idx[dimindx];
}
size_t get_local_size(uint32_t dimindx) {
	if(dimindx >= floor_work_dim) return 1;
	return floor_local_work_size[dimindx];
}
size_t get_group_id(uint32_t dimindx) {
	if(dimindx >= floor_work_dim) return 0;
	return floor_group_idx[dimindx];
}
size_t get_num_groups(uint32_t dimindx) {
	if(dimindx >= floor_work_dim) return 1;
	return floor_group_size[dimindx];
}
uint32_t get_work_dim() {
	return floor_work_dim;
}

// barrier handling (all the same)
void global_barrier() {
#if defined(FLOOR_HOST_COMPUTE_MT_ITEM)
	// save current barrier generation/id
	const uint32_t cur_gen = barrier_gen;
	
	// dec counter, and:
	if(--barrier_counter == 0) {
		// if this is the last thread to encounter the barrier,
		// reset the counter and increase the gen/id, so that the other threads can continue
		barrier_counter = barrier_users;
		++barrier_gen; // note: overflow doesn't matter
	}
	else {
		// if this isn't the last thread to encounter the barrier,
		// wait until the barrier gen/id changes, then continue
		while(cur_gen == barrier_gen) {
			this_thread::yield();
		}
	}
#elif defined(FLOOR_HOST_COMPUTE_MT_GROUP)
	// save indices, switch to next fiber and restore indices again
	const auto saved_global_id = floor_global_idx;
	const auto saved_local_id = floor_local_idx;
	const auto save_item_local_linear_idx = item_local_linear_idx;
	
	fiber_context* this_ctx = &item_contexts[item_local_linear_idx];
	fiber_context* next_ctx = &item_contexts[(item_local_linear_idx + 1u) % floor_linear_local_work_size];
	this_ctx->swap_context(next_ctx);
	
	item_local_linear_idx = save_item_local_linear_idx;
	floor_local_idx = saved_local_id;
	floor_global_idx = saved_global_id;
#endif
}
void global_mem_fence() {
	global_barrier();
}
void global_read_mem_fence() {
	global_barrier();
}
void global_write_mem_fence() {
	global_barrier();
}
void local_barrier() {
	global_barrier();
}
void local_mem_fence() {
	global_barrier();
}
void local_read_mem_fence() {
	global_barrier();
}
void local_write_mem_fence() {
	global_barrier();
}

#endif
