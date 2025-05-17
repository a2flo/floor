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

#if !defined(FLOOR_NO_VULKAN)
#include "internal/vulkan_headers.hpp"
#include <floor/device/vulkan/vulkan_program.hpp>
#include <floor/device/vulkan/vulkan_function.hpp>
#include <floor/device/vulkan/vulkan_device.hpp>
#include <floor/device/vulkan/vulkan_context.hpp>
#include "internal/vulkan_descriptor_set.hpp"
#include "internal/vulkan_function_entry.hpp"

namespace fl {

using namespace toolchain;

static std::optional<vulkan_descriptor_set_layout_t> build_descriptor_set_layout(const device& dev,
																				 const std::string& func_name,
																				 const toolchain::function_info& info,
																				 const VkShaderStageFlagBits stage) {
	const auto& vk_dev = (const vulkan_device&)dev;
	
	// handle implicit args which add to the total #args
	const auto is_soft_printf = has_flag<FUNCTION_FLAGS::USES_SOFT_PRINTF>(info.flags);
	const uint32_t implicit_arg_count = (is_soft_printf ? 1u : 0u);
	const uint32_t explicit_arg_count = uint32_t(info.args.size());
	const uint32_t total_arg_count = explicit_arg_count + implicit_arg_count;
	
	// currently, we always build universal binaries using a fixed map mip level count of 16
	// -> need to account for this in here if it doesn't match the device max mip level count
	// TODO: more flexibility, add parameter to FUBAR vulkan target
	const auto max_mip_levels = (info.is_fubar ? vulkan_device::builtin_max_mip_levels : dev.max_mip_levels);
	
	// create function + device specific descriptor set layout
	vulkan_descriptor_set_layout_t layout {};
	layout.bindings.reserve(total_arg_count);
	bool valid_desc = true;
	for (uint32_t i = 0, binding_idx = 0; i < (uint32_t)total_arg_count; ++i) {
		// fully ignore argument buffer args, these are encoded as separate descriptor sets
		if (i < explicit_arg_count && has_flag<ARG_FLAG::ARGUMENT_BUFFER>(info.args[i].flags)) {
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
					const bool is_image_array = has_flag<ARG_FLAG::IMAGE_ARRAY>(arg.flags);
					if (is_image_array) {
						assert(arg.array_extent > 0u && arg.array_extent <= 0xFFFF'FFFFu);
						binding.descriptorCount = uint32_t(arg.array_extent);
					}
					switch (arg.access) {
						case ARG_ACCESS::READ:
							binding.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
							if (is_image_array) {
								layout.read_image_desc += arg.size;
							} else {
								++layout.read_image_desc;
							}
							break;
						case ARG_ACCESS::WRITE:
							binding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
							binding.descriptorCount *= max_mip_levels;
							if (is_image_array) {
								layout.write_image_desc += arg.size * max_mip_levels;
							} else {
								layout.write_image_desc += max_mip_levels;
							}
							break;
						case ARG_ACCESS::READ_WRITE: {
							if (is_image_array) {
								log_error("read/write image array not supported (arg #$ in $)", i, func_name);
								return {};
							}
							
							// need to add both a sampled one and a storage one
							binding.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
							++binding_idx;
							layout.bindings.emplace_back(VkDescriptorSetLayoutBinding {
								.binding = binding_idx,
								.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
								.descriptorCount = max_mip_levels,
								.stageFlags = VkShaderStageFlags(stage),
								.pImmutableSamplers = nullptr,
							});
							++layout.read_image_desc;
							layout.write_image_desc += max_mip_levels;
							break;
						}
						case ARG_ACCESS::UNSPECIFIED:
							log_error("unspecified image access type (arg #$ in $)", i, func_name);
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
					if (has_flag<ARG_FLAG::IUB>(arg.flags)) {
						binding.descriptorType = VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK;
						// descriptor count == size, which must be a multiple of 4
						assert(arg.size <= 0xFFFF'FFFFu);
						const uint32_t arg_size_4 = const_math::round_next_multiple(uint32_t(arg.size), 4u);
						layout.max_iub_size = std::max(arg_size_4, layout.max_iub_size);
						binding.descriptorCount = arg_size_4;
						++layout.iub_desc;
					} else if (has_flag<ARG_FLAG::BUFFER_ARRAY>(arg.flags)) {
						binding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
						assert(arg.array_extent > 0u && arg.array_extent <= 0xFFFF'FFFFu);
						binding.descriptorCount = uint32_t(arg.array_extent);
						layout.ssbo_desc += arg.size;
					} else {
						binding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
						++layout.ssbo_desc;
						
						// put all non-IUB constant args into a single constant buffer that is allocated only once
						if (arg.address_space == ARG_ADDRESS_SPACE::CONSTANT) {
							assert(arg.size > 0u && arg.size <= 0xFFFF'FFFFu);
							// offsets in SSBOs must be aligned to minStorageBufferOffsetAlignment
							const auto ssbo_alignment = vk_dev.min_storage_buffer_offset_alignment;
							layout.constant_buffer_size = ((layout.constant_buffer_size + ssbo_alignment - 1u) / ssbo_alignment) * ssbo_alignment;
							layout.constant_buffer_info.emplace(uint32_t(i), vulkan_constant_buffer_info_t {
								.offset = layout.constant_buffer_size,
								.size = uint32_t(arg.size),
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
					if (has_flag<ARG_FLAG::STAGE_INPUT>(arg.flags)) {
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
		.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT,
		.bindingCount = (uint32_t)layout.bindings.size(),
		.pBindings = (!layout.bindings.empty() ? layout.bindings.data() : nullptr),
	};
	VK_CALL_RET(vkCreateDescriptorSetLayout(vk_dev.device, &desc_set_layout_info, nullptr, &layout.desc_set_layout),
				"failed to create descriptor set layout (" + func_name + ")", {})
	// TODO: vkDestroyDescriptorSetLayout cleanup
	
	// retrieve and cache layout size
	vkGetDescriptorSetLayoutSizeEXT(vk_dev.device, layout.desc_set_layout, &layout.layout_size);
	
	return layout;
}

static bool allocate_constant_buffers(vulkan_function_entry& entry,
									  const device& dev,
									  const std::string& func_name,
									  const vulkan_descriptor_set_layout_t& layout) {
	if (layout.constant_buffer_size > 0) {
		assert(!entry.constant_buffer_info.empty() && "should have already moved the constant buffer info");
		
		// align size to 16 bytes
		const uint32_t alignment_pot = 4u, alignment = 1u << alignment_pot;
		const auto constant_buffer_size = ((layout.constant_buffer_size + alignment - 1u) / alignment) * alignment;
		const auto& ctx = *(const vulkan_context*)dev.context;
		const auto dev_queue = ctx.get_device_default_queue(dev);
		assert(dev_queue != nullptr);
		
		// we need to allocate as many buffers as we have descriptor sets
		std::array<device_buffer*, vulkan_descriptor_buffer_container::descriptor_count> constant_buffers;
#if defined(FLOOR_DEBUG)
		std::string const_buffer_label_stem = "const_buf:" + func_name + "#";
#endif
		for (uint32_t buf_idx = 0; buf_idx < vulkan_descriptor_buffer_container::descriptor_count; ++buf_idx) {
			// allocate in device-local/host-coherent memory
			entry.constant_buffers_storage[buf_idx] = ctx.create_buffer(*dev_queue, constant_buffer_size,
																		MEMORY_FLAG::READ | MEMORY_FLAG::HOST_WRITE |
																		MEMORY_FLAG::VULKAN_HOST_COHERENT);
#if defined(FLOOR_DEBUG)
			entry.constant_buffers_storage[buf_idx]->set_debug_label(const_buffer_label_stem + std::to_string(buf_idx));
#endif
			entry.constant_buffer_mappings[buf_idx] = entry.constant_buffers_storage[buf_idx]->map(*dev_queue,
																								   MEMORY_MAP_FLAG::WRITE_INVALIDATE |
																								   MEMORY_MAP_FLAG::BLOCK);
			if (entry.constant_buffer_mappings[buf_idx] == nullptr) {
				log_error("failed to memory map constant buffer #$ for function $", buf_idx, func_name);
				return false;
			}
			constant_buffers[buf_idx] = entry.constant_buffers_storage[buf_idx].get();
		}
		entry.constant_buffers = std::make_unique<safe_resource_container<device_buffer*, vulkan_descriptor_buffer_container::descriptor_count>>(std::move(constant_buffers));
	}
	return true;
}

static bool create_function_entry_descriptor_buffer(vulkan_function_entry& entry,
												  const device& dev,
												  const VkShaderStageFlagBits stage,
												  const std::string& func_name,
												  const toolchain::function_info& info) {
	const auto& vk_dev = (const vulkan_device&)dev;
	auto& vk_ctx = *(vulkan_context*)vk_dev.context;
	const auto dev_queue = vk_ctx.get_device_default_queue(vk_dev);
	assert(dev_queue != nullptr);
	
	if (!has_flag<toolchain::FUNCTION_FLAGS::USES_VULKAN_DESCRIPTOR_BUFFER>(info.flags)) {
		log_error("can't use a function that has not been built with descriptor buffer support with descriptor buffers: in function $", func_name);
		return false;
	}
	
	auto layout = build_descriptor_set_layout(dev, func_name, info, stage);
	if (!layout) {
		return false;
	}
	entry.desc_set_layout = layout->desc_set_layout;
	entry.constant_buffer_info = std::move(layout->constant_buffer_info);
	
	if (!layout->bindings.empty()) {
		// query required buffer size + ensure good alignment
		entry.desc_buffer.layout_size_in_bytes = layout->layout_size;
		entry.desc_buffer.layout_size_in_bytes = const_math::round_next_multiple(entry.desc_buffer.layout_size_in_bytes,
																				 uint64_t(std::max(256u, vk_dev.descriptor_buffer_offset_alignment)));
		
		// query offset for each binding/argument
		entry.desc_buffer.argument_offsets.reserve(layout->bindings.size());
		for (const auto& binding : layout->bindings) {
			VkDeviceSize offset = 0;
			vkGetDescriptorSetLayoutBindingOffsetEXT(vk_dev.device, entry.desc_set_layout, binding.binding, &offset);
			entry.desc_buffer.argument_offsets.emplace_back(offset);
		}
		
		// allocate descriptor buffers
		std::array<vulkan_descriptor_buffer_container::resource_type, vulkan_descriptor_buffer_container::descriptor_count> desc_buffers;
		for (uint32_t buf_idx = 0; buf_idx < vulkan_descriptor_buffer_container::descriptor_count; ++buf_idx) {
			// alloc with Vulkan flags: need host-visible/host-coherent and descriptor buffer usage flags
			desc_buffers[buf_idx].first = vk_ctx.create_buffer(*dev_queue, entry.desc_buffer.layout_size_in_bytes,
															   // NOTE: read-only on the device side (until writable argument buffers are implemented)
															   MEMORY_FLAG::READ |
															   MEMORY_FLAG::HOST_READ_WRITE |
															   MEMORY_FLAG::VULKAN_HOST_COHERENT |
															   MEMORY_FLAG::VULKAN_DESCRIPTOR_BUFFER);
			desc_buffers[buf_idx].first->set_debug_label("desc_buf:" + func_name + "#" + std::to_string(buf_idx));
			auto mapped_host_ptr = desc_buffers[buf_idx].first->map(*dev_queue, (MEMORY_MAP_FLAG::WRITE_INVALIDATE |
																				 MEMORY_MAP_FLAG::BLOCK));
			desc_buffers[buf_idx].second = { (uint8_t*)mapped_host_ptr, entry.desc_buffer.layout_size_in_bytes };
		}
		entry.desc_buffer.desc_buffer_container = std::make_unique<vulkan_descriptor_buffer_container>(std::move(desc_buffers));
		
		if (!allocate_constant_buffers(entry, dev, func_name, *layout)) {
			return false;
		}
	}
	// else: no function parameters/bindings
	
	// handle argument buffers -> need to create descriptor set layouts for these as well
	{
		for (uint32_t i = 0, explicit_arg_count = uint32_t(info.args.size()); i < explicit_arg_count; ++i) {
			if (has_flag<ARG_FLAG::ARGUMENT_BUFFER>(info.args[i].flags)) {
				if (!info.args[i].argument_buffer_info) {
					log_error("function $ has an argument buffer at index $, but no argument buffer info exists", func_name, i);
					return false;
				}
				auto arg_buf_layout = build_descriptor_set_layout(dev, func_name + ".arg_buffer@" + std::to_string(i),
																  *info.args[i].argument_buffer_info, stage);
				if (!arg_buf_layout) {
					log_error("failed to create argument buffer descriptor set layout at index $ in function $", i, func_name);
					return false;
				}
				entry.argument_buffers.emplace_back(vulkan_function_entry::argument_buffer_t {
					std::move(*arg_buf_layout)
				});
			}
		}
	}
	
	return true;
}

vulkan_program::vulkan_program(program_map_type&& programs_) :
device_program(retrieve_unique_function_names(programs_)), programs(std::move(programs_)) {
	if (programs.empty()) {
		return;
	}
	
	// create all functions of all device programs
	// note that this essentially reshuffles the program "device -> functions" data to "functions -> devices"
	functions.reserve(function_names.size());
	for (const auto& func_name : function_names) {
		vulkan_function::function_map_type function_map;
		for (auto&& prog : programs) {
			if (!prog.second.valid) continue;
			
			for (const auto& info : prog.second.functions) {
				if (info.name == func_name) {
					auto entry = std::make_shared<vulkan_function_entry>();
					entry->info = &info;
					
					auto& prog_entry = prog.second;
					const auto dev = prog.first;
					const auto& vk_dev = (const vulkan_device&)*dev;
					
					const VkShaderStageFlagBits stage = (info.type == FUNCTION_TYPE::VERTEX ? VK_SHADER_STAGE_VERTEX_BIT :
														 info.type == FUNCTION_TYPE::FRAGMENT ? VK_SHADER_STAGE_FRAGMENT_BIT :
														 VK_SHADER_STAGE_COMPUTE_BIT /* should notice anything else earlier */);
					
					if (!info.has_valid_required_local_size()) {
						entry->max_local_size = vk_dev.max_local_size;
						
						// always assume that we can execute this with the max possible work-group size,
						// i.e. use this as the initial default
						entry->max_total_local_size = vk_dev.max_total_local_size;
					} else {
						// a required local size/dim is specified -> use it
						entry->max_local_size = info.required_local_size;
						entry->max_total_local_size = info.required_local_size.extent();
					}
					
					// TODO: make sure that _all_ of this is synchronized
					if (!create_function_entry_descriptor_buffer(*entry, *dev, stage, func_name, info)) {
						continue;
					}
					
					// find SPIR-V module index for this function
					const auto mod_iter = prog_entry.func_to_mod_map.find(func_name);
					if (mod_iter == prog_entry.func_to_mod_map.end()) {
						log_error("did not find a module mapping for function \"$\"", func_name);
						continue;
					}
					
					// stage info, can be used here or at a later point
					entry->stage_sub_group_info = VkPipelineShaderStageRequiredSubgroupSizeCreateInfo {
						.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_REQUIRED_SUBGROUP_SIZE_CREATE_INFO,
						.pNext = nullptr,
						// TODO: sub-group size / SIMD-width must really be stored in function info,
						// this may not work if the device supports a SIMD-width range and the program has not been compiled for the default width
						.requiredSubgroupSize = vk_dev.simd_width,
					};
					entry->stage_info = VkPipelineShaderStageCreateInfo {
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
						std::vector<VkDescriptorSetLayout> layouts {
							vk_dev.fixed_sampler_desc_set_layout,
							entry->desc_set_layout,
						};
						for (const auto& arg_buf : entry->argument_buffers) {
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
						VK_CALL_CONT(vkCreatePipelineLayout(vk_dev.device, &pipeline_layout_info, nullptr, &entry->pipeline_layout),
									 "failed to create pipeline layout (" + func_name + ")")
						
						GUARD(entry->specializations_lock);
						const uint3 work_group_size = (info.has_valid_required_local_size() ?
													   info.required_local_size :
													   uint3 { entry->max_total_local_size, 1, 1 });
						if (std::popcount(work_group_size.x) != 1u) {
							log_error("work-group size X ($) must be a power-of-two in function \"$\"",
									  work_group_size.x, func_name);
							continue;
						}
						const auto work_group_size_extent = work_group_size.extent();
						if ((work_group_size_extent % entry->stage_sub_group_info.requiredSubgroupSize) != 0) {
							log_error("work-group size ($) must be a multiple of the sub-group size ($) in function \"$\"",
									  work_group_size_extent, entry->stage_sub_group_info.requiredSubgroupSize, func_name);
							continue;
						}
						if (entry->specialize(vk_dev, work_group_size) == nullptr) {
							// NOTE: if specialization failed, this will have already printed an error
							continue;
						}
					}
					
					function_map.emplace(&*dev, std::move(entry));
					break;
				}
			}
		}
		
		functions.emplace_back(std::make_shared<vulkan_function>(func_name, std::move(function_map)));
	}
}

VkDescriptorSetLayout vulkan_program::get_empty_descriptor_set(const device& dev) {
	// NOTE: this will create an empty descriptor upon call, but only once per device
	
	static safe_mutex empty_desc_set_lock;
	GUARD(empty_desc_set_lock);
	
	const auto& vk_dev = (const vulkan_device&)dev;
	static fl::flat_map<const device*, VkDescriptorSetLayout> empty_descriptor_sets;
	const auto iter = empty_descriptor_sets.find(&vk_dev);
	if (iter != empty_descriptor_sets.end()) {
		// already created
		return iter->second;
	}
	
	// first call for this device: create it
	const VkDescriptorSetLayoutCreateInfo empty_desc_set_layout_info {
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
		.pNext = nullptr,
		.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT,
		.bindingCount = 0,
		.pBindings = nullptr,
	};
	VkDescriptorSetLayout empty_desc_set_layout {};
	VK_CALL_RET(vkCreateDescriptorSetLayout(vk_dev.device, &empty_desc_set_layout_info, nullptr, &empty_desc_set_layout),
				"failed to create empty descriptor set layout for device " + dev.name, nullptr)
	empty_descriptor_sets.emplace(&dev, empty_desc_set_layout);
	
	return empty_desc_set_layout;
}

} // namespace fl

#endif
