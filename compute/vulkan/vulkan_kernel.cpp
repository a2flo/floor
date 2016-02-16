/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2016 Florian Ziesche
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

#include <floor/compute/vulkan/vulkan_kernel.hpp>

#if !defined(FLOOR_NO_VULKAN)

#include <floor/compute/compute_queue.hpp>
#include <floor/compute/vulkan/vulkan_common.hpp>

vulkan_kernel::vulkan_kernel(kernel_map_type&& kernels_) : kernels(move(kernels_)) {
}

vulkan_kernel::~vulkan_kernel() {}

typename vulkan_kernel::kernel_map_type::const_iterator vulkan_kernel::get_kernel(const compute_queue* queue) const {
	return kernels.find((vulkan_device*)queue->get_device().get());
}

void vulkan_kernel::execute_internal(compute_queue* queue,
									 const vulkan_kernel_entry& entry,
									 const uint32_t& work_dim,
									 const uint3& global_work_size,
									 const uint3& local_work_size) const {
	const size3 global_ws { global_work_size };
	const size3 local_ws { local_work_size };
	
	// TODO: implement this
}

#endif
