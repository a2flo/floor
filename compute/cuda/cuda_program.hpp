/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2020 Florian Ziesche
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
#include <floor/compute/llvm_toolchain.hpp>
#include <floor/core/flat_map.hpp>

FLOOR_PUSH_WARNINGS()
FLOOR_IGNORE_WARNING(weak-vtables)

class cuda_device;
class cuda_program final : public compute_program {
public:
	//! stores a cuda program + function infos for an individual device
	struct cuda_program_entry : program_entry {
		cu_module program { nullptr };
	};
	
	//! lookup map that contains the corresponding cuda program for multiple devices
	typedef flat_map<const cuda_device&, cuda_program_entry> program_map_type;
	
	cuda_program(program_map_type&& programs);
	
protected:
	const program_map_type programs;
	
};

FLOOR_POP_WARNINGS()

#endif

#endif
