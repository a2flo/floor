/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2017 Florian Ziesche
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
#include <floor/compute/llvm_toolchain.hpp>

// the amount of macro voodoo is too damn high ...
#define FLOOR_CUDA_KERNEL_IMPL 1
#include <floor/compute/compute_kernel.hpp>
#undef FLOOR_CUDA_KERNEL_IMPL

class cuda_device;
class cuda_kernel final : public compute_kernel {
public:
	struct cuda_kernel_entry : kernel_entry {
		cu_function kernel { nullptr };
		size_t kernel_args_size;
	};
	typedef flat_map<cuda_device*, cuda_kernel_entry> kernel_map_type;
	
	cuda_kernel(kernel_map_type&& kernels);
	~cuda_kernel() override;
	
	template <typename... Args> void execute(compute_queue* queue,
											 const uint32_t work_dim floor_unused,
											 const uint3 global_work_size,
											 const uint3 local_work_size,
											 Args&&... args) {
		handle_execute<false>(queue, global_work_size, local_work_size, forward<Args>(args)...);
	}
	
	template <typename... Args> void execute_cooperative(compute_queue* queue,
														 const uint32_t work_dim floor_unused,
														 const uint3 global_work_size,
														 const uint3 local_work_size,
														 Args&&... args) {
		handle_execute<true>(queue, global_work_size, local_work_size, forward<Args>(args)...);
	}
	
	const kernel_entry* get_kernel_entry(shared_ptr<compute_device> dev) const override {
		const auto ret = kernels.get((cuda_device*)dev.get());
		return !ret.first ? nullptr : &ret.second->second;
	}
	
protected:
	const kernel_map_type kernels;
	
	COMPUTE_TYPE get_compute_type() const override { return COMPUTE_TYPE::CUDA; }
	
	typename kernel_map_type::const_iterator get_kernel(const compute_queue* queue) const;
	
	template <bool cooperative = false, typename... Args>
	void handle_execute(compute_queue* queue,
						const uint3 global_work_size,
						const uint3 local_work_size,
						Args&&... args) {
		// find entry for queue device
		const auto kernel_iter = get_kernel(queue);
		if(kernel_iter == kernels.cend()) {
			log_error("no kernel for this compute queue/device exists!");
			return;
		}
		
		// check work size (NOTE: will set elements to at least 1)
		const uint3 block_dim = check_local_work_size(kernel_iter->second, local_work_size);
		
		// set and handle kernel arguments
		static constexpr const size_t heap_protect {
#if defined(FLOOR_DEBUG)
			4096
#else
			0
#endif
		};
		array<void*, sizeof...(Args)> kernel_params;
		auto kernel_params_data = make_unique<uint8_t[]>(kernel_iter->second.kernel_args_size + heap_protect);
		uint8_t* data_ptr = kernel_params_data.get();
		set_kernel_arguments(0, kernel_iter->second, &kernel_params[0], data_ptr, forward<Args>(args)...);
		
		const auto written_args_size = distance(&kernel_params_data[0], data_ptr);
		if((size_t)written_args_size != kernel_iter->second.kernel_args_size) {
			log_error("invalid kernel parameters size (in %s): got %u, expected %u",
					  kernel_iter->second.info->name,
					  written_args_size, kernel_iter->second.kernel_args_size);
			return;
		}
		
		// run
		const uint3 grid_dim_overflow {
			global_work_size.x > 0 ? std::min(uint32_t(global_work_size.x % block_dim.x), 1u) : 0u,
			global_work_size.y > 0 ? std::min(uint32_t(global_work_size.y % block_dim.y), 1u) : 0u,
			global_work_size.z > 0 ? std::min(uint32_t(global_work_size.z % block_dim.z), 1u) : 0u
		};
		uint3 grid_dim { (global_work_size / block_dim) + grid_dim_overflow };
		grid_dim.max(1u);
		
		if constexpr(!cooperative) {
			execute_internal(queue, kernel_iter->second, grid_dim, block_dim, &kernel_params[0]);
		}
		else {
			execute_cooperative_internal(queue, kernel_iter->second, grid_dim, block_dim, &kernel_params[0]);
		}
	}
	
	void execute_internal(compute_queue* queue,
						  const cuda_kernel_entry& entry,
						  const uint3& grid_dim,
						  const uint3& block_dim,
						  void** kernel_params);
	
	void execute_cooperative_internal(compute_queue* queue,
									  const cuda_kernel_entry& entry,
									  const uint3& grid_dim,
									  const uint3& block_dim,
									  void** kernel_params);
	
