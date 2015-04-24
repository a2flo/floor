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

#include <floor/core/gl_shader.hpp>

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

#define __FLOOR_DECLARE_GL_TYPE_CHECK(type) case type: return true;
static bool is_gl_sampler_type(const GLenum& type) {
	switch(type) {
		FLOOR_GL_SHADER_SAMPLER_TYPES(__FLOOR_DECLARE_GL_TYPE_CHECK);
		default: break;
	}
	return false;
}

#define SHADER_LOG_SIZE 32767
bool floor_compile_shader(floor_shader_object& shd, const char* vs_text, const char* fs_text) {
	// success flag (if it's 1 (true), we successfully created a shader object)
	GLint success = 0;
	GLchar info_log[SHADER_LOG_SIZE+1];
	memset(&info_log[0], 0, SHADER_LOG_SIZE+1);
	
	// add a new program object to this shader
	floor_shader_object::internal_shader_object& shd_obj = shd.program;
	
	// create the vertex shader object
	shd_obj.vertex_shader = glCreateShader(GL_VERTEX_SHADER);
	glShaderSource(shd_obj.vertex_shader, 1, (GLchar const**)&vs_text, nullptr);
	glCompileShader(shd_obj.vertex_shader);
	glGetShaderiv(shd_obj.vertex_shader, GL_COMPILE_STATUS, &success);
	if(!success) {
		glGetShaderInfoLog(shd_obj.vertex_shader, SHADER_LOG_SIZE, nullptr, info_log);
		log_error("error in vertex shader \"%s\" compilation:\n%s", shd.name, info_log);
		return false;
	}
	
	// create the fragment shader object
	shd_obj.fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);
	glShaderSource(shd_obj.fragment_shader, 1, (GLchar const**)&fs_text, nullptr);
	glCompileShader(shd_obj.fragment_shader);
	glGetShaderiv(shd_obj.fragment_shader, GL_COMPILE_STATUS, &success);
	if(!success) {
		glGetShaderInfoLog(shd_obj.fragment_shader, SHADER_LOG_SIZE, nullptr, info_log);
		log_error("error in fragment shader \"%s\" compilation:\n%s", shd.name, info_log);
		return false;
	}
	
	// create the program object
	shd_obj.program = glCreateProgram();
	// attach the vertex and fragment shader progam to it
	glAttachShader(shd_obj.program, shd_obj.vertex_shader);
	glAttachShader(shd_obj.program, shd_obj.fragment_shader);
	
	// now link the program object
	glLinkProgram(shd_obj.program);
	glGetProgramiv(shd_obj.program, GL_LINK_STATUS, &success);
	if(!success) {
		glGetProgramInfoLog(shd_obj.program, SHADER_LOG_SIZE, nullptr, info_log);
		log_error("error in program \"%s\" linkage!\nInfo log: %s", shd.name, info_log);
		return false;
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
		glGetActiveAttrib(shd_obj.program, (GLuint)attr, max_attr_len-1, &written_size, &var_size, &var_type, attr_name.get());
		var_location = glGetAttribLocation(shd_obj.program, attr_name.get());
		if(var_location < 0) {
			continue;
		}
		string attribute_name(attr_name.get(), (size_t)written_size);
		if(attribute_name.find("[") != string::npos) attribute_name = attribute_name.substr(0, attribute_name.find("["));
		shd_obj.attributes.emplace(attribute_name,
								   floor_shader_object::internal_shader_object::shader_variable {
									   (size_t)var_location,
									   (size_t)var_size,
									   var_type });
	}
	
	auto uni_name = make_unique<GLchar[]>(size_t(max_uni_len));
	for(GLint uniform = 0; uniform < uni_count; uniform++) {
		memset(uni_name.get(), 0, (size_t)max_uni_len);
		GLsizei written_size = 0;
		glGetActiveUniform(shd_obj.program, (GLuint)uniform, max_uni_len-1, &written_size, &var_size, &var_type, uni_name.get());
		var_location = glGetUniformLocation(shd_obj.program, uni_name.get());
		if(var_location < 0) {
			continue;
		}
		string uniform_name(uni_name.get(), (size_t)written_size);
		if(uniform_name.find("[") != string::npos) uniform_name = uniform_name.substr(0, uniform_name.find("["));
		shd_obj.uniforms.emplace(uniform_name,
								 floor_shader_object::internal_shader_object::shader_variable {
									 (size_t)var_location,
									 (size_t)var_size,
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
		return false;
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
	return true;
}
