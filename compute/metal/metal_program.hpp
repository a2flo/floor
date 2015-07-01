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

#ifndef __FLOOR_METAL_PROGRAM_HPP__
#define __FLOOR_METAL_PROGRAM_HPP__

#include <floor/compute/metal/metal_common.hpp>

#if !defined(FLOOR_NO_METAL)

#include <floor/compute/compute_program.hpp>
#include <Metal/Metal.h>

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wweak-vtables"
#endif

class metal_program final : public compute_program {
public:
	metal_program(const metal_device* device, id <MTLLibrary> program, const vector<llvm_compute::kernel_info>& kernels_info);
	
protected:
	id <MTLLibrary> program;
	
	struct metal_kernel_data {
		id <MTLFunction> kernel;
		id <MTLComputePipelineState> state;
	};
	vector<metal_kernel_data> metal_kernels;
	
};

#if defined(__clang__)
#pragma clang diagnostic pop
#endif

#endif

#endif
