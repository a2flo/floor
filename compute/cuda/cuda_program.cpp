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

#include <floor/compute/cuda/cuda_program.hpp>

#if !defined(FLOOR_NO_CUDA)

#include <floor/compute/cuda/cuda_kernel.hpp>
#include <floor/compute/cuda/cuda_device.hpp>

// note that cuda doesn't have any special argument types and everything is just sized "memory"
// -> only need to add up sizes
static size_t compute_kernel_args_size(const llvm_compute::function_info& info) {
	size_t ret = 0;
	const auto arg_count = info.args.size();
	for(size_t i = 0; i < arg_count; ++i) {
		// actual arg or pointer?
		if(info.args[i].address_space == llvm_compute::function_info::ARG_ADDRESS_SPACE::CONSTANT) {
			ret += info.args[i].size;
		}
		else if(info.args[i].address_space == llvm_compute::function_info::ARG_ADDRESS_SPACE::IMAGE) {
			ret += sizeof(uint64_t) * (cuda_sampler_count() + 1 /* surface */) + sizeof(COMPUTE_IMAGE_TYPE) + 4 /* padding */;
		}
		else ret += sizeof(void*);
	}
	return ret;
}

cuda_program::cuda_program(program_map_type&& programs_) : programs(move(programs_)) {
	if(programs.empty()) return;
	retrieve_unique_kernel_names(programs);
	
	// create all kernels of all device programs
	// note that this essentially reshuffles the program "device -> kernels" data to "kernels -> devices"
	kernels.reserve(kernel_names.size());
	for(const auto& kernel_name : kernel_names) {
		cuda_kernel::kernel_map_type kernel_map;
		kernel_map.reserve(kernel_names.size());
		
		for(const auto& prog : programs) {
			if(!prog.second.valid) continue;
			for(const auto& info : prog.second.kernels_info) {
				if(info.name == kernel_name) {
					cuda_kernel::cuda_kernel_entry entry;
					entry.info = &info;
					entry.kernel_args_size = compute_kernel_args_size(info);
					entry.max_work_group_item_sizes = prog.first->max_work_group_item_sizes;
					
					CU_CALL_CONT(cu_module_get_function(&entry.kernel, prog.second.program, kernel_name.c_str()),
								 "failed to get function \"" + kernel_name + "\"");
					
					// retrieve max local work size for this kernel for this device
					int max_local_work_size = 0;
					CU_CALL_IGNORE(cu_function_get_attribute(&max_local_work_size,
															 CU_FUNCTION_ATTRIBUTE::MAX_THREADS_PER_BLOCK, entry.kernel));
					entry.max_local_work_size = (max_local_work_size < 0 ? 0 : (uint32_t)max_local_work_size);
					
#if 0
					// use this to compute max occupancy
					int min_grid_size = 0, block_size = 0;
					CU_CALL_NO_ACTION(cu_occupancy_max_potential_block_size(&min_grid_size, &block_size, &entry.kernel, 0, 0, 0),
									  "failed to compute max potential occupancy");
					log_debug("%s max occupancy: grid size >= %u with block size %u", kernel_name, min_grid_size, block_size);
#endif
					
					// success, insert into map
					kernel_map.insert_or_assign(prog.first, entry);
					break;
				}
			}
		}
		
		kernels.emplace_back(make_shared<cuda_kernel>(move(kernel_map)));
	}
}

#endif
