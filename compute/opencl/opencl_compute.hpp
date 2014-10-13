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

#ifndef __FLOOR_OPENCL_COMPUTE_HPP__
#define __FLOOR_OPENCL_COMPUTE_HPP__

#include <floor/core/essentials.hpp>

#if !defined(FLOOR_NO_OPENCL)

#include <floor/compute/compute_base.hpp>

#if defined(__APPLE__)
#if defined(FLOOR_IOS)
// don't let cl_gl_ext.h get included (it won't work anyways)
#define __OPENCL_CL_GL_EXT_H
#define __GCL_H
#endif
#include <OpenCL/OpenCL.h>
#include <OpenCL/cl.h>
#include <OpenCL/cl_platform.h>
#include <OpenCL/cl_ext.h>
#include <OpenCL/cl_gl.h>
#if !defined(FLOOR_IOS)
#include <OpenGL/CGLContext.h>
#include <OpenGL/CGLCurrent.h>
#include <OpenGL/CGLDevice.h>
#endif
#else
#include <CL/cl.h>
#include <CL/cl_platform.h>
#include <CL/cl_ext.h>
#include <CL/cl_gl.h>
#endif

#include <floor/compute/opencl/opencl_device.hpp>

//! opencl version of the platform/driver/device
enum class OPENCL_VERSION : uint32_t {
	OPENCL_1_0,
	OPENCL_1_1,
	OPENCL_1_2,
	OPENCL_2_0,
};

//! TODO
class opencl_compute final : compute_base {
public:
	//////////////////////////////////////////
	// init / context creation
	
	~opencl_compute() override {}
	
	//! initializes the compute context/object
	void init(const bool use_platform_devices = false,
			  const size_t platform_index = 0,
			  const bool gl_sharing = true,
			  const set<string> device_restriction = set<string> {}) override;
	
	//! returns true if there is compute support (i.e. a compute context could be created and available compute devices exist)
	bool is_supported() const override { return supported; }
	
	//////////////////////////////////////////
	// basic control functions
	
	//! block until all currently scheduled kernels have been executed
	void finish() override;
	
	//! flush all prior
	void flush() override;
	
	//! TODO: figure out how to do this in cuda
	//void barrier() override;
	
	//! makes the compute context active in the current thread
	void activate_context() override;
	
	//! makes the compute context inactive in the current thread
	void deactivate_context() override;
	
	//////////////////////////////////////////
	// kernel functionality
	
	//! adds and compiles a kernel from a file
	weak_ptr<compute_kernel> add_kernel_file(const string& file_name,
											 const string additional_options = "") override;
	
	//! adds and compiles a kernel from the provided source code
	weak_ptr<compute_kernel> add_kernel_source(const string& source_code,
											   const string additional_options = "") override;
	
	//! deletes a kernel object
	void delete_kernel(weak_ptr<compute_kernel> kernel) override;
	
	//! excutes the specified kernel with the specified arguments
	//! TODO: proper interface for this: 1) variadic template, 2) functor with preset args
	void execute_kernel(weak_ptr<compute_kernel> kernel) override;
	
protected:
	cl_context ctx { nullptr };
	vector<cl_device_id> ctx_devices;
	
	OPENCL_VERSION platform_cl_version { OPENCL_VERSION::OPENCL_1_0 };
	
	vector<cl_image_format> image_formats;
	
};

#endif

#endif
