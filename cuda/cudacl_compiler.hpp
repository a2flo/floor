/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2013 Florian Ziesche
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

#ifndef __FLOOR_CUDACL_COMPILER_HPP__
#define __FLOOR_CUDACL_COMPILER_HPP__

#if defined(FLOOR_CUDA_CL)

#include "floor/floor.hpp"

class cudacl_compiler {
public:
	// compiles .cu source code to .ptx using tccpp, cudafe and cicc
	static string compile(const string& code,
						  const string& identifier,
						  const string& cc_target_str,
						  const string& user_options,
						  const string& tmp_name,
						  string& error_log,
						  string& info_log);
	
};

#endif
#endif
