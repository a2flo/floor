/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2018 Florian Ziesche
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

cuda_kernel::cuda_kernel(kernel_map_type&& kernels_) : kernels(move(kernels_)) {
}

cuda_kernel::~cuda_kernel() {}

typename cuda_kernel::kernel_map_type::const_iterator cuda_kernel::get_kernel(const compute_queue* queue) const {
	return kernels.find((cuda_device*)queue->get_device().get());
}

void cuda_kernel::execute_internal(compute_queue* queue,
								   const cuda_kernel_entry& entry,
								   const uint3& grid_dim,
								   const uint3& block_dim,
								   void** kernel_params) {
	CU_CALL_NO_ACTION(cu_launch_kernel(entry.kernel,
									   grid_dim.x, grid_dim.y, grid_dim.z,
									   block_dim.x, block_dim.y, block_dim.z,
									   0,
									   (cu_stream)queue->get_queue_ptr(),
									   kernel_params,
									   nullptr),
					  "failed to execute kernel")
}

void cuda_kernel::execute_cooperative_internal(compute_queue* queue,
											   const cuda_kernel_entry& entry,
											   const uint3& grid_dim,
											   const uint3& block_dim,
											   void** kernel_params) {
	CU_CALL_NO_ACTION(cu_launch_cooperative_kernel(entry.kernel,
												   grid_dim.x, grid_dim.y, grid_dim.z,
												   block_dim.x, block_dim.y, block_dim.z,
												   0,
												   (cu_stream)queue->get_queue_ptr(),
												   kernel_params),
					  "failed to execute cooperative kernel")
}

#endif
