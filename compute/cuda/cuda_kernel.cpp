/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2021 Florian Ziesche
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
#include <floor/compute/cuda/cuda_device.hpp>

cuda_kernel::cuda_kernel(kernel_map_type&& kernels_) : kernels(move(kernels_)) {
}

typename cuda_kernel::kernel_map_type::const_iterator cuda_kernel::get_kernel(const compute_queue& cqueue) const {
	return kernels.find((const cuda_device&)cqueue.get_device());
}

void cuda_kernel::execute_internal(const compute_queue& cqueue,
								   const cuda_kernel_entry& entry,
								   const uint3& grid_dim,
								   const uint3& block_dim,
								   void** kernel_params) const {
	CU_CALL_NO_ACTION(cu_launch_kernel(entry.kernel,
									   grid_dim.x, grid_dim.y, grid_dim.z,
									   block_dim.x, block_dim.y, block_dim.z,
									   0,
									   (const_cu_stream)cqueue.get_queue_ptr(),
									   kernel_params,
									   nullptr),
					  "failed to execute kernel")
}

void cuda_kernel::execute_cooperative_internal(const compute_queue& cqueue,
											   const cuda_kernel_entry& entry,
											   const uint3& grid_dim,
											   const uint3& block_dim,
											   void** kernel_params) const {
	CU_CALL_NO_ACTION(cu_launch_cooperative_kernel(entry.kernel,
												   grid_dim.x, grid_dim.y, grid_dim.z,
												   block_dim.x, block_dim.y, block_dim.z,
												   0,
												   (const_cu_stream)cqueue.get_queue_ptr(),
												   kernel_params),
					  "failed to execute cooperative kernel")
}

void cuda_kernel::execute(const compute_queue& cqueue,
						  const bool& is_cooperative,
						  const uint32_t& dim floor_unused,
						  const uint3& global_work_size,
						  const uint3& local_work_size,
						  const vector<compute_kernel_arg>& args) const {
	// find entry for queue device
	const auto kernel_iter = get_kernel(cqueue);
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
	vector<void*> kernel_params(args.size(), nullptr);
	auto kernel_params_data = make_unique<uint8_t[]>(kernel_iter->second.kernel_args_size + heap_protect);
	uint8_t* data = kernel_params_data.get();
	
	{
#if defined(FLOOR_DEBUG)
		const auto& entry = kernel_iter->second;
		uint32_t param_idx = 0;
#endif
		auto param_iter = kernel_params.begin();
		for (const auto& arg : args) {
			void*& param = *param_iter++;
#if defined(FLOOR_DEBUG)
			const auto idx = param_idx++;
#endif
			
			if (auto buf_ptr = get_if<const compute_buffer*>(&arg.var)) {
				param = data;
				memcpy(data, &((const cuda_buffer*)*buf_ptr)->get_cuda_buffer(), sizeof(cu_device_ptr));
				data += sizeof(cu_device_ptr);
			} else if (auto vec_buf_ptrs = get_if<const vector<compute_buffer*>*>(&arg.var)) {
				log_error("array of buffers is not yet supported for CUDA");
				return;
			} else if (auto vec_buf_sptrs = get_if<const vector<shared_ptr<compute_buffer>>*>(&arg.var)) {
				log_error("array of buffers is not yet supported for CUDA");
				return;
			} else if (auto img_ptr = get_if<const compute_image*>(&arg.var)) {
				auto cu_img = (const cuda_image*)*img_ptr;
				
#if defined(FLOOR_DEBUG)
				// sanity checks
				if (entry.info->args[idx].image_access == llvm_toolchain::ARG_IMAGE_ACCESS::NONE) {
					log_error("no image access qualifier specified!");
					return;
				}
				if (entry.info->args[idx].image_access == llvm_toolchain::ARG_IMAGE_ACCESS::READ ||
					entry.info->args[idx].image_access == llvm_toolchain::ARG_IMAGE_ACCESS::READ_WRITE) {
					if (cu_img->get_cuda_textures()[0] == 0) {
						log_error("image is set to be readable, but texture objects don't exist!");
						return;
					}
				}
				if (entry.info->args[idx].image_access == llvm_toolchain::ARG_IMAGE_ACCESS::WRITE ||
					entry.info->args[idx].image_access == llvm_toolchain::ARG_IMAGE_ACCESS::READ_WRITE) {
					if (cu_img->get_cuda_surfaces()[0] == 0) {
						log_error("image is set to be writable, but surface object doesn't exist!");
						return;
					}
				}
#endif
				
				// set this to the start
				param = data;
				
				// set texture+sampler objects
				const auto& textures = cu_img->get_cuda_textures();
				for (const auto& texture : textures) {
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
				} else {
					memset(data, 0, sizeof(cu_device_ptr));
				}
				data += sizeof(cu_device_ptr);
				
				// set run-time image type
				memcpy(data, &cu_img->get_image_type(), sizeof(COMPUTE_IMAGE_TYPE));
				data += sizeof(COMPUTE_IMAGE_TYPE);
				
				data += 4 /* padding */;
			} else if (auto vec_img_ptrs = get_if<const vector<compute_image*>*>(&arg.var)) {
				log_error("array of images is not supported for CUDA");
				return;
			} else if (auto vec_img_sptrs = get_if<const vector<shared_ptr<compute_image>>*>(&arg.var)) {
				log_error("array of images is not supported for CUDA");
				return;
			} else if (auto arg_buf_ptr = get_if<const argument_buffer*>(&arg.var)) {
				log_error("argument buffer handling is not implemented yet for CUDA");
				return;
			} else if (auto generic_arg_ptr = get_if<const void*>(&arg.var)) {
				param = data;
				memcpy(data, *generic_arg_ptr, arg.size);
				data += arg.size;
			} else {
				log_error("encountered invalid arg");
				return;
			}
		}
	}
	
	const auto written_args_size = distance(&kernel_params_data[0], data);
	if((size_t)written_args_size != kernel_iter->second.kernel_args_size) {
		log_error("invalid kernel parameters size (in $): got $, expected $",
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
	
	if (!is_cooperative) {
		execute_internal(cqueue, kernel_iter->second, grid_dim, block_dim, &kernel_params[0]);
	} else {
		execute_cooperative_internal(cqueue, kernel_iter->second, grid_dim, block_dim, &kernel_params[0]);
	}
}

const compute_kernel::kernel_entry* cuda_kernel::get_kernel_entry(const compute_device& dev) const {
	const auto ret = kernels.get((const cuda_device&)dev);
	return !ret.first ? nullptr : &ret.second->second;
}

#endif
