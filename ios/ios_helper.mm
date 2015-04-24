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

#if defined(__clang__)
// someone decided to recursively define boolean values ...
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdisabled-macro-expansion"
#endif

#include <floor/ios/ios_helper.hpp>
#include <floor/core/logger.hpp>
#include <floor/core/core.hpp>
#include <regex>

#include <SDL2/SDL.h>
#include <SDL2/SDL_syswm.h>
#include <OpenGLES/EAGL.h>
#include <sys/types.h>
#include <sys/sysctl.h>

unordered_map<string, shared_ptr<floor_shader_object>> ios_helper::shader_objects;

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

void ios_helper::compile_shaders() {
	static constexpr char blit_vs_text[] { u8R"RAWSTR(
		attribute vec2 in_vertex;
		varying lowp vec2 tex_coord;
		void main() {
			gl_Position = vec4(in_vertex.x, in_vertex.y, 0.0, 1.0);
			tex_coord = in_vertex * 0.5 + 0.5;
		}
	)RAWSTR"};
	static constexpr char blit_fs_text[] { u8R"RAWSTR(
		uniform sampler2D tex;
		varying lowp vec2 tex_coord;
		void main() {
			gl_FragColor = texture2D(tex, tex_coord);
		}
	)RAWSTR"};
		
	auto shd = make_shared<floor_shader_object>("BLIT");
	floor_compile_shader(*shd.get(), blit_vs_text, blit_fs_text);
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
		const size_t major_version = stosize(osrelease.substr(0, major_dot));
		const size_t ios_minor_version = stosize(osrelease.substr(major_dot + 1, major_dot - minor_dot - 1));
		// osrelease = kernel version, not ios version -> substract 7 (NOTE: this won't run on anything < iOS 7.0,
		// so any differentiation below doesn't matter (5.0: darwin 11, 6.0: darwin 13, 7.0: darwin 14)
		const size_t ios_major_version = major_version - 7;
		const string ios_major_version_str = to_string(ios_major_version);
		
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
		constexpr const size_t max_machine_len { 10u };
		size_t machine_len { max_machine_len };
		char machine[max_machine_len + 1u];
		memset(machine, 0, machine_len + 1u);
		sysctlbyname("hw.machine", &machine[0], &machine_len, nullptr, 0);
		machine[max_machine_len] = 0;
		const string machine_str { machine };
		if(machine_str == "iPad4,4" || machine_str == "iPad4,5" || machine_str == "iPad4,6" ||
		   machine_str == "iPad4,7" || machine_str == "iPad4,8") {
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

int64_t ios_helper::get_memory_size() {
	int64_t mem_size { 0 };
	static int sysctl_cmd[2] { CTL_HW, HW_MEMSIZE };
	static size_t size = sizeof(mem_size);
	sysctl(&sysctl_cmd[0], 2, &mem_size, &size, nullptr, 0);
	return mem_size;
	
}
		
string ios_helper::get_bundle_identifier() {
	return [[[NSBundle mainBundle] objectForInfoDictionaryKey:@"CFBundleIdentifier"] UTF8String];
}

#if defined(__clang__)
#pragma clang diagnostic pop
#endif
