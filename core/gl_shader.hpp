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
		uint32_t program { 0u };
		uint32_t vertex_shader { 0u };
		uint32_t fragment_shader { 0u };
		
		struct shader_variable {
			int32_t location;
			int32_t size;
			uint32_t type;
		};
		unordered_map<string, shader_variable> uniforms;
		unordered_map<string, shader_variable> attributes;
		unordered_map<string, int32_t> samplers;
	} program {};
	
	string name;
};

pair<bool, floor_shader_object> floor_compile_shader(const char* name,
													 const char* vs_text, const char* fs_text,
#if !defined(FLOOR_IOS)
													 const uint32_t glsl_version = 150, // glsl 1.50 core
#else
#if defined(PLATFORM_X64)
													 const uint32_t glsl_version = 300, // glsl es 3.00
#else
													 const uint32_t glsl_version = 100, // glsl es 1.00
#endif
#endif
													 const vector<pair<string, int32_t>> options = {});

#endif
