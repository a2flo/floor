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

#include <floor/core/essentials.hpp>

#if !defined(FLOOR_NO_OPENCL)
#define FLOOR_OPENCL_INFO_FUNCS 1
#include <floor/compute/opencl/opencl_program.hpp>
#include <floor/compute/opencl/opencl_kernel.hpp>

opencl_program::opencl_program(const cl_program program_,
							   const vector<llvm_compute::kernel_info>& kernels_info,
							   const bool use_cl_queries) : program(program_) {
	// create kernels (all in the program)
	const auto kernel_count = (use_cl_queries ? cl_get_info<CL_PROGRAM_NUM_KERNELS>(program) : kernels_info.size());
	if(kernel_count == 0) {
		log_error("no kernels in program!");
	}
	else {
		string kernel_names_str = "";
		if(!use_cl_queries) {
			for(const auto& kernel : kernels_info) {
				kernel_names_str += kernel.name + ";";
			}
		}
		else kernel_names_str = cl_get_info<CL_PROGRAM_KERNEL_NAMES>(program);
		log_debug("got %u kernels in program: %s", kernel_count, kernel_names_str);
		
		vector<cl_kernel> program_kernels(kernel_count);
		const auto kernel_err = clCreateKernelsInProgram(program, (cl_uint)kernel_count, &program_kernels[0], nullptr);
		if(kernel_err != CL_SUCCESS) {
			log_error("failed to create kernels for program: %u: %s", kernel_err, cl_error_to_string(kernel_err));
		}
		else {
			for(size_t i = 0; i < kernel_count; ++i) {
				const auto name = (use_cl_queries ? cl_get_info<CL_KERNEL_FUNCTION_NAME>(program_kernels[i]) : kernels_info[i].name);
				kernels.push_back(make_shared<opencl_kernel>(program_kernels[i], name));
				kernel_names.push_back(name);
			}
		}
	}
}

#endif
