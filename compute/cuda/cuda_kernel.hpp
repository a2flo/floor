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

#ifndef __FLOOR_CUDA_KERNEL_HPP__
#define __FLOOR_CUDA_KERNEL_HPP__

#include <floor/compute/cuda/cuda_common.hpp>

#if !defined(FLOOR_NO_CUDA)

#include <floor/core/logger.hpp>
#include <floor/threading/atomic_spin_lock.hpp>
#include <floor/compute/cuda/cuda_buffer.hpp>

// the amount of macro voodoo is too damn high ...
#define FLOOR_CUDA_KERNEL_IMPL 1
#include <floor/compute/compute_kernel.hpp>
#undef FLOOR_CUDA_KERNEL_IMPL

// TODO: !
class cuda_kernel final : public compute_kernel {
public:
	cuda_kernel(const CUfunction kernel, const string& func_name);
	~cuda_kernel() override;
	
	template <typename... Args> void execute(const CUstream queue,
											 const uint32_t work_dim,
											 const size3 global_work_size,
											 const size3 local_work_size,
											 Args&&... args) {
		// need to make sure that only one thread is setting kernel arguments at a time
		GUARD(args_lock);
		
		// set and handle kernel arguments
		set_kernel_arguments<0>(forward<Args>(args)...);
		
		// TODO: run
		/*const auto exec_error = clEnqueueNDRangeKernel(queue, kernel, work_dim, nullptr,
													   global_work_size.data(), local_work_size.data(),
													   // TODO: use of event stuff?
													   0, nullptr, nullptr);
		if(exec_error != CL_SUCCESS) {
			log_error("failed to execute kernel: %u!", exec_error);
		}*/
	}
	
protected:
	const CUfunction kernel;
	const string func_name;
	
	atomic_spin_lock args_lock;
	
	COMPUTE_TYPE get_compute_type() const override { return COMPUTE_TYPE::CUDA; }
	
	//! handle kernel call terminator
	template <uint32_t num>
	floor_inline_always void set_kernel_arguments() {}
	
	//! set kernel argument and recurse
	template <uint32_t num, typename T, typename... Args>
	floor_inline_always void set_kernel_arguments(T&& arg, Args&&... args) {
		set_kernel_argument(num, forward<T>(arg));
		set_kernel_arguments<num + 1>(forward<Args>(args)...);
	}
	
	//! actual kernel argument setter
	//! TODO: specialize for types (raw/pod types, images, ...)
	//! TODO: check for invalid args (e.g. raw pointers)
	template <typename T>
	floor_inline_always void set_kernel_argument(const uint32_t num, T&& arg) {
		// TODO: !
		/*CL_CALL_RET(clSetKernelArg(kernel, num, sizeof(T), &arg),
					"failed to set generic kernel argument");*/
	}
	
	floor_inline_always void set_kernel_argument(const uint32_t num, shared_ptr<compute_buffer> arg) {
		// TODO: !
		/*CL_CALL_RET(clSetKernelArg(kernel, num, sizeof(cl_mem), &((cuda_buffer*)arg.get())->get_cl_buffer()),
					"failed to set buffer kernel argument");*/
	}
	
};

#endif

#endif
