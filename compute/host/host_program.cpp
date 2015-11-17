/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2015 Florian Ziesche
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

#if !defined(__WINDOWS__)
#include <dlfcn.h>
#else
static HMODULE exe_module { nullptr };
#endif

host_program::host_program(shared_ptr<compute_device> device_) : device(device_) {
}

shared_ptr<compute_kernel> host_program::get_kernel(const string& func_name) const {
#if !defined(__WINDOWS__)
	auto func_ptr = dlsym(RTLD_DEFAULT, func_name.c_str());
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

	auto func_ptr = GetProcAddress(exe_module, func_name.c_str());
#endif
	if(func_ptr == nullptr) {
#if !defined(__WINDOWS__)
		log_error("failed to retrieve function pointer to \"%s\": %s", func_name, dlerror());
#else
		log_error("failed to retrieve function pointer to \"%s\": %u", func_name, GetLastError());
#endif
		return {};
	}
	
	compute_kernel::kernel_entry entry;
	entry.max_local_work_size = device->max_work_group_size;
	entry.max_work_group_item_sizes = device->max_work_group_item_sizes;
	
	auto kernel = make_shared<host_kernel>((const void*)func_ptr, func_name, move(entry));
	kernels.emplace_back(kernel);
	kernel_names.emplace_back(func_name);
	
	return kernel;
}

#endif
