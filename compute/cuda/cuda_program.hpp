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

#ifndef __FLOOR_CUDA_PROGRAM_HPP__
#define __FLOOR_CUDA_PROGRAM_HPP__

#include <floor/compute/cuda/cuda_common.hpp>

#if !defined(FLOOR_NO_CUDA)

#include <floor/compute/compute_program.hpp>
#include <floor/compute/llvm_compute.hpp>

FLOOR_PUSH_WARNINGS()
FLOOR_IGNORE_WARNING(weak-vtables)

class cuda_program final : public compute_program {
public:
	cuda_program(const CUmodule program, const vector<llvm_compute::kernel_info>& kernels_info);
	
protected:
	const CUmodule program;
	
};

FLOOR_POP_WARNINGS()

#endif

#endif
