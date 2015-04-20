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

#ifndef __FLOOR_COMPUTE_PROGRAM_HPP__
#define __FLOOR_COMPUTE_PROGRAM_HPP__

#include <string>
#include <vector>
#include <floor/math/vector_lib.hpp>
#include <floor/compute/compute_kernel.hpp>

class FLOOR_API compute_program {
public:
	//! returns the kernel with the exact function name of "func_name", nullptr if not found
	shared_ptr<compute_kernel> get_kernel(const string& func_name) const;
	
	//! returns the first kernel whose function name contains "fuzzy_func_name", nullptr if none was found
	//! NOTE: this might be necessary / an easier way of doing things due to c++ name mangling
	shared_ptr<compute_kernel> get_kernel_fuzzy(const string& fuzzy_func_name) const;
	
protected:
	vector<shared_ptr<compute_kernel>> kernels;
	vector<string> kernel_names;
	
};

#endif
