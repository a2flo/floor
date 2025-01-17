/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2025 Florian Ziesche
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
#include <floor/compute/vulkan/vulkan_args.hpp>
#include <floor/compute/vulkan/vulkan_encoder.hpp>
#include <floor/graphics/vulkan/vulkan_pipeline.hpp>

using namespace llvm_toolchain;

vulkan_shader::vulkan_shader(kernel_map_type&& kernels_) : vulkan_kernel(""sv, std::move(kernels_)) {
}

void vulkan_shader::execute(const compute_queue& cqueue floor_unused,
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

vector<VkImageMemoryBarrier2> vulkan_shader::draw(const compute_queue& cqueue,
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
		return {};
	}
	
	const auto& vk_dev = (const vulkan_device&)cqueue.get_device();
	
	// create command buffer ("encoder") for this kernel execution
	bool encoder_success = false;
	const vector<const vulkan_kernel_entry*> shader_entries {
		vertex_shader, fragment_shader
	};
	auto encoder = create_encoder(cqueue, &cmd_buffer, pipeline, pipeline_layout,
								  shader_entries, nullptr, encoder_success);
	if (!encoder_success) {
		log_error("failed to create vulkan encoder / command buffer for shader \"$\"",
				  vertex_shader->info->name);
		return {};
	}
	
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
			auto printf_buffer = allocate_printf_buffer(cqueue);
			initialize_printf_buffer(cqueue, *printf_buffer);
			printf_buffers.emplace_back(printf_buffer);
			implicit_args.emplace_back(printf_buffer);
		}
	}
	
	// acquire shader descriptor sets/buffers and constant buffers, or set dummy ones if shader stage doesn't exist
	bool vs_has_descriptors = false;
	if (vertex_shader->desc_buffer.desc_buffer_container) {
		encoder->acquired_descriptor_buffers.emplace_back(vertex_shader->desc_buffer.desc_buffer_container->acquire_descriptor_buffer());
		vs_has_descriptors = true;
	}
	if (vs_has_descriptors && vertex_shader->constant_buffers) {
		encoder->acquired_constant_buffers.emplace_back(vertex_shader->constant_buffers->acquire());
		encoder->constant_buffer_mappings.emplace_back(vertex_shader->constant_buffer_mappings[encoder->acquired_constant_buffers.back().second]);
		encoder->constant_buffer_wrappers.emplace_back(vulkan_args::constant_buffer_wrapper_t {
			&vertex_shader->constant_buffer_info,
			encoder->acquired_constant_buffers.back().first,
			{ (uint8_t*)encoder->constant_buffer_mappings.back(), encoder->acquired_constant_buffers.back().first->get_size() }
		});
	} else {
		encoder->acquired_constant_buffers.emplace_back(pair<compute_buffer*, uint32_t> { nullptr, ~0u }); // add dummy
		encoder->constant_buffer_mappings.emplace_back(nullptr);
		encoder->constant_buffer_wrappers.emplace_back(vulkan_args::constant_buffer_wrapper_t {});
	}
	
	bool fs_has_descriptors = false;
	if (fragment_shader != nullptr && fragment_shader->desc_buffer.desc_buffer_container) {
		encoder->acquired_descriptor_buffers.emplace_back(fragment_shader->desc_buffer.desc_buffer_container->acquire_descriptor_buffer());
		fs_has_descriptors = true;
	}
	if (fragment_shader != nullptr && fs_has_descriptors && fragment_shader->constant_buffers) {
		encoder->acquired_constant_buffers.emplace_back(fragment_shader->constant_buffers->acquire());
		encoder->constant_buffer_mappings.emplace_back(fragment_shader->constant_buffer_mappings[encoder->acquired_constant_buffers.back().second]);
		encoder->constant_buffer_wrappers.emplace_back(vulkan_args::constant_buffer_wrapper_t {
			&fragment_shader->constant_buffer_info,
			encoder->acquired_constant_buffers.back().first,
			{ (uint8_t*)encoder->constant_buffer_mappings.back(), encoder->acquired_constant_buffers.back().first->get_size() }
		});
	} else {
		encoder->acquired_constant_buffers.emplace_back(pair<compute_buffer*, uint32_t> { nullptr, ~0u }); // add dummy
		encoder->constant_buffer_mappings.emplace_back(nullptr);
		encoder->constant_buffer_wrappers.emplace_back(vulkan_args::constant_buffer_wrapper_t {});
	}

	assert(encoder->constant_buffer_wrappers.size() == 2);

	// set and handle arguments
	vulkan_args::transition_info_t transition_info {};
	if (!set_and_handle_arguments(true, *encoder, shader_entries, args, implicit_args, transition_info)) {
		return {};
	}
	// NOTE for shader executions / rendering, we can't create a pipeline barrier for all image transitions in the same cmd buffer we're rendering in
	// -> this will instead be done in a separate command buffer in the caller (vulkan_renderer)
	
	// set/write/update descriptors
	const VkBindDescriptorBufferEmbeddedSamplersInfoEXT bind_embedded_info {
		.sType = VK_STRUCTURE_TYPE_BIND_DESCRIPTOR_BUFFER_EMBEDDED_SAMPLERS_INFO_EXT,
		.pNext = nullptr,
		.stageFlags = VK_SHADER_STAGE_ALL_GRAPHICS,
		.layout = encoder->pipeline_layout,
		.set = 0 /* always set #0 */,
	};
	vkCmdBindDescriptorBufferEmbeddedSamplers2EXT(encoder->cmd_buffer.cmd_buffer, &bind_embedded_info);
	
	if (!encoder->acquired_descriptor_buffers.empty() || !encoder->argument_buffers.empty()) {
		// setup + bind descriptor buffers
		const auto desc_buf_count = uint32_t(encoder->acquired_descriptor_buffers.size()) + uint32_t(encoder->argument_buffers.size());
		vector<VkDescriptorBufferBindingInfoEXT> desc_buf_bindings;
		desc_buf_bindings.resize(desc_buf_count);
		vector<VkBufferUsageFlags2CreateInfo> desc_buf_bindings_usage;
		desc_buf_bindings_usage.resize(desc_buf_count);
		size_t desc_buf_binding_idx = 0;
		
		for (const auto& acq_desc_buffer : encoder->acquired_descriptor_buffers) {
			const auto& desc_buffer = *(const vulkan_buffer*)acq_desc_buffer.desc_buffer;
			desc_buf_bindings_usage[desc_buf_binding_idx] = {
				.sType = VK_STRUCTURE_TYPE_BUFFER_USAGE_FLAGS_2_CREATE_INFO,
				.pNext = nullptr,
				.usage = desc_buffer.get_vulkan_buffer_usage(),
			};
			desc_buf_bindings[desc_buf_binding_idx] = {
				.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_BUFFER_BINDING_INFO_EXT,
				.pNext = &desc_buf_bindings_usage[desc_buf_binding_idx],
				.address = desc_buffer.get_vulkan_buffer_device_address(),
				.usage = VK_LEGACY_USAGE_FLAGS_WORKAROUND(desc_buffer.get_vulkan_buffer_usage()),
			};
			++desc_buf_binding_idx;
		}
		
		for (const auto& arg_buffer : encoder->argument_buffers) {
			desc_buf_bindings_usage[desc_buf_binding_idx] = {
				.sType = VK_STRUCTURE_TYPE_BUFFER_USAGE_FLAGS_2_CREATE_INFO,
				.pNext = nullptr,
				.usage = arg_buffer.second->get_vulkan_buffer_usage(),
			};
			desc_buf_bindings[desc_buf_binding_idx] = {
				.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_BUFFER_BINDING_INFO_EXT,
				.pNext = &desc_buf_bindings_usage[desc_buf_binding_idx],
				.address = arg_buffer.second->get_vulkan_buffer_device_address(),
				.usage = VK_LEGACY_USAGE_FLAGS_WORKAROUND(arg_buffer.second->get_vulkan_buffer_usage()),
			};
			++desc_buf_binding_idx;
		}
		
		vkCmdBindDescriptorBuffersEXT(encoder->cmd_buffer.cmd_buffer, uint32_t(desc_buf_bindings.size()), desc_buf_bindings.data());
		
		// set fixed descriptor buffers (set #1 is the vertex shader, set #2 is the fragment shader)
		// NOTE: these may be optional
		static constexpr const uint32_t buffer_indices[2] { 0, 1 };
		static constexpr const VkDeviceSize offsets [2] { 0, 0 };
		const uint32_t start_set = (vertex_shader->desc_buffer.desc_buffer_container ? 1 : 2);
		const uint32_t set_count = ((vertex_shader->desc_buffer.desc_buffer_container ? 1 : 0) +
									(fragment_shader && fragment_shader->desc_buffer.desc_buffer_container ? 1 : 0));
		if (set_count > 0) {
			const VkSetDescriptorBufferOffsetsInfoEXT set_desc_buffer_offsets_info {
				.sType = VK_STRUCTURE_TYPE_SET_DESCRIPTOR_BUFFER_OFFSETS_INFO_EXT,
				.pNext = nullptr,
				.stageFlags = VK_SHADER_STAGE_ALL_GRAPHICS,
				.layout = encoder->pipeline_layout,
				.firstSet = start_set,
				.setCount = set_count,
				.pBufferIndices = &buffer_indices[0],
				.pOffsets = &offsets[0],
			};
			vkCmdSetDescriptorBufferOffsets2EXT(encoder->cmd_buffer.cmd_buffer, &set_desc_buffer_offsets_info);
		}
		
		// bind argument buffers if there are any
		// NOTE: descriptor set range is [5, 8] for vertex shaders and [9, 12] for fragment shaders
		vector<uint32_t> arg_buf_vs_buf_indices;
		vector<uint32_t> arg_buf_fs_buf_indices;
		uint32_t arg_buf_vs_set_count = 0, arg_buf_fs_set_count = 0;
		uint32_t desc_buf_index = set_count;
		for (const auto& arg_buffer : encoder->argument_buffers) {
			assert(arg_buffer.first <= 1u);
			if (arg_buffer.first == 0) {
				++arg_buf_vs_set_count;
				arg_buf_vs_buf_indices.emplace_back(desc_buf_index++);
			} else if (arg_buffer.first == 1) {
				++arg_buf_fs_set_count;
				arg_buf_fs_buf_indices.emplace_back(desc_buf_index++);
			}
		}
		const vector<VkDeviceSize> arg_buf_offsets(std::max(arg_buf_vs_set_count, arg_buf_fs_set_count), 0); // always 0 for all
		if (arg_buf_vs_set_count > 0) {
			const VkSetDescriptorBufferOffsetsInfoEXT set_desc_buffer_offsets_info {
				.sType = VK_STRUCTURE_TYPE_SET_DESCRIPTOR_BUFFER_OFFSETS_INFO_EXT,
				.pNext = nullptr,
				.stageFlags = VK_SHADER_STAGE_ALL_GRAPHICS,
				.layout = encoder->pipeline_layout,
				.firstSet = vulkan_pipeline::argument_buffer_vs_start_set,
				.setCount = arg_buf_vs_set_count,
				.pBufferIndices = arg_buf_vs_buf_indices.data(),
				.pOffsets = arg_buf_offsets.data(),
			};
			vkCmdSetDescriptorBufferOffsets2EXT(encoder->cmd_buffer.cmd_buffer, &set_desc_buffer_offsets_info);
		}
		if (arg_buf_fs_set_count > 0) {
			const VkSetDescriptorBufferOffsetsInfoEXT set_desc_buffer_offsets_info {
				.sType = VK_STRUCTURE_TYPE_SET_DESCRIPTOR_BUFFER_OFFSETS_INFO_EXT,
				.pNext = nullptr,
				.stageFlags = VK_SHADER_STAGE_ALL_GRAPHICS,
				.layout = encoder->pipeline_layout,
				.firstSet = vulkan_pipeline::argument_buffer_fs_start_set,
				.setCount = arg_buf_fs_set_count,
				.pBufferIndices = arg_buf_fs_buf_indices.data(),
				.pOffsets = arg_buf_offsets.data(),
			};
			vkCmdSetDescriptorBufferOffsets2EXT(encoder->cmd_buffer.cmd_buffer, &set_desc_buffer_offsets_info);
		}
	}
	
	if (draw_entries != nullptr) {
		for (const auto& entry : *draw_entries) {
			vkCmdDraw(encoder->cmd_buffer.cmd_buffer, entry.vertex_count, entry.instance_count,
					  entry.first_vertex, entry.first_instance);
		}
	}
	if (draw_indexed_entries != nullptr) {
		for (const auto& entry : *draw_indexed_entries) {
			const auto vk_idx_buffer = entry.index_buffer->get_underlying_vulkan_buffer_safe()->get_vulkan_buffer();
			vkCmdBindIndexBuffer2KHR(encoder->cmd_buffer.cmd_buffer, vk_idx_buffer, 0, VK_WHOLE_SIZE, VK_INDEX_TYPE_UINT32);
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
				handle_printf_buffer(span { cpu_printf_buffer.get(), printf_buffer_size / 4 });
			}
		});
	}

	// attach constant buffers to queue+cmd_buffer so that they will be destroyed once this is completed
	if (!encoder->constant_buffers.empty()) {
		vk_queue.add_retained_buffers(cmd_buffer, encoder->constant_buffers);
	}
	
	// release acquired descriptor sets again after completion
	if (vs_has_descriptors || fs_has_descriptors) {
		auto acq_desc_buffers_local = make_shared<decltype(encoder->acquired_descriptor_buffers)>(std::move(encoder->acquired_descriptor_buffers));
		auto acq_const_buffers_local = make_shared<decltype(encoder->acquired_constant_buffers)>(std::move(encoder->acquired_constant_buffers));
		vk_queue.add_completion_handler(cmd_buffer, [acq_desc_buffers = std::move(acq_desc_buffers_local),
													 acq_const_buffers = std::move(acq_const_buffers_local),
													 kernel_entries = encoder->entries]() {
			acq_desc_buffers->clear();
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
	
	return transition_info.barriers;
}

#endif
