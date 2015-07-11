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

// host compute exeuction model, choose wisely:

// single-threaded, one logical cpu (the calling thread) corresponding to all work-items and work-groups
// NOTE: no parallelism
//#define FLOOR_HOST_COMPUTE_ST 1

// multi-threaded, each logical cpu ("h/w thread") corresponding to one work-item in a work-group
// NOTE: has intra-group parallelism, has no inter-group parallelism
// NOTE: no fibers, barriers are sync'ed through spin locking
//#define FLOOR_HOST_COMPUTE_MT_ITEM 1

// multi-threaded, each logical cpu ("h/w thread") corresponding to multiple work-items in a work-group
// NOTE: has intra-group parallelism, has no inter-group parallelism
// NOTE: uses fibers when encountering a barrier, running all fibers up to the barrier, then spin lock sync
//#define FLOOR_HOST_COMPUTE_MT_ITEM_FIBERED 1

// multi-threaded, each logical cpu ("h/w thread") corresponding to one work-group
// NOTE: has no intra-group parallelism, has inter-group parallelism
// NOTE: uses fibers when encountering a barrier, running all fibers up to the barrier, then continuing
#define FLOOR_HOST_COMPUTE_MT_GROUP 1

// multi-threaded, each logical cpu ("h/w thread") corresponding to multiple work-items in multiple work-groups
// NOTE: has intra-group parallelism and potentially inter-group parallelism
// NOTE: uses fibers when encountering a barrier, running all fibers up to the barrier in all work-groups,
// then spin lock sync (TODO: +could continue as soon as one barrier has completed)
//#define FLOOR_HOST_COMPUTE_MT_MIXED 1

// the amount of macro voodoo is too damn high ...
#define FLOOR_HOST_KERNEL_IMPL 1
#include <floor/compute/compute_kernel.hpp>
#undef FLOOR_HOST_KERNEL_IMPL

class host_kernel final : public compute_kernel {
public:
	host_kernel(const void* kernel, const string& func_name);
	~host_kernel() override;
	
	template <typename... Args> void execute(compute_queue* queue,
											 const uint32_t work_dim,
											 const size3 global_work_size,
											 const size3 local_work_size,
											 Args&&... args) {
		execute_internal(queue, work_dim, global_work_size, local_work_size,
						 [this, args...] { (*kernel)(handle_kernel_arg(args)...); });
	}
	
protected:
	typedef void (*kernel_func_type)(...);
	const kernel_func_type kernel;
	const string func_name;
	
	COMPUTE_TYPE get_compute_type() const override { return COMPUTE_TYPE::HOST; }
	
	template <typename T> void* handle_kernel_arg(T&& obj) { return (void*)&obj; }
	void* handle_kernel_arg(shared_ptr<compute_buffer> buffer);
	struct host_image_arg { void* ptr; uint4 dim; };
	host_image_arg handle_kernel_arg(shared_ptr<compute_image> buffer);
	
	void execute_internal(compute_queue* queue,
						  const uint32_t work_dim,
						  const size3 global_work_size,
						  const size3 local_work_size,
						  const function<void()>& kernel_func);
	
};

#endif

#endif
