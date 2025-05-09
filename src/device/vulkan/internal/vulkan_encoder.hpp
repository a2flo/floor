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

#pragma once

#include <floor/core/essentials.hpp>

#if !defined(FLOOR_NO_VULKAN)

namespace fl {

struct vulkan_function_entry;

struct vulkan_encoder {
	vulkan_command_buffer cmd_buffer;
	const vulkan_queue& cqueue;
	const vulkan_device& dev;
	std::vector<std::shared_ptr<device_buffer>> constant_buffers;
	const VkPipeline pipeline { nullptr };
	const VkPipelineLayout pipeline_layout { nullptr };
	const std::vector<const vulkan_function_entry*> entries;
	std::vector<descriptor_buffer_instance_t> acquired_descriptor_buffers;
	std::vector<std::pair<uint32_t /* entry idx */, const vulkan_buffer*>> argument_buffers;
	std::vector<std::pair<device_buffer*, uint32_t>> acquired_constant_buffers;
	std::vector<void*> constant_buffer_mappings;
	std::vector<std::unique_ptr<VkDescriptorBufferInfo>> constant_buffer_desc_info;
	//! for easier access later on: wrap constant buffer info (size must be == size of entries)
	std::vector<vulkan_args::constant_buffer_wrapper_t> constant_buffer_wrappers;
	//! NOTE: this is created automatically right before setting/handling all args
	std::vector<const vulkan_args::constant_buffer_wrapper_t*> constant_buffer_wrappers_ptr;
	
#if defined(FLOOR_DEBUG)
	std::string debug_label;
#endif
};

} // namespace fl

#endif // FLOOR_NO_VULKAN
