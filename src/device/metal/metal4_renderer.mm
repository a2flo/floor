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

#include <floor/device/metal/metal4_renderer.hpp>

#if !defined(FLOOR_NO_METAL)
#include <floor/core/essentials.hpp>
#include <floor/device/metal/metal_context.hpp>
#include <floor/device/metal/metal_fence.hpp>
#include <floor/device/metal/metal_device.hpp>
#include <floor/device/metal/metal4_queue.hpp>
#include <floor/device/metal/metal4_buffer.hpp>
#include <floor/device/metal/metal4_indirect_command.hpp>
#include <floor/device/metal/metal4_soft_indirect_command.hpp>
#include <floor/device/metal/metal4_pass.hpp>
#include <floor/device/metal/metal4_pipeline.hpp>
#include <floor/device/metal/metal4_shader.hpp>
#include <floor/device/generic_indirect_command.hpp>
#include <floor/core/logger.hpp>

// work around C++26 compat issue
#define CGBitmapInfoMake(...) CGBitmapInfoMakeDummy(uint32_t alpha, uint32_t component, uint32_t byteOrder, uint32_t pixelFormat)
#import <QuartzCore/CAMetalLayer.h>

namespace fl {

metal4_renderer::metal4_renderer(const device_queue& cqueue_,
								 const graphics_pass& pass_,
								 const graphics_pipeline& pipeline_,
								 const bool multi_view_) :
graphics_renderer(cqueue_, pass_, pipeline_, multi_view_) {
	@autoreleasepool {
		if (!valid) {
			// already marked invalid, no point in continuing
			return;
		}
		
#if defined(FLOOR_DEBUG)
		const auto& pipeline_desc = cur_pipeline->get_description(multi_view);
#endif
		
		// create a command buffer from the specified queue (this will be used throughout until commit)
		cmd_buffer = ((const metal4_queue&)cqueue).make_command_buffer(
#if defined(FLOOR_DEBUG)
																	   pipeline_desc.debug_label.c_str()
#endif
																	   );
		assert(cmd_buffer);
		
#if defined(FLOOR_DEBUG)
		cmd_buffer.cmd_buffer.label = (pipeline_desc.debug_label.empty() ? @"metal4_renderer" :
									   [NSString stringWithUTF8String:pipeline_desc.debug_label.c_str()]);
#endif
		
		if (!update_metal_pipeline()) {
			valid = false;
			return;
		}
		
		// all successful
		valid = true;
	}
}

metal4_renderer::~metal4_renderer() {
	@autoreleasepool {
		const auto& mtl_queue = (const metal4_queue&)cqueue;
		
		// if we acquired a drawable, but didn't present it, we need to properly free it
		if (cur_drawable && cur_drawable->is_valid() && !did_present) {
			if (!cur_drawable->is_multi_view_drawable) {
				[mtl_queue.get_queue() signalDrawable:cur_drawable->metal_drawable];
			}
		}
		
		// if we never submitted to command buffer, properly free it
		if (cmd_buffer && !did_submit) {
			if (did_begin) {
				[cmd_buffer.cmd_buffer endCommandBuffer];
			}
			mtl_queue.free_command_buffer(std::move(cmd_buffer));
		}
		
		encoder = nil;
		cur_drawable = nullptr;
		mtl_pass_desc = nil;
	}
}

bool metal4_renderer::begin(const dynamic_render_state_t dynamic_render_state) {
	const auto& pipeline_desc = cur_pipeline->get_description(multi_view);
	const auto& mtl_pass = (const metal4_pass&)pass;
	const auto mtl_pass_desc_template = mtl_pass.get_metal_pass_desc(multi_view);
	
	@autoreleasepool {
		mtl_pass_desc = [mtl_pass_desc_template copy];
		
		if (dynamic_render_state.clear_values &&
			dynamic_render_state.clear_values->size() != attachments_map.size() + (depth_attachment ? 1u : 0u)) {
			log_error("invalid clear values size: $", dynamic_render_state.clear_values->size());
			return false;
		}
		
		// must set/update attachments (and drawable) + create their residency set before creating the encoder
		uint32_t attachment_idx = 0;
		uint2 min_attachment_dim { ~0u };
		for (auto&& att : attachments_map) {
			min_attachment_dim.min(att.second.image->get_image_dim().xy);
			auto att_img = att.second.image->get_underlying_metal4_image_safe()->get_metal_image();
			mtl_pass_desc.colorAttachments[att.first].texture = att_img;
			mtl_pass_desc.colorAttachments[att.first].slice = att.second.layer;
			if ([att_img storageMode] != MTLStorageModeMemoryless) {
				[cmd_buffer.residency_set addAllocation:att_img];
			}
			if (att.second.resolve_image) {
				auto att_resolve_img = att.second.resolve_image->get_underlying_metal4_image_safe()->get_metal_image();
				min_attachment_dim.min(att.second.resolve_image->get_image_dim().xy);
				mtl_pass_desc.colorAttachments[att.first].resolveTexture = att_resolve_img;
				mtl_pass_desc.colorAttachments[att.first].resolveSlice = att.second.layer;
				if ([att_resolve_img storageMode] != MTLStorageModeMemoryless) {
					[cmd_buffer.residency_set addAllocation:att_resolve_img];
				}
			}
			if (dynamic_render_state.clear_values) {
				const auto dbl_clear_color = (*dynamic_render_state.clear_values)[attachment_idx].color.cast<double>();
				mtl_pass_desc.colorAttachments[att.first].clearColor = MTLClearColorMake(dbl_clear_color.x, dbl_clear_color.y,
																						 dbl_clear_color.z, dbl_clear_color.w);
			}
			++attachment_idx;
		}
		if (depth_attachment) {
			min_attachment_dim.min(depth_attachment->image->get_image_dim().xy);
			auto att_img = depth_attachment->image->get_underlying_metal4_image_safe()->get_metal_image();
			mtl_pass_desc.depthAttachment.texture = att_img;
			mtl_pass_desc.depthAttachment.slice = depth_attachment->layer;
			if ([att_img storageMode] != MTLStorageModeMemoryless) {
				[cmd_buffer.residency_set addAllocation:att_img];
			}
			if (depth_attachment->resolve_image) {
				auto att_resolve_img = depth_attachment->resolve_image->get_underlying_metal4_image_safe()->get_metal_image();
				min_attachment_dim.min(depth_attachment->resolve_image->get_image_dim().xy);
				mtl_pass_desc.depthAttachment.resolveTexture = att_resolve_img;
				mtl_pass_desc.depthAttachment.resolveSlice = depth_attachment->layer;
				if ([att_resolve_img storageMode] != MTLStorageModeMemoryless) {
					[cmd_buffer.residency_set addAllocation:att_resolve_img];
				}
			}
			if (dynamic_render_state.clear_values) {
				mtl_pass_desc.depthAttachment.clearDepth = double((*dynamic_render_state.clear_values)[attachment_idx].depth);
			}
			++attachment_idx;
		}
		
		[cmd_buffer.residency_set commit];
		
		// create and setup the encoder
		[cmd_buffer.cmd_buffer beginCommandBufferWithAllocator:cmd_buffer.allocator];
		did_begin = true;
		[cmd_buffer.cmd_buffer useResidencySet:cmd_buffer.residency_set];
		encoder = [cmd_buffer.cmd_buffer renderCommandEncoderWithDescriptor:mtl_pass_desc];
		encoder.label = (pipeline_desc.debug_label.empty() ? @"metal4_renderer_encoder" :
						 [NSString stringWithUTF8String:(pipeline_desc.debug_label + "_encoder").c_str()]);
		if (!pipeline_desc.debug_label.empty()) {
			[encoder pushDebugGroup:(encoder.label ? encoder.label : @"metal4_renderer")];
		}
		
		[encoder setRenderPipelineState:mtl_pipeline_state->pipeline_state];
		[encoder setDepthStencilState:mtl_pipeline_state->depth_stencil_state];
		[encoder setCullMode:metal4_pipeline::metal_cull_mode_from_cull_mode(pipeline_desc.cull_mode)];
		[encoder setFrontFacingWinding:metal4_pipeline::metal_winding_from_front_face(pipeline_desc.front_face)];
		[encoder setBlendColorRed:pipeline_desc.blend.constant_color.x
							green:pipeline_desc.blend.constant_color.y
							 blue:pipeline_desc.blend.constant_color.z
							alpha:pipeline_desc.blend.constant_alpha];
		if (pipeline_desc.render_wireframe) {
			[encoder setTriangleFillMode:MTLTriangleFillModeLines];
		}
		
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
				.width = std::min(pipeline_desc.scissor.extent.x, uint32_t(viewport.width)),
				.height = std::min(pipeline_desc.scissor.extent.y, uint32_t(-viewport.height))
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
			log_error("scissor rectangle is out-of-bounds (viewport): @$ + $ > $",
					  ulong2 { scissor_rect.x, scissor_rect.y }, ulong2 { scissor_rect.width, scissor_rect.height },
					  double2 { viewport.width, -viewport.height });
			[encoder endEncoding];
			return false;
		}
		if (scissor_rect.x + scissor_rect.width > min_attachment_dim.x ||
			scissor_rect.y + scissor_rect.height > min_attachment_dim.y) {
			log_error("scissor rectangle is out-of-bounds (attachment): @$ + $ > $",
					  ulong2 { scissor_rect.x, scissor_rect.y }, ulong2 { scissor_rect.width, scissor_rect.height },
					  min_attachment_dim);
			[encoder endEncoding];
			return false;
		}
		[encoder setScissorRect:scissor_rect];
		
		return true;
	}
}

