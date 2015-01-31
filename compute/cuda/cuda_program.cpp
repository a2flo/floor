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

#define FLOOR_CUDA_INFO_FUNCS 1
#include <floor/compute/cuda/cuda_program.hpp>

#if !defined(FLOOR_NO_CUDA)

#include <floor/compute/cuda/cuda_kernel.hpp>

cuda_program::cuda_program(const CUmodule program_) : program(program_) {
	// TODO: create kernels (all in the program)
	/*for(const auto& func : function_mappings) {
		CUfunction kernel;
		CU_CALL_CONT(cuModuleGetFunction(&kernel, program, func.first.c_str()),
					 "failed to get function " + func.first);
		kernels.push_back(make_shared<cuda_kernel>(kernel, name));
		kernel_names.push_back(name);
	}*/
}

#endif
