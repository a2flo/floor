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

#include <floor/graphics/vulkan/vulkan_renderer.hpp>

#if !defined(FLOOR_NO_VULKAN)
#include <floor/core/essentials.hpp>
#include <floor/compute/vulkan/vulkan_common.hpp>
#include <floor/compute/vulkan/vulkan_compute.hpp>
#include <floor/compute/vulkan/vulkan_buffer.hpp>
#include <floor/compute/vulkan/vulkan_kernel.hpp>
#include <floor/graphics/vulkan/vulkan_pass.hpp>
#include <floor/graphics/vulkan/vulkan_pipeline.hpp>
#include <floor/graphics/vulkan/vulkan_shader.hpp>
#include <floor/core/logger.hpp>

vulkan_renderer::vulkan_renderer(const compute_queue& cqueue_, const graphics_pass& pass_, const graphics_pipeline& pipeline_) :
graphics_renderer(cqueue_, pass_, pipeline_) {
	if (!valid) {
		// already marked invalid, no point in continuing
		return;
	}
	
	if (!update_vulkan_pipeline()) {
		valid = false;
		return;
	}
	
	// all successful
	valid = true;
}

vulkan_renderer::~vulkan_renderer() {
	// TODO: implement this
}

VkFramebuffer vulkan_renderer::create_vulkan_framebuffer(const VkRenderPass& vk_render_pass) {
	const auto& vk_dev = ((const vulkan_device&)cqueue.get_device()).device;
	
	vector<VkImageView> vk_attachments;
	for (const auto& att : attachments_map) {
		vk_attachments.emplace_back(((const vulkan_image*)att.second)->get_vulkan_image_view());
	}
	if (depth_attachment != nullptr) {
		vk_attachments.emplace_back(((const vulkan_image*)depth_attachment)->get_vulkan_image_view());
	}
	
	VkFramebufferCreateInfo framebuffer_create_info {
		.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
		.pNext = nullptr,
		.flags = 0,
		.renderPass = vk_render_pass,
		.attachmentCount = uint32_t(vk_attachments.size()),
		.pAttachments = vk_attachments.data(),
		.width = cur_pipeline->get_description().viewport.x,
		.height = cur_pipeline->get_description().viewport.y,
		.layers = 1,
	};
	VkFramebuffer framebuffer { nullptr };
	VK_CALL_RET(vkCreateFramebuffer(vk_dev, &framebuffer_create_info, nullptr, &framebuffer),
				"failed to create framebuffer", nullptr)
	
	if (framebuffer != nullptr) {
		// need to store these and destroy them again once we're finished
		framebuffers.emplace_back(framebuffer);
	}
	
	return framebuffer;
}

bool vulkan_renderer::create_cmd_buffer() {
	const auto& vk_queue = (const vulkan_queue&)cqueue;
	cmd_buffer = vk_queue.make_command_buffer("vk_renderer_cmd_buffer");
	const VkCommandBufferBeginInfo begin_info{
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
		.pNext = nullptr,
		// TODO: different usage?
		.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
		.pInheritanceInfo = nullptr,
	};
	VK_CALL_ERR_EXEC(vkBeginCommandBuffer(cmd_buffer.cmd_buffer, &begin_info),
					 "failed to begin command buffer", valid = false; return false;)

	// register completion callback to destroy all framebuffers once we're done
	vk_queue.add_completion_handler(cmd_buffer, [this]() {
		for (const auto& fb : framebuffers) {
			vkDestroyFramebuffer(((const vulkan_device&)cqueue.get_device()).device, fb, nullptr);
		}
		framebuffers.clear();
	});
	
	// register completion callback to make all attachments readable once we're done (except for the drawable, which is dealt with differently)
	vk_queue.add_completion_handler(cmd_buffer, [this]() {
		if (attachments_map.empty() && depth_attachment == nullptr) {
			return; // nop
		}
		
		// figure out what the drawable compute_image is and if we actually need to transition anything
		compute_image* cur_drawable_img = nullptr;
		if (cur_drawable) {
			cur_drawable_img = cur_drawable->image;
		}
		if (depth_attachment == nullptr) {
			bool has_non_swapchain_iamge = false;
			for (auto& att : attachments_map) {
				if (att.second != cur_drawable_img) {
					has_non_swapchain_iamge = true;
					break;
				}
			}
			if (!has_non_swapchain_iamge) {
				return; // nothing to transition
			}
		}
		
		// transition all except "cur_drawable_img"
		const auto& transition_vk_queue = (const vulkan_queue&)cqueue;
		auto transition_cmd_buffer = transition_vk_queue.make_command_buffer("vk_renderer_transition_read_attachments_cmd_buffer");
		const VkCommandBufferBeginInfo transition_begin_info{
			.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
			.pNext = nullptr,
			.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
			.pInheritanceInfo = nullptr,
		};
		VK_CALL_ERR_EXEC(vkBeginCommandBuffer(transition_cmd_buffer.cmd_buffer, &transition_begin_info),
						 "failed to begin command buffer for attachments read transition", return;)
		
		for (auto& att : attachments_map) {
			if (att.second != nullptr && att.second != cur_drawable_img) {
				((vulkan_image*)att.second)->transition_read(cqueue, transition_cmd_buffer.cmd_buffer);
			}
		}
		if (depth_attachment != nullptr) {
			((vulkan_image*)depth_attachment)->transition_read(cqueue, transition_cmd_buffer.cmd_buffer);
		}
		
		VK_CALL_RET(vkEndCommandBuffer(transition_cmd_buffer.cmd_buffer), "failed to end command buffer for attachments read transition")
		transition_vk_queue.submit_command_buffer(transition_cmd_buffer, true);
	});

	// can now use this cmd buffer + reuse it for every begin/end until commit is called
	did_begin_cmd_buffer = true;
	return true;
}

