/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2019 Florian Ziesche
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

#include <floor/graphics/vulkan/vulkan_pass.hpp>

#if !defined(FLOOR_NO_VULKAN)

#include <floor/compute/vulkan/vulkan_image.hpp>
#include <floor/compute/vulkan/vulkan_device.hpp>

vulkan_pass::vulkan_pass(const render_pass_description& pass_desc_, const vector<unique_ptr<compute_device>>& devices) : graphics_pass(pass_desc_) {
	vector<VkAttachmentDescription> attachment_desc;
	vector<VkAttachmentReference> color_attachment_refs;
	VkAttachmentDescription depth_attachment_desc;
	VkAttachmentReference depth_attachment_ref;
	
	bool has_depth_attachment { false };
	VkClearValue clear_depth; // appended to the end
	uint32_t color_att_counter = 0;
	for (size_t i = 0, count = pass_desc.attachments.size(); i < count; ++i) {
		const auto& att = pass_desc.attachments[i];
		const auto load_op = vulkan_load_op_from_load_op(att.load_op);
		const auto store_op = vulkan_store_op_from_store_op(att.store_op);
		
		VkImageLayout layout;
		const bool is_depth = has_flag<COMPUTE_IMAGE_TYPE::FLAG_DEPTH>(att.format);
		if (is_depth) {
			// depth attachment
			layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
			has_depth_attachment = true;
			depth_attachment_ref.attachment = 0; // -> set at the end
			depth_attachment_ref.layout = layout;
			clear_depth = {
				.depthStencil = {
					.depth = att.clear.depth,
					.stencil = 0,
				}
			};
		} else {
			// color attachment
			layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
			color_attachment_refs.emplace_back(VkAttachmentReference {
				.attachment = color_att_counter++,
				.layout = layout,
			});
			clear_values.emplace_back(VkClearValue {
				.color = {
					.float32 = { att.clear.color.x, att.clear.color.y, att.clear.color.z, att.clear.color.w },
				}
			});
		}
		
		const auto vk_format = vulkan_image::vulkan_format_from_image_type(att.format);
		if (!vk_format) {
			log_error("unsupported Vulkan format: %X", att.format);
			return;
		}
		
		VkAttachmentDescription att_desc {
			.flags = 0, // no-alias
			.format = *vk_format,
			.samples = VK_SAMPLE_COUNT_1_BIT,
			.loadOp = load_op,
			.storeOp = store_op,
			.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
			.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
			.initialLayout = layout,
			.finalLayout = layout,
		};
		if (!is_depth) {
			attachment_desc.emplace_back(att_desc);
		} else {
			// -> set at the end
			depth_attachment_desc = att_desc;
		}
	}
	if (has_depth_attachment) {
		// depth attachment must always be at the end
		depth_attachment_ref.attachment = color_att_counter++;
		attachment_desc.emplace_back(depth_attachment_desc);
		clear_values.emplace_back(clear_depth);
	}
	
	const VkSubpassDescription sub_pass_info {
		.flags = 0,
		.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
		.inputAttachmentCount = 0,
		.pInputAttachments = nullptr,
		.colorAttachmentCount = (uint32_t)color_attachment_refs.size(),
		.pColorAttachments = (!color_attachment_refs.empty() ? color_attachment_refs.data() : nullptr),
		.pResolveAttachments = nullptr,
		.pDepthStencilAttachment = (has_depth_attachment ? &depth_attachment_ref : nullptr),
		.preserveAttachmentCount = 0,
		.pPreserveAttachments = nullptr,
	};
	
	const VkRenderPassCreateInfo render_pass_info {
		.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
		.pNext = nullptr,
		.flags = 0,
		.attachmentCount = (uint32_t)attachment_desc.size(),
		.pAttachments = attachment_desc.data(),
		.subpassCount = 1,
		.pSubpasses = &sub_pass_info,
		.dependencyCount = 0,
		.pDependencies = nullptr,
	};
	
	for (const auto& dev : devices) {
		VkRenderPass render_pass { nullptr };
		VK_CALL_RET(vkCreateRenderPass(((const vulkan_device*)dev.get())->device, &render_pass_info, nullptr, &render_pass),
					"failed to create render pass")
		render_passes.insert_or_assign(*dev, render_pass);
	}
	
	// success
	valid = true;
}

vulkan_pass::~vulkan_pass() {
	for (const auto& render_pass : render_passes) {
		vkDestroyRenderPass(((const vulkan_device&)render_pass.first.get()).device, render_pass.second, nullptr);
	}
}

VkAttachmentLoadOp vulkan_pass::vulkan_load_op_from_load_op(const LOAD_OP& load_op) {
	switch (load_op) {
		case LOAD_OP::LOAD:
			return VK_ATTACHMENT_LOAD_OP_LOAD;
		case LOAD_OP::CLEAR:
			return VK_ATTACHMENT_LOAD_OP_CLEAR;
		case LOAD_OP::DONT_CARE:
			return VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	}
}

VkAttachmentStoreOp vulkan_pass::vulkan_store_op_from_store_op(const STORE_OP& store_op) {
	switch (store_op) {
		case STORE_OP::STORE:
			return VK_ATTACHMENT_STORE_OP_STORE;
		case STORE_OP::DONT_CARE:
			return VK_ATTACHMENT_STORE_OP_DONT_CARE;
	}
}

VkRenderPass vulkan_pass::get_vulkan_render_pass(const compute_device& dev) const {
	const auto ret = render_passes.get(dev);
	return !ret.first ? nullptr : ret.second->second;
}

#endif
