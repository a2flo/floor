/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2016 Florian Ziesche
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
#include <floor/core/core.hpp>
#include <floor/core/file_io.hpp>
#include <floor/core/event.hpp>
#include <floor/core/json.hpp>
#include <floor/math/vector_lib.hpp>
#include <floor/math/matrix4.hpp>
#include <floor/core/unicode.hpp>
#include <floor/compute/compute_context.hpp>

class floor {
public:
	static void init(const char* callpath, const char* datapath,
					 const bool console_only = false, const string config_name = "config.json",
					 const bool use_gl33 = false,
					 // sdl window creation flags
					 // note: fullscreen, borderless and hidpi flags will be set automatically depending on the config settings
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
	static bool has_opengl_extension(const char* ext_name);
	static bool is_console_only();
	static bool is_gl_version(const uint32_t& major, const uint32_t& minor);
	static const string& get_gl_vendor();
	
	// class return functions
	static event* get_event();

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
	static json::document& get_config_doc();
	
	// screen/window
	static SDL_Window* get_window();
	static unsigned int get_window_flags();
	static SDL_GLContext get_context();
	static bool get_fullscreen();
	static void set_fullscreen(const bool& state);
	static bool get_vsync();
	static void set_vsync(const bool& state);
	static bool get_stereo();
	static const uint64_t& get_dpi();
	static bool get_hidpi();
	
	//! gets the logical window width
	static uint32_t get_width();
	//! gets the logical window height
	static uint32_t get_height();
	//! gets the logical window size
	static uint2 get_screen_size();
	
	//! gets the physical window height
	static uint32_t get_physical_width();
	//! gets the physical window height
	static uint32_t get_physical_height();
	//! gets the physical window size
	static uint2 get_physical_screen_size();
	
	static void set_screen_size(const uint2& screen_size);
	
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
	static const string& get_compute_backend();
	static bool get_compute_gl_sharing();
	static bool get_compute_debug();
	static bool get_compute_profiling();
	static bool get_compute_log_binaries();
	static bool get_compute_keep_temp();
	static bool get_compute_keep_binaries();
	static bool get_compute_use_cache();
	static bool get_compute_log_commands();
	
	// compute toolchain
	static const string& get_compute_default_compiler();
	static const string& get_compute_default_llc();
	static const string& get_compute_default_as();
	static const string& get_compute_default_dis();
	
	// opencl
	static const string& get_opencl_base_path();
	static const uint32_t& get_opencl_toolchain_version();
	static const vector<string>& get_opencl_whitelist();
	static const uint64_t& get_opencl_platform();
	static bool get_opencl_verify_spir();
	static bool get_opencl_validate_spirv();
	static bool get_opencl_force_spirv_check();
	static const string& get_opencl_compiler();
	static const string& get_opencl_llc();
	static const string& get_opencl_as();
	static const string& get_opencl_dis();
	static const string& get_opencl_spir_encoder();
	static const string& get_opencl_spir_verifier();
	static const string& get_opencl_applecl_encoder();
	static const string& get_opencl_spirv_encoder();
	static const string& get_opencl_spirv_as();
	static const string& get_opencl_spirv_dis();
	static const string& get_opencl_spirv_validator();
	
	// cuda
	static const string& get_cuda_base_path();
	static const uint32_t& get_cuda_toolchain_version();
	static const vector<string>& get_cuda_whitelist();
	static const string& get_cuda_compiler();
	static const string& get_cuda_llc();
	static const string& get_cuda_as();
	static const string& get_cuda_dis();
	static const string& get_cuda_force_driver_sm();
	static const string& get_cuda_force_compile_sm();
	static const string& get_cuda_force_ptx();
	static const uint32_t& get_cuda_max_registers();
	static const bool& get_cuda_jit_verbose();
	static const uint32_t& get_cuda_jit_opt_level();
	
	// metal
	static const string& get_metal_base_path();
	static const uint32_t& get_metal_toolchain_version();
	static const vector<string>& get_metal_whitelist();
	static const string& get_metal_compiler();
	static const string& get_metal_llc();
	static const string& get_metal_as();
	static const string& get_metal_dis();
	
	// vulkan
	static const string& get_vulkan_base_path();
	static const uint32_t& get_vulkan_toolchain_version();
	static const vector<string>& get_vulkan_whitelist();
	static bool get_vulkan_validate_spirv();
	static const string& get_vulkan_compiler();
	static const string& get_vulkan_llc();
	static const string& get_vulkan_as();
	static const string& get_vulkan_dis();
	static const string& get_vulkan_spirv_encoder();
	static const string& get_vulkan_spirv_as();
	static const string& get_vulkan_spirv_dis();
	static const string& get_vulkan_spirv_validator();
	
	// host
	static const string& get_execution_model();
	
	// compute (opencl/cuda/metal/host)
	static shared_ptr<compute_context> get_compute_context();
	
protected:
	// static class
	floor(const floor&) = delete;
	~floor() = delete;
	floor& operator=(const floor&) = delete;
	
	static event* evt;
	static bool console_only;
	static shared_ptr<compute_context> compute_ctx;
	static unordered_set<string> gl_extensions;
	
	static void init_internal(const bool use_gl33, const unsigned int window_flags);
	
	static struct floor_config {
		// screen
		uint64_t width = 1280, height = 720, dpi = 0;
		bool fullscreen = false, vsync = false, stereo = false, hidpi = false;
		
		// audio
		bool audio_disabled = true;
		float music_volume = 1.0f, sound_volume = 1.0f;
		string audio_device_name = "";
		
		// logging
		uint64_t verbosity = (size_t)logger::LOG_TYPE::UNDECORATED;
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
		uint64_t key_repeat = 200;
		uint64_t ldouble_click_time = 200;
		uint64_t mdouble_click_time = 200;
		uint64_t rdouble_click_time = 200;
		
		// compute
		string backend = "";
		bool gl_sharing = false;
		bool debug = false;
		bool profiling = false;
		bool log_binaries = false;
		bool keep_temp = false;
		bool keep_binaries = true;
		bool use_cache = true;
		bool log_commands = false;
		
		// compute toolchain
		string default_compiler = "clang";
		string default_llc = "llc";
		string default_as = "llvm-as";
		string default_dis = "llvm-dis";
		
		// opencl
		bool opencl_toolchain_exists = false;
		uint32_t opencl_toolchain_version = 0;
		string opencl_base_path = "";
		uint64_t opencl_platform = 0;
		bool opencl_verify_spir = false;
		bool opencl_validate_spirv = false;
		bool opencl_force_spirv_check = false;
		vector<string> opencl_whitelist;
		string opencl_compiler = default_compiler;
		string opencl_llc = default_llc;
		string opencl_as = default_as;
		string opencl_dis = default_dis;
		string opencl_spir_encoder = "spir-encoder";
		string opencl_spir_verifier = "spir-verifier";
		string opencl_applecl_encoder = "applecl-encoder";
		string opencl_spirv_encoder = "llvm-spirv";
		string opencl_spirv_as = "spirv-as";
		string opencl_spirv_dis = "spirv-dis";
		string opencl_spirv_validator = "spirv-val";
		
		// cuda
		bool cuda_toolchain_exists = false;
		uint32_t cuda_toolchain_version = 0;
		string cuda_base_path = "";
		vector<string> cuda_whitelist;
		string cuda_compiler = default_compiler;
		string cuda_llc = default_llc;
		string cuda_as = default_as;
		string cuda_dis = default_dis;
		string cuda_force_driver_sm = "";
		string cuda_force_compile_sm = "";
		string cuda_force_ptx = "";
		uint32_t cuda_max_registers = 32;
		bool cuda_jit_verbose = false;
		uint32_t cuda_jit_opt_level = 4;
		
		// metal
		bool metal_toolchain_exists = false;
		uint32_t metal_toolchain_version = 0;
		string metal_base_path = "";
		vector<string> metal_whitelist;
		string metal_compiler = default_compiler;
		string metal_llc = default_llc;
		string metal_as = default_as;
		string metal_dis = default_dis;
		
		// host
		string host_base_path = "";
		string execution_model = "mt-group";
		
		// vulkan
		bool vulkan_toolchain_exists = false;
		uint32_t vulkan_toolchain_version = 0;
		string vulkan_base_path = "";
		bool vulkan_validate_spirv = false;
		vector<string> vulkan_whitelist;
		string vulkan_compiler = default_compiler;
		string vulkan_llc = default_llc;
		string vulkan_as = default_as;
		string vulkan_dis = default_dis;
		string vulkan_spirv_encoder = "llvm-spirv";
		string vulkan_spirv_as = "spirv-as";
		string vulkan_spirv_dis = "spirv-dis";
		string vulkan_spirv_validator = "spirv-val";

		// sdl
		SDL_Window* wnd = nullptr;
		SDL_GLContext ctx = nullptr;
		unsigned int flags = 0;
		recursive_mutex ctx_lock;
		atomic<unsigned int> ctx_active_locks { 0 };
	} config;
	static json::document config_doc;
	
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
	static uint32_t global_vao;
	static string gl_vendor;

};

#endif