bool metal4_renderer::end() {
	@autoreleasepool {
		[encoder endEncoding];
		
		const auto& pipeline_desc = cur_pipeline->get_description(multi_view);
		if (!pipeline_desc.debug_label.empty()) {
			[encoder popDebugGroup];
		}
		
		return true;
	}
}

bool metal4_renderer::commit_and_finish() {
	return commit_internal(true, true, {});
}

struct metal4_renderer_internal {
	template <template <typename...> class smart_ptr_type, typename renderer_type>
	static bool commit_and_release_internal(smart_ptr_type<renderer_type>&& renderer, graphics_renderer::completion_handler_f&& compl_handler) {
		auto renderer_ptr = (metal4_renderer*)renderer.get();
		// NOTE: since a std::function must be copyable, we can't use a unique_ptr here (TODO: move_only_function with C++23)
		auto queue_submission_compl_handler = [retained_renderer = std::shared_ptr<renderer_type>(std::move(renderer))]() {
			// nop
		};
		return renderer_ptr->commit_internal(false, true, std::move(compl_handler), std::move(queue_submission_compl_handler));
	}
};

bool metal4_renderer::commit_and_release(std::unique_ptr<graphics_renderer>&& renderer, completion_handler_f&& compl_handler) {
	return metal4_renderer_internal::commit_and_release_internal(std::move(renderer), std::move(compl_handler));
}

