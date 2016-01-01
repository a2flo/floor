/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2016 Florian Ziesche
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

FLOOR_PUSH_WARNINGS()
FLOOR_IGNORE_WARNING(weak-vtables)

class metal_program final : public compute_program {
public:
	//! stores a metal program + kernel infos for an individual device
	struct metal_program_entry : program_entry {
		id <MTLLibrary> program;
		
		struct metal_kernel_data {
			id <MTLFunction> kernel;
			id <MTLComputePipelineState> state;
		};
		//! internal state, automatically created in metal_program
		vector<metal_kernel_data> metal_kernels {};
	};
	
	//! lookup map that contains the corresponding metal program for multiple devices
	typedef flat_map<metal_device*, metal_program_entry> program_map_type;
	
	metal_program(program_map_type&& programs);
	
protected:
	program_map_type programs;
	
};

FLOOR_POP_WARNINGS()

#endif

#endif
