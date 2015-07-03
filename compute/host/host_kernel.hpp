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

#ifndef __FLOOR_HOST_KERNEL_HPP__
#define __FLOOR_HOST_KERNEL_HPP__

#include <floor/compute/host/host_common.hpp>

#if !defined(FLOOR_NO_HOST_COMPUTE)

#include <floor/core/core.hpp>
#include <floor/core/logger.hpp>
#include <floor/threading/atomic_spin_lock.hpp>
#include <floor/threading/task.hpp>

#include <floor/core/timer.hpp> // TODO: remove this again

// TODO: better way of doing this?
void floor_host_exec_setup(const uint32_t& dim,
						   const size3& global_work_size,
						   const uint3& local_work_size);
size3& floor_host_exec_get_global();
size3& floor_host_exec_get_local();
size3& floor_host_exec_get_group();
atomic<uint32_t>& floor_host_exec_get_barrier_counter();
atomic<uint32_t>& floor_host_exec_get_barrier_gen();
uint32_t& floor_host_exec_get_barrier_users();

// the amount of macro voodoo is too damn high ...
#define FLOOR_HOST_KERNEL_IMPL 1
#include <floor/compute/compute_kernel.hpp>
#undef FLOOR_HOST_KERNEL_IMPL

class host_kernel final : public compute_kernel {
public:
	host_kernel(const void* kernel, const string& func_name);
	~host_kernel() override;
	
	template <typename... Args> void execute(compute_queue* queue floor_unused,
											 const uint32_t work_dim,
											 const size3 global_work_size,
											 const size3 local_work_size,
											 Args&&... args) {
		//
		const uint3 local_dim { local_work_size.maxed(1u) };
		const uint3 group_dim_overflow {
			global_work_size.x > 0 ? std::min(uint32_t(global_work_size.x % local_dim.x), 1u) : 0u,
			global_work_size.y > 0 ? std::min(uint32_t(global_work_size.y % local_dim.y), 1u) : 0u,
			global_work_size.z > 0 ? std::min(uint32_t(global_work_size.z % local_dim.z), 1u) : 0u
		};
		uint3 group_dim { (global_work_size / local_dim) + group_dim_overflow };
		group_dim.max(1u);
		
		// setup id handling
		floor_host_exec_setup(work_dim, global_work_size, local_dim);
		
		auto tstart = floor_timer2::start();
#if 0 // single-threaded
		// it's usually best to go from largest to smallest loop count (usually: X > Y > Z)
		size3& global_idx = floor_host_exec_get_global();
		size3& local_idx = floor_host_exec_get_local();
		size3& group_idx = floor_host_exec_get_group();
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
								(*kernel)(handle_kernel_arg(args)...);
							}
						}
					}
				}
			}
		}
#else
		// #work-items per group
		const uint32_t local_size = local_dim.x * local_dim.y * local_dim.z;
		// amount of work-items (in a group) in-flight (0 when group is done, then reset for every group)
		atomic<uint32_t> items_in_flight { 0 };
		// for group syncing purposes, waited on until all work-items in a group are done
		atomic<uint32_t> group_id { ~0u };
		
		// init barrier vars
		floor_host_exec_get_barrier_counter() = local_size;
		floor_host_exec_get_barrier_gen() = 0;
		floor_host_exec_get_barrier_users() = local_size;
		
		// start worker threads
		vector<unique_ptr<thread>> worker_threads(local_size);
		for(uint32_t local_linear_idx = 0; local_linear_idx < local_size; ++local_linear_idx) {
			worker_threads[local_linear_idx] = make_unique<thread>([&items_in_flight, &group_id,
																	local_linear_idx, local_size,
																	local_dim, group_dim,
																	this, args...] {
				// local id is fixed for all execution
				const uint3 local_id {
					local_linear_idx % local_dim.x,
					(local_linear_idx / local_dim.x) % local_dim.y,
					local_linear_idx / (local_dim.x * local_dim.y)
				};
				floor_host_exec_get_local() = local_id;
				
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
							floor_host_exec_get_group() = { group_x, group_y, group_z };
							
							// compute global id for this work-item
							const uint3 global_id {
								group_x * local_dim.x + local_id.x,
								group_y * local_dim.y + local_id.y,
								group_z * local_dim.z + local_id.z
							};
							floor_host_exec_get_global() = global_id;
							
							// finally: execute work-item
							(*kernel)(handle_kernel_arg(args)...);
							
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
#endif
		log_debug("time: %ums", double(floor_timer2::stop<chrono::microseconds>(tstart)) / 1000.0);
	}
	
protected:
	typedef void (*kernel_func_type)(...);
	const kernel_func_type kernel;
	const string func_name;
	
	COMPUTE_TYPE get_compute_type() const override { return COMPUTE_TYPE::HOST; }
	
	template <typename T> auto handle_kernel_arg(T&& obj) { return &obj; }
	uint8_t* handle_kernel_arg(shared_ptr<compute_buffer> buffer);
	uint8_t* handle_kernel_arg(shared_ptr<compute_image> buffer);
	
};

#endif

#endif