bool metal4_renderer::commit_and_release(std::shared_ptr<graphics_renderer>&& renderer, completion_handler_f&& compl_handler) {
	return metal4_renderer_internal::commit_and_release_internal(std::move(renderer), std::move(compl_handler));
}

bool metal4_renderer::commit_and_continue() {
	return commit_internal(true, false, {});
}

bool metal4_renderer::commit_internal(const bool is_blocking, const bool is_finishing,
									  completion_handler_f&& user_compl_handler,
									  completion_handler_f&& renderer_compl_handler) {
	(void)is_finishing; // not needed in Metal
	
	[cmd_buffer.cmd_buffer endCommandBuffer];
	
	const auto& mtl_queue = (const metal4_queue&)cqueue;
	mtl_queue.submit_command_buffer(std::move(cmd_buffer),
									[user_compl_handler_ = std::move(user_compl_handler),
									 renderer_compl_handler_ = std::move(renderer_compl_handler)](const metal4_command_buffer& cmd_buffer_,
																								  const bool is_error,
																								  const std::string_view err_string) {
		if (is_error) {
			log_error("renderer command buffer error (in \"$\"): $", cmd_buffer_.name ? cmd_buffer_.name : "<unnamed>", err_string);
		}
		
		if (user_compl_handler_) {
			user_compl_handler_();
		}
		
		// NOTE: must be run at the very end, because it retains the renderer
		if (renderer_compl_handler_) {
			renderer_compl_handler_();
		}
	}, is_blocking);
	did_submit = true;
	
	return true;
}

