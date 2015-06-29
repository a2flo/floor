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

#ifndef __FLOOR_HOST_KERNEL_HPP__
#define __FLOOR_HOST_KERNEL_HPP__

#include <floor/compute/host/host_common.hpp>

#if !defined(FLOOR_NO_HOST_COMPUTE)

#include <floor/core/logger.hpp>
#include <floor/threading/atomic_spin_lock.hpp>
#include <floor/compute/host/host_buffer.hpp>
#include <floor/compute/host/host_image.hpp>

// the amount of macro voodoo is too damn high ...
#define FLOOR_HOST_KERNEL_IMPL 1
#include <floor/compute/compute_kernel.hpp>
#undef FLOOR_HOST_KERNEL_IMPL

class host_kernel final : public compute_kernel {
public:
	host_kernel(const void* kernel, const string& func_name);
	~host_kernel() override;
	
	template <typename... Args> void execute(compute_queue* queue,
											 const uint32_t work_dim,
											 const size3 global_work_size,
											 const size3 local_work_size,
											 Args&&... args) {
		// TODO: implement this!
	}
	
protected:
	const void* kernel;
	const string func_name;
	
	COMPUTE_TYPE get_compute_type() const override { return COMPUTE_TYPE::HOST; }
	
};

#endif

#endif
