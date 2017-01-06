/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2017 Florian Ziesche
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

#ifndef __FLOOR_METAL_COMPUTE_HPP__
#define __FLOOR_METAL_COMPUTE_HPP__

#include <floor/compute/metal/metal_common.hpp>

#if !defined(FLOOR_NO_METAL)

#include <floor/compute/compute_context.hpp>
class metal_program;

class metal_compute final : public compute_context {
public:
	//////////////////////////////////////////
	// init / context creation
	
	metal_compute(const vector<string> whitelist = {});
	
	~metal_compute() override {}
	
	bool is_supported() const override { return supported; }
	
	COMPUTE_TYPE get_compute_type() const override { return COMPUTE_TYPE::METAL; }
	
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
															 const vector<llvm_toolchain::function_info>& functions) override REQUIRES(!programs_lock);
	
	shared_ptr<compute_program::program_entry> create_program_entry(shared_ptr<compute_device> device,
																	llvm_toolchain::program_data program,
																	const llvm_toolchain::TARGET target) override REQUIRES(!programs_lock);
	
	//////////////////////////////////////////
	// metal specific functions
	
	shared_ptr<compute_queue> get_device_internal_queue(shared_ptr<compute_device> dev) const;
	shared_ptr<compute_queue> get_device_internal_queue(const compute_device* dev) const;
	
	//! for debugging/testing purposes only (circumvents the internal program handling)
	shared_ptr<compute_program> create_metal_test_program(shared_ptr<compute_program::program_entry> entry);
	
protected:
	void* ctx { nullptr };
	
	vector<pair<shared_ptr<compute_device>, shared_ptr<compute_queue>>> internal_queues;
	
	atomic_spin_lock programs_lock;
	vector<shared_ptr<metal_program>> programs GUARDED_BY(programs_lock);
	
};

#endif

#endif