bool metal4_renderer::add_completion_handler(completion_handler_f&& compl_handler) {
	if (!compl_handler) {
		return false;
	}
	
	((const metal4_queue&)cqueue).add_completion_handler(cmd_buffer, [handler = std::move(compl_handler)] {
		handler();
	});
	return true;
}

metal4_renderer::metal_drawable_t::~metal_drawable_t() {
	@autoreleasepool {
		metal_image = nullptr;
		metal_drawable = nil;
	}
}

graphics_renderer::drawable_t* metal4_renderer::get_next_drawable(const bool get_multi_view_drawable) {
	@autoreleasepool {
		const auto& mtl_ctx = (const metal_context&)ctx;
		const auto& mtl_queue = (const metal4_queue&)cqueue;
		
		// VR / multi-view drawable
		if (get_multi_view_drawable) {
			cur_drawable = std::make_unique<metal_drawable_t>();
			cur_drawable->metal_image = mtl_ctx.get_metal_next_vr_drawable();
			if (!cur_drawable->metal_image) {
				log_error("no Metal VR/multi-view drawable");
				return nullptr;
			}
			cur_drawable->valid = true;
			cur_drawable->image = cur_drawable->metal_image.get();
			cur_drawable->is_multi_view_drawable = true;
			return cur_drawable.get();
		}
		
		// screen drawable
		auto mtl_drawable = mtl_ctx.get_metal_next_drawable((const metal4_queue&)cqueue, cmd_buffer);
		if (mtl_drawable == nil) {
			log_error("Metal drawable is nil");
			return nullptr;
		}
		
		[mtl_queue.get_queue() waitForDrawable:mtl_drawable];
		
		cur_drawable = std::make_unique<metal_drawable_t>();
		cur_drawable->metal_drawable = mtl_drawable;
		cur_drawable->valid = true;
		cur_drawable->metal_image = std::make_shared<metal4_image>(cqueue, mtl_drawable.texture, std::span<uint8_t> {});
		cur_drawable->image = cur_drawable->metal_image.get();
		return cur_drawable.get();
	}
}

void metal4_renderer::present() {
	@autoreleasepool {
		if (!cur_drawable || !cur_drawable->is_valid()) {
			log_error("current Metal drawable is invalid");
			return;
		}
		if (cur_drawable->is_multi_view_drawable) {
			const auto& mtl_ctx = (const metal_context&)ctx;
			mtl_ctx.present_metal_vr_drawable(cqueue, *cur_drawable->metal_image.get());
		} else {
			const auto& mtl_queue = (const metal4_queue&)cqueue;
			[mtl_queue.get_queue() signalDrawable:cur_drawable->metal_drawable];
			[cur_drawable->metal_drawable present];
		}
		did_present = true;
	}
}

bool metal4_renderer::set_attachments(std::vector<attachment_t>& attachments) {
	if (!graphics_renderer::set_attachments(attachments)) {
		return false;
	}
	return true;
}

bool metal4_renderer::set_attachment(const uint32_t& index, attachment_t& attachment) {
	if (!graphics_renderer::set_attachment(index, attachment)) {
		return false;
	}
	return true;
}

template <typename mtl_indirect_cmd_type>
requires (std::is_same_v<mtl_indirect_cmd_type, metal4_soft_indirect_command_pipeline> ||
		  std::is_same_v<mtl_indirect_cmd_type, metal4_indirect_command_pipeline>)
