/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2022 Florian Ziesche
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
#include <floor/compute/vulkan/vulkan_compute.hpp>

using namespace llvm_toolchain;

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
			
			const auto max_mip_levels = prog.first.get().max_mip_levels;
			for(const auto& info : prog.second.functions) {
				if(info.name == func_name) {
					vulkan_kernel::vulkan_kernel_entry entry;
					entry.info = &info;
					
					if(!info.has_valid_local_size()) {
						entry.max_local_size = prog.first.get().max_local_size;
						
						// always assume that we can execute this with the max possible work-group size,
						// i.e. use this as the initial default
						entry.max_total_local_size = prog.first.get().max_total_local_size;
					}
					else {
						// a required local size/dim is specified -> use it
						entry.max_local_size = info.local_size;
						entry.max_total_local_size = info.local_size.extent();
					}
					
					// TODO: make sure that _all_ of this is synchronized
					
					const VkShaderStageFlagBits stage = (info.type == FUNCTION_TYPE::VERTEX ? VK_SHADER_STAGE_VERTEX_BIT :
														 info.type == FUNCTION_TYPE::FRAGMENT ? VK_SHADER_STAGE_FRAGMENT_BIT :
														 VK_SHADER_STAGE_COMPUTE_BIT /* should notice anything else earlier */);
					
					// handle implicit args which add to the total #args
					const auto is_soft_printf = has_flag<FUNCTION_FLAGS::USES_SOFT_PRINTF>(info.flags);
					const uint32_t implicit_arg_count = (is_soft_printf ? 1u : 0u);
					const uint32_t explicit_arg_count = uint32_t(info.args.size());
					const uint32_t total_arg_count = explicit_arg_count + implicit_arg_count;
					
					// create function + device specific descriptor set layout
					vector<VkDescriptorSetLayoutBinding> bindings(total_arg_count);
					vector<VkDescriptorType> descriptor_types(total_arg_count);
					uint32_t ssbo_desc = 0, iub_desc = 0, read_image_desc = 0, write_image_desc = 0;
					uint32_t max_iub_size = 0;
					decltype(vulkan_kernel::vulkan_kernel_entry::constant_buffer_info) constant_buffer_info;
					uint32_t constant_arg_count = 0, constant_buffer_size = 0;
					bool valid_desc = true;
					for(uint32_t i = 0, binding_idx = 0; i < (uint32_t)total_arg_count; ++i) {
						bindings[binding_idx].binding = binding_idx;
						bindings[binding_idx].descriptorCount = 1;
						bindings[binding_idx].stageFlags = stage;
						bindings[binding_idx].pImmutableSamplers = nullptr;
						
						if (i < info.args.size()) {
							switch(info.args[i].address_space) {
								// image
								case ARG_ADDRESS_SPACE::IMAGE: {
									const bool is_image_array = (info.args[i].special_type == SPECIAL_TYPE::IMAGE_ARRAY);
									if (is_image_array) {
										bindings[binding_idx].descriptorCount = info.args[i].size;
									}
									switch(info.args[i].image_access) {
										case ARG_IMAGE_ACCESS::READ:
											bindings[binding_idx].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
											if (is_image_array) {
												read_image_desc += info.args[i].size;
											}
											else ++read_image_desc;
											break;
										case ARG_IMAGE_ACCESS::WRITE:
											bindings[binding_idx].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
											bindings[binding_idx].descriptorCount *= max_mip_levels;
											if (is_image_array) {
												write_image_desc += info.args[i].size * max_mip_levels;
											} else {
												write_image_desc += max_mip_levels;
											}
											break;
										case ARG_IMAGE_ACCESS::READ_WRITE: {
											if (is_image_array) {
												log_error("read/write image array not supported");
												//valid_desc = false;
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
																 .stageFlags = VkShaderStageFlags(stage),
																 .pImmutableSamplers = nullptr,
															 });
											++read_image_desc;
											write_image_desc += max_mip_levels;
											break;
										}
										case ARG_IMAGE_ACCESS::NONE:
											log_error("unknown image access type");
											valid_desc = false;
											break;
									}
									break;
								}
									// buffer and param (there are no proper constant parameters)
								case ARG_ADDRESS_SPACE::GLOBAL:
								case ARG_ADDRESS_SPACE::CONSTANT:
									// NOTE: buffers are always SSBOs
									// NOTE: uniforms/param can either be SSBOs or IUBs, dpending on their size and device support
									if (info.args[i].special_type == SPECIAL_TYPE::IUB) {
										bindings[binding_idx].descriptorType = VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK_EXT;
										// descriptor count == size, which must be a multiple of 4
										const uint32_t arg_size_4 = ((info.args[i].size + 3u) / 4u) * 4u;
										max_iub_size = max(arg_size_4, max_iub_size);
										bindings[binding_idx].descriptorCount = arg_size_4;
										++iub_desc;
									} else {
										bindings[binding_idx].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
										++ssbo_desc;
										
										// put all non-IUB constant args into a single constant buffer that is allocated only once
										if (info.args[i].address_space == ARG_ADDRESS_SPACE::CONSTANT) {
											assert(info.args[i].size > 0);
											// offsets in SSBOs must be aligned to minStorageBufferOffsetAlignment
											const auto ssbo_alignment = prog.first.get().min_storage_buffer_offset_alignment;
											constant_buffer_size = ((constant_buffer_size + ssbo_alignment - 1u) / ssbo_alignment) * ssbo_alignment;
											constant_buffer_info.emplace(uint32_t(i), vulkan_kernel::vulkan_kernel_entry::constant_buffer_info_t {
												.offset = constant_buffer_size,
												.size = info.args[i].size,
											});
											constant_buffer_size += info.args[i].size;
											++constant_arg_count;
										}
									}
									break;
								case ARG_ADDRESS_SPACE::LOCAL:
									log_error("arg with a local address space is not supported (#$ in $)", i, func_name);
									valid_desc = false;
									break;
								case ARG_ADDRESS_SPACE::UNKNOWN:
									if (info.args[i].special_type == SPECIAL_TYPE::STAGE_INPUT) {
										// ignore + compact
										bindings.pop_back();
										continue;
									}
									log_error("arg with an unknown address space");
									valid_desc = false;
									break;
							}
						} else {
							// implicit args
							if (i < total_arg_count) {
								const auto implicit_arg_num = i - explicit_arg_count;
								if (is_soft_printf && implicit_arg_num == 0) {
									// printf buffer is always a SSBO
									bindings[binding_idx].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
									++ssbo_desc;
								} else {
									log_error("invalid implicit arg count for function \"$\": $ is out-of-bounds", func_name, implicit_arg_num);
									valid_desc = false;
								}
							} else {
								log_error("invalid arg count for function \"$\": $ is out-of-bounds", func_name, i);
								valid_desc = false;
							}
						}
						if(!valid_desc) break;
						
						descriptor_types[binding_idx] = bindings[binding_idx].descriptorType;
						++binding_idx;
					}
					if(!valid_desc) {
						log_error("invalid descriptor bindings for function \"$\" for device \"$\"!", func_name, prog.first.get().name);
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
					VK_CALL_CONT(vkCreateDescriptorSetLayout(prog.first.get().device, &desc_set_layout_info, nullptr, &entry.desc_set_layout),
								 "failed to create descriptor set layout (" + func_name + ")")
					// TODO: vkDestroyDescriptorSetLayout cleanup
					
					if(!bindings.empty()) {
						// create descriptor pool + descriptors
						// TODO: think about how this can be properly handled (creating a pool per function per device is probably not a good idea)
						//       -> create a descriptor allocation handler, start with a large vkCreateDescriptorPool,
						//          then create new ones if allocation fails (due to fragmentation)
						//          DO NOT use VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT
						const uint32_t pool_count = ((ssbo_desc > 0 ? 1 : 0) +
													 (iub_desc > 0 ? 1 : 0) +
													 (read_image_desc > 0 ? 1 : 0) +
													 (write_image_desc > 0 ? 1 : 0));
						vector<VkDescriptorPoolSize> pool_sizes(max(pool_count, 1u));
						uint32_t pool_index = 0;
						if(ssbo_desc > 0 || pool_count == 0) {
							pool_sizes[pool_index].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
							pool_sizes[pool_index].descriptorCount = (ssbo_desc > 0 ? ssbo_desc : 1) * vulkan_descriptor_set_container::descriptor_count;
							++pool_index;
						}
						if(iub_desc > 0) {
							pool_sizes[pool_index].type = VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK_EXT;
							// amount of bytes to allocate for descriptors of this type
							// -> use max arg size here
							// TODO/NOTE: no idea where the size requirement comes from -> just round to multiples of 128 for now
							pool_sizes[pool_index].descriptorCount = (const_math::round_next_multiple(max_iub_size + 128u, 128u) *
																	  vulkan_descriptor_set_container::descriptor_count);
							++pool_index;
						}
						if(read_image_desc > 0) {
							pool_sizes[pool_index].type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
							pool_sizes[pool_index].descriptorCount = read_image_desc * vulkan_descriptor_set_container::descriptor_count;
							++pool_index;
						}
						if(write_image_desc > 0) {
							pool_sizes[pool_index].type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
							pool_sizes[pool_index].descriptorCount = write_image_desc * vulkan_descriptor_set_container::descriptor_count;
							++pool_index;
						}
						VkDescriptorPoolCreateInfo desc_pool_info {
							.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
							.pNext = nullptr,
							.flags = 0,
							// we only need a fixed number of sets for now
							.maxSets = vulkan_descriptor_set_container::descriptor_count,
							.poolSizeCount = pool_count,
							.pPoolSizes = pool_sizes.data(),
						};
						VkDescriptorPoolInlineUniformBlockCreateInfoEXT iub_pool_info;
						if (iub_desc > 0) {
							desc_pool_info.pNext = &iub_pool_info;
							iub_pool_info = {
								.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_INLINE_UNIFORM_BLOCK_CREATE_INFO_EXT,
								.pNext = nullptr,
								.maxInlineUniformBlockBindings = iub_desc,
							};
						}
						VK_CALL_CONT(vkCreateDescriptorPool(prog.first.get().device, &desc_pool_info, nullptr, &entry.desc_pool),
									 "failed to create descriptor pool (" + func_name + ")")
						
						// allocate fixed number of descriptor sets
						array<VkDescriptorSet, vulkan_descriptor_set_container::descriptor_count> desc_sets;
						array<VkDescriptorSetLayout, vulkan_descriptor_set_container::descriptor_count> desc_set_layouts;
						desc_set_layouts.fill(entry.desc_set_layout);
						const VkDescriptorSetAllocateInfo desc_set_alloc_info {
							.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
							.pNext = nullptr,
							.descriptorPool = entry.desc_pool,
							.descriptorSetCount = vulkan_descriptor_set_container::descriptor_count,
							.pSetLayouts = &desc_set_layouts[0],
						};
						VK_CALL_CONT(vkAllocateDescriptorSets(prog.first.get().device, &desc_set_alloc_info, &desc_sets[0]),
									 "failed to allocate descriptor sets (" + func_name + ")")
#if defined(FLOOR_DEBUG)
						string desc_set_label_stem = "desc_set:" + func_name + "#";
						for (uint32_t desc_set_idx = 0; desc_set_idx < vulkan_descriptor_set_container::descriptor_count; ++desc_set_idx) {
							((const vulkan_compute*)prog.first.get().context)->set_vulkan_debug_label(prog.first.get(), VK_OBJECT_TYPE_DESCRIPTOR_SET,
																									  uint64_t(desc_sets[desc_set_idx]),
																									  desc_set_label_stem + to_string(desc_set_idx));
							
						}
#endif
						entry.desc_set_container = make_unique<vulkan_descriptor_set_container>(move(desc_sets));
						
						// allocate constant buffers
						if (constant_buffer_size > 0) {
							entry.constant_buffer_info = move(constant_buffer_info);
							
							// align size to 16 bytes
							const uint32_t alignment_pot = 4u, alignment = 1u << alignment_pot;
							constant_buffer_size = ((constant_buffer_size + alignment - 1u) / alignment) * alignment;
							const auto& ctx = *(const vulkan_compute*)prog.first.get().context;
							const vulkan_device& dev = prog.first.get();
							const auto dev_queue = ctx.get_device_default_queue(dev);
							assert(dev_queue != nullptr);
							
							// we need to allocate as many buffers as we have descriptor sets
							array<compute_buffer*, vulkan_descriptor_set_container::descriptor_count> constant_buffers;
#if defined(FLOOR_DEBUG)
							string const_buffer_label_stem = "const_buf:" + func_name + "#";
#endif
							for (uint32_t buf_idx = 0; buf_idx < vulkan_descriptor_set_container::descriptor_count; ++buf_idx) {
								// allocate in device-local/host-coherent memory
								entry.constant_buffers_storage[buf_idx] = ctx.create_buffer(*dev_queue, constant_buffer_size,
																							COMPUTE_MEMORY_FLAG::READ | COMPUTE_MEMORY_FLAG::HOST_WRITE |
																							COMPUTE_MEMORY_FLAG::VULKAN_HOST_COHERENT);
#if defined(FLOOR_DEBUG)
								entry.constant_buffers_storage[buf_idx]->set_debug_label(const_buffer_label_stem + to_string(buf_idx));
#endif
								entry.constant_buffer_mappings[buf_idx] = entry.constant_buffers_storage[buf_idx]->map(*dev_queue,
																													   COMPUTE_MEMORY_MAP_FLAG::WRITE_INVALIDATE |
																													   COMPUTE_MEMORY_MAP_FLAG::BLOCK);
								if (entry.constant_buffer_mappings[buf_idx] == nullptr) {
									log_error("failed to memory map constant buffer #$ for function $", buf_idx, func_name);
									continue;
								}
								constant_buffers[buf_idx] = entry.constant_buffers_storage[buf_idx].get();
							}
							entry.constant_buffers = make_unique<safe_resource_container<compute_buffer*, vulkan_descriptor_set_container::descriptor_count>>(move(constant_buffers));
						}
					}
					// else: no descriptors entry.desc_* already nullptr
					
					// find spir-v module index for this function
					const auto mod_iter = prog.second.func_to_mod_map.find(func_name);
					if(mod_iter == prog.second.func_to_mod_map.end()) {
						log_error("did not find a module mapping for function \"$\"", func_name);
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
					if(info.type == FUNCTION_TYPE::KERNEL) {
						// create the pipeline layout
						const VkDescriptorSetLayout layouts[2] {
							prog.first.get().fixed_sampler_desc_set_layout,
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
						VK_CALL_CONT(vkCreatePipelineLayout(prog.first.get().device, &pipeline_layout_info, nullptr, &entry.pipeline_layout),
									 "failed to create pipeline layout (" + func_name + ")")
						
						GUARD(entry.specializations_lock);
						const uint3 work_group_size = (info.has_valid_local_size() ?
													   info.local_size :
													   uint3 { entry.max_total_local_size, 1, 1 });
						if(entry.specialize(prog.first.get(), work_group_size) == nullptr) {
							// NOTE: if specialization failed, this will have already printed an error
							continue;
						}
					}
					
					// success, insert into map
					kernel_map.emplace_or_assign(move(prog.first), move(entry));
					break;
				}
			}
		}
		
		kernels.emplace_back(make_shared<vulkan_kernel>(move(kernel_map)));
	}
}

#endif
