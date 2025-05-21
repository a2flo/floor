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

#include <floor/core/essentials.hpp>

#if !defined(FLOOR_NO_HOST_COMPUTE)
#include <floor/core/logger.hpp>
#include <floor/device/host/host_program.hpp>
#include <floor/device/host/host_function.hpp>
#include <floor/device/host/host_device.hpp>
#include <floor/device/host/elf_binary.hpp>
#include <floor/device/backend/host_limits.hpp>

#if !defined(__WINDOWS__)
#include <dlfcn.h>
#else
#include <floor/core/platform_windows.hpp>
#include <floor/core/essentials.hpp> // cleanup
static HMODULE exe_module { nullptr };
#endif

namespace fl {

host_program::host_program(const device& dev_, program_map_type&& programs_) :
device_program(retrieve_unique_function_names(programs_)), dev(dev_), programs(std::move(programs_)), has_device_binary(!programs.empty()) {
	if (programs.empty()) {
		return;
	}
	
	// create all functions of all device programs
	// note that this essentially reshuffles the program "device -> functions" data to "functions -> devices"
	functions.reserve(function_names.size());
	for (const auto& function_name : function_names) {
		host_function::function_map_type function_map;
		for (auto&& prog : programs) {
			if (!prog.second.valid || !prog.second.program) {
				continue;
			}
			
			const auto& function_names = prog.second.program->get_function_names();
			if (function_names.empty()) {
				continue;
			}
			
			for (const auto& info : prog.second.functions) {
				if (info.name != function_name) {
					continue;
				}
				
				const auto func_iter = find_if(function_names.begin(), function_names.end(), [&function_name](const auto& entry) {
					return (entry == function_name);
				});
				if (func_iter == function_names.end()) {
					continue;
				}
				
				if (should_ignore_function_for_device(*prog.first, info)) {
					continue;
				}
				
				host_function::host_function_entry entry;
				entry.info = &info;
				entry.program = prog.second.program;
				if (info.has_valid_required_local_size()) {
					const auto local_size_extent = info.required_local_size.extent();
					if (local_size_extent > host_limits::max_total_local_size) {
						log_error("function $ required local size extent of $ is larger than the max supported local size of $",
								  info.name, local_size_extent, host_limits::max_total_local_size);
						continue;
					}
					entry.max_local_size = info.required_local_size;
					entry.max_total_local_size = local_size_extent;
				} else {
					// else: just assume the device/global default
					entry.max_local_size = prog.first->max_local_size;
					entry.max_total_local_size = host_limits::max_total_local_size;
				}
				
				if (info.has_valid_required_simd_width()) {
					entry.required_simd_width = info.required_simd_width;
				}
				
				// success, insert into map
				function_map.insert_or_assign(prog.first, entry);
				break;
			}
		}
		
		functions.emplace_back(std::make_shared<host_function>(function_name, std::move(function_map)));
	}
}

std::shared_ptr<device_function> host_program::get_function(const std::string_view& func_name) const {
	if (has_device_binary) {
		return device_program::get_function(func_name);
	}
	
	const auto iter = find_if(cbegin(dynamic_function_names), cend(dynamic_function_names), [&func_name](const auto& name) {
		return (*name == func_name);
	});
	if (iter != cend(dynamic_function_names)) {
		return dynamic_functions[(size_t)distance(cbegin(dynamic_function_names), iter)];
	}
	
#if !defined(__WINDOWS__)
FLOOR_PUSH_WARNINGS()
FLOOR_IGNORE_WARNING(zero-as-null-pointer-constant) // RTLD_DEFAULT is implementation-defined, but cast from int to void*
	auto func_ptr = dlsym(RTLD_DEFAULT, func_name.data());
FLOOR_POP_WARNINGS()
#else
	// get a handle to the main program / exe if it hasn't been created yet
	if(exe_module == nullptr) {
		exe_module = GetModuleHandle(nullptr);
	}
	// failed to get a handle
	if(exe_module == nullptr) {
		log_error("failed to get a module handle of the main program exe");
		return {};
	}
	
	auto func_ptr = (void*)GetProcAddress(exe_module, func_name.data());
#endif
	if(func_ptr == nullptr) {
#if !defined(__WINDOWS__)
		log_error("failed to retrieve function pointer to \"$\": $", func_name, dlerror());
#else
		log_error("failed to retrieve function pointer to \"$\": $", func_name, GetLastError());
#endif
		return {};
	}
	
	device_function::function_entry entry;
	entry.max_total_local_size = dev.max_total_local_size;
	entry.max_local_size = dev.max_local_size;
	
	auto dyn_function_name = std::make_unique<std::string>(func_name); // needs to be owned by us
	std::string_view dyn_function_name_sv(*dyn_function_name);
	dynamic_function_names.emplace_back(std::move(dyn_function_name));
	auto function = std::make_shared<host_function>(dyn_function_name_sv, (const void*)func_ptr, std::move(entry));
	dynamic_functions.emplace_back(function);
	
	return function;
}

} // namespace fl

#endif
