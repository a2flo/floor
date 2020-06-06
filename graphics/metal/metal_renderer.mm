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

#include <floor/graphics/metal/metal_renderer.hpp>

#if !defined(FLOOR_NO_METAL)
#include <floor/core/essentials.hpp>
#include <floor/compute/metal/metal_compute.hpp>
#include <floor/compute/metal/metal_queue.hpp>
#include <floor/compute/metal/metal_buffer.hpp>
#include <floor/graphics/metal/metal_pass.hpp>
#include <floor/graphics/metal/metal_pipeline.hpp>
#include <floor/graphics/metal/metal_shader.hpp>
#include <floor/core/logger.hpp>

metal_renderer::metal_renderer(const compute_queue& cqueue_,
							   const graphics_pass& pass_,
							   const graphics_pipeline& pipeline_,
							   const bool multi_view_) :
graphics_renderer(cqueue_, pass_, pipeline_, multi_view_) {
	if (!valid) {
		// already marked invalid, no point in continuing
		return;
	}
	
	// TODO: check any metal related validity
	
	// create a command buffer from the specified queue (this will be used throughout until commit)
	const auto mtl_queue = ((const metal_queue&)cqueue).get_queue();
	cmd_buffer = [mtl_queue commandBuffer];
	cmd_buffer.label = @"metal_renderer";
	
	if (!update_metal_pipeline()) {
		valid = false;
		return;
	}
	
	// all successful
	valid = true;
}

bool metal_renderer::begin(const dynamic_render_state_t dynamic_render_state) {
	const auto& pipeline_desc = cur_pipeline->get_description(multi_view);
	const auto& mtl_pass = (const metal_pass&)pass;
	const auto mtl_pass_desc_template = mtl_pass.get_metal_pass_desc(multi_view);
	
	MTLRenderPassDescriptor* mtl_pass_desc = [mtl_pass_desc_template copy];
	
	if (dynamic_render_state.clear_values &&
		dynamic_render_state.clear_values->size() != attachments_map.size() + (depth_attachment ? 1u : 0u)) {
		log_error("invalid clear values size: %u", dynamic_render_state.clear_values->size());
		return false;
	}
	
	// must set/update attachments (and drawable) before creating the encoder
	uint32_t attachment_idx = 0;
	for (const auto& att : attachments_map) {
		mtl_pass_desc.colorAttachments[att.first].texture = ((const metal_image*)att.second)->get_metal_image();
		if (dynamic_render_state.clear_values) {
			const auto dbl_clear_color = (*dynamic_render_state.clear_values)[attachment_idx].color.cast<double>();
			mtl_pass_desc.colorAttachments[att.first].clearColor = MTLClearColorMake(dbl_clear_color.x, dbl_clear_color.y,
																					 dbl_clear_color.z, dbl_clear_color.w);
		}
		++attachment_idx;
	}
	if (depth_attachment != nullptr) {
		mtl_pass_desc.depthAttachment.texture = ((const metal_image*)depth_attachment)->get_metal_image();
		if (dynamic_render_state.clear_values) {
			mtl_pass_desc.depthAttachment.clearDepth = double((*dynamic_render_state.clear_values)[attachment_idx].depth);
		}
		++attachment_idx;
	}
	
	// create and setup the encoder
	encoder = [cmd_buffer renderCommandEncoderWithDescriptor:mtl_pass_desc];
	encoder.label = @"metal_renderer encoder";
	
	[encoder setCullMode:metal_pipeline::metal_cull_mode_from_cull_mode(pipeline_desc.cull_mode)];
	[encoder setFrontFacingWinding:metal_pipeline::metal_winding_from_front_face(pipeline_desc.front_face)];
	[encoder setBlendColorRed:pipeline_desc.blend.constant_color.x
						green:pipeline_desc.blend.constant_color.y
						 blue:pipeline_desc.blend.constant_color.z
						alpha:pipeline_desc.blend.constant_alpha];
	
	// viewport handling:
	// since Metal uses top-left origin for framebuffers, flip the viewport vertically, so that the origin is where it's supposed to be
	MTLViewport viewport;
	if (!dynamic_render_state.viewport) {
		viewport = {
			.originX = 0.0,
			.originY = double(pipeline_desc.viewport.y),
			.width = double(pipeline_desc.viewport.x),
			.height = -double(pipeline_desc.viewport.y),
			.znear = double(pipeline_desc.depth.range.x),
			.zfar = double(pipeline_desc.depth.range.y)
		};
	} else {
		viewport = {
			.originX = 0.0,
			.originY = double(dynamic_render_state.viewport->y),
			.width = double(dynamic_render_state.viewport->x),
			.height = -double(dynamic_render_state.viewport->y),
			.znear = double(pipeline_desc.depth.range.x),
			.zfar = double(pipeline_desc.depth.range.y)
		};
	}
	[encoder setViewport:viewport];
	
	// scissor handling:
	MTLScissorRect scissor_rect;
	if (!dynamic_render_state.scissor) {
		scissor_rect = {
			.x = pipeline_desc.scissor.offset.x,
			.y = pipeline_desc.scissor.offset.y,
			.width = pipeline_desc.scissor.extent.x,
			.height = pipeline_desc.scissor.extent.y
		};
	} else {
		scissor_rect = {
			.x = dynamic_render_state.scissor->offset.x,
			.y = dynamic_render_state.scissor->offset.y,
			.width = dynamic_render_state.scissor->extent.x,
			.height = dynamic_render_state.scissor->extent.y
		};
	}
	if (scissor_rect.x + scissor_rect.width > (uint32_t)viewport.width ||
		scissor_rect.y + scissor_rect.height > (uint32_t)viewport.height) {
		log_error("scissor rectangle is out-of-bounds: @%v + %v > %v",
				  ulong2 { scissor_rect.x, scissor_rect.y }, ulong2 { scissor_rect.width, scissor_rect.height },
				  double2 { viewport.width, viewport.height });
		return false;
	}
	[encoder setScissorRect:scissor_rect];
	
	[encoder pushDebugGroup:@"metal_renderer render"];
	[encoder setDepthStencilState:mtl_pipeline_state->depth_stencil_state];
	[encoder setRenderPipelineState:mtl_pipeline_state->pipeline_state];
	
	return true;
}

