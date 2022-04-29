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

#ifndef __FLOOR_CUDA_KERNEL_HPP__
#define __FLOOR_CUDA_KERNEL_HPP__

#include <floor/compute/cuda/cuda_common.hpp>

#if !defined(FLOOR_NO_CUDA)

#include <floor/core/logger.hpp>
#include <floor/threading/atomic_spin_lock.hpp>
#include <floor/compute/cuda/cuda_buffer.hpp>
#include <floor/compute/cuda/cuda_image.hpp>
#include <floor/compute/llvm_toolchain.hpp>
#include <floor/compute/compute_kernel.hpp>

class cuda_device;

class cuda_kernel final : public compute_kernel {
public:
	struct cuda_kernel_entry : kernel_entry {
		cu_function kernel { nullptr };
		size_t kernel_args_size;
	};
	typedef flat_map<const cuda_device&, cuda_kernel_entry> kernel_map_type;
	
	cuda_kernel(kernel_map_type&& kernels);
	~cuda_kernel() override = default;
	
	void execute(const compute_queue& cqueue,
				 const bool& is_cooperative,
				 const uint32_t& dim,
				 const uint3& global_work_size,
				 const uint3& local_work_size,
				 const vector<compute_kernel_arg>& args,
				 const vector<const compute_fence*>& wait_fences,
				 const vector<const compute_fence*>& signal_fences,
				 const char* debug_label,
				 kernel_completion_handler_f&& completion_handler) const override;
	
	const kernel_entry* get_kernel_entry(const compute_device& dev) const override;
	
protected:
	const kernel_map_type kernels;
	
	COMPUTE_TYPE get_compute_type() const override { return COMPUTE_TYPE::CUDA; }
	
	typename kernel_map_type::const_iterator get_kernel(const compute_queue& cqueue) const;
	
	void execute_internal(const compute_queue& cqueue,
						  const cuda_kernel_entry& entry,
						  const uint3& grid_dim,
						  const uint3& block_dim,
						  void** kernel_params) const;
	
	void execute_cooperative_internal(const compute_queue& cqueue,
									  const cuda_kernel_entry& entry,
									  const uint3& grid_dim,
									  const uint3& block_dim,
									  void** kernel_params) const;
	
	unique_ptr<argument_buffer> create_argument_buffer_internal(const compute_queue& cqueue,
																const kernel_entry& entry,
																const llvm_toolchain::arg_info& arg,
																const uint32_t& user_arg_index,
																const uint32_t& ll_arg_index) const override;
	
};

#endif

#endif
