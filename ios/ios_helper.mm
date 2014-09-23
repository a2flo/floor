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

#if defined(__clang__)
// someone decided to recursively define boolean values ...
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdisabled-macro-expansion"
#endif

#include <floor/ios_helper.hpp>
#include <floor/core/logger.hpp>
#include <floor/core/core.hpp>
#include <regex>

#include <SDL2/SDL.h>
#include <SDL2/SDL_syswm.h>
#include <OpenGLES/EAGL.h>
#include <sys/types.h>
#include <sys/sysctl.h>

map<string, floor_shader_object*> ios_helper::shader_objects;

void* ios_helper::get_eagl_sharegroup() {
	return (__bridge void*)[[EAGLContext currentContext] sharegroup];
}

static void log_pretty_print(const char* log, const char* code) {
	static const regex rx_log_line("\\w+: 0:(\\d+):.*");
	smatch regex_result;
	
	const vector<string> lines { core::tokenize(string(log), '\n') };
	const vector<string> code_lines { core::tokenize(string(code), '\n') };
	for(const string& line : lines) {
		if(line.size() == 0) continue;
		log_undecorated("## \033[31m%s\033[m", line);
		
		// find code line and print it (+/- 1 line)
		if(regex_match(line, regex_result, rx_log_line)) {
			const size_t src_line_num = string2size_t(regex_result[1]) - 1;
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

#if 0
static bool is_gl_sampler_type(const GLenum& type) {
	switch(type) {
		case GL_SAMPLER_2D: return true;
		case GL_SAMPLER_CUBE: return true;
		default: break;
	}
	return false;
}
#endif

#define OCLRASTER_SHADER_LOG_SIZE 65535
static void compile_shader(floor_shader_object& shd, const char* vs_text, const char* fs_text) {
	// success flag (if it's 1 (true), we successfully created a shader object)
	GLint success = 0;
	GLchar info_log[OCLRASTER_SHADER_LOG_SIZE+1];
	info_log[OCLRASTER_SHADER_LOG_SIZE] = 0;
	
	// add a new program object to this shader
	floor_shader_object::internal_shader_object& shd_obj = shd.program;
	
	// create the vertex shader object
	shd_obj.vertex_shader = glCreateShader(GL_VERTEX_SHADER);
	glShaderSource(shd_obj.vertex_shader, 1, (GLchar const**)&vs_text, nullptr);
	glCompileShader(shd_obj.vertex_shader);
	glGetShaderiv(shd_obj.vertex_shader, GL_COMPILE_STATUS, &success);
	if(!success) {
		glGetShaderInfoLog(shd_obj.vertex_shader, OCLRASTER_SHADER_LOG_SIZE, nullptr, info_log);
		log_error("error in vertex shader \"%s\" compilation!", shd.name);
		log_pretty_print(info_log, vs_text);
		return;
	}
	
	// create the fragment shader object
	shd_obj.fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);
	glShaderSource(shd_obj.fragment_shader, 1, (GLchar const**)&fs_text, nullptr);
	glCompileShader(shd_obj.fragment_shader);
	glGetShaderiv(shd_obj.fragment_shader, GL_COMPILE_STATUS, &success);
	if(!success) {
		glGetShaderInfoLog(shd_obj.fragment_shader, OCLRASTER_SHADER_LOG_SIZE, nullptr, info_log);
		log_error("error in fragment shader \"%s\" compilation!", shd.name);
		log_pretty_print(info_log, fs_text);
		return;
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
		glGetProgramInfoLog(shd_obj.program, OCLRASTER_SHADER_LOG_SIZE, nullptr, info_log);
		log_error("error in program \"%s\" linkage!\nInfo log: %s", shd.name, info_log);
		return;
	}
	glUseProgram(shd_obj.program);
	
	// grab number and names of all attributes and uniforms and get their locations (needs to be done before validation, b/c we have to set sampler locations)
	GLint attr_count = 0, uni_count = 0, max_attr_len = 0, max_uni_len = 0;
#if 0
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
#if 0
	// note: this may report weird attribute/uniform names (and locations), if uniforms/attributes are optimized away by the compiler
	GLchar* attr_name = new GLchar[(size_t)max_attr_len];
	for(GLint attr = 0; attr < attr_count; attr++) {
		memset(attr_name, 0, (size_t)max_attr_len);
		GLsizei written_size = 0;
		glGetActiveAttrib(shd_obj.program, (GLuint)attr, max_attr_len-1, &written_size, &var_size, &var_type, attr_name);
		var_location = glGetAttribLocation(shd_obj.program, attr_name);
		if(var_location < 0) {
			continue;
		}
		string attribute_name(attr_name, (size_t)written_size);
		cout << "attribute_name: " << attribute_name << endl;
		if(attribute_name.find("[") != string::npos) attribute_name = attribute_name.substr(0, attribute_name.find("["));
		shd_obj.attributes.emplace(attribute_name,
								   floor_shader_object::internal_shader_object::shader_variable {
									   (size_t)var_location,
									   (size_t)var_size,
									   var_type });
	}
	delete [] attr_name;
	
	GLchar* uni_name = new GLchar[(size_t)max_uni_len];
	for(GLint uniform = 0; uniform < uni_count; uniform++) {
		memset(uni_name, 0, (size_t)max_uni_len);
		GLsizei written_size = 0;
		glGetActiveUniform(shd_obj.program, (GLuint)uniform, max_uni_len-1, &written_size, &var_size, &var_type, uni_name);
		var_location = glGetUniformLocation(shd_obj.program, uni_name);
		if(var_location < 0) {
			continue;
		}
		string uniform_name(uni_name, (size_t)written_size);
		cout << "uniform_name: " << uniform_name << endl;
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
	delete [] uni_name;
#endif
	
	// validate the program object
	glValidateProgram(shd_obj.program);
	glGetProgramiv(shd_obj.program, GL_VALIDATE_STATUS, &success);
	if(!success) {
		glGetProgramInfoLog(shd_obj.program, OCLRASTER_SHADER_LOG_SIZE, nullptr, info_log);
		log_error("error in program \"%s\" validation!\nInfo log: %s", shd.name, info_log);
		return;
	}
	else {
		glGetProgramInfoLog(shd_obj.program, OCLRASTER_SHADER_LOG_SIZE, nullptr, info_log);
		
		// check if shader will run in software (if so, print out a debug message)
		if(strstr((const char*)info_log, (const char*)"software") != nullptr) {
			log_debug("program \"%s\" validation: %s", shd.name, info_log);
		}
	}
	
	//
	glUseProgram(0);
}

void ios_helper::compile_shaders() {
	static constexpr char blit_vs_text[] { u8R"OCLRASTER_RAWSTR(
		attribute vec2 in_vertex;
		varying lowp vec2 tex_coord;
		void main() {
			gl_Position = vec4(in_vertex.x, in_vertex.y, 0.0, 1.0);
			tex_coord = in_vertex * 0.5 + 0.5;
		}
	)OCLRASTER_RAWSTR"};
	static constexpr char blit_fs_text[] { u8R"OCLRASTER_RAWSTR(
		uniform sampler2D tex;
		varying lowp vec2 tex_coord;
		void main() {
			gl_FragColor = texture2D(tex, tex_coord);
		}
	)OCLRASTER_RAWSTR"};
		
	floor_shader_object* shd = new floor_shader_object("BLIT");
	compile_shader(*shd, blit_vs_text, blit_fs_text);
	shader_objects.emplace("BLIT", shd);
}

floor_shader_object* ios_helper::get_shader(const string& name) {
	const auto iter = shader_objects.find(name);
	if(iter == shader_objects.end()) return nullptr;
	return iter->second;
}
		
size_t ios_helper::get_system_version() {
	// add a define for the run time os x version
	string osrelease(16, 0);
	size_t size = osrelease.size() - 1;
	sysctlbyname("kern.osrelease", &osrelease[0], &size, nullptr, 0);
	osrelease.back() = 0;
	const size_t major_dot = osrelease.find(".");
	const size_t minor_dot = (major_dot != string::npos ? osrelease.find(".", major_dot + 1) : string::npos);
	if(major_dot != string::npos && minor_dot != string::npos) {
		const size_t major_version = string2size_t(osrelease.substr(0, major_dot));
		const size_t ios_minor_version = string2size_t(osrelease.substr(major_dot + 1, major_dot - minor_dot - 1));
		// osrelease = kernel version, not ios version -> substract 7 (NOTE: this won't run on anything < iOS 7.0,
		// so any differentiation below doesn't matter (5.0: darwin 11, 6.0: darwin 13, 7.0: darwin 14)
		const size_t ios_major_version = major_version - 7;
		const string ios_major_version_str = size_t2string(ios_major_version);
		
		// mimic the compiled version string (xxyyy, xx = major, y = minor)
		size_t condensed_version = ios_major_version * 10000;
		// -> single digit minor version (cut off at 9, this should probably suffice)
		condensed_version += (ios_minor_version < 10 ? ios_minor_version : 9) * 100;
		return condensed_version;
	}
	
	log_error("unable to retrieve iOS version!");
	return __IPHONE_7_0; // just return lowest version
}

size_t ios_helper::get_compiled_system_version() {
	// this is a number: 70000 (7.0), 60100 (6.1), ...
	return __IPHONE_OS_VERSION_MAX_ALLOWED;
}

size_t ios_helper::get_dpi() {
	if([[UIDevice currentDevice] userInterfaceIdiom] == UIUserInterfaceIdiomPhone) {
		// iphone / ipod touch: all supported devices have 326 dpi
		return 326;
	}
	else {
		// ipad: this is trickier: ipad2 has 132 dpi, ipad 3+ have 264 dpi, ipad mini has 163 dpi, ipad mini retina has 326 dpi
#if defined(PLATFORM_X32)
		// ipad 2, 3, 4 or ipad mini
		if([[UIScreen mainScreen] scale] == 1.0f) {
			// ipad 2 or ipad mini
			string machine(8, 0);
			size_t size = machine.size() - 1;
			sysctlbyname("hw.machine", &machine[0], &size, nullptr, 0);
			machine.back() = 0;
			if(machine == "iPad2,5" || machine == "iPad2,6" || machine == "iPad2,7") {
				// ipad mini
				return 163;
			}
			// else: ipad 2
			return 132;
		}
		// else: ipad 3 or 4
		else return 264;
#else // PLATFORM_X64
		// TODO: there seems to be a way to get the product-name via iokit (arm device -> product -> product-name)
		// ipad air/5+ or ipad mini retina
		string machine(8, 0);
		size_t size = machine.size() - 1;
		sysctlbyname("hw.machine", &machine[0], &size, nullptr, 0);
		machine.back() = 0;
		if(machine == "iPad4,4" || machine == "iPad4,5" || machine == "iPad4,6") {
			// ipad mini retina (for now ...)
			return 326;
		}
		// else: ipad air/5+ (for now ...)
		return 264;
#endif
	}
}

string ios_helper::get_computer_name() {
	return [[[UIDevice currentDevice] name] UTF8String];
}
		
string ios_helper::utf8_decomp_to_precomp(const string& str) {
	return [[[NSString stringWithUTF8String:str.c_str()] precomposedStringWithCanonicalMapping] UTF8String];
}

#if defined(__clang__)
#pragma clang diagnostic pop
#endif