bool vulkan_renderer::begin() {
	const auto& vk_pass = (const vulkan_pass&)pass;
	
	// create framebuffer(s) for this pass
	const auto vk_render_pass = vk_pass.get_vulkan_render_pass(cqueue.get_device());
	cur_framebuffer = create_vulkan_framebuffer(vk_render_pass);
	if (cur_framebuffer == nullptr) {
		return false;
	}

	// create cmd buffer if we haven't yet
	if (!did_begin_cmd_buffer) {
		// it's now safe to begin the cmd buffer
		if (!create_cmd_buffer()) {
			return false;
		}
	}
	
	// actually begin the render pass
	const VkViewport viewport {
		.x = 0.0f,
		.y = 0.0f,
		.width = (float)cur_pipeline->get_description().viewport.x,
		.height = (float)cur_pipeline->get_description().viewport.y,
		.minDepth = 0.0f,
		.maxDepth = 1.0f,
	};
	const VkRect2D render_area {
		.offset = { 0, 0 },
		.extent = { cur_pipeline->get_description().viewport.x, cur_pipeline->get_description().viewport.y },
	};
	vkCmdSetViewport(cmd_buffer.cmd_buffer, 0, 1, &viewport);
	vkCmdSetScissor(cmd_buffer.cmd_buffer, 0, 1, &render_area);
	
	const auto& clear_values = vk_pass.get_vulkan_clear_values();
	const VkRenderPassBeginInfo pass_begin_info {
		.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
		.pNext = nullptr,
		.renderPass = vk_render_pass,
		.framebuffer = cur_framebuffer,
		.renderArea = render_area,
		.clearValueCount = (uint32_t)clear_values.size(),
		.pClearValues = clear_values.data(),
	};
	vkCmdBeginRenderPass(cmd_buffer.cmd_buffer, &pass_begin_info, VK_SUBPASS_CONTENTS_INLINE);
	
	return true;
}

bool vulkan_renderer::end() {
	vkCmdEndRenderPass(cmd_buffer.cmd_buffer);
	return true;
}

bool vulkan_renderer::commit() {
	const auto& vk_queue = (const vulkan_queue&)cqueue;
	
	// TODO: blocking or not? yes for now, until we can ensure that everything gets cleaned up properly + is synced properly
	VK_CALL_RET(vkEndCommandBuffer(cmd_buffer.cmd_buffer), "failed to end command buffer", false)
	vk_queue.submit_command_buffer(cmd_buffer, true);
	
	// if present has been called earlier, we can now actually present the image to the screen
	if (is_presenting) {
		((vulkan_compute*)cqueue.get_device().context)->queue_present(cur_drawable->vk_drawable);
		is_presenting = false;
	}
	
	return true;
}

vulkan_renderer::vulkan_drawable_t::~vulkan_drawable_t() {
	// nop
	// TODO: free any image?
}

graphics_renderer::drawable_t* vulkan_renderer::get_next_drawable() {
	auto drawable_ret = ((vulkan_compute*)cqueue.get_device().context)->acquire_next_image();
	if (!drawable_ret.first) {
		return nullptr;
	}
	auto& vk_drawable = drawable_ret.second;
	
	cur_drawable = make_unique<vulkan_drawable_t>();
	cur_drawable->vk_drawable = vk_drawable;
	cur_drawable->valid = true;
	
	// wrapping the Vulkan image is non-trival
	vulkan_image::external_vulkan_image_info info {
		.image = vk_drawable.image,
		.image_view = vk_drawable.image_view,
		.format = vk_drawable.format,
		.access_mask = vk_drawable.access_mask,
		.layout = vk_drawable.layout,
		.image_base_type = COMPUTE_IMAGE_TYPE::IMAGE_2D,
		.dim = { vk_drawable.image_size, 0, 0 },
	};
	cur_drawable->vk_image = make_unique<vulkan_image>(cqueue, info);
	cur_drawable->image = cur_drawable->vk_image.get();
	
	return cur_drawable.get();
}

void vulkan_renderer::present() {
	if (!cur_drawable || !cur_drawable->is_valid()) {
		log_error("current drawable is invalid");
		return;
	}
	
	// transition drawable image back to present mode
	const VkImageMemoryBarrier present_image_barrier {
		.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
		.pNext = nullptr,
		.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
		.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT,
		.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
		.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
		.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
		.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
		.image = cur_drawable->vk_drawable.image,
		.subresourceRange = {
			.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
			.baseMipLevel = 0,
			.levelCount = 1,
			.baseArrayLayer = 0,
			.layerCount = 1,
		},
	};
	vkCmdPipelineBarrier(cmd_buffer.cmd_buffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
						 0, 0, nullptr, 0, nullptr, 1, &present_image_barrier);
	
	// actual queue present must happen after the command buffer has been submitted and finished
	is_presenting = true;
}

