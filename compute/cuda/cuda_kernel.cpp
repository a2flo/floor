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

#include <floor/compute/cuda/cuda_kernel.hpp>

#if !defined(FLOOR_NO_CUDA)

#include <floor/compute/compute_queue.hpp>

// note that cuda doesn't have any special argument types and everything is just sized "memory"
// -> only need to add up sizes
static size_t compute_kernel_args_size(const llvm_compute::kernel_info& info) {
	size_t ret = 0;
	const auto arg_count = info.args.size();
	for(size_t i = 0; i < arg_count; ++i) {
		// actual arg or pointer?
		if(info.args[i].address_space == llvm_compute::kernel_info::ARG_ADDRESS_SPACE::CONSTANT ||
		   info.args[i].address_space == llvm_compute::kernel_info::ARG_ADDRESS_SPACE::IMAGE) {
			ret += info.args[i].size;
		}
		else ret += sizeof(void*);
	}
	return ret;
}

cuda_kernel::cuda_kernel(const CUfunction kernel_, const llvm_compute::kernel_info& info_) :
kernel(kernel_), func_name(info_.name), kernel_args_size(compute_kernel_args_size(info_)), info(info_) {
}

cuda_kernel::~cuda_kernel() {}

void cuda_kernel::execute_internal(compute_queue* queue,
								   const uint3& grid_dim,
								   const uint3& block_dim,
								   void** kernel_params) {
	CU_CALL_NO_ACTION(cuLaunchKernel(kernel,
									 grid_dim.x, grid_dim.y, grid_dim.z,
									 block_dim.x, block_dim.y, block_dim.z,
									 0,
									 (CUstream)queue->get_queue_ptr(),
									 kernel_params,
									 nullptr),
					  "failed to execute kernel");
}

#endif
