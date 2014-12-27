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

#ifndef __FLOOR_CUDA_COMPUTE_HPP__
#define __FLOOR_CUDA_COMPUTE_HPP__

#include <floor/core/essentials.hpp>

#if !defined(FLOOR_NO_CUDA)

#include <floor/compute/compute_base.hpp>

class cuda_compute final : compute_base {
public:
	//////////////////////////////////////////
	// init / context creation
	
	~cuda_compute() override {}
	
	//! initializes the compute context/object
	void init(const bool use_platform_devices = false,
			  const size_t platform_index = 0,
			  const bool gl_sharing = true,
			  const unordered_set<string> device_restriction = {}) override;
	
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
	// program/kernel functionality
	
	//! adds and compiles a program and its kernels from a file
	weak_ptr<compute_program> add_program_file(const string& file_name,
											   const string additional_options = "") override;
	
	//! adds and compiles a program and its kernels from the provided source code
	weak_ptr<compute_program> add_program_source(const string& source_code,
												 const string additional_options = "") override;
	
	//! excutes the specified kernel with the specified arguments
	//! TODO: proper interface for this: 1) variadic template, 2) functor with preset args
	void execute_kernel(weak_ptr<compute_kernel> kernel) override;
	
};

#endif

#endif
