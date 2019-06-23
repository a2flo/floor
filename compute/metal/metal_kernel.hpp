/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2019 Florian Ziesche
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

#ifndef __FLOOR_METAL_KERNEL_HPP__
#define __FLOOR_METAL_KERNEL_HPP__

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
	// encapsulates MTLComputeCommandEncoder and MTLCommandBuffer, so we don't need obj-c++ here
	struct metal_encoder;
	
	struct metal_kernel_entry : kernel_entry {
		const void* kernel { nullptr };
		const void* kernel_state { nullptr };
	};
	typedef flat_map<const metal_device&, metal_kernel_entry> kernel_map_type;
	
	metal_kernel(kernel_map_type&& kernels);
	~metal_kernel() override = default;
	
	void execute(const compute_queue& cqueue,
				 const bool& is_cooperative,
				 const uint32_t& dim,
				 const uint3& global_work_size,
				 const uint3& local_work_size,
				 const vector<compute_kernel_arg>& args) const override;
	
	const kernel_entry* get_kernel_entry(const compute_device& dev) const override;
	
protected:
	const kernel_map_type kernels;
	
	COMPUTE_TYPE get_compute_type() const override { return COMPUTE_TYPE::METAL; }
	
	typename kernel_map_type::const_iterator get_kernel(const compute_queue& queue) const;
	
};

#endif

#endif
