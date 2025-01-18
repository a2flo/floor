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

#include <floor/graphics/vulkan/vulkan_renderer.hpp>

#if !defined(FLOOR_NO_VULKAN)
#include <floor/core/essentials.hpp>
#include <floor/compute/vulkan/vulkan_common.hpp>
#include <floor/compute/vulkan/vulkan_compute.hpp>
#include <floor/compute/vulkan/vulkan_buffer.hpp>
#include <floor/compute/vulkan/vulkan_kernel.hpp>
#include <floor/compute/vulkan/vulkan_indirect_command.hpp>
#include <floor/compute/vulkan/vulkan_fence.hpp>
#include <floor/graphics/vulkan/vulkan_pass.hpp>
#include <floor/graphics/vulkan/vulkan_pipeline.hpp>
#include <floor/graphics/vulkan/vulkan_shader.hpp>
#include <floor/core/logger.hpp>

vulkan_renderer::vulkan_renderer(const compute_queue& cqueue_,
								 const graphics_pass& pass_,
								 const graphics_pipeline& pipeline_,
								 const bool multi_view_) :
graphics_renderer(cqueue_, pass_, pipeline_, multi_view_) {
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

VkFramebuffer vulkan_renderer::create_vulkan_framebuffer(const VkRenderPass& vk_render_pass, const string& pass_debug_label [[maybe_unused]],
														 const optional<decltype(render_pipeline_description::viewport)>& dyn_viewport) {
	const auto& vk_dev = (const vulkan_device&)cqueue.get_device();
	
	vector<VkImageView> vk_attachments;
	for (const auto& att : attachments_map) {
		vk_attachments.emplace_back(att.second.image->get_underlying_vulkan_image_safe()->get_vulkan_image_view());
		if (att.second.resolve_image) {
			vk_attachments.emplace_back(att.second.resolve_image->get_underlying_vulkan_image_safe()->get_vulkan_image_view());
		}
	}
	if (depth_attachment) {
		vk_attachments.emplace_back(depth_attachment->image->get_underlying_vulkan_image_safe()->get_vulkan_image_view());
	}
	
	VkFramebufferCreateInfo framebuffer_create_info {
		.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
		.pNext = nullptr,
		.flags = 0,
		.renderPass = vk_render_pass,
		.attachmentCount = uint32_t(vk_attachments.size()),
		.pAttachments = vk_attachments.data(),
		.width = (dyn_viewport ? dyn_viewport->x : cur_pipeline->get_description(multi_view).viewport.x),
		.height = (dyn_viewport ? dyn_viewport->y : cur_pipeline->get_description(multi_view).viewport.y),
		.layers = 1,
	};
	VkFramebuffer framebuffer { nullptr };
	VK_CALL_RET(vkCreateFramebuffer(vk_dev.device, &framebuffer_create_info, nullptr, &framebuffer),
				"failed to create framebuffer", nullptr)
#if defined(FLOOR_DEBUG)
	string debug_label = "framebuffer";
	if (!pass_debug_label.empty()) {
		debug_label += ":" + pass_debug_label;
	}
	((const vulkan_compute*)vk_dev.context)->set_vulkan_debug_label(vk_dev, VK_OBJECT_TYPE_FRAMEBUFFER, uint64_t(framebuffer), debug_label);
#endif
	
	if (framebuffer != nullptr) {
		// need to store these and destroy them again once we're finished
		framebuffers.emplace_back(framebuffer);
	}
	
	return framebuffer;
}

bool vulkan_renderer::create_cmd_buffer() {
	const auto& vk_queue = (const vulkan_queue&)cqueue;
#if defined(FLOOR_DEBUG)
	const char* cmd_buffer_label = (!cur_pipeline->get_description(multi_view).debug_label.empty() ?
									cur_pipeline->get_description(multi_view).debug_label.c_str() :
									"vk_renderer_cmd_buffer");
#else
	static constexpr const char* cmd_buffer_label = "vk_renderer_cmd_buffer";
#endif
	render_cmd_buffer = vk_queue.make_command_buffer(cmd_buffer_label);
	const VkCommandBufferBeginInfo begin_info{
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
		.pNext = nullptr,
		.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
		.pInheritanceInfo = nullptr,
	};
	VK_CALL_ERR_EXEC(vkBeginCommandBuffer(render_cmd_buffer.cmd_buffer, &begin_info),
					 "failed to begin command buffer", valid = false; return false;)

	// register completion callback to destroy all framebuffers once we're done
	vk_queue.add_completion_handler(render_cmd_buffer, [this]() {
		for (const auto& fb : framebuffers) {
			vkDestroyFramebuffer(((const vulkan_device&)cqueue.get_device()).device, fb, nullptr);
		}
		framebuffers.clear();
	});

	// can now use this cmd buffer + reuse it for every begin/end until commit is called
	did_begin_cmd_buffer = true;
	return true;
}

bool vulkan_renderer::begin(const dynamic_render_state_t dynamic_render_state) {
#if defined(FLOOR_DEBUG)
	if (is_indirect) {
		if (dynamic_render_state.viewport || dynamic_render_state.scissor) {
			log_warn("dynamic viewport/scissor is not supported in indirect render pipelines");
		}
	}
#endif
	
	const auto& vk_pass = (const vulkan_pass&)pass;
	
	// create framebuffer(s) for this pass
	const auto vk_render_pass = vk_pass.get_vulkan_render_pass(cqueue.get_device(), multi_view);
	cur_framebuffer = create_vulkan_framebuffer(vk_render_pass, vk_pass.get_description(multi_view).debug_label, dynamic_render_state.viewport);
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
	
	// transition attachments
	if (!att_transition_barriers.empty()) {
		const VkDependencyInfo dep_info {
			.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
			.pNext = nullptr,
			.dependencyFlags = 0,
			.memoryBarrierCount = 0,
			.pMemoryBarriers = nullptr,
			.bufferMemoryBarrierCount = 0,
			.pBufferMemoryBarriers = nullptr,
			.imageMemoryBarrierCount = uint32_t(att_transition_barriers.size()),
			.pImageMemoryBarriers = &att_transition_barriers[0],
		};
		vkCmdPipelineBarrier2(render_cmd_buffer.cmd_buffer, &dep_info);
	}
	
	// actually begin the render pass
	const auto& pipeline_desc = cur_pipeline->get_description(multi_view);
	
	if (!dynamic_render_state.viewport) {
		cur_viewport = {
			.x = 0.0f,
			.y = 0.0f,
			.width = (float)pipeline_desc.viewport.x,
			.height = (float)pipeline_desc.viewport.y,
			.minDepth = pipeline_desc.depth.range.x,
			.maxDepth = pipeline_desc.depth.range.y,
		};
	} else {
		cur_viewport = {
			.x = 0.0f,
			.y = 0.0f,
			.width = (float)dynamic_render_state.viewport->x,
			.height = (float)dynamic_render_state.viewport->y,
			.minDepth = pipeline_desc.depth.range.x,
			.maxDepth = pipeline_desc.depth.range.y,
		};
	}
	vkCmdSetViewport(render_cmd_buffer.cmd_buffer, 0, 1, &cur_viewport);
	
	if (!dynamic_render_state.scissor) {
		cur_render_area = {
			// NOTE: Vulkan uses signed integers for the offset, but doesn't actually allows it to be < 0
			.offset = { int(pipeline_desc.scissor.offset.x), int(pipeline_desc.scissor.offset.y) },
			.extent = { pipeline_desc.scissor.extent.x, pipeline_desc.scissor.extent.y },
		};
	} else {
		cur_render_area = {
			.offset = { int(dynamic_render_state.scissor->offset.x), int(dynamic_render_state.scissor->offset.y) },
			.extent = { dynamic_render_state.scissor->extent.x, dynamic_render_state.scissor->extent.y },
		};
	}
	if (uint32_t(cur_render_area.offset.x) >= (uint32_t)cur_viewport.width ||
		uint32_t(cur_render_area.offset.y) >= (uint32_t)cur_viewport.height) {
		log_error("scissor offset is out-of-bounds: $ >= $",
				  int2 { cur_render_area.offset.x, cur_render_area.offset.y },
				  float2 { cur_viewport.width, cur_viewport.height });
		return false;
	}
	if (dynamic_render_state.viewport) {
		// clamp scissor rect and render area if we have a dynamic viewport
		const auto clamped_width = min(uint32_t(cur_render_area.offset.x) + cur_render_area.extent.width, (uint32_t)cur_viewport.width);
		const auto clamped_height = min(uint32_t(cur_render_area.offset.y) + cur_render_area.extent.height, (uint32_t)cur_viewport.height);
		cur_render_area.extent.width = clamped_width - uint32_t(cur_render_area.offset.x);
		cur_render_area.extent.height = clamped_height - uint32_t(cur_render_area.offset.y);
	}
	vkCmdSetScissor(render_cmd_buffer.cmd_buffer, 0, 1, &cur_render_area);
	
	const auto& pass_clear_values = vk_pass.get_vulkan_clear_values(multi_view);
	const auto needs_clear = vk_pass.needs_clear();
	vector<VkClearValue> clear_values;
	if (needs_clear) {
		if (dynamic_render_state.clear_values) {
			if (dynamic_render_state.clear_values->size() != pass_clear_values.size()) {
				log_error("invalid clear values size: $", dynamic_render_state.clear_values->size());
				return false;
			}
			
			clear_values.reserve(dynamic_render_state.clear_values->size());
			const bool has_depth = depth_attachment.has_value();
			const auto depth_cv_idx = (dynamic_render_state.clear_values->size() - 1u);
			for (uint32_t attachment_idx = 0, attachment_count = uint32_t(dynamic_render_state.clear_values->size());
				 attachment_idx < attachment_count; ++attachment_idx) {
				const auto& clear_value = (*dynamic_render_state.clear_values)[attachment_idx];
				if (!has_depth || attachment_idx != depth_cv_idx) {
					clear_values.emplace_back(VkClearValue {
						.color = { .float32 = { clear_value.color.x, clear_value.color.y, clear_value.color.z, clear_value.color.w }, }
					});
				} else {
					clear_values.emplace_back(VkClearValue {
						.depthStencil = { .depth = clear_value.depth, .stencil = 0 }
					});
				}
			}
		} else {
			clear_values = pass_clear_values;
		}
	}
	const VkRenderPassBeginInfo pass_begin_info {
		.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
		.pNext = nullptr,
		.renderPass = vk_render_pass,
		.framebuffer = cur_framebuffer,
		.renderArea = cur_render_area,
		.clearValueCount = (needs_clear ? (uint32_t)clear_values.size() : 0),
		.pClearValues = (needs_clear ? clear_values.data() : nullptr),
	};
	// NOTE: if indirect rendering is enabled, we need to perform all rendering within secondary cmd buffers (which may also be specified
	//       by the user when calling execute_indirect()), otherwise always perform rendering in primary buffers (-> inline)
	//       also: for any direct rendering when "indirect" is enabled, we will create and execute a sec cmd buffer on-the-fly
	const VkSubpassBeginInfo subpass_begin_info {
		.sType = VK_STRUCTURE_TYPE_SUBPASS_BEGIN_INFO,
		.pNext = nullptr,
		.contents = (!is_indirect ? VK_SUBPASS_CONTENTS_INLINE : VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS),
	};
	vkCmdBeginRenderPass2(render_cmd_buffer.cmd_buffer, &pass_begin_info, &subpass_begin_info);
	
	return true;
}

bool vulkan_renderer::end() {
	const VkSubpassEndInfo subpass_end_info {
		.sType = VK_STRUCTURE_TYPE_SUBPASS_END_INFO,
		.pNext = nullptr,
	};
	vkCmdEndRenderPass2(render_cmd_buffer.cmd_buffer, &subpass_end_info);
	return true;
}

bool vulkan_renderer::commit_and_finish() {
	return commit_internal(true, true, {});
}

struct vulkan_renderer_internal {
	template <template <typename...> class smart_ptr_type, typename renderer_type>
	static bool commit_and_release_internal(smart_ptr_type<renderer_type>&& renderer, graphics_renderer::completion_handler_f&& compl_handler) {
		auto renderer_ptr = (vulkan_renderer*)renderer.get();
		// NOTE: since a std::function must be copyable, we can't use a unique_ptr here (TODO: move_only_function with C++23)
		auto queue_submission_compl_handler = [retained_renderer = shared_ptr<renderer_type>(std::move(renderer))](const vulkan_command_buffer&) {
			// call all user completion handlers
			auto vk_renderer = (vulkan_renderer*)retained_renderer.get();
			auto exec_compl_handlers = std::move(vk_renderer->completion_handlers);
			for (const auto& exec_compl_handler : exec_compl_handlers) {
				exec_compl_handler();
			}
		};
		return renderer_ptr->commit_internal(false, true, std::move(compl_handler), std::move(queue_submission_compl_handler));
	}
};

bool vulkan_renderer::commit_and_release(unique_ptr<graphics_renderer>&& renderer, completion_handler_f&& compl_handler) {
	return vulkan_renderer_internal::commit_and_release_internal(std::move(renderer), std::move(compl_handler));
}

bool vulkan_renderer::commit_and_release(shared_ptr<graphics_renderer>&& renderer, completion_handler_f&& compl_handler) {
	return vulkan_renderer_internal::commit_and_release_internal(std::move(renderer), std::move(compl_handler));
}

bool vulkan_renderer::commit_and_continue() {
	return commit_internal(true, false, {});
}

bool vulkan_renderer::commit_internal(const bool is_blocking, const bool is_finishing,
									  completion_handler_f&& user_compl_handler,
									  function<void(const vulkan_command_buffer&)>&& renderer_compl_handler) {
	assert(((!is_blocking && is_finishing) || is_blocking) && "non-blocking commit must always finish");
	assert(((!is_blocking && renderer_compl_handler) || is_blocking) && "non-blocking commit must have a renderer completion handler");
	
	const auto& vk_queue = (const vulkan_queue&)cqueue;
	
	// add the completion handler for later (must do this before submission)
	if (user_compl_handler) {
		(void)add_completion_handler(std::move(user_compl_handler));
	}
	
	// non-blocking: add present completion handler at the end
	if (!is_blocking && is_presenting) {
		(void)add_completion_handler([this]() {
			((vulkan_compute*)cqueue.get_device().context)->queue_present(cqueue, cur_drawable->vk_drawable);
		});
	}
	
	// if any image layout transitions are necessary, perform them now
	if (!img_transition_barriers.empty()) {
		VK_CMD_BLOCK_RET(vk_queue, "image layout transition", ({
			const VkDependencyInfo dep_info {
				.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
				.pNext = nullptr,
				.dependencyFlags = 0,
				.memoryBarrierCount = 0,
				.pMemoryBarriers = nullptr,
				.bufferMemoryBarrierCount = 0,
				.pBufferMemoryBarriers = nullptr,
				.imageMemoryBarrierCount = uint32_t(img_transition_barriers.size()),
				.pImageMemoryBarriers = &img_transition_barriers[0],
			};
			vkCmdPipelineBarrier2(block_cmd_buffer.cmd_buffer, &dep_info);
		}), false, is_blocking);
	}
	
	if (is_presenting) {
		// transition drawable image back to present mode (after render pass is complete)
		cur_drawable->vk_image->transition(&cqueue, render_cmd_buffer.cmd_buffer, 0 /* as per doc */, cur_drawable->vk_drawable.present_layout,
										   VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
										   VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT);
	}
	
	VK_CALL_RET(vkEndCommandBuffer(render_cmd_buffer.cmd_buffer), "failed to end command buffer", false)
	vk_queue.submit_command_buffer(std::move(render_cmd_buffer), std::move(wait_fences), std::move(signal_fences),
								   std::move(renderer_compl_handler), is_blocking);
	
	// NOTE: all of this can only be called when doing a blocking commit(), for non-blocking commits, ownership
	if (is_blocking) {
		// if present has been called earlier, we can now actually present the image to the screen
		if (is_presenting) {
			((vulkan_compute*)cqueue.get_device().context)->queue_present(cqueue, cur_drawable->vk_drawable);
			is_presenting = false;
		}
		
		if (!is_finishing) {
			// reset
			did_begin_cmd_buffer = false;
		}
	
		// call all user completion handlers
		// NOTE: we need to store/move all completion handlers into a local variable,
		//       because a completion handler may actually hold onto the vulkan_renderer object
		auto exec_compl_handlers = std::move(completion_handlers);
		for (const auto& exec_compl_handler : exec_compl_handlers) {
			exec_compl_handler();
		}
	}
	
	return true;
}

bool vulkan_renderer::add_completion_handler(completion_handler_f&& compl_handler) {
	if (!compl_handler) {
		return false;
	}
	if (!did_begin_cmd_buffer) {
		log_error("no work has been started or enqueued yet");
		return false;
	}
	completion_handlers.emplace_back(std::move(compl_handler));
	return true;
}

vulkan_renderer::vulkan_drawable_t::~vulkan_drawable_t() {
	// nop
	// TODO: free any image?
}

graphics_renderer::drawable_t* vulkan_renderer::get_next_drawable(const bool get_multi_view_drawable) {
	auto drawable_ret = ((vulkan_compute*)cqueue.get_device().context)->acquire_next_image(cqueue, get_multi_view_drawable);
	if (!drawable_ret.first) {
		return nullptr;
	}
	auto& vk_drawable = drawable_ret.second;
	
	cur_drawable = make_unique<vulkan_drawable_t>();
	cur_drawable->vk_drawable = vk_drawable;
	cur_drawable->valid = true;
	
	// wrapping the Vulkan image is non-trivial
	vulkan_image::external_vulkan_image_info info {
		.image = vk_drawable.image,
		.image_view = vk_drawable.image_view,
		.format = vk_drawable.format,
		.access_mask = vk_drawable.access_mask,
		.layout = vk_drawable.layout,
		.image_base_type = vk_drawable.base_type,
		.dim = { vk_drawable.image_size, vk_drawable.layer_count, 0 },
	};
	cur_drawable->vk_image = make_unique<vulkan_image>(cqueue, info);
#if defined(FLOOR_DEBUG)
	cur_drawable->vk_image->set_debug_label("swapchain_image#" + to_string(vk_drawable.index));
#endif
	cur_drawable->image = cur_drawable->vk_image.get();
	
	return cur_drawable.get();
}

void vulkan_renderer::present() {
	if (!cur_drawable || !cur_drawable->is_valid()) {
		log_error("current drawable is invalid");
		return;
	}

	// actual queue present must happen after the command buffer has been submitted and finished
	is_presenting = true;
}

bool vulkan_renderer::set_attachments(vector<attachment_t>& attachments) {
	// besides settings all attachments, this will gather all attachments that need to be transitioned and then transition them together later on
	att_transition_barriers.clear();
	if (!graphics_renderer::set_attachments(attachments)) {
		return false;
	}
	return true;
}

static inline bool attachment_transition(compute_image& img, vector<VkImageMemoryBarrier2>& att_transition_barriers,
										 const bool is_read_only = false) {
	auto vk_img = img.get_underlying_vulkan_image_safe();
	if (!is_read_only) {
		// make attachment writable
		auto [needs_transition, barrier] = vk_img->transition_write(nullptr, nullptr, false, false, false, true /* soft_transition */);
		if (needs_transition) {
			att_transition_barriers.emplace_back(std::move(barrier));
		}
	} else {
		// make attachment readable
		auto [needs_transition, barrier] = vk_img->transition_read(nullptr, nullptr, false, true /* soft_transition */);
		if (needs_transition) {
			att_transition_barriers.emplace_back(std::move(barrier));
		}
	}
	return true;
}

bool vulkan_renderer::set_attachment(const uint32_t& index, attachment_t& attachment) {
	if (!graphics_renderer::set_attachment(index, attachment)) {
		return false;
	}
	const auto is_read_only_color = cur_pipeline->get_description(multi_view).color_attachments[index].blend.write_mask.none();
	auto ret = attachment_transition(*attachment.image, att_transition_barriers, is_read_only_color);
	if (ret && attachment.resolve_image) {
		ret |= attachment_transition(*attachment.resolve_image, att_transition_barriers, false);
	}
	return ret;
}

bool vulkan_renderer::set_depth_attachment(attachment_t& attachment) {
	if (!graphics_renderer::set_depth_attachment(attachment)) {
		return false;
	}
	const auto is_read_only_depth = !cur_pipeline->get_description(multi_view).depth.write;
	return attachment_transition(*attachment.image, att_transition_barriers, is_read_only_depth);
}

void vulkan_renderer::execute_indirect(const indirect_command_pipeline& indirect_cmd,
									   const uint32_t command_offset,
									   const uint32_t command_count) {
	if (command_count == 0) {
		return;
	}
	
#if defined(FLOOR_DEBUG)
	if (indirect_cmd.get_description().command_type != indirect_command_description::COMMAND_TYPE::RENDER) {
		log_error("specified indirect command pipeline \"$\" must be a render pipeline",
				  indirect_cmd.get_description().debug_label);
		return;
	}
#endif
	
	const auto& vk_indirect_cmd = (const vulkan_indirect_command_pipeline&)indirect_cmd;
	const auto vk_indirect_pipeline_entry = vk_indirect_cmd.get_vulkan_pipeline_entry(cqueue.get_device());
	if (!vk_indirect_pipeline_entry) {
		log_error("no indirect command pipeline state for device \"$\" in indirect command pipeline \"$\"",
				  cqueue.get_device().name, indirect_cmd.get_description().debug_label);
		return;
	}
	
	const auto range = vk_indirect_cmd.compute_and_validate_command_range(command_offset, command_count);
	if (!range) {
		return;
	}
	
	if (vk_indirect_pipeline_entry->printf_buffer) {
		vk_indirect_pipeline_entry->printf_init(cqueue);
	}
	
	// NOTE: for render pipelines, this is always per_queue_data[0]
	vkCmdExecuteCommands(render_cmd_buffer.cmd_buffer, range->count,
						 &vk_indirect_pipeline_entry->per_queue_data[0].cmd_buffers[range->offset]);
	
	if (vk_indirect_pipeline_entry->printf_buffer) {
		vk_indirect_pipeline_entry->printf_completion(cqueue, render_cmd_buffer);
	}
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
	vk_pipeline_state = vk_pipeline.get_vulkan_pipeline_state(dev, multi_view, false /* never indirect */);
	if (vk_pipeline_state == nullptr) {
		log_error("no pipeline entry for device $", dev.name);
		return false;
	}
	return true;
}

void vulkan_renderer::draw_internal(const vector<multi_draw_entry>* draw_entries,
									const vector<multi_draw_indexed_entry>* draw_indexed_entries,
									const vector<compute_kernel_arg>& args) {
	const auto& vk_queue = (const vulkan_queue&)cqueue;
	vulkan_command_buffer* cmd_buffer = &render_cmd_buffer;
	vulkan_command_buffer sec_cmd_buffer {};
	if (is_indirect) {
		// -> direct draw within an indirect renderer
		// we need to create and execute a secondary cmd buffer for any direct rendering
		sec_cmd_buffer = vk_queue.make_secondary_command_buffer("vk_renderer_sec_cmd_buffer");
		cmd_buffer = &sec_cmd_buffer;
		const VkCommandBufferInheritanceInfo inheritance_info {
			.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO,
			.pNext = nullptr,
			.renderPass = ((const vulkan_pass&)pass).get_vulkan_render_pass(cqueue.get_device(), multi_view),
			.subpass = 0,
			.framebuffer = nullptr,
			.occlusionQueryEnable = false,
			.queryFlags = 0,
			.pipelineStatistics = 0,
		};
		const VkCommandBufferBeginInfo begin_info{
			.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
			.pNext = nullptr,
			.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT | VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT,
			.pInheritanceInfo = &inheritance_info,
		};
		VK_CALL_RET(vkBeginCommandBuffer(sec_cmd_buffer.cmd_buffer, &begin_info),
					"failed to begin command buffer for direct rendering within an indirect renderer")
		
		// need to set viewport + scissor again for this cmd buffer
		vkCmdSetViewport(sec_cmd_buffer.cmd_buffer, 0, 1, &cur_viewport);
		vkCmdSetScissor(sec_cmd_buffer.cmd_buffer, 0, 1, &cur_render_area);
	}
	
	const auto vs = (const vulkan_shader*)cur_pipeline->get_description(multi_view).vertex_shader;
	img_transition_barriers = vs->draw(cqueue,
									   *cmd_buffer,
									   vk_pipeline_state->pipeline,
									   vk_pipeline_state->layout,
									   (const vulkan_kernel::vulkan_kernel_entry*)vk_pipeline_state->vs_entry,
									   (const vulkan_kernel::vulkan_kernel_entry*)vk_pipeline_state->fs_entry,
									   draw_entries,
									   draw_indexed_entries,
									   args);
	
	
	if (is_indirect) {
		// end + execute this secondary cmd buffer
		VK_CALL_RET(vkEndCommandBuffer(sec_cmd_buffer.cmd_buffer), "failed to end secondary command buffer")
		vk_queue.execute_secondary_command_buffer(render_cmd_buffer, sec_cmd_buffer);
	}
}

void vulkan_renderer::draw_patches_internal(const patch_draw_entry* draw_entry floor_unused,
											const patch_draw_indexed_entry* draw_indexed_entry floor_unused,
											const vector<compute_kernel_arg>& args floor_unused) {
	// TODO: implement this!
	log_error("patch drawing not implemented yet!");
}

void vulkan_renderer::wait_for_fence(const compute_fence& fence, const compute_fence::SYNC_STAGE before_stage) {
	const auto& vk_fence = (const vulkan_fence&)fence;
	wait_fences.emplace_back(vulkan_queue::wait_fence_t {
		.fence = &fence,
		.signaled_value = vk_fence.get_signaled_value(),
		.stage = before_stage,
	});
}

void vulkan_renderer::signal_fence(compute_fence& fence, const compute_fence::SYNC_STAGE after_stage) {
	auto& vk_fence = (vulkan_fence&)fence;
	if (!vk_fence.next_signal_value()) {
		throw runtime_error("failed to set next signal value on fence");
	}
	signal_fences.emplace_back(vulkan_queue::signal_fence_t {
		.fence = &fence,
		.unsignaled_value = vk_fence.get_unsignaled_value(),
		.signaled_value = vk_fence.get_signaled_value(),
		.stage = after_stage,
	});
}

#endif
