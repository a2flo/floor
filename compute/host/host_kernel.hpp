/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2024 Florian Ziesche
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

class host_device;
class elf_binary;
struct kernel_func_wrapper;

// host kernel execution implementation:
// multi-threaded, each logical cpu ("h/w thread") corresponds to one work-group
// NOTE: has no intra-group parallelism, but has inter-group parallelism
// NOTE: uses fibers when encountering a barrier, running all fibers up to the barrier, then continuing
class host_kernel final : public compute_kernel {
public:
	struct host_kernel_entry : kernel_entry {
		shared_ptr<elf_binary> program;
	};
	using kernel_map_type = floor_core::flat_map<const host_device&, host_kernel_entry>;
	
	//! constructor for kernels built using the host compiler / vanilla toolchain
	host_kernel(const void* kernel, const string& func_name, compute_kernel::kernel_entry&& entry);
	//! constructor for kernels built using the floor host-compute device toolchain
	host_kernel(kernel_map_type&& kernels);
	~host_kernel() override = default;
	
	void execute(const compute_queue& cqueue,
				 const bool& is_cooperative,
				 const bool& wait_until_completion,
				 const uint32_t& work_dim,
				 const uint3& global_work_size,
				 const uint3& local_work_size,
				 const vector<compute_kernel_arg>& args,
				 const vector<const compute_fence*>& wait_fences,
				 const vector<compute_fence*>& signal_fences,
				 const char* debug_label,
				 kernel_completion_handler_f&& completion_handler) const override;
	
	const kernel_entry* get_kernel_entry(const compute_device&) const override;
	
protected:
	const void* kernel { nullptr };
	const string func_name;
	const compute_kernel::kernel_entry entry;
	
	const kernel_map_type kernels {};
	
	COMPUTE_TYPE get_compute_type() const override { return COMPUTE_TYPE::HOST; }
	
	//! host-compute "host" execution
	void execute_host(const kernel_func_wrapper& func,
					  const uint32_t& cpu_count,
					  const uint3& group_dim,
					  const uint3& group_size,
					  const uint3& global_dim,
					  const uint3& local_dim,
					  const uint32_t& work_dim) const;
	
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
																const uint32_t& user_arg_index,
																const uint32_t& ll_arg_index,
																const COMPUTE_MEMORY_FLAG& add_mem_flags) const override;
	
};

//! host-compute device specific barrier
extern "C" void host_compute_device_barrier() FLOOR_HOST_COMPUTE_CC;
//! host-compute device specific printf buffer
extern "C" uint32_t* host_compute_device_printf_buffer() FLOOR_HOST_COMPUTE_CC;

//! host-compute (host) local memory offset retrieval
extern uint32_t floor_host_compute_thread_local_memory_offset_get() FLOOR_HOST_COMPUTE_CC;
//! host-compute (host) global index retrieval
extern uint3 floor_host_compute_global_idx_get() FLOOR_HOST_COMPUTE_CC;
//! host-compute (host) local index retrieval
extern uint3 floor_host_compute_local_idx_get() FLOOR_HOST_COMPUTE_CC;
//! host-compute (host) group index retrieval
extern uint3 floor_host_compute_group_idx_get() FLOOR_HOST_COMPUTE_CC;
//! host-compute (host) work dim retrieval
extern uint32_t floor_host_compute_work_dim_get() FLOOR_HOST_COMPUTE_CC;
//! host-compute (host) global work size/dim retrieval
extern uint3 floor_host_compute_global_work_size_get() FLOOR_HOST_COMPUTE_CC;
//! host-compute (host) local work size/dim retrieval
extern uint3 floor_host_compute_local_work_size_get() FLOOR_HOST_COMPUTE_CC;
//! host-compute (host) group size/dim retrieval
extern uint3 floor_host_compute_group_size_get() FLOOR_HOST_COMPUTE_CC;

#endif

#endif
