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

#include <floor/core/essentials.hpp>

#if !defined(FLOOR_NO_VULKAN)
#include <floor/compute/vulkan/vulkan_program.hpp>
#include <floor/compute/vulkan/vulkan_kernel.hpp>
#include <floor/compute/vulkan/vulkan_device.hpp>
#include <floor/compute/vulkan/vulkan_compute.hpp>

using namespace llvm_toolchain;

optional<vulkan_descriptor_set_layout_t> vulkan_program::build_descriptor_set_layout(const compute_device& dev,
																					 const string& func_name,
																					 const llvm_toolchain::function_info& info,
																					 const VkShaderStageFlagBits stage,
																					 const bool is_legacy) {
	const auto& vk_dev = (const vulkan_device&)dev;
	
	// handle implicit args which add to the total #args
	const auto is_soft_printf = has_flag<FUNCTION_FLAGS::USES_SOFT_PRINTF>(info.flags);
	const uint32_t implicit_arg_count = (is_soft_printf ? 1u : 0u);
	const uint32_t explicit_arg_count = uint32_t(info.args.size());
	const uint32_t total_arg_count = explicit_arg_count + implicit_arg_count;
	
	// create function + device specific descriptor set layout
	vulkan_descriptor_set_layout_t layout {};
	layout.bindings.reserve(total_arg_count);
	layout.legacy.desc_types.reserve(total_arg_count);
	bool valid_desc = true;
	for (uint32_t i = 0, binding_idx = 0; i < (uint32_t)total_arg_count; ++i) {
		// fully ignore argument buffer args, these are encoded as separate descriptor sets
		if (i < explicit_arg_count && info.args[i].special_type == SPECIAL_TYPE::ARGUMENT_BUFFER) {
			continue;
		}
		
		layout.bindings.push_back({});
		auto& binding = layout.bindings.back();
		
		binding.binding = binding_idx;
		binding.descriptorCount = 1;
		binding.stageFlags = stage;
		binding.pImmutableSamplers = nullptr;
		
		if (i < explicit_arg_count) {
			const auto& arg = info.args[i];
			switch (arg.address_space) {
				// image
				case ARG_ADDRESS_SPACE::IMAGE: {
					const bool is_image_array = (arg.special_type == SPECIAL_TYPE::IMAGE_ARRAY);
					if (is_image_array) {
						binding.descriptorCount = arg.size;
					}
					switch (arg.image_access) {
						case ARG_IMAGE_ACCESS::READ:
							binding.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
							if (is_image_array) {
								layout.read_image_desc += arg.size;
							} else {
								++layout.read_image_desc;
							}
							break;
						case ARG_IMAGE_ACCESS::WRITE:
							binding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
							binding.descriptorCount *= dev.max_mip_levels;
							if (is_image_array) {
								layout.write_image_desc += arg.size * dev.max_mip_levels;
							} else {
								layout.write_image_desc += dev.max_mip_levels;
							}
							break;
						case ARG_IMAGE_ACCESS::READ_WRITE: {
							if (is_image_array) {
								log_error("read/write image array not supported (arg #$ in $)", i, func_name);
								return {};
							}
							
							// need to add both a sampled one and a storage one
							binding.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
							layout.legacy.desc_types.emplace_back(VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE);
							++binding_idx;
							layout.bindings.emplace_back(VkDescriptorSetLayoutBinding {
								.binding = binding_idx,
								.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
								.descriptorCount = dev.max_mip_levels,
								.stageFlags = VkShaderStageFlags(stage),
								.pImmutableSamplers = nullptr,
							});
							++layout.read_image_desc;
							layout.write_image_desc += dev.max_mip_levels;
							break;
						}
						case ARG_IMAGE_ACCESS::NONE:
							log_error("unknown image access type (arg #$ in $)", i, func_name);
							valid_desc = false;
							break;
					}
					break;
				}
				// buffer and param (there are no proper constant parameters)
				case ARG_ADDRESS_SPACE::GLOBAL:
				case ARG_ADDRESS_SPACE::CONSTANT:
					// NOTE: buffers are always SSBOs
					// NOTE: uniforms/param can either be SSBOs or IUBs, depending on their size and device support
					if (arg.special_type == SPECIAL_TYPE::IUB) {
						binding.descriptorType = VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK;
						// descriptor count == size, which must be a multiple of 4
						const uint32_t arg_size_4 = ((arg.size + 3u) / 4u) * 4u;
						layout.max_iub_size = max(arg_size_4, layout.max_iub_size);
						binding.descriptorCount = arg_size_4;
						++layout.iub_desc;
					} else if (arg.special_type == SPECIAL_TYPE::BUFFER_ARRAY) {
						binding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
						binding.descriptorCount = arg.size;
						layout.ssbo_desc += arg.size;
					} else {
						binding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
						++layout.ssbo_desc;
						
						// put all non-IUB constant args into a single constant buffer that is allocated only once
						if (arg.address_space == ARG_ADDRESS_SPACE::CONSTANT) {
							assert(arg.size > 0);
							// offsets in SSBOs must be aligned to minStorageBufferOffsetAlignment
							const auto ssbo_alignment = vk_dev.min_storage_buffer_offset_alignment;
							layout.constant_buffer_size = ((layout.constant_buffer_size + ssbo_alignment - 1u) / ssbo_alignment) * ssbo_alignment;
							layout.constant_buffer_info.emplace(uint32_t(i), vulkan_constant_buffer_info_t {
								.offset = layout.constant_buffer_size,
								.size = arg.size,
							});
							layout.constant_buffer_size += arg.size;
							++layout.constant_arg_count;
						}
					}
					break;
				case ARG_ADDRESS_SPACE::LOCAL:
					log_error("arg with a local address space is not supported (arg #$ in $)", i, func_name);
					valid_desc = false;
					break;
				case ARG_ADDRESS_SPACE::UNKNOWN:
					if (arg.special_type == SPECIAL_TYPE::STAGE_INPUT) {
						// ignore + compact
						layout.bindings.pop_back();
						continue;
					}
					log_error("arg with an unknown address space (arg #$ in $)", i, func_name);
					valid_desc = false;
					break;
			}
		} else {
			// implicit args
			if (i < total_arg_count) {
				const auto implicit_arg_num = i - explicit_arg_count;
				if (is_soft_printf && implicit_arg_num == 0) {
					// printf buffer is always a SSBO
					binding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
					++layout.ssbo_desc;
				} else {
					log_error("invalid implicit arg count for function \"$\": $ is out-of-bounds", func_name, implicit_arg_num);
					valid_desc = false;
				}
			} else {
				log_error("invalid arg count for function \"$\": $ is out-of-bounds", func_name, i);
				valid_desc = false;
			}
		}
		if (!valid_desc) {
			break;
		}
		
		layout.legacy.desc_types.emplace_back(binding.descriptorType);
		++binding_idx;
	}
	if (!valid_desc) {
		log_error("invalid descriptor bindings for function \"$\" for device \"$\"!", func_name, dev.name);
		return {};
	}
	
	// always create a descriptor set layout, even when it's empty (we still need to be able to set/skip it later on)
	const VkDescriptorSetLayoutCreateInfo desc_set_layout_info {
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
		.pNext = nullptr,
		.flags = (!is_legacy ? VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT : 0),
		.bindingCount = (uint32_t)layout.bindings.size(),
		.pBindings = (!layout.bindings.empty() ? layout.bindings.data() : nullptr),
	};
	VK_CALL_RET(vkCreateDescriptorSetLayout(vk_dev.device, &desc_set_layout_info, nullptr, &layout.desc_set_layout),
				"failed to create descriptor set layout (" + func_name + ")", {})
	// TODO: vkDestroyDescriptorSetLayout cleanup
	
	return layout;
}

