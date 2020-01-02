/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2020 Florian Ziesche
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
#include <floor/compute/compute_kernel.hpp>

// host compute exeuction model, choose wisely:

// single-threaded, one logical cpu (the calling thread) corresponding to all work-items and work-groups
// NOTE: no parallelism
//#define FLOOR_HOST_COMPUTE_ST 1

// multi-threaded, each logical cpu ("h/w thread") corresponding to one work-item in a work-group
// NOTE: has intra-group parallelism, has no inter-group parallelism
// NOTE: no fibers, barriers are sync'ed through spin locking
//#define FLOOR_HOST_COMPUTE_MT_ITEM 1

// multi-threaded, each logical cpu ("h/w thread") corresponding to one work-group
// NOTE: has no intra-group parallelism, has inter-group parallelism
// NOTE: uses fibers when encountering a barrier, running all fibers up to the barrier, then continuing
#define FLOOR_HOST_COMPUTE_MT_GROUP 1

class host_kernel final : public compute_kernel {
public:
	host_kernel(const void* kernel, const string& func_name, compute_kernel::kernel_entry&& entry);
	~host_kernel() override = default;
	
	void execute(const compute_queue& cqueue,
				 const bool& is_cooperative,
				 const uint32_t& dim,
				 const uint3& global_work_size,
				 const uint3& local_work_size,
				 const vector<compute_kernel_arg>& args) const override;
	
	const kernel_entry* get_kernel_entry(const compute_device&) const override {
		return &entry; // can't really check if the device is correct here
	}
	
protected:
	typedef void (*kernel_func_type)(...);
	const kernel_func_type kernel;
	const string func_name;
	const compute_kernel::kernel_entry entry;
	
	COMPUTE_TYPE get_compute_type() const override { return COMPUTE_TYPE::HOST; }
	
	void execute_internal(const compute_queue& cqueue,
						  const uint32_t work_dim,
						  const uint3 global_work_size,
						  const uint3 local_work_size) const;
	
};

#endif

#endif
