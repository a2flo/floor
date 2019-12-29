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

struct vulkan_render_pass_info {
	vector<VkAttachmentDescription> attachment_desc;
	vector<VkAttachmentReference> color_attachment_refs;
	VkAttachmentDescription depth_attachment_desc {};
	VkAttachmentReference depth_attachment_ref {};
	vector<VkClearValue> clear_values;

	VkSubpassDescription sub_pass_info {};
	VkRenderPassCreateInfo render_pass_info {};

	uint32_t mv_view_mask { 0 };
	uint32_t mv_correlation_mask { 0 };
	VkRenderPassMultiviewCreateInfo mv_render_pass_info {};
};

static unique_ptr<vulkan_render_pass_info> create_vulkan_render_pass_info_from_description(const render_pass_description& desc,
																						   const bool is_multi_view) {
	auto info = make_unique<vulkan_render_pass_info>();

	bool has_depth_attachment { false };
	VkClearValue clear_depth; // appended to the end
	uint32_t color_att_counter = 0;
	for (const auto& att : desc.attachments) {
		const auto load_op = vulkan_pass::vulkan_load_op_from_load_op(att.load_op);
		const auto store_op = vulkan_pass::vulkan_store_op_from_store_op(att.store_op);

		VkImageLayout layout;
		const bool is_depth = has_flag<COMPUTE_IMAGE_TYPE::FLAG_DEPTH>(att.format);
		if (is_depth) {
			// depth attachment
			layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
			has_depth_attachment = true;
			info->depth_attachment_ref.attachment = 0; // -> set at the end
			info->depth_attachment_ref.layout = layout;
			clear_depth = {
				.depthStencil = {
					.depth = att.clear.depth,
					.stencil = 0,
				}
			};
		} else {
			// color attachment
			layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
			info->color_attachment_refs.emplace_back(VkAttachmentReference {
				.attachment = color_att_counter++,
				.layout = layout,
			});
			info->clear_values.emplace_back(VkClearValue {
				.color = {
					.float32 = { att.clear.color.x, att.clear.color.y, att.clear.color.z, att.clear.color.w },
				}
			});
		}

		const auto vk_format = vulkan_image::vulkan_format_from_image_type(att.format);
		if (!vk_format) {
			log_error("unsupported Vulkan format: %X", att.format);
			return {};
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
			info->attachment_desc.emplace_back(att_desc);
		} else {
			// -> set at the end
			info->depth_attachment_desc = att_desc;
		}
	}
	if (has_depth_attachment) {
		// depth attachment must always be at the end
		info->depth_attachment_ref.attachment = color_att_counter++;
		info->attachment_desc.emplace_back(info->depth_attachment_desc);
		info->clear_values.emplace_back(clear_depth);
	}

	info->sub_pass_info = {
		.flags = 0,
		.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
		.inputAttachmentCount = 0,
		.pInputAttachments = nullptr,
		.colorAttachmentCount = (uint32_t)info->color_attachment_refs.size(),
		.pColorAttachments = (!info->color_attachment_refs.empty() ? info->color_attachment_refs.data() : nullptr),
		.pResolveAttachments = nullptr,
		.pDepthStencilAttachment = (has_depth_attachment ? &info->depth_attachment_ref : nullptr),
		.preserveAttachmentCount = 0,
		.pPreserveAttachments = nullptr,
	};

	if (is_multi_view) {
		// mask: view 1 and 2 (left/right eye)
		info->mv_view_mask = 0b11u;
		info->mv_correlation_mask = 0b11u;
		info->mv_render_pass_info = {
			.sType = VK_STRUCTURE_TYPE_RENDER_PASS_MULTIVIEW_CREATE_INFO,
			.pNext = nullptr,
			.subpassCount = 1,
			.pViewMasks = &info->mv_view_mask,
			.dependencyCount = 0,
			.pViewOffsets = nullptr,
			.correlationMaskCount = 1,
			.pCorrelationMasks = &info->mv_correlation_mask,
		};
	}

	info->render_pass_info = {
		.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
		.pNext = (!is_multi_view ? nullptr : &info->mv_render_pass_info),
		.flags = 0,
		.attachmentCount = (uint32_t)info->attachment_desc.size(),
		.pAttachments = info->attachment_desc.data(),
		.subpassCount = 1,
		.pSubpasses = &info->sub_pass_info,
		.dependencyCount = 0,
		.pDependencies = nullptr,
	};

	return info;
}

vulkan_pass::vulkan_pass(const render_pass_description& pass_desc_,
						 const vector<unique_ptr<compute_device>>& devices,
						 const bool with_multi_view_support) : graphics_pass(pass_desc_, with_multi_view_support) {
	const bool create_sv_pass = is_single_view_capable();
	const bool create_mv_pass = is_multi_view_capable();

	unique_ptr<vulkan_render_pass_info> sv_render_pass_info;
	if (create_sv_pass) {
		sv_render_pass_info = create_vulkan_render_pass_info_from_description(pass_desc, false);
		if (!sv_render_pass_info) {
			return;
		}
		sv_clear_values = move(sv_render_pass_info->clear_values);
	}

	unique_ptr<vulkan_render_pass_info> mv_render_pass_info;
	if (create_mv_pass) {
		mv_render_pass_info = create_vulkan_render_pass_info_from_description(!multi_view_pass_desc ? pass_desc : *multi_view_pass_desc, true);
		if (!mv_render_pass_info) {
			return;
		}
		mv_clear_values = move(mv_render_pass_info->clear_values);
	}
	
	for (const auto& dev : devices) {
		VkRenderPass sv_render_pass { nullptr };
		if (create_sv_pass) {
			VK_CALL_RET(vkCreateRenderPass(((const vulkan_device*)dev.get())->device, &sv_render_pass_info->render_pass_info, nullptr, &sv_render_pass),
						"failed to create render pass")
		}

		VkRenderPass mv_render_pass { nullptr };
		if (create_mv_pass) {
			VK_CALL_RET(vkCreateRenderPass(((const vulkan_device*)dev.get())->device, &mv_render_pass_info->render_pass_info, nullptr, &mv_render_pass),
						"failed to create multi-view render pass")
		}

		render_passes.insert_or_assign(*dev, { sv_render_pass, mv_render_pass });
	}
	
	// success
	valid = true;
}

vulkan_pass::~vulkan_pass() {
	for (const auto& render_pass : render_passes) {
		if (render_pass.second.single_view_pass) {
			vkDestroyRenderPass(((const vulkan_device&)render_pass.first.get()).device, render_pass.second.single_view_pass, nullptr);
		}
		if (render_pass.second.multi_view_pass) {
			vkDestroyRenderPass(((const vulkan_device&)render_pass.first.get()).device, render_pass.second.multi_view_pass, nullptr);
		}
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

VkRenderPass vulkan_pass::get_vulkan_render_pass(const compute_device& dev, const bool get_multi_view) const {
	const auto ret = render_passes.get(dev);
	return !ret.first ? nullptr : (!get_multi_view ? ret.second->second.single_view_pass : ret.second->second.multi_view_pass);
}

#endif
