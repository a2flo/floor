/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2017 Florian Ziesche
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

#include <floor/core/gl_shader.hpp>
#include <floor/core/gl_support.hpp>
#include <regex>

#if !defined(FLOOR_IOS) // unused on ios right now

#if !defined(FLOOR_IOS)
#define FLOOR_GL_SHADER_SAMPLER_TYPES(F) \
F(GL_SAMPLER_1D) \
F(GL_SAMPLER_2D) \
F(GL_SAMPLER_3D) \
F(GL_SAMPLER_CUBE) \
F(GL_SAMPLER_1D_SHADOW) \
F(GL_SAMPLER_2D_SHADOW) \
F(GL_SAMPLER_1D_ARRAY) \
F(GL_SAMPLER_2D_ARRAY) \
F(GL_SAMPLER_1D_ARRAY_SHADOW) \
F(GL_SAMPLER_2D_ARRAY_SHADOW) \
F(GL_SAMPLER_CUBE_SHADOW) \
F(GL_SAMPLER_BUFFER) \
F(GL_SAMPLER_2D_RECT) \
F(GL_SAMPLER_2D_RECT_SHADOW) \
F(GL_INT_SAMPLER_1D) \
F(GL_INT_SAMPLER_2D) \
F(GL_INT_SAMPLER_3D) \
F(GL_INT_SAMPLER_1D_ARRAY) \
F(GL_INT_SAMPLER_2D_ARRAY) \
F(GL_INT_SAMPLER_2D_RECT) \
F(GL_INT_SAMPLER_BUFFER) \
F(GL_INT_SAMPLER_CUBE) \
F(GL_UNSIGNED_INT_SAMPLER_1D) \
F(GL_UNSIGNED_INT_SAMPLER_2D) \
F(GL_UNSIGNED_INT_SAMPLER_3D) \
F(GL_UNSIGNED_INT_SAMPLER_1D_ARRAY) \
F(GL_UNSIGNED_INT_SAMPLER_2D_ARRAY) \
F(GL_UNSIGNED_INT_SAMPLER_2D_RECT) \
F(GL_UNSIGNED_INT_SAMPLER_BUFFER) \
F(GL_UNSIGNED_INT_SAMPLER_CUBE) \
F(GL_SAMPLER_2D_MULTISAMPLE) \
F(GL_INT_SAMPLER_2D_MULTISAMPLE) \
F(GL_UNSIGNED_INT_SAMPLER_2D_MULTISAMPLE) \
F(GL_SAMPLER_2D_MULTISAMPLE_ARRAY) \
F(GL_INT_SAMPLER_2D_MULTISAMPLE_ARRAY) \
F(GL_UNSIGNED_INT_SAMPLER_2D_MULTISAMPLE_ARRAY)
#else
#define FLOOR_GL_SHADER_SAMPLER_TYPES(F) \
F(GL_SAMPLER_2D) \
F(GL_SAMPLER_3D) \
F(GL_SAMPLER_CUBE) \
F(GL_SAMPLER_2D_SHADOW) \
F(GL_SAMPLER_2D_ARRAY) \
F(GL_SAMPLER_2D_ARRAY_SHADOW) \
F(GL_SAMPLER_CUBE_SHADOW) \
F(GL_INT_SAMPLER_2D) \
F(GL_INT_SAMPLER_3D) \
F(GL_INT_SAMPLER_CUBE) \
F(GL_INT_SAMPLER_2D_ARRAY) \
F(GL_UNSIGNED_INT_SAMPLER_2D) \
F(GL_UNSIGNED_INT_SAMPLER_3D) \
F(GL_UNSIGNED_INT_SAMPLER_CUBE) \
F(GL_UNSIGNED_INT_SAMPLER_2D_ARRAY)
#endif

#define __FLOOR_DECLARE_GL_TYPE_CHECK(type) case type: return true;
static bool is_gl_sampler_type(const GLenum& type) {
	switch(type) {
		FLOOR_GL_SHADER_SAMPLER_TYPES(__FLOOR_DECLARE_GL_TYPE_CHECK);
		default: break;
	}
	return false;
}

#endif // !FLOOR_IOS

#if defined(__APPLE__)
static void log_pretty_print(const char* log, const string& code) {
	static const regex rx_log_line("\\w+: 0:(\\d+):.*");
	smatch regex_result;
	
	const vector<string> lines { core::tokenize(string(log), '\n') };
	const vector<string> code_lines { core::tokenize(code, '\n') };
	for(const string& line : lines) {
		if(line.size() == 0) continue;
		log_undecorated("## \033[31m%s\033[m", line);
		
		// find code line and print it (+/- 1 line)
		if(regex_match(line, regex_result, rx_log_line)) {
			const size_t src_line_num = stosize(regex_result[1]) - 1;
			if(src_line_num < code_lines.size()) {
				if(src_line_num != 0) {
					log_undecorated("\033[37m%s\033[m", code_lines[src_line_num-1]);
				}
				log_undecorated("\033[31m%s\033[m", code_lines[src_line_num]);
				if(src_line_num+1 < code_lines.size()) {
					log_undecorated("\033[37m%s\033[m", code_lines[src_line_num+1]);
				}
			}
			log_undecorated("");
		}
	}
}
#else
static void log_pretty_print(const char* log, const string&) {
	log_undecorated("%s", log);
}
#endif

