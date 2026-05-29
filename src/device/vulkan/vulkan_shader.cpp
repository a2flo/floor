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

#include <floor/device/vulkan/vulkan_shader.hpp>

#if !defined(FLOOR_NO_VULKAN)
#include <floor/core/essentials.hpp>
#include <floor/device/device_context.hpp>
#include <floor/device/toolchain.hpp>
#include <floor/device/soft_printf.hpp>
#include "internal/vulkan_headers.hpp"
#include <floor/device/vulkan/vulkan_context.hpp>
#include <floor/device/vulkan/vulkan_device.hpp>
#include <floor/device/vulkan/vulkan_queue.hpp>
#include "internal/vulkan_args.hpp"
#include "internal/vulkan_debug.hpp"
#include "internal/vulkan_descriptor_set.hpp"
#include "internal/vulkan_encoder.hpp"
#include "internal/vulkan_function_entry.hpp"
#include "internal/vulkan_conversion.hpp"
#include <floor/device/vulkan/vulkan_pipeline.hpp>

namespace fl {
using namespace std::literals;
using namespace toolchain;

vulkan_shader::vulkan_shader(function_map_type&& functions_) : vulkan_function(""sv, std::move(functions_)) {
}

void vulkan_shader::execute(const device_queue& cqueue floor_unused,
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

template <bool is_mesh_shader>
std::vector<VkImageMemoryBarrier2> vulkan_shader::draw_internal(const device_queue& cqueue,
																const vulkan_command_buffer& cmd_buffer,
																vulkan_encoder& encoder,
																std::vector<vulkan_function_entry*>&& shader_entries,
																vulkan_function_entry* vertex_shader,
																vulkan_function_entry* task_shader,
																vulkan_function_entry* mesh_shader,
																vulkan_function_entry* fragment_shader,
																const std::span<const graphics_renderer::multi_draw_entry> draw_entries,
																const std::span<const graphics_renderer::multi_draw_indexed_entry> draw_indexed_entries,
																const graphics_renderer::mesh_draw_entry* draw_mesh_entry,
																const std::vector<device_function_arg>& args) const {
	const auto& vk_dev = (const vulkan_device&)cqueue.get_device();
	
	// create implicit args
	std::vector<device_function_arg> implicit_args;
	
	// create + init printf buffers if soft-printf is used
	std::vector<std::shared_ptr<device_buffer>> printf_buffers;
	uint32_t printf_buffer_count = 0u;
	if constexpr (!is_mesh_shader) {
		if (has_flag<FUNCTION_FLAGS::USES_SOFT_PRINTF>(vertex_shader->info->flags)) {
			++printf_buffer_count;
		}
		if (fragment_shader && has_flag<FUNCTION_FLAGS::USES_SOFT_PRINTF>(fragment_shader->info->flags)) {
			++printf_buffer_count;
		}
	} else {
		if (task_shader && has_flag<FUNCTION_FLAGS::USES_SOFT_PRINTF>(task_shader->info->flags)) {
			++printf_buffer_count;
		}
		if (has_flag<FUNCTION_FLAGS::USES_SOFT_PRINTF>(mesh_shader->info->flags)) {
			++printf_buffer_count;
		}
		if (fragment_shader && has_flag<FUNCTION_FLAGS::USES_SOFT_PRINTF>(fragment_shader->info->flags)) {
			++printf_buffer_count;
		}
	}
	if (printf_buffer_count > 0u) {
		for (uint32_t i = 0; i < printf_buffer_count; ++i) {
			auto printf_buffer = allocate_printf_buffer(cqueue);
			initialize_printf_buffer(cqueue, *printf_buffer);
			printf_buffers.emplace_back(printf_buffer);
			implicit_args.emplace_back(printf_buffer);
		}
	}
	
	// acquire shader descriptor sets/buffers and constant buffers, or set dummy ones if shader stage doesn't exist
	bool vs_has_descriptors = false, ts_has_descriptors = false, ms_has_descriptors = false;
	const auto add_const_buffers = [&encoder](vulkan_function_entry& shader) {
		auto acq_const_buf = shader.constant_buffers.acquire_resource_no_auto_release();
		assert(acq_const_buf.res != nullptr);
		encoder.acquired_constant_buffers.emplace_back(*acq_const_buf.res, acq_const_buf.index());
		encoder.constant_buffer_mappings.emplace_back(shader.constant_buffer_mappings[acq_const_buf.index()]);
		encoder.constant_buffer_wrappers.emplace_back(vulkan_args::constant_buffer_wrapper_t {
			.constant_buffer_info = &shader.constant_buffer_info,
			.constant_buffer_storage = *acq_const_buf.res,
			.constant_buffer_mapping = { (uint8_t*)encoder.constant_buffer_mappings.back(), (*acq_const_buf.res)->get_size() }
		});
	};
	const auto add_dummy_const_buffers = [&encoder] {
		encoder.acquired_constant_buffers.emplace_back(std::pair<device_buffer*, uint32_t> { nullptr, ~0u });
		encoder.constant_buffer_mappings.emplace_back(nullptr);
		encoder.constant_buffer_wrappers.emplace_back(vulkan_args::constant_buffer_wrapper_t {});
	};
	if constexpr (!is_mesh_shader) {
		if (vertex_shader->desc_buffer.desc_buffer_container) {
			encoder.acquired_descriptor_buffers.emplace_back(vertex_shader->desc_buffer.desc_buffer_container->acquire_descriptor_buffer());
			vs_has_descriptors = true;
		}
		if (vs_has_descriptors && vertex_shader->has_constant_buffers()) {
			add_const_buffers(*vertex_shader);
		} else {
			add_dummy_const_buffers();
		}
	} else {
		if (task_shader && task_shader->desc_buffer.desc_buffer_container) {
			encoder.acquired_descriptor_buffers.emplace_back(task_shader->desc_buffer.desc_buffer_container->acquire_descriptor_buffer());
			ts_has_descriptors = true;
		}
		if (task_shader && ts_has_descriptors && task_shader->has_constant_buffers()) {
			add_const_buffers(*task_shader);
		} else {
			add_dummy_const_buffers();
		}
		
		if (mesh_shader->desc_buffer.desc_buffer_container) {
			encoder.acquired_descriptor_buffers.emplace_back(mesh_shader->desc_buffer.desc_buffer_container->acquire_descriptor_buffer());
			ms_has_descriptors = true;
		}
		if (ms_has_descriptors && mesh_shader->has_constant_buffers()) {
			add_const_buffers(*mesh_shader);
		} else {
			add_dummy_const_buffers();
		}
	}
	
	bool fs_has_descriptors = false;
	if (fragment_shader && fragment_shader->desc_buffer.desc_buffer_container) {
		encoder.acquired_descriptor_buffers.emplace_back(fragment_shader->desc_buffer.desc_buffer_container->acquire_descriptor_buffer());
		fs_has_descriptors = true;
	}
	if (fragment_shader && fs_has_descriptors && fragment_shader->has_constant_buffers()) {
		add_const_buffers(*fragment_shader);
	} else {
		add_dummy_const_buffers();
	}
	
	bool has_any_descriptors = false;
	if constexpr (!is_mesh_shader) {
		has_any_descriptors = (vs_has_descriptors || fs_has_descriptors);
	} else {
		has_any_descriptors = (ts_has_descriptors || ms_has_descriptors || fs_has_descriptors);
	}
	
	assert(encoder.constant_buffer_wrappers.size() == (!is_mesh_shader ? 2 : 3));
	
	// set and handle arguments
	vulkan_args::transition_info_t transition_info {};
	if (!set_and_handle_arguments(true, encoder, shader_entries, args, implicit_args, transition_info)) {
		return {};
	}
	// NOTE for shader executions / rendering, we can't create a pipeline barrier for all image transitions in the same cmd buffer we're rendering in
	// -> this will instead be done in a separate command buffer in the caller (vulkan_renderer)
	
	// set/write/update descriptors
	const VkBindDescriptorBufferEmbeddedSamplersInfoEXT bind_embedded_info {
		.sType = VK_STRUCTURE_TYPE_BIND_DESCRIPTOR_BUFFER_EMBEDDED_SAMPLERS_INFO_EXT,
		.pNext = nullptr,
		.stageFlags = vk_dev.shader_stage_all_graphics,
		.layout = encoder.pipeline_layout,
		.set = 0 /* always set #0 */,
	};
	vkCmdBindDescriptorBufferEmbeddedSamplers2EXT(cmd_buffer.cmd_buffer, &bind_embedded_info);
	
	if (!encoder.acquired_descriptor_buffers.empty() || !encoder.argument_buffers.empty()) {
		// setup + bind descriptor buffers
		const auto desc_buf_count = uint32_t(encoder.acquired_descriptor_buffers.size() + encoder.argument_buffers.size());
		std::vector<VkDescriptorBufferBindingInfoEXT> desc_buf_bindings;
		desc_buf_bindings.resize(desc_buf_count);
		std::vector<VkBufferUsageFlags2CreateInfo> desc_buf_bindings_usage;
		desc_buf_bindings_usage.resize(desc_buf_count);
		size_t desc_buf_binding_idx = 0;
		
		for (const auto& acq_desc_buffer : encoder.acquired_descriptor_buffers) {
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
				.usage = 0,
			};
			++desc_buf_binding_idx;
		}
		
		for (const auto& arg_buffer : encoder.argument_buffers) {
			desc_buf_bindings_usage[desc_buf_binding_idx] = {
				.sType = VK_STRUCTURE_TYPE_BUFFER_USAGE_FLAGS_2_CREATE_INFO,
				.pNext = nullptr,
				.usage = arg_buffer.second->get_vulkan_buffer_usage(),
			};
			desc_buf_bindings[desc_buf_binding_idx] = {
				.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_BUFFER_BINDING_INFO_EXT,
				.pNext = &desc_buf_bindings_usage[desc_buf_binding_idx],
				.address = arg_buffer.second->get_vulkan_buffer_device_address(),
				.usage = 0,
			};
			++desc_buf_binding_idx;
		}
		
		vkCmdBindDescriptorBuffersEXT(cmd_buffer.cmd_buffer, uint32_t(desc_buf_bindings.size()), desc_buf_bindings.data());
		
		// set fixed descriptor buffers
		//  * VS/FS: set #1 is the vertex shader, set #2 is the fragment shader)
		//  * TS/MS/FS: set #1 is task shader, set #2 is the fragment shader, set #3 is the mesh shader
		// NOTE: these may be optional
		if constexpr (!is_mesh_shader) {
			static constexpr const uint32_t buffer_indices[2] { 0, 1 };
			static constexpr const VkDeviceSize offsets [2] { 0, 0 };
			const uint32_t start_set = (vertex_shader->desc_buffer.desc_buffer_container ? 1 : 2);
			const uint32_t set_count = ((vertex_shader->desc_buffer.desc_buffer_container ? 1 : 0) +
										(fragment_shader && fragment_shader->desc_buffer.desc_buffer_container ? 1 : 0));
			if (set_count > 0) {
				const VkSetDescriptorBufferOffsetsInfoEXT set_desc_buffer_offsets_info {
					.sType = VK_STRUCTURE_TYPE_SET_DESCRIPTOR_BUFFER_OFFSETS_INFO_EXT,
					.pNext = nullptr,
					.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
					.layout = encoder.pipeline_layout,
					.firstSet = start_set,
					.setCount = set_count,
					.pBufferIndices = &buffer_indices[0],
					.pOffsets = &offsets[0],
				};
				vkCmdSetDescriptorBufferOffsets2EXT(cmd_buffer.cmd_buffer, &set_desc_buffer_offsets_info);
			}
			
			// bind argument buffers if there are any
			// NOTE: see vulkan_pipeline.hpp for layout/indices
			std::vector<uint32_t> arg_buf_vs_buf_indices;
			std::vector<uint32_t> arg_buf_fs_buf_indices;
			uint32_t arg_buf_vs_set_count = 0, arg_buf_fs_set_count = 0;
			uint32_t desc_buf_index = set_count;
			for (const auto& arg_buffer : encoder.argument_buffers) {
				assert(arg_buffer.first <= 1u);
				if (arg_buffer.first == 0) {
					++arg_buf_vs_set_count;
					arg_buf_vs_buf_indices.emplace_back(desc_buf_index++);
				} else if (arg_buffer.first == 1) {
					++arg_buf_fs_set_count;
					arg_buf_fs_buf_indices.emplace_back(desc_buf_index++);
				}
			}
			const std::vector<VkDeviceSize> arg_buf_offsets(std::max(arg_buf_vs_set_count, arg_buf_fs_set_count), 0); // always 0 for all
			if (arg_buf_vs_set_count > 0) {
				const VkSetDescriptorBufferOffsetsInfoEXT set_desc_buffer_offsets_info {
					.sType = VK_STRUCTURE_TYPE_SET_DESCRIPTOR_BUFFER_OFFSETS_INFO_EXT,
					.pNext = nullptr,
					.stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
					.layout = encoder.pipeline_layout,
					.firstSet = vulkan_pipeline::argument_buffer_vs_start_set,
					.setCount = arg_buf_vs_set_count,
					.pBufferIndices = arg_buf_vs_buf_indices.data(),
					.pOffsets = arg_buf_offsets.data(),
				};
				vkCmdSetDescriptorBufferOffsets2EXT(cmd_buffer.cmd_buffer, &set_desc_buffer_offsets_info);
			}
			if (arg_buf_fs_set_count > 0) {
				const auto is_fs_low_desc_count = (fragment_shader != nullptr &&
												   has_flag<FUNCTION_FLAGS::VULKAN_LOW_DS>(fragment_shader->info->flags));
				const VkSetDescriptorBufferOffsetsInfoEXT set_desc_buffer_offsets_info {
					.sType = VK_STRUCTURE_TYPE_SET_DESCRIPTOR_BUFFER_OFFSETS_INFO_EXT,
					.pNext = nullptr,
					.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
					.layout = encoder.pipeline_layout,
					.firstSet = (is_fs_low_desc_count ?
								 vulkan_pipeline::argument_buffer_fs_start_set_low :
								 vulkan_pipeline::argument_buffer_fs_start_set_high),
					.setCount = arg_buf_fs_set_count,
					.pBufferIndices = arg_buf_fs_buf_indices.data(),
					.pOffsets = arg_buf_offsets.data(),
				};
				vkCmdSetDescriptorBufferOffsets2EXT(cmd_buffer.cmd_buffer, &set_desc_buffer_offsets_info);
			}
		} else {
			// as sets must be contiguous, but any of them may be optional, just set them all individually rather than making this a mess
			static constexpr const VkDeviceSize offset { 0 };
			uint32_t set_count = 0u;
			if (task_shader->desc_buffer.desc_buffer_container) {
				const VkSetDescriptorBufferOffsetsInfoEXT set_desc_buffer_offsets_info {
					.sType = VK_STRUCTURE_TYPE_SET_DESCRIPTOR_BUFFER_OFFSETS_INFO_EXT,
					.pNext = nullptr,
					.stageFlags = VK_SHADER_STAGE_TASK_BIT_EXT,
					.layout = encoder.pipeline_layout,
					.firstSet = 1,
					.setCount = 1,
					.pBufferIndices = &set_count,
					.pOffsets = &offset,
				};
				vkCmdSetDescriptorBufferOffsets2EXT(cmd_buffer.cmd_buffer, &set_desc_buffer_offsets_info);
				++set_count;
			}
			// NOTE: while MS set is 3 and FS set is 2, we encode everything in TS->MS->FS order
			if (mesh_shader->desc_buffer.desc_buffer_container) {
				const VkSetDescriptorBufferOffsetsInfoEXT set_desc_buffer_offsets_info {
					.sType = VK_STRUCTURE_TYPE_SET_DESCRIPTOR_BUFFER_OFFSETS_INFO_EXT,
					.pNext = nullptr,
					.stageFlags = VK_SHADER_STAGE_MESH_BIT_EXT,
					.layout = encoder.pipeline_layout,
					.firstSet = 3,
					.setCount = 1,
					.pBufferIndices = &set_count,
					.pOffsets = &offset,
				};
				vkCmdSetDescriptorBufferOffsets2EXT(cmd_buffer.cmd_buffer, &set_desc_buffer_offsets_info);
				++set_count;
			}
			if (fragment_shader->desc_buffer.desc_buffer_container) {
				const VkSetDescriptorBufferOffsetsInfoEXT set_desc_buffer_offsets_info {
					.sType = VK_STRUCTURE_TYPE_SET_DESCRIPTOR_BUFFER_OFFSETS_INFO_EXT,
					.pNext = nullptr,
					.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
					.layout = encoder.pipeline_layout,
					.firstSet = 2,
					.setCount = 1,
					.pBufferIndices = &set_count,
					.pOffsets = &offset,
				};
				vkCmdSetDescriptorBufferOffsets2EXT(cmd_buffer.cmd_buffer, &set_desc_buffer_offsets_info);
				++set_count;
			}
			
			// bind argument buffers if there are any
			// NOTE: see vulkan_pipeline.hpp for layout/indices
			std::vector<uint32_t> arg_buf_ts_buf_indices;
			std::vector<uint32_t> arg_buf_ms_buf_indices;
			std::vector<uint32_t> arg_buf_fs_buf_indices;
			uint32_t arg_buf_ts_set_count = 0, arg_buf_ms_set_count = 0, arg_buf_fs_set_count = 0;
			uint32_t desc_buf_index = set_count;
			for (const auto& arg_buffer : encoder.argument_buffers) {
				assert(arg_buffer.first <= 2u);
				if (arg_buffer.first == 0) {
					++arg_buf_ts_set_count;
					arg_buf_ts_buf_indices.emplace_back(desc_buf_index++);
				} else if (arg_buffer.first == 1) {
					++arg_buf_ms_set_count;
					arg_buf_ms_buf_indices.emplace_back(desc_buf_index++);
				} else if (arg_buffer.first == 2) {
					++arg_buf_fs_set_count;
					arg_buf_fs_buf_indices.emplace_back(desc_buf_index++);
				}
			}
			// always 0 for all
			const std::vector<VkDeviceSize> arg_buf_offsets(std::max({ arg_buf_ts_set_count, arg_buf_ms_set_count, arg_buf_fs_set_count }), 0);
			if (arg_buf_ts_set_count > 0) {
				[[maybe_unused]] const auto is_ts_low_desc_count = (task_shader != nullptr &&
																	has_flag<FUNCTION_FLAGS::VULKAN_LOW_DS>(task_shader->info->flags));
				assert(!is_ts_low_desc_count && "low descriptor set count not supported for mesh shading");
				const VkSetDescriptorBufferOffsetsInfoEXT set_desc_buffer_offsets_info {
					.sType = VK_STRUCTURE_TYPE_SET_DESCRIPTOR_BUFFER_OFFSETS_INFO_EXT,
					.pNext = nullptr,
					.stageFlags = VK_SHADER_STAGE_TASK_BIT_EXT,
					.layout = encoder.pipeline_layout,
					.firstSet = vulkan_pipeline::argument_buffer_ts_start_set_high,
					.setCount = arg_buf_ts_set_count,
					.pBufferIndices = arg_buf_ts_buf_indices.data(),
					.pOffsets = arg_buf_offsets.data(),
				};
				vkCmdSetDescriptorBufferOffsets2EXT(cmd_buffer.cmd_buffer, &set_desc_buffer_offsets_info);
			}
			if (arg_buf_ms_set_count > 0) {
				[[maybe_unused]] const auto is_ms_low_desc_count = has_flag<FUNCTION_FLAGS::VULKAN_LOW_DS>(mesh_shader->info->flags);
				assert(!is_ms_low_desc_count && "low descriptor set count not supported for mesh shading");
				const VkSetDescriptorBufferOffsetsInfoEXT set_desc_buffer_offsets_info {
					.sType = VK_STRUCTURE_TYPE_SET_DESCRIPTOR_BUFFER_OFFSETS_INFO_EXT,
					.pNext = nullptr,
					.stageFlags = VK_SHADER_STAGE_MESH_BIT_EXT,
					.layout = encoder.pipeline_layout,
					.firstSet = vulkan_pipeline::argument_buffer_ms_start_set_high,
					.setCount = arg_buf_ms_set_count,
					.pBufferIndices = arg_buf_ms_buf_indices.data(),
					.pOffsets = arg_buf_offsets.data(),
				};
				vkCmdSetDescriptorBufferOffsets2EXT(cmd_buffer.cmd_buffer, &set_desc_buffer_offsets_info);
			}
			if (arg_buf_fs_set_count > 0) {
				[[maybe_unused]] const auto is_fs_low_desc_count = (fragment_shader != nullptr &&
																	has_flag<FUNCTION_FLAGS::VULKAN_LOW_DS>(fragment_shader->info->flags));
				assert(!is_fs_low_desc_count && "low descriptor set count not supported for mesh shading");
				const VkSetDescriptorBufferOffsetsInfoEXT set_desc_buffer_offsets_info {
					.sType = VK_STRUCTURE_TYPE_SET_DESCRIPTOR_BUFFER_OFFSETS_INFO_EXT,
					.pNext = nullptr,
					.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
					.layout = encoder.pipeline_layout,
					.firstSet = vulkan_pipeline::argument_buffer_fs_start_set_high,
					.setCount = arg_buf_fs_set_count,
					.pBufferIndices = arg_buf_fs_buf_indices.data(),
					.pOffsets = arg_buf_offsets.data(),
				};
				vkCmdSetDescriptorBufferOffsets2EXT(cmd_buffer.cmd_buffer, &set_desc_buffer_offsets_info);
			}
		}
	}
	
	if constexpr (!is_mesh_shader) {
		assert(draw_entries.empty() != draw_indexed_entries.empty()); // only one must be active
		if (!draw_entries.empty()) {
			for (const auto& entry : draw_entries) {
				vkCmdDraw(cmd_buffer.cmd_buffer, entry.vertex_count, entry.instance_count,
						  entry.first_vertex, entry.first_instance);
			}
		} else if (!draw_indexed_entries.empty()) {
			for (const auto& entry : draw_indexed_entries) {
				const auto vk_idx_buffer = entry.index_buffer->get_underlying_vulkan_buffer_safe()->get_vulkan_buffer();
				const auto vk_idx_type = vulkan_index_type_from_index_type(entry.index_type);
				if (vk_dev.vulkan_version >= VULKAN_VERSION::VULKAN_1_4) {
					vkCmdBindIndexBuffer2(cmd_buffer.cmd_buffer, vk_idx_buffer, 0, VK_WHOLE_SIZE, vk_idx_type);
				} else {
					vkCmdBindIndexBuffer2KHR(cmd_buffer.cmd_buffer, vk_idx_buffer, 0, VK_WHOLE_SIZE, vk_idx_type);
				}
				vkCmdDrawIndexed(cmd_buffer.cmd_buffer, entry.index_count, entry.instance_count, entry.first_index,
								 entry.vertex_offset, entry.first_instance);
			}
		}
	} else {
		assert(draw_mesh_entry);
		vkCmdDrawMeshTasksEXT(cmd_buffer.cmd_buffer,
							  draw_mesh_entry->work_group_count.x,
							  draw_mesh_entry->work_group_count.y,
							  draw_mesh_entry->work_group_count.z);
	}
	
	const auto& vk_queue = (const vulkan_queue&)cqueue;
	
	// add completion handler to evaluate printf buffers on completion
	if (printf_buffer_count > 0u) {
		vk_queue.add_completion_handler(cmd_buffer, [printf_buffers, dev = &vk_dev]() {
			const auto default_queue = ((const vulkan_context*)dev->context)->get_device_default_queue(*dev);
			for (const auto& printf_buffer : printf_buffers) {
				auto cpu_printf_buffer = std::make_unique<uint32_t[]>(printf_buffer_size / 4);
				printf_buffer->read(*default_queue, cpu_printf_buffer.get());
				handle_printf_buffer(std::span { cpu_printf_buffer.get(), printf_buffer_size / 4 });
			}
		});
	}
	
	// release acquired descriptor sets again after completion
	if (has_any_descriptors) {
		auto acq_desc_buffers_local = std::make_shared<decltype(encoder.acquired_descriptor_buffers)>(std::move(encoder.acquired_descriptor_buffers));
		auto acq_const_buffers_local = std::make_shared<decltype(encoder.acquired_constant_buffers)>(std::move(encoder.acquired_constant_buffers));
		vk_queue.add_completion_handler(cmd_buffer, [acq_desc_buffers = std::move(acq_desc_buffers_local),
													 acq_const_buffers = std::move(acq_const_buffers_local),
													 function_entries = encoder.entries]() {
			acq_desc_buffers->clear();
			for (size_t entry_idx = 0, entry_count = function_entries.size(); entry_idx < entry_count; ++entry_idx) {
				auto& acq_const_buffer = (*acq_const_buffers)[entry_idx];
				if (acq_const_buffer.first != nullptr) {
					function_entries[entry_idx]->constant_buffers.release_resource(acq_const_buffer.second);
				}
			}
		});
	}
	
#if defined(FLOOR_DEBUG)
	vulkan_end_cmd_debug_label(cmd_buffer.cmd_buffer);
#endif
	
	return transition_info.barriers;
}


std::vector<VkImageMemoryBarrier2> vulkan_shader::draw(const device_queue& cqueue,
													   const vulkan_command_buffer& cmd_buffer,
													   const VkPipeline pipeline,
													   const VkPipelineLayout pipeline_layout,
													   vulkan_function_entry* vertex_shader,
													   vulkan_function_entry* fragment_shader,
													   const std::span<const graphics_renderer::multi_draw_entry> draw_entries,
													   const std::span<const graphics_renderer::multi_draw_indexed_entry> draw_indexed_entries,
													   const std::vector<device_function_arg>& args) const {
	if (!vertex_shader) {
		log_error("must specify a vertex shader!");
		return {};
	}
	
	// create command buffer ("encoder") for this function execution
	bool encoder_success = false;
	std::vector<vulkan_function_entry*> shader_entries {
		vertex_shader, fragment_shader
	};
	auto encoder = create_encoder(cqueue, cmd_buffer, pipeline, pipeline_layout,
								  shader_entries, nullptr, encoder_success);
	if (!encoder_success) {
		log_error("failed to create Vulkan encoder / command buffer for shader \"$\"",
				  vertex_shader->info->name);
		return {};
	}
	
	return draw_internal<false>(cqueue, cmd_buffer, *encoder, std::move(shader_entries),
								vertex_shader, nullptr, nullptr, fragment_shader,
								draw_entries, draw_indexed_entries, nullptr, args);
}

std::vector<VkImageMemoryBarrier2> vulkan_shader::draw(const device_queue& cqueue,
													   const vulkan_command_buffer& cmd_buffer,
													   const VkPipeline pipeline,
													   const VkPipelineLayout pipeline_layout,
													   vulkan_function_entry* task_shader,
													   vulkan_function_entry* mesh_shader,
													   vulkan_function_entry* fragment_shader,
													   const graphics_renderer::mesh_draw_entry& draw_entry,
													   const std::vector<device_function_arg>& args) const {
	if (!mesh_shader) {
		log_error("must specify a mesh shader!");
		return {};
	}
	
	// create command buffer ("encoder") for this function execution
	bool encoder_success = false;
	std::vector<vulkan_function_entry*> shader_entries {
		task_shader, mesh_shader, fragment_shader
	};
	auto encoder = create_encoder(cqueue, cmd_buffer, pipeline, pipeline_layout,
								  shader_entries, nullptr, encoder_success);
	if (!encoder_success) {
		log_error("failed to create Vulkan encoder / command buffer for shader \"$\"",
				  mesh_shader->info->name);
		return {};
	}
	
	return draw_internal<true>(cqueue, cmd_buffer, *encoder, std::move(shader_entries),
							   nullptr, task_shader, mesh_shader, fragment_shader,
							   {}, {}, &draw_entry, args);
}

} // namespace fl

#endif
