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

#include <floor/compute/host/host_common.hpp>

#if !defined(FLOOR_NO_HOST_COMPUTE)

#include <floor/compute/compute_program.hpp>

class host_device;
class elf_binary;
class host_program final : public compute_program {
public:
	//! stores a host program
	struct host_program_entry : program_entry {
		shared_ptr<elf_binary> program;
	};
	
	//! lookup map that contains the corresponding host program for multiple devices
	typedef floor_core::flat_map<const host_device&, host_program_entry> program_map_type;
	
	host_program(const compute_device& device, program_map_type&& programs);
	
	//! NOTE: for non-host-device execution, this dynamically load / looks up "func_name" and adds it to "dynamic_kernel_names"
	shared_ptr<compute_kernel> get_kernel(const string_view& func_name) const override;
	
	//! return the kernels that were loaded dynamically (non-host-device execution only)
	const vector<unique_ptr<string>>& get_dynamic_kernel_names() const {
		return dynamic_kernel_names;
	}
	
protected:
	const compute_device& device;
	const program_map_type programs;
	const bool has_device_binary { false };
	mutable vector<unique_ptr<string>> dynamic_kernel_names;
	mutable vector<shared_ptr<compute_kernel>> dynamic_kernels;
	
};

#endif