#define SHADER_LOG_SIZE 32767
pair<bool, floor_shader_object> floor_compile_shader(const char* name,
													 const char* vs_text,
													 const char* gs_text,
													 const char* fs_text,
													 const uint32_t glsl_version,
													 const vector<pair<string, int32_t>> options) {
	floor_shader_object shd { .name = name };
	
	// start header with requested glsl version:
	// opengl 2.x / opengl es 2.0 -> glsl 1.10/1.20 / glsl es 1.00: no suffix
	// opengl 3.0+ -> glsl 1.30 - glsl 4.50+: always core suffix
	// opengl es 3.0 -> glsl es 3.00: always es suffix
	string header = "#version " + to_string(glsl_version) + (glsl_version < 130 ? "" :
															 (glsl_version == 300 ? " es" : " core")) + "\n";
	
	// add options/defines list
	for(const auto& option : options) {
		header += "#define " + option.first + " " + to_string(option.second) + "\n";
	}
	
	// combine code
	const string vs_code = header + vs_text;
	const string fs_code = header + fs_text;
	const string gs_code = header + (gs_text != nullptr ? gs_text : "");
	const auto vs_code_ptr = vs_code.c_str();
	const auto fs_code_ptr = fs_code.c_str();
#if defined(FLOOR_IOS)
	floor_unused
#endif
	const auto gs_code_ptr = gs_code.c_str();
	
	// success flag (if it's 1 (true), we successfully created a shader object)
	GLint success = 0;
	GLchar info_log[SHADER_LOG_SIZE+1];
	memset(&info_log[0], 0, SHADER_LOG_SIZE+1);
	
	// add a new program object to this shader
	floor_shader_object::internal_shader_object& shd_obj = shd.program;
	
	// create the vertex shader object
	shd_obj.vertex_shader = glCreateShader(GL_VERTEX_SHADER);
	glShaderSource(shd_obj.vertex_shader, 1, (GLchar const**)&vs_code_ptr, nullptr);
	glCompileShader(shd_obj.vertex_shader);
	glGetShaderiv(shd_obj.vertex_shader, GL_COMPILE_STATUS, &success);
	if(!success) {
		glGetShaderInfoLog(shd_obj.vertex_shader, SHADER_LOG_SIZE, nullptr, info_log);
		log_error("error in vertex shader \"%s\" compilation:", shd.name);
		log_pretty_print(info_log, vs_code);
		return { false, {} };
	}
	
	// create the geometry shader object
	if(gs_text != nullptr) {
#if !defined(FLOOR_IOS)
		shd_obj.geometry_shader = glCreateShader(GL_GEOMETRY_SHADER);
		glShaderSource(shd_obj.geometry_shader, 1, (GLchar const**)&gs_code_ptr, nullptr);
		glCompileShader(shd_obj.geometry_shader);
		glGetShaderiv(shd_obj.geometry_shader, GL_COMPILE_STATUS, &success);
		if(!success) {
			glGetShaderInfoLog(shd_obj.geometry_shader, SHADER_LOG_SIZE, nullptr, info_log);
			log_error("error in geometry shader \"%s\" compilation:", shd.name);
			log_pretty_print(info_log, gs_code);
			return { false, {} };
		}
#else
		log_error("GLSL geometry shaders are not supported on iOS!");
#endif
	}
	
	// create the fragment shader object
	shd_obj.fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);
	glShaderSource(shd_obj.fragment_shader, 1, (GLchar const**)&fs_code_ptr, nullptr);
	glCompileShader(shd_obj.fragment_shader);
	glGetShaderiv(shd_obj.fragment_shader, GL_COMPILE_STATUS, &success);
	if(!success) {
		glGetShaderInfoLog(shd_obj.fragment_shader, SHADER_LOG_SIZE, nullptr, info_log);
		log_error("error in fragment shader \"%s\" compilation:", shd.name);
		log_pretty_print(info_log, fs_code);
		return { false, {} };
	}
	
	// create the program object
	shd_obj.program = glCreateProgram();
	// attach the vertex and fragment shader progam to it
	glAttachShader(shd_obj.program, shd_obj.vertex_shader);
	if(gs_text != nullptr) {
		glAttachShader(shd_obj.program, shd_obj.geometry_shader);
	}
	glAttachShader(shd_obj.program, shd_obj.fragment_shader);
	
	// now link the program object
	glLinkProgram(shd_obj.program);
	glGetProgramiv(shd_obj.program, GL_LINK_STATUS, &success);
	if(!success) {
		glGetProgramInfoLog(shd_obj.program, SHADER_LOG_SIZE, nullptr, info_log);
		log_error("error in program \"%s\" linkage!\nInfo log: %s", shd.name, info_log);
		return { false, {} };
	}
	glUseProgram(shd_obj.program);
	
	// grab number and names of all attributes and uniforms and get their locations (needs to be done before validation, b/c we have to set sampler locations)
	GLint attr_count = 0, uni_count = 0, max_attr_len = 0, max_uni_len = 0;
