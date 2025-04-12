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

#include <floor/compute/host/host_common.hpp>

#if !defined(FLOOR_NO_HOST_COMPUTE)

#include <floor/compute/compute_context.hpp>
#include <floor/compute/host/host_buffer.hpp>
#include <floor/compute/host/host_image.hpp>
#include <floor/compute/host/host_device.hpp>
#include <floor/compute/host/host_program.hpp>
#include <floor/compute/host/host_queue.hpp>
#include <floor/threading/atomic_spin_lock.hpp>

class host_compute final : public compute_context {
public:
	//////////////////////////////////////////
	// init / context creation
	
	host_compute(const COMPUTE_CONTEXT_FLAGS ctx_flags = COMPUTE_CONTEXT_FLAGS::NONE,
				 const bool has_toolchain_ = false);
	
	~host_compute() override {}
	
	bool is_supported() const override { return supported; }
	
	bool is_graphics_supported() const override { return false; }
	
	COMPUTE_TYPE get_compute_type() const override { return COMPUTE_TYPE::HOST; }
	
	//////////////////////////////////////////
	// device functions
	
	shared_ptr<compute_queue> create_queue(const compute_device& dev) const override;
	
	const compute_queue* get_device_default_queue(const compute_device&) const override {
		return main_queue.get();
	}
	
	optional<uint32_t> get_max_distinct_queue_count(const compute_device& dev) const override;
	
	optional<uint32_t> get_max_distinct_compute_queue_count(const compute_device& dev) const override;
	
	vector<shared_ptr<compute_queue>> create_distinct_queues(const compute_device& dev, const uint32_t wanted_count) const override;
	
	vector<shared_ptr<compute_queue>> create_distinct_compute_queues(const compute_device& dev, const uint32_t wanted_count) const override;
	
	unique_ptr<compute_fence> create_fence(const compute_queue& cqueue) const override;
	
	memory_usage_t get_memory_usage(const compute_device& dev) const override;
	
	//////////////////////////////////////////
	// buffer creation
	
	shared_ptr<compute_buffer> create_buffer(const compute_queue& cqueue,
											 const size_t& size,
											 const COMPUTE_MEMORY_FLAG flags = (COMPUTE_MEMORY_FLAG::READ_WRITE |
																				COMPUTE_MEMORY_FLAG::HOST_READ_WRITE)) const override;
	
	shared_ptr<compute_buffer> create_buffer(const compute_queue& cqueue,
											 std::span<uint8_t> data,
											 const COMPUTE_MEMORY_FLAG flags = (COMPUTE_MEMORY_FLAG::READ_WRITE |
																				COMPUTE_MEMORY_FLAG::HOST_READ_WRITE)) const override;
	
	shared_ptr<compute_buffer> wrap_buffer(const compute_queue& cqueue,
										   metal_buffer& mtl_buffer,
										   const COMPUTE_MEMORY_FLAG flags = (COMPUTE_MEMORY_FLAG::READ_WRITE |
																			  COMPUTE_MEMORY_FLAG::HOST_READ_WRITE)) const override;
	shared_ptr<compute_buffer> wrap_buffer(const compute_queue& cqueue,
										   vulkan_buffer& vk_buffer,
										   const COMPUTE_MEMORY_FLAG flags = (COMPUTE_MEMORY_FLAG::READ_WRITE |
																			  COMPUTE_MEMORY_FLAG::HOST_READ_WRITE)) const override;
	
	//////////////////////////////////////////
	// image creation
	
	shared_ptr<compute_image> create_image(const compute_queue& cqueue,
										   const uint4 image_dim,
										   const COMPUTE_IMAGE_TYPE image_type,
										   std::span<uint8_t> data,
										   const COMPUTE_MEMORY_FLAG flags = (COMPUTE_MEMORY_FLAG::HOST_READ_WRITE),
										   const uint32_t mip_level_limit = 0u) const override;
	
	shared_ptr<compute_image> wrap_image(const compute_queue& cqueue,
										 metal_image& mtl_image,
										 const COMPUTE_MEMORY_FLAG flags = (COMPUTE_MEMORY_FLAG::READ_WRITE |
																			COMPUTE_MEMORY_FLAG::HOST_READ_WRITE)) const override;
	
	shared_ptr<compute_image> wrap_image(const compute_queue& cqueue,
										 vulkan_image& vk_image,
										 const COMPUTE_MEMORY_FLAG flags = (COMPUTE_MEMORY_FLAG::READ_WRITE |
																			COMPUTE_MEMORY_FLAG::HOST_READ_WRITE)) const override;
	
	//////////////////////////////////////////
	// program/kernel functionality
	
	shared_ptr<compute_program> add_universal_binary(const string& file_name) override REQUIRES(!programs_lock);
	
	shared_ptr<compute_program> add_universal_binary(const span<const uint8_t> data) override REQUIRES(!programs_lock);
	
	shared_ptr<compute_program> add_program_file(const string& file_name,
												 const string additional_options) override REQUIRES(!programs_lock);
	
	shared_ptr<compute_program> add_program_file(const string& file_name,
												 compile_options options = {}) override REQUIRES(!programs_lock);
	
	shared_ptr<compute_program> add_program_source(const string& source_code,
												   const string additional_options) override REQUIRES(!programs_lock);
	
	shared_ptr<compute_program> add_program_source(const string& source_code,
												   compile_options options = {}) override REQUIRES(!programs_lock);
	
	shared_ptr<compute_program> add_precompiled_program_file(const string& file_name,
															 const vector<llvm_toolchain::function_info>& functions) override;
	
	//! NOTE: for internal purposes (not exposed by other backends)
	host_program::host_program_entry create_host_program(const host_device& device,
														 llvm_toolchain::program_data program);
	
	//! NOTE: for internal purposes (not exposed by other backends)
	shared_ptr<host_program> add_program(host_program::program_map_type&& prog_map) REQUIRES(!programs_lock);
	
	shared_ptr<compute_program::program_entry> create_program_entry(const compute_device& device,
																	llvm_toolchain::program_data program,
																	const llvm_toolchain::TARGET target) override;
	
	//////////////////////////////////////////
	// execution functionality
	
	unique_ptr<indirect_command_pipeline> create_indirect_command_pipeline(const indirect_command_description& desc) const override;
	
	//////////////////////////////////////////
	// host specific functions
	
	//! returns true if host-compute device support is available
	bool has_host_device_support() const;
	
protected:
	atomic_spin_lock programs_lock;
	vector<shared_ptr<host_program>> programs GUARDED_BY(programs_lock);
	
	shared_ptr<compute_queue> main_queue;
	
	host_program::host_program_entry create_host_program_internal(const host_device& device,
																  const optional<string> elf_bin_file_name,
																  const uint8_t* elf_bin_data,
																  const size_t elf_bin_size,
																  const vector<llvm_toolchain::function_info>& functions,
																  const bool& silence_debug_output);
	
	shared_ptr<compute_program> create_program_from_archive_binaries(universal_binary::archive_binaries& bins) REQUIRES(!programs_lock);
	
};

#endif
