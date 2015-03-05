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

#ifndef __FLOOR_LLVM_COMPUTE_HPP__
#define __FLOOR_LLVM_COMPUTE_HPP__

#include <floor/core/essentials.hpp>
#include <floor/compute/compute_device.hpp>

class llvm_compute {
public:
	enum class TARGET {
		//! OpenCL SPIR 1.2
		SPIR,
		//! Nvidia CUDA PTX
		PTX,
		//! Metal Apple-IR
		AIR,
	};
	
	//
	struct kernel_info {
		string name;
		vector<uint32_t> arg_sizes;
		
		enum class ARG_ADDRESS_SPACE : uint32_t {
			UNKNOWN = 0,
			GLOBAL = 1,
			LOCAL = 2,
			CONSTANT = 3,
		};
		//! NOTE: these will only be correct for OpenCL and Metal, CUDA uses a different approach,
		//! although some arguments might be marked with an address space nonetheless.
		vector<ARG_ADDRESS_SPACE> arg_address_spaces;
	};
	
	//
	static pair<string, vector<kernel_info>> compile_program(shared_ptr<compute_device> device,
															 const string& code,
															 const string additional_options = "",
															 const TARGET target = TARGET::SPIR);
	static pair<string, vector<kernel_info>> compile_program_file(shared_ptr<compute_device> device,
																  const string& filename,
																  const string additional_options = "",
																  const TARGET target = TARGET::SPIR);
	
protected:
	// static class
	llvm_compute(const llvm_compute&) = delete;
	~llvm_compute() = delete;
	llvm_compute& operator=(const llvm_compute&) = delete;
	
};

#endif
