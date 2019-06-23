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

#include <floor/graphics/metal/metal_shader.hpp>

#if !defined(FLOOR_NO_METAL)

#include <floor/compute/metal/metal_buffer.hpp>
#include <floor/compute/metal/metal_image.hpp>
#include <floor/compute/metal/metal_compute.hpp>
#include <floor/compute/metal/metal_args.hpp>
#include <floor/graphics/metal/metal_pipeline.hpp>
#include <floor/compute/soft_printf.hpp>

metal_shader::metal_shader(kernel_map_type&& kernels_) : metal_kernel(move(kernels_)) {
}

void metal_shader::execute(const compute_queue& cqueue floor_unused,
						   const bool& is_cooperative floor_unused,
						   const uint32_t& dim floor_unused,
						   const uint3& global_work_size floor_unused,
						   const uint3& local_work_size floor_unused,
						   const vector<compute_kernel_arg>& args floor_unused) const {
	log_error("executing a shader is not supported!");
}

void metal_shader::set_shader_arguments(const compute_queue& cqueue,
										id <MTLRenderCommandEncoder> encoder,
										id <MTLCommandBuffer> cmd_buffer,
										const metal_kernel_entry* vertex_shader,
										const metal_kernel_entry* fragment_shader,
										const vector<compute_kernel_arg>& args) const {
	// create implicit args
	vector<compute_kernel_arg> implicit_args;

	// create + init printf buffers if soft-printf is used
	vector<shared_ptr<compute_buffer>> printf_buffers;
	const auto is_vs_soft_printf = function_info::has_flag<function_info::FUNCTION_FLAGS::USES_SOFT_PRINTF>(vertex_shader->info->flags);
	const auto is_fs_soft_printf = (fragment_shader != nullptr && function_info::has_flag<function_info::FUNCTION_FLAGS::USES_SOFT_PRINTF>(fragment_shader->info->flags));
	if (is_vs_soft_printf || is_fs_soft_printf) {
		const uint32_t printf_buffer_count = (is_vs_soft_printf ? 1u : 0u) + (is_fs_soft_printf ? 1u : 0u);
		for (uint32_t i = 0; i < printf_buffer_count; ++i) {
			auto printf_buffer = cqueue.get_device().context->create_buffer(cqueue, printf_buffer_size);
			printf_buffer->write_from(uint2 { printf_buffer_header_size, printf_buffer_size }, cqueue);
			printf_buffers.emplace_back(printf_buffer);
			implicit_args.emplace_back(printf_buffer);
		}
	}

	// set and handle kernel arguments
	metal_args::set_and_handle_arguments<metal_args::FUNCTION_TYPE::SHADER>(encoder, { vertex_shader, fragment_shader }, args, implicit_args);
	
	// add completion handler to evaluate printf buffers on completion
	if (is_vs_soft_printf || is_fs_soft_printf) {
		auto internal_dev_queue = ((const metal_compute*)cqueue.get_device().context)->get_device_internal_queue(cqueue.get_device());
		[cmd_buffer addCompletedHandler:^(id <MTLCommandBuffer>) {
			for (const auto& printf_buffer : printf_buffers) {
				auto cpu_printf_buffer = make_unique<uint32_t[]>(printf_buffer_size / 4);
				printf_buffer->read(*internal_dev_queue, cpu_printf_buffer.get());
				handle_printf_buffer(cpu_printf_buffer);
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
					 indexBufferOffset:entry.first_index
						 instanceCount:entry.instance_count
							baseVertex:entry.vertex_offset
						  baseInstance:entry.first_instance];
	}
}

#endif
