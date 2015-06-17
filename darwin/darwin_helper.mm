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

#if !defined(FLOOR_IOS)
#include <Cocoa/Cocoa.h>
#endif
#include <SDL2/SDL.h>
#include <SDL2/SDL_syswm.h>
#include <sys/types.h>
#include <sys/sysctl.h>
#include <floor/math/vector_lib.hpp>
#include <floor/core/core.hpp>
#include <floor/core/logger.hpp>
#include <floor/core/util.hpp>
#include <floor/darwin/darwin_helper.hpp>

#if defined(FLOOR_IOS)
#include <OpenGLES/EAGL.h>

unordered_map<string, shared_ptr<floor_shader_object>> darwin_helper::shader_objects;
#endif

size_t darwin_helper::get_dpi(SDL_Window* wnd
#if defined(FLOOR_IOS)
							  floor_unused
#endif
							  ) {
#if !defined(FLOOR_IOS) // on OS X this can be done properly through querying CoreGraphics
	float2 display_res(float2(CGDisplayPixelsWide(CGMainDisplayID()),
							  CGDisplayPixelsHigh(CGMainDisplayID())) * get_scale_factor(wnd));
	const CGSize display_phys_size(CGDisplayScreenSize(CGMainDisplayID()));
	const float2 display_dpi((display_res.x / (float)display_phys_size.width) * 25.4f,
							 (display_res.y / (float)display_phys_size.height) * 25.4f);
	return (size_t)floorf(std::max(display_dpi.x, display_dpi.y));
#else // on iOS there is now way of doing this properly, so it must be hardcoded for each device type
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
#endif
}

float darwin_helper::get_scale_factor(SDL_Window* wnd) {
	SDL_SysWMinfo wm_info;
	SDL_VERSION(&wm_info.version);
	if(SDL_GetWindowWMInfo(wnd, &wm_info) == 1) {
#if !defined(FLOOR_IOS)
		return (float)[wm_info.info.cocoa.window backingScaleFactor];
#else
		return (float)[[wm_info.info.uikit.window screen] scale];
#endif
	}
	return 1.0f;
}

#if !defined(FLOOR_IOS)
float darwin_helper::get_menu_bar_height() {
	return (float)[[[NSApplication sharedApplication] mainMenu] menuBarHeight];
}
#endif

