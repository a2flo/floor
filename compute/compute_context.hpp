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

#ifndef __FLOOR_COMPUTE_CONTEXT_HPP__
#define __FLOOR_COMPUTE_CONTEXT_HPP__

#include <unordered_set>

#include <floor/core/logger.hpp>
#include <floor/compute/llvm_compute.hpp>
#include <floor/compute/compute_common.hpp>
#include <floor/compute/compute_buffer.hpp>
#include <floor/compute/compute_image.hpp>
#include <floor/compute/compute_device.hpp>
#include <floor/compute/compute_kernel.hpp>
#include <floor/compute/compute_queue.hpp>
#include <floor/compute/compute_program.hpp>

// necessary here, because there are no out-of-line virtual method definitions
FLOOR_PUSH_WARNINGS()
FLOOR_IGNORE_WARNING(weak-vtables)

//! pure abstract base class that provides the interface for all compute implementations (opencl, cuda, ...)
class compute_context {
public:
	//////////////////////////////////////////
	// init / context creation
	
	//! unfortunately necessary, has empty body in .cpp
	virtual ~compute_context() = 0;
	
	//! returns true if there is compute support (i.e. a compute context could be created and available compute devices exist)
	virtual bool is_supported() const = 0;
	
	//! returns the underlying compute implementation type
	virtual COMPUTE_TYPE get_compute_type() const = 0;
	
	//////////////////////////////////////////
	// device functions
	
	//! returns the array of all valid devices in this context
	const vector<shared_ptr<compute_device>>& get_devices() const {
		return devices;
	}
	
	//! tries to return the device matching the specified "type"
	//! NOTE: will return any valid device if none matches "type"!
	shared_ptr<compute_device> get_device(const compute_device::TYPE type) const;
	
	//! creates and returns a compute_queue (aka command queue or stream) for the specified device
	virtual shared_ptr<compute_queue> create_queue(shared_ptr<compute_device> dev) = 0;
	
	//////////////////////////////////////////
	// buffer creation
	
	//! constructs an uninitialized buffer of the specified size on the specified device
	virtual shared_ptr<compute_buffer> create_buffer(shared_ptr<compute_device> device,
													 const size_t& size,
													 const COMPUTE_MEMORY_FLAG flags = (COMPUTE_MEMORY_FLAG::READ_WRITE |
																						COMPUTE_MEMORY_FLAG::HOST_READ_WRITE),
													 const uint32_t opengl_type = 0) = 0;
	
	//! constructs a buffer of the specified size, using the host pointer as specified by the flags on the specified device
	virtual shared_ptr<compute_buffer> create_buffer(shared_ptr<compute_device> device,
													 const size_t& size,
													 void* data,
													 const COMPUTE_MEMORY_FLAG flags = (COMPUTE_MEMORY_FLAG::READ_WRITE |
																						COMPUTE_MEMORY_FLAG::HOST_READ_WRITE),
													 const uint32_t opengl_type = 0) = 0;
	
	//! constructs a buffer of the specified data (under consideration of the specified flags) on the specified device
	template <typename data_type>
	shared_ptr<compute_buffer> create_buffer(shared_ptr<compute_device> device,
											 const vector<data_type>& data,
											 const COMPUTE_MEMORY_FLAG flags = (COMPUTE_MEMORY_FLAG::READ_WRITE |
																				COMPUTE_MEMORY_FLAG::HOST_READ_WRITE),
											 const uint32_t opengl_type = 0) {
		return create_buffer(device, sizeof(data_type) * data.size(), (void*)&data[0], flags, opengl_type);
	}
	
	//! constructs a buffer of the specified data (under consideration of the specified flags) on the specified device
	template <typename data_type, size_t n>
	shared_ptr<compute_buffer> create_buffer(shared_ptr<compute_device> device,
											 const array<data_type, n>& data,
											 const COMPUTE_MEMORY_FLAG flags = (COMPUTE_MEMORY_FLAG::READ_WRITE |
																				COMPUTE_MEMORY_FLAG::HOST_READ_WRITE),
											 const uint32_t opengl_type = 0) {
		return create_buffer(device, sizeof(data_type) * n, (void*)&data[0], flags, opengl_type);
	}
	
	//! wraps an already existing opengl buffer, with the specified flags
	//! NOTE: OPENGL_SHARING flag is always implied
	virtual shared_ptr<compute_buffer> wrap_buffer(shared_ptr<compute_device> device,
												   const uint32_t opengl_buffer,
												   const uint32_t opengl_type,
												   const COMPUTE_MEMORY_FLAG flags = (COMPUTE_MEMORY_FLAG::READ_WRITE |
																					  COMPUTE_MEMORY_FLAG::HOST_READ_WRITE)) = 0;
	
	//! wraps an already existing opengl buffer, with the specified flags and backed by the specified host pointer
	//! NOTE: OPENGL_SHARING flag is always implied
	virtual shared_ptr<compute_buffer> wrap_buffer(shared_ptr<compute_device> device,
												   const uint32_t opengl_buffer,
												   const uint32_t opengl_type,
												   void* data,
												   const COMPUTE_MEMORY_FLAG flags = (COMPUTE_MEMORY_FLAG::READ_WRITE |
																					  COMPUTE_MEMORY_FLAG::HOST_READ_WRITE)) = 0;
	
	//////////////////////////////////////////
	// image creation
	
