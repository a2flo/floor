/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2021 Florian Ziesche
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
	vector<VkWriteDescriptorSet> write_descs;
	vector<VkWriteDescriptorSetInlineUniformBlockEXT> iub_descs;
	vector<shared_ptr<compute_buffer>> constant_buffers;
	vector<uint32_t> dyn_offsets;
	vector<shared_ptr<vector<VkDescriptorImageInfo>>> image_array_info;
	const VkPipeline pipeline { nullptr };
	const VkPipelineLayout pipeline_layout { nullptr };
	vector<descriptor_set_instance_t> acquired_descriptor_sets;
	//! if set, won't transition kernel/shader image arguments to read or write optimal layout during argument encoding
	//! NOTE: this is useful in cases we don't want to or can't have a pipeline barrier
	bool allow_generic_layout { false };
};

#endif
