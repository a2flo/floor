/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2024 Florian Ziesche
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

#include <floor/graphics/metal/metal_shader.hpp>

#if !defined(FLOOR_NO_METAL)
#include <floor/core/essentials.hpp>
#include <floor/compute/metal/metal_buffer.hpp>
#include <floor/compute/metal/metal_image.hpp>
#include <floor/compute/metal/metal_compute.hpp>
#include <floor/compute/metal/metal_args.hpp>
#include <floor/graphics/metal/metal_pipeline.hpp>
#include <floor/compute/soft_printf.hpp>

metal_shader::metal_shader(kernel_map_type&& kernels_) : metal_kernel(std::move(kernels_)) {
}

void metal_shader::execute(const compute_queue& cqueue floor_unused,
						   const bool& is_cooperative floor_unused,
						   const bool& wait_until_completion floor_unused,
						   const uint32_t& dim floor_unused,
						   const uint3& global_work_size floor_unused,
						   const uint3& local_work_size floor_unused,
						   const vector<compute_kernel_arg>& args floor_unused,
						   const vector<const compute_fence*>& wait_fences floor_unused,
						   const vector<compute_fence*>& signal_fences floor_unused,
						   const char* debug_label floor_unused,
						   kernel_completion_handler_f&& completion_handler floor_unused) const {
	log_error("executing a shader is not supported!");
}

void metal_shader::set_shader_arguments(const compute_queue& cqueue,
										id <MTLRenderCommandEncoder> encoder,
										id <MTLCommandBuffer> cmd_buffer,
										const metal_kernel_entry* vertex_shader,
										const metal_kernel_entry* fragment_shader,
										const vector<compute_kernel_arg>& args) const {
	const auto dev = &cqueue.get_device();
	const auto ctx = (const metal_compute*)dev->context;
	
	// create implicit args
	vector<compute_kernel_arg> implicit_args;

	// create + init printf buffers if soft-printf is used
	vector<pair<compute_buffer*, uint32_t>> printf_buffer_rsrcs;
	const auto is_vs_soft_printf = has_flag<FUNCTION_FLAGS::USES_SOFT_PRINTF>(vertex_shader->info->flags);
	const auto is_fs_soft_printf = (fragment_shader != nullptr && has_flag<FUNCTION_FLAGS::USES_SOFT_PRINTF>(fragment_shader->info->flags));
	if (is_vs_soft_printf || is_fs_soft_printf) {
		const uint32_t printf_buffer_count = (is_vs_soft_printf ? 1u : 0u) + (is_fs_soft_printf ? 1u : 0u);
		for (uint32_t i = 0; i < printf_buffer_count; ++i) {
			auto rsrc = ctx->acquire_soft_printf_buffer(*dev);
			initialize_printf_buffer(cqueue, *rsrc.first);
			implicit_args.emplace_back(rsrc.first);
			printf_buffer_rsrcs.emplace_back(std::move(rsrc));
		}
	}

	// set and handle kernel arguments
	metal_args::set_and_handle_arguments<metal_args::ENCODER_TYPE::SHADER>(cqueue.get_device(), encoder, {
		(vertex_shader ? vertex_shader->info : nullptr),
		(fragment_shader ? fragment_shader->info : nullptr),
	}, args, implicit_args);
	
	// add completion handler to evaluate printf buffers on completion
	if (is_vs_soft_printf || is_fs_soft_printf) {
		auto internal_dev_queue = ((const metal_compute*)cqueue.get_device().context)->get_device_default_queue(cqueue.get_device());
		[cmd_buffer addCompletedHandler:^(id <MTLCommandBuffer>) {
			for (const auto& printf_buffer_rsrc : printf_buffer_rsrcs) {
				auto cpu_printf_buffer = make_unique<uint32_t[]>(printf_buffer_size / 4);
				printf_buffer_rsrc.first->read(*internal_dev_queue, cpu_printf_buffer.get());
				handle_printf_buffer(cpu_printf_buffer);
				ctx->release_soft_printf_buffer(*dev, printf_buffer_rsrc);
			}
		}];
	}
}

void metal_shader::draw(id <MTLRenderCommandEncoder> encoder, const PRIMITIVE& primitive,
						const vector<graphics_renderer::multi_draw_entry>& draw_entries) const {
	const auto mtl_primitve = metal_pipeline::metal_primitive_type_from_primitive(primitive);
	for (const auto& entry : draw_entries) {
		[encoder drawPrimitives:mtl_primitve
					vertexStart:entry.first_vertex
					vertexCount:entry.vertex_count
				  instanceCount:entry.instance_count
				   baseInstance:entry.first_instance];
	}
}

void metal_shader::draw(id <MTLRenderCommandEncoder> encoder, const PRIMITIVE& primitive,
						const vector<graphics_renderer::multi_draw_indexed_entry>& draw_indexed_entries) const {
	const auto mtl_primitve = metal_pipeline::metal_primitive_type_from_primitive(primitive);
	for (const auto& entry : draw_indexed_entries) {
		[encoder drawIndexedPrimitives:mtl_primitve
							indexCount:entry.index_count
							 indexType:MTLIndexTypeUInt32
						   indexBuffer:((const metal_buffer*)entry.index_buffer)->get_metal_buffer()
					 indexBufferOffset:(entry.first_index * 4u)
						 instanceCount:entry.instance_count
							baseVertex:entry.vertex_offset
						  baseInstance:entry.first_instance];
	}
}

void metal_shader::draw(id <MTLRenderCommandEncoder> encoder, const graphics_renderer::patch_draw_entry& entry) const {
	if (entry.control_point_buffers.empty()) {
		log_error("no control-point buffer specified");
		return;
	}
	
	// always contiguous at the front
	uint32_t vbuffer_idx = 0u;
	for (const auto& vbuffer : entry.control_point_buffers) {
		[encoder setVertexBuffer:((const metal_buffer*)vbuffer)->get_metal_buffer()
						  offset:0u
						 atIndex:vbuffer_idx++];
	}
	
	[encoder drawPatches:entry.patch_control_point_count
			  patchStart:entry.first_patch
			  patchCount:entry.patch_count
		patchIndexBuffer:nil
  patchIndexBufferOffset:0u
		   instanceCount:entry.instance_count
			baseInstance:entry.first_instance];
}

void metal_shader::draw(id <MTLRenderCommandEncoder> encoder,
						const graphics_renderer::patch_draw_indexed_entry& entry) const {
	if (entry.control_point_buffers.empty()) {
		log_error("no control-point buffer specified");
		return;
	}
	if (!entry.control_point_index_buffer) {
		log_error("invalid control-point index buffer");
		return;
	}
	
	// always contiguous at the front
	uint32_t vbuffer_idx = 0u;
	for (const auto& vbuffer : entry.control_point_buffers) {
		[encoder setVertexBuffer:((const metal_buffer*)vbuffer)->get_metal_buffer()
						  offset:0u
						 atIndex:vbuffer_idx++];
	}
	
	[encoder drawIndexedPatches:entry.patch_control_point_count
					 patchStart:entry.first_patch
					 patchCount:entry.patch_count
			   patchIndexBuffer:nil
		 patchIndexBufferOffset:0u
		controlPointIndexBuffer:((const metal_buffer*)entry.control_point_index_buffer)->get_metal_buffer()
  controlPointIndexBufferOffset:(entry.first_index * 4u)
				  instanceCount:entry.instance_count
				   baseInstance:entry.first_instance];
}

#endif
