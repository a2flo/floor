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

#pragma once

#include <floor/device/opencl/opencl_common.hpp>

#if !defined(FLOOR_NO_OPENCL)

#include <floor/device/device_context.hpp>
#include <floor/device/opencl/opencl_buffer.hpp>
#include <floor/device/opencl/opencl_image.hpp>
#include <floor/device/opencl/opencl_device.hpp>
#include <floor/device/opencl/opencl_program.hpp>
#include <floor/device/opencl/opencl_queue.hpp>
#include <floor/threading/atomic_spin_lock.hpp>

namespace fl {

class opencl_context final : public device_context {
public:
	//////////////////////////////////////////
	// init / context creation
	
	opencl_context(const DEVICE_CONTEXT_FLAGS ctx_flags = DEVICE_CONTEXT_FLAGS::NONE,
				   const bool has_toolchain_ = false,
				   const uint32_t platform_index = ~0u,
				   const std::vector<std::string> whitelist = {});
	
	~opencl_context() override {}
	
	bool is_supported() const override { return supported; }
	
	bool is_graphics_supported() const override { return false; }
	
	PLATFORM_TYPE get_platform_type() const override { return PLATFORM_TYPE::OPENCL; }
	
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
	
	//////////////////////////////////////////
	// image creation
	
	std::shared_ptr<device_image> create_image(const device_queue& cqueue,
										   const uint4 image_dim,
										   const IMAGE_TYPE image_type,
										   std::span<uint8_t> data,
										   const MEMORY_FLAG flags = (MEMORY_FLAG::HOST_READ_WRITE),
										   const uint32_t mip_level_limit = 0u) const override;
	
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
	opencl_program::opencl_program_entry create_opencl_program(const device& dev,
															   toolchain::program_data program,
															   const toolchain::TARGET& target);
	
	//! NOTE: for internal purposes (not exposed by other backends)
	std::shared_ptr<opencl_program> add_program(opencl_program::program_map_type&& prog_map) REQUIRES(!programs_lock);
	
	std::shared_ptr<device_program::program_entry> create_program_entry(const device& dev,
																	toolchain::program_data program,
																	const toolchain::TARGET target) override REQUIRES(!programs_lock);
	
	//////////////////////////////////////////
	// execution functionality
	
	// nop
	
	//////////////////////////////////////////
	// OpenCL specific functions
	
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
	std::vector<cl_device_id> ctx_devices;
	
	mutable fl::flat_map<const device*, std::shared_ptr<device_queue>> default_queues;
	mutable fl::flat_map<const device*, uint8_t> default_queues_user_accessed;
	
	OPENCL_VERSION platform_cl_version { OPENCL_VERSION::NONE };
	
	std::vector<cl_image_format> image_formats;
	
	atomic_spin_lock programs_lock;
	std::vector<std::shared_ptr<opencl_program>> programs GUARDED_BY(programs_lock);
	
	opencl_program::opencl_program_entry
	create_opencl_program_internal(const opencl_device& cl_dev,
								   const void* program_data,
								   const size_t& program_size,
								   const std::vector<toolchain::function_info>& functions,
								   const toolchain::TARGET& target,
								   const bool& silence_debug_output);
	
	std::shared_ptr<device_program> create_program_from_archive_binaries(universal_binary::archive_binaries& bins) REQUIRES(!programs_lock);
	
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

} // namespace fl

#endif
