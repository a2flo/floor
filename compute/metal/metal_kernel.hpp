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

#pragma once

#include <floor/compute/metal/metal_common.hpp>

#if !defined(FLOOR_NO_METAL)

#include <floor/core/logger.hpp>
#include <floor/threading/atomic_spin_lock.hpp>
#include <floor/compute/compute_buffer.hpp>
#include <floor/compute/llvm_toolchain.hpp>
#include <floor/compute/compute_kernel.hpp>

class metal_buffer;
class metal_device;

class metal_kernel : public compute_kernel {
public:
	struct metal_kernel_entry : kernel_entry {
		const void* kernel { nullptr };
		const void* kernel_state { nullptr };
		bool supports_indirect_compute { false };
	};
	typedef floor_core::flat_map<const metal_device&, metal_kernel_entry> kernel_map_type;
	
	metal_kernel(const string_view kernel_name_, kernel_map_type&& kernels);
	~metal_kernel() override = default;
	
	void execute(const compute_queue& cqueue,
				 const bool& is_cooperative,
				 const bool& wait_until_completion,
				 const uint32_t& dim,
				 const uint3& global_work_size,
				 const uint3& local_work_size,
				 const vector<compute_kernel_arg>& args,
				 const vector<const compute_fence*>& wait_fences,
				 const vector<compute_fence*>& signal_fences,
				 const char* debug_label,
				 kernel_completion_handler_f&& completion_handler) const override;
	
	const kernel_entry* get_kernel_entry(const compute_device& dev) const override;
	
	//! helper function to compute the Metal grid dim ("#threadgroups") and block dim ("threads per threadgroup")
	pair<uint3, uint3> compute_grid_and_block_dim(const kernel_entry& entry,
												  const uint32_t& dim,
												  const uint3& global_work_size,
												  const uint3& local_work_size) const;
	
protected:
	const kernel_map_type kernels;
	
	COMPUTE_TYPE get_compute_type() const override { return COMPUTE_TYPE::METAL; }
	
	typename kernel_map_type::const_iterator get_kernel(const compute_queue& queue) const;
	
	unique_ptr<argument_buffer> create_argument_buffer_internal(const compute_queue& cqueue,
																const kernel_entry& entry,
																const llvm_toolchain::arg_info& arg,
																const uint32_t& user_arg_index,
																const uint32_t& ll_arg_index,
																const COMPUTE_MEMORY_FLAG& add_mem_flags) const override;
	
};

#endif
