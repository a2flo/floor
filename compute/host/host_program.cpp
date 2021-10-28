/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2021 Florian Ziesche
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
#include <floor/compute/host/host_program.hpp>
#include <floor/compute/host/host_kernel.hpp>
#include <floor/compute/host/host_device.hpp>
#include <floor/compute/host/elf_binary.hpp>
#include <floor/compute/device/host_limits.hpp>

#if !defined(__WINDOWS__)
#include <dlfcn.h>
#else
#include <floor/core/platform_windows.hpp>
#include <floor/core/essentials.hpp> // cleanup
static HMODULE exe_module { nullptr };
#endif

host_program::host_program(const compute_device& device_, program_map_type&& programs_) :
device(device_), programs(programs_), has_device_binary(!programs.empty()) {
	if (programs.empty()) {
		return;
	}
	retrieve_unique_kernel_names(programs);
	
	// create all kernels of all device programs
	// note that this essentially reshuffles the program "device -> kernels" data to "kernels -> devices"
	kernels.reserve(kernel_names.size());
	for (const auto& kernel_name : kernel_names) {
		host_kernel::kernel_map_type kernel_map;
		kernel_map.reserve(kernel_names.size());
		
		for (const auto& prog : programs) {
			if (!prog.second.valid || !prog.second.program) {
				continue;
			}
			
			const auto& function_names = prog.second.program->get_function_names();
			if (function_names.empty()) {
				continue;
			}
			
			for (const auto& info : prog.second.functions) {
				if (info.name != kernel_name) {
					continue;
				}
				
				const auto func_iter = find_if(function_names.begin(), function_names.end(), [&kernel_name](const auto& entry) {
					return (entry == kernel_name);
				});
				if (func_iter == function_names.end()) {
					continue;
				}
				
				host_kernel::host_kernel_entry entry;
				entry.info = &info;
				entry.program = prog.second.program;
				if (info.has_valid_local_size()) {
					const auto local_size_extent = info.local_size.extent();
					if (local_size_extent > host_limits::max_total_local_size) {
						log_error("kernel $ local size extent of $ is larger than the max supported local size of $",
								  info.name, local_size_extent, host_limits::max_total_local_size);
						continue;
					}
					entry.max_local_size = info.local_size;
					entry.max_total_local_size = local_size_extent;
				} else {
					// else: just assume the device/global default
					entry.max_local_size = prog.first.get().max_local_size;
					entry.max_total_local_size = host_limits::max_total_local_size;
				}
				
				// success, insert into map
				kernel_map.insert_or_assign(prog.first, entry);
				break;
			}
		}
		
		kernels.emplace_back(make_shared<host_kernel>(move(kernel_map)));
	}
}

shared_ptr<compute_kernel> host_program::get_kernel(const string& func_name) const {
	if (has_device_binary) {
		return compute_program::get_kernel(func_name);
	}
	
#if !defined(__WINDOWS__)
FLOOR_PUSH_WARNINGS()
FLOOR_IGNORE_WARNING(zero-as-null-pointer-constant) // RTLD_DEFAULT is implementation-defined, but cast from int to void*
	auto func_ptr = dlsym(RTLD_DEFAULT, func_name.c_str());
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

	auto func_ptr = (void*)GetProcAddress(exe_module, func_name.c_str());
#endif
	if(func_ptr == nullptr) {
#if !defined(__WINDOWS__)
		log_error("failed to retrieve function pointer to \"$\": $", func_name, dlerror());
#else
		log_error("failed to retrieve function pointer to \"$\": $", func_name, GetLastError());
#endif
		return {};
	}
	
	compute_kernel::kernel_entry entry;
	entry.max_total_local_size = device.max_total_local_size;
	entry.max_local_size = device.max_local_size;
	
	auto kernel = make_shared<host_kernel>((const void*)func_ptr, func_name, move(entry));
	kernels.emplace_back(kernel);
	kernel_names.emplace_back(func_name);
	
	return kernel;
}

#endif