	//! set kernel argument and recurse
	template <typename T, typename... Args>
	floor_inline_always void set_kernel_arguments(const uint8_t num, const kernel_entry& entry,
												  void** params, uint8_t*& data,
												  T&& arg, Args&&... args) const {
		set_kernel_argument(num, entry, params, data, forward<T>(arg));
		set_kernel_arguments(num + 1, entry, ++params, data, forward<Args>(args)...);
	}
	
	//! handle kernel call terminator
	floor_inline_always void set_kernel_arguments(const uint8_t, const kernel_entry&, void**, uint8_t*&) const {}
	
	//! actual kernel argument setter
	template <typename T, enable_if_t<!is_pointer<decay_t<T>>::value>* = nullptr>
	floor_inline_always void set_kernel_argument(const uint8_t, const kernel_entry&,
												 void** param, uint8_t*& data, T&& arg) const {
		*param = data;
		memcpy(data, &arg, sizeof(T));
		data += sizeof(T);
	}
	
	floor_inline_always void set_kernel_argument(const uint8_t num, const kernel_entry& entry,
												 void** param, uint8_t*& data, shared_ptr<compute_buffer> arg) const {
		set_kernel_argument(num, entry, param, data, (compute_buffer*)arg.get());
	}
	
	floor_inline_always void set_kernel_argument(const uint8_t num, const kernel_entry& entry,
												 void** param, uint8_t*& data, shared_ptr<compute_image> arg) const {
		set_kernel_argument(num, entry, param, data, (compute_image*)arg.get());
	}
	
	floor_inline_always void set_kernel_argument(const uint8_t, const kernel_entry&, void** param, uint8_t*& data,
												 const compute_buffer* arg) const {
		*param = data;
		memcpy(data, &((const cuda_buffer*)arg)->get_cuda_buffer(), sizeof(cu_device_ptr));
		data += sizeof(cu_device_ptr);
	}
	
	floor_inline_always void set_kernel_argument(
#if defined(FLOOR_DEBUG) // only used in debug mode
												 const uint8_t num, const kernel_entry& entry,
#else
												 const uint8_t, const kernel_entry&,
#endif
												 void** param, uint8_t*& data, const compute_image* arg) const {
		auto cu_img = (const cuda_image*)arg;
		
#if defined(FLOOR_DEBUG)
		// sanity checks
		if(entry.info->args[num].image_access == llvm_toolchain::function_info::ARG_IMAGE_ACCESS::NONE) {
			log_error("no image access qualifier specified!");
			return;
		}
		if(entry.info->args[num].image_access == llvm_toolchain::function_info::ARG_IMAGE_ACCESS::READ ||
		   entry.info->args[num].image_access == llvm_toolchain::function_info::ARG_IMAGE_ACCESS::READ_WRITE) {
			if(cu_img->get_cuda_textures()[0] == 0) {
				log_error("image is set to be readable, but texture objects don't exist!");
				return;
			}
		}
		if(entry.info->args[num].image_access == llvm_toolchain::function_info::ARG_IMAGE_ACCESS::WRITE ||
		   entry.info->args[num].image_access == llvm_toolchain::function_info::ARG_IMAGE_ACCESS::READ_WRITE) {
			if(cu_img->get_cuda_surfaces()[0] == 0) {
				log_error("image is set to be writable, but surface object doesn't exist!");
				return;
			}
		}
#endif
		
		// set this to the start
		*param = data;
		
		// set texture+sampler objects
		const auto& textures = cu_img->get_cuda_textures();
		for(const auto& texture : textures) {
			memcpy(data, &texture, sizeof(uint32_t));
			data += sizeof(uint32_t);
		}
		
		// set surface object
		memcpy(data, &cu_img->get_cuda_surfaces()[0], sizeof(uint64_t));
		data += sizeof(uint64_t);
		
		// set ptr to surfaces lod buffer
		const auto lod_buffer = cu_img->get_cuda_surfaces_lod_buffer();
		if(lod_buffer != nullptr) {
			memcpy(data, &lod_buffer->get_cuda_buffer(), sizeof(cu_device_ptr));
		}
		else {
			memset(data, 0, sizeof(cu_device_ptr));
		}
		data += sizeof(cu_device_ptr);
		
		// set run-time image type
		memcpy(data, &cu_img->get_image_type(), sizeof(COMPUTE_IMAGE_TYPE));
		data += sizeof(COMPUTE_IMAGE_TYPE);
		
#if defined(PLATFORM_X64)
		data += 4 /* padding */;
#endif
	}
	
};

#endif

#endif
