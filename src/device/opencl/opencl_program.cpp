/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2025 Florian Ziesche
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
#include <floor/device/opencl/opencl_program.hpp>
#include <floor/device/opencl/opencl_function.hpp>
#include <floor/device/opencl/opencl_device.hpp>

namespace fl {

opencl_program::opencl_program(program_map_type&& programs_) :
device_program(retrieve_unique_function_names(programs_)), programs(std::move(programs_)) {
	if (programs.empty()) {
		return;
	}
	
	// create all functions of all device programs
	// note that this essentially reshuffles the program "device -> functions" data to "functions -> devices"
	functions.reserve(function_names.size());
	for(const auto& function_name : function_names) {
		opencl_function::function_map_type function_map;
		for(auto&& prog : programs) {
			if(!prog.second.valid) continue;
			
			for(const auto& info : prog.second.functions) {
				if(info.name == function_name) {
					opencl_function::opencl_function_entry entry;
					entry.info = &info;
					entry.max_local_size = prog.first->max_local_size;
					
					CL_CALL_ERR_PARAM_CONT(entry.function = clCreateKernel(prog.second.program, function_name.c_str(),
																		 &kernel_err), kernel_err,
										   "failed to create kernel \"" + function_name + "\" for device \"" + prog.first->name + "\"")
					
					// retrieve max possible work-group size for this device for this function
					entry.max_total_local_size = (uint32_t)cl_get_info<CL_KERNEL_WORK_GROUP_SIZE>(entry.function, prog.first->device_id);
					
					// sanity check/override if reported local size > actual supported one (especially on Intel CPUs ...)
					entry.max_total_local_size = std::min(entry.max_total_local_size, prog.first->max_total_local_size);
					
#if 0 // dump function + function args info
					const auto arg_count = cl_get_info<CL_KERNEL_NUM_ARGS>(entry.function);
					log_debug("function $: arg count: $", function_name, arg_count);
					for(uint32_t i = 0; i < arg_count; ++i) {
						log_debug("\targ #$: $: $ $ $ $", i,
								  cl_get_info<CL_KERNEL_ARG_NAME>(entry.function, i),
								  cl_get_info<CL_KERNEL_ARG_ADDRESS_QUALIFIER>(entry.function, i),
								  cl_get_info<CL_KERNEL_ARG_ACCESS_QUALIFIER>(entry.function, i),
								  cl_get_info<CL_KERNEL_ARG_TYPE_NAME>(entry.function, i),
								  cl_get_info<CL_KERNEL_ARG_TYPE_QUALIFIER>(entry.function, i));
					}
#endif
					
					// success, insert into map
					function_map.insert_or_assign(prog.first, entry);
					break;
				}
			}
		}
		
		functions.emplace_back(std::make_shared<opencl_function>(function_name, std::move(function_map)));
	}
}

} // namespace fl

#endif
