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

#if defined(__clang__) // ignore warnings about deprecated functions
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#endif

// id handling vars
static uint32_t floor_work_dim { 1u };
static size3 floor_global_work_size;
static size3 floor_local_work_size;
static size3 floor_group_size;
static _Thread_local size3 floor_global_idx;
static _Thread_local size3 floor_local_idx;
static _Thread_local size3 floor_group_idx;

// barrier handling vars
// -> mt-item
static atomic<uint32_t> barrier_counter { 0 };
static atomic<uint32_t> barrier_gen { 0 };
static uint32_t barrier_users { 0 };
// -> mt-group
static _Thread_local ucontext_t* this_ctx, *next_ctx;

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

uint32_t host_kernel::get_cpu_count(const compute_queue* queue) {
	return ((host_queue*)queue)->get_device()->units;
}

extern "C" void setup_mt_group_ctx(int32_t idx);
void host_kernel::execute_internal(compute_queue* queue,
								   const uint32_t work_dim,
								   const size3 global_work_size,
								   const size3 local_work_size,
								   const function<void()>& kernel_func) {
	
	//
	const auto cpu_count = min(get_cpu_count(queue), 1u);
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
	vector<unique_ptr<thread>> worker_threads(cpu_count);
	for(uint32_t cpu_idx = 0; cpu_idx < cpu_count; ++cpu_idx) {
		worker_threads[cpu_idx] = make_unique<thread>([cpu_idx, &kernel_func,
													   &group_idx, group_count, group_dim,
													   local_size, local_dim] {
			// sanity check (mostly necessary on os x where some fool had the idea to make the size of ucontext_t define dependent)
			static_assert(sizeof(ucontext_t) > 64, "ucontext_t should not be this small, something is wrong!");
			
			// init contexts (aka fibers)
			ucontext_t main_ctx;
			memset(&main_ctx, 0, sizeof(ucontext_t));
			
			vector<ucontext_t> items(local_size);
			memset(&items[0], 0, local_size * sizeof(ucontext_t));
			
			// 32k stack should be enough, considering this runs on gpus (also: this is the minimum stack size on os x)
			static constexpr const size_t item_stack_size { 32768 };
			static constexpr const size_t max_item_count { 128 }; // TODO: get/set this from host_compute/host_device
			uint8_t* item_stacks = new uint8_t[item_stack_size * max_item_count] alignas(128);
			
			//
			static _Thread_local volatile bool done;
			done = false;
			getcontext((ucontext_t*)&main_ctx); // must init this before using it
			if(!done) {
				done = true;
				
				// must init these in reverse order due to makecontext depending on a valid uc_link/ucontext
				for(uint32_t i = 0; i < local_size; ++i) {
					ucontext_t* ctx = &items[i];
					getcontext(ctx);
					// continue with next on return, or return to main ctx when the last item returns
					ctx->uc_link = (i + 1 < local_size ? &items[i + 1] : &main_ctx);
					ctx->uc_stack.ss_size = item_stack_size;
					ctx->uc_stack.ss_sp = &item_stacks[i * item_stack_size];
					makecontext(ctx, (void (*)())setup_mt_group_ctx, 1, i);
				}
				logger::flush();
				
				// start first
				setcontext(&items[0]);
			}
			
			delete [] item_stacks; // TODO: better use unique_ptr
			
#if 0
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
				floor_host_exec_get_group() = group_id;
				
				// iterate over all work-items in this group
				for(uint32_t local_linear_idx = 0; local_linear_idx < local_size; ++local_linear_idx) {
					// set local + global id
					const uint3 local_id {
						local_linear_idx % local_dim.x,
						(local_linear_idx / local_dim.x) % local_dim.y,
						local_linear_idx / (local_dim.x * local_dim.y)
					};
					floor_host_exec_get_local() = local_id;
					
					const uint3 global_id {
						group_id.x * local_dim.x + local_id.x,
						group_id.y * local_dim.y + local_id.y,
						group_id.z * local_dim.z + local_id.z
					};
					floor_host_exec_get_global() = global_id;
					
					// finally: execute work-item
					kernel_func();
				}
			}
#endif
		});
	}
	// wait for worker threads to finish
	for(auto& item : worker_threads) {
		item->join();
	}
#endif
}

extern "C" void setup_mt_group_ctx(int32_t idx) {
	printf("item: %d\n", idx); fflush(stdout);
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
	// TODO: store + restore indices (+other stuff?)
	swapcontext(this_ctx, next_ctx);
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
