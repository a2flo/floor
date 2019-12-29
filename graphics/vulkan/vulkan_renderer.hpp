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

#ifndef __FLOOR_GRAPHICS_VULKAN_VULKAN_RENDERER_HPP__
#define __FLOOR_GRAPHICS_VULKAN_VULKAN_RENDERER_HPP__

#include <floor/compute/vulkan/vulkan_common.hpp>

#if !defined(FLOOR_NO_VULKAN)
#include <floor/graphics/graphics_renderer.hpp>
#include <floor/compute/vulkan/vulkan_compute.hpp>
#include <floor/compute/vulkan/vulkan_queue.hpp>
#include <floor/compute/vulkan/vulkan_image.hpp>
#include <floor/graphics/vulkan/vulkan_pipeline.hpp>

class vulkan_renderer final : public graphics_renderer {
public:
	vulkan_renderer(const compute_queue& cqueue_,
					const graphics_pass& pass_,
					const graphics_pipeline& pipeline_,
					const bool multi_view_ = false);
	~vulkan_renderer() override;
	
	bool begin() override;
	bool end() override;
	bool commit() override;
	
	struct vulkan_drawable_t final : public drawable_t {
		vulkan_drawable_t() : drawable_t() {}
		~vulkan_drawable_t() override;
		
		vulkan_compute::drawable_image_info vk_drawable;
		unique_ptr<vulkan_image> vk_image;
		
		using drawable_t::valid;
	};
	
	drawable_t* get_next_drawable(const bool get_multi_view_drawable = false) override;
	void present() override;
	
	bool set_attachments(vector<attachment_t>& attachments) override;
	bool set_attachment(const uint32_t& index, attachment_t& attachment) override;
	
	bool switch_pipeline(const graphics_pipeline& pipeline_) override;

protected:
	vulkan_command_buffer cmd_buffer;
	unique_ptr<vulkan_drawable_t> cur_drawable;
	VkFramebuffer cur_framebuffer { nullptr };
	vector<VkFramebuffer> framebuffers;
	bool is_presenting { false };
	optional<vulkan_command_buffer> att_cmd_buffer;

	// cmd buffer begin must be delayed until we actually start drawing,
	// otherwise we'll run into trouble with the drawable cmd buffer and dependencies
	bool did_begin_cmd_buffer { false };
	bool create_cmd_buffer();
	
	void draw_internal(const vector<multi_draw_entry>* draw_entries,
					   const vector<multi_draw_indexed_entry>* draw_indexed_entries,
					   const vector<compute_kernel_arg>& args) const override;
	
	bool update_vulkan_pipeline();
	
	const vulkan_pipeline::vulkan_pipeline_state_t* vk_pipeline_state { nullptr };
	VkFramebuffer create_vulkan_framebuffer(const VkRenderPass& vk_render_pass);
	
	bool set_depth_attachment(attachment_t& attachment) override;
	
};

#endif

#endif
