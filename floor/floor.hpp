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

class opencl_base;
extern opencl_base* ocl;

class floor {
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
	
	// opencl
	static bool get_gl_sharing();
	static bool get_log_binaries();
	
	// cuda
	static const string& get_cuda_base_dir();
	static bool get_cuda_debug();
	static bool get_cuda_profiling();
	static bool get_cuda_keep_temp();
	static bool get_cuda_keep_binaries();
	static bool get_cuda_use_cache();
	
protected:
	// static class
	floor(const floor&) = delete;
	~floor() = delete;
	floor& operator=(const floor&) = delete;
	
	static event* evt;
	static xml* x;
	static bool console_only;
	
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
		string log_filename = "log.txt";
		string msg_filename = "msg.txt";
		
		// projection
		float fov = 72.0f;
		float2 near_far_plane { 0.1f, 1000.0f };
		float upscaling = 1.0f;
		
		// input
		size_t key_repeat = 200;
		size_t ldouble_click_time = 200;
		size_t mdouble_click_time = 200;
		size_t rdouble_click_time = 200;
		
		// opencl
		string opencl_platform = "0";
		bool clear_cache = false;
		bool gl_sharing = true;
		bool log_binaries = false;
		set<string> cl_device_restriction;
		
		// cuda
		string cuda_base_dir = "/usr/local/cuda";
		bool cuda_debug = false;
		bool cuda_profiling = false;
		bool cuda_keep_temp = false;
		bool cuda_keep_binaries = true;
		bool cuda_use_cache = true;

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

};

#endif
