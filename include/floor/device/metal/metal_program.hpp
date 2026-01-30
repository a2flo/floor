/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2026 Florian Ziesche
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

#pragma once

#include <floor/device/metal/metal_common.hpp>

#if !defined(FLOOR_NO_METAL)

#include <floor/device/device_program.hpp>
#include <Metal/Metal.h>

namespace fl {

FLOOR_PUSH_WARNINGS()
FLOOR_IGNORE_WARNING(weak-vtables)

class metal_device;

class metal_program final : public device_program {
public:
	//! stores a Metal program + function infos for an individual device
	struct metal_program_entry : program_entry {
		id <MTLLibrary> program { nil };
		
		struct metal_function_data {
			id <MTLFunction> function { nil };
			id <MTLComputePipelineState> state { nil };
		};
		//! internal state, automatically created in metal_program
		std::vector<metal_function_data> metal_functions {};
	};
	
	//! lookup map that contains the corresponding Metal program for multiple devices
	using program_map_type = fl::flat_map<const metal_device*, metal_program_entry>;
	
	metal_program(program_map_type&& programs);
	
#if defined(__OBJC__)
	//! dumps the specified reflection info (bindings) to console
	static void dump_bindings_reflection(const std::string& reflection_info_name, NSArray<id<MTLBinding>>* bindings);
#endif
	
protected:
	program_map_type programs;
	
};

FLOOR_POP_WARNINGS()

} // namespace fl

#endif
