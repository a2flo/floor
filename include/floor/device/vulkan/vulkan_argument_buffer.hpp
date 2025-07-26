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

#pragma once

#include <floor/device/argument_buffer.hpp>

#if !defined(FLOOR_NO_VULKAN)
#include <floor/core/aligned_ptr.hpp>
#include <floor/device/vulkan/vulkan_function.hpp>

namespace fl {

struct vulkan_descriptor_set_layout_t;

class vulkan_argument_buffer : public argument_buffer {
public:
	vulkan_argument_buffer(const device_function& func_,
						   std::shared_ptr<device_buffer> storage_buffer,
						   const toolchain::function_info& arg_info,
						   const vulkan_descriptor_set_layout_t& layout_,
						   std::vector<VkDeviceSize>&& argument_offsets_,
						   std::span<uint8_t> mapped_host_memory_,
						   std::shared_ptr<device_buffer> constant_buffer_storage_,
						   std::span<uint8_t> constant_buffer_mapping_);
	~vulkan_argument_buffer() override;
	
	bool set_arguments(const device_queue& dev_queue, const std::vector<device_function_arg>& args) override;
	
	void set_debug_label(const std::string& label) override;
	
protected:
	const toolchain::function_info& arg_info;
	const vulkan_descriptor_set_layout_t& layout;
	const std::vector<VkDeviceSize> argument_offsets;
	const std::span<uint8_t> mapped_host_memory {};
	std::shared_ptr<device_buffer> constant_buffer_storage;
	const std::span<uint8_t> constant_buffer_mapping {};
	
};

} // namespace fl

#endif
