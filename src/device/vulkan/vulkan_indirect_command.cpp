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

#include <floor/core/essentials.hpp>

#if !defined(FLOOR_NO_VULKAN)
#include <floor/core/logger.hpp>
#include <floor/device/device_context.hpp>
#include "internal/vulkan_headers.hpp"
#include <floor/device/vulkan/vulkan_context.hpp>
#include <floor/device/vulkan/vulkan_indirect_command.hpp>
#include <floor/device/vulkan/vulkan_device.hpp>
#include <floor/device/vulkan/vulkan_buffer.hpp>
#include <floor/device/vulkan/vulkan_function.hpp>
#include "internal/vulkan_args.hpp"
#include "internal/vulkan_descriptor_set.hpp"
#include "internal/vulkan_function_entry.hpp"
#include "internal/vulkan_debug.hpp"
#include "internal/vulkan_conversion.hpp"
#include <floor/device/vulkan/vulkan_pipeline.hpp>
#include <floor/device/vulkan/vulkan_shader.hpp>
#include <floor/device/soft_printf.hpp>

namespace fl {

vulkan_indirect_command_pipeline::vulkan_indirect_command_pipeline(const indirect_command_description& desc_,
																   const std::vector<std::unique_ptr<device>>& devices) :
indirect_command_pipeline(desc_) {
	if (!valid) {
		return;
	}
	if (devices.empty()) {
		log_error("no devices specified in indirect command pipeline \"$\"",
				  desc.debug_label);
		valid = false;
		return;
	}
	[[maybe_unused]] bool tessellation_support = true;
	for (const auto& dev : devices) {
		if (!dev->argument_buffer_support) {
			log_error("specified device \"$\" has no support for argument buffers in indirect command pipeline \"$\"",
					  dev->name, desc.debug_label);
			valid = false;
			return;
		}
		// NOTE: only need to check for general indirect command support here (if it's supported, both compute and render commands are supported as well)
		if (!dev->indirect_command_support) {
			log_error("specified device \"$\" has no support for indirect commands in indirect command pipeline \"$\"",
					  dev->name, desc.debug_label);
			valid = false;
			return;
		}
		assert(dev->indirect_compute_command_support && dev->indirect_render_command_support);
		if (!dev->tessellation_support) {
			tessellation_support = false;
		}
	}
	// NOTE: we don't have a technical limit for this in Vulkan -> keep it at the same limit as Metal for consistency
	if (!desc.ignore_max_max_command_count_limit && desc.max_command_count > 16384u) {
		log_error("max supported command count by Vulkan is 16384, in indirect command pipeline \"$\"",
				  desc.debug_label);
		valid = false;
		return;
	}
	
	for (const auto& dev : devices) {
		const auto& vk_ctx = (const vulkan_context&)*dev->context;
		const auto& vk_dev = (const vulkan_device&)*dev;
		const auto& vk_dev_ptr = vk_dev.device;
		
		vulkan_pipeline_entry entry;
		entry.vk_dev = vk_dev_ptr;
		
		// when this is a compute pipeline and the device supports a separate compute-only queue family: create for both ALL and COMPUTE queues
		const auto needs_compute_only = (desc.command_type == indirect_command_description::COMMAND_TYPE::COMPUTE &&
										 vk_dev.all_queue_family_index != vk_dev.compute_queue_family_index);
		entry.per_queue_data.resize(needs_compute_only ? 2 : 1);
		entry.per_queue_data[0].queue_family_index = vk_dev.all_queue_family_index;
		if (needs_compute_only) {
			entry.per_queue_data[1].queue_family_index = vk_dev.compute_queue_family_index;
		}
		
		const auto err_str = " for device \"" + dev->name + "\" in indirect command pipeline \"" + desc.debug_label + "\"";
		
		for (auto& per_queue_data : entry.per_queue_data) {
			const auto default_queue = (const vulkan_queue*)(per_queue_data.queue_family_index == vk_dev.all_queue_family_index ?
															 dev->context->get_device_default_queue(*dev) :
															 dev->context->get_device_default_compute_queue(*dev));
			
			// create pool
			const VkCommandPoolCreateInfo cmd_pool_info {
				.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
				.pNext = nullptr,
				// not transient/short-lived, but need individual reset
				.flags = (VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT),
				.queueFamilyIndex = default_queue->get_family_index(),
			};
			VK_CALL_RET(vkCreateCommandPool(vk_dev_ptr, &cmd_pool_info, nullptr, &per_queue_data.cmd_pool),
						"failed to create command pool" + err_str)
#if defined(FLOOR_DEBUG)
			set_vulkan_debug_label(vk_dev, VK_OBJECT_TYPE_COMMAND_POOL, uint64_t(per_queue_data.cmd_pool), "icp_cmd_pool");
#endif
			
			// create cmd buffers
			per_queue_data.cmd_buffers.resize(desc.max_command_count);
			const VkCommandBufferAllocateInfo sec_cmd_buffer_info {
				.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
				.pNext = nullptr,
				.commandPool = per_queue_data.cmd_pool,
				// always create secondary
				.level = VK_COMMAND_BUFFER_LEVEL_SECONDARY,
				.commandBufferCount = desc.max_command_count,
			};
			VK_CALL_RET(vkAllocateCommandBuffers(vk_dev_ptr, &sec_cmd_buffer_info, &per_queue_data.cmd_buffers[0]),
						"failed to create secondary command buffers" + err_str)
#if defined(FLOOR_DEBUG)
			uint32_t sec_cmd_buffer_counter = 0;
			for (const auto& cb : per_queue_data.cmd_buffers) {
				set_vulkan_debug_label(vk_dev, VK_OBJECT_TYPE_COMMAND_BUFFER, uint64_t(cb),
									   "icp_cmd_buf_" + std::to_string(sec_cmd_buffer_counter++));
			}
#endif
		}
		
		// allocate memory for per-command parameters
		// NOTE: we assume we only use SSBOs or that compute_buffer_counts_from_functions() was used to compute the buffer count
		// NOTE: each descriptor buffer "sub-set" must be aligned to "descriptor_buffer_offset_alignment"
		entry.per_cmd_size = 0u;
		if (desc.command_type == indirect_command_description::COMMAND_TYPE::COMPUTE) {
			entry.per_cmd_size += const_math::round_next_multiple(vk_dev.desc_buffer_sizes.ssbo * desc.max_kernel_buffer_count,
																  vk_dev.descriptor_buffer_offset_alignment);
		} else {
			entry.per_cmd_size += const_math::round_next_multiple(vk_dev.desc_buffer_sizes.ssbo * desc.max_vertex_buffer_count,
																  vk_dev.descriptor_buffer_offset_alignment);
			entry.per_cmd_size += const_math::round_next_multiple(vk_dev.desc_buffer_sizes.ssbo * desc.max_fragment_buffer_count,
																  vk_dev.descriptor_buffer_offset_alignment);
		}
		
		const auto cmd_params_size = desc.max_command_count * entry.per_cmd_size;
		assert(cmd_params_size > 0);
		
		// need host-visible/host-coherent and descriptor buffer usage flags
		const auto default_queue = (const vulkan_queue*)dev->context->get_device_default_queue(*dev);
		entry.cmd_parameters = vk_ctx.create_buffer(*default_queue, cmd_params_size,
													// NOTE: read-only on the device side (until writable argument buffers are implemented)
													MEMORY_FLAG::READ |
													MEMORY_FLAG::HOST_READ_WRITE |
													MEMORY_FLAG::VULKAN_HOST_COHERENT |
													MEMORY_FLAG::VULKAN_DESCRIPTOR_BUFFER);
#if defined(FLOOR_DEBUG)
		entry.cmd_parameters->set_debug_label("icp_cmd_params_buf");
#endif
		// note that this doesn't have any direct dependency on the queue (type) itself (-> will work for both ALL and COMPUTE queues later on)
		entry.mapped_cmd_parameters = entry.cmd_parameters->map(*default_queue, (MEMORY_MAP_FLAG::WRITE |
																				 MEMORY_MAP_FLAG::BLOCK));
		if (!entry.mapped_cmd_parameters) {
			throw std::runtime_error("failed to map cmd parameters buffer" + err_str);
		}
		
		pipelines.insert_or_assign(dev.get(), std::move(entry));
	}
}

vulkan_indirect_command_pipeline::~vulkan_indirect_command_pipeline() {
}

const vulkan_indirect_command_pipeline::vulkan_pipeline_entry* vulkan_indirect_command_pipeline::get_vulkan_pipeline_entry(const device& dev) const {
	if (const auto iter = pipelines.find(&dev); iter != pipelines.end()) {
		return &iter->second;
	}
	return nullptr;
}

vulkan_indirect_command_pipeline::vulkan_pipeline_entry* vulkan_indirect_command_pipeline::get_vulkan_pipeline_entry(const device& dev) {
	if (const auto iter = pipelines.find(&dev); iter != pipelines.end()) {
		return &iter->second;
	}
	return nullptr;
}

indirect_render_command_encoder& vulkan_indirect_command_pipeline::add_render_command(const device& dev,
																					  const graphics_pipeline& pipeline,
																					  const bool is_multi_view_) {
	if (desc.command_type != indirect_command_description::COMMAND_TYPE::RENDER) {
		throw std::runtime_error("adding render commands to a compute indirect command pipeline is not allowed");
	}
	
	const auto pipeline_entry = get_vulkan_pipeline_entry(dev);
	if (!pipeline_entry) {
		throw std::runtime_error("no pipeline entry for device " + dev.name);
	}
	if (commands.size() >= desc.max_command_count) {
		throw std::runtime_error("already encoded the max amount of commands in indirect command pipeline " + desc.debug_label);
	}
	
	auto render_enc = std::make_unique<vulkan_indirect_render_command_encoder>(*pipeline_entry, uint32_t(commands.size()), dev, pipeline, is_multi_view_);
	auto render_enc_ptr = render_enc.get();
	commands.emplace_back(std::move(render_enc));
	return *render_enc_ptr;
}

indirect_compute_command_encoder& vulkan_indirect_command_pipeline::add_compute_command(const device& dev,
																						const device_function& kernel_obj) {
	if (desc.command_type != indirect_command_description::COMMAND_TYPE::COMPUTE) {
		throw std::runtime_error("adding compute commands to a render indirect command pipeline is not allowed");
	}
	
	const auto pipeline_entry = get_vulkan_pipeline_entry(dev);
	if (!pipeline_entry) {
		throw std::runtime_error("no pipeline entry for device " + dev.name);
	}
	if (commands.size() >= desc.max_command_count) {
		throw std::runtime_error("already encoded the max amount of commands in indirect command pipeline " + desc.debug_label);
	}
	
	auto compute_enc = std::make_unique<vulkan_indirect_compute_command_encoder>(*pipeline_entry, uint32_t(commands.size()), dev, kernel_obj);
	auto compute_enc_ptr = compute_enc.get();
	commands.emplace_back(std::move(compute_enc));
	return *compute_enc_ptr;
}

void vulkan_indirect_command_pipeline::complete(const device& dev) {
	auto pipeline_entry = get_vulkan_pipeline_entry(dev);
	if (!pipeline_entry) {
		throw std::runtime_error("no pipeline entry for device " + dev.name);
	}
	complete_pipeline(dev, *pipeline_entry);
}

void vulkan_indirect_command_pipeline::complete() {
	for (auto&& pipeline : pipelines) {
		complete_pipeline(*pipeline.first, pipeline.second);
	}
}

void vulkan_indirect_command_pipeline::complete_pipeline(const device& dev, vulkan_pipeline_entry& entry) {
	// end all cmd buffers
	for (size_t cmd_idx = 0, cmd_count = commands.size(); cmd_idx < cmd_count; ++cmd_idx) {
		const auto& cmd = commands[cmd_idx];
		if (!cmd || &cmd->get_device() != &dev) {
			continue;
		}
		for (auto& per_queue_data : entry.per_queue_data) {
			auto cmd_buffer = per_queue_data.cmd_buffers[cmd_idx];
			VK_CALL_RET(vkEndCommandBuffer(cmd_buffer),
						"failed to end command buffer #" + std::to_string(cmd_idx) + " for device " + dev.name)
		}
	}
}

void vulkan_indirect_command_pipeline::reset() {
	for (auto&& pipeline : pipelines) {
		// just clear all parameters
		if (pipeline.second.mapped_cmd_parameters && pipeline.second.cmd_parameters && pipeline.second.cmd_parameters->get_size() > 0) {
			memset(pipeline.second.mapped_cmd_parameters, 0, pipeline.second.cmd_parameters->get_size());
		}
	}
	indirect_command_pipeline::reset();
}

std::optional<vulkan_indirect_command_pipeline::command_range_t>
vulkan_indirect_command_pipeline::compute_and_validate_command_range(const uint32_t command_offset,
																	 const uint32_t command_count) const {
	command_range_t range { command_offset, command_count };
	if (command_count == ~0u) {
		range.count = get_command_count();
	}
#if defined(FLOOR_DEBUG)
	{
		const auto cmd_count = get_command_count();
		if (command_offset != 0 || range.count != cmd_count) {
			static std::once_flag flag;
			std::call_once(flag, [] {
				// see below
				log_warn("efficient resource usage declarations when using partial command ranges is not implemented yet");
			});
		}
		if (cmd_count == 0) {
			log_warn("no commands in indirect command pipeline \"$\"", desc.debug_label);
		}
		if (range.offset >= cmd_count) {
			log_error("out-of-bounds command offset $ for indirect command pipeline \"$\"",
					  range.offset, desc.debug_label);
			return {};
		}
		uint32_t sum = 0;
		if (__builtin_uadd_overflow((uint32_t)range.offset, (uint32_t)range.count, &sum)) {
			log_error("command offset $ + command count $ overflow for indirect command pipeline \"$\"",
					  range.offset, range.count, desc.debug_label);
			return {};
		}
		if (sum > cmd_count) {
			log_error("out-of-bounds command count $ for indirect command pipeline \"$\"",
					  range.count, desc.debug_label);
			return {};
		}
	}
#endif
	// post count check, since this might have been modified, but we still want the debug messages
	if (range.count == 0) {
		return {};
	}
	
	return range;
}

vulkan_indirect_command_pipeline::vulkan_pipeline_entry::vulkan_pipeline_entry(vulkan_pipeline_entry&& entry) :
vk_dev(entry.vk_dev), per_queue_data(std::move(entry.per_queue_data)),
cmd_parameters(entry.cmd_parameters), mapped_cmd_parameters(entry.mapped_cmd_parameters), per_cmd_size(entry.per_cmd_size), printf_buffer(entry.printf_buffer) {
	entry.per_queue_data.clear();
	entry.cmd_parameters = nullptr;
	entry.mapped_cmd_parameters = nullptr;
	entry.per_cmd_size = 0;
	entry.printf_buffer = nullptr;
}

vulkan_indirect_command_pipeline::vulkan_pipeline_entry& vulkan_indirect_command_pipeline::vulkan_pipeline_entry::operator=(vulkan_pipeline_entry&& entry) {
	vk_dev = entry.vk_dev;
	per_queue_data = std::move(entry.per_queue_data);
	cmd_parameters = entry.cmd_parameters;
	mapped_cmd_parameters = entry.mapped_cmd_parameters;
	per_cmd_size = entry.per_cmd_size;
	printf_buffer = entry.printf_buffer;
	entry.per_queue_data.clear();
	entry.cmd_parameters = nullptr;
	entry.mapped_cmd_parameters = nullptr;
	entry.per_cmd_size = 0;
	entry.printf_buffer = nullptr;
	return *this;
}

vulkan_indirect_command_pipeline::vulkan_pipeline_entry::~vulkan_pipeline_entry() {
	if (vk_dev) {
		// NOTE: this will implicitly kill all command buffers as well
		for (auto& per_queue_data_ : per_queue_data) {
			if (per_queue_data_.cmd_pool) {
				vkDestroyCommandPool(vk_dev, per_queue_data_.cmd_pool, nullptr);
			}
		}
	}
}

void vulkan_indirect_command_pipeline::vulkan_pipeline_entry::printf_init(const device_queue& dev_queue) const {
	initialize_printf_buffer(dev_queue, *printf_buffer);
}

void vulkan_indirect_command_pipeline::vulkan_pipeline_entry::printf_completion(const device_queue& dev_queue, vulkan_command_buffer cmd_buffer) const {
	auto internal_dev_queue = ((const vulkan_context*)dev_queue.get_device().context)->get_device_default_queue(dev_queue.get_device());
	((const vulkan_queue&)dev_queue).add_completion_handler(cmd_buffer, [this, internal_dev_queue]() {
		auto cpu_printf_buffer = std::make_unique<uint32_t[]>(printf_buffer_size / 4);
		printf_buffer->read(*internal_dev_queue, cpu_printf_buffer.get());
		handle_printf_buffer(std::span { cpu_printf_buffer.get(), printf_buffer_size / 4 });
	});
}

vulkan_indirect_render_command_encoder::vulkan_indirect_render_command_encoder(const vulkan_indirect_command_pipeline::vulkan_pipeline_entry& pipeline_entry_,
																			   const uint32_t command_idx_,
																			   const device& dev_, const graphics_pipeline& pipeline_,
																			   const bool is_multi_view_) :
indirect_render_command_encoder(dev_, pipeline_, is_multi_view_), pipeline_entry(pipeline_entry_), command_idx(command_idx_) {
	const auto& vk_render_pipeline = (const vulkan_pipeline&)pipeline;
	const auto& vk_dev = (const vulkan_device&)dev;
	pipeline_state = vk_render_pipeline.get_vulkan_pipeline_state(vk_dev, is_multi_view,
																  !vk_dev.inherited_viewport_scissor_support /* indirect if !inheriting viewport/scissor */);
	if (!pipeline_state || !pipeline_state->pipeline) {
		throw std::runtime_error("no render pipeline entry exists for device " + vk_dev.name);
	}
	pass = vk_render_pipeline.get_vulkan_pass(is_multi_view);
	if (!pass) {
		throw std::runtime_error("no render pass object exists for device " + vk_dev.name);
	}
	render_pass = pass->get_vulkan_render_pass(vk_dev, is_multi_view);
	if (!render_pass) {
		throw std::runtime_error("no Vulkan render pass exists for device " + vk_dev.name);
	}
#if defined(FLOOR_DEBUG)
	const auto& desc = vk_render_pipeline.get_description(false);
	if (!desc.support_indirect_rendering) {
		log_error("graphics pipeline \"$\" specified for indirect render command does not support indirect rendering",
				  desc.debug_label);
		return;
	}
#endif
	bool has_soft_printf = false;
	if (pipeline_state->vs_entry) {
		vs = pipeline_state->vs_entry;
		has_soft_printf |= has_flag<toolchain::FUNCTION_FLAGS::USES_SOFT_PRINTF>(vs->info->flags);
	}
	if (pipeline_state->fs_entry) {
		fs = pipeline_state->fs_entry;
		has_soft_printf |= has_flag<toolchain::FUNCTION_FLAGS::USES_SOFT_PRINTF>(fs->info->flags);
	}
	
	// NOTE: for render commands/pipelines, this will always be the first per-query data entry
	cmd_buffer = pipeline_entry.per_queue_data[0].cmd_buffers.at(command_idx);
	
	const auto& pipeline_desc = pipeline.get_description(is_multi_view);
	const VkViewport cur_viewport {
		.x = 0.0f,
		.y = 0.0f,
		.width = (float)pipeline_desc.viewport.x,
		.height = (float)pipeline_desc.viewport.y,
		.minDepth = pipeline_desc.depth.range.x,
		.maxDepth = pipeline_desc.depth.range.y,
	};
	const VkCommandBufferInheritanceViewportScissorInfoNV inherit_viewport_scissor {
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_VIEWPORT_SCISSOR_INFO_NV,
		.pNext = nullptr,
		.viewportScissor2D = true,
		.viewportDepthCount = 1,
		.pViewportDepths = &cur_viewport,
	};
	const VkCommandBufferInheritanceInfo inheritance_info {
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO,
		.pNext = (vk_dev.inherited_viewport_scissor_support ? &inherit_viewport_scissor : nullptr),
		.renderPass = render_pass,
		.subpass = 0,
		.framebuffer = nullptr,
		.occlusionQueryEnable = false,
		.queryFlags = 0,
		.pipelineStatistics = 0,
	};
	const VkCommandBufferBeginInfo begin_info {
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
		.pNext = nullptr,
		.flags = ((((const vulkan_device&)dev).nested_cmd_buffers_support ?
				   VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT /* must be enabled if properly supported */ :
				   0 /* nop */) |
				  VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT /* all commands are within the same render pass */),
		.pInheritanceInfo = &inheritance_info,
	};
	VK_CALL_ERR_EXEC(vkBeginCommandBuffer(cmd_buffer, &begin_info),
					 "failed to begin command buffer for indirect render command",
					 throw std::runtime_error("failed to begin command buffer for indirect render command");)
	
	if (!vk_dev.inherited_viewport_scissor_support) {
		// need to set viewport + scissor in here already (if inheriting viewport/scissor is not supported)
		vkCmdSetViewport(cmd_buffer, 0, 1, &cur_viewport);
		
		const VkRect2D cur_render_area {
			// NOTE: Vulkan uses signed integers for the offset, but doesn't actually it to be < 0
			.offset = { int(pipeline_desc.scissor.offset.x), int(pipeline_desc.scissor.offset.y) },
			.extent = { pipeline_desc.scissor.extent.x, pipeline_desc.scissor.extent.y },
		};
		vkCmdSetScissor(cmd_buffer, 0, 1, &cur_render_area);
	}
	
	if (has_soft_printf) {
		if (!pipeline_entry.printf_buffer) {
			const auto default_queue = (const vulkan_queue*)dev.context->get_device_default_queue(dev);
			pipeline_entry.printf_buffer = allocate_printf_buffer(*default_queue);
		}
	}
}

vulkan_indirect_render_command_encoder::~vulkan_indirect_render_command_encoder() {
	// nop
}

void vulkan_indirect_render_command_encoder::set_arguments_vector(std::vector<device_function_arg>&& args_) {
	args = std::move(args_);
	implicit_args.clear();
	
	const auto vs_has_soft_printf = (vs && toolchain::has_flag<toolchain::FUNCTION_FLAGS::USES_SOFT_PRINTF>(vs->info->flags));
	const auto fs_has_soft_printf = (fs && toolchain::has_flag<toolchain::FUNCTION_FLAGS::USES_SOFT_PRINTF>(fs->info->flags));
	if (vs_has_soft_printf || fs_has_soft_printf) {
		// NOTE: will use the same printf buffer here, but we need to specify it twice if both functions use it
		// NOTE: these are automatically added to the used resources
		if (vs_has_soft_printf) {
			implicit_args.emplace_back(pipeline_entry.printf_buffer);
		}
		if (fs_has_soft_printf) {
			implicit_args.emplace_back(pipeline_entry.printf_buffer);
		}
	}
}

indirect_render_command_encoder& vulkan_indirect_render_command_encoder::draw(const uint32_t vertex_count,
																			  const uint32_t instance_count,
																			  const uint32_t first_vertex,
																			  const uint32_t first_instance) {
	const graphics_renderer::multi_draw_entry draw_entry {
		.vertex_count = vertex_count,
		.instance_count = instance_count,
		.first_vertex = first_vertex,
		.first_instance = first_instance,
	};
	draw_internal(&draw_entry, nullptr);
	return *this;
}

indirect_render_command_encoder& vulkan_indirect_render_command_encoder::draw_indexed(const device_buffer& index_buffer,
																					  const uint32_t index_count,
																					  const uint32_t instance_count,
																					  const uint32_t first_index,
																					  const int32_t vertex_offset,
																					  const uint32_t first_instance,
																					  const INDEX_TYPE index_type) {
	const graphics_renderer::multi_draw_indexed_entry draw_indexed_entry {
		.index_buffer = &index_buffer,
		.index_count = index_count,
		.instance_count = instance_count,
		.first_index = first_index,
		.vertex_offset = vertex_offset,
		.first_instance = first_instance,
		.index_type = index_type,
	};
	draw_internal(nullptr, &draw_indexed_entry);
	return *this;
}

void vulkan_indirect_render_command_encoder::draw_internal(const graphics_renderer::multi_draw_entry* draw_entry,
														   const graphics_renderer::multi_draw_indexed_entry* draw_index_entry) {
	assert(pipeline_state);
	const auto vk_vs = (const vulkan_function_entry*)vs;
	const auto vk_fs = (const vulkan_function_entry*)fs;
	
	// set up pipeline
	vkCmdBindPipeline(cmd_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_state->pipeline);
	
	// offset into "cmd_parameters" buffer where we'll write all parameters to for this command
	const auto cmd_params_offset = command_idx * pipeline_entry.per_cmd_size;
	std::vector<std::span<uint8_t>> cmd_params;
	size_t fs_cmd_params_offset = 0;
#if defined(FLOOR_DEBUG)
	// validate that all descriptor data fits into "per_cmd_size"
	const auto total_desc_data_size = (vk_vs ? vk_vs->desc_buffer.layout_size_in_bytes : 0) + (vk_fs ? vk_fs->desc_buffer.layout_size_in_bytes : 0);
	if (total_desc_data_size > pipeline_entry.per_cmd_size) {
		throw std::runtime_error("total descriptor data size " + std::to_string(total_desc_data_size) +
							" > expected per-cmd size " + std::to_string(pipeline_entry.per_cmd_size));
	}
#endif
	if (vk_vs) {
		// pipeline_entry.per_cmd_size check
		cmd_params.emplace_back(std::span<uint8_t> {
			(uint8_t*)pipeline_entry.mapped_cmd_parameters + cmd_params_offset,
			vk_vs->desc_buffer.layout_size_in_bytes
		});
		fs_cmd_params_offset = vk_vs->desc_buffer.layout_size_in_bytes;
	}
	if (vk_fs) {
		// pipeline_entry.per_cmd_size check
		cmd_params.emplace_back(std::span<uint8_t> {
			(uint8_t*)pipeline_entry.mapped_cmd_parameters + cmd_params_offset + fs_cmd_params_offset,
			vk_fs->desc_buffer.layout_size_in_bytes
		});
	}
	
	// set/handle arguments
	const auto has_non_arg_buffer_arguments_vs = (vk_vs && vk_vs->desc_buffer.desc_buffer_container != nullptr);
	const auto has_non_arg_buffer_arguments_fs = (vk_fs && vk_fs->desc_buffer.desc_buffer_container != nullptr);
	const auto has_non_arg_buffer_arguments = (has_non_arg_buffer_arguments_vs || has_non_arg_buffer_arguments_fs);
	const std::vector<const function_info*> entries {
		vk_vs ? vk_vs->info : nullptr,
		vk_fs ? vk_fs->info : nullptr,
	};
	const std::vector<const std::vector<VkDeviceSize>*> argument_offsets {
		vk_vs ? &vk_vs->desc_buffer.argument_offsets : nullptr,
		vk_fs ? &vk_fs->desc_buffer.argument_offsets : nullptr,
	};
	auto [args_success, arg_buffers] = vulkan_args::set_arguments<vulkan_args::ENCODER_TYPE::INDIRECT_SHADER>((const vulkan_device&)dev,
																											  cmd_params,
																											  entries,
																											  argument_offsets,
																											  { /* we never have any const buffers */ },
																											  args, implicit_args);
	args.clear(); // no longer needed
	if (!args_success) {
		throw std::runtime_error("failed to set arguments for indirect compute command");
	}
	
	// bind all the things (embedded + internal and external (arg-buffer) desc buffers + handle desc buf offsets)
	const VkBindDescriptorBufferEmbeddedSamplersInfoEXT bind_embedded_info {
		.sType = VK_STRUCTURE_TYPE_BIND_DESCRIPTOR_BUFFER_EMBEDDED_SAMPLERS_INFO_EXT,
		.pNext = nullptr,
		.stageFlags = VK_SHADER_STAGE_ALL_GRAPHICS,
		.layout = pipeline_state->layout,
		.set = 0 /* always set #0 */,
	};
	vkCmdBindDescriptorBufferEmbeddedSamplers2EXT(cmd_buffer, &bind_embedded_info);
	
	// setup + bind descriptor buffers
	const auto desc_buf_count = ((has_non_arg_buffer_arguments ? 1u : 0u) /* our cmd params desc buf */ +
								 uint32_t(arg_buffers.size()) /* user-specified argument buffers */);
	std::vector<VkDescriptorBufferBindingInfoEXT> desc_buf_bindings;
	desc_buf_bindings.resize(desc_buf_count);
	std::vector<VkBufferUsageFlags2CreateInfo> desc_buf_bindings_usage;
	desc_buf_bindings_usage.resize(desc_buf_count);
	size_t desc_buf_binding_idx = 0;
	
	if (has_non_arg_buffer_arguments) {
		const auto& desc_buffer = *(const vulkan_buffer*)pipeline_entry.cmd_parameters.get();
		desc_buf_bindings_usage[desc_buf_binding_idx] = {
			.sType = VK_STRUCTURE_TYPE_BUFFER_USAGE_FLAGS_2_CREATE_INFO,
			.pNext = nullptr,
			.usage = desc_buffer.get_vulkan_buffer_usage(),
		};
		desc_buf_bindings[desc_buf_binding_idx] = {
			.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_BUFFER_BINDING_INFO_EXT,
			.pNext = &desc_buf_bindings_usage[desc_buf_binding_idx],
			.address = desc_buffer.get_vulkan_buffer_device_address() + cmd_params_offset,
			.usage = 0,
		};
		++desc_buf_binding_idx;
	}
	
	for (const auto& arg_buffer : arg_buffers) {
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
	
	vkCmdBindDescriptorBuffersEXT(cmd_buffer, uint32_t(desc_buf_bindings.size()), desc_buf_bindings.data());
	
	// set fixed descriptor buffers (set #1 is the vertex shader, set #2 is the fragment shader)
	// NOTE: these may be optional
	static constexpr const uint32_t buffer_indices[2] { 0, 0 }; // same descriptor buffer here
	const VkDeviceSize offsets [2] { 0, fs_cmd_params_offset };
	const uint32_t start_set = (has_non_arg_buffer_arguments_vs ? 1 : 2);
	const uint32_t set_count = ((has_non_arg_buffer_arguments_vs ? 1 : 0) + (has_non_arg_buffer_arguments_fs ? 1 : 0));
	if (set_count > 0) {
		const VkSetDescriptorBufferOffsetsInfoEXT set_desc_buffer_offsets_info {
			.sType = VK_STRUCTURE_TYPE_SET_DESCRIPTOR_BUFFER_OFFSETS_INFO_EXT,
			.pNext = nullptr,
			.stageFlags = VK_SHADER_STAGE_ALL_GRAPHICS,
			.layout = pipeline_state->layout,
			.firstSet = start_set,
			.setCount = set_count,
			.pBufferIndices = &buffer_indices[0],
			.pOffsets = &offsets[0],
		};
		vkCmdSetDescriptorBufferOffsets2EXT(cmd_buffer, &set_desc_buffer_offsets_info);
	}
	
	// bind argument buffers if there are any
	// NOTE: descriptor set range is [5, 8] for vertex shaders and [9, 12] for fragment shaders
	std::vector<uint32_t> arg_buf_vs_buf_indices;
	std::vector<uint32_t> arg_buf_fs_buf_indices;
	uint32_t arg_buf_vs_set_count = 0, arg_buf_fs_set_count = 0;
	uint32_t desc_buf_index = (set_count > 0 ? 1 : 0); // 0 if no command desc buffer is used, 1 otherwise
	for (const auto& arg_buffer : arg_buffers) {
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
			.stageFlags = VK_SHADER_STAGE_ALL_GRAPHICS,
			.layout = pipeline_state->layout,
			.firstSet = vulkan_pipeline::argument_buffer_vs_start_set,
			.setCount = arg_buf_vs_set_count,
			.pBufferIndices = arg_buf_vs_buf_indices.data(),
			.pOffsets = arg_buf_offsets.data(),
		};
		vkCmdSetDescriptorBufferOffsets2EXT(cmd_buffer, &set_desc_buffer_offsets_info);
	}
	if (arg_buf_fs_set_count > 0) {
		const VkSetDescriptorBufferOffsetsInfoEXT set_desc_buffer_offsets_info {
			.sType = VK_STRUCTURE_TYPE_SET_DESCRIPTOR_BUFFER_OFFSETS_INFO_EXT,
			.pNext = nullptr,
			.stageFlags = VK_SHADER_STAGE_ALL_GRAPHICS,
			.layout = pipeline_state->layout,
			.firstSet = vulkan_pipeline::argument_buffer_fs_start_set,
			.setCount = arg_buf_fs_set_count,
			.pBufferIndices = arg_buf_fs_buf_indices.data(),
			.pOffsets = arg_buf_offsets.data(),
		};
		vkCmdSetDescriptorBufferOffsets2EXT(cmd_buffer, &set_desc_buffer_offsets_info);
	}
	
	// finally: draw
	if (draw_entry) {
		vkCmdDraw(cmd_buffer, draw_entry->vertex_count, draw_entry->instance_count, draw_entry->first_vertex, draw_entry->first_instance);
	} else if (draw_index_entry) {
		const auto vk_idx_buffer = draw_index_entry->index_buffer->get_underlying_vulkan_buffer_safe()->get_vulkan_buffer();
		const auto vk_idx_type = vulkan_index_type_from_index_type(draw_index_entry->index_type);
		if (((const vulkan_device&)dev).vulkan_version >= VULKAN_VERSION::VULKAN_1_4) {
			vkCmdBindIndexBuffer2(cmd_buffer, vk_idx_buffer, 0, VK_WHOLE_SIZE, vk_idx_type);
		} else {
			vkCmdBindIndexBuffer2KHR(cmd_buffer, vk_idx_buffer, 0, VK_WHOLE_SIZE, vk_idx_type);
		}
		vkCmdDrawIndexed(cmd_buffer, draw_index_entry->index_count, draw_index_entry->instance_count, draw_index_entry->first_index,
						 draw_index_entry->vertex_offset, draw_index_entry->first_instance);
	}
}

indirect_render_command_encoder& vulkan_indirect_render_command_encoder::draw_patches(const std::vector<const device_buffer*> control_point_buffers [[maybe_unused]],
																					  const device_buffer& tessellation_factors_buffer [[maybe_unused]],
																					  const uint32_t patch_control_point_count [[maybe_unused]],
																					  const uint32_t patch_count [[maybe_unused]],
																					  const uint32_t first_patch [[maybe_unused]],
																					  const uint32_t instance_count [[maybe_unused]],
																					  const uint32_t first_instance [[maybe_unused]]) {
	throw std::runtime_error("tessellation not implemented in Vulkan yet");
}

indirect_render_command_encoder& vulkan_indirect_render_command_encoder::draw_patches_indexed(const std::vector<const device_buffer*> control_point_buffers [[maybe_unused]],
																							  const device_buffer& control_point_index_buffer [[maybe_unused]],
																							  const device_buffer& tessellation_factors_buffer [[maybe_unused]],
																							  const uint32_t patch_control_point_count [[maybe_unused]],
																							  const uint32_t patch_count [[maybe_unused]],
																							  const uint32_t first_index [[maybe_unused]],
																							  const uint32_t first_patch [[maybe_unused]],
																							  const uint32_t instance_count [[maybe_unused]],
																							  const uint32_t first_instance [[maybe_unused]]) {
	throw std::runtime_error("tessellation not implemented in Vulkan yet");
}

vulkan_indirect_compute_command_encoder::vulkan_indirect_compute_command_encoder(const vulkan_indirect_command_pipeline::vulkan_pipeline_entry& pipeline_entry_,
																				 const uint32_t command_idx_,
																				 const device& dev_, const device_function& kernel_obj_) :
indirect_compute_command_encoder(dev_, kernel_obj_), pipeline_entry(pipeline_entry_), command_idx(command_idx_) {
	const auto vk_function_entry = (const vulkan_function_entry*)entry;
	if (!vk_function_entry || !vk_function_entry->pipeline_layout || !vk_function_entry->info) {
		throw std::runtime_error("state is invalid or no compute pipeline entry exists for device " + dev.name);
	}
	
#if defined(FLOOR_DEBUG)
	// verify that the layout size is <= our pre-computed max per command size
	if (pipeline_entry.per_cmd_size < vk_function_entry->desc_buffer.layout_size_in_bytes) {
		throw std::runtime_error("miscalculated per command size: " + std::to_string(pipeline_entry.per_cmd_size)
							+ " < actual layout size " + std::to_string(vk_function_entry->desc_buffer.layout_size_in_bytes));
	}
#endif
	
	uint32_t per_queue_data_idx = 0;
	for (auto& per_queue_data : pipeline_entry.per_queue_data) {
		cmd_buffers[per_queue_data_idx] = per_queue_data.cmd_buffers.at(command_idx);
		const VkCommandBufferInheritanceInfo inheritance_info {
			.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO,
			.pNext = nullptr,
			.renderPass = nullptr,
			.subpass = 0,
			.framebuffer = nullptr,
			.occlusionQueryEnable = false,
			.queryFlags = 0,
			.pipelineStatistics = 0,
		};
		const VkCommandBufferBeginInfo begin_info {
			.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
			.pNext = nullptr,
			.flags = (((const vulkan_device&)dev).nested_cmd_buffers_support ?
					  VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT /* must be enabled if properly supported */ :
					  0 /* nop */),
			.pInheritanceInfo = &inheritance_info,
		};
		VK_CALL_ERR_EXEC(vkBeginCommandBuffer(cmd_buffers[per_queue_data_idx], &begin_info),
						 "failed to begin command buffer for indirect compute command",
						 throw std::runtime_error("failed to begin command buffer for indirect compute command");)
		
		++per_queue_data_idx;
	}
	
	if (toolchain::has_flag<toolchain::FUNCTION_FLAGS::USES_SOFT_PRINTF>(vk_function_entry->info->flags)) {
		if (!pipeline_entry.printf_buffer) {
			const auto default_queue = (const vulkan_queue*)dev.context->get_device_default_queue(dev);
			pipeline_entry.printf_buffer = allocate_printf_buffer(*default_queue);
		}
	}
}

vulkan_indirect_compute_command_encoder::~vulkan_indirect_compute_command_encoder() {
	// nop
}

void vulkan_indirect_compute_command_encoder::set_arguments_vector(std::vector<device_function_arg>&& args_) {
	args = std::move(args_);
	implicit_args.clear();
	
	if (toolchain::has_flag<toolchain::FUNCTION_FLAGS::USES_SOFT_PRINTF>(entry->info->flags)) {
		// NOTE: this is automatically added to the used resources
		implicit_args.emplace_back(pipeline_entry.printf_buffer);
	}
}

indirect_compute_command_encoder& vulkan_indirect_compute_command_encoder::execute(const uint32_t dim floor_unused,
																				   const uint3& global_work_size,
																				   const uint3& local_work_size) {
	const auto& vk_dev = (const vulkan_device&)dev;
	const auto& vk_kernel = (const vulkan_function&)kernel_obj;
	const auto& vk_function_entry = (const vulkan_function_entry&)*entry;
	
	// check and set work sizes
	const uint3 block_dim = vk_kernel.check_local_work_size(*entry, local_work_size);
	const uint3 grid_dim_overflow {
		global_work_size.x > 0 ? std::min(uint32_t(global_work_size.x % block_dim.x), 1u) : 0u,
		global_work_size.y > 0 ? std::min(uint32_t(global_work_size.y % block_dim.y), 1u) : 0u,
		global_work_size.z > 0 ? std::min(uint32_t(global_work_size.z % block_dim.z), 1u) : 0u
	};
	uint3 grid_dim { (global_work_size / block_dim) + grid_dim_overflow };
	grid_dim.max(1u);
	
	// encode args
	// offset into "cmd_parameters" buffer where we'll write all parameters to for this command
	const auto cmd_params_offset = command_idx * pipeline_entry.per_cmd_size;
	std::span<uint8_t> cmd_params { (uint8_t*)pipeline_entry.mapped_cmd_parameters + cmd_params_offset, pipeline_entry.per_cmd_size };
	
	// set/handle arguments
	const auto has_non_arg_buffer_arguments = (vk_function_entry.desc_buffer.desc_buffer_container != nullptr);
	auto [args_success, arg_buffers] = vulkan_args::set_arguments<vulkan_args::ENCODER_TYPE::INDIRECT_COMPUTE>((const vulkan_device&)dev,
																											   { cmd_params },
																											   { entry->info },
																											   { &vk_function_entry.desc_buffer.argument_offsets },
																											   { /* we never have any const buffers */ },
																											   args, implicit_args);
	args.clear(); // no longer needed
	if (!args_success) {
		throw std::runtime_error("failed to set arguments for indirect compute command");
	}
	
	// encode per-queue data
	auto vk_pipeline = vk_kernel.get_pipeline_spec(vk_dev,
												   // unfortunate, but we know this is modifiable
												   const_cast<vulkan_function_entry&>(vk_function_entry),
												   block_dim, {} /* use default or program defined */);
	
	const auto desc_buf_count = ((has_non_arg_buffer_arguments ? 1u : 0u) /* our cmd params desc buf */ +
								 uint32_t(arg_buffers.size()) /* user-specified argument buffers */);
	std::vector<VkDescriptorBufferBindingInfoEXT> desc_buf_bindings;
	desc_buf_bindings.resize(desc_buf_count);
	std::vector<VkBufferUsageFlags2CreateInfo> desc_buf_bindings_usage;
	desc_buf_bindings_usage.resize(desc_buf_count);
	size_t desc_buf_binding_idx = 0;
	
	if (has_non_arg_buffer_arguments) {
		const auto& kernel_desc_buffer = *(const vulkan_buffer*)pipeline_entry.cmd_parameters.get();
		desc_buf_bindings_usage[desc_buf_binding_idx] = {
			.sType = VK_STRUCTURE_TYPE_BUFFER_USAGE_FLAGS_2_CREATE_INFO,
			.pNext = nullptr,
			.usage = kernel_desc_buffer.get_vulkan_buffer_usage(),
		};
		desc_buf_bindings[desc_buf_binding_idx] = {
			.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_BUFFER_BINDING_INFO_EXT,
			.pNext = &desc_buf_bindings_usage[desc_buf_binding_idx],
			.address = kernel_desc_buffer.get_vulkan_buffer_device_address() + cmd_params_offset,
			.usage = 0,
		};
		++desc_buf_binding_idx;
	}
	
	for (const auto& arg_buffer : arg_buffers) {
		assert(arg_buffer.first == 0);
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
	
	const std::vector<VkDeviceSize> offsets(desc_buf_count, 0); // always 0 for all
	std::vector<uint32_t> buffer_indices(desc_buf_count, 0);
	for (uint32_t i = 1; i < desc_buf_count; ++i) {
		buffer_indices[i] = i;
	}
	const auto start_set = (!has_non_arg_buffer_arguments ? 2u /* first arg buffer set */ : 1u /* kernel set */);
	
	for (uint32_t per_queue_data_idx = 0; per_queue_data_idx < uint32_t(pipeline_entry.per_queue_data.size()); ++per_queue_data_idx) {
		auto cmd_buffer = cmd_buffers[per_queue_data_idx];
		
		// set up pipeline
		vkCmdBindPipeline(cmd_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, vk_pipeline);
		
		// bind all the things (embedded + internal and external (arg-buffer) desc buffers + handle desc buf offsets)
		const VkBindDescriptorBufferEmbeddedSamplersInfoEXT bind_embedded_info {
			.sType = VK_STRUCTURE_TYPE_BIND_DESCRIPTOR_BUFFER_EMBEDDED_SAMPLERS_INFO_EXT,
			.pNext = nullptr,
			.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
			.layout = vk_function_entry.pipeline_layout,
			.set = 0 /* always set #0 */,
		};
		vkCmdBindDescriptorBufferEmbeddedSamplers2EXT(cmd_buffer, &bind_embedded_info);
		
		vkCmdBindDescriptorBuffersEXT(cmd_buffer, desc_buf_count, desc_buf_bindings.data());
		
		// kernel descriptor set + any argument buffers are stored in contiguous descriptor set indices
		const VkSetDescriptorBufferOffsetsInfoEXT set_desc_buffer_offsets_info {
			.sType = VK_STRUCTURE_TYPE_SET_DESCRIPTOR_BUFFER_OFFSETS_INFO_EXT,
			.pNext = nullptr,
			.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
			.layout = vk_function_entry.pipeline_layout,
			.firstSet = start_set,
			.setCount = desc_buf_count,
			.pBufferIndices = buffer_indices.data(),
			.pOffsets = offsets.data(),
		};
		vkCmdSetDescriptorBufferOffsets2EXT(cmd_buffer, &set_desc_buffer_offsets_info);
		
		// finally: dispatch
		vkCmdDispatch(cmd_buffer, grid_dim.x, grid_dim.y, grid_dim.z);
	}
	
	return *this;
}

indirect_compute_command_encoder& vulkan_indirect_compute_command_encoder::barrier() {
	// full barrier
	const VkMemoryBarrier2 mem_bar {
		.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2,
		.pNext = nullptr,
		.srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
		.srcAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT,
		.dstStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
		.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT,
	};
	const VkDependencyInfo dep_info {
		.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
		.pNext = nullptr,
		.dependencyFlags = 0,
		.memoryBarrierCount = 1,
		.pMemoryBarriers = &mem_bar,
		.bufferMemoryBarrierCount = 0,
		.pBufferMemoryBarriers = nullptr,
		.imageMemoryBarrierCount = 0,
		.pImageMemoryBarriers = nullptr,
	};
	for (uint32_t per_queue_data_idx = 0; per_queue_data_idx < uint32_t(pipeline_entry.per_queue_data.size()); ++per_queue_data_idx) {
		vkCmdPipelineBarrier2(cmd_buffers[per_queue_data_idx], &dep_info);
	}
	return *this;
}

} // namespace fl

#endif
