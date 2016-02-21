/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2016 Florian Ziesche
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

#include <floor/compute/vulkan/vulkan_kernel.hpp>

#if !defined(FLOOR_NO_VULKAN)
#include <floor/compute/vulkan/vulkan_common.hpp>
#include <floor/compute/vulkan/vulkan_queue.hpp>
#include <floor/compute/vulkan/vulkan_device.hpp>

struct vulkan_kernel::vulkan_encoder {
	vulkan_queue::command_buffer cmd_buffer;
	vector<VkWriteDescriptorSet> write_descs;
};

vulkan_kernel::vulkan_kernel(kernel_map_type&& kernels_) : kernels(move(kernels_)) {
}

vulkan_kernel::~vulkan_kernel() {}

typename vulkan_kernel::kernel_map_type::const_iterator vulkan_kernel::get_kernel(const compute_queue* queue) const {
	return kernels.find((vulkan_device*)queue->get_device().get());
}

shared_ptr<vulkan_kernel::vulkan_encoder> vulkan_kernel::create_encoder(compute_queue* queue, const vulkan_kernel_entry& entry, bool& success) {
	success = false;
	auto encoder = make_shared<vulkan_encoder>(vulkan_encoder {
		.cmd_buffer = ((vulkan_queue*)queue)->make_command_buffer(),
	});
	
	if(encoder->cmd_buffer.cmd_buffer == nullptr) {
		return {};
	}

	// begin recording
	const VkCommandBufferBeginInfo begin_info {
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
		.pNext = nullptr,
		.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
		.pInheritanceInfo = nullptr,
	};
	VK_CALL_RET(vkBeginCommandBuffer(encoder->cmd_buffer.cmd_buffer, &begin_info),
				"failed to begin command buffer", {});
	
	vkCmdBindPipeline(encoder->cmd_buffer.cmd_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, entry.pipeline);
	
	// allocate #args write descriptor sets
	encoder->write_descs.resize(entry.info->args.size());

	success = true;
	return encoder;
}

void vulkan_kernel::execute_internal(vulkan_encoder* encoder,
									 compute_queue* queue,
									 const vulkan_kernel_entry& entry,
									 const uint32_t& work_dim floor_unused,
									 const uint3& grid_dim,
									 const uint3& block_dim floor_unused /* unused for now, until dyn local size is possible */) const {
	// TODO: vkCmdPushConstants
	
	// set/write/update descriptors
	const auto write_desc_count = (uint32_t)encoder->write_descs.size();
	vkUpdateDescriptorSets(((vulkan_device*)((vulkan_queue*)queue)->get_device().get())->device,
						   write_desc_count, (write_desc_count > 0 ? encoder->write_descs.data() : nullptr),
						   0, nullptr);
	
	// final desc set binding after all parameters have been updated/set
	vkCmdBindDescriptorSets(encoder->cmd_buffer.cmd_buffer,
							VK_PIPELINE_BIND_POINT_COMPUTE,
							entry.pipeline_layout,
							0,
							1,
							&entry.desc_set,
							0,
							nullptr);
	
	// set dims + pipeline
	// TODO: check if grid_dim matches compute shader defintion
	vkCmdDispatch(encoder->cmd_buffer.cmd_buffer, grid_dim.x, grid_dim.y, grid_dim.z);
	
	// all done here, end + submit
	VK_CALL_RET(vkEndCommandBuffer(encoder->cmd_buffer.cmd_buffer), "failed to end command buffer");
	((vulkan_queue*)queue)->submit_command_buffer(encoder->cmd_buffer);
}

void vulkan_kernel::set_kernel_argument(vulkan_encoder* encoder floor_unused, const vulkan_kernel_entry& entry floor_unused,
										const uint32_t num floor_unused, const void* ptr floor_unused, const size_t& size floor_unused) const {
	// TODO: implement this (need to allocate a tmp buffer for this unless it's a push constant ...)
}

void vulkan_kernel::set_kernel_argument(vulkan_encoder* encoder,
										const vulkan_kernel_entry& entry,
										const uint32_t num, shared_ptr<compute_buffer> arg) const {
	auto& write_desc = encoder->write_descs[num];
	write_desc.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	write_desc.pNext = nullptr;
	write_desc.dstSet = entry.desc_set;
	write_desc.dstBinding = num; // TODO: need to differentiate images, buffers, others?
	write_desc.dstArrayElement = 0;
	write_desc.descriptorCount = 1;
	write_desc.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC;
	write_desc.pImageInfo = nullptr;
	write_desc.pBufferInfo = ((vulkan_buffer*)arg.get())->get_vulkan_buffer_info();
	write_desc.pTexelBufferView = nullptr;
}

void vulkan_kernel::set_kernel_argument(vulkan_encoder* encoder floor_unused,
										const vulkan_kernel_entry& entry floor_unused,
										const uint32_t num floor_unused, shared_ptr<compute_image> arg floor_unused) const {
	// TODO: implement this
}

#endif
