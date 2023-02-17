/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2023 Florian Ziesche
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
#include <floor/graphics/vulkan/vulkan_pipeline.hpp>

using namespace llvm_toolchain;

vulkan_shader::vulkan_shader(kernel_map_type&& kernels_) : vulkan_kernel(std::move(kernels_)) {
}

void vulkan_shader::execute(const compute_queue& cqueue floor_unused,
							const bool& is_cooperative floor_unused,
							const bool& wait_until_completion floor_unused,
							const uint32_t& dim floor_unused,
							const uint3& global_work_size floor_unused,
							const uint3& local_work_size floor_unused,
							const vector<compute_kernel_arg>& args floor_unused,
							const vector<const compute_fence*>& wait_fences floor_unused,
							const vector<const compute_fence*>& signal_fences floor_unused,
							const char* debug_label floor_unused,
							kernel_completion_handler_f&& completion_handler floor_unused) const {
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
	const auto& vk_ctx = *(vulkan_compute*)vk_dev.context;
	
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
			auto printf_buffer = allocate_printf_buffer(cqueue);
			initialize_printf_buffer(cqueue, *printf_buffer);
			printf_buffers.emplace_back(printf_buffer);
			implicit_args.emplace_back(printf_buffer);
		}
	}
	
	// acquire shader descriptor sets/buffers and constant buffers, or set dummy ones if shader stage doesn't exist
	bool vs_has_descriptors = false;
	if (vk_dev.descriptor_buffer_support) {
		if (vertex_shader->desc_buffer.desc_buffer_container) {
			encoder->acquired_descriptor_buffers.emplace_back(vertex_shader->desc_buffer.desc_buffer_container->acquire_descriptor_buffer());
			vs_has_descriptors = true;
		}
	} else {
		if (vertex_shader->desc_legacy.desc_set_container) {
			encoder->legacy.acquired_descriptor_sets.emplace_back(vertex_shader->desc_legacy.desc_set_container->acquire_descriptor_set());
			vs_has_descriptors = true;
		} else {
			encoder->legacy.acquired_descriptor_sets.emplace_back(descriptor_set_instance_t {}); // add dummy
		}
	}
	if (vs_has_descriptors && vertex_shader->constant_buffers) {
		encoder->acquired_constant_buffers.emplace_back(vertex_shader->constant_buffers->acquire());
		encoder->constant_buffer_mappings.emplace_back(vertex_shader->constant_buffer_mappings[encoder->acquired_constant_buffers.back().second]);
	} else {
		encoder->acquired_constant_buffers.emplace_back(pair<compute_buffer*, uint32_t> { nullptr, ~0u }); // add dummy
		encoder->constant_buffer_mappings.emplace_back(nullptr);
	}
	
	bool fs_has_descriptors = false;
	if (vk_dev.descriptor_buffer_support) {
		if (fragment_shader != nullptr && fragment_shader->desc_buffer.desc_buffer_container) {
			encoder->acquired_descriptor_buffers.emplace_back(fragment_shader->desc_buffer.desc_buffer_container->acquire_descriptor_buffer());
			fs_has_descriptors = true;
		}
	} else {
		if (fragment_shader != nullptr && fragment_shader->desc_legacy.desc_set_container) {
			encoder->legacy.acquired_descriptor_sets.emplace_back(fragment_shader->desc_legacy.desc_set_container->acquire_descriptor_set());
			fs_has_descriptors = true;
		} else {
			encoder->legacy.acquired_descriptor_sets.emplace_back(descriptor_set_instance_t {}); // add dummy
		}
	}
	if (fragment_shader != nullptr && fs_has_descriptors && fragment_shader->constant_buffers) {
		encoder->acquired_constant_buffers.emplace_back(fragment_shader->constant_buffers->acquire());
		encoder->constant_buffer_mappings.emplace_back(fragment_shader->constant_buffer_mappings[encoder->acquired_constant_buffers.back().second]);
	} else {
		encoder->acquired_constant_buffers.emplace_back(pair<compute_buffer*, uint32_t> { nullptr, ~0u }); // add dummy
		encoder->constant_buffer_mappings.emplace_back(nullptr);
	}
	
	// set and handle arguments
	idx_handler idx;
	if (vk_dev.descriptor_buffer_support) {
		if (!set_and_handle_arguments(*encoder, shader_entries, idx, args, implicit_args)) {
			return;
		}
	} else {
		if (!set_and_handle_arguments_legacy(*encoder, shader_entries, idx, args, implicit_args)) {
			return;
		}
	}
	
	// set/write/update descriptors
	bool legacy_has_vs_desc = false, legacy_has_fs_desc = false;
	if (vk_dev.descriptor_buffer_support) {
		// this always exists
		vk_ctx.vulkan_cmd_bind_descriptor_buffer_embedded_samplers(encoder->cmd_buffer.cmd_buffer,
																   VK_PIPELINE_BIND_POINT_GRAPHICS,
																   encoder->pipeline_layout, 0 /* always set #0 */);
		
		if (!encoder->acquired_descriptor_buffers.empty() || !encoder->argument_buffers.empty()) {
			// setup + bind descriptor buffers
			const auto desc_buf_count = uint32_t(encoder->acquired_descriptor_buffers.size()) + uint32_t(encoder->argument_buffers.size());
			vector<VkDescriptorBufferBindingInfoEXT> desc_buf_bindings;
			desc_buf_bindings.reserve(desc_buf_count);
			
			for (const auto& acq_desc_buffer : encoder->acquired_descriptor_buffers) {
				const auto& desc_buffer = *(const vulkan_buffer*)acq_desc_buffer.desc_buffer;
				desc_buf_bindings.emplace_back(VkDescriptorBufferBindingInfoEXT {
					.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_BUFFER_BINDING_INFO_EXT,
					.pNext = nullptr,
					.address = desc_buffer.get_vulkan_buffer_device_address(),
					.usage = desc_buffer.get_vulkan_buffer_usage(),
				});
			}
			
			for (const auto& arg_buffer : encoder->argument_buffers) {
				desc_buf_bindings.emplace_back(VkDescriptorBufferBindingInfoEXT {
					.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_BUFFER_BINDING_INFO_EXT,
					.pNext = nullptr,
					.address = arg_buffer.second->get_vulkan_buffer_device_address(),
					.usage = arg_buffer.second->get_vulkan_buffer_usage(),
				});
			}
			
			vk_ctx.vulkan_cmd_bind_descriptor_buffers(encoder->cmd_buffer.cmd_buffer,
													  uint32_t(desc_buf_bindings.size()),
													  desc_buf_bindings.data());
			
			// set fixed descriptor buffers (set #1 is the vertex shader, set #2 is the fragment shader)
			// NOTE: these may be optional
			static constexpr const uint32_t buffer_indices[2] { 0, 1 };
			static constexpr const VkDeviceSize offsets [2] { 0, 0 };
			const uint32_t start_set = (vertex_shader->desc_buffer.desc_buffer_container ? 1 : 2);
			const uint32_t set_count = ((vertex_shader->desc_buffer.desc_buffer_container ? 1 : 0) +
										(fragment_shader && fragment_shader->desc_buffer.desc_buffer_container ? 1 : 0));
			if (set_count > 0) {
				vk_ctx.vulkan_cmd_set_descriptor_buffer_offsets(encoder->cmd_buffer.cmd_buffer,
																VK_PIPELINE_BIND_POINT_GRAPHICS,
																encoder->pipeline_layout,
																start_set, set_count,
																&buffer_indices[0], &offsets[0]);
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
				vk_ctx.vulkan_cmd_set_descriptor_buffer_offsets(encoder->cmd_buffer.cmd_buffer,
																VK_PIPELINE_BIND_POINT_GRAPHICS,
																encoder->pipeline_layout,
																vulkan_pipeline::argument_buffer_vs_start_set, arg_buf_vs_set_count,
																arg_buf_vs_buf_indices.data(), arg_buf_offsets.data());
			}
			if (arg_buf_fs_set_count > 0) {
				vk_ctx.vulkan_cmd_set_descriptor_buffer_offsets(encoder->cmd_buffer.cmd_buffer,
																VK_PIPELINE_BIND_POINT_GRAPHICS,
																encoder->pipeline_layout,
																vulkan_pipeline::argument_buffer_fs_start_set, arg_buf_fs_set_count,
																arg_buf_fs_buf_indices.data(), arg_buf_offsets.data());
			}
		}
	} else {
		vkUpdateDescriptorSets(vk_dev.device,
							   (uint32_t)encoder->legacy.write_descs.size(), encoder->legacy.write_descs.data(),
							   // never copy (bad for performance)
							   0, nullptr);
		
		// final desc set binding after all parameters have been updated/set
		// note that we need to take care of the situation where the vertex shader doesn't have a desc set,
		// but the fragment shader does -> binding discontiguous sets is not directly possible
		legacy_has_vs_desc = (vertex_shader->desc_legacy.desc_set_container != nullptr);
		legacy_has_fs_desc = (fragment_shader != nullptr && fragment_shader->desc_legacy.desc_set_container != nullptr);
		const bool discontiguous = (!legacy_has_vs_desc && legacy_has_fs_desc);
		
		array<VkDescriptorSet, 3> desc_sets {{
			vk_dev.legacy_fixed_sampler_desc_set,
			// NOTE: these may be nullptr / dummy ones (see above)
			encoder->legacy.acquired_descriptor_sets[0].desc_set,
			encoder->legacy.acquired_descriptor_sets[1].desc_set,
		}};
		
		// either binds everything or just the fixed sampler set
		vkCmdBindDescriptorSets(encoder->cmd_buffer.cmd_buffer,
								VK_PIPELINE_BIND_POINT_GRAPHICS,
								encoder->pipeline_layout,
								0,
								(discontiguous || !legacy_has_vs_desc) ? 1 : (!legacy_has_fs_desc ? 2 : 3),
								desc_sets.data(),
								// don't want to set dyn offsets when only binding the fixed sampler set
								discontiguous || encoder->legacy.dyn_offsets.empty() ? 0 : uint32_t(encoder->legacy.dyn_offsets.size()),
								discontiguous || encoder->legacy.dyn_offsets.empty() ? nullptr : encoder->legacy.dyn_offsets.data());
		
		// bind fs set
		if (discontiguous) {
			vkCmdBindDescriptorSets(encoder->cmd_buffer.cmd_buffer,
									VK_PIPELINE_BIND_POINT_GRAPHICS,
									encoder->pipeline_layout,
									2,
									1,
									&desc_sets[2],
									encoder->legacy.dyn_offsets.empty() ? 0 : (uint32_t)encoder->legacy.dyn_offsets.size(),
									encoder->legacy.dyn_offsets.empty() ? nullptr : encoder->legacy.dyn_offsets.data());
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
	if (vk_dev.descriptor_buffer_support) {
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
	} else {
		if (legacy_has_vs_desc || legacy_has_fs_desc) {
			auto acq_desc_sets_local = make_shared<decltype(encoder->legacy.acquired_descriptor_sets)>(std::move(encoder->legacy.acquired_descriptor_sets));
			auto acq_const_buffers_local = make_shared<decltype(encoder->acquired_constant_buffers)>(std::move(encoder->acquired_constant_buffers));
			vk_queue.add_completion_handler(cmd_buffer, [acq_desc_sets = std::move(acq_desc_sets_local),
														 acq_const_buffers = std::move(acq_const_buffers_local),
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
	}
	
#if defined(FLOOR_DEBUG)
	((const vulkan_compute*)vk_dev.context)->vulkan_end_cmd_debug_label(encoder->cmd_buffer.cmd_buffer);
#endif
}

#endif
