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

#ifndef __FLOOR_METAL_KERNEL_HPP__
#define __FLOOR_METAL_KERNEL_HPP__

#include <floor/compute/metal/metal_common.hpp>

#if !defined(FLOOR_NO_METAL)

#include <floor/core/logger.hpp>
#include <floor/threading/atomic_spin_lock.hpp>
#include <floor/compute/compute_buffer.hpp>
#include <floor/compute/llvm_compute.hpp>

// the amount of macro voodoo is too damn high ...
#define FLOOR_METAL_KERNEL_IMPL 1
#include <floor/compute/compute_kernel.hpp>
#undef FLOOR_METAL_KERNEL_IMPL

class metal_buffer;
class metal_device;
class metal_kernel final : public compute_kernel {
protected:
	// encapsulates MTLComputeCommandEncoder and MTLCommandBuffer, so we don't need obj-c++ here
	struct metal_encoder;
	
public:
	struct kernel_entry {
		const void* kernel { nullptr };
		const void* kernel_state { nullptr };
		const llvm_compute::kernel_info* info;
	};
	typedef flat_map<metal_device*, kernel_entry> kernel_map_type;
	
	metal_kernel(kernel_map_type&& kernels);
	~metal_kernel() override;
	
	template <typename... Args> void execute(compute_queue* queue,
											 const uint32_t work_dim floor_unused,
											 const uint3 global_work_size,
											 const uint3 local_work_size_,
											 Args&&... args) {
		// find entry for queue device
		const auto kernel_iter = get_kernel(queue);
		if(kernel_iter == kernels.cend()) {
			log_error("no kernel for this compute queue/device exists!");
			return;
		}
		
		//
		auto encoder = create_encoder(queue, kernel_iter->second);
		
		// set and handle kernel arguments
		uint32_t total_idx = 0, buffer_idx = 0, texture_idx = 0;
		set_kernel_arguments(encoder.get(), kernel_iter->second,
							 total_idx, buffer_idx, texture_idx, forward<Args>(args)...);
		
		// run
		const uint3 block_dim { local_work_size_.maxed(1u) }; // prevent % or / by 0, also: needs at least 1
		const uint3 grid_dim_overflow {
			global_work_size.x > 0 ? std::min(uint32_t(global_work_size.x % block_dim.x), 1u) : 0u,
			global_work_size.y > 0 ? std::min(uint32_t(global_work_size.y % block_dim.y), 1u) : 0u,
			global_work_size.z > 0 ? std::min(uint32_t(global_work_size.z % block_dim.z), 1u) : 0u
		};
		uint3 grid_dim { (global_work_size / block_dim) + grid_dim_overflow };
		grid_dim.max(1u);
		
		execute_internal(encoder, grid_dim, block_dim);
	}
	
protected:
	const kernel_map_type kernels;
	
	COMPUTE_TYPE get_compute_type() const override { return COMPUTE_TYPE::METAL; }
	
	typename kernel_map_type::const_iterator get_kernel(const compute_queue* queue) const;
	
	// must use shared_ptr, b/c metal_encoder is incomplete at this point
	shared_ptr<metal_encoder> create_encoder(compute_queue* queue,
											 const kernel_entry& entry);
	
	void execute_internal(shared_ptr<metal_encoder> encoder,
						  const uint3& grid_dim,
						  const uint3& block_dim);
	
	//! handle kernel call terminator
	floor_inline_always void set_kernel_arguments(metal_encoder*, const kernel_entry&,
												  uint32_t&, uint32_t&, uint32_t&) {}
	
	//! set kernel argument and recurse
	template <typename T, typename... Args>
	floor_inline_always void set_kernel_arguments(metal_encoder* encoder, const kernel_entry& entry,
												  uint32_t& total_idx, uint32_t& buffer_idx, uint32_t& texture_idx,
												  T&& arg, Args&&... args) {
		set_kernel_argument(total_idx, buffer_idx, texture_idx, encoder, entry, forward<T>(arg));
		++total_idx;
		set_kernel_arguments(encoder, entry, total_idx, buffer_idx, texture_idx, forward<Args>(args)...);
	}
	
	//! actual kernel argument setter
	template <typename T>
	void set_kernel_argument(uint32_t&, uint32_t& buffer_idx, uint32_t&,
							 metal_encoder* encoder, const kernel_entry&, T&& arg) {
		set_const_parameter(encoder, buffer_idx, &arg, sizeof(T));
		++buffer_idx;
	}
	void set_const_parameter(metal_encoder* encoder, const uint32_t& idx,
							 const void* ptr, const size_t& size);
	
	void set_kernel_argument(uint32_t& total_idx, uint32_t& buffer_idx, uint32_t& texture_idx,
							 metal_encoder* encoder, const kernel_entry& entry,
							 shared_ptr<compute_buffer> arg);
	void set_kernel_argument(uint32_t& total_idx, uint32_t& buffer_idx, uint32_t& texture_idx,
							 metal_encoder* encoder, const kernel_entry& entry,
							 shared_ptr<compute_image> arg);
	
};

#endif

#endif
