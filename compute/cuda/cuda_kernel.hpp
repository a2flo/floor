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

#ifndef __FLOOR_CUDA_KERNEL_HPP__
#define __FLOOR_CUDA_KERNEL_HPP__

#include <floor/compute/cuda/cuda_common.hpp>

#if !defined(FLOOR_NO_CUDA)

#include <floor/core/logger.hpp>
#include <floor/threading/atomic_spin_lock.hpp>
#include <floor/compute/cuda/cuda_buffer.hpp>
#include <floor/compute/cuda/cuda_image.hpp>
#include <floor/compute/llvm_compute.hpp>

// the amount of macro voodoo is too damn high ...
#define FLOOR_CUDA_KERNEL_IMPL 1
#include <floor/compute/compute_kernel.hpp>
#undef FLOOR_CUDA_KERNEL_IMPL

class cuda_kernel final : public compute_kernel {
public:
	cuda_kernel(const CUfunction kernel, const llvm_compute::kernel_info& info);
	~cuda_kernel() override;
	
	template <typename... Args> void execute(compute_queue* queue,
											 const uint32_t work_dim floor_unused,
											 const size3 global_work_size,
											 const size3 local_work_size_,
											 Args&&... args) {
		// set and handle kernel arguments (all alloc'ed on stack)
		array<void*, sizeof...(Args)> kernel_params;
		vector<uint8_t> kernel_params_data(kernel_args_size);
		uint8_t* data_ptr = &kernel_params_data[0];
		set_kernel_arguments(0, &kernel_params[0], data_ptr, forward<Args>(args)...);
		
		// run
		const uint3 block_dim { local_work_size_.maxed(1u) }; // prevent % or / by 0, also: cuda needs at least 1
		const uint3 grid_dim_overflow {
			global_work_size.x > 0 ? std::min(uint32_t(global_work_size.x % block_dim.x), 1u) : 0u,
			global_work_size.y > 0 ? std::min(uint32_t(global_work_size.y % block_dim.y), 1u) : 0u,
			global_work_size.z > 0 ? std::min(uint32_t(global_work_size.z % block_dim.z), 1u) : 0u
		};
		uint3 grid_dim { (global_work_size / block_dim) + grid_dim_overflow };
		grid_dim.max(1u);
		
		execute_internal(queue, grid_dim, block_dim, &kernel_params[0]);
	}
	
protected:
	const CUfunction kernel;
	const string func_name;
	const size_t kernel_args_size;
	const llvm_compute::kernel_info info;
	
	COMPUTE_TYPE get_compute_type() const override { return COMPUTE_TYPE::CUDA; }
	
	void execute_internal(compute_queue* queue,
						  const uint3& grid_dim,
						  const uint3& block_dim,
						  void** kernel_params);
	
	//! set kernel argument and recurse
	template <typename T, typename... Args>
	floor_inline_always void set_kernel_arguments(const uint8_t num, void** params, uint8_t*& data, T&& arg, Args&&... args) {
		set_kernel_argument(num, params, data, forward<T>(arg));
		set_kernel_arguments(num + 1, ++params, data, forward<Args>(args)...);
	}
	
	//! handle kernel call terminator
	floor_inline_always void set_kernel_arguments(const uint8_t, void**, uint8_t*&) {}
	
	//! actual kernel argument setter
	template <typename T>
	floor_inline_always void set_kernel_argument(const uint8_t, void** param, uint8_t*& data, T&& arg) {
		*param = data;
		memcpy(data, &arg, sizeof(T));
		data += sizeof(T);
	}
	
	floor_inline_always void set_kernel_argument(const uint8_t, void** param, uint8_t*& data, shared_ptr<compute_buffer> arg) {
		*param = data;
		memcpy(data, &((cuda_buffer*)arg.get())->get_cuda_buffer(), sizeof(CUdeviceptr));
		data += sizeof(CUdeviceptr);
	}
	
	floor_inline_always void set_kernel_argument(const uint8_t num, void** param, uint8_t*& data, shared_ptr<compute_image> arg) {
		cuda_image* cu_img = (cuda_image*)arg.get();
		
#if defined(FLOOR_DEBUG)
		// sanity checks
		if(info.args[num].image_access == llvm_compute::kernel_info::ARG_IMAGE_ACCESS::NONE) {
			log_error("no image access qualifier specified!");
			return;
		}
		if(info.args[num].image_access == llvm_compute::kernel_info::ARG_IMAGE_ACCESS::READ ||
		   info.args[num].image_access == llvm_compute::kernel_info::ARG_IMAGE_ACCESS::READ_WRITE) {
			if(cu_img->get_cuda_texture() == 0) {
				log_error("image is set to be readable, but texture object doesn't exist!");
				return;
			}
		}
		if(info.args[num].image_access == llvm_compute::kernel_info::ARG_IMAGE_ACCESS::WRITE ||
		   info.args[num].image_access == llvm_compute::kernel_info::ARG_IMAGE_ACCESS::READ_WRITE) {
			if(cu_img->get_cuda_surface() == 0) {
				log_error("image is set to be writable, but surface object doesn't exist!");
				return;
			}
		}
#endif
		
		// set this to the start
		*param = data;
		
		if(info.args[num].image_access == llvm_compute::kernel_info::ARG_IMAGE_ACCESS::READ ||
		   info.args[num].image_access == llvm_compute::kernel_info::ARG_IMAGE_ACCESS::READ_WRITE) {
			memcpy(data, &cu_img->get_cuda_texture(), sizeof(uint64_t));
			data += sizeof(uint64_t);
		}
		
		if(info.args[num].image_access == llvm_compute::kernel_info::ARG_IMAGE_ACCESS::WRITE ||
		   info.args[num].image_access == llvm_compute::kernel_info::ARG_IMAGE_ACCESS::READ_WRITE) {
			memcpy(data, &cu_img->get_cuda_surface(), sizeof(uint64_t));
			data += sizeof(uint64_t);
		}
	}
	
};

#endif

#endif
