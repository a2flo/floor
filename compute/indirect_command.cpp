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

#include <floor/compute/indirect_command.hpp>
#include <floor/compute/compute_buffer.hpp>
#include <floor/compute/compute_context.hpp>
#include <floor/compute/vulkan/vulkan_device.hpp>
#include <floor/compute/vulkan/vulkan_kernel.hpp>
#include <floor/graphics/graphics_pipeline.hpp>

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

void indirect_command_description::compute_buffer_counts_from_functions(const compute_device& dev, const vector<const compute_kernel*>& functions) {
#if !defined(FLOOR_NO_VULKAN)
	// for Vulkan, we can directly derive a "buffer count" from the descriptor buffer/layout size and the SSBO descriptor size
	const auto is_vulkan = (dev.context->get_compute_type() == COMPUTE_TYPE::VULKAN);
	if (is_vulkan) {
		const auto ssbo_size = ((const vulkan_device&)dev).desc_buffer_sizes.ssbo;
		for (const auto& func : functions) {
			const auto entry = func->get_kernel_entry(dev);
			if (!entry || !entry->info) {
				continue;
			}
			const auto vk_entry = (const vulkan_kernel::vulkan_kernel_entry*)entry;
			const auto buf_count = uint32_t((vk_entry->desc_buffer.layout_size_in_bytes + ssbo_size - 1u) / ssbo_size);
			switch (entry->info->type) {
				case llvm_toolchain::FUNCTION_TYPE::KERNEL:
					max_kernel_buffer_count = max(max_kernel_buffer_count, buf_count);
					break;
				case llvm_toolchain::FUNCTION_TYPE::VERTEX:
				case llvm_toolchain::FUNCTION_TYPE::TESSELLATION_EVALUATION:
					max_vertex_buffer_count = max(max_vertex_buffer_count, buf_count);
					break;
				case llvm_toolchain::FUNCTION_TYPE::FRAGMENT:
					max_fragment_buffer_count = max(max_fragment_buffer_count, buf_count);
					break;
				case llvm_toolchain::FUNCTION_TYPE::NONE:
				case llvm_toolchain::FUNCTION_TYPE::TESSELLATION_CONTROL:
				case llvm_toolchain::FUNCTION_TYPE::ARGUMENT_BUFFER_STRUCT:
					throw runtime_error("unhandled function type");
			}
		}
		// NOTE: still continue and perform the "normal" buffer count computation (as a validity check)
	}
#else
	constexpr const auto is_vulkan = false;
#endif
	
	for (const auto& func : functions) {
		const auto entry = func->get_kernel_entry(dev);
		if (!entry || !entry->info) {
			continue;
		}
		uint32_t buf_count = 0;
		for (const auto& arg : entry->info->args) {
#if defined(FLOOR_DEBUG)
			if (arg.image_type != llvm_toolchain::ARG_IMAGE_TYPE::NONE) {
				log_error("must not have image parameters (in function \"$\") intended for indirect compute/render", entry->info->name);
				continue;
			}
#endif
			switch (arg.special_type) {
				case llvm_toolchain::SPECIAL_TYPE::NONE:
				case llvm_toolchain::SPECIAL_TYPE::SSBO:
					++buf_count;
					break;
				case llvm_toolchain::SPECIAL_TYPE::ARGUMENT_BUFFER:
					// for Vulkan, argument buffers are separately stored descriptor buffers (-> don't need to account for them here)
					if (!is_vulkan) {
						++buf_count;
					}
					break;
				case llvm_toolchain::SPECIAL_TYPE::STAGE_INPUT:
					// only tessellation evaluation shaders may contain buffers in stage_input
					if (entry->info->type == llvm_toolchain::FUNCTION_TYPE::TESSELLATION_EVALUATION) {
						buf_count += arg.size;
					}
					break;
				case llvm_toolchain::SPECIAL_TYPE::IMAGE_ARRAY:
				case llvm_toolchain::SPECIAL_TYPE::BUFFER_ARRAY:
				case llvm_toolchain::SPECIAL_TYPE::IUB:
				case llvm_toolchain::SPECIAL_TYPE::PUSH_CONSTANT:
#if defined(FLOOR_DEBUG)
					log_error("must not have image/buffer-array, IUB or push-constant parameters (in function \"$\") intended for indirect compute/render",
							  entry->info->name);
#endif
					break;
			}
		}
		if (llvm_toolchain::has_flag<llvm_toolchain::FUNCTION_FLAGS::USES_SOFT_PRINTF>(entry->info->flags)) {
			++buf_count;
		}
		switch (entry->info->type) {
			case llvm_toolchain::FUNCTION_TYPE::KERNEL:
				max_kernel_buffer_count = max(max_kernel_buffer_count, buf_count);
				break;
			case llvm_toolchain::FUNCTION_TYPE::VERTEX:
			case llvm_toolchain::FUNCTION_TYPE::TESSELLATION_EVALUATION:
				max_vertex_buffer_count = max(max_vertex_buffer_count, buf_count);
				break;
			case llvm_toolchain::FUNCTION_TYPE::FRAGMENT:
				max_fragment_buffer_count = max(max_fragment_buffer_count, buf_count);
				break;
			case llvm_toolchain::FUNCTION_TYPE::NONE:
			case llvm_toolchain::FUNCTION_TYPE::TESSELLATION_CONTROL:
			case llvm_toolchain::FUNCTION_TYPE::ARGUMENT_BUFFER_STRUCT:
				throw runtime_error("unhandled function type");
		}
	}
}

uint32_t indirect_command_pipeline::get_command_count() const {
	return (uint32_t)commands.size();
}

void indirect_command_pipeline::reset() {
	commands.clear();
}

indirect_render_command_encoder::indirect_render_command_encoder(const compute_queue& dev_queue_, const graphics_pipeline& pipeline_) :
indirect_command_encoder(dev_queue_), pipeline(pipeline_) {
	if (!pipeline.is_valid()) {
		throw runtime_error("invalid graphics_pipeline ('" + pipeline.get_description(false).debug_label +
							"') specified in indirect render command encoder");
	}
}

indirect_compute_command_encoder::indirect_compute_command_encoder(const compute_queue& dev_queue_, const compute_kernel& kernel_obj_) :
indirect_command_encoder(dev_queue_), kernel_obj(kernel_obj_) {
	entry = kernel_obj.get_kernel_entry(dev_queue.get_device());
	if (!entry) {
		throw runtime_error("invalid compute_kernel specified in indirect compute command encoder");
	}
}
