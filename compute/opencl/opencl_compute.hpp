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

#include <floor/compute/opencl/opencl_common.hpp>

#if !defined(FLOOR_NO_OPENCL)

#include <floor/compute/compute_context.hpp>
#include <floor/compute/opencl/opencl_buffer.hpp>
#include <floor/compute/opencl/opencl_image.hpp>
#include <floor/compute/opencl/opencl_device.hpp>
#include <floor/compute/opencl/opencl_program.hpp>
#include <floor/compute/opencl/opencl_queue.hpp>
#include <floor/threading/atomic_spin_lock.hpp>

class opencl_compute final : public compute_context {
public:
	//////////////////////////////////////////
	// init / context creation
	
	opencl_compute(const COMPUTE_CONTEXT_FLAGS ctx_flags = COMPUTE_CONTEXT_FLAGS::NONE,
				   const bool has_toolchain_ = false,
				   const uint32_t platform_index = ~0u,
				   const vector<string> whitelist = {});
	
	~opencl_compute() override {}
	
	bool is_supported() const override { return supported; }
	
	bool is_graphics_supported() const override { return false; }
	
	COMPUTE_TYPE get_compute_type() const override { return COMPUTE_TYPE::OPENCL; }
	
	//////////////////////////////////////////
	// device functions
	
	shared_ptr<compute_queue> create_queue(const compute_device& dev) const override;
	
	const compute_queue* get_device_default_queue(const compute_device& dev) const override;
	
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
	
	//////////////////////////////////////////
	// image creation
	
	shared_ptr<compute_image> create_image(const compute_queue& cqueue,
										   const uint4 image_dim,
										   const COMPUTE_IMAGE_TYPE image_type,
										   std::span<uint8_t> data,
										   const COMPUTE_MEMORY_FLAG flags = (COMPUTE_MEMORY_FLAG::HOST_READ_WRITE),
										   const uint32_t mip_level_limit = 0u) const override;
	
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
															 const vector<llvm_toolchain::function_info>& functions) override REQUIRES(!programs_lock);
	
	//! NOTE: for internal purposes (not exposed by other backends)
	opencl_program::opencl_program_entry create_opencl_program(const compute_device& device,
															   llvm_toolchain::program_data program,
															   const llvm_toolchain::TARGET& target);
	
	//! NOTE: for internal purposes (not exposed by other backends)
	shared_ptr<opencl_program> add_program(opencl_program::program_map_type&& prog_map) REQUIRES(!programs_lock);
	
	shared_ptr<compute_program::program_entry> create_program_entry(const compute_device& device,
																	llvm_toolchain::program_data program,
																	const llvm_toolchain::TARGET target) override REQUIRES(!programs_lock);
	
	//////////////////////////////////////////
	// execution functionality
	
	unique_ptr<indirect_command_pipeline> create_indirect_command_pipeline(const indirect_command_description& desc) const override;
	
	//////////////////////////////////////////
	// opencl specific functions
	
	const cl_context& get_opencl_context() const {
		return ctx;
	}
	
	// for compat with clGetKernelSubGroupInfo(KHR) and misc sub-group extensions
	cl_int get_kernel_sub_group_info(cl_kernel kernel,
									 cl_device_id device,
									 cl_kernel_sub_group_info param_name,
									 size_t input_value_size,
									 const void* input_value,
									 size_t param_value_size,
									 void* param_value,
									 size_t* param_value_size_ret) const;
	
protected:
	cl_context ctx { nullptr };
	vector<cl_device_id> ctx_devices;
	
	mutable floor_core::flat_map<const compute_device*, shared_ptr<compute_queue>> default_queues;
	mutable floor_core::flat_map<const compute_device*, uint8_t> default_queues_user_accessed;
	
	OPENCL_VERSION platform_cl_version { OPENCL_VERSION::NONE };
	
	vector<cl_image_format> image_formats;
	
	atomic_spin_lock programs_lock;
	vector<shared_ptr<opencl_program>> programs GUARDED_BY(programs_lock);
	
	opencl_program::opencl_program_entry
	create_opencl_program_internal(const opencl_device& cl_dev,
								   const void* program_data,
								   const size_t& program_size,
								   const vector<llvm_toolchain::function_info>& functions,
								   const llvm_toolchain::TARGET& target,
								   const bool& silence_debug_output);
	
	shared_ptr<compute_program> create_program_from_archive_binaries(universal_binary::archive_binaries& bins) REQUIRES(!programs_lock);
	
	// either core clCreateProgramWithIL or extension clCreateProgramWithILKHR
	CL_API_CALL cl_program (*cl_create_program_with_il)(cl_context context,
														const void* il,
														size_t length,
														cl_int* errcode_ret) = nullptr;
	
	// either core clGetKernelSubGroupInfo or extension clGetKernelSubGroupInfoKHR
	CL_API_CALL cl_int (*cl_get_kernel_sub_group_info)(cl_kernel kernel,
													   cl_device_id device,
													   cl_kernel_sub_group_info param_name,
													   size_t input_value_size,
													   const void* input_value,
													   size_t param_value_size,
													   void* param_value,
													   size_t* param_value_size_ret) = nullptr;
	
};

#endif
