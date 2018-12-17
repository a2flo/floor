/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2018 Florian Ziesche
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

#if !defined(FLOOR_NO_VULKAN)
#include <floor/compute/vulkan/vulkan_program.hpp>
#include <floor/compute/vulkan/vulkan_kernel.hpp>
#include <floor/compute/vulkan/vulkan_device.hpp>

vulkan_program::vulkan_program(program_map_type&& programs_) : programs(move(programs_)) {
	if(programs.empty()) return;
	retrieve_unique_kernel_names(programs);
	
	// create all kernels of all device programs
	// note that this essentially reshuffles the program "device -> kernels" data to "kernels -> devices"
	kernels.reserve(kernel_names.size());
	for(const auto& func_name : kernel_names) {
		vulkan_kernel::kernel_map_type kernel_map;
		kernel_map.reserve(kernel_names.size());
		
		for(const auto& prog : programs) {
			if(!prog.second.valid) continue;
			
			const auto max_mip_levels = prog.first->max_mip_levels;
			for(const auto& info : prog.second.functions) {
				if(info.name == func_name) {
					vulkan_kernel::vulkan_kernel_entry entry;
					entry.info = &info;
					
					if(!info.has_valid_local_size()) {
						entry.max_local_size = prog.first->max_local_size;
						
						// always assume that we can execute this with the max possible work-group size,
						// i.e. use this as the initial default
						entry.max_total_local_size = prog.first->max_total_local_size;
					}
					else {
						// a required local size/dim is specified -> use it
						entry.max_local_size = info.local_size;
						entry.max_total_local_size = info.local_size.extent();
					}
					
					// TODO: make sure that _all_ of this is synchronized
					
					const VkShaderStageFlagBits stage = (info.type == llvm_toolchain::function_info::FUNCTION_TYPE::VERTEX ? VK_SHADER_STAGE_VERTEX_BIT :
														 info.type == llvm_toolchain::function_info::FUNCTION_TYPE::FRAGMENT ? VK_SHADER_STAGE_FRAGMENT_BIT :
														 VK_SHADER_STAGE_COMPUTE_BIT /* should notice anything else earlier */);
					
					// create function + device specific descriptor set layout
					vector<VkDescriptorSetLayoutBinding> bindings(info.args.size());
					vector<VkDescriptorType> descriptor_types(info.args.size());
					uint32_t ssbo_desc = 0, read_image_desc = 0, write_image_desc = 0;
					bool valid_desc = true;
					for(uint32_t i = 0, binding_idx = 0; i < (uint32_t)info.args.size(); ++i) {
						bindings[binding_idx].binding = binding_idx;
						bindings[binding_idx].descriptorCount = 1;
						bindings[binding_idx].stageFlags = stage;
						bindings[binding_idx].pImmutableSamplers = nullptr;
						
						switch(info.args[i].address_space) {
							// image
							case llvm_toolchain::function_info::ARG_ADDRESS_SPACE::IMAGE: {
								const bool is_image_array = (info.args[i].special_type ==
															 llvm_toolchain::function_info::SPECIAL_TYPE::IMAGE_ARRAY);
								if(is_image_array) {
									bindings[binding_idx].descriptorCount = info.args[i].size;
								}
								switch(info.args[i].image_access) {
									case llvm_toolchain::function_info::ARG_IMAGE_ACCESS::READ:
										bindings[binding_idx].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
										if(is_image_array) {
											read_image_desc += info.args[i].size;
										}
										else ++read_image_desc;
										break;
									case llvm_toolchain::function_info::ARG_IMAGE_ACCESS::WRITE:
										bindings[binding_idx].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
										bindings[binding_idx].descriptorCount *= max_mip_levels;
										if(is_image_array) {
											write_image_desc += info.args[i].size * max_mip_levels;
										}
										else {
											write_image_desc += max_mip_levels;
										}
										break;
									case llvm_toolchain::function_info::ARG_IMAGE_ACCESS::READ_WRITE: {
										if(is_image_array) {
											log_error("read/write image array not supported");
											valid_desc = false;
											return;
										}
										
										// need to add both a sampled one and a storage one
										bindings[binding_idx].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
										descriptor_types.emplace(next(begin(descriptor_types), binding_idx),
																 VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE);
										++binding_idx;
										bindings.emplace(next(begin(bindings), binding_idx),
														 VkDescriptorSetLayoutBinding {
															 .binding = binding_idx,
															 .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
															 .descriptorCount = max_mip_levels,
															 .stageFlags = stage,
															 .pImmutableSamplers = nullptr,
														 });
										++read_image_desc;
										write_image_desc += max_mip_levels;
										break;
									}
									case llvm_toolchain::function_info::ARG_IMAGE_ACCESS::NONE:
										log_error("unknown image access type");
										valid_desc = false;
										break;
								}
								break;
							}
							// buffer and param (there are no proper constant parameters)
							case llvm_toolchain::function_info::ARG_ADDRESS_SPACE::GLOBAL:
							case llvm_toolchain::function_info::ARG_ADDRESS_SPACE::CONSTANT:
								// TODO/NOTE: for now, this is always a buffer, later on it might make sense to fit as much as possible
								//            into push constants (will require compiler support of course + device specific binary)
								// NOTE: min push constants size is at least 128 bytes
								// NOTE: uniforms/param and buffers are always SSBOs - uniforms/param could technically be
								//       Block/uniform variables, but these have insane alignment/offset requirements,
								//       so always make them SSBOs, which have less restrictions
								bindings[binding_idx].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC;
								++ssbo_desc;
								break;
							case llvm_toolchain::function_info::ARG_ADDRESS_SPACE::LOCAL:
								log_error("arg with a local address space is not supported (#%u in %s)", i, func_name);
								valid_desc = false;
								break;
							case llvm_toolchain::function_info::ARG_ADDRESS_SPACE::UNKNOWN:
								if(info.args[i].special_type == llvm_toolchain::function_info::SPECIAL_TYPE::STAGE_INPUT) {
									// ignore + compact
									bindings.pop_back();
									continue;
								}
								log_error("arg with an unknown address space");
								valid_desc = false;
								break;
						}
						if(!valid_desc) break;
						
						descriptor_types[binding_idx] = bindings[binding_idx].descriptorType;
						++binding_idx;
					}
					if(!valid_desc) {
						log_error("invalid descriptor bindings for function \"%s\" for device \"%s\"!", func_name, prog.first->name);
						continue;
					}
					
					// move descriptor types to the kernel entry, we'll need these when setting function args
					entry.desc_types = move(descriptor_types);
					
					// always create a descriptor set layout, even when it's empty (we still need to be able to set/skip it later on)
					const VkDescriptorSetLayoutCreateInfo desc_set_layout_info {
						.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
						.pNext = nullptr,
						.flags = 0,
						.bindingCount = (uint32_t)bindings.size(),
						.pBindings = (!bindings.empty() ? bindings.data() : nullptr),
					};
					VK_CALL_CONT(vkCreateDescriptorSetLayout(prog.first->device, &desc_set_layout_info, nullptr, &entry.desc_set_layout),
								 "failed to create descriptor set layout (" + func_name + ")")
					// TODO: vkDestroyDescriptorSetLayout cleanup
					
					if(!bindings.empty()) {
						// create descriptor pool + descriptors
						// TODO: think about how this can be properly handled (creating a pool per function per device is probably not a good idea)
						//       -> create a descriptor allocation handler, start with a large vkCreateDescriptorPool,
						//          then create new ones if allocation fails (due to fragmentation)
						//          DO NOT use VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT
						const uint32_t pool_count = ((ssbo_desc > 0 ? 1 : 0) +
													 (read_image_desc > 0 ? 1 : 0) +
													 (write_image_desc > 0 ? 1 : 0));
						vector<VkDescriptorPoolSize> pool_sizes(pool_count);
						uint32_t pool_index = 0;
						if(ssbo_desc > 0 || pool_count == 0) {
							pool_sizes[pool_index].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC;
							pool_sizes[pool_index].descriptorCount = (ssbo_desc > 0 ? ssbo_desc : 1);
							++pool_index;
						}
						if(read_image_desc > 0) {
							pool_sizes[pool_index].type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
							pool_sizes[pool_index].descriptorCount = read_image_desc;
							++pool_index;
						}
						if(write_image_desc > 0) {
							pool_sizes[pool_index].type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
							pool_sizes[pool_index].descriptorCount = write_image_desc;
							++pool_index;
						}
						const VkDescriptorPoolCreateInfo desc_pool_info {
							.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
							.pNext = nullptr,
							.flags = 0,
							// we only need one set for now
							.maxSets = 1,
							.poolSizeCount = pool_count,
							.pPoolSizes = pool_sizes.data(),
						};
						VK_CALL_CONT(vkCreateDescriptorPool(prog.first->device, &desc_pool_info, nullptr, &entry.desc_pool),
									 "failed to create descriptor pool (" + func_name + ")")
						
						// allocate descriptor set
						const VkDescriptorSetAllocateInfo desc_set_alloc_info {
							.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
							.pNext = nullptr,
							.descriptorPool = entry.desc_pool,
							.descriptorSetCount = 1,
							.pSetLayouts = &entry.desc_set_layout,
						};
						VK_CALL_CONT(vkAllocateDescriptorSets(prog.first->device, &desc_set_alloc_info, &entry.desc_set),
									 "failed to allocate descriptor set (" + func_name + ")")
					}
					// else: no descriptors entry.desc_* already nullptr
					
					// find spir-v module index for this function
					const auto mod_iter = prog.second.func_to_mod_map.find(func_name);
					if(mod_iter == prog.second.func_to_mod_map.end()) {
						log_error("did not find a module mapping for function \"%s\"", func_name);
						continue;
					}
					
					// stage info, can be used here or at a later point
					entry.stage_info = VkPipelineShaderStageCreateInfo {
						.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
						.pNext = nullptr,
						.flags = 0,
						.stage = stage,
						.module = prog.second.programs[mod_iter->second],
						.pName = func_name.c_str(),
						.pSpecializationInfo = nullptr,
					};
					
					// we can only actually create compute pipelines here, because they can exist on their own
					// vertex/fragment/etc graphics pipelines would need much more information (which ones to combine to begin with)
					if(info.type == llvm_toolchain::function_info::FUNCTION_TYPE::KERNEL) {
						// create the pipeline layout
						const VkDescriptorSetLayout layouts[2] {
							prog.first->fixed_sampler_desc_set_layout,
							entry.desc_set_layout,
						};
						const VkPipelineLayoutCreateInfo pipeline_layout_info {
							.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
							.pNext = nullptr,
							.flags = 0,
							.setLayoutCount = 2u,
							.pSetLayouts = layouts,
							.pushConstantRangeCount = 0,
							.pPushConstantRanges = nullptr,
						};
						VK_CALL_CONT(vkCreatePipelineLayout(prog.first->device, &pipeline_layout_info, nullptr, &entry.pipeline_layout),
									 "failed to create pipeline layout (" + func_name + ")")
						
						const uint3 work_group_size = (info.has_valid_local_size() ?
													   info.local_size :
													   uint3 { entry.max_total_local_size, 1, 1 });
						if(entry.specialize(prog.first, work_group_size) == nullptr) {
							// NOTE: if specialization failed, this will have already printed an error
							continue;
						}
					}
					
					// success, insert into map
					kernel_map.insert_or_assign(prog.first, entry);
					break;
				}
			}
		}
		
		kernels.emplace_back(make_shared<vulkan_kernel>(move(kernel_map)));
	}
}

#endif
