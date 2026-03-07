/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2026 Florian Ziesche
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

#include <floor/device/metal/metal4_program.hpp>

#if !defined(FLOOR_NO_METAL)

#include <floor/device/metal/metal4_function.hpp>
#include <floor/device/metal/metal4_shader.hpp>
#include <floor/device/metal/metal4_args.hpp>
#include <floor/device/metal/metal_device.hpp>
#include <floor/device/metal/metal_program.hpp>
#include <floor/device/metal/metal4_function_entry.hpp>
#include <floor/device/toolchain.hpp>
#include <floor/floor.hpp>

namespace fl {

//! per device internal program state
struct metal4_program_internal_t {
	id <MTL4Compiler> compiler;
	
	metal4_program_internal_t(const device& dev) {
		@autoreleasepool {
			const auto& mtl_dev = (const metal_device&)dev;
			MTL4CompilerDescriptor* compiler_desc = [MTL4CompilerDescriptor new];
			compiler_desc.label = @"default_compiler";
			compiler_desc.pipelineDataSetSerializer = nil;
			NSError* err = nil;
			compiler = [mtl_dev.device newCompilerWithDescriptor:compiler_desc error:&err];
			if (!compiler || err) {
				throw std::runtime_error("failed to create default compiler for device " +
										 dev.name + ": " + (err ? [[err localizedDescription] UTF8String] : "<no-error-msg>"));
			}
		}
	}
	
