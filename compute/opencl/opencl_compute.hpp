/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2016 Florian Ziesche
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

#ifndef __FLOOR_OPENCL_COMPUTE_HPP__
#define __FLOOR_OPENCL_COMPUTE_HPP__

#include <floor/compute/opencl/opencl_common.hpp>

#if !defined(FLOOR_NO_OPENCL)

#include <floor/compute/compute_context.hpp>
#include <floor/compute/opencl/opencl_buffer.hpp>
#include <floor/compute/opencl/opencl_image.hpp>
#include <floor/compute/opencl/opencl_device.hpp>
#include <floor/compute/opencl/opencl_kernel.hpp>
#include <floor/compute/opencl/opencl_program.hpp>
#include <floor/compute/opencl/opencl_queue.hpp>

class opencl_compute final : public compute_context {
public:
	//////////////////////////////////////////
	// init / context creation
	
	opencl_compute(const uint32_t platform_index = ~0u,
				   const bool gl_sharing = false,
				   const vector<string> whitelist = {});
	
	~opencl_compute() override {}
	
	bool is_supported() const override { return supported; }
	
	COMPUTE_TYPE get_compute_type() const override { return COMPUTE_TYPE::OPENCL; }
	
	//////////////////////////////////////////
	// device functions
	
	shared_ptr<compute_queue> create_queue(shared_ptr<compute_device> dev) override;
	
	//////////////////////////////////////////
	// buffer creation
	
	shared_ptr<compute_buffer> create_buffer(shared_ptr<compute_device> device,
											 const size_t& size,
											 const COMPUTE_MEMORY_FLAG flags = (COMPUTE_MEMORY_FLAG::READ_WRITE |
																				COMPUTE_MEMORY_FLAG::HOST_READ_WRITE),
											 const uint32_t opengl_type = 0) override;
	
	shared_ptr<compute_buffer> create_buffer(shared_ptr<compute_device> device,
											 const size_t& size,
											 void* data,
											 const COMPUTE_MEMORY_FLAG flags = (COMPUTE_MEMORY_FLAG::READ_WRITE |
																				COMPUTE_MEMORY_FLAG::HOST_READ_WRITE),
											 const uint32_t opengl_type = 0) override;
	
	shared_ptr<compute_buffer> wrap_buffer(shared_ptr<compute_device> device,
										   const uint32_t opengl_buffer,
										   const uint32_t opengl_type,
										   const COMPUTE_MEMORY_FLAG flags = (COMPUTE_MEMORY_FLAG::READ_WRITE |
																			  COMPUTE_MEMORY_FLAG::HOST_READ_WRITE)) override;
	
	shared_ptr<compute_buffer> wrap_buffer(shared_ptr<compute_device> device,
										   const uint32_t opengl_buffer,
										   const uint32_t opengl_type,
										   void* data,
										   const COMPUTE_MEMORY_FLAG flags = (COMPUTE_MEMORY_FLAG::READ_WRITE |
																			  COMPUTE_MEMORY_FLAG::HOST_READ_WRITE)) override;
	
	//////////////////////////////////////////
	// image creation
	
	shared_ptr<compute_image> create_image(shared_ptr<compute_device> device,
										   const uint4 image_dim,
										   const COMPUTE_IMAGE_TYPE image_type,
										   const COMPUTE_MEMORY_FLAG flags = (COMPUTE_MEMORY_FLAG::HOST_READ_WRITE),
										   const uint32_t opengl_type = 0) override;
	
	shared_ptr<compute_image> create_image(shared_ptr<compute_device> device,
										   const uint4 image_dim,
										   const COMPUTE_IMAGE_TYPE image_type,
										   void* data,
										   const COMPUTE_MEMORY_FLAG flags = (COMPUTE_MEMORY_FLAG::HOST_READ_WRITE),
										   const uint32_t opengl_type = 0) override;
	
	shared_ptr<compute_image> wrap_image(shared_ptr<compute_device> device,
										 const uint32_t opengl_image,
										 const uint32_t opengl_target,
										 const COMPUTE_MEMORY_FLAG flags = (COMPUTE_MEMORY_FLAG::READ_WRITE |
																			COMPUTE_MEMORY_FLAG::HOST_READ_WRITE)) override;
	
	shared_ptr<compute_image> wrap_image(shared_ptr<compute_device> device,
										 const uint32_t opengl_image,
										 const uint32_t opengl_target,
										 void* data,
										 const COMPUTE_MEMORY_FLAG flags = (COMPUTE_MEMORY_FLAG::READ_WRITE |
																			COMPUTE_MEMORY_FLAG::HOST_READ_WRITE)) override;
	
	//////////////////////////////////////////
	// program/kernel functionality
	
	shared_ptr<compute_program> add_program_file(const string& file_name,
												 const string additional_options) override REQUIRES(!programs_lock);
	
	shared_ptr<compute_program> add_program_file(const string& file_name,
												 compile_options options = {}) override REQUIRES(!programs_lock);
	
	shared_ptr<compute_program> add_program_source(const string& source_code,
												   const string additional_options) override REQUIRES(!programs_lock);
	
	shared_ptr<compute_program> add_program_source(const string& source_code,
												   compile_options options = {}) override REQUIRES(!programs_lock);
	
	shared_ptr<compute_program> add_precompiled_program_file(const string& file_name,
															 const vector<llvm_compute::function_info>& functions) override REQUIRES(!programs_lock);
	
	//! NOTE: for internal purposes (not exposed by other backends)
	opencl_program::opencl_program_entry create_opencl_program(shared_ptr<compute_device> device,
															   llvm_compute::program_data program,
															   const llvm_compute::TARGET& target);
	
	//! NOTE: for internal purposes (not exposed by other backends)
	shared_ptr<opencl_program> add_program(opencl_program::program_map_type&& prog_map) REQUIRES(!programs_lock);
	
	shared_ptr<compute_program::program_entry> create_program_entry(shared_ptr<compute_device> device,
																	llvm_compute::program_data program,
																	const llvm_compute::TARGET target) override REQUIRES(!programs_lock);
	
	//////////////////////////////////////////
	// opencl specific functions
	
	const cl_context& get_opencl_context() const {
		return ctx;
	}
	
	shared_ptr<compute_queue> get_device_default_queue(shared_ptr<compute_device> dev) const;
	shared_ptr<compute_queue> get_device_default_queue(const compute_device* dev) const;
	
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
	
	vector<pair<shared_ptr<compute_device>, shared_ptr<opencl_queue>>> default_queues;
	unordered_map<shared_ptr<compute_device>, bool> default_queues_user_accessed;
	
	OPENCL_VERSION platform_cl_version { OPENCL_VERSION::NONE };
	
	vector<cl_image_format> image_formats;
	
	atomic_spin_lock programs_lock;
	vector<shared_ptr<opencl_program>> programs GUARDED_BY(programs_lock);
	
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

#endif
