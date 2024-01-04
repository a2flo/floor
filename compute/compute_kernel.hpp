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

#ifndef __FLOOR_COMPUTE_KERNEL_HPP__
#define __FLOOR_COMPUTE_KERNEL_HPP__

#include <string>
#include <vector>
#include <floor/math/vector_lib.hpp>
#include <floor/compute/compute_common.hpp>
#include <floor/compute/compute_kernel_arg.hpp>
#include <floor/compute/compute_fence.hpp>
#include <floor/compute/compute_memory_flags.hpp>
#include <floor/compute/llvm_toolchain.hpp>
#include <floor/compute/argument_buffer.hpp>
#include <floor/core/flat_map.hpp>
#include <floor/threading/atomic_spin_lock.hpp>

FLOOR_PUSH_WARNINGS()
FLOOR_IGNORE_WARNING(weak-vtables)

class compute_queue;
class compute_buffer;
class compute_image;

class compute_kernel {
public:
	virtual ~compute_kernel() = default;
	
	struct kernel_entry {
		const llvm_toolchain::function_info* info { nullptr };
		uint32_t max_total_local_size { 0u };
		uint3 max_local_size;
	};
	//! returns the internal kernel entry for the specified device
	virtual const kernel_entry* get_kernel_entry(const compute_device& dev) const = 0;
	
	//! don't call this directly, call the execute function in a compute_queue object instead!
	virtual void execute(const compute_queue& cqueue,
						 const bool& is_cooperative,
						 const bool& wait_until_completion,
						 const uint32_t& dim,
						 const uint3& global_work_size,
						 const uint3& local_work_size,
						 const vector<compute_kernel_arg>& args,
						 const vector<const compute_fence*>& wait_fences,
						 const vector<compute_fence*>& signal_fences,
						 const char* debug_label = nullptr,
						 kernel_completion_handler_f&& completion_handler = {}) const = 0;
	
	//! creates an argument buffer for the specified argument index,
	//! "add_mem_flags" may set additional memory flags (already read-write and using host-memory by default)
	//! NOTE: this will perform basic validity checking and automatically compute the necessary buffer size
	virtual unique_ptr<argument_buffer> create_argument_buffer(const compute_queue& cqueue, const uint32_t& arg_index,
															   const COMPUTE_MEMORY_FLAG add_mem_flags = COMPUTE_MEMORY_FLAG::NONE) const;
	
	//! checks the specified local work size against the max local work size in the kernel_entry,
	//! and will compute a proper local work size if the specified one is invalid
	//! NOTE: will only warn/error once per kernel per device
	uint3 check_local_work_size(const kernel_entry& entry,
								const uint3& local_work_size) const REQUIRES(!warn_map_lock);
	
protected:
	//! same as the one in compute_context, but this way we don't need access to that object
	virtual COMPUTE_TYPE get_compute_type() const = 0;
	
	mutable atomic_spin_lock warn_map_lock;
	//! used to prevent console/log spam by remembering if a warning/error has already been printed for a kernel
	mutable floor_core::flat_map<const kernel_entry*, bool> warn_map GUARDED_BY(warn_map_lock);
	
	//! internal function to create the actual argument buffer (should be implemented by backends)
	virtual unique_ptr<argument_buffer> create_argument_buffer_internal(const compute_queue& cqueue,
																		const kernel_entry& entry,
																		const llvm_toolchain::arg_info& arg,
																		const uint32_t& user_arg_index,
																		const uint32_t& ll_arg_index,
																		const COMPUTE_MEMORY_FLAG& add_mem_flags) const;
	
};

FLOOR_POP_WARNINGS()

#endif
