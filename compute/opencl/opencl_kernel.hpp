/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2015 Florian Ziesche
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

#ifndef __FLOOR_OPENCL_KERNEL_HPP__
#define __FLOOR_OPENCL_KERNEL_HPP__

#include <floor/compute/opencl/opencl_common.hpp>

#if !defined(FLOOR_NO_OPENCL)

// the amount of macro voodoo is too damn high ...
#define FLOOR_OPENCL_KERNEL_IMPL 1
#include <floor/compute/compute_kernel.hpp>
#undef FLOOR_OPENCL_KERNEL_IMPL

#include <floor/threading/atomic_spin_lock.hpp>

// TODO: !
class opencl_kernel final : public compute_kernel {
public:
	opencl_kernel(const cl_kernel& kernel, const string& func_name);
	~opencl_kernel() override;
	
	template <typename... Args> void execute(const cl_command_queue queue,
											 const uint32_t work_dim,
											 const size3 global_work_size,
											 const size3 local_work_size,
											 Args&&... args) {
		// need to make sure that only one thread is setting kernel arguments at a time
		GUARD(args_lock);
		
		// set and handle kernel arguments
		// TODO: use clSetKernelArgsListAPPLE(kernel, sizeof...(Args), forward<Args>(args)...) if available
		set_kernel_arguments<0>(forward<Args>(args)...);
		
		// run
		clEnqueueNDRangeKernel(queue, kernel, work_dim, nullptr, global_work_size.data(), local_work_size.data(),
							   // TODO: use of event stuff?
							   0, nullptr, nullptr);
	}
	
protected:
	const cl_kernel kernel;
	const string func_name;
	
	atomic_spin_lock args_lock;
	
	COMPUTE_TYPE get_compute_type() const override { return COMPUTE_TYPE::OPENCL; }
	
	//! handle empty kernel call
	template <cl_uint num, enable_if_t<num == 0, int> = 0>
	floor_inline_always void set_kernel_arguments() {}
	
	//! set kernel argument and recurse
	template <cl_uint num, typename T, typename... Args>
	floor_inline_always void set_kernel_arguments(T&& arg, Args&&... args) {
		set_kernel_argument(num, forward<T>(arg));
		set_kernel_arguments<num + 1>(forward<Args>(args)...);
	}
	
	//! actual kernel argument setter
	//! TODO: specialize for types (raw/pod types, buffers, images, ...)
	//! TODO: check for invalid args (e.g. raw pointers)
	template <typename T>
	floor_inline_always void set_kernel_argument(const cl_uint num, T&& arg) {
		clSetKernelArg(kernel, num, sizeof(T), &arg);
	}
	
};

#endif

#endif
