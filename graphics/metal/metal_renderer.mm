/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2022 Florian Ziesche
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
#include <floor/compute/metal/metal_indirect_command.hpp>
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
	const auto& pipeline_desc = cur_pipeline->get_description(multi_view);
	cmd_buffer.label = (pipeline_desc.debug_label.empty() ? @"metal_renderer" :
						[NSString stringWithUTF8String:(pipeline_desc.debug_label).c_str()]);
	
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
		log_error("invalid clear values size: $", dynamic_render_state.clear_values->size());
		return false;
	}
	
	// must set/update attachments (and drawable) before creating the encoder
	uint32_t attachment_idx = 0;
	for (const auto& att : attachments_map) {
		mtl_pass_desc.colorAttachments[att.first].texture = ((const metal_image*)att.second.image)->get_metal_image();
		if (att.second.resolve_image) {
			mtl_pass_desc.colorAttachments[att.first].resolveTexture = ((const metal_image*)att.second.resolve_image)->get_metal_image();
		}
		if (dynamic_render_state.clear_values) {
			const auto dbl_clear_color = (*dynamic_render_state.clear_values)[attachment_idx].color.cast<double>();
			mtl_pass_desc.colorAttachments[att.first].clearColor = MTLClearColorMake(dbl_clear_color.x, dbl_clear_color.y,
																					 dbl_clear_color.z, dbl_clear_color.w);
		}
		++attachment_idx;
	}
	if (depth_attachment) {
		mtl_pass_desc.depthAttachment.texture = ((const metal_image*)depth_attachment->image)->get_metal_image();
		if (depth_attachment->resolve_image) {
			mtl_pass_desc.depthAttachment.resolveTexture = ((const metal_image*)depth_attachment->resolve_image)->get_metal_image();
		}
		if (dynamic_render_state.clear_values) {
			mtl_pass_desc.depthAttachment.clearDepth = double((*dynamic_render_state.clear_values)[attachment_idx].depth);
		}
		++attachment_idx;
	}
	
	// create and setup the encoder
	encoder = [cmd_buffer renderCommandEncoderWithDescriptor:mtl_pass_desc];
	encoder.label = (pipeline_desc.debug_label.empty() ? @"metal_renderer encoder" :
					 [NSString stringWithUTF8String:(pipeline_desc.debug_label + " encoder").c_str()]);
	if (!pipeline_desc.debug_label.empty()) {
		[encoder pushDebugGroup:(encoder.label ? encoder.label : @"metal_renderer")];
	}
	
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
			.width = min(pipeline_desc.scissor.extent.x, uint32_t(viewport.width)),
			.height = min(pipeline_desc.scissor.extent.y, uint32_t(-viewport.height))
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
		scissor_rect.y + scissor_rect.height > (uint32_t)-viewport.height) {
		log_error("scissor rectangle is out-of-bounds: @$ + $ > $",
				  ulong2 { scissor_rect.x, scissor_rect.y }, ulong2 { scissor_rect.width, scissor_rect.height },
				  double2 { viewport.width, -viewport.height });
		return false;
	}
	[encoder setScissorRect:scissor_rect];
	
	[encoder setDepthStencilState:mtl_pipeline_state->depth_stencil_state];
	[encoder setRenderPipelineState:mtl_pipeline_state->pipeline_state];
	
	return true;
}

bool metal_renderer::end() {
	[encoder endEncoding];
	
	const auto& pipeline_desc = cur_pipeline->get_description(multi_view);
	if (!pipeline_desc.debug_label.empty()) {
		[encoder popDebugGroup];
	}
	
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

void metal_renderer::execute_indirect(const indirect_command_pipeline& indirect_cmd,
									  const uint32_t command_offset,
									  const uint32_t command_count) const {
	if (command_count == 0) {
		return;
	}
	
	const auto& mtl_indirect_cmd = (const metal_indirect_command_pipeline&)indirect_cmd;
	const auto mtl_indirect_pipeline_entry = mtl_indirect_cmd.get_metal_pipeline_entry(cqueue.get_device());
	if (!mtl_indirect_pipeline_entry) {
		log_error("no indirect command pipeline state for device \"$\" in indirect command pipeline \"$\"",
				  cqueue.get_device().name, indirect_cmd.get_description().debug_label);
		return;
	}
	
	const auto range = mtl_indirect_cmd.compute_and_validate_command_range(command_offset, command_count);
	if (!range) {
		return;
	}
	
	// declare all used resources
	// TODO: efficient resource usage declaration for command ranges != full range (see warning above)
	const auto& resources = mtl_indirect_pipeline_entry->get_resources();
	if (!resources.read_only.empty()) {
		[encoder useResources:resources.read_only.data()
						count:resources.read_only.size()
						usage:MTLResourceUsageRead];
	}
	if (!resources.write_only.empty()) {
		[encoder useResources:resources.write_only.data()
						count:resources.write_only.size()
						usage:MTLResourceUsageWrite];
	}
	if (!resources.read_write.empty()) {
		[encoder useResources:resources.read_write.data()
						count:resources.read_write.size()
						usage:(MTLResourceUsageRead | MTLResourceUsageWrite)];
	}
	if (!resources.read_only_images.empty()) {
		[encoder useResources:resources.read_only_images.data()
						count:resources.read_only_images.size()
						usage:(MTLResourceUsageRead | MTLResourceUsageSample)];
	}
	if (!resources.read_write_images.empty()) {
		[encoder useResources:resources.read_write_images.data()
						count:resources.read_write_images.size()
						usage:(MTLResourceUsageRead | MTLResourceUsageWrite | MTLResourceUsageSample)];
	}
	
	[encoder executeCommandsInBuffer:mtl_indirect_pipeline_entry->icb
						   withRange:*range];
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
		log_error("no pipeline state for device $", dev.name);
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

bool metal_renderer::set_tessellation_factors(const compute_buffer& tess_factors_buffer) {
	if (!graphics_renderer::set_tessellation_factors(tess_factors_buffer)) {
		return false;
	}
	
	[encoder setTessellationFactorBuffer:((const metal_buffer&)tess_factors_buffer).get_metal_buffer()
								  offset:0u
						  instanceStride:0u];
	
	return true;
}

void metal_renderer::draw_patches_internal(const patch_draw_entry* draw_entry,
										   const patch_draw_indexed_entry* draw_indexed_entry,
										   const vector<compute_kernel_arg>& args) const {
	const auto tes = (const metal_shader*)cur_pipeline->get_description(multi_view).vertex_shader;
	tes->set_shader_arguments(cqueue, encoder, cmd_buffer,
							  (const metal_kernel::metal_kernel_entry*)mtl_pipeline_state->vs_entry,
							  (const metal_kernel::metal_kernel_entry*)mtl_pipeline_state->fs_entry, args);
	if (draw_entry != nullptr) {
		tes->draw(encoder, *draw_entry);
	} else if (draw_indexed_entry != nullptr) {
		tes->draw(encoder, *draw_indexed_entry);
	}
}

#endif
