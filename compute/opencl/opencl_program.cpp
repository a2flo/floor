/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2024 Florian Ziesche
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

#include <floor/core/essentials.hpp>

#if !defined(FLOOR_NO_OPENCL)
#define FLOOR_OPENCL_INFO_FUNCS 1
#include <floor/compute/opencl/opencl_program.hpp>
#include <floor/compute/opencl/opencl_kernel.hpp>
#include <floor/compute/opencl/opencl_device.hpp>

opencl_program::opencl_program(program_map_type&& programs_) :
compute_program(retrieve_unique_kernel_names(programs_)), programs(std::move(programs_)) {
	if (programs.empty()) {
		return;
	}
	
	// create all kernels of all device programs
	// note that this essentially reshuffles the program "device -> kernels" data to "kernels -> devices"
	kernels.reserve(kernel_names.size());
	for(const auto& kernel_name : kernel_names) {
		opencl_kernel::kernel_map_type kernel_map;
		kernel_map.reserve(kernel_names.size());
		
		for(const auto& prog : programs) {
			if(!prog.second.valid) continue;
			
			for(const auto& info : prog.second.functions) {
				if(info.name == kernel_name) {
					opencl_kernel::opencl_kernel_entry entry;
					entry.info = &info;
					entry.max_local_size = prog.first.get().max_local_size;
					
					CL_CALL_ERR_PARAM_CONT(entry.kernel = clCreateKernel(prog.second.program, kernel_name.c_str(),
																		 &kernel_err), kernel_err,
										   "failed to create kernel \"" + kernel_name + "\" for device \"" + prog.first.get().name + "\"")
					
					// retrieve max possible work-group size for this device for this kernel
					entry.max_total_local_size = (uint32_t)cl_get_info<CL_KERNEL_WORK_GROUP_SIZE>(entry.kernel, prog.first.get().device_id);
					
					// sanity check/override if reported local size > actual supported one (especially on Intel CPUs ...)
					entry.max_total_local_size = min(entry.max_total_local_size, prog.first.get().max_total_local_size);
					
#if 0 // dump kernel + kernel args info
					const auto arg_count = cl_get_info<CL_KERNEL_NUM_ARGS>(entry.kernel);
					log_debug("kernel $: arg count: $", kernel_name, arg_count);
					for(uint32_t i = 0; i < arg_count; ++i) {
						log_debug("\targ #$: $: $ $ $ $", i,
								  cl_get_info<CL_KERNEL_ARG_NAME>(entry.kernel, i),
								  cl_get_info<CL_KERNEL_ARG_ADDRESS_QUALIFIER>(entry.kernel, i),
								  cl_get_info<CL_KERNEL_ARG_ACCESS_QUALIFIER>(entry.kernel, i),
								  cl_get_info<CL_KERNEL_ARG_TYPE_NAME>(entry.kernel, i),
								  cl_get_info<CL_KERNEL_ARG_TYPE_QUALIFIER>(entry.kernel, i));
					}
#endif
					
					// success, insert into map
					kernel_map.insert_or_assign(prog.first, entry);
					break;
				}
			}
		}
		
		kernels.emplace_back(make_shared<opencl_kernel>(kernel_name, std::move(kernel_map)));
	}
}

#endif