#if !defined(FLOOR_IOS)
	GLint var_location = 0;
	GLint var_size = 0;
	GLenum var_type = 0;
#endif
	
	glGetProgramiv(shd_obj.program, GL_ACTIVE_ATTRIBUTE_MAX_LENGTH, &max_attr_len);
	glGetProgramiv(shd_obj.program, GL_ACTIVE_UNIFORM_MAX_LENGTH, &max_uni_len);
	glGetProgramiv(shd_obj.program, GL_ACTIVE_ATTRIBUTES, &attr_count);
	glGetProgramiv(shd_obj.program, GL_ACTIVE_UNIFORMS, &uni_count);
	max_attr_len+=2;
	max_uni_len+=2;
	
	
	// TODO: fix this (I've currently no idea why this is failing to emplace/insert elements into maps)
#if !defined(FLOOR_IOS)
	// note: this may report weird attribute/uniform names (and locations), if uniforms/attributes are optimized away by the compiler
	auto attr_name = make_unique<GLchar[]>(size_t(max_attr_len));
	for(GLint attr = 0; attr < attr_count; attr++) {
		memset(attr_name.get(), 0, (size_t)max_attr_len);
		GLsizei written_size = 0;
		glGetActiveAttrib(shd_obj.program, (GLuint)attr, max_attr_len - 1, &written_size, &var_size, &var_type, attr_name.get());
		var_location = glGetAttribLocation(shd_obj.program, attr_name.get());
		if(var_location < 0) {
			continue;
		}
		string attribute_name(attr_name.get(), (size_t)written_size);
		if(attribute_name.find("[") != string::npos) attribute_name = attribute_name.substr(0, attribute_name.find("["));
		shd_obj.attributes.emplace(attribute_name,
								   floor_shader_object::internal_shader_object::shader_variable {
									   var_location,
									   var_size,
									   var_type });
	}
	
	auto uni_name = make_unique<GLchar[]>(size_t(max_uni_len));
	for(GLint uniform = 0; uniform < uni_count; uniform++) {
		memset(uni_name.get(), 0, (size_t)max_uni_len);
		GLsizei written_size = 0;
		glGetActiveUniform(shd_obj.program, (GLuint)uniform, max_uni_len - 1, &written_size, &var_size, &var_type, uni_name.get());
		var_location = glGetUniformLocation(shd_obj.program, uni_name.get());
		if(var_location < 0) {
			continue;
		}
		string uniform_name(uni_name.get(), (size_t)written_size);
		if(uniform_name.find("[") != string::npos) uniform_name = uniform_name.substr(0, uniform_name.find("["));
		shd_obj.uniforms.emplace(uniform_name,
								 floor_shader_object::internal_shader_object::shader_variable {
									 var_location,
									 var_size,
									 var_type });
		
		// if the uniform is a sampler, add it to the sampler mapping (with increasing id/num)
		// also: use shader_gl3 here, because we can't use shader_base directly w/o instantiating it
		if(is_gl_sampler_type(var_type)) {
			shd_obj.samplers.emplace(uniform_name, shd_obj.samplers.size());
			
			// while we are at it, also set the sampler location to a dummy value (this has to be done to satisfy program validation)
			glUniform1i(var_location, (GLint)shd_obj.samplers.size()-1);
		}
	}
#endif
	
	// validate the program object
	glValidateProgram(shd_obj.program);
	glGetProgramiv(shd_obj.program, GL_VALIDATE_STATUS, &success);
	if(!success) {
		glGetProgramInfoLog(shd_obj.program, SHADER_LOG_SIZE, nullptr, info_log);
		log_error("error in program \"%s\" validation!\nInfo log: %s", shd.name, info_log);
		return { false, {} };
	}
	else {
		glGetProgramInfoLog(shd_obj.program, SHADER_LOG_SIZE, nullptr, info_log);
		
		// check if shader will run in software (if so, print out a debug message)
		if(strstr((const char*)info_log, (const char*)"software") != nullptr) {
			log_debug("program \"%s\" validation: %s", shd.name, info_log);
		}
	}
	
	//
	glUseProgram(0);
	return { true, shd };
}
