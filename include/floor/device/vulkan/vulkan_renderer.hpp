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

#include <floor/device/vulkan/vulkan_common.hpp>

#if !defined(FLOOR_NO_VULKAN)
#include <floor/device/graphics_renderer.hpp>
#include <floor/device/vulkan/vulkan_context.hpp>
#include <floor/device/vulkan/vulkan_queue.hpp>
#include <floor/device/vulkan/vulkan_image.hpp>
#include <floor/device/vulkan/vulkan_pipeline.hpp>

namespace fl {

struct vulkan_renderer_internal;
class vulkan_renderer final : public graphics_renderer {
public:
	vulkan_renderer(const device_queue& cqueue_,
					const graphics_pass& pass_,
					const graphics_pipeline& pipeline_,
					const bool multi_view_ = false);
	~vulkan_renderer() override;
	
	bool begin(const dynamic_render_state_t dynamic_render_state = {}) override;
	bool end() override;
	bool commit_and_finish() override;
	bool commit_and_release(std::unique_ptr<graphics_renderer>&& renderer, completion_handler_f&& compl_handler) override;
	bool commit_and_release(std::shared_ptr<graphics_renderer>&& renderer, completion_handler_f&& compl_handler) override;
	bool commit_and_continue() override;
	bool add_completion_handler(completion_handler_f&& compl_handler) override;
	
	drawable_t* get_next_drawable(const bool get_multi_view_drawable = false) override;
	void present() override;
	
	bool set_attachments(std::vector<attachment_t>& attachments) override;
	bool set_attachment(const uint32_t& index, attachment_t& attachment) override;
	
	void execute_indirect(const indirect_command_pipeline& indirect_cmd,
						  const uint32_t command_offset = 0u,
						  const uint32_t command_count = ~0u) override;
	
	bool switch_pipeline(const graphics_pipeline& pipeline_) override;
	
	void wait_for_fence(const device_fence& fence, const SYNC_STAGE before_stage) override;
	void signal_fence(device_fence& fence, const SYNC_STAGE after_stage) override;
	
protected:
	friend vulkan_renderer_internal;
	std::unique_ptr<vulkan_renderer_internal> internal;
	
	vulkan_command_buffer render_cmd_buffer;
	VkFramebuffer cur_framebuffer { nullptr };
	std::vector<VkFramebuffer> framebuffers;
	bool is_presenting { false };
	std::vector<VkImageMemoryBarrier2> att_transition_barriers;
	std::vector<VkImageMemoryBarrier2> img_transition_barriers;
	
	// fence handling
	std::vector<vulkan_queue::wait_fence_t> wait_fences;
	std::vector<vulkan_queue::signal_fence_t> signal_fences;
	
	// cmd buffer begin must be delayed until we actually start drawing,
	// otherwise we'll run into trouble with the drawable cmd buffer and dependencies
	bool did_begin_cmd_buffer { false };
	std::vector<completion_handler_f> completion_handlers;
	bool create_cmd_buffer();
	
	void draw_internal(const std::vector<multi_draw_entry>* draw_entries,
					   const std::vector<multi_draw_indexed_entry>* draw_indexed_entries,
					   const std::vector<device_function_arg>& args) override;
	
	void draw_patches_internal(const patch_draw_entry* draw_entry,
							   const patch_draw_indexed_entry* draw_indexed_entry,
							   const std::vector<device_function_arg>& args) override;
	
	bool update_vulkan_pipeline();
	
	const vulkan_pipeline_state_t* vk_pipeline_state { nullptr };
	VkFramebuffer create_vulkan_framebuffer(const VkRenderPass& vk_render_pass, const std::string& pass_debug_label,
											const std::optional<decltype(render_pipeline_description::viewport)>& dyn_viewport);
	
	bool set_depth_attachment(attachment_t& attachment) override;
	
	bool commit_internal(const bool is_blocking, const bool is_finishing,
						 completion_handler_f&& user_compl_handler,
						 std::function<void(const vulkan_command_buffer&)>&& renderer_compl_handler = {});
	
};

} // namespace fl

#endif
