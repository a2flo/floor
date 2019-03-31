/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2019 Florian Ziesche
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

#ifndef __FLOOR_COMPUTE_PROGRAM_HPP__
#define __FLOOR_COMPUTE_PROGRAM_HPP__

#include <string>
#include <vector>
#include <floor/math/vector_lib.hpp>
#include <floor/compute/llvm_toolchain.hpp>
#include <floor/compute/universal_binary.hpp>
#include <floor/core/flat_map.hpp>

class compute_kernel;
class compute_program {
public:
	virtual ~compute_program() = 0;
	
	//! returns the kernel with the exact function name of "func_name", nullptr if not found
	virtual shared_ptr<compute_kernel> get_kernel(const string& func_name) const;
	
	//! returns a container of all kernels in this program
	const vector<shared_ptr<compute_kernel>>& get_kernels() const {
		return kernels;
	}
	
	//! returns a container of all kernel function names in this program
	const vector<string>& get_kernel_names() const {
		return kernel_names;
	}
	
	//! stores a program + function infos for an individual device
	struct program_entry {
		//! only non-nullptr for backends that need to keep the archive memory around
		shared_ptr<universal_binary::archive> archive;
		
		vector<llvm_toolchain::function_info> functions;
		bool valid { false };
	};
	
protected:
	mutable vector<shared_ptr<compute_kernel>> kernels;
	mutable vector<string> kernel_names;
	
	template <typename device_type, typename program_entry_type>
	void retrieve_unique_kernel_names(const flat_map<device_type, program_entry_type>& programs) {
		// go through all kernels in all device programs and create a unique list of all kernel names
		kernel_names.clear(); // just in case
		for(const auto& prog : programs) {
			if(!prog.second.valid) continue;
			for(const auto& info : prog.second.functions) {
				kernel_names.push_back(info.name);
			}
		}
		kernel_names.erase(unique(begin(kernel_names), end(kernel_names)), end(kernel_names));
	}
	
};

#endif
