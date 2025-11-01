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

#include <floor/device/cuda/cuda_common.hpp>

#if !defined(FLOOR_NO_CUDA)

#include <floor/device/device_context.hpp>
#include <floor/device/cuda/cuda_buffer.hpp>
#include <floor/device/cuda/cuda_image.hpp>
#include <floor/device/cuda/cuda_device.hpp>
#include <floor/device/cuda/cuda_program.hpp>
#include <floor/device/cuda/cuda_queue.hpp>
#include <floor/threading/atomic_spin_lock.hpp>

namespace fl {

class cuda_context final : public device_context {
public:
	//////////////////////////////////////////
	// init / context creation
	
	cuda_context(const DEVICE_CONTEXT_FLAGS ctx_flags = DEVICE_CONTEXT_FLAGS::NONE,
				 const bool has_toolchain_ = false, const std::vector<std::string> whitelist = {});
	
	~cuda_context() override {}
	
	bool is_supported() const override { return supported; }
	
	bool is_graphics_supported() const override { return false; }
	
	PLATFORM_TYPE get_platform_type() const override { return PLATFORM_TYPE::CUDA; }
	
	//////////////////////////////////////////
	// device functions
	
	std::shared_ptr<device_queue> create_queue(const device& dev) const override;
	
	const device_queue* get_device_default_queue(const device& dev) const override;
	
	std::unique_ptr<device_fence> create_fence(const device_queue& cqueue) const override;
	
	memory_usage_t get_memory_usage(const device& dev) const override;
	
	//////////////////////////////////////////
	// buffer creation
	
	std::shared_ptr<device_buffer> create_buffer(const device_queue& cqueue,
												 const size_t size,
												 const MEMORY_FLAG flags = (MEMORY_FLAG::READ_WRITE |
																			MEMORY_FLAG::HOST_READ_WRITE)) const override;
	
	std::shared_ptr<device_buffer> create_buffer(const device_queue& cqueue,
												 std::span<uint8_t> data,
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
															 const std::vector<toolchain::function_info>& functions) override REQUIRES(!programs_lock);
	
	//! NOTE: for internal purposes (not exposed by other backends)
	cuda_program::cuda_program_entry create_cuda_program(const cuda_device& dev,
														 toolchain::program_data program);
	
	//! NOTE: for internal purposes (not exposed by other backends)
	std::shared_ptr<cuda_program> add_program(cuda_program::program_map_type&& prog_map) REQUIRES(!programs_lock);
	
	std::shared_ptr<device_program::program_entry> create_program_entry(const device& dev,
																	toolchain::program_data program,
																	const toolchain::TARGET target) override REQUIRES(!programs_lock);
	
	//////////////////////////////////////////
	// execution functionality
	
	// nop
	
	//////////////////////////////////////////
	// CUDA specific functions
	
	//! returns the CUDA driver API version
	uint32_t get_cuda_driver_version() const {
		return driver_version;
	}
	
	//! returns true if external memory can be used (i.e. Vulkan buffer/image sharing)
	bool can_use_external_memory() const {
		return has_external_memory_support;
	}
	
protected:
	atomic_spin_lock programs_lock;
	std::vector<std::shared_ptr<cuda_program>> programs GUARDED_BY(programs_lock);
	
	uint32_t driver_version { 0 };
	bool has_external_memory_support { false };
	
	fl::flat_map<const device*, std::shared_ptr<device_queue>> default_queues;
	
	cuda_program::cuda_program_entry create_cuda_program_internal(const cuda_device& dev,
																  const std::span<const uint8_t> program,
																  const std::vector<toolchain::function_info>& functions,
																  const uint32_t max_registers,
																  const bool silence_debug_output);
	
	std::shared_ptr<device_program> create_program_from_archive_binaries(universal_binary::archive_binaries& bins) REQUIRES(!programs_lock);
	
};

} // namespace fl

#endif