bool metal_renderer::end() {
	[encoder endEncoding];
	[encoder popDebugGroup];
	return true;
}

bool metal_renderer::commit() {
	[cmd_buffer commit];
	return true;
}

metal_renderer::metal_drawable_t::~metal_drawable_t() {
	// nop
}

graphics_renderer::drawable_t* metal_renderer::get_next_drawable(const bool get_multi_view_drawable) {
	const auto& mtl_ctx = (const metal_compute&)ctx;
	
	// VR / multi-view drawable
	if (get_multi_view_drawable) {
		cur_drawable = make_unique<metal_drawable_t>();
		cur_drawable->metal_image = mtl_ctx.get_metal_next_vr_drawable();
		if (!cur_drawable->metal_image) {
			log_error("no VR/multi-view drawable");
			return nullptr;
		}
		cur_drawable->metal_drawable = nil;
		cur_drawable->valid = true;
		cur_drawable->image = cur_drawable->metal_image.get();
		cur_drawable->is_multi_view_drawable = true;
		return cur_drawable.get();
	}
	
	// screen drawable
	auto mtl_drawable = mtl_ctx.get_metal_next_drawable(cmd_buffer);
	if (mtl_drawable == nil) {
		log_error("drawable is nil!");
		return nullptr;
	}
	
	cur_drawable = make_unique<metal_drawable_t>();
	cur_drawable->metal_drawable = mtl_drawable;
	cur_drawable->valid = true;
	cur_drawable->metal_image = make_shared<metal_image>(cqueue, mtl_drawable.texture);
	cur_drawable->image = cur_drawable->metal_image.get();
	return cur_drawable.get();
}

void metal_renderer::present() {
	if (!cur_drawable || !cur_drawable->is_valid()) {
		log_error("current drawable is invalid");
		return;
	}
	if (cur_drawable->is_multi_view_drawable) {
		const auto& mtl_ctx = (const metal_compute&)ctx;
		mtl_ctx.present_metal_vr_drawable(cqueue, *cur_drawable->metal_image.get());
	} else {
		[cmd_buffer presentDrawable:cur_drawable->metal_drawable];
	}
}

bool metal_renderer::set_attachments(vector<attachment_t>& attachments) {
	if (!graphics_renderer::set_attachments(attachments)) {
		return false;
	}
	return true;
}

bool metal_renderer::set_attachment(const uint32_t& index, attachment_t& attachment) {
	if (!graphics_renderer::set_attachment(index, attachment)) {
		return false;
	}
	return true;
}

bool metal_renderer::switch_pipeline(const graphics_pipeline& pipeline_) {
	if (!graphics_renderer::switch_pipeline(pipeline_)) {
		return false;
	}
	return update_metal_pipeline();
}

bool metal_renderer::update_metal_pipeline() {
	const auto& dev = cqueue.get_device();
	const auto& mtl_pipeline = (const metal_pipeline&)*cur_pipeline;
	mtl_pipeline_state = mtl_pipeline.get_metal_pipeline_entry(dev);
	if (mtl_pipeline_state == nullptr) {
		log_error("no pipeline state for device %s", dev.name);
		return false;
	}
	return true;
}

void metal_renderer::draw_internal(const vector<multi_draw_entry>* draw_entries,
								   const vector<multi_draw_indexed_entry>* draw_indexed_entries,
								   const vector<compute_kernel_arg>& args) const {
	const auto vs = (const metal_shader*)cur_pipeline->get_description(multi_view).vertex_shader;
	vs->set_shader_arguments(cqueue, encoder, cmd_buffer,
							 (const metal_kernel::metal_kernel_entry*)mtl_pipeline_state->vs_entry,
							 (const metal_kernel::metal_kernel_entry*)mtl_pipeline_state->fs_entry, args);
	if (draw_entries != nullptr) {
		vs->draw(encoder, cur_pipeline->get_description(multi_view).primitive, *draw_entries);
	} else if (draw_indexed_entries != nullptr) {
		vs->draw(encoder, cur_pipeline->get_description(multi_view).primitive, *draw_indexed_entries);
	}
}

#endif