static inline void handle_metal_indirect_rendering(const mtl_indirect_cmd_type& mtl_indirect_cmd,
												   const metal4_queue& cqueue, const metal_device& dev,
												   metal4_command_buffer& cmd_buffer,
												   id <MTL4RenderCommandEncoder> encoder,
												   const metal4_pipeline::metal4_pipeline_entry& parent_mtl_pipeline_state,
												   const uint32_t command_count) {
	@autoreleasepool {
		auto mtl_indirect_pipeline_entry = mtl_indirect_cmd.get_metal_pipeline_entry(dev);
		if (!mtl_indirect_pipeline_entry) {
			log_error("no indirect command pipeline state for device \"$\" in indirect command pipeline \"$\"",
					  dev.name, mtl_indirect_cmd.get_description().debug_label);
			return;
		}
		
		if constexpr (std::is_same_v<mtl_indirect_cmd_type, metal4_soft_indirect_command_pipeline>) {
			mtl_indirect_cmd.update_resources(dev, *mtl_indirect_pipeline_entry);
		} else if constexpr (std::is_same_v<mtl_indirect_cmd_type, metal4_indirect_command_pipeline>) {
			mtl_indirect_cmd.update_resources(dev, *mtl_indirect_pipeline_entry);
		}
		
		// declare all used resources
		const auto has_resources = mtl_indirect_pipeline_entry->has_resources();
		if (has_resources) {
			[cmd_buffer.cmd_buffer useResidencySet:mtl_indirect_pipeline_entry->residency_set];
#if FLOOR_METAL_DEBUG_RS
			dev.commit_debug_residency_set();
#endif
		}
		
		if (mtl_indirect_pipeline_entry->printf_buffer) {
			mtl_indirect_pipeline_entry->printf_init(cqueue);
		}
		
		if constexpr (std::is_same_v<mtl_indirect_cmd_type, metal4_indirect_command_pipeline>) {
			[encoder executeCommandsInBuffer:mtl_indirect_pipeline_entry->icb
								   withRange:NSRange { .location = 0u, .length = command_count }];
		} else {
			id <MTLRenderPipelineState> cur_pipeline_state = parent_mtl_pipeline_state.pipeline_state;
			for (uint32_t cmd_idx = 0u; cmd_idx < command_count; ++cmd_idx) {
				const auto& cmd_encoder = mtl_indirect_cmd.get_render_command(cmd_idx);
				const auto& cmd_params = mtl_indirect_pipeline_entry->render_commands[cmd_idx];
				
				if (cur_pipeline_state != cmd_encoder.pipeline_state) {
					[encoder setRenderPipelineState:cmd_encoder.pipeline_state];
					cur_pipeline_state = cmd_encoder.pipeline_state;
				}
				
				[encoder setArgumentTable:cmd_encoder.vs_arg_table atStages:MTLRenderStageVertex];
				if (cmd_encoder.fs_arg_table) {
					[encoder setArgumentTable:cmd_encoder.fs_arg_table atStages:MTLRenderStageFragment];
				}
				
				if (!cmd_params.is_indexed) {
					[encoder drawPrimitives:cmd_params.vertex.primitive_type
								vertexStart:cmd_params.vertex.first_vertex
								vertexCount:cmd_params.vertex.vertex_count
							  instanceCount:cmd_params.vertex.instance_count
							   baseInstance:cmd_params.vertex.first_instance];
				} else {
					[encoder drawIndexedPrimitives:cmd_params.indexed.primitive_type
										indexCount:cmd_params.indexed.index_count
										 indexType:cmd_params.indexed.index_type
									   indexBuffer:cmd_params.indexed.index_buffer_address
								 indexBufferLength:cmd_params.indexed.index_buffer_length
									 instanceCount:cmd_params.indexed.instance_count
										baseVertex:cmd_params.indexed.vertex_offset
									  baseInstance:cmd_params.indexed.base_instance];
				}
			}
			
			// reset parent pipeline?
			if (cur_pipeline_state != parent_mtl_pipeline_state.pipeline_state) {
				[encoder setRenderPipelineState:parent_mtl_pipeline_state.pipeline_state];
			}
		}
		
		if (mtl_indirect_pipeline_entry->printf_buffer) {
			cqueue.printf_completion(cmd_buffer, mtl_indirect_pipeline_entry->printf_buffer);
		}
	}
};

