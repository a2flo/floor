/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2014 Florian Ziesche
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

#ifndef __FLOOR_COMPUTE_BASE_HPP__
#define __FLOOR_COMPUTE_BASE_HPP__

#include <floor/core/essentials.hpp>

#if !defined(FLOOR_NO_OPENCL) && !defined(FLOOR_NO_CUDA_CL)

// TODO: implement these
class compute_kernel;
class compute_buffer;
class compute_device;

//! pure abstract base class that provides the interface for all compute implementations (opencl, cuda, ...)
class compute_base {
public:
	//////////////////////////////////////////
	// init / context creation
	
	//! unfortunately necessary, has empty body in .cpp
	virtual ~compute_base() = 0;
	
	//! initializes the compute context/object
	virtual void init(const bool use_platform_devices = false,
					  const size_t platform_index = 0,
					  const bool gl_sharing = true,
					  const set<string> device_restriction = set<string> {}) = 0;
	
	//////////////////////////////////////////
	// enums
	
	//! device types for device selection
	enum class DEVICE_TYPE : uint32_t {
		NONE,
		FASTEST_GPU,
		FASTEST_CPU,
		ALL_GPU,
		ALL_CPU,
		ALL_DEVICES,
		GPU0,
		GPU1,
		GPU2,
		GPU4,
		GPU5,
		GPU6,
		GPU7,
		GPU255 = GPU0+255,
		CPU0,
		CPU1,
		CPU2,
		CPU3,
		CPU4,
		CPU5,
		CPU6,
		CPU7,
		CPU255 = CPU0+255
	};
	
	//! opencl and cuda platform vendors
	enum class PLATFORM_VENDOR : uint32_t {
		NVIDIA,
		INTEL,
		AMD,
		APPLE,
		FREEOCL,
		POCL,
		CUDA,
		UNKNOWN
	};
	
	//! opencl and cuda device vendors
	enum class DEVICE_VENDOR : uint32_t {
		NVIDIA,
		INTEL,
		AMD,
		APPLE,
		FREEOCL,
		POCL,
		UNKNOWN
	};
	
	//////////////////////////////////////////
	// basic control functions
	
	//! block until all currently scheduled kernels have been executed
	virtual void finish() = 0;
	
	//! flush all prior
	virtual void flush() = 0;
	
	//! TODO: figure out how to do this in cuda
	//virtual void barrier() = 0;
	
	//! makes the compute context active in the current thread
	virtual void activate_context() = 0;
	
	//! makes the compute context inactive in the current thread
	virtual void deactivate_context() = 0;
	
	//////////////////////////////////////////
	// kernel functionality
	
	//! adds and compiles a kernel from a file
	virtual weak_ptr<compute_kernel> add_kernel_file(const string& file_name,
													 const string additional_options = "") = 0;
	
	//! adds and compiles a kernel from the provided source code
	virtual weak_ptr<compute_kernel> add_kernel_source(const string& source_code,
													   const string additional_options = "") = 0;
	
	//! deletes a kernel object
	virtual void delete_kernel(weak_ptr<compute_kernel> kernel) = 0;
	
	//! excutes the specified kernel with the specified arguments
	//! TODO: proper interface for this: 1) variadic template, 2) functor with preset args
	virtual void execute_kernel(weak_ptr<compute_kernel> kernel) = 0;
	
protected:
	
};

#endif

#endif
