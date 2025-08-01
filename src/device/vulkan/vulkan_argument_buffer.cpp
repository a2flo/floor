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
#include "internal/vulkan_headers.hpp"
#include <floor/device/vulkan/vulkan_argument_buffer.hpp>
#include <floor/device/vulkan/vulkan_device.hpp>
#include <floor/device/vulkan/vulkan_context.hpp>
#include "internal/vulkan_args.hpp"
#include "internal/vulkan_descriptor_set.hpp"
#include <floor/device/device_context.hpp>

namespace fl {

vulkan_argument_buffer::vulkan_argument_buffer(const device_function& func_,
											   std::shared_ptr<device_buffer> storage_buffer_,
											   const toolchain::function_info& arg_info_,
											   const vulkan_descriptor_set_layout_t& layout_,
											   std::vector<VkDeviceSize>&& argument_offsets_,
											   std::span<uint8_t> mapped_host_memory_,
											   std::shared_ptr<device_buffer> constant_buffer_storage_,
											   std::span<uint8_t> constant_buffer_mapping_) :
argument_buffer(func_, storage_buffer_), arg_info(arg_info_), layout(layout_), argument_offsets(argument_offsets_),
mapped_host_memory(mapped_host_memory_), constant_buffer_storage(constant_buffer_storage_), constant_buffer_mapping(constant_buffer_mapping_) {
	if (mapped_host_memory.data() == nullptr || mapped_host_memory.size_bytes() == 0) {
		throw std::runtime_error("argument buffer host memory has not been mapped");
	}
	if (constant_buffer_storage && (constant_buffer_mapping.data() == nullptr || constant_buffer_mapping.size_bytes() == 0)) {
		throw std::runtime_error("constant buffer for argument buffer exists, but no host memory has been mapped");
	}
	if (has_flag<toolchain::FUNCTION_FLAGS::USES_SOFT_PRINTF>(arg_info.flags)) {
		throw std::runtime_error("should not have soft-printf flag in argument buffer function info");
	}
#if defined(FLOOR_DEBUG)
	if ((((const vulkan_buffer*)storage_buffer.get())->get_vulkan_buffer_usage() & VK_BUFFER_USAGE_2_RESOURCE_DESCRIPTOR_BUFFER_BIT_EXT) == 0) {
		throw std::runtime_error("argument buffer storage has not been created with descriptor buffer support");
	}
#endif
}

vulkan_argument_buffer::~vulkan_argument_buffer() {
	// ensure buffers are unmapped when they are heap-allocated
	if (storage_buffer->is_heap_allocated()) {
		storage_buffer->unmap(*storage_buffer->get_default_queue_for_memory(*storage_buffer), mapped_host_memory.data());
	}
	if (constant_buffer_storage && constant_buffer_storage->is_heap_allocated()) {
		constant_buffer_storage->unmap(*constant_buffer_storage->get_default_queue_for_memory(*constant_buffer_storage),
									   constant_buffer_mapping.data());
	}
}

bool vulkan_argument_buffer::set_arguments(const device_queue& dev_queue, const std::vector<device_function_arg>& args) {
	const auto& vk_dev = (const vulkan_device&)dev_queue.get_device();
	const vulkan_args::constant_buffer_wrapper_t const_buf {
		&layout.constant_buffer_info, constant_buffer_storage.get(), constant_buffer_mapping
	};
	auto [success, arg_buffers] = vulkan_args::set_arguments<vulkan_args::ENCODER_TYPE::ARGUMENT>(vk_dev,
																								  { mapped_host_memory },
																								  { &arg_info },
																								  { &argument_offsets },
																								  { &const_buf }, args, {});
	if (!success) {
		return false;
	}
	if (!arg_buffers.empty()) {
		log_error("argument buffers inside other argument buffers are not allowed");
		return false;
	}
	return true;
}

void vulkan_argument_buffer::set_debug_label(const std::string& label) {
	argument_buffer::set_debug_label(label);
	if (storage_buffer) {
		if (auto vk_buf = storage_buffer->get_underlying_vulkan_buffer_safe(); vk_buf) {
			const_cast<vulkan_buffer*>(vk_buf)->set_debug_label(label);
		}
	}
}

} // namespace fl

#endif
