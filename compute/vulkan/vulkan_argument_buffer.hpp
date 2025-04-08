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

#include <floor/compute/argument_buffer.hpp>

#if !defined(FLOOR_NO_VULKAN)
#include <floor/core/aligned_ptr.hpp>
#include <floor/compute/vulkan/vulkan_kernel.hpp>
#include <floor/compute/vulkan/vulkan_descriptor_set.hpp>

class vulkan_argument_buffer : public argument_buffer {
public:
	vulkan_argument_buffer(const compute_kernel& func_,
						   shared_ptr<compute_buffer> storage_buffer,
						   const llvm_toolchain::function_info& arg_info,
						   const vulkan_descriptor_set_layout_t& layout_,
						   vector<VkDeviceSize>&& argument_offsets_,
						   span<uint8_t> mapped_host_memory_,
						   shared_ptr<compute_buffer> constant_buffer_storage_,
						   span<uint8_t> constant_buffer_mapping_);
	
	bool set_arguments(const compute_queue& dev_queue, const vector<compute_kernel_arg>& args) override;
	
	void set_debug_label(const string& label) override;
	
protected:
	const llvm_toolchain::function_info& arg_info;
	const vulkan_descriptor_set_layout_t& layout;
	const vector<VkDeviceSize> argument_offsets;
	const span<uint8_t> mapped_host_memory {};
	shared_ptr<compute_buffer> constant_buffer_storage;
	const span<uint8_t> constant_buffer_mapping {};
	
};

#endif
