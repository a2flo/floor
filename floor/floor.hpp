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

#ifndef __FLOOR_FLOOR_HPP__
#define __FLOOR_FLOOR_HPP__

#include <floor/core/platform.hpp>

#define FLOOR_STRUCT_ALIGNMENT (16)

#include <floor/core/core.hpp>
#include <floor/core/file_io.hpp>
#include <floor/core/event.hpp>
#include <floor/core/xml.hpp>
#include <floor/math/vector_lib.hpp>
#include <floor/math/matrix4.hpp>
#include <floor/core/unicode.hpp>
#include <floor/compute/compute_base.hpp>

class FLOOR_API floor {
public:
	static void init(const char* callpath, const char* datapath,
					 const bool console_only = false, const string config_name = "config.xml",
					 const bool use_gl32_core = false,
					 // sdl window creation flags
					 // note: fullscreen + borderless flag will be set automatically depending on the config setting
#if !defined(FLOOR_IOS)
					 const unsigned int window_flags = (SDL_WINDOW_OPENGL |
														SDL_WINDOW_RESIZABLE)
#else
					 const unsigned int window_flags = (SDL_WINDOW_OPENGL |
														SDL_WINDOW_RESIZABLE |
														SDL_WINDOW_BORDERLESS |
														SDL_WINDOW_FULLSCREEN)
#endif
	);
	static void destroy();
	
	// graphic control functions
	static void start_draw();
	static void stop_draw(const bool window_swap = true);
	static void init_gl();
	static void resize_window();
	static void swap();
	static const string get_version();
	
	// class return functions
	static event* get_event();
	static xml* get_xml();

	// miscellaneous control functions
	static void set_caption(const string& caption);
	static string get_caption();

	static void set_cursor_visible(const bool& state);
	static bool get_cursor_visible();
	
	static void set_data_path(const char* data_path = "../data/");
	static string get_data_path();
	static string get_call_path();
	static string get_kernel_path();
	static string data_path(const string& str);
	static string kernel_path(const string& str);
	static string strip_data_path(const string& str);
	static string get_absolute_path();
	
	static void reload_kernels();
	
	static void acquire_context();
	static void release_context();
	
	// set to false to not acquire/release the gl context in acquire/release_context()
	static void set_use_gl_context(const bool& state);
	static const bool& get_use_gl_context();
	
	// fps functions
	static unsigned int get_fps();
	static float get_frame_time();
	static bool is_new_fps_count();

	// config functions
	static xml::xml_doc& get_config_doc();
	
	// screen/window
	static SDL_Window* get_window();
	static unsigned int get_window_flags();
	static SDL_GLContext get_context();
	static unsigned int get_width();
	static unsigned int get_height();
	static uint2 get_screen_size();
	static bool get_fullscreen();
	static bool get_vsync();
	static bool get_stereo();
	static const size_t& get_dpi();
	
	static void set_width(const unsigned int& width);
	static void set_height(const unsigned int& height);
	static void set_screen_size(const uint2& screen_size);
	static void set_fullscreen(const bool& state);
	static void set_vsync(const bool& state);
	
	// audio
	static void set_audio_disabled(const bool& state);
	static bool is_audio_disabled();
	static void set_music_volume(const float& volume);
	static const float& get_music_volume();
	static void set_sound_volume(const float& volume);
	static const float& get_sound_volume();
	static const string& get_audio_device_name();
	
	// projection
	static const float& get_fov();
	static const float2& get_near_far_plane();
	static void set_fov(const float& fov);
	static void set_upscaling(const float& upscaling);
	static const float& get_upscaling();
	static float get_scale_factor();
	
	// input
	static unsigned int get_key_repeat();
	static unsigned int get_ldouble_click_time();
	static unsigned int get_mdouble_click_time();
	static unsigned int get_rdouble_click_time();
	
	// compute
	static const string& get_compute_platform();
	static bool get_compute_gl_sharing();
	static bool get_compute_debug();
	static bool get_compute_profiling();
	static bool get_compute_log_binaries();
	static bool get_compute_keep_temp();
	static bool get_compute_keep_binaries();
	static bool get_compute_use_cache();
	
	// opencl
	static const string& get_opencl_platform();
	static const string& get_opencl_compiler();
	static const string& get_opencl_llc();
	static const string& get_opencl_as();
	static const string& get_opencl_dis();
	static const string& get_opencl_libcxx_path();
	static const string& get_opencl_clang_path();
	
	// cuda
	static const string& get_cuda_compiler();
	static const string& get_cuda_llc();
	static const string& get_cuda_libcxx_path();
	static const string& get_cuda_clang_path();
	static const string& get_cuda_as();
	static const string& get_cuda_dis();
	static const string& get_cuda_force_driver_sm();
	static const string& get_cuda_force_compile_sm();
	
