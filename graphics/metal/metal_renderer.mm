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

metal_renderer::metal_renderer(const compute_queue& cqueue_, const graphics_pass& pass_, const graphics_pipeline& pipeline_) :
graphics_renderer(cqueue_, pass_, pipeline_) {
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

bool metal_renderer::begin() {
	const auto& pipeline_desc = cur_pipeline->get_description();
	const auto& mtl_pass = (const metal_pass&)pass;
	const auto mtl_pass_desc_template = mtl_pass.get_metal_pass_desc();
	
	MTLRenderPassDescriptor* mtl_pass_desc = [mtl_pass_desc_template copy];
	
	// must set/update attachments (and drawable) before creating the encoder
	for (const auto& att : attachments_map) {
		mtl_pass_desc.colorAttachments[att.first].texture = ((const metal_image*)att.second)->get_metal_image();
	}
	if (depth_attachment != nullptr) {
		mtl_pass_desc.depthAttachment.texture = ((const metal_image*)depth_attachment)->get_metal_image();
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

const graphics_renderer::drawable_t* metal_renderer::get_next_drawable() {
	auto mtl_drawable = ((const metal_compute&)ctx).get_metal_next_drawable(cmd_buffer);
	if (mtl_drawable == nil) {
		log_error("drawable is nil!");
		return {};
	}
	
	cur_drawable = make_unique<metal_drawable_t>();
	cur_drawable->metal_drawable = mtl_drawable;
	cur_drawable->valid = true;
	cur_drawable->metal_image = make_unique<metal_image>(cqueue, mtl_drawable.texture);
	cur_drawable->image = cur_drawable->metal_image.get();
	return cur_drawable.get();
}

void metal_renderer::present() {
	if (!cur_drawable || !cur_drawable->is_valid()) {
		log_error("current drawable is invalid");
		return;
	}
	[cmd_buffer presentDrawable:cur_drawable->metal_drawable];
}

bool metal_renderer::set_attachments(const vector<attachment_t>& attachments) {
	if (!graphics_renderer::set_attachments(attachments)) {
		return false;
	}
	return true;
}

bool metal_renderer::set_attachment(const uint32_t& index, const attachment_t& attachment) {
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
	const auto vs = (const metal_shader*)cur_pipeline->get_description().vertex_shader;
	vs->set_shader_arguments(cqueue, encoder, cmd_buffer,
							 (const metal_kernel::metal_kernel_entry*)mtl_pipeline_state->vs_entry,
							 (const metal_kernel::metal_kernel_entry*)mtl_pipeline_state->fs_entry, args);
	if (draw_entries != nullptr) {
		vs->draw(encoder, cur_pipeline->get_description().primitive, *draw_entries);
	} else if (draw_indexed_entries != nullptr) {
		vs->draw(encoder, cur_pipeline->get_description().primitive, *draw_indexed_entries);
	}
}

#endif
