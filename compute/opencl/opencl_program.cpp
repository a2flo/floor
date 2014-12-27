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

#define FLOOR_OPENCL_INFO_FUNCS 1
#include <floor/compute/opencl/opencl_program.hpp>

#if !defined(FLOOR_NO_OPENCL)

#include <floor/compute/opencl/opencl_kernel.hpp>

opencl_program::opencl_program(const cl_program& program_) : program(program_) {
	// create kernels (all in the program)
	const auto kernel_count = cl_get_info<CL_PROGRAM_NUM_KERNELS>(program);
	if(kernel_count == 0) {
		log_error("no kernels in program!");
		log_debug("check: %s", cl_get_info<CL_PROGRAM_KERNEL_NAMES>(program));
		cl_uint kernel_count_new = 0;
		clCreateKernelsInProgram(program, 0, nullptr, &kernel_count_new);
		log_debug("second kernel count: %d", kernel_count_new);
	}
	else {
		log_debug("got %u kernels in program: %s", kernel_count, cl_get_info<CL_PROGRAM_KERNEL_NAMES>(program));
		
		vector<cl_kernel> program_kernels(kernel_count);
		const auto kernel_err = clCreateKernelsInProgram(program, (cl_uint)kernel_count, &program_kernels[0], nullptr);
		if(kernel_err != CL_SUCCESS) {
			log_error("failed to create kernels for program: %u", kernel_err);
		}
		else {
			for(const auto& kernel : program_kernels) {
				kernels.push_back(make_shared<opencl_kernel>(kernel));
			}
		}
	}
}

opencl_program::~opencl_program() {}

#endif
