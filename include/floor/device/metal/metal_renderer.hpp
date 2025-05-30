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

#pragma once

#include <floor/device/metal/metal_common.hpp>

#if !defined(FLOOR_NO_METAL)
#include <floor/device/graphics_renderer.hpp>
#include <floor/device/metal/metal_pipeline.hpp>
#include <floor/device/metal/metal_image.hpp>
#import <QuartzCore/CAMetalLayer.h>
#include <Metal/Metal.h>

namespace fl {

struct metal_renderer_internal;
class metal_renderer final : public graphics_renderer {
public:
	metal_renderer(const device_queue& cqueue_,
				   const graphics_pass& pass_,
				   const graphics_pipeline& pipeline_,
				   const bool multi_view_ = false);
	~metal_renderer() override;
	
	bool begin(const dynamic_render_state_t dynamic_render_state = {}) override;
	bool end() override;
	bool commit_and_finish() override;
	bool commit_and_release(std::unique_ptr<graphics_renderer>&& renderer, completion_handler_f&& compl_handler) override;
	bool commit_and_release(std::shared_ptr<graphics_renderer>&& renderer, completion_handler_f&& compl_handler) override;
	bool commit_and_continue() override;
	bool add_completion_handler(completion_handler_f&& compl_handler) override;
	
	struct metal_drawable_t final : public drawable_t {
		metal_drawable_t() : drawable_t() {}
		~metal_drawable_t() override;
		
		id <CAMetalDrawable> metal_drawable { nil };
		std::shared_ptr<device_image> metal_image;
		bool is_multi_view_drawable { false };
		
		using drawable_t::valid;
	};
	
	drawable_t* get_next_drawable(const bool get_multi_view_drawable = false) override;
	void present() override;
	
	bool set_attachments(std::vector<attachment_t>& attachments) override;
	bool set_attachment(const uint32_t& index, attachment_t& attachment) override;
	
	void execute_indirect(const indirect_command_pipeline& indirect_cmd,
						  const uint32_t command_offset = 0u,
						  const uint32_t command_count = ~0u) override;
	
	bool set_tessellation_factors(const device_buffer& tess_factors_buffer) override;
	
	bool switch_pipeline(const graphics_pipeline& pipeline_) override;
	
	void wait_for_fence(const device_fence& fence, const SYNC_STAGE before_stage) override;
	void signal_fence(device_fence& fence, const SYNC_STAGE after_stage) override;
	
protected:
	friend metal_renderer_internal;
	
	id <MTLCommandBuffer> cmd_buffer;
	id <MTLRenderCommandEncoder> encoder;
	std::unique_ptr<metal_drawable_t> cur_drawable;
	
	void draw_internal(const std::vector<multi_draw_entry>* draw_entries,
					   const std::vector<multi_draw_indexed_entry>* draw_indexed_entries,
					   const std::vector<device_function_arg>& args) override;
	
	void draw_patches_internal(const patch_draw_entry* draw_entry,
							   const patch_draw_indexed_entry* draw_indexed_entry,
							   const std::vector<device_function_arg>& args) override;
	
	const metal_pipeline::metal_pipeline_entry* mtl_pipeline_state { nullptr };
	bool update_metal_pipeline();
	
	bool commit_internal();
	bool commit_internal(const bool is_blocking, const bool is_finishing,
						 completion_handler_f&& user_compl_handler,
						 completion_handler_f&& renderer_compl_handler = {});
	
};

} // namespace fl

#endif
