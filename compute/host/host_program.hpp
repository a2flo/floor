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

#ifndef __FLOOR_HOST_PROGRAM_HPP__
#define __FLOOR_HOST_PROGRAM_HPP__

#include <floor/compute/host/host_common.hpp>

#if !defined(FLOOR_NO_HOST_COMPUTE)

#include <floor/compute/compute_program.hpp>

class host_device;
class host_program final : public compute_program {
public:
	//! stores a host program
	struct host_program_entry : program_entry {
		// TODO
	};
	
	//! lookup map that contains the corresponding host program for multiple devices
	typedef flat_map<const host_device&, host_program_entry> program_map_type;
	
	host_program(const compute_device& device, program_map_type&& programs);
	
	shared_ptr<compute_kernel> get_kernel(const string& func_name) const override;
	
protected:
	const compute_device& device;
	const program_map_type programs;
	
};

#endif

#endif
