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

#include <floor/device/indirect_command.hpp>
#include <floor/device/device_buffer.hpp>
#include <floor/device/device_context.hpp>
#include <floor/device/vulkan/vulkan_device.hpp>
#include <floor/device/vulkan/vulkan_function.hpp>
#include <floor/device/graphics_pipeline.hpp>
#if !defined(FLOOR_NO_VULKAN)
#include "vulkan/internal/vulkan_headers.hpp"
#include "vulkan/internal/vulkan_descriptor_set.hpp"
#include "vulkan/internal/vulkan_function_entry.hpp"
#endif

namespace fl {

indirect_command_pipeline::indirect_command_pipeline(const indirect_command_description& desc_) : desc(desc_) {
	// check validity
	if (desc.max_command_count == 0) {
		log_error("must be able to encode at least one command in indirect command pipeline \"$\"",
				  desc.debug_label);
		valid = false;
	}
	if (desc.command_type != indirect_command_description::COMMAND_TYPE::RENDER && (desc.max_vertex_buffer_count > 0 ||
																					desc.max_fragment_buffer_count > 0)) {
		log_warn("render commands are disabled, but max vertex/fragment buffer count is not 0 in indirect command pipeline \"$\"",
				 desc.debug_label);
	}
	if (desc.command_type != indirect_command_description::COMMAND_TYPE::COMPUTE && desc.max_kernel_buffer_count > 0) {
		log_warn("compute commands are disabled, but max kernel buffer count is not 0 in indirect command pipeline \"$\"",
				 desc.debug_label);
	}
}

void indirect_command_description::compute_buffer_counts_from_functions(const device& dev, const std::vector<const device_function*>& functions) {
#if !defined(FLOOR_NO_VULKAN)
	// for Vulkan, we can directly derive a "buffer count" from the descriptor buffer/layout size and the SSBO descriptor size
	const auto is_vulkan = (dev.context->get_platform_type() == PLATFORM_TYPE::VULKAN);
	if (is_vulkan) {
		const auto ssbo_size = ((const vulkan_device&)dev).desc_buffer_sizes.ssbo;
		for (const auto& func : functions) {
			const auto entry = func->get_function_entry(dev);
			if (!entry || !entry->info) {
				continue;
			}
			const auto vk_entry = (const vulkan_function_entry*)entry;
			const auto buf_count = uint32_t((vk_entry->desc_buffer.layout_size_in_bytes + ssbo_size - 1u) / ssbo_size);
			switch (entry->info->type) {
				case toolchain::FUNCTION_TYPE::KERNEL:
					max_kernel_buffer_count = std::max(max_kernel_buffer_count, buf_count);
					break;
				case toolchain::FUNCTION_TYPE::VERTEX:
				case toolchain::FUNCTION_TYPE::TESSELLATION_EVALUATION:
					max_vertex_buffer_count = std::max(max_vertex_buffer_count, buf_count);
					break;
				case toolchain::FUNCTION_TYPE::FRAGMENT:
					max_fragment_buffer_count = std::max(max_fragment_buffer_count, buf_count);
					break;
				case toolchain::FUNCTION_TYPE::NONE:
				case toolchain::FUNCTION_TYPE::TESSELLATION_CONTROL:
				case toolchain::FUNCTION_TYPE::ARGUMENT_BUFFER_STRUCT:
					throw std::runtime_error("unhandled function type");
			}
		}
		// NOTE: still continue and perform the "normal" buffer count computation (as a validity check)
	}
#else
	constexpr const auto is_vulkan = false;
#endif
	
	for (const auto& func : functions) {
		const auto entry = func->get_function_entry(dev);
		if (!entry || !entry->info) {
			continue;
		}
		uint32_t buf_count = 0;
		for (const auto& arg : entry->info->args) {
#if defined(FLOOR_DEBUG)
			if (arg.image_type != toolchain::ARG_IMAGE_TYPE::NONE) {
				log_error("must not have image parameters (in function \"$\") intended for indirect compute/render", entry->info->name);
				continue;
			}
#endif
			
			if (has_flag<toolchain::ARG_FLAG::IMAGE_ARRAY>(arg.flags) ||
				has_flag<toolchain::ARG_FLAG::BUFFER_ARRAY>(arg.flags) ||
				has_flag<toolchain::ARG_FLAG::IUB>(arg.flags) ||
				has_flag<toolchain::ARG_FLAG::PUSH_CONSTANT>(arg.flags)) {
#if defined(FLOOR_DEBUG)
				log_error("must not have image/buffer-array, IUB or push-constant parameters (in function \"$\") intended for indirect compute/render",
						  entry->info->name);
				continue;
#endif
			}
			
			if (arg.flags == toolchain::ARG_FLAG::NONE ||
				has_flag<toolchain::ARG_FLAG::SSBO>(arg.flags)) {
				++buf_count;
			} else if (has_flag<toolchain::ARG_FLAG::ARGUMENT_BUFFER>(arg.flags)) {
				// for Vulkan, argument buffers are separately stored descriptor buffers (-> don't need to account for them here)
				if (!is_vulkan) {
					++buf_count;
				}
			} else if (has_flag<toolchain::ARG_FLAG::STAGE_INPUT>(arg.flags)) {
				// only tessellation evaluation shaders may contain buffers in stage_input
				if (entry->info->type == toolchain::FUNCTION_TYPE::TESSELLATION_EVALUATION) {
					buf_count += arg.size;
				}
			}
		}
		if (toolchain::has_flag<toolchain::FUNCTION_FLAGS::USES_SOFT_PRINTF>(entry->info->flags)) {
			++buf_count;
		}
		switch (entry->info->type) {
			case toolchain::FUNCTION_TYPE::KERNEL:
				max_kernel_buffer_count = std::max(max_kernel_buffer_count, buf_count);
				break;
			case toolchain::FUNCTION_TYPE::VERTEX:
			case toolchain::FUNCTION_TYPE::TESSELLATION_EVALUATION:
				max_vertex_buffer_count = std::max(max_vertex_buffer_count, buf_count);
				break;
			case toolchain::FUNCTION_TYPE::FRAGMENT:
				max_fragment_buffer_count = std::max(max_fragment_buffer_count, buf_count);
				break;
			case toolchain::FUNCTION_TYPE::NONE:
			case toolchain::FUNCTION_TYPE::TESSELLATION_CONTROL:
			case toolchain::FUNCTION_TYPE::ARGUMENT_BUFFER_STRUCT:
				throw std::runtime_error("unhandled function type");
		}
	}
}

uint32_t indirect_command_pipeline::get_command_count() const {
	return (uint32_t)commands.size();
}

void indirect_command_pipeline::reset() {
	commands.clear();
}

indirect_render_command_encoder::indirect_render_command_encoder(const device& dev_,
																 const graphics_pipeline& pipeline_,
																 const bool is_multi_view_) :
indirect_command_encoder(dev_), pipeline(pipeline_), is_multi_view(is_multi_view_) {
	if (!pipeline.is_valid()) {
		throw std::runtime_error("invalid graphics_pipeline ('" + pipeline.get_description(false).debug_label +
							"') specified in indirect render command encoder");
	}
	if (is_multi_view && !pipeline.is_multi_view_capable()) {
		throw std::runtime_error("invalid graphics_pipeline ('" + pipeline.get_description(false).debug_label +
							"') configuration: multi-view was requested, but is not supported by the pipeline");
	}
}

indirect_compute_command_encoder::indirect_compute_command_encoder(const device& dev_,
																   const device_function& kernel_obj_) :
indirect_command_encoder(dev_), kernel_obj(kernel_obj_) {
	entry = kernel_obj.get_function_entry(dev);
	if (!entry) {
		throw std::runtime_error("invalid device_function specified in indirect compute command encoder");
	}
}

//
generic_indirect_command_pipeline::generic_indirect_command_pipeline(const indirect_command_description& desc_,
																	 const std::vector<std::unique_ptr<device>>& devices) :
indirect_command_pipeline(desc_) {
	if (!valid) {
		return;
	}
	if (desc.command_type == indirect_command_description::COMMAND_TYPE::RENDER) {
		log_error("indirect render command pipelines are not supported (pipeline \"$\")", desc.debug_label);
		valid = false;
		return;
	}
	if (devices.empty()) {
		log_error("no devices specified in indirect command pipeline \"$\"", desc.debug_label);
		valid = false;
		return;
	}
	if (!desc.ignore_max_max_command_count_limit && desc.max_command_count > 16384u) {
		log_error("max supported command count is 16384, in indirect command pipeline \"$\"", desc.debug_label);
		valid = false;
		return;
	}
	
	for (const auto& dev_ptr : devices) {
		auto entry = std::make_shared<generic_indirect_pipeline_entry>();
		entry->dev = dev_ptr.get();
		entry->debug_label = (desc.debug_label.empty() ? "generic_indirect_command_pipeline" : desc.debug_label);
		entry->commands.reserve(desc.max_command_count);
		pipelines.insert_or_assign(dev_ptr.get(), std::move(entry));
	}
}

const generic_indirect_pipeline_entry* generic_indirect_command_pipeline::get_pipeline_entry(const device& dev) const {
	if (const auto iter = pipelines.find(&dev); iter != pipelines.end()) {
		return iter->second.get();
	}
	return nullptr;
}

generic_indirect_pipeline_entry* generic_indirect_command_pipeline::get_pipeline_entry(const device& dev) {
	if (const auto iter = pipelines.find(&dev); iter != pipelines.end()) {
		return iter->second.get();
	}
	return nullptr;
}

indirect_compute_command_encoder& generic_indirect_command_pipeline::add_compute_command(const device& dev, const device_function& kernel_obj) {
	if (desc.command_type != indirect_command_description::COMMAND_TYPE::COMPUTE) {
		throw std::runtime_error("adding compute commands to a render indirect command pipeline is not allowed");
	}
	
	const auto pipeline_entry = get_pipeline_entry(dev);
	if (!pipeline_entry) {
		throw std::runtime_error("no pipeline entry for device " + dev.name);
	}
	if (commands.size() >= desc.max_command_count) {
		throw std::runtime_error("already encoded the max amount of commands in indirect command pipeline " + desc.debug_label);
	}
	
	auto compute_enc = std::make_unique<generic_indirect_compute_command_encoder>(*pipeline_entry, dev, kernel_obj);
	auto compute_enc_ptr = compute_enc.get();
	commands.emplace_back(std::move(compute_enc));
	return *compute_enc_ptr;
}

void generic_indirect_command_pipeline::complete(const device& dev) {
	if (!get_pipeline_entry(dev)) {
		throw std::runtime_error("no pipeline entry for device " + dev.name);
	}
}

void generic_indirect_command_pipeline::complete() {
	// nop
}

void generic_indirect_command_pipeline::reset() {
	for (auto&& pipeline : pipelines) {
		// just clear all parameters
		pipeline.second->commands.clear();
	}
	indirect_command_pipeline::reset();
}

std::optional<generic_indirect_command_pipeline::command_range_t>
generic_indirect_command_pipeline::compute_and_validate_command_range(const uint32_t command_offset,
																	  const uint32_t command_count) const {
	command_range_t range { command_offset, command_count };
	if (command_count == ~0u) {
		range.count = get_command_count();
	}
#if defined(FLOOR_DEBUG)
	{
		const auto cmd_count = get_command_count();
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

//
generic_indirect_compute_command_encoder::generic_indirect_compute_command_encoder(generic_indirect_pipeline_entry& pipeline_entry_,
																				   const device& dev_, const device_function& kernel_obj_) :
indirect_compute_command_encoder(dev_, kernel_obj_), pipeline_entry(pipeline_entry_) {
	if (!entry || !entry->info) {
		throw std::runtime_error("state is invalid or no compute pipeline entry exists for device " + dev.name);
	}
}

void generic_indirect_compute_command_encoder::set_arguments_vector(std::vector<device_function_arg>&& args_) {
	args = std::move(args_);
}

indirect_compute_command_encoder& generic_indirect_compute_command_encoder::execute(const uint32_t dim,
																					const uint3& global_work_size,
																					const uint3& local_work_size) {
	// add command
	pipeline_entry.commands.emplace_back(generic_indirect_pipeline_entry::command_t {
		.kernel_ptr = &kernel_obj,
		.dim = dim,
		.global_work_size = global_work_size,
		.local_work_size = local_work_size,
		.args = std::move(args),
	});
	return *this;
}

indirect_compute_command_encoder& generic_indirect_compute_command_encoder::barrier() {
	// nop if there isn't any command yet
	if (pipeline_entry.commands.empty()) {
		return *this;
	}
	// make last command blocking
	// TODO: or do we need a full queue sync/finish?
	pipeline_entry.commands.back().wait_until_completion = true;
	return *this;
}

} // namespace fl
