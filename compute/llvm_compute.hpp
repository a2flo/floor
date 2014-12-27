/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2014 Florian Ziesche
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

// TODO: make this compile with either opencl or cuda disabled (currently requires cuda if opencl is enabled)
#if !defined(FLOOR_NO_OPENCL) || !defined(FLOOR_NO_CUDA)

#include <floor/cl/opencl.hpp>

class llvm_compute {
public:
	static void init();
	
	enum class TARGET {
		SPIR,
		PTX,
	};
	
	//
	static string compile_program(const string& code,
								  const string additional_options = "",
								  const TARGET target = TARGET::SPIR);
	static string compile_program_file(const string& filename,
									   const string additional_options = "",
									   const TARGET target = TARGET::SPIR);
	
	//
	static void load_module_from_file(const string& file_name,
									  const vector<pair<string, string>>& function_mappings);
	static void load_module(const char* module_data,
							const vector<pair<string, string>>& function_mappings);
	
protected:
	// static class
	llvm_compute(const llvm_compute&) = delete;
	~llvm_compute() = delete;
	llvm_compute& operator=(const llvm_compute&) = delete;
	
	//
	static cudacl* cucl;
	
	static vector<CUmodule> modules;
	static unordered_map<string, CUfunction> functions;
	
};

#endif

#endif
