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

#include <floor/device/host/host_common.hpp>

#if !defined(FLOOR_NO_HOST_COMPUTE)

#include <floor/device/device_program.hpp>

namespace fl {

class host_device;
class elf_binary;
class host_program final : public device_program {
public:
	//! stores a host program
	struct host_program_entry : program_entry {
		std::shared_ptr<elf_binary> program;
	};
	
	//! lookup map that contains the corresponding host program for multiple devices
	using program_map_type = fl::flat_map<const host_device*, host_program_entry>;
	
	host_program(const device& dev, program_map_type&& programs);
	
	//! NOTE: for non-host-device execution, this dynamically load / looks up "func_name" and adds it to "dynamic_function_names"
	std::shared_ptr<device_function> get_function(const std::string_view& func_name) const override;
	
	//! return the functions that were loaded dynamically (non-host-device execution only)
	const std::vector<std::unique_ptr<std::string>>& get_dynamic_function_names() const {
		return dynamic_function_names;
	}
	
protected:
	const device& dev;
	const program_map_type programs;
	const bool has_device_binary { false };
	mutable std::vector<std::unique_ptr<std::string>> dynamic_function_names;
	mutable std::vector<std::shared_ptr<device_function>> dynamic_functions;
	
};

} // namespace fl

#endif