bool vulkan_renderer::set_attachments(vector<attachment_t>& attachments) {
	// TODO: check if we actually need to transition any attachment in the specified container
	// try to use a single cmd buffer for all attachment transitions
	const auto& vk_queue = (const vulkan_queue&)cqueue;
	att_cmd_buffer = vk_queue.make_command_buffer("vk_renderer_transition_write_attachments_cmd_buffer");
	const VkCommandBufferBeginInfo begin_info{
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
		.pNext = nullptr,
		.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
		.pInheritanceInfo = nullptr,
	};
	VK_CALL_ERR_EXEC(vkBeginCommandBuffer(att_cmd_buffer->cmd_buffer, &begin_info),
					 "failed to begin command buffer for attachments write transition", return false;)
	
	if (!graphics_renderer::set_attachments(attachments)) {
		return false;
	}
	
	VK_CALL_RET(vkEndCommandBuffer(att_cmd_buffer->cmd_buffer), "failed to end command buffer for attachments write transition", false)
	((const vulkan_queue&)cqueue).submit_command_buffer(*att_cmd_buffer, true);
	
	att_cmd_buffer.reset();
	
	return true;
}

static inline bool attachment_transition_write(const compute_queue& cqueue, compute_image& img, optional<vulkan_command_buffer> transition_cmd_buffer) {
	// make attachments writable
	const auto& vk_queue = (const vulkan_queue&)cqueue;
	VkCommandBuffer vk_cmd_buffer = (transition_cmd_buffer ? transition_cmd_buffer->cmd_buffer : nullptr);
	vulkan_command_buffer tmp_cmd_buffer {};
	if (vk_cmd_buffer == nullptr) {
		// use a tmp command buffer if we don't have one (set_attchments creates a single one for bulk transition)
		tmp_cmd_buffer = vk_queue.make_command_buffer("vk_renderer_transition_write_attachment_cmd_buffer");
		const VkCommandBufferBeginInfo begin_info {
			.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
			.pNext = nullptr,
			.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
			.pInheritanceInfo = nullptr,
		};
		VK_CALL_ERR_EXEC(vkBeginCommandBuffer(tmp_cmd_buffer.cmd_buffer, &begin_info),
						 "failed to begin command buffer for attachment write transition", return false;)
		vk_cmd_buffer = tmp_cmd_buffer.cmd_buffer;
	}
	((vulkan_image&)img).transition_write(cqueue, vk_cmd_buffer);
	
	if (tmp_cmd_buffer) {
		// with a tmp buffer, we need to block here already
		VK_CALL_RET(vkEndCommandBuffer(tmp_cmd_buffer.cmd_buffer), "failed to end command buffer for attachment write transition", false)
		vk_queue.submit_command_buffer(tmp_cmd_buffer, true);
	}
	return true;
}

bool vulkan_renderer::set_attachment(const uint32_t& index, attachment_t& attachment) {
	if (!graphics_renderer::set_attachment(index, attachment)) {
		return false;
	}
	return attachment_transition_write(cqueue, attachment.image, att_cmd_buffer);
}

bool vulkan_renderer::set_depth_attachment(attachment_t& attachment) {
	if (!graphics_renderer::set_depth_attachment(attachment)) {
		return false;
	}
	return attachment_transition_write(cqueue, attachment.image, att_cmd_buffer);
}

bool vulkan_renderer::switch_pipeline(const graphics_pipeline& pipeline_) {
	if (!graphics_renderer::switch_pipeline(pipeline_)) {
		return false;
	}
	return update_vulkan_pipeline();
}

bool vulkan_renderer::update_vulkan_pipeline() {
	const auto& dev = cqueue.get_device();
	const auto& vk_pipeline = (const vulkan_pipeline&)*cur_pipeline;
	vk_pipeline_state = vk_pipeline.get_vulkan_pipeline_entry(dev);
	if (vk_pipeline_state == nullptr) {
		log_error("no pipeline state for device %s", dev.name);
		return false;
	}
	return true;
}

void vulkan_renderer::draw_internal(const vector<multi_draw_entry>* draw_entries,
									const vector<multi_draw_indexed_entry>* draw_indexed_entries,
									const vector<compute_kernel_arg>& args) const {
	const auto vs = (const vulkan_shader*)cur_pipeline->get_description().vertex_shader;
	vs->draw(cqueue,
			 cmd_buffer,
			 vk_pipeline_state->pipeline,
			 vk_pipeline_state->layout,
			 (const vulkan_kernel::vulkan_kernel_entry*)vk_pipeline_state->vs_entry,
			 (const vulkan_kernel::vulkan_kernel_entry*)vk_pipeline_state->fs_entry,
			 draw_entries,
			 draw_indexed_entries,
			 args);
}

#endif
