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

#pragma once

#include <floor/core/essentials.hpp>

#if !defined(FLOOR_NO_VULKAN)
#include <floor/device/device_function.hpp>

namespace fl {

struct vulkan_function_entry : device_function::function_entry {
	VkPipelineLayout pipeline_layout { nullptr };
	VkPipelineShaderStageRequiredSubgroupSizeCreateInfo stage_sub_group_info;
	VkShaderModuleCreateInfo shader_module_info;
	VkPipelineShaderStageCreateInfo stage_info;
	VkDescriptorSetLayout desc_set_layout { nullptr };
	
	struct desc_buffer_t {
		VkDeviceSize layout_size_in_bytes { 0u };
		std::vector<VkDeviceSize> argument_offsets;
		std::unique_ptr<vulkan_descriptor_buffer_container> desc_buffer_container;
		//! for internal (clean up) use only
		std::array<std::pair<device_buffer*, void*>, vulkan_descriptor_buffer_container::descriptor_count> desc_buffer_ptrs {};
	} desc_buffer;
	
	//! buffers/storage for constant data
	//! NOTE: must be the same amount as the number of descriptors in "desc_set_container"
	std::array<std::shared_ptr<device_buffer>, vulkan_descriptor_buffer_container::descriptor_count> constant_buffers_storage;
	std::array<void*, vulkan_descriptor_buffer_container::descriptor_count> constant_buffer_mappings;
	std::unique_ptr<safe_resource_container<device_buffer*, vulkan_descriptor_buffer_container::descriptor_count>> constant_buffers;
	//! argument index -> constant buffer info
	fl::flat_map<uint32_t, vulkan_constant_buffer_info_t> constant_buffer_info;
	
	//! argument buffer data
	struct argument_buffer_t {
		vulkan_descriptor_set_layout_t layout;
	};
	std::vector<argument_buffer_t> argument_buffers;
	
	struct spec_entry {
		VkPipeline pipeline { nullptr };
		VkSpecializationInfo info;
		std::vector<VkSpecializationMapEntry> map_entries;
		std::vector<uint32_t> data;
	};
	// must sync access to specializations
	atomic_spin_lock specializations_lock;
	// work-group size -> spec entry
	fl::flat_map<uint64_t, spec_entry> specializations GUARDED_BY(specializations_lock);
	
	//! creates a 64-bit key out of the specified ushort3 work-group size and SIMD width
	static uint64_t make_spec_key(const ushort3 work_group_size, const uint16_t simd_width);
	
	//! specializes/builds a compute pipeline for the specified work-group size and SIMD width
	vulkan_function_entry::spec_entry* specialize(const vulkan_device& dev,
												  const ushort3 work_group_size,
												  const uint16_t simd_width) REQUIRES(specializations_lock);
};

} // namespace fl

#endif // FLOOR_NO_VULKAN