	~metal4_program_internal_t() {
		@autoreleasepool {
			compiler = nil;
		}
	}
};

static bool did_init_metal4_program { false };
static fl::flat_map<const device*, std::unique_ptr<metal4_program_internal_t>> metal4_program_internal;

void metal4_program::init(const std::vector<std::unique_ptr<device>>& devices) {
	if (!did_init_metal4_program) {
		did_init_metal4_program = true;
		for (const auto& dev : devices) {
			metal4_program_internal.emplace(dev.get(), std::make_unique<metal4_program_internal_t>(*dev));
		}
	}
}
void metal4_program::destroy() {
	if (did_init_metal4_program) {
		metal4_program_internal.clear();
	}
}

id <MTL4Compiler> metal4_program::get_compiler(const device& dev) {
	if (!did_init_metal4_program) {
		return nil;
	}
	const auto dev_iter = metal4_program_internal.find(&dev);
	if (dev_iter == metal4_program_internal.end()) {
		return nil;
	}
	return dev_iter->second->compiler;
}

metal4_program::metal4_program(program_map_type&& programs_) :
device_program(retrieve_unique_function_names(programs_)), programs(std::move(programs_)) {
	if (programs.empty()) {
		return;
	}
	
	@autoreleasepool {
		// create all functions of all device programs
		// note that this essentially reshuffles the program "device -> functions" data to "functions -> devices"
		static const bool dump_reflection_info = floor::get_metal_dump_reflection_info();
		static const auto& validation_enable_set = floor::get_metal_kernel_validation_enable();
		static const auto& validation_disable_set = floor::get_metal_kernel_validation_disable();
		functions.reserve(function_names.size());
		for (const auto& function_name : function_names) {
			metal4_function::function_map_type function_map;
			bool is_kernel = true;
			for (auto&& prog : programs) {
				if (!prog.second.valid) {
					continue;
				}
				
				id <MTL4Compiler> compiler = get_compiler(*prog.first);
				assert(compiler);
				
				for (const auto& info : prog.second.functions) {
					if (info.name == function_name) {
						if (should_ignore_function_for_device(*prog.first, info)) {
							continue;
						}
						
						const auto func_name = [NSString stringWithUTF8String:info.name.c_str()];
						if (!func_name) {
							log_error("invalid function name: $", info.name);
							continue;
						}
						
						id <MTLFunction> func = [prog.second.program newFunctionWithName:floor_force_nonnull(func_name)];
						if (!func) {
							log_error("failed to get function \"$\" for device \"$\"", info.name, prog.first->name);
							continue;
						}
						
						auto entry = std::make_shared<metal4_function_entry>();
						entry->info = &info;
						if (entry->info->type != toolchain::FUNCTION_TYPE::KERNEL) {
							is_kernel = false;
						}
						if (entry->info->has_valid_required_local_size()) {
							entry->max_local_size = info.required_local_size;
							entry->max_total_local_size = info.required_local_size.extent();
						} else {
							entry->max_local_size = prog.first->max_local_size;
							entry->max_total_local_size = prog.first->max_total_local_size;
						}
						if (entry->info->has_valid_required_simd_width()) {
							entry->required_simd_width = info.required_simd_width;
						}
						
						MTL4LibraryFunctionDescriptor* func_desc = [MTL4LibraryFunctionDescriptor new];
						func_desc.name = [NSString stringWithUTF8String:info.name.c_str()];
						func_desc.library = prog.second.program;
						
						NSError* err = nullptr;
						id <MTLComputePipelineState> kernel_state = nil;
						bool supports_indirect_compute = false;
						if ([func functionType] == MTLFunctionTypeKernel) {
							MTL4ComputePipelineDescriptor* mtl_pipeline_desc = [MTL4ComputePipelineDescriptor new];
							mtl_pipeline_desc.options = [MTL4PipelineOptions new];
							
							const std::string label = info.name + "_pipeline";
							mtl_pipeline_desc.label = [NSString stringWithUTF8String:label.c_str()];
							mtl_pipeline_desc.computeFunctionDescriptor = func_desc;
							
							// optimization opt-in
							mtl_pipeline_desc.threadGroupSizeIsMultipleOfThreadExecutionWidth = true;
							mtl_pipeline_desc.supportBinaryLinking = false;
							if (entry->info->has_valid_required_local_size()) {
								mtl_pipeline_desc.maxTotalThreadsPerThreadgroup = entry->info->required_local_size.extent();
								mtl_pipeline_desc.requiredThreadsPerThreadgroup = {
									.width = entry->info->required_local_size.x,
									.height = entry->info->required_local_size.y,
									.depth = entry->info->required_local_size.z,
								};
							}
							
							if (!floor::get_metal_soft_indirect()) {
								// implicitly support indirect compute when the function doesn't take any non-global-AS parameters
								bool has_non_global_args = false;
								for (const auto& func_arg : info.args) {
									if (func_arg.address_space != toolchain::ARG_ADDRESS_SPACE::GLOBAL &&
										!has_flag<toolchain::ARG_FLAG::ARGUMENT_BUFFER>(func_arg.flags)) {
										has_non_global_args = true;
										break;
									}
								}
								mtl_pipeline_desc.supportIndirectCommandBuffers = (has_non_global_args ?
																				   MTL4IndirectCommandBufferSupportStateDisabled :
																				   MTL4IndirectCommandBufferSupportStateEnabled);
							} else {
								mtl_pipeline_desc.supportIndirectCommandBuffers = MTL4IndirectCommandBufferSupportStateDisabled;
							}
							
							// since compute pipeline creation is not exposed, shader validation enablement is handled via config
							if (!validation_enable_set.empty() && validation_enable_set.contains(function_name)) {
								mtl_pipeline_desc.options.shaderValidation = MTLShaderValidation::MTLShaderValidationEnabled;
							} else if (!validation_disable_set.empty() && validation_disable_set.contains(function_name)) {
								mtl_pipeline_desc.options.shaderValidation = MTLShaderValidation::MTLShaderValidationDisabled;
							}
							// else: keep default
							
							if (dump_reflection_info) {
								// need to explicitly request reflections
								mtl_pipeline_desc.options.shaderReflection = (MTL4ShaderReflectionBindingInfo |
																			  MTL4ShaderReflectionBufferTypeInfo);
							}
							
							kernel_state = [compiler newComputePipelineStateWithDescriptor:mtl_pipeline_desc
																	   compilerTaskOptions:nil
																					 error:&err];
							if (!kernel_state) {
								log_error("failed to create kernel state \"$\" for device \"$\": $", info.name, prog.first->name,
										  (err != nullptr ? [[err localizedDescription] UTF8String] : "<no-error-msg>"));
								continue;
							}
							
							if (dump_reflection_info && [kernel_state reflection]) {
								metal_program::dump_bindings_reflection("kernel \"" + function_name + "\"",
																		[[kernel_state reflection] bindings]);
							}
							
							supports_indirect_compute = ([kernel_state supportIndirectCommandBuffers] ==
														 MTL4IndirectCommandBufferSupportStateEnabled);
#if defined(FLOOR_DEBUG) || defined(FLOOR_IOS) || defined(FLOOR_VISIONOS)
							log_debug("$ ($): max work-items: $, simd width: $, local mem: $, indirect: $",
									  info.name, prog.first->name,
									  [kernel_state maxTotalThreadsPerThreadgroup], [kernel_state threadExecutionWidth],
									  [kernel_state staticThreadgroupMemoryLength], supports_indirect_compute ? "yes" : "no");
#endif
							
							
#if FLOOR_METAL_DEBUG_RS
							prog.first->add_debug_allocation(kernel_state);
#endif
						}
						
						// success, insert necessary info/data everywhere
						prog.second.metal_functions.emplace_back(metal4_program_entry::metal4_function_data { func_desc, func, kernel_state });
						entry->function_descriptor = func_desc;
						entry->function = func;
						entry->kernel_state = kernel_state;
						entry->supports_indirect_compute = supports_indirect_compute;
						if (kernel_state != nil) {
							// adjust downwards from reported max local size
							entry->max_total_local_size = std::min(uint32_t([kernel_state maxTotalThreadsPerThreadgroup]),
																   entry->max_total_local_size);
							if (entry->max_total_local_size < entry->max_local_size.extent()) {
								entry->max_local_size = device_function::check_local_work_size(entry->max_local_size, entry->max_local_size,
																							   entry->max_total_local_size);
							}
						}
						
						auto dev_queue = prog.first->context->get_device_default_queue(*prog.first);
						assert(dev_queue);
						if (!entry->init(*dev_queue)) {
							log_error("failed to initialize function entry argument handling: $", info.name);
							continue;
						}
						
						function_map.insert_or_assign(prog.first, entry);
						break;
					}
				}
			}
			
			functions.emplace_back(is_kernel ?
								   std::make_shared<metal4_function>(function_name, std::move(function_map)) :
								   std::make_shared<metal4_shader>(std::move(function_map)));
		}
	}
}

metal4_program::~metal4_program() {
	@autoreleasepool {
#if FLOOR_METAL_DEBUG_RS
		for (auto&& prog : programs) {
			for (auto&& func : prog.second.metal_functions) {
				if (func.state) {
					prog.first->remove_debug_allocation(func.state);
				}
			}
		}
#endif
		programs.clear();
	}
}

} // namespace fl

#endif
