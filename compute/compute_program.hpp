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
	
protected:
	mutable vector<shared_ptr<compute_kernel>> kernels;
	mutable vector<string> kernel_names;
	
};

#endif
