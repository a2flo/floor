/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2023 Florian Ziesche
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
#include <floor/graphics/metal/metal_shader.hpp>
#include <floor/compute/metal/metal_device.hpp>
#include <floor/compute/llvm_toolchain.hpp>
#include <floor/core/core.hpp>

metal_program::metal_program(program_map_type&& programs_) : programs(std::move(programs_)) {
	if(programs.empty()) return;
	retrieve_unique_kernel_names(programs);
	
	// create all kernels of all device programs
	// note that this essentially reshuffles the program "device -> kernels" data to "kernels -> devices"
	kernels.reserve(kernel_names.size());
	for(const auto& kernel_name : kernel_names) {
		metal_kernel::kernel_map_type kernel_map;
		kernel_map.reserve(kernel_names.size());
		
		bool is_kernel = true;
		for(auto& prog : programs) {
			if(!prog.second.valid) continue;
			for(const auto& info : prog.second.functions) {
				if(info.name == kernel_name) {
					metal_kernel::metal_kernel_entry entry;
					entry.info = &info;
					entry.max_local_size = prog.first.get().max_local_size;
					if (entry.info->type != llvm_toolchain::FUNCTION_TYPE::KERNEL) {
						is_kernel = false;
					}
					
					//
					const auto func_name = [NSString stringWithUTF8String:info.name.c_str()];
					if (!func_name) {
						log_error("invalid function name: $", info.name);
						continue;
					}
					id <MTLFunction> func = [prog.second.program newFunctionWithName:floor_force_nonnull(func_name)];
					if(!func) {
						log_error("failed to get function \"$\" for device \"$\"", info.name, prog.first.get().name);
						continue;
					}
					
					NSError* err = nullptr;
					id <MTLComputePipelineState> kernel_state = nil;
					bool supports_indirect_compute = false;
					if([func functionType] == MTLFunctionTypeKernel) {
						MTLComputePipelineDescriptor* mtl_pipeline_desc = [[MTLComputePipelineDescriptor alloc] init];
						const string label = info.name + " pipeline";
						mtl_pipeline_desc.label = [NSString stringWithUTF8String:label.c_str()];
						mtl_pipeline_desc.computeFunction = func;
						
						// optimization opt-in
						mtl_pipeline_desc.threadGroupSizeIsMultipleOfThreadExecutionWidth = true;
						
						// implicitly support indirect compute when the function doesn't take any image parameters
						bool has_image_args = false;
						for (const auto& func_arg : info.args) {
							if (func_arg.image_type != llvm_toolchain::ARG_IMAGE_TYPE::NONE) {
								has_image_args = true;
								break;
							}
						}
						if (!has_image_args) {
							mtl_pipeline_desc.supportIndirectCommandBuffers = true;
						}
						
						// TODO: set buffer mutability
						
						kernel_state = [[prog.second.program device] newComputePipelineStateWithDescriptor:mtl_pipeline_desc
																								   options:MTLPipelineOptionNone
																								reflection:nil
																									 error:&err];
						if (!kernel_state) {
							log_error("failed to create kernel state \"$\" for device \"$\": $", info.name, prog.first.get().name,
									  (err != nullptr ? [[err localizedDescription] UTF8String] : "unknown error"));
							continue;
						}
						supports_indirect_compute = [kernel_state supportIndirectCommandBuffers];
#if defined(FLOOR_DEBUG) || defined(FLOOR_IOS)
						log_debug("$ ($): max work-items: $, simd width: $, local mem: $",
								  info.name, prog.first.get().name,
								  [kernel_state maxTotalThreadsPerThreadgroup], [kernel_state threadExecutionWidth], [kernel_state staticThreadgroupMemoryLength]);
#endif
					}
					
					// success, insert necessary info/data everywhere
					prog.second.metal_kernels.emplace_back(metal_program_entry::metal_kernel_data { func, kernel_state });
					entry.kernel = (__bridge void*)func;
					entry.kernel_state = (__bridge void*)kernel_state;
					entry.supports_indirect_compute = supports_indirect_compute;
					if(kernel_state != nil) {
						entry.max_total_local_size = (uint32_t)[kernel_state maxTotalThreadsPerThreadgroup];
					}
					kernel_map.insert_or_assign(prog.first.get(), entry);
					break;
				}
			}
		}
		
		kernels.emplace_back(is_kernel ? make_shared<metal_kernel>(std::move(kernel_map)) : make_shared<metal_shader>(std::move(kernel_map)));
	}
}

#endif
