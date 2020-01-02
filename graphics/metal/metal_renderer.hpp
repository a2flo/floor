/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2020 Florian Ziesche
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

#ifndef __FLOOR_GRAPHICS_METAL_METAL_RENDERER_HPP__
#define __FLOOR_GRAPHICS_METAL_METAL_RENDERER_HPP__

#include <floor/compute/metal/metal_common.hpp>

#if !defined(FLOOR_NO_METAL)
#include <floor/graphics/graphics_renderer.hpp>
#include <floor/graphics/metal/metal_pipeline.hpp>
#include <floor/compute/metal/metal_image.hpp>
#import <QuartzCore/CAMetalLayer.h>
#include <Metal/Metal.h>

class metal_renderer final : public graphics_renderer {
public:
	metal_renderer(const compute_queue& cqueue_,
				   const graphics_pass& pass_,
				   const graphics_pipeline& pipeline_,
				   const bool multi_view_ = false);
	~metal_renderer() override = default;
	
	bool begin() override;
	bool end() override;
	bool commit() override;
	
	struct metal_drawable_t final : public drawable_t {
		metal_drawable_t() : drawable_t() {}
		~metal_drawable_t() override;
		
		id <CAMetalDrawable> metal_drawable;
		shared_ptr<compute_image> metal_image;
		bool is_multi_view_drawable { false };
		
		using drawable_t::valid;
	};
	
	drawable_t* get_next_drawable(const bool get_multi_view_drawable = false) override;
	void present() override;
	
	bool set_attachments(vector<attachment_t>& attachments) override;
	bool set_attachment(const uint32_t& index, attachment_t& attachment) override;
	
	bool switch_pipeline(const graphics_pipeline& pipeline_) override;
	
protected:
	id <MTLCommandBuffer> cmd_buffer;
	id <MTLRenderCommandEncoder> encoder;
	unique_ptr<metal_drawable_t> cur_drawable;
	
	void draw_internal(const vector<multi_draw_entry>* draw_entries,
					   const vector<multi_draw_indexed_entry>* draw_indexed_entries,
					   const vector<compute_kernel_arg>& args) const override;
	
	const metal_pipeline::metal_pipeline_entry* mtl_pipeline_state { nullptr };
	bool update_metal_pipeline();
	
};

#endif

#endif
