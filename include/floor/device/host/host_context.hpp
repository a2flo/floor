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

#include <floor/device/host/host_common.hpp>

#if !defined(FLOOR_NO_HOST_COMPUTE)

#include <floor/device/device_context.hpp>
#include <floor/device/host/host_buffer.hpp>
#include <floor/device/host/host_image.hpp>
#include <floor/device/host/host_device.hpp>
#include <floor/device/host/host_program.hpp>
#include <floor/device/host/host_queue.hpp>
#include <floor/threading/atomic_spin_lock.hpp>

namespace fl {

class host_context final : public device_context {
public:
	//////////////////////////////////////////
	// init / context creation
	
	host_context(const DEVICE_CONTEXT_FLAGS ctx_flags = DEVICE_CONTEXT_FLAGS::NONE,
				 const bool has_toolchain_ = false);
	
	~host_context() override {}
	
	bool is_supported() const override { return supported; }
	
	bool is_graphics_supported() const override { return false; }
	
	PLATFORM_TYPE get_platform_type() const override { return PLATFORM_TYPE::HOST; }
	
	//////////////////////////////////////////
	// device functions
	
	std::shared_ptr<device_queue> create_queue(const device& dev) const override;
	
	const device_queue* get_device_default_queue(const device&) const override {
		return main_queue.get();
	}
	
	std::optional<uint32_t> get_max_distinct_queue_count(const device& dev) const override;
	
	std::optional<uint32_t> get_max_distinct_compute_queue_count(const device& dev) const override;
	
	std::vector<std::shared_ptr<device_queue>> create_distinct_queues(const device& dev, const uint32_t wanted_count) const override;
	
	std::vector<std::shared_ptr<device_queue>> create_distinct_compute_queues(const device& dev, const uint32_t wanted_count) const override;
	
	std::unique_ptr<device_fence> create_fence(const device_queue& cqueue) const override;
	
	memory_usage_t get_memory_usage(const device& dev) const override;
	
	//////////////////////////////////////////
	// buffer creation
	
	std::shared_ptr<device_buffer> create_buffer(const device_queue& cqueue,
											 const size_t& size,
											 const MEMORY_FLAG flags = (MEMORY_FLAG::READ_WRITE |
																				MEMORY_FLAG::HOST_READ_WRITE)) const override;
	
	std::shared_ptr<device_buffer> create_buffer(const device_queue& cqueue,
											 std::span<uint8_t> data,
											 const MEMORY_FLAG flags = (MEMORY_FLAG::READ_WRITE |
																				MEMORY_FLAG::HOST_READ_WRITE)) const override;
	
	std::shared_ptr<device_buffer> wrap_buffer(const device_queue& cqueue,
										   metal_buffer& mtl_buffer,
										   const MEMORY_FLAG flags = (MEMORY_FLAG::READ_WRITE |
																			  MEMORY_FLAG::HOST_READ_WRITE)) const override;
	std::shared_ptr<device_buffer> wrap_buffer(const device_queue& cqueue,
										   vulkan_buffer& vk_buffer,
										   const MEMORY_FLAG flags = (MEMORY_FLAG::READ_WRITE |
																			  MEMORY_FLAG::HOST_READ_WRITE)) const override;
	
	//////////////////////////////////////////
	// image creation
	
	std::shared_ptr<device_image> create_image(const device_queue& cqueue,
										   const uint4 image_dim,
										   const IMAGE_TYPE image_type,
										   std::span<uint8_t> data,
										   const MEMORY_FLAG flags = (MEMORY_FLAG::HOST_READ_WRITE),
										   const uint32_t mip_level_limit = 0u) const override;
	
	std::shared_ptr<device_image> wrap_image(const device_queue& cqueue,
										 metal_image& mtl_image,
										 const MEMORY_FLAG flags = (MEMORY_FLAG::READ_WRITE |
																			MEMORY_FLAG::HOST_READ_WRITE)) const override;
	
	std::shared_ptr<device_image> wrap_image(const device_queue& cqueue,
										 vulkan_image& vk_image,
										 const MEMORY_FLAG flags = (MEMORY_FLAG::READ_WRITE |
																			MEMORY_FLAG::HOST_READ_WRITE)) const override;
	
	//////////////////////////////////////////
	// program/function functionality
	
	std::shared_ptr<device_program> add_universal_binary(const std::string& file_name) override REQUIRES(!programs_lock);
	
	std::shared_ptr<device_program> add_universal_binary(const std::span<const uint8_t> data) override REQUIRES(!programs_lock);
	
	std::shared_ptr<device_program> add_program_file(const std::string& file_name,
												 const std::string additional_options) override REQUIRES(!programs_lock);
	
	std::shared_ptr<device_program> add_program_file(const std::string& file_name,
												 compile_options options = {}) override REQUIRES(!programs_lock);
	
	std::shared_ptr<device_program> add_program_source(const std::string& source_code,
												   const std::string additional_options) override REQUIRES(!programs_lock);
	
	std::shared_ptr<device_program> add_program_source(const std::string& source_code,
												   compile_options options = {}) override REQUIRES(!programs_lock);
	
	std::shared_ptr<device_program> add_precompiled_program_file(const std::string& file_name,
															 const std::vector<toolchain::function_info>& functions) override;
	
	//! NOTE: for internal purposes (not exposed by other backends)
	host_program::host_program_entry create_host_program(const host_device& dev,
														 toolchain::program_data program);
	
	//! NOTE: for internal purposes (not exposed by other backends)
	std::shared_ptr<host_program> add_program(host_program::program_map_type&& prog_map) REQUIRES(!programs_lock);
	
	std::shared_ptr<device_program::program_entry> create_program_entry(const device& dev,
																	toolchain::program_data program,
																	const toolchain::TARGET target) override;
	
	//////////////////////////////////////////
	// execution functionality
	
	std::unique_ptr<indirect_command_pipeline> create_indirect_command_pipeline(const indirect_command_description& desc) const override;
	
	//////////////////////////////////////////
	// host specific functions
	
	//! returns true if Host-Compute device support is available
	bool has_host_device_support() const;
	
protected:
	atomic_spin_lock programs_lock;
	std::vector<std::shared_ptr<host_program>> programs GUARDED_BY(programs_lock);
	
	std::shared_ptr<device_queue> main_queue;
	
	host_program::host_program_entry create_host_program_internal(const host_device& dev,
																  const std::optional<std::string> elf_bin_file_name,
																  const uint8_t* elf_bin_data,
																  const size_t elf_bin_size,
																  const std::vector<toolchain::function_info>& functions,
																  const bool& silence_debug_output);
	
	std::shared_ptr<device_program> create_program_from_archive_binaries(universal_binary::archive_binaries& bins) REQUIRES(!programs_lock);
	
};

} // namespace fl

#endif
