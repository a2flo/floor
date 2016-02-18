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

#ifndef __FLOOR_VULKAN_KERNEL_HPP__
#define __FLOOR_VULKAN_KERNEL_HPP__

#include <floor/compute/vulkan/vulkan_common.hpp>

#if !defined(FLOOR_NO_VULKAN)

#include <floor/core/logger.hpp>
#include <floor/threading/atomic_spin_lock.hpp>
#include <floor/compute/vulkan/vulkan_buffer.hpp>
#include <floor/compute/vulkan/vulkan_image.hpp>

// the amount of macro voodoo is too damn high ...
#define FLOOR_VULKAN_KERNEL_IMPL 1
#include <floor/compute/compute_kernel.hpp>
#undef FLOOR_VULKAN_KERNEL_IMPL

class vulkan_device;
class vulkan_kernel final : public compute_kernel {
public:
	struct vulkan_kernel_entry : kernel_entry {
		// TODO: object / full compute pipeline probably?
		void* kernel { nullptr };
	};
	typedef flat_map<vulkan_device*, vulkan_kernel_entry> kernel_map_type;
	
	vulkan_kernel(kernel_map_type&& kernels);
	~vulkan_kernel() override;
	
	template <typename... Args> void execute(compute_queue* queue,
											 const uint32_t work_dim,
											 const uint3 global_work_size,
											 const uint3 local_work_size_,
											 Args&&... args) {
		// find entry for queue device
		const auto kernel_iter = get_kernel(queue);
		if(kernel_iter == kernels.cend()) {
			log_error("no kernel for this compute queue/device exists!");
			return;
		}
		
		// check work size
		const uint3 local_work_size = check_local_work_size(kernel_iter->second, local_work_size_);
		
		// set and handle kernel arguments
		set_kernel_arguments<0>(kernel_iter->second, forward<Args>(args)...);
		
		// run
		execute_internal(queue, kernel_iter->second, work_dim, global_work_size, local_work_size);
	}
	
	const kernel_entry* get_kernel_entry(shared_ptr<compute_device> dev) const override {
		const auto ret = kernels.get((vulkan_device*)dev.get());
		return !ret.first ? nullptr : &ret.second->second;
	}
	
protected:
	const kernel_map_type kernels;
	flat_map<const kernel_entry*, bool> warn_map;
	
	typename kernel_map_type::const_iterator get_kernel(const compute_queue* queue) const;
	
	COMPUTE_TYPE get_compute_type() const override { return COMPUTE_TYPE::VULKAN; }
	
	void execute_internal(compute_queue* queue,
						  const vulkan_kernel_entry& entry,
						  const uint32_t& work_dim,
						  const uint3& global_work_size,
						  const uint3& local_work_size) const;
	
	//! handle kernel call terminator
	template <uint32_t num>
	floor_inline_always void set_kernel_arguments(const vulkan_kernel_entry&) const {}
	
	//! set kernel argument and recurse
	template <uint32_t num, typename T, typename... Args>
	floor_inline_always void set_kernel_arguments(const vulkan_kernel_entry& entry,
												  T&& arg, Args&&... args) const {
		set_kernel_argument(entry, num, forward<T>(arg));
		set_kernel_arguments<num + 1>(entry, forward<Args>(args)...);
	}
	
	//! actual kernel argument setter
	template <typename T>
	floor_inline_always void set_kernel_argument(const vulkan_kernel_entry& entry floor_unused,
												 const uint32_t num floor_unused, T&& arg floor_unused) const {
		// TODO: implement this
	}
	
	floor_inline_always void set_kernel_argument(const vulkan_kernel_entry& entry floor_unused,
												 const uint32_t num floor_unused, shared_ptr<compute_buffer> arg floor_unused) const {
		// TODO: implement this
	}
	
	floor_inline_always void set_kernel_argument(const vulkan_kernel_entry& entry floor_unused,
												 const uint32_t num floor_unused, shared_ptr<compute_image> arg floor_unused) const {
		// TODO: implement this
	}
	
};

#endif

#endif
