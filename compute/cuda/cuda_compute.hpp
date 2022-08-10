/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2022 Florian Ziesche
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

#ifndef __FLOOR_CUDA_COMPUTE_HPP__
#define __FLOOR_CUDA_COMPUTE_HPP__

#include <floor/compute/cuda/cuda_common.hpp>

#if !defined(FLOOR_NO_CUDA)

#include <floor/compute/compute_context.hpp>
#include <floor/compute/cuda/cuda_buffer.hpp>
#include <floor/compute/cuda/cuda_image.hpp>
#include <floor/compute/cuda/cuda_device.hpp>
#include <floor/compute/cuda/cuda_program.hpp>
#include <floor/compute/cuda/cuda_queue.hpp>
#include <floor/threading/atomic_spin_lock.hpp>

class cuda_compute final : public compute_context {
public:
	//////////////////////////////////////////
	// init / context creation
	
	cuda_compute(const vector<string> whitelist = {});
	
	~cuda_compute() override {}
	
	bool is_supported() const override { return supported; }
	
	bool is_graphics_supported() const override { return false; }
	
	COMPUTE_TYPE get_compute_type() const override { return COMPUTE_TYPE::CUDA; }
	
	//////////////////////////////////////////
	// device functions
	
	shared_ptr<compute_queue> create_queue(const compute_device& dev) const override;
	
	const compute_queue* get_device_default_queue(const compute_device& dev) const override;
	
	unique_ptr<compute_fence> create_fence(const compute_queue& cqueue) const override;
	
	//////////////////////////////////////////
	// buffer creation
	
	shared_ptr<compute_buffer> create_buffer(const compute_queue& cqueue,
											 const size_t& size,
											 const COMPUTE_MEMORY_FLAG flags = (COMPUTE_MEMORY_FLAG::READ_WRITE |
																				COMPUTE_MEMORY_FLAG::HOST_READ_WRITE),
											 const uint32_t opengl_type = 0) const override;
	
	shared_ptr<compute_buffer> create_buffer(const compute_queue& cqueue,
											 const size_t& size,
											 void* data,
											 const COMPUTE_MEMORY_FLAG flags = (COMPUTE_MEMORY_FLAG::READ_WRITE |
																				COMPUTE_MEMORY_FLAG::HOST_READ_WRITE),
											 const uint32_t opengl_type = 0) const override;
	
	shared_ptr<compute_buffer> wrap_buffer(const compute_queue& cqueue,
										   const uint32_t opengl_buffer,
										   const uint32_t opengl_type,
										   const COMPUTE_MEMORY_FLAG flags = (COMPUTE_MEMORY_FLAG::READ_WRITE |
																			  COMPUTE_MEMORY_FLAG::HOST_READ_WRITE)) const override;
	
	shared_ptr<compute_buffer> wrap_buffer(const compute_queue& cqueue,
										   const uint32_t opengl_buffer,
										   const uint32_t opengl_type,
										   void* data,
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
										   const COMPUTE_MEMORY_FLAG flags = (COMPUTE_MEMORY_FLAG::HOST_READ_WRITE),
										   const uint32_t opengl_type = 0) const override;
	
	shared_ptr<compute_image> create_image(const compute_queue& cqueue,
										   const uint4 image_dim,
										   const COMPUTE_IMAGE_TYPE image_type,
										   void* data,
										   const COMPUTE_MEMORY_FLAG flags = (COMPUTE_MEMORY_FLAG::HOST_READ_WRITE),
										   const uint32_t opengl_type = 0) const override;
	
	shared_ptr<compute_image> wrap_image(const compute_queue& cqueue,
										 const uint32_t opengl_image,
										 const uint32_t opengl_target,
										 const COMPUTE_MEMORY_FLAG flags = (COMPUTE_MEMORY_FLAG::READ_WRITE |
																			COMPUTE_MEMORY_FLAG::HOST_READ_WRITE)) const override;
	
	shared_ptr<compute_image> wrap_image(const compute_queue& cqueue,
										 const uint32_t opengl_image,
										 const uint32_t opengl_target,
										 void* data,
										 const COMPUTE_MEMORY_FLAG flags = (COMPUTE_MEMORY_FLAG::READ_WRITE |
																			COMPUTE_MEMORY_FLAG::HOST_READ_WRITE)) const override;
	
	shared_ptr<compute_image> wrap_image(const compute_queue& cqueue,
										 vulkan_image& vk_image,
										 const COMPUTE_MEMORY_FLAG flags = (COMPUTE_MEMORY_FLAG::READ_WRITE |
																			COMPUTE_MEMORY_FLAG::HOST_READ_WRITE)) const override;
	
	//////////////////////////////////////////
	// program/kernel functionality
	
	shared_ptr<compute_program> add_universal_binary(const string& file_name) override REQUIRES(!programs_lock);
	
	shared_ptr<compute_program> add_program_file(const string& file_name,
												 const string additional_options) override REQUIRES(!programs_lock);
	
	shared_ptr<compute_program> add_program_file(const string& file_name,
												 compile_options options = {}) override REQUIRES(!programs_lock);
	
	shared_ptr<compute_program> add_program_source(const string& source_code,
												   const string additional_options) override REQUIRES(!programs_lock);
	
	shared_ptr<compute_program> add_program_source(const string& source_code,
												   compile_options options = {}) override REQUIRES(!programs_lock);
	
	shared_ptr<compute_program> add_precompiled_program_file(const string& file_name,
															 const vector<llvm_toolchain::function_info>& functions) override REQUIRES(!programs_lock);
	
	//! NOTE: for internal purposes (not exposed by other backends)
	cuda_program::cuda_program_entry create_cuda_program(const cuda_device& device,
														 llvm_toolchain::program_data program);
	
	//! NOTE: for internal purposes (not exposed by other backends)
	shared_ptr<cuda_program> add_program(cuda_program::program_map_type&& prog_map) REQUIRES(!programs_lock);
	
	shared_ptr<compute_program::program_entry> create_program_entry(const compute_device& device,
																	llvm_toolchain::program_data program,
																	const llvm_toolchain::TARGET target) override REQUIRES(!programs_lock);
	
	//////////////////////////////////////////
	// execution functionality
	
	unique_ptr<indirect_command_pipeline> create_indirect_command_pipeline(const indirect_command_description& desc) const override;
	
	//////////////////////////////////////////
	// cuda specific functions
	
	//! returns the cuda driver api version
	uint32_t get_cuda_driver_version() const {
		return driver_version;
	}
	
	//! returns true if external memory can be used (i.e. Vulkan buffer/image sharing)
	bool can_use_external_memory() const {
		return has_external_memory_support;
	}
	
protected:
	atomic_spin_lock programs_lock;
	vector<shared_ptr<cuda_program>> programs GUARDED_BY(programs_lock);
	
	uint32_t driver_version { 0 };
	bool has_external_memory_support { false };
	
	floor_core::flat_map<const compute_device&, shared_ptr<compute_queue>> default_queues;
	
	cuda_program::cuda_program_entry create_cuda_program_internal(const cuda_device& device,
																  const void* program_data,
																  const size_t& program_size,
																  const vector<llvm_toolchain::function_info>& functions,
																  const uint32_t& max_registers,
																  const bool& silence_debug_output);
	
};

#endif

#endif
