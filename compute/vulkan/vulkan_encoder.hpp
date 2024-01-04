/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2024 Florian Ziesche
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

#ifndef __FLOOR_COMPUTE_VULKAN_VULKAN_ENCODER_HPP__
#define __FLOOR_COMPUTE_VULKAN_VULKAN_ENCODER_HPP__

// NOTE: only included from vulkan_kernel and vulkan_shader

struct vulkan_encoder {
	vulkan_command_buffer cmd_buffer;
	const vulkan_queue& cqueue;
	const vulkan_device& device;
	vector<shared_ptr<compute_buffer>> constant_buffers;
	const VkPipeline pipeline { nullptr };
	const VkPipelineLayout pipeline_layout { nullptr };
	const vector<const vulkan_kernel::vulkan_kernel_entry*> entries;
	vector<descriptor_buffer_instance_t> acquired_descriptor_buffers;
	vector<pair<uint32_t /* entry idx */, const vulkan_buffer*>> argument_buffers;
	vector<pair<compute_buffer*, uint32_t>> acquired_constant_buffers;
	vector<void*> constant_buffer_mappings;
	vector<unique_ptr<VkDescriptorBufferInfo>> constant_buffer_desc_info;
	//! for easier access later on: wrap constant buffer info (size must be == size of entries)
	vector<vulkan_args::constant_buffer_wrapper_t> constant_buffer_wrappers;
	//! NOTE: this is created automatically right before setting/handling all args
	vector<const vulkan_args::constant_buffer_wrapper_t*> constant_buffer_wrappers_ptr;
};

#endif
