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

#if !defined(FLOOR_NO_OPENCL) || !defined(FLOOR_NO_CUDA)

#include <string>
using namespace std;

class llvm_compute {
public:
	enum class TARGET {
		SPIR,
		PTX,
	};
	
	//
	static string compile_program(const string& code,
								  const string additional_options = "",
								  const TARGET target = TARGET::SPIR,
								  // NOTE: only used with PTX
								  vector<string>* kernel_names = nullptr);
	static string compile_program_file(const string& filename,
									   const string additional_options = "",
									   const TARGET target = TARGET::SPIR,
									   // NOTE: only used with PTX
									   vector<string>* kernel_names = nullptr);
	
protected:
	// static class
	llvm_compute(const llvm_compute&) = delete;
	~llvm_compute() = delete;
	llvm_compute& operator=(const llvm_compute&) = delete;
	
};

#endif

#endif
