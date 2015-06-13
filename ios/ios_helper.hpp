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

#ifndef __FLOOR_IOS_HELPER_HPP__
#define __FLOOR_IOS_HELPER_HPP__

#include <floor/core/gl_support.hpp>
#include <floor/core/cpp_headers.hpp>
#include <floor/core/gl_shader.hpp>

class ios_helper {
public:
	static void* get_eagl_sharegroup();
	static void compile_shaders();
	static shared_ptr<floor_shader_object> get_shader(const string& name);
	static size_t get_system_version();
	static size_t get_compiled_system_version();
	static size_t get_dpi();
	static string get_computer_name();
	static string utf8_decomp_to_precomp(const string& str);
	static int64_t get_memory_size();
	static string get_bundle_identifier();
	
protected:
	static unordered_map<string, shared_ptr<floor_shader_object>> shader_objects;
	
};

#endif