void metal4_renderer::execute_indirect(const indirect_command_pipeline& indirect_cmd) {
	const auto command_count = indirect_cmd.get_command_count();
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
	
	if (const auto generic_ind_pipeline = dynamic_cast<const generic_indirect_command_pipeline*>(&indirect_cmd); generic_ind_pipeline) {
		graphics_renderer::execute_indirect(indirect_cmd);
		return;
	} else if (const auto mtl_soft_indirect_cmd = dynamic_cast<const metal4_soft_indirect_command_pipeline*>(&indirect_cmd);
			   mtl_soft_indirect_cmd) {
		handle_metal_indirect_rendering(*mtl_soft_indirect_cmd, (const metal4_queue&)cqueue, (const metal_device&)cqueue.get_device(),
										cmd_buffer, encoder, *mtl_pipeline_state, command_count);
		return;
	} else if (const auto mtl_indirect_cmd = dynamic_cast<const metal4_indirect_command_pipeline*>(&indirect_cmd);
			   mtl_indirect_cmd) {
		handle_metal_indirect_rendering(*mtl_indirect_cmd, (const metal4_queue&)cqueue, (const metal_device&)cqueue.get_device(),
										cmd_buffer, encoder, *mtl_pipeline_state, command_count);
		return;
	} else {
		log_error("invalid indirect command pipeline \"$\"", indirect_cmd.get_description().debug_label);
		return;
	}
}

bool metal4_renderer::switch_pipeline(const graphics_pipeline& pipeline_) {
	if (!graphics_renderer::switch_pipeline(pipeline_)) {
		return false;
	}
	return update_metal_pipeline();
}

bool metal4_renderer::update_metal_pipeline() {
	const auto& dev = cqueue.get_device();
	const auto& mtl_pipeline = (const metal4_pipeline&)*cur_pipeline;
	mtl_pipeline_state = mtl_pipeline.get_metal_pipeline_entry(dev);
	if (mtl_pipeline_state == nullptr) {
		log_error("no pipeline state for device $", dev.name);
		return false;
	}
	return true;
}

void metal4_renderer::draw_internal(const std::span<const multi_draw_entry> draw_entries,
									const std::span<const multi_draw_indexed_entry> draw_indexed_entries,
									const std::vector<device_function_arg>& args) {
	@autoreleasepool {
		const auto vs = (const metal4_shader*)cur_pipeline->get_description(multi_view).vertex_shader;
		if (!vs->set_shader_arguments(cqueue, encoder, cmd_buffer,
									  (const metal4_function_entry*)mtl_pipeline_state->vs_entry,
									  (const metal4_function_entry*)mtl_pipeline_state->fs_entry, args,
									  draw_indexed_entries)) {
			return;
		}
		assert(draw_entries.empty() != draw_indexed_entries.empty()); // only one must be active
		if (!draw_entries.empty()) {
			vs->draw(encoder, cur_pipeline->get_description(multi_view).primitive, draw_entries);
		} else if (!draw_indexed_entries.empty()) {
			vs->draw(encoder, cur_pipeline->get_description(multi_view).primitive, draw_indexed_entries);
		}
	}
}

void metal4_renderer::draw_patches_internal(const patch_draw_entry*,
											const patch_draw_indexed_entry*,
											const std::vector<device_function_arg>&) {
	throw std::runtime_error("tessellation is not implemented in Metal 4");
}

static inline MTLStages sync_stage_to_metal_render_stages(const SYNC_STAGE stage) {
	assert(stage != SYNC_STAGE::NONE);
	MTLStages mtl_stages { 0u };
	if (has_flag<SYNC_STAGE::VERTEX>(stage) || has_flag<SYNC_STAGE::BOTTOM_OF_PIPE>(stage)) {
		mtl_stages |= MTLStageVertex;
	}
	if (has_flag<SYNC_STAGE::FRAGMENT>(stage) || has_flag<SYNC_STAGE::COLOR_ATTACHMENT_OUTPUT>(stage) || has_flag<SYNC_STAGE::TOP_OF_PIPE>(stage)) {
		mtl_stages |= MTLStageFragment;
	}
	return mtl_stages;
}

void metal4_renderer::wait_for_fence(const device_fence& fence, const SYNC_STAGE before_stage) {
	@autoreleasepool {
		[encoder waitForFence:((const metal_fence&)fence).get_metal_fence()
		  beforeEncoderStages:sync_stage_to_metal_render_stages(before_stage)];
	}
}

void metal4_renderer::signal_fence(device_fence& fence, const SYNC_STAGE after_stage) {
	@autoreleasepool {
		[encoder updateFence:((const metal_fence&)fence).get_metal_fence()
		  afterEncoderStages:sync_stage_to_metal_render_stages(after_stage)];
	}
}

} // namespace fl

#endif