static bool allocate_constant_buffers(vulkan_kernel::vulkan_kernel_entry& entry,
									  const compute_device& dev,
									  const string& func_name,
									  const vulkan_descriptor_set_layout_t& layout) {
	if (layout.constant_buffer_size > 0) {
		assert(!entry.constant_buffer_info.empty() && "should have already moved the constant buffer info");
		
		// align size to 16 bytes
		const uint32_t alignment_pot = 4u, alignment = 1u << alignment_pot;
		const auto constant_buffer_size = ((layout.constant_buffer_size + alignment - 1u) / alignment) * alignment;
		const auto& ctx = *(const vulkan_compute*)dev.context;
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
				return false;
			}
			constant_buffers[buf_idx] = entry.constant_buffers_storage[buf_idx].get();
		}
		entry.constant_buffers = make_unique<safe_resource_container<compute_buffer*, vulkan_descriptor_set_container::descriptor_count>>(std::move(constant_buffers));
	}
	return true;
}

static bool create_kernel_entry_legacy(vulkan_kernel::vulkan_kernel_entry& entry,
									   const compute_device& dev,
									   const VkShaderStageFlagBits stage,
									   const string& func_name,
									   const llvm_toolchain::function_info& info) {
	const auto& vk_dev = (const vulkan_device&)dev;
	
	if (has_flag<llvm_toolchain::FUNCTION_FLAGS::USES_VULKAN_DESCRIPTOR_BUFFER>(info.flags)) {
		log_error("can't use a function that has been built for descriptor buffer use with legacy descriptor sets: in function $", func_name);
		return false;
	}
	
	auto layout = vulkan_program::build_descriptor_set_layout(dev, func_name, info, stage, true);
	if (!layout) {
		return false;
	}
	entry.desc_set_layout = layout->desc_set_layout;
	entry.constant_buffer_info = std::move(layout->constant_buffer_info);
	entry.desc_legacy.desc_types = std::move(layout->legacy.desc_types);
	
	if (!layout->bindings.empty()) {
		// create descriptor pool + descriptors
		// TODO: think about how this can be properly handled (creating a pool per function per device is probably not a good idea)
		//       -> create a descriptor allocation handler, start with a large vkCreateDescriptorPool,
		//          then create new ones if allocation fails (due to fragmentation)
		//          DO NOT use VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT
		const uint32_t pool_count = ((layout->ssbo_desc > 0 ? 1 : 0) +
									 (layout->iub_desc > 0 ? 1 : 0) +
									 (layout->read_image_desc > 0 ? 1 : 0) +
									 (layout->write_image_desc > 0 ? 1 : 0));
		vector<VkDescriptorPoolSize> pool_sizes(max(pool_count, 1u));
		uint32_t pool_index = 0;
		if (layout->ssbo_desc > 0 || pool_count == 0) {
			pool_sizes[pool_index].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
			pool_sizes[pool_index].descriptorCount = (layout->ssbo_desc > 0 ? layout->ssbo_desc : 1) * vulkan_descriptor_set_container::descriptor_count;
			++pool_index;
		}
		if (layout->iub_desc > 0) {
			pool_sizes[pool_index].type = VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK;
			// amount of bytes to allocate for descriptors of this type
			// -> use max arg size here
			// TODO/NOTE: no idea where the size requirement comes from -> just round to multiples of 128 for now
			pool_sizes[pool_index].descriptorCount = (const_math::round_next_multiple(layout->max_iub_size + 128u, 128u) *
													  vulkan_descriptor_set_container::descriptor_count);
			++pool_index;
		}
		if (layout->read_image_desc > 0) {
			pool_sizes[pool_index].type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
			pool_sizes[pool_index].descriptorCount = layout->read_image_desc * vulkan_descriptor_set_container::descriptor_count;
			++pool_index;
		}
		if (layout->write_image_desc > 0) {
			pool_sizes[pool_index].type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
			pool_sizes[pool_index].descriptorCount = layout->write_image_desc * vulkan_descriptor_set_container::descriptor_count;
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
		VkDescriptorPoolInlineUniformBlockCreateInfo iub_pool_info;
		if (layout->iub_desc > 0) {
			desc_pool_info.pNext = &iub_pool_info;
			iub_pool_info = {
				.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_INLINE_UNIFORM_BLOCK_CREATE_INFO,
				.pNext = nullptr,
				.maxInlineUniformBlockBindings = layout->iub_desc,
			};
		}
		VK_CALL_RET(vkCreateDescriptorPool(vk_dev.device, &desc_pool_info, nullptr, &entry.desc_legacy.desc_pool),
					"failed to create descriptor pool (" + func_name + ")", false)
		
		// allocate fixed number of descriptor sets
		array<VkDescriptorSet, vulkan_descriptor_set_container::descriptor_count> desc_sets;
		array<VkDescriptorSetLayout, vulkan_descriptor_set_container::descriptor_count> desc_set_layouts;
		desc_set_layouts.fill(entry.desc_set_layout);
		const VkDescriptorSetAllocateInfo desc_set_alloc_info {
			.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
			.pNext = nullptr,
			.descriptorPool = entry.desc_legacy.desc_pool,
			.descriptorSetCount = vulkan_descriptor_set_container::descriptor_count,
			.pSetLayouts = &desc_set_layouts[0],
		};
		VK_CALL_RET(vkAllocateDescriptorSets(vk_dev.device, &desc_set_alloc_info, &desc_sets[0]),
					"failed to allocate descriptor sets (" + func_name + ")", false)
#if defined(FLOOR_DEBUG)
		string desc_set_label_stem = "desc_set:" + func_name + "#";
		for (uint32_t desc_set_idx = 0; desc_set_idx < vulkan_descriptor_set_container::descriptor_count; ++desc_set_idx) {
			((const vulkan_compute*)dev.context)->set_vulkan_debug_label(vk_dev, VK_OBJECT_TYPE_DESCRIPTOR_SET,
																		 uint64_t(desc_sets[desc_set_idx]),
																		 desc_set_label_stem + to_string(desc_set_idx));
			
		}
#endif
		entry.desc_legacy.desc_set_container = make_unique<vulkan_descriptor_set_container>(std::move(desc_sets));
		
		if (!allocate_constant_buffers(entry, dev, func_name, *layout)) {
			return false;
		}
	}
	// else: no descriptors entry.desc_* already nullptr
	
	return true;
}

static bool create_kernel_entry_descriptor_buffer(vulkan_kernel::vulkan_kernel_entry& entry,
												  const compute_device& dev,
												  const VkShaderStageFlagBits stage,
												  const string& func_name,
												  const llvm_toolchain::function_info& info) {
	const auto& vk_dev = (const vulkan_device&)dev;
	auto& vk_ctx = *(vulkan_compute*)vk_dev.context;
	const auto dev_queue = vk_ctx.get_device_default_queue(vk_dev);
	assert(dev_queue != nullptr);
	
	if (!has_flag<llvm_toolchain::FUNCTION_FLAGS::USES_VULKAN_DESCRIPTOR_BUFFER>(info.flags)) {
		log_error("can't use a function that has not been built with descriptor buffer support with descriptor buffers: in function $", func_name);
		return false;
	}
	
	auto layout = vulkan_program::build_descriptor_set_layout(dev, func_name, info, stage, false);
	if (!layout) {
		return false;
	}
	entry.desc_set_layout = layout->desc_set_layout;
	entry.constant_buffer_info = std::move(layout->constant_buffer_info);
	
	if (!layout->bindings.empty()) {
		// query required buffer size + ensure good alignment
		VkDeviceSize layout_size_in_bytes = 0;
		vk_ctx.vulkan_get_descriptor_set_layout_size(vk_dev.device, entry.desc_set_layout, &layout_size_in_bytes);
		layout_size_in_bytes = const_math::round_next_multiple(layout_size_in_bytes, uint64_t(256));
		
		// query offset for each binding/argument
		entry.desc_buffer.argument_offsets.reserve(layout->bindings.size());
		for (const auto& binding : layout->bindings) {
			VkDeviceSize offset = 0;
			vk_ctx.vulkan_get_descriptor_set_layout_binding_offset(vk_dev.device, entry.desc_set_layout, binding.binding, &offset);
			entry.desc_buffer.argument_offsets.emplace_back(offset);
		}
		
		// allocate descriptor buffers
		array<vulkan_descriptor_buffer_container::resource_type, vulkan_descriptor_buffer_container::descriptor_count> desc_buffers;
		for (uint32_t buf_idx = 0; buf_idx < vulkan_descriptor_buffer_container::descriptor_count; ++buf_idx) {
			// alloc with Vulkan flags: need host-visible/host-coherent and descriptor buffer usage flags
			desc_buffers[buf_idx].first = vk_ctx.create_buffer(*dev_queue, layout_size_in_bytes,
															   // NOTE: read-only on the device side (until writable argument buffers are implemented)
															   COMPUTE_MEMORY_FLAG::READ |
															   COMPUTE_MEMORY_FLAG::HOST_READ_WRITE |
															   COMPUTE_MEMORY_FLAG::VULKAN_HOST_COHERENT |
															   COMPUTE_MEMORY_FLAG::VULKAN_DESCRIPTOR_BUFFER);
			desc_buffers[buf_idx].first->set_debug_label("desc_buf:" + func_name + "#" + to_string(buf_idx));
			auto mapped_host_ptr = desc_buffers[buf_idx].first->map(*dev_queue, (COMPUTE_MEMORY_MAP_FLAG::WRITE_INVALIDATE |
																				 COMPUTE_MEMORY_MAP_FLAG::BLOCK));
			desc_buffers[buf_idx].second = { (uint8_t*)mapped_host_ptr, layout_size_in_bytes };
		}
		entry.desc_buffer.desc_buffer_container = make_unique<vulkan_descriptor_buffer_container>(std::move(desc_buffers));
		
		if (!allocate_constant_buffers(entry, dev, func_name, *layout)) {
			return false;
		}
	}
	// else: no function parameters/bindings
	
	// handle argument buffers -> need to create descriptor set layouts for these as well
	{
		for (uint32_t i = 0, explicit_arg_count = uint32_t(info.args.size()); i < explicit_arg_count; ++i) {
			if (info.args[i].special_type == SPECIAL_TYPE::ARGUMENT_BUFFER) {
				if (!info.args[i].argument_buffer_info) {
					log_error("function $ has an argument buffer at index $, but no argument buffer info exists", func_name, i);
					return false;
				}
				auto arg_buf_layout = vulkan_program::build_descriptor_set_layout(dev, func_name + ".arg_buffer@" + to_string(i),
																				  *info.args[i].argument_buffer_info, stage, false);
				if (!arg_buf_layout) {
					log_error("failed to create argument buffer descriptor set layout at index $ in function $", i, func_name);
					return false;
				}
				entry.argument_buffers.emplace_back(vulkan_kernel::vulkan_kernel_entry::argument_buffer_t {
					std::move(*arg_buf_layout)
				});
			}
		}
	}
	
	return true;
}

vulkan_program::vulkan_program(program_map_type&& programs_) : programs(std::move(programs_)) {
	if (programs.empty()) return;
	retrieve_unique_kernel_names(programs);
	
	// create all kernels of all device programs
	// note that this essentially reshuffles the program "device -> kernels" data to "kernels -> devices"
	kernels.reserve(kernel_names.size());
	for (const auto& func_name : kernel_names) {
		vulkan_kernel::kernel_map_type kernel_map;
		kernel_map.reserve(kernel_names.size());
		
		for (const auto& prog : programs) {
			if (!prog.second.valid) continue;
			
			for (const auto& info : prog.second.functions) {
				if (info.name == func_name) {
					vulkan_kernel::vulkan_kernel_entry entry;
					entry.info = &info;
					
					auto& prog_entry = prog.second;
					const auto& dev = prog.first.get();
					const auto& vk_dev = (const vulkan_device&)dev;
					
					const VkShaderStageFlagBits stage = (info.type == FUNCTION_TYPE::VERTEX ? VK_SHADER_STAGE_VERTEX_BIT :
														 info.type == FUNCTION_TYPE::FRAGMENT ? VK_SHADER_STAGE_FRAGMENT_BIT :
														 VK_SHADER_STAGE_COMPUTE_BIT /* should notice anything else earlier */);
					
					if (!info.has_valid_local_size()) {
						entry.max_local_size = dev.max_local_size;
						
						// always assume that we can execute this with the max possible work-group size,
						// i.e. use this as the initial default
						entry.max_total_local_size = dev.max_total_local_size;
					} else {
						// a required local size/dim is specified -> use it
						entry.max_local_size = info.local_size;
						entry.max_total_local_size = info.local_size.extent();
					}
					
					// TODO: make sure that _all_ of this is synchronized
					if (dev.descriptor_buffer_support) {
						if (!create_kernel_entry_descriptor_buffer(entry, dev, stage, func_name, info)) {
							continue;
						}
					} else {
						if (!create_kernel_entry_legacy(entry, dev, stage, func_name, info)) {
							continue;
						}
					}
					
					// find SPIR-V module index for this function
					const auto mod_iter = prog_entry.func_to_mod_map.find(func_name);
					if (mod_iter == prog_entry.func_to_mod_map.end()) {
						log_error("did not find a module mapping for function \"$\"", func_name);
						continue;
					}
					
					// stage info, can be used here or at a later point
					entry.stage_info = VkPipelineShaderStageCreateInfo {
						.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
						.pNext = nullptr,
						.flags = 0,
						.stage = stage,
						.module = prog_entry.programs[mod_iter->second],
						.pName = func_name.c_str(),
						.pSpecializationInfo = nullptr,
					};
					
					// we can only actually create compute pipelines here, because they can exist on their own
					// vertex/fragment/etc graphics pipelines would need much more information (which ones to combine to begin with)
					if (info.type == FUNCTION_TYPE::KERNEL) {
						// create the pipeline layout
						vector<VkDescriptorSetLayout> layouts {
							vk_dev.fixed_sampler_desc_set_layout,
							entry.desc_set_layout,
						};
						for (const auto& arg_buf : entry.argument_buffers) {
							layouts.emplace_back(arg_buf.layout.desc_set_layout);
						}
						const VkPipelineLayoutCreateInfo pipeline_layout_info {
							.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
							.pNext = nullptr,
							.flags = 0,
							.setLayoutCount = uint32_t(layouts.size()),
							.pSetLayouts = layouts.data(),
							.pushConstantRangeCount = 0,
							.pPushConstantRanges = nullptr,
						};
						VK_CALL_CONT(vkCreatePipelineLayout(vk_dev.device, &pipeline_layout_info, nullptr, &entry.pipeline_layout),
									 "failed to create pipeline layout (" + func_name + ")")
						
						GUARD(entry.specializations_lock);
						const uint3 work_group_size = (info.has_valid_local_size() ?
													   info.local_size :
													   uint3 { entry.max_total_local_size, 1, 1 });
						if (entry.specialize(vk_dev, work_group_size) == nullptr) {
							// NOTE: if specialization failed, this will have already printed an error
							continue;
						}
					}
					
					kernel_map.emplace_or_assign(std::move(prog.first), std::move(entry));
					break;
				}
			}
		}
		
		kernels.emplace_back(make_shared<vulkan_kernel>(std::move(kernel_map)));
	}
}

VkDescriptorSetLayout vulkan_program::get_empty_descriptor_set(const compute_device& dev) {
	// NOTE: this will create an empty descriptor upon call, but only once per device
	
	static safe_mutex empty_desc_set_lock;
	GUARD(empty_desc_set_lock);
	
	const auto& vk_dev = (const vulkan_device&)dev;
	static floor_core::flat_map<const compute_device&, VkDescriptorSetLayout> empty_descriptor_sets;
	const auto iter = empty_descriptor_sets.find(vk_dev);
	if (iter != empty_descriptor_sets.end()) {
		// already created
		return iter->second;
	}
	
	// first call for this device: create it
	const auto is_legacy = !vk_dev.descriptor_buffer_support;
	const VkDescriptorSetLayoutCreateInfo empty_desc_set_layout_info {
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
		.pNext = nullptr,
		.flags = (!is_legacy ? VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT : 0),
		.bindingCount = 0,
		.pBindings = nullptr,
	};
	VkDescriptorSetLayout empty_desc_set_layout {};
	VK_CALL_RET(vkCreateDescriptorSetLayout(vk_dev.device, &empty_desc_set_layout_info, nullptr, &empty_desc_set_layout),
				"failed to create empty descriptor set layout for device " + dev.name, nullptr)
	empty_descriptor_sets.insert(vk_dev, empty_desc_set_layout);
	
	return empty_desc_set_layout;
}

#endif
