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
		log_error("failed to create vulkan encoder / command buffer for shader \"$\"",
				  vertex_shader->info->name);
		return;
	}
	
	// for shader execution: leave images in general layout if necessary, since we can't have arbitrary pipeline barriers for image layout transitions,
	// because that would require passes to have self-dependencies that would need to be correctl set up (with the current design, this isn't possible)
	encoder->allow_generic_layout = true;
	
	// create implicit args
	vector<compute_kernel_arg> implicit_args;
	
	// create + init printf buffers if soft-printf is used
	vector<shared_ptr<compute_buffer>> printf_buffers;
	const auto is_vs_soft_printf = has_flag<FUNCTION_FLAGS::USES_SOFT_PRINTF>(vertex_shader->info->flags);
	const auto is_fs_soft_printf = (fragment_shader != nullptr &&
									has_flag<FUNCTION_FLAGS::USES_SOFT_PRINTF>(fragment_shader->info->flags));
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
	
	// acquire shader descriptor sets and constant buffers, or set dummy ones if shader stage doesn't exist
	if (vertex_shader->desc_set_container) {
		encoder->acquired_descriptor_sets.emplace_back(vertex_shader->desc_set_container->acquire_descriptor_set());
	} else {
		encoder->acquired_descriptor_sets.emplace_back(descriptor_set_instance_t {}); // add dummy
	}
	if (vertex_shader->desc_set_container && vertex_shader->constant_buffers) {
		encoder->acquired_constant_buffers.emplace_back(vertex_shader->constant_buffers->acquire());
		encoder->constant_buffer_mappings.emplace_back(vertex_shader->constant_buffer_mappings[encoder->acquired_constant_buffers.back().second]);
	} else {
		encoder->acquired_constant_buffers.emplace_back(pair<compute_buffer*, uint32_t> { nullptr, ~0u }); // add dummy
		encoder->constant_buffer_mappings.emplace_back(nullptr);
	}
	
	if (fragment_shader != nullptr && fragment_shader->desc_set_container) {
		encoder->acquired_descriptor_sets.emplace_back(fragment_shader->desc_set_container->acquire_descriptor_set());
	} else {
		encoder->acquired_descriptor_sets.emplace_back(descriptor_set_instance_t {}); // add dummy
	}
	if (fragment_shader != nullptr && fragment_shader->desc_set_container && fragment_shader->constant_buffers) {
		encoder->acquired_constant_buffers.emplace_back(fragment_shader->constant_buffers->acquire());
		encoder->constant_buffer_mappings.emplace_back(fragment_shader->constant_buffer_mappings[encoder->acquired_constant_buffers.back().second]);
	} else {
		encoder->acquired_constant_buffers.emplace_back(pair<compute_buffer*, uint32_t> { nullptr, ~0u }); // add dummy
		encoder->constant_buffer_mappings.emplace_back(nullptr);
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
	const bool has_vs_desc = (vertex_shader->desc_set_container != nullptr);
	const bool has_fs_desc = (fragment_shader != nullptr && fragment_shader->desc_set_container != nullptr);
	const bool discontiguous = (!has_vs_desc && has_fs_desc);
	
	array<VkDescriptorSet, 3> desc_sets {{
		vk_dev.fixed_sampler_desc_set,
		// NOTE: these may be nullptr / dummy ones (see above)
		encoder->acquired_descriptor_sets[0].desc_set,
		encoder->acquired_descriptor_sets[1].desc_set,
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
								&desc_sets[2],
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
			VkBuffer vk_idx_buffer { nullptr };
			if (has_flag<COMPUTE_MEMORY_FLAG::VULKAN_SHARING>(entry.index_buffer->get_flags())) {
				auto vk_buffer = entry.index_buffer->get_shared_vulkan_buffer();
				if (vk_buffer == nullptr) {
					vk_buffer = (const vulkan_buffer*)entry.index_buffer;
#if defined(FLOOR_DEBUG)
					if (auto test_cast_vk_buffer = dynamic_cast<const vulkan_buffer*>(entry.index_buffer); !test_cast_vk_buffer) {
						log_error("specified index buffer \"$\" is neither a Vulkan buffer nor a shared Vulkan buffer",
								  entry.index_buffer->get_debug_label());
						continue;
					}
#endif
				}
				vk_idx_buffer = vk_buffer->get_vulkan_buffer();
			} else {
				vk_idx_buffer = ((const vulkan_buffer*)entry.index_buffer)->get_vulkan_buffer();
			}
			
			vkCmdBindIndexBuffer(encoder->cmd_buffer.cmd_buffer, vk_idx_buffer, 0, VK_INDEX_TYPE_UINT32);
			vkCmdDrawIndexed(encoder->cmd_buffer.cmd_buffer, entry.index_count, entry.instance_count, entry.first_index,
							 entry.vertex_offset, entry.first_instance);
		}
	}
	
	const auto& vk_queue = (const vulkan_queue&)cqueue;
	
	// add completion handler to evaluate printf buffers on completion
	if (is_vs_soft_printf || is_fs_soft_printf) {
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
		vk_queue.add_retained_buffers(cmd_buffer, encoder->constant_buffers);
	}
	
	// release acquired descriptor sets again after completion
	if (has_vs_desc || has_fs_desc) {
		auto acq_desc_sets_local = make_shared<decltype(encoder->acquired_descriptor_sets)>(move(encoder->acquired_descriptor_sets));
		auto acq_const_buffers_local = make_shared<decltype(encoder->acquired_constant_buffers)>(move(encoder->acquired_constant_buffers));
		vk_queue.add_completion_handler(cmd_buffer, [acq_desc_sets = move(acq_desc_sets_local),
													 acq_const_buffers = move(acq_const_buffers_local),
													 kernel_entries = encoder->entries]() {
			acq_desc_sets->clear();
			for (size_t entry_idx = 0, entry_count = kernel_entries.size(); entry_idx < entry_count; ++entry_idx) {
				auto& acq_const_buffer = (*acq_const_buffers)[entry_idx];
				if (acq_const_buffer.first != nullptr) {
					kernel_entries[entry_idx]->constant_buffers->release(acq_const_buffer);
				}
			}
		});
	}
	
#if defined(FLOOR_DEBUG)
	((const vulkan_compute*)vk_dev.context)->vulkan_end_cmd_debug_label(encoder->cmd_buffer.cmd_buffer);
#endif
}

#endif
