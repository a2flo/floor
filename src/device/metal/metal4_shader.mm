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

#include <floor/device/metal/metal4_shader.hpp>

#if !defined(FLOOR_NO_METAL)
#include <floor/core/essentials.hpp>
#include <floor/device/metal/metal4_buffer.hpp>
#include <floor/device/metal/metal4_image.hpp>
#include <floor/device/metal/metal4_args.hpp>
#include <floor/device/metal/metal4_pipeline.hpp>
#include <floor/device/metal/metal4_queue.hpp>
#include <floor/device/metal/metal_context.hpp>
#include <floor/device/soft_printf.hpp>

namespace fl {
using namespace std::literals;

metal4_shader::metal4_shader(function_map_type&& functions_) : metal4_function(""sv, std::move(functions_)) {
}

void metal4_shader::execute(const device_queue& cqueue floor_unused,
							const bool& is_cooperative floor_unused,
							const bool& wait_until_completion floor_unused,
							const uint32_t& dim floor_unused,
							const uint3& global_work_size floor_unused,
							const uint3& local_work_size floor_unused,
							const std::vector<device_function_arg>& args floor_unused,
							const std::vector<const device_fence*>& wait_fences floor_unused,
							const std::vector<device_fence*>& signal_fences floor_unused,
							const char* debug_label floor_unused,
							kernel_completion_handler_f&& completion_handler floor_unused) const {
	log_error("executing a shader is not supported!");
}

bool metal4_shader::set_shader_arguments(const device_queue& cqueue,
										 id <MTL4RenderCommandEncoder> encoder,
										 metal4_command_buffer& cmd_buffer,
										 const metal4_function_entry* vertex_shader,
										 const metal4_function_entry* fragment_shader,
										 const std::vector<device_function_arg>& args,
										 const std::span<const graphics_renderer::multi_draw_indexed_entry> draw_indexed_entries) const {
	@autoreleasepool {
		const auto dev = &cqueue.get_device();
		const auto ctx = (const metal_context*)dev->context;
		const auto& mtl_queue = (const metal4_queue&)cqueue;
		
		// create implicit args
		std::vector<device_function_arg> implicit_args;
		
		// create + init printf buffers if soft-printf is used
		std::vector<std::pair<device_buffer*, uint32_t>> printf_buffer_rsrcs;
		const auto is_vs_soft_printf = has_flag<toolchain::FUNCTION_FLAGS::USES_SOFT_PRINTF>(vertex_shader->info->flags);
		const auto is_fs_soft_printf = (fragment_shader != nullptr &&
										has_flag<toolchain::FUNCTION_FLAGS::USES_SOFT_PRINTF>(fragment_shader->info->flags));
		if (is_vs_soft_printf || is_fs_soft_printf) {
			const uint32_t printf_buffer_count = (is_vs_soft_printf ? 1u : 0u) + (is_fs_soft_printf ? 1u : 0u);
			for (uint32_t i = 0; i < printf_buffer_count; ++i) {
				auto rsrc = ctx->acquire_soft_printf_buffer(*dev);
				initialize_printf_buffer(cqueue, *rsrc.first);
				implicit_args.emplace_back(rsrc.first);
				printf_buffer_rsrcs.emplace_back(std::move(rsrc));
			}
		}
		
		// set and handle function arguments
		metal4_args::argument_table_encoders_t<2u> shader_encoders;
		
		auto vs_acq_args_res = vertex_shader->acquire_exec_instance(cqueue);
		auto& vs_args_res = *vs_acq_args_res.res;
		shader_encoders.encoders[0] = {
			.arg_table = vs_args_res.arg_table,
			.constants_buffer = vs_args_res.constants_buffer.get(),
			.constant_buffer_info = &vertex_shader->constant_buffer_info,
		};
		vs_args_res.resources.clear_resources();
		
		decltype(vs_acq_args_res) fs_acq_args_res {};
		id <MTL4ArgumentTable> fs_arg_table = nil;
		metal4_resource_tracking<true>* fs_resources = nullptr;
		if (fragment_shader) {
			fs_acq_args_res = fragment_shader->acquire_exec_instance(cqueue);
			auto& fs_args_res = *fs_acq_args_res.res;
			fs_arg_table = fs_args_res.arg_table;
			shader_encoders.encoders[1] = {
				.arg_table = fs_args_res.arg_table,
				.constants_buffer = fs_args_res.constants_buffer.get(),
				.constant_buffer_info = &fragment_shader->constant_buffer_info,
			};
			fs_resources = &fs_args_res.resources;
			fs_args_res.resources.clear_resources();
		}
		
		if (!metal4_args::set_and_handle_arguments<metal4_args::ENCODER_TYPE::ARGUMENT_TABLE, true, 2u>(cqueue.get_device(), shader_encoders, {
			vertex_shader->info, (fragment_shader ? fragment_shader->info : nullptr)
		}, args, implicit_args, { &vs_args_res.resources.get_resources(), (fs_resources ? &fs_resources->get_resources() : nullptr) })) {
			log_error("failed to encode shader arguments in \"$\" + \"$\"", vertex_shader->info->name,
					  (fragment_shader ? fragment_shader->info->name : "<no-fragment-shader>"));
			
			vertex_shader->release_exec_instance(vs_acq_args_res.index());
			if (fragment_shader) {
				fragment_shader->release_exec_instance(fs_acq_args_res.index());
			}
			for (const auto& printf_buffer_rsrc : printf_buffer_rsrcs) {
				if (printf_buffer_rsrc.first) {
					ctx->release_soft_printf_buffer(*dev, printf_buffer_rsrc);
				}
			}
			return false;
		}
		
		auto has_vs_resources = vs_args_res.resources.has_resources();
		if (!draw_indexed_entries.empty()) {
			// add all index buffers to the vertex shader resources
			has_vs_resources = true;
			auto& resources = vs_args_res.resources.get_resources();
			for (const auto& draw_entry : draw_indexed_entries) {
				const metal4_buffer* mtl_buffer = draw_entry.index_buffer->get_underlying_metal4_buffer_safe();
				if (!mtl_buffer->is_heap_allocated()) {
					resources.add_resource(mtl_buffer->get_metal_buffer());
				}
			}
		}
		if (has_vs_resources) {
			vs_args_res.resources.update_and_commit();
			[cmd_buffer.cmd_buffer useResidencySet:vs_args_res.resources.residency_set];
		}
		
		const auto has_fs_resources = (fs_resources ? fs_resources->has_resources() : false);
		if (has_fs_resources) {
			fs_resources->update_and_commit();
			[cmd_buffer.cmd_buffer useResidencySet:fs_resources->residency_set];
		}
		
		[encoder setArgumentTable:vs_args_res.arg_table atStages:MTLRenderStageVertex];
		if (fragment_shader) {
			[encoder setArgumentTable:fs_arg_table atStages:MTLRenderStageFragment];
		}
		
		// add completion handler to evaluate printf buffers on completion
		if (is_vs_soft_printf || is_fs_soft_printf) {
			mtl_queue.add_completion_handler(cmd_buffer, [printf_buffer_rsrcs, ctx, dev] {
				auto internal_dev_queue = ctx->get_device_default_queue(*dev);
				for (const auto& printf_buffer_rsrc : printf_buffer_rsrcs) {
					auto cpu_printf_buffer = std::make_unique<uint32_t[]>(printf_buffer_size / 4);
					printf_buffer_rsrc.first->read(*internal_dev_queue, cpu_printf_buffer.get());
					handle_printf_buffer(std::span { cpu_printf_buffer.get(), printf_buffer_size / 4 });
					ctx->release_soft_printf_buffer(*dev, printf_buffer_rsrc);
				}
			});
		}
		
		mtl_queue.add_completion_handler(cmd_buffer, [vertex_shader_ptr = vertex_shader,
													  vs_res_slot_idx = vs_acq_args_res.index(),
													  fragment_shader_ptr = fragment_shader,
													  fs_res_slot_idx = (fragment_shader ? fs_acq_args_res.index() : decltype(fs_acq_args_res)::invalid_slot_idx)] {
			vertex_shader_ptr->release_exec_instance(vs_res_slot_idx);
			if (fragment_shader_ptr) {
				fragment_shader_ptr->release_exec_instance(fs_res_slot_idx);
			}
		});
	}
	return true;
}

void metal4_shader::draw(id <MTL4RenderCommandEncoder> encoder, const PRIMITIVE& primitive,
						 const std::span<const graphics_renderer::multi_draw_entry> draw_entries) const {
	const auto mtl_primitve = metal4_pipeline::metal_primitive_type_from_primitive(primitive);
	for (const auto& entry : draw_entries) {
		[encoder drawPrimitives:mtl_primitve
					vertexStart:entry.first_vertex
					vertexCount:entry.vertex_count
				  instanceCount:entry.instance_count
				   baseInstance:entry.first_instance];
	}
}

void metal4_shader::draw(id <MTL4RenderCommandEncoder> encoder, const PRIMITIVE& primitive,
						 const std::span<const graphics_renderer::multi_draw_indexed_entry> draw_indexed_entries) const {
	@autoreleasepool {
		const auto mtl_primitve = metal4_pipeline::metal_primitive_type_from_primitive(primitive);
		for (const auto& entry : draw_indexed_entries) {
			const auto mtl_index_type = metal4_pipeline::metal_index_type_from_index_type(entry.index_type);
			const auto index_size = index_type_size(entry.index_type);
			[encoder drawIndexedPrimitives:mtl_primitve
								indexCount:entry.index_count
								 indexType:mtl_index_type
							   indexBuffer:(entry.index_buffer->get_underlying_metal4_buffer_safe()->get_metal_buffer().gpuAddress +
											entry.first_index * index_size)
						 indexBufferLength:(entry.index_count * index_size)
							 instanceCount:entry.instance_count
								baseVertex:entry.vertex_offset
							  baseInstance:entry.first_instance];
		}
	}
}

} // namespace fl

#endif
