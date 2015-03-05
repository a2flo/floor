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

#include <floor/core/logger.hpp>
#include <floor/threading/atomic_spin_lock.hpp>
#include <floor/compute/opencl/opencl_buffer.hpp>

// the amount of macro voodoo is too damn high ...
#define FLOOR_OPENCL_KERNEL_IMPL 1
#include <floor/compute/compute_kernel.hpp>
#undef FLOOR_OPENCL_KERNEL_IMPL

class opencl_kernel final : public compute_kernel {
public:
	opencl_kernel(const cl_kernel kernel, const string& func_name);
	~opencl_kernel() override;
	
	template <typename... Args> void execute(compute_queue* queue,
											 const uint32_t work_dim,
											 const size3 global_work_size,
											 const size3 local_work_size,
											 Args&&... args) REQUIRES(!args_lock) {
		// need to make sure that only one thread is setting kernel arguments at a time
		GUARD(args_lock);
		
		// set and handle kernel arguments
		set_kernel_arguments<0>(forward<Args>(args)...);
		
		// run
		execute_internal(queue, work_dim, global_work_size, local_work_size);
	}
	
protected:
	const cl_kernel kernel;
	const string func_name;
	
	atomic_spin_lock args_lock;
	
	COMPUTE_TYPE get_compute_type() const override { return COMPUTE_TYPE::OPENCL; }
	
	void execute_internal(compute_queue* queue,
						  const uint32_t& work_dim,
						  const size3& global_work_size,
						  const size3& local_work_size);
	
	//! handle kernel call terminator
	template <cl_uint num>
	floor_inline_always void set_kernel_arguments() {}
	
	//! set kernel argument and recurse
	template <cl_uint num, typename T, typename... Args>
	floor_inline_always void set_kernel_arguments(T&& arg, Args&&... args) {
		set_kernel_argument(num, forward<T>(arg));
		set_kernel_arguments<num + 1>(forward<Args>(args)...);
	}
	
	//! actual kernel argument setter
	//! TODO: specialize for types (raw/pod types, images, ...)
	//! TODO: check for invalid args (e.g. raw pointers)
	template <typename T>
	floor_inline_always void set_kernel_argument(const cl_uint num, T&& arg) {
		CL_CALL_RET(clSetKernelArg(kernel, num, sizeof(T), &arg),
					"failed to set generic kernel argument");
	}
	
	floor_inline_always void set_kernel_argument(const cl_uint num, shared_ptr<compute_buffer> arg) {
		CL_CALL_RET(clSetKernelArg(kernel, num, sizeof(cl_mem), &((opencl_buffer*)arg.get())->get_cl_buffer()),
					"failed to set buffer kernel argument");
	}
	
};

#endif

#endif
