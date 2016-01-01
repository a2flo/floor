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

#include <floor/compute/metal/metal_program.hpp>

#if !defined(FLOOR_NO_METAL)

#include <floor/compute/metal/metal_kernel.hpp>
#include <floor/compute/metal/metal_device.hpp>
#include <floor/compute/llvm_compute.hpp>
#include <floor/core/core.hpp>

metal_program::metal_program(program_map_type&& programs_) : programs(move(programs_)) {
	if(programs.empty()) return;
	retrieve_unique_kernel_names(programs);
	
	// create all kernels of all device programs
	// note that this essentially reshuffles the program "device -> kernels" data to "kernels -> devices"
	kernels.reserve(kernel_names.size());
	for(const auto& kernel_name : kernel_names) {
		metal_kernel::kernel_map_type kernel_map;
		kernel_map.reserve(kernel_names.size());
		
		for(auto& prog : programs) {
			if(!prog.second.valid) continue;
			for(const auto& info : prog.second.kernels_info) {
				if(info.name == kernel_name) {
					metal_kernel::metal_kernel_entry entry;
					entry.info = &info;
					entry.max_work_group_item_sizes = prog.first->max_work_group_item_sizes;
					
					//
					id <MTLFunction> kernel = [prog.second.program newFunctionWithName:[NSString stringWithUTF8String:info.name.c_str()]];
					if(!kernel) {
						log_error("failed to get function \"%s\" for device \"%s\"", info.name, prog.first->name);
						continue;
					}
					
					NSError* err = nullptr;
					id <MTLComputePipelineState> kernel_state = [[prog.second.program device] newComputePipelineStateWithFunction:kernel error:&err];
					if(!kernel_state) {
						log_error("failed to create kernel state \"%s\" for device \"%s\": %s", info.name, prog.first->name,
								  (err != nullptr ? [[err localizedDescription] UTF8String] : "unknown error"));
						continue;
					}
#if defined(FLOOR_DEBUG)
					log_debug("%s (%s): max work-items: %u, simd width: %u",
							  info.name, prog.first->name,
							  [kernel_state maxTotalThreadsPerThreadgroup], [kernel_state threadExecutionWidth]);
#endif
					
					// success, insert necessary info/data everywhere
					prog.second.metal_kernels.emplace_back(metal_program_entry::metal_kernel_data { kernel, kernel_state });
					entry.kernel = (__bridge void*)kernel;
					entry.kernel_state = (__bridge void*)kernel_state;
					entry.max_local_work_size = [kernel_state maxTotalThreadsPerThreadgroup];
					kernel_map.insert_or_assign(prog.first, entry);
					break;
				}
			}
		}
		
		kernels.emplace_back(make_shared<metal_kernel>(move(kernel_map)));
	}
}

#endif