size_t darwin_helper::get_system_version() {
	// TODO: fix for 10.10
	// add a define for the run time os x version
	string osrelease(16, 0);
	size_t size = osrelease.size() - 1;
	sysctlbyname("kern.osrelease", &osrelease[0], &size, nullptr, 0);
	osrelease.back() = 0;
	const size_t major_dot = osrelease.find(".");
	const size_t minor_dot = (major_dot != string::npos ? osrelease.find(".", major_dot + 1) : string::npos);
	if(major_dot != string::npos && minor_dot != string::npos) {
		const size_t major_version = stosize(osrelease.substr(0, major_dot));
		const size_t os_minor_version = stosize(osrelease.substr(major_dot + 1, major_dot - minor_dot - 1));
		
#if !defined(FLOOR_IOS)
		// osrelease = kernel version, not os x version -> substract 4 (10.11 = 15 - 4, 10.10 = 14 - 4, 10.9 = 13 - 4, ...)
		const size_t os_major_version = major_version - 4;
#else
		// osrelease = kernel version, not ios version -> substract 7 (NOTE: this won't run on anything < iOS 7.0,
		// so any differentiation below doesn't matter (5.0: darwin 11, 6.0: darwin 13, 7.0: darwin 14)
		size_t os_major_version = major_version - 7;
		
		// iOS 7.x and 8.x are both based on darwin 14.x -> need to differentiate through xnu kernel version
		// this is 24xx on iOS 7.x and 27xx on iOS 8.x
		// NOTE: iOS 9.x is based on darwin 15, so add 1 in this case as well (and all future versions)
		string kern_version(256, 0);
		size_t kern_version_size = kern_version.size() - 1;
		sysctlbyname("kern.version", &kern_version[0], &kern_version_size, nullptr, 0);
		if(kern_version.find("xnu-24") != string::npos) {
			// this is iOS 7.x, do nothing, os_major_version is already correct
		}
		else {
			// must be iOS 8.x or higher -> add 1 to the version
			++os_major_version;
		}
#endif
		const string os_major_version_str = to_string(os_major_version);
		
		// mimic the compiled version string:
		// OS X <= 10.9:   10xy,   xx = major, y = minor
		// OS X >= 10.10:  10xxyy, xx = major, y = minor
		// iOS:            xxyyy,  xx = major, y = minor
		size_t condensed_version;
#if !defined(FLOOR_IOS)
		if(major_version <= 9) { // 1000 - 1090
			condensed_version = 1000;
			condensed_version += os_major_version * 10;
			// -> single digit minor version (cut off at 9, this should probably suffice)
			condensed_version += (os_minor_version < 10 ? os_minor_version : 9);
		}
		else { // 101000+
			condensed_version = 100000;
			condensed_version += os_major_version * 100;
			condensed_version += os_minor_version;
		}
#else
		condensed_version = os_major_version * 10000;
		condensed_version += os_minor_version * 100;
#endif
		return condensed_version;
	}
	
	// just return lowest supported version
#if !defined(FLOOR_IOS)
	log_error("unable to retrieve OS X version!");
	return 1090;
#else
	log_error("unable to retrieve iOS version!");
	return 70000;
#endif
}

size_t darwin_helper::get_compiled_system_version() {
#if !defined(FLOOR_IOS)
	// this is a number: 101000 (10.10), 1090 (10.9), 1080 (10.8), ...
	return MAC_OS_X_VERSION_MAX_ALLOWED;
#else
	// this is a number: 70000 (7.0), 60100 (6.1), ...
	return __IPHONE_OS_VERSION_MAX_ALLOWED;
#endif
}

string darwin_helper::get_computer_name() {
#if !defined(FLOOR_IOS)
	return [[[NSHost currentHost] localizedName] UTF8String];
#else
	return [[[UIDevice currentDevice] name] UTF8String];
#endif
}

string darwin_helper::utf8_decomp_to_precomp(const string& str) {
	return [[[NSString stringWithUTF8String:str.c_str()] precomposedStringWithCanonicalMapping] UTF8String];
}

int64_t darwin_helper::get_memory_size() {
	int64_t mem_size { 0 };
	static int sysctl_cmd[2] { CTL_HW, HW_MEMSIZE };
	static size_t size = sizeof(mem_size);
	sysctl(&sysctl_cmd[0], 2, &mem_size, &size, nullptr, 0);
	return mem_size;
}

string darwin_helper::get_bundle_identifier() {
	return [[[NSBundle mainBundle] objectForInfoDictionaryKey:@"CFBundleIdentifier"] UTF8String];
}

#if defined(FLOOR_IOS)
void* darwin_helper::get_eagl_sharegroup() {
	return (__bridge void*)[[EAGLContext currentContext] sharegroup];
}

void darwin_helper::compile_shaders() {
	static constexpr const char blit_vs_text[] { u8R"RAWSTR(
		attribute vec2 in_vertex;
		varying lowp vec2 tex_coord;
		void main() {
			gl_Position = vec4(in_vertex.x, in_vertex.y, 0.0, 1.0);
			tex_coord = in_vertex * 0.5 + 0.5;
		}
	)RAWSTR"};
	static constexpr const char blit_fs_text[] { u8R"RAWSTR(
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

shared_ptr<floor_shader_object> darwin_helper::get_shader(const string& name) {
	const auto iter = shader_objects.find(name);
	if(iter == shader_objects.end()) return nullptr;
	return iter->second;
}
#endif
