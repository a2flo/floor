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

#include <floor/device/vulkan/vulkan_pass.hpp>

#if !defined(FLOOR_NO_VULKAN)

#include <floor/device/device.hpp>
#include "internal/vulkan_headers.hpp"
#include <floor/device/vulkan/vulkan_image.hpp>
#include <floor/device/vulkan/vulkan_device.hpp>
#include <floor/device/vulkan/vulkan_context.hpp>
#include "internal/vulkan_debug.hpp"
#include "internal/vulkan_conversion.hpp"
#include "internal/vulkan_headers.hpp"

namespace fl {

struct vulkan_render_pass_info {
	std::vector<VkAttachmentDescription2> attachment_desc;
	std::vector<VkAttachmentReference2> color_attachment_refs;
	std::vector<VkAttachmentReference2> resolve_attachment_refs;
	VkAttachmentDescription2 depth_attachment_desc {};
	VkAttachmentReference2 depth_attachment_ref {};
	std::vector<VkClearValue> clear_values;

	VkSubpassDescription2 sub_pass_info {};
	VkRenderPassCreateInfo2 render_pass_info {};
	uint32_t mv_correlation_mask { 0 };
	VkRenderPassCreateInfo2 mv_render_pass_info {};
};

static std::unique_ptr<vulkan_render_pass_info> create_vulkan_render_pass_info_from_description(const render_pass_description& desc,
																						   const bool is_multi_view) {
	auto info = std::make_unique<vulkan_render_pass_info>();
	
	bool has_any_resolve = false;
	for (const auto& att : desc.attachments) {
		const auto is_msaa_resolve = (att.store_op == STORE_OP::RESOLVE || att.store_op == STORE_OP::STORE_AND_RESOLVE);
		if (is_msaa_resolve) {
			has_any_resolve = true;
			break;
		}
	}
	
	bool has_depth_attachment { false };
	VkClearValue clear_depth; // appended to the end
	uint32_t att_counter = 0;
	for (const auto& att : desc.attachments) {
		const auto load_op = vulkan_load_op_from_load_op(att.load_op);
		const auto store_op = vulkan_store_op_from_store_op(att.store_op);
		const auto is_read_only = (att.store_op == STORE_OP::DONT_CARE);
		const auto is_multi_sampling = has_flag<IMAGE_TYPE::FLAG_MSAA>(att.format);
		const auto is_transient = has_flag<IMAGE_TYPE::FLAG_TRANSIENT>(att.format);
		const auto is_msaa_resolve = (att.store_op == STORE_OP::RESOLVE || att.store_op == STORE_OP::STORE_AND_RESOLVE);
		const bool is_depth = has_flag<IMAGE_TYPE::FLAG_DEPTH>(att.format);
		
		if (!is_multi_sampling && is_msaa_resolve) {
			log_warn("graphics_pass: MSAA resolve is set, but format is not MSAA");
		}
		
		VkImageLayout layout;
		if (is_depth) {
			// depth attachment
			layout = (!is_read_only ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL : VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL);
			has_depth_attachment = true;
			info->depth_attachment_ref = {
				.sType = VK_STRUCTURE_TYPE_ATTACHMENT_REFERENCE_2,
				.pNext = nullptr,
				.attachment = 0, // -> set at the end
				.layout = layout,
				.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
			};
			clear_depth = {
				.depthStencil = {
					.depth = att.clear.depth,
					.stencil = 0,
				}
			};
		} else {
			// color attachment
			layout = (!is_read_only ? VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
			info->color_attachment_refs.emplace_back(VkAttachmentReference2 {
				.sType = VK_STRUCTURE_TYPE_ATTACHMENT_REFERENCE_2,
				.pNext = nullptr,
				.attachment = att_counter++,
				.layout = layout,
				.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
			});
			info->clear_values.emplace_back(VkClearValue {
				.color = {
					.float32 = { att.clear.color.x, att.clear.color.y, att.clear.color.z, att.clear.color.w },
				}
			});
			
			// resolve handling
			if (has_any_resolve) {
				if (is_msaa_resolve) {
					// -> resolve
					info->resolve_attachment_refs.emplace_back(VkAttachmentReference2 {
						// corresponding resolve attachment always comes after the color attachment
						.sType = VK_STRUCTURE_TYPE_ATTACHMENT_REFERENCE_2,
						.pNext = nullptr,
						.attachment = att_counter++,
						.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
						.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
					});
					// same clear color as color attachment
					info->clear_values.emplace_back(info->clear_values.back());
				} else {
					// -> not being resolved
					info->resolve_attachment_refs.emplace_back(VkAttachmentReference2 {
						.sType = VK_STRUCTURE_TYPE_ATTACHMENT_REFERENCE_2,
						.pNext = nullptr,
						.attachment = VK_ATTACHMENT_UNUSED,
						.layout = VK_IMAGE_LAYOUT_UNDEFINED,
						.aspectMask = VK_IMAGE_ASPECT_NONE,
					});
				}
			}
		}
		
		const auto vk_format = vulkan_format_from_image_type(att.format);
		if (!vk_format) {
			log_error("unsupported Vulkan format: $X", att.format);
			return {};
		}

		VkAttachmentDescription2 att_desc {
			.sType = VK_STRUCTURE_TYPE_ATTACHMENT_DESCRIPTION_2,
			.pNext = nullptr,
			.flags = 0, // no-alias
			.format = *vk_format,
			.samples = sample_count_to_vulkan_sample_count(image_sample_count(att.format)),
			// with resolve and w/o write back, we don't need to load anything here
			.loadOp = (is_msaa_resolve && att.store_op != STORE_OP::STORE_AND_RESOLVE && att.load_op != LOAD_OP::CLEAR ?
					   VK_ATTACHMENT_LOAD_OP_DONT_CARE : load_op),
			.storeOp = (is_msaa_resolve && is_transient ? VK_ATTACHMENT_STORE_OP_DONT_CARE : store_op),
			.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
			.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
			.initialLayout = (is_msaa_resolve && att.store_op != STORE_OP::STORE_AND_RESOLVE ? VK_IMAGE_LAYOUT_UNDEFINED : layout),
			.finalLayout = layout,
		};
		if (!is_depth) {
			info->attachment_desc.push_back(att_desc);
			if (is_msaa_resolve) {
				// resolving to 1 sample (overwrite old + always store)
				att_desc.samples = VK_SAMPLE_COUNT_1_BIT;
				att_desc.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
				att_desc.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
				att_desc.initialLayout = (att.load_op == LOAD_OP::LOAD ? layout : VK_IMAGE_LAYOUT_UNDEFINED);
				att_desc.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
				info->attachment_desc.push_back(att_desc);
			}
		} else {
			// -> set at the end
			info->depth_attachment_desc = att_desc;
		}
	}
	if (has_depth_attachment) {
		// depth attachment must always be at the end
		info->depth_attachment_ref.attachment = att_counter++;
		info->attachment_desc.push_back(info->depth_attachment_desc);
		info->clear_values.emplace_back(clear_depth);
	}
	
	info->sub_pass_info = {
		.sType = VK_STRUCTURE_TYPE_SUBPASS_DESCRIPTION_2,
		.pNext = nullptr,
		.flags = 0,
		.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
		.viewMask = (is_multi_view ? 0b11u /* mask: view 1 and 2 (left/right eye) */: 0u),
		.inputAttachmentCount = 0,
		.pInputAttachments = nullptr,
		.colorAttachmentCount = (uint32_t)info->color_attachment_refs.size(),
		.pColorAttachments = (!info->color_attachment_refs.empty() ? info->color_attachment_refs.data() : nullptr),
		.pResolveAttachments = (has_any_resolve ? info->resolve_attachment_refs.data() : nullptr),
		.pDepthStencilAttachment = (has_depth_attachment ? &info->depth_attachment_ref : nullptr),
		.preserveAttachmentCount = 0,
		.pPreserveAttachments = nullptr,
	};

	info->render_pass_info = {
		.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO_2,
		.pNext = nullptr,
		.flags = 0,
		.attachmentCount = (uint32_t)info->attachment_desc.size(),
		.pAttachments = info->attachment_desc.data(),
		.subpassCount = 1,
		.pSubpasses = &info->sub_pass_info,
		.dependencyCount = 0,
		.pDependencies = nullptr,
		.correlatedViewMaskCount = 0,
		.pCorrelatedViewMasks = nullptr,
	};

	if (is_multi_view) {
		info->mv_render_pass_info = info->render_pass_info;
		
		// mask: view 1 and 2 (left/right eye)
		info->mv_correlation_mask = 0b11u;
		info->mv_render_pass_info.correlatedViewMaskCount = 1;
		info->mv_render_pass_info.pCorrelatedViewMasks = &info->mv_correlation_mask;
	}

	floor_return_no_nrvo(info);
}

vulkan_pass::vulkan_pass(const render_pass_description& pass_desc_,
						 const std::vector<std::unique_ptr<device>>& devices,
						 const bool with_multi_view_support) : graphics_pass(pass_desc_, with_multi_view_support) {
	const bool create_sv_pass = is_single_view_capable();
	const bool create_mv_pass = is_multi_view_capable();
	
	for (const auto& att : pass_desc.attachments) {
		if (att.load_op == LOAD_OP::CLEAR) {
			has_any_clear_load_op = true;
			break;
		}
	}
	
	std::unique_ptr<vulkan_render_pass_info> sv_render_pass_info;
	if (create_sv_pass) {
		sv_render_pass_info = create_vulkan_render_pass_info_from_description(pass_desc, false);
		if (!sv_render_pass_info) {
			return;
		}
		sv_clear_values = std::make_shared<std::vector<VkClearValue>>(std::move(sv_render_pass_info->clear_values));
	}
	
	std::unique_ptr<vulkan_render_pass_info> mv_render_pass_info;
	if (create_mv_pass) {
		mv_render_pass_info = create_vulkan_render_pass_info_from_description(!multi_view_pass_desc ? pass_desc : *multi_view_pass_desc, true);
		if (!mv_render_pass_info) {
			return;
		}
		mv_clear_values = std::make_shared<std::vector<VkClearValue>>(std::move(mv_render_pass_info->clear_values));
	}
	
	for (const auto& dev : devices) {
		const auto& vk_dev = (const vulkan_device&)*dev;
		VkRenderPass sv_render_pass { nullptr };
		if (create_sv_pass) {
			VK_CALL_RET(vkCreateRenderPass2(vk_dev.device, &sv_render_pass_info->render_pass_info, nullptr, &sv_render_pass),
						"failed to create render pass")
#if defined(FLOOR_DEBUG)
			if (!pass_desc.debug_label.empty()) {
				set_vulkan_debug_label(vk_dev, VK_OBJECT_TYPE_RENDER_PASS, uint64_t(sv_render_pass), pass_desc.debug_label);
			}
#endif
		}
		
		VkRenderPass mv_render_pass { nullptr };
		if (create_mv_pass) {
			VK_CALL_RET(vkCreateRenderPass2(vk_dev.device, &mv_render_pass_info->render_pass_info, nullptr, &mv_render_pass),
						"failed to create multi-view render pass")
#if defined(FLOOR_DEBUG)
			if (!pass_desc.debug_label.empty()) {
				set_vulkan_debug_label(vk_dev, VK_OBJECT_TYPE_RENDER_PASS, uint64_t(mv_render_pass), pass_desc.debug_label);
			}
#endif
		}

		render_passes.insert_or_assign(dev.get(), vulkan_pass_t { sv_render_pass, mv_render_pass });
	}
	
	// success
	valid = true;
}

vulkan_pass::~vulkan_pass() {
	for (auto&& render_pass : render_passes) {
		if (render_pass.second.single_view_pass) {
			vkDestroyRenderPass(((const vulkan_device*)render_pass.first)->device, render_pass.second.single_view_pass, nullptr);
		}
		if (render_pass.second.multi_view_pass) {
			vkDestroyRenderPass(((const vulkan_device*)render_pass.first)->device, render_pass.second.multi_view_pass, nullptr);
		}
	}
}

VkRenderPass vulkan_pass::get_vulkan_render_pass(const device& dev, const bool get_multi_view) const {
	if (const auto iter = render_passes.find((const vulkan_device*)&dev); iter != render_passes.end()) {
		return (!get_multi_view ? iter->second.single_view_pass : iter->second.multi_view_pass);
	}
	return nullptr;
}

const std::vector<VkClearValue>& vulkan_pass::get_vulkan_clear_values(const bool get_multi_view) const {
	return (!get_multi_view ? *sv_clear_values : *mv_clear_values);
}

} // namespace fl

#endif
