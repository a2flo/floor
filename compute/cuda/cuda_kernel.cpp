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

cuda_kernel::cuda_kernel(const CUfunction kernel_, const llvm_compute::kernel_info& info) :
kernel(kernel_), func_name(info.name) {
}

cuda_kernel::~cuda_kernel() {}

void cuda_kernel::execute_internal(compute_queue* queue,
								   const uint3& grid_dim,
								   const uint3& block_dim,
								   void** kernel_params) {
	CU_CALL_NO_ACTION(cuLaunchKernel(kernel,
									 grid_dim.x, grid_dim.y, grid_dim.z,
									 block_dim.x, block_dim.y, block_dim.z,
									 0, // TODO: make sharedMemBytes specifiable
									 (CUstream)queue->get_queue_ptr(),
									 kernel_params,
									 nullptr),
					  "failed to execute kernel");
}

#endif