	//! constructs an uninitialized image of the specified dimensions, types and channel count on the specified device
	virtual shared_ptr<compute_image> create_image(shared_ptr<compute_device> device,
												   const uint4 image_dim,
												   const COMPUTE_IMAGE_TYPE image_type,
												   const COMPUTE_MEMORY_FLAG flags = (COMPUTE_MEMORY_FLAG::HOST_READ_WRITE),
												   const uint32_t opengl_type = 0) = 0;
	
	//! constructs an image of the specified dimensions, types and channel count on the specified device
	virtual shared_ptr<compute_image> create_image(shared_ptr<compute_device> device,
												   const uint4 image_dim,
												   const COMPUTE_IMAGE_TYPE image_type,
												   void* data,
												   const COMPUTE_MEMORY_FLAG flags = (COMPUTE_MEMORY_FLAG::HOST_READ_WRITE),
												   const uint32_t opengl_type = 0) = 0;
	
	//! constructs an image of the specified dimensions, types and channel count, with the specified data on the specified device
	template <typename data_type>
	shared_ptr<compute_image> create_image(shared_ptr<compute_device> device,
										   const uint4 image_dim,
										   const COMPUTE_IMAGE_TYPE image_type,
										   const vector<data_type>& data,
										   const COMPUTE_MEMORY_FLAG flags = (COMPUTE_MEMORY_FLAG::HOST_READ_WRITE),
										   const uint32_t opengl_type = 0) {
		return create_image(device, image_dim, image_type, &data[0], flags, opengl_type);
	}
	
	//! constructs an image of the specified dimensions, types and channel count, with the specified data on the specified device
	template <typename data_type, size_t n>
	shared_ptr<compute_image> create_image(shared_ptr<compute_device> device,
										   const uint4 image_dim,
										   const COMPUTE_IMAGE_TYPE image_type,
										   const array<data_type, n>& data,
										   const COMPUTE_MEMORY_FLAG flags = (COMPUTE_MEMORY_FLAG::HOST_READ_WRITE),
										   const uint32_t opengl_type = 0) {
		return create_image(device, image_dim, image_type, &data[0], flags, opengl_type);
	}
	
	//! wraps an already existing opengl image, with the specified flags
	//! NOTE: OPENGL_SHARING flag is always implied
	virtual shared_ptr<compute_image> wrap_image(shared_ptr<compute_device> device,
												 const uint32_t opengl_image,
												 const uint32_t opengl_target,
												 const COMPUTE_MEMORY_FLAG flags = (COMPUTE_MEMORY_FLAG::READ_WRITE |
																					COMPUTE_MEMORY_FLAG::HOST_READ_WRITE)) = 0;
	
	//! wraps an already existing opengl image, with the specified flags and backed by the specified host pointer
	//! NOTE: OPENGL_SHARING flag is always implied
	virtual shared_ptr<compute_image> wrap_image(shared_ptr<compute_device> device,
												 const uint32_t opengl_image,
												 const uint32_t opengl_target,
												 void* data,
												 const COMPUTE_MEMORY_FLAG flags = (COMPUTE_MEMORY_FLAG::READ_WRITE |
																					COMPUTE_MEMORY_FLAG::HOST_READ_WRITE)) = 0;
	
	// TODO: add is_image_format_supported(...) function
	
	//////////////////////////////////////////
	// program/function functionality
	
	//! alias the llvm_compute compile_options (for now)
	using compile_options = llvm_compute::compile_options;
	
	//! adds and compiles a program and its functions from a file
	virtual shared_ptr<compute_program> add_program_file(const string& file_name,
														 const string additional_options) = 0;
	
	//! adds and compiles a program and its functions from a file
	virtual shared_ptr<compute_program> add_program_file(const string& file_name,
														 compile_options options = {}) = 0;
	
	//! adds and compiles a program and its functions from the provided source code
	virtual shared_ptr<compute_program> add_program_source(const string& source_code,
														   const string additional_options = "") = 0;
	
	//! adds and compiles a program and its functions from the provided source code
	virtual shared_ptr<compute_program> add_program_source(const string& source_code,
														   compile_options options = {}) = 0;
	
	//! adds a precompiled program and its functions, using the provided file name and function infos
	virtual shared_ptr<compute_program> add_precompiled_program_file(const string& file_name,
																	 const vector<llvm_compute::function_info>& functions) = 0;
	
	//! creates a program entry from pre-existing program data and function information on the specified device
	//! NOTE: this is intended for rolling custom or semi-custom compilation, for normal code use the add_program_* functions
	//! NOTE: this usually leads to final program compilation on most compute platforms (but not all!)
	virtual shared_ptr<compute_program::program_entry> create_program_entry(shared_ptr<compute_device> device,
																			llvm_compute::program_data program,
																			const llvm_compute::TARGET target) = 0;
	
protected:
	//! platform vendor enum (set after initialization)
	COMPUTE_VENDOR platform_vendor { COMPUTE_VENDOR::UNKNOWN };
	
	//! true if compute support (set after initialization)
	bool supported { false };
	
	//! all compute devices of the current compute context
	vector<shared_ptr<compute_device>> devices;
	//! pointer to the fastest (any) compute_device if it exists
	shared_ptr<compute_device> fastest_device;
	//! pointer to the fastest cpu compute_device if it exists
	shared_ptr<compute_device> fastest_cpu_device;
	//! pointer to the fastest gpu compute_device if it exists
	shared_ptr<compute_device> fastest_gpu_device;
	
	//! all compute queues of the current compute context
	vector<shared_ptr<compute_queue>> queues;
	
};

FLOOR_POP_WARNINGS()

#endif
