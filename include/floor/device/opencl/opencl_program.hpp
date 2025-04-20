/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2025 Florian Ziesche
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

#include <floor/device/opencl/opencl_common.hpp>

#if !defined(FLOOR_NO_OPENCL)

#include <floor/device/device_program.hpp>
#include <floor/device/toolchain.hpp>

namespace fl {

FLOOR_PUSH_WARNINGS()
FLOOR_IGNORE_WARNING(weak-vtables)

class opencl_device;
class opencl_program final : public device_program {
public:
	//! stores a OpenCL program + function infos for an individual device
	struct opencl_program_entry : program_entry {
		cl_program program { nullptr };
	};
	
	//! lookup map that contains the corresponding OpenCL program for multiple devices
	using program_map_type = fl::flat_map<const opencl_device*, opencl_program_entry>;
	
	opencl_program(program_map_type&& programs);
	
protected:
	const program_map_type programs;
	
};

FLOOR_POP_WARNINGS()

} // namespace fl

#endif
