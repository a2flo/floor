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

#ifndef __FLOOR_COMPUTE_KERNEL_HPP__
#define __FLOOR_COMPUTE_KERNEL_HPP__

#include <string>
#include <vector>
#include <floor/math/vector_lib.hpp>
#include <floor/compute/compute_common.hpp>

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wweak-vtables"
#endif

// TODO: !
class compute_kernel {
public:
	virtual ~compute_kernel() = 0;
	
	//! don't call this directly, call the execute function in a compute_queue object instead!
	template <typename... Args, class work_size_type> void execute(const void* queue_ptr,
																   work_size_type&& global_work_size,
																   work_size_type&& local_work_size,
																   Args&&... args);
	
protected:
	//! same as the one in compute_base, but this way we don't need access to that object
	virtual COMPUTE_TYPE get_compute_type() const = 0;
	
};

//#include <floor/compute/cuda/cuda_kernel.hpp>
#include <floor/compute/opencl/opencl_kernel.hpp>

#if !defined(FLOOR_OPENCL_KERNEL_IMPL) && !defined(FLOOR_CUDA_KERNEL_IMPL)
// forwarder to the actual kernel classes (disabled when included by them)
template <typename... Args, class work_size_type> void compute_kernel::execute(const void* queue_ptr,
																			   work_size_type&& global_work_size,
																			   work_size_type&& local_work_size,
																			   Args&&... args) {
	// get around the nightmare of the non-existence of virtual (variadic) template member functions ...
	switch(get_compute_type()) {
		case COMPUTE_TYPE::CUDA:
#if !defined(FLOOR_NO_CUDA)
			//static_cast<cuda_kernel*>(this)->execute((CUStream)queue_ptr, forward<Args>(args)...);
#endif // else: nop
			break;
		case COMPUTE_TYPE::OPENCL:
#if !defined(FLOOR_NO_OPENCL)
			static_cast<opencl_kernel*>(this)->execute((cl_command_queue)queue_ptr,
													   decay_t<work_size_type>::dim,
													   size3 { global_work_size },
													   size3 { local_work_size },
													   forward<Args>(args)...);
#endif // else: nop
			break;
	}
}
#endif

#if defined(__clang__)
#pragma clang diagnostic pop
#endif

#endif