	// metal
	static const string& get_metal_compiler();
	static const string& get_metal_llc();
	static const string& get_metal_as();
	static const string& get_metal_dis();
	static const string& get_metal_libcxx_path();
	static const string& get_metal_clang_path();
	
	// host
	static const string& get_host_compiler();
	static const string& get_host_llc();
	static const string& get_host_as();
	static const string& get_host_dis();
	static const string& get_host_libcxx_path();
	static const string& get_host_clang_path();
	
	// compute (opencl/cuda/metal/host)
	static shared_ptr<compute_base> get_compute_context();
	
protected:
	// static class
	floor(const floor&) = delete;
	~floor() = delete;
	floor& operator=(const floor&) = delete;
	
	static event* evt;
	static xml* x;
	static bool console_only;
	static shared_ptr<compute_base> compute_ctx;
	
	static void init_internal(const bool use_gl32_core, const unsigned int window_flags);
	
	static struct floor_config {
		// screen
		size_t width = 1280, height = 720, dpi = 0;
		bool fullscreen = false, vsync = false, stereo = false;
		
		// audio
		bool audio_disabled = true;
		float music_volume = 1.0f, sound_volume = 1.0f;
		string audio_device_name = "";
		
		// logging
		size_t verbosity = (size_t)logger::LOG_TYPE::UNDECORATED;
		bool separate_msg_file = false;
		bool append_mode = false;
		bool log_use_time = true;
		bool log_use_color = true;
		string log_filename = "";
		string msg_filename = "";
		
		// projection
		float fov = 72.0f;
		float2 near_far_plane { 0.1f, 1000.0f };
		float upscaling = 1.0f;
		
		// input
		size_t key_repeat = 200;
		size_t ldouble_click_time = 200;
		size_t mdouble_click_time = 200;
		size_t rdouble_click_time = 200;
		
		// compute
		string platform = "opencl";
		bool gl_sharing = false;
		bool debug = false;
		bool profiling = false;
		bool log_binaries = false;
		bool keep_temp = false;
		bool keep_binaries = true;
		bool use_cache = true;
		
		// opencl
		string opencl_platform = "0";
		unordered_set<string> opencl_restrictions;
		string opencl_compiler = "compute_clang";
		string opencl_llc = "compute_llc";
		string opencl_as = "compute_as";
		string opencl_dis = "compute_dis";
		string opencl_libcxx = "/usr/local/include/floor/libcxx/include";
		string opencl_clang = "/usr/local/include/floor/libcxx/clang";
		
		// cuda
		string cuda_compiler = "compute_clang";
		string cuda_llc = "compute_llc";
		string cuda_as = "compute_as";
		string cuda_dis = "compute_dis";
		string cuda_libcxx = "/usr/local/include/floor/libcxx/include";
		string cuda_clang = "/usr/local/include/floor/libcxx/clang";
		string cuda_force_driver_sm = "";
		string cuda_force_compile_sm = "";
		
		// metal
		string metal_compiler = "compute_clang";
		string metal_llc = "compute_llc";
		string metal_as = "compute_as";
		string metal_dis = "compute_dis";
		string metal_libcxx = "/usr/local/include/floor/libcxx/include";
		string metal_clang = "/usr/local/include/floor/libcxx/clang";
		
		// host
		string host_compiler = "compute_clang";
		string host_llc = "compute_llc";
		string host_as = "compute_as";
		string host_dis = "compute_dis";
		string host_libcxx = "/usr/local/include/floor/libcxx/include";
		string host_clang = "/usr/local/include/floor/libcxx/clang";

		// sdl
		SDL_Window* wnd = nullptr;
		SDL_GLContext ctx = nullptr;
		unsigned int flags = 0;
		recursive_mutex ctx_lock;
		atomic<unsigned int> ctx_active_locks { 0 };
		
		floor_config() {}
	} config;
	static xml::xml_doc config_doc;
	
	// path variables
	static string datapath;
	static string rel_datapath;
	static string callpath;
	static string kernelpath;
	static string abs_bin_path;
	static string config_name;

	// fps counting
	static unsigned int fps;
	static unsigned int fps_counter;
	static unsigned int fps_time;
	static float frame_time;
	static unsigned int frame_time_sum;
	static unsigned int frame_time_counter;
	static bool new_fps_count;
	
	// cursor
	static bool cursor_visible;
	
	// window event handlers
	static event::handler event_handler_fnctr;
	static bool event_handler(EVENT_TYPE type, shared_ptr<event_object> obj);
	
	// misc
	static atomic<bool> reload_kernels_flag;
	static bool use_gl_context;

};

#endif
