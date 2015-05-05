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

#ifndef __FLOOR_GL_SHADER_HPP__
#define __FLOOR_GL_SHADER_HPP__

#include <floor/core/cpp_headers.hpp>

struct floor_shader_object {
	struct internal_shader_object {
		unsigned int program { 0 };
		unsigned int vertex_shader { 0 };
		unsigned int fragment_shader { 0 };
		
		struct shader_variable {
			int location;
			int size;
			unsigned int type;
		};
		unordered_map<string, shader_variable> uniforms;
		unordered_map<string, shader_variable> attributes;
		unordered_map<string, int> samplers;
	};
	const string name;
	internal_shader_object program;
	
	floor_shader_object(const string& shd_name) : name(shd_name) {}
	~floor_shader_object() = default;
};

bool floor_compile_shader(floor_shader_object& shd, const char* vs_text, const char* fs_text);

#endif
