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

#include <floor/compute/opencl/opencl_kernel.hpp>

#if !defined(FLOOR_NO_OPENCL)

#include <floor/compute/compute_queue.hpp>
#include <floor/compute/opencl/opencl_common.hpp>

opencl_kernel::opencl_kernel(kernel_map_type&& kernels_) : kernels(move(kernels_)) {
}

opencl_kernel::~opencl_kernel() {}

typename opencl_kernel::kernel_map_type::const_iterator opencl_kernel::get_kernel(const compute_queue* queue) const {
	return kernels.find((opencl_device*)queue->get_device().get());
}

void opencl_kernel::execute_internal(compute_queue* queue,
									 const kernel_entry& entry,
									 const uint32_t& work_dim,
									 const uint3& global_work_size,
									 const uint3& local_work_size) {
	const size3 global_ws { global_work_size };
	const size3 local_ws { local_work_size };
	
	CL_CALL_RET(clEnqueueNDRangeKernel((cl_command_queue)queue->get_queue_ptr(),
									   entry.kernel, work_dim, nullptr,
									   global_ws.data(), local_ws.data(),
									   // TODO: use of event stuff?
									   0, nullptr, nullptr),
				"failed to execute kernel")
}

#endif
