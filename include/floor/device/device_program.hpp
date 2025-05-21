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

#include <string>
#include <vector>
#include <floor/math/vector_lib.hpp>
#include <floor/device/toolchain.hpp>
#include <floor/device/universal_binary.hpp>
#include <floor/core/flat_map.hpp>

namespace fl {

class device_function;
class device_program {
public:
	device_program(std::vector<std::string>&& function_names_) noexcept : function_names(std::move(function_names_)) {}
	virtual ~device_program() = 0;
	
	//! returns the function with the exact function name of "func_name", nullptr if not found
	virtual std::shared_ptr<device_function> get_function(const std::string_view& func_name) const;
	
	//! returns a container of all functions in this program
	const std::vector<std::shared_ptr<device_function>>& get_functions() const {
		return functions;
	}
	
	//! returns a container of all function function names in this program
	const std::vector<std::string>& get_function_names() const {
		return function_names;
	}
	
	//! stores a program + function infos for an individual device
	struct program_entry {
		//! only non-nullptr for backends that need to keep the archive memory around
		std::shared_ptr<universal_binary::archive> archive;
		
		std::vector<toolchain::function_info> functions;
		bool valid { false };
	};
	
protected:
	std::vector<std::shared_ptr<device_function>> functions;
	const std::vector<std::string> function_names;
	
	template <typename device_type, typename program_entry_type>
	static std::vector<std::string> retrieve_unique_function_names(const fl::flat_map<device_type, program_entry_type>& programs) {
		// go through all functions in all device programs and create a unique list of all function names
		std::vector<std::string> names;
		for (const auto& prog : programs) {
			if (!prog.second.valid) continue;
			for (const auto& info : prog.second.functions) {
				names.push_back(info.name);
			}
		}
		names.erase(unique(begin(names), end(names)), end(names));
		return names;
	}
	
	//! returns true if the specified function (info) should be ignored for the specified device,
	//! i.e. the function has requirements that the device can't fulfill
	bool should_ignore_function_for_device(const device& dev, const toolchain::function_info& func_info) const;
	
};

} // namespace fl
