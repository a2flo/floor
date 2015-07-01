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

// TODO: better way of doing this?
extern void floor_setup_host_exec(const uint32_t& dim,
								  const size3& global_work_size,
								  const size3& local_work_size);

// the amount of macro voodoo is too damn high ...
#define FLOOR_HOST_KERNEL_IMPL 1
#include <floor/compute/compute_kernel.hpp>
#undef FLOOR_HOST_KERNEL_IMPL

class host_kernel final : public compute_kernel {
public:
	host_kernel(const void* kernel, const string& func_name);
	~host_kernel() override;
	
	template <typename... Args> void execute(compute_queue* queue floor_unused,
											 const uint32_t work_dim,
											 const size3 global_work_size,
											 const size3 local_work_size,
											 Args&&... args) {
		// TODO: implement this!
		// TODO: move to internal function so it doesn't get emitted/instantiated for every exec call
		
		// setup id handling
		floor_setup_host_exec(work_dim, global_work_size, local_work_size);
		
		//
		const uint3 local_dim { local_work_size.maxed(1u) };
		const uint3 group_dim_overflow {
			global_work_size.x > 0 ? std::min(uint32_t(global_work_size.x % local_dim.x), 1u) : 0u,
			global_work_size.y > 0 ? std::min(uint32_t(global_work_size.y % local_dim.y), 1u) : 0u,
			global_work_size.z > 0 ? std::min(uint32_t(global_work_size.z % local_dim.z), 1u) : 0u
		};
		uint3 group_dim { (global_work_size / local_dim) + group_dim_overflow };
		group_dim.max(1u);
		
		// it's usually best to go from largest to smallest loop count (usually: X > Y > Z)
		for(uint32_t group_x = 0; group_x < group_dim.x; ++group_x) {
			for(uint32_t group_y = 0; group_y < group_dim.y; ++group_y) {
				for(uint32_t group_z = 0; group_z < group_dim.z; ++group_z) {
				}
			}
		}
	}
	
protected:
	const void* kernel;
	const string func_name;
	
	COMPUTE_TYPE get_compute_type() const override { return COMPUTE_TYPE::HOST; }
	
};

#endif

#endif
