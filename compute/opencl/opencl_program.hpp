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

#ifndef __FLOOR_OPENCL_PROGRAM_HPP__
#define __FLOOR_OPENCL_PROGRAM_HPP__

#include <floor/compute/opencl/opencl_common.hpp>

#if !defined(FLOOR_NO_OPENCL)

#include <floor/compute/compute_program.hpp>
#include <floor/compute/llvm_compute.hpp>

FLOOR_PUSH_WARNINGS()
FLOOR_IGNORE_WARNING(weak-vtables)

class opencl_device;
class opencl_program final : public compute_program {
public:
	//! stores a opencl program + function infos for an individual device
	struct opencl_program_entry : program_entry {
		cl_program program { nullptr };
	};
	
	//! lookup map that contains the corresponding opencl program for multiple devices
	typedef flat_map<opencl_device*, opencl_program_entry> program_map_type;
	
	opencl_program(program_map_type&& programs);
	
protected:
	const program_map_type programs;
	
};

FLOOR_POP_WARNINGS()

#endif

#endif
