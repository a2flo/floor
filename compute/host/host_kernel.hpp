/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2022 Florian Ziesche
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

class host_device;
class elf_binary;

class host_kernel final : public compute_kernel {
public:
	typedef void (*kernel_func_type)(...);
	
	struct host_kernel_entry : kernel_entry {
		shared_ptr<elf_binary> program;
	};
	typedef flat_map<const host_device&, host_kernel_entry> kernel_map_type;
	
	//! constructor for kernels built using the host compiler / vanilla toolchain
	host_kernel(const void* kernel, const string& func_name, compute_kernel::kernel_entry&& entry);
	//! constructor for kernels built using the floor host-compute device toolchain
	host_kernel(kernel_map_type&& kernels);
	~host_kernel() override = default;
	
	void execute(const compute_queue& cqueue,
				 const bool& is_cooperative,
				 const uint32_t& work_dim,
				 const uint3& global_work_size,
				 const uint3& local_work_size,
				 const vector<compute_kernel_arg>& args) const override;
	
	const kernel_entry* get_kernel_entry(const compute_device&) const override;
	
protected:
	const kernel_func_type kernel { nullptr };
	const string func_name;
	const compute_kernel::kernel_entry entry;
	
	const kernel_map_type kernels {};
	
	COMPUTE_TYPE get_compute_type() const override { return COMPUTE_TYPE::HOST; }
	
	//! host-compute "host" execution
	void execute_host(const uint32_t& cpu_count,
					  const uint3& group_dim,
					  const uint3& local_dim) const;
	
	//! host-compute "device" execution
	void execute_device(const host_kernel_entry& func_entry,
						const uint32_t& cpu_count,
						const uint3& group_dim,
						const uint3& local_dim,
						const uint32_t& work_dim,
						const vector<const void*>& vptr_args) const;
	
	typename kernel_map_type::const_iterator get_kernel(const compute_queue& cqueue) const;
	
	unique_ptr<argument_buffer> create_argument_buffer_internal(const compute_queue& cqueue,
																const kernel_entry& entry,
																const llvm_toolchain::arg_info& arg,
																const uint32_t& arg_index) const override;
	
};

//! host-compute device specific barrier
extern "C" void host_compute_device_barrier();

#endif

#endif
