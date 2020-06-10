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

#include <floor/graphics/vulkan/vulkan_shader.hpp>

#if !defined(FLOOR_NO_VULKAN)
#include <floor/core/essentials.hpp>
#include <floor/compute/compute_context.hpp>
#include <floor/compute/llvm_toolchain.hpp>
#include <floor/compute/soft_printf.hpp>
#include <floor/compute/vulkan/vulkan_compute.hpp>
#include <floor/compute/vulkan/vulkan_device.hpp>
#include <floor/compute/vulkan/vulkan_queue.hpp>
#include <floor/compute/vulkan/vulkan_encoder.hpp>

using namespace llvm_toolchain;

vulkan_shader::vulkan_shader(kernel_map_type&& kernels_) : vulkan_kernel(move(kernels_)) {
}

void vulkan_shader::execute(const compute_queue& cqueue floor_unused,
							const bool& is_cooperative floor_unused,
							const uint32_t& dim floor_unused,
							const uint3& global_work_size floor_unused,
							const uint3& local_work_size floor_unused,
							const vector<compute_kernel_arg>& args floor_unused) const {
	log_error("executing a shader is not supported!");
}

void vulkan_shader::draw(const compute_queue& cqueue,
						 const vulkan_command_buffer& cmd_buffer,
						 const VkPipeline pipeline,
						 const VkPipelineLayout pipeline_layout,
						 const vulkan_kernel_entry* vertex_shader,
						 const vulkan_kernel_entry* fragment_shader,
						 const vector<graphics_renderer::multi_draw_entry>* draw_entries,
						 const vector<graphics_renderer::multi_draw_indexed_entry>* draw_indexed_entries,
						 const vector<compute_kernel_arg>& args) const {
	if (vertex_shader == nullptr) {
		log_error("must specify a vertex shader!");
		return;
	}
	
	const auto& vk_dev = (const vulkan_device&)cqueue.get_device();
	
	// create command buffer ("encoder") for this kernel execution
	bool encoder_success = false;
	const vector<const vulkan_kernel_entry*> shader_entries {
		vertex_shader, fragment_shader
	};
	auto encoder = create_encoder(cqueue, &cmd_buffer, pipeline, pipeline_layout,
								  shader_entries, encoder_success);
	if (!encoder_success) {
		log_error("failed to create vulkan encoder / command buffer for shader \"%s\"",
				  vertex_shader->info->name);
		return;
	}
	
	// create implicit args
	vector<compute_kernel_arg> implicit_args;
	
	// create + init printf buffers if soft-printf is used
	vector<shared_ptr<compute_buffer>> printf_buffers;
	const auto is_vs_soft_printf = function_info::has_flag<function_info::FUNCTION_FLAGS::USES_SOFT_PRINTF>(vertex_shader->info->flags);
	const auto is_fs_soft_printf = (fragment_shader != nullptr &&
									function_info::has_flag<function_info::FUNCTION_FLAGS::USES_SOFT_PRINTF>(fragment_shader->info->flags));
	if (is_vs_soft_printf || is_fs_soft_printf) {
		const uint32_t printf_buffer_count = (is_vs_soft_printf ? 1u : 0u) + (is_fs_soft_printf ? 1u : 0u);
		for (uint32_t i = 0; i < printf_buffer_count; ++i) {
			auto printf_buffer = cqueue.get_device().context->create_buffer(cqueue, printf_buffer_size);
			printf_buffer->set_debug_label("printf_buffer");
			printf_buffer->write_from(uint2 { printf_buffer_header_size, printf_buffer_size }, cqueue);
			printf_buffers.emplace_back(printf_buffer);
			implicit_args.emplace_back(printf_buffer);
		}
	}
	
	// set and handle arguments
	idx_handler idx;
	if (!set_and_handle_arguments(*encoder, shader_entries, idx, args, implicit_args)) {
		return;
	}
	
	// set/write/update descriptors
	vkUpdateDescriptorSets(vk_dev.device,
						   (uint32_t)encoder->write_descs.size(), encoder->write_descs.data(),
						   // never copy (bad for performance)
						   0, nullptr);
	
	// final desc set binding after all parameters have been updated/set
	// note that we need to take care of the situation where the vertex shader doesn't have a desc set,
	// but the fragment shader does -> binding discontiguous sets is not directly possible
	const bool has_vs_desc = (vertex_shader->desc_set != nullptr);
	const bool has_fs_desc = (fragment_shader != nullptr && fragment_shader->desc_set != nullptr);
	const bool discontiguous = (!has_vs_desc && has_fs_desc);
	
	const array<VkDescriptorSet, 3> desc_sets {{
		vk_dev.fixed_sampler_desc_set,
		vertex_shader->desc_set,
		has_fs_desc ? fragment_shader->desc_set : nullptr,
	}};
	// either binds everything or just the fixed sampler set
	vkCmdBindDescriptorSets(encoder->cmd_buffer.cmd_buffer,
							VK_PIPELINE_BIND_POINT_GRAPHICS,
							encoder->pipeline_layout,
							0,
							(discontiguous || !has_vs_desc) ? 1 : (!has_fs_desc ? 2 : 3),
							desc_sets.data(),
							// don't want to set dyn offsets when only binding the fixed sampler set
							discontiguous || encoder->dyn_offsets.empty() ? 0 : uint32_t(encoder->dyn_offsets.size()),
							discontiguous || encoder->dyn_offsets.empty() ? nullptr : encoder->dyn_offsets.data());
	
	// bind fs set
	if (discontiguous) {
		vkCmdBindDescriptorSets(encoder->cmd_buffer.cmd_buffer,
								VK_PIPELINE_BIND_POINT_GRAPHICS,
								encoder->pipeline_layout,
								2,
								1,
								&fragment_shader->desc_set,
								encoder->dyn_offsets.empty() ? 0 : (uint32_t)encoder->dyn_offsets.size(),
								encoder->dyn_offsets.empty() ? nullptr : encoder->dyn_offsets.data());
	}
	
	if (draw_entries != nullptr) {
		for (const auto& entry : *draw_entries) {
			vkCmdDraw(encoder->cmd_buffer.cmd_buffer, entry.vertex_count, entry.instance_count,
					  entry.first_vertex, entry.first_instance);
		}
	}
	if (draw_indexed_entries != nullptr) {
		for (const auto& entry : *draw_indexed_entries) {
			vkCmdBindIndexBuffer(encoder->cmd_buffer.cmd_buffer, ((vulkan_buffer*)entry.index_buffer)->get_vulkan_buffer(),
								 0, VK_INDEX_TYPE_UINT32);
			vkCmdDrawIndexed(encoder->cmd_buffer.cmd_buffer, entry.index_count, entry.instance_count, entry.first_index,
							 entry.vertex_offset, entry.first_instance);
		}
	}
	
	// add completion handler to evaluate printf buffers on completion
	if (is_vs_soft_printf || is_fs_soft_printf) {
		const auto& vk_queue = (const vulkan_queue&)cqueue;
		vk_queue.add_completion_handler(cmd_buffer, [printf_buffers, dev = &vk_dev]() {
			const auto default_queue = ((const vulkan_compute*)dev->context)->get_device_default_queue(*dev);
			for (const auto& printf_buffer : printf_buffers) {
				auto cpu_printf_buffer = make_unique<uint32_t[]>(printf_buffer_size / 4);
				printf_buffer->read(*default_queue, cpu_printf_buffer.get());
				handle_printf_buffer(cpu_printf_buffer);
			}
		});
	}

	// attach constant buffers to queue+cmd_buffer so that they will be destroyed once this is completed
	if (!encoder->constant_buffers.empty()) {
		const auto& vk_queue = (const vulkan_queue&)cqueue;
		vk_queue.add_retained_buffers(cmd_buffer, encoder->constant_buffers);
	}
}

#endif
