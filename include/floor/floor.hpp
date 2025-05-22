/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2025 Florian Ziesche
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

#pragma once

#include <floor/core/platform.hpp>
#include <floor/core/event.hpp>
#include <floor/math/vector_lib.hpp>
#include <floor/device/device_context.hpp>

namespace fl {

// forward decls
namespace json {
struct document;
} // namespace json

class vr_context;

class floor {
public:
	//! renderer backend that should be used and initialized
	enum class RENDERER : uint32_t {
		//! don't create any renderer
		NONE = 0,
		
		//! selects the renderer based on the config and OS
		DEFAULT = 1,
		
		//! use the Vulkan 1.3+ renderer
		VULKAN = 2,
		
		//! use the Metal 3.0+ renderer
		METAL = 3,
	};
	
	struct init_state {
		//! call path of the application binary, should generally be argv[0]
		const char* call_path;
		
		//! floor data path
		const char* data_path;
		
		//! application name
		const char* app_name { "libfloor" };
		
		//! application version
		uint32_t app_version { 1 };
		
		//! floor config file name that must be located in the data path
		//! NOTE: the local/user config will be used if a file with the specified name + ".local" exists
		const char* config_name { "config.json" };
		
		//! if true, will not create a window and will not create a Vulkan surface/swapchain
		bool console_only { false };
		
		//! specifies the default platform type (backend) when !NONE
		//! NOTE: the backend specified in the config file takes priority over this
		PLATFORM_TYPE default_platform { PLATFORM_TYPE::NONE };
		
		//! renderer backend that should be used and initialized
		RENDERER renderer { RENDERER::DEFAULT };
		
		//! min compatible Vulkan API version
		//! NOTE: this still allows Vulkan 1.3 instances/devices
		uint3 vulkan_api_version { 1, 4, 309 };
		
		//! window creation flags
		//! NOTE: fullscreen, borderless and hidpi flags will also be set automatically depending on the config
		struct window_flags_t {
			uint32_t resizable : 1 { 1u };
			uint32_t borderless : 1 {
#if defined(FLOOR_IOS) || defined(FLOOR_VISIONOS)
				1u
#else
				0u
#endif
			};
			uint32_t fullscreen : 1 {
#if defined(FLOOR_IOS) || defined(FLOOR_VISIONOS)
				1u
#else
				0u
#endif
			};
			uint32_t always_on_top : 1 { 0u };
			uint32_t focusable : 1 { 1u };
			uint32_t hidden : 1 { 0u };
			uint32_t maximized : 1 { 0u };
			uint32_t minimized : 1 { 0u };
			uint32_t transparent : 1 { 0u };
			uint32_t _unused : 23 { 0u };
		} window_flags;
		
		//! the position the window should be created at
		//! NOTE: this can be overwritten by the config
		long2 window_position {
			SDL_WINDOWPOS_CENTERED,
			SDL_WINDOWPOS_CENTERED
		};
		
		//! compute/graphics backend context flags that are used/specified during construction
		DEVICE_CONTEXT_FLAGS context_flags { DEVICE_CONTEXT_FLAGS::NONE };
	};
	static bool init(const init_state& state);
	static void destroy();
	static bool is_initialized();
	
	// graphic control functions
	static RENDERER get_renderer();
	static void end_frame();
	static bool is_console_only();
	//! returns the default render/graphics context if Metal or Vulkan is used, or nullptr otherwise
	//! NOTE: this returns the same context as "get_device_context" if Metal/Vulkan are used as the compute backend as well
	static std::shared_ptr<device_context> get_render_context();
	
	// Vulkan-only
	static std::shared_ptr<vulkan_context> get_vulkan_context();
	static const uint3& get_vulkan_api_version();
	
	// Metal-only
	static std::shared_ptr<metal_context> get_metal_context();
	
	// class return functions
	//! returns a pointer to the event instance
	static event* get_event();
	
	// miscellaneous control functions
	//! sets the window caption
	static void set_caption(const std::string& caption);
	//! returns the window caption
	static std::string get_caption();
	
	//! sets the cursors visibility to "state"
	static void set_cursor_visible(const bool state);
	//! returns the cursor visibility stateo
	static bool get_cursor_visible();
	
	static const std::string get_version();
	static const std::string& get_app_name();
	static const uint32_t& get_app_version();
	
	//! sets the data path
	static void set_data_path(const char* data_path = "../data/");
	//! returns the data path
	static std::string get_data_path();
	//! returns the call path
	static std::string get_call_path();
	//! returns data path + str
	static std::string data_path(const std::string& str);
	//! strips the data path from a string
	static std::string strip_data_path(const std::string& str);
	static std::string get_absolute_path();
	
	static bool is_x11_forwarding();
	
	// fps functions
	static uint32_t get_fps();
	static float get_frame_time();
	static bool is_new_fps_count();
	
	// config functions
	static json::document& get_config_doc();
	
	// screen/window
	static SDL_Window* get_window();
	static floor::init_state::window_flags_t get_window_flags();
	static uint32_t get_window_refresh_rate();
	static void raise_main_window();
	static bool get_fullscreen();
	static void set_fullscreen(const bool& state);
	static bool get_vsync();
	static void set_vsync(const bool& state);
	static const uint32_t& get_dpi();
	static bool get_hidpi();
	static bool get_wide_gamut();
	static bool get_hdr();
	static bool get_hdr_linear();
	
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
	
	// VR
	static bool get_vr();
	static bool get_vr_companion();
	static uint32_t get_vr_physical_width();
	static uint32_t get_vr_physical_height();
	static uint2 get_vr_physical_screen_size();
	static const std::string& get_vr_backend();
	static bool get_vr_validation();
	static bool get_vr_trackers();
	static bool get_vr_hand_tracking();
	
	// projection
	static const float& get_fov();
	static const float2& get_near_far_plane();
	static void set_fov(const float& fov);
	static void set_upscaling(const float& upscaling);
	static const float& get_upscaling();
	static float get_scale_factor();
	
	// input
	static uint32_t get_key_repeat();
	static uint32_t get_ldouble_click_time();
	static uint32_t get_mdouble_click_time();
	static uint32_t get_rdouble_click_time();
	
	// memory
	static float get_heap_private_size();
	static float get_heap_shared_size();
	static bool get_metal_shared_only_with_unified_memory();
	
	// toolchain
	static const std::string& get_toolchain_backend();
	static bool get_toolchain_debug();
	static bool get_toolchain_profiling();
	static bool get_toolchain_log_binaries();
	static bool get_toolchain_keep_temp();
	static bool get_toolchain_keep_binaries();
	static bool get_toolchain_use_cache();
	static bool get_toolchain_log_commands();
	
	// generic toolchain
	static const std::string& get_toolchain_default_compiler();
	static const std::string& get_toolchain_default_as();
	static const std::string& get_toolchain_default_dis();
	
	// opencl
	static bool has_opencl_toolchain();
	static const std::string& get_opencl_base_path();
	static const uint32_t& get_opencl_toolchain_version();
	static const std::vector<std::string>& get_opencl_whitelist();
	static const uint32_t& get_opencl_platform();
	static bool get_opencl_verify_spir();
	static bool get_opencl_validate_spirv();
	static bool get_opencl_force_spirv_check();
	static bool get_opencl_disable_spirv();
	static bool get_opencl_spirv_param_workaround();
	static const std::string& get_opencl_compiler();
	static const std::string& get_opencl_as();
	static const std::string& get_opencl_dis();
	static const std::string& get_opencl_objdump();
	static const std::string& get_opencl_spirv_encoder();
	static const std::string& get_opencl_spirv_as();
	static const std::string& get_opencl_spirv_dis();
	static const std::string& get_opencl_spirv_validator();
	
	// cuda
	static bool has_cuda_toolchain();
	static const std::string& get_cuda_base_path();
	static const uint32_t& get_cuda_toolchain_version();
	static const std::vector<std::string>& get_cuda_whitelist();
	static const std::string& get_cuda_compiler();
	static const std::string& get_cuda_as();
	static const std::string& get_cuda_dis();
	static const std::string& get_cuda_objdump();
	static const std::string& get_cuda_force_driver_sm();
	static const std::string& get_cuda_force_compile_sm();
	static const std::string& get_cuda_force_ptx();
	static const uint32_t& get_cuda_max_registers();
	static const bool& get_cuda_jit_verbose();
	static const uint32_t& get_cuda_jit_opt_level();
	static const bool& get_cuda_use_internal_api();
	
	// metal
	static bool has_metal_toolchain();
	static const std::string& get_metal_base_path();
	static const uint32_t& get_metal_toolchain_version();
	static const std::vector<std::string>& get_metal_whitelist();
	static const std::string& get_metal_compiler();
	static const std::string& get_metal_as();
	static const std::string& get_metal_dis();
	static const std::string& get_metallib_dis();
	static const std::string& get_metal_objdump();
	static const uint32_t& get_metal_force_version();
	static const bool& get_metal_soft_printf();
	static const bool& get_metal_dump_reflection_info();
	
	// vulkan
	static bool has_vulkan_toolchain();
	static const std::string& get_vulkan_base_path();
	static const uint32_t& get_vulkan_toolchain_version();
	static const std::vector<std::string>& get_vulkan_whitelist();
	static bool get_vulkan_validation();
	static bool get_vulkan_validate_spirv();
	static const std::string& get_vulkan_compiler();
	static const std::string& get_vulkan_as();
	static const std::string& get_vulkan_dis();
	static const std::string& get_vulkan_objdump();
	static const std::string& get_vulkan_spirv_encoder();
	static const std::string& get_vulkan_spirv_as();
	static const std::string& get_vulkan_spirv_dis();
	static const std::string& get_vulkan_spirv_validator();
	static const bool& get_vulkan_soft_printf();
	static const std::vector<std::string>& get_vulkan_log_binary_filter();
	static const bool& get_vulkan_nvidia_device_diagnostics();
	static const bool& get_vulkan_debug_labels();
	static const bool& get_vulkan_sema_wait_polling();
	
	// host
	static bool has_host_toolchain();
	static const std::string& get_host_base_path();
	static const uint32_t& get_host_toolchain_version();
	static const std::string& get_host_compiler();
	static const std::string& get_host_as();
	static const std::string& get_host_dis();
	static const std::string& get_host_objdump();
	static const bool& get_host_run_on_device();
	static const uint32_t& get_host_max_core_count();
	
	//! returns the default compute/graphics context (CUDA/Host/Metal/OpenCL/Vulkan)
	//! NOTE: if floor was initialized with Vulkan/Metal, this will return the same context as "get_render_context"
	static std::shared_ptr<device_context> get_device_context();
	
protected:
	// static class
	floor(const floor&) = delete;
	~floor() = delete;
	floor& operator=(const floor&) = delete;
	
	static bool init_internal(const init_state& state);
	
	static struct floor_config {
		// screen
		uint32_t width = 1280, height = 720, dpi = 0;
		long2 position { SDL_WINDOWPOS_UNDEFINED };
		bool fullscreen = false;
		bool vsync = false;
		bool hidpi = true;
		bool wide_gamut = true;
		bool hdr = true;
		bool hdr_linear {
#if defined(__APPLE__)
			true
#else
			false
#endif
		};
		bool prefer_native_device_resolution = true;
		
		// VR
		bool vr = false;
		bool vr_companion = true;
		uint32_t vr_width = 0;
		uint32_t vr_height = 0;
		std::string vr_backend;
		bool vr_validation = false;
		bool vr_trackers = true;
		bool vr_hand_tracking = true;
		
		// logging
		uint32_t verbosity = (uint32_t)logger::LOG_TYPE::UNDECORATED;
		bool separate_msg_file = false;
		bool append_mode = false;
		bool log_use_time = true;
		bool log_use_color = true;
		bool log_synchronous = false;
		std::string log_filename;
		std::string msg_filename;
		
		// projection
		float fov = 72.0f;
		float2 near_far_plane { 0.1f, 1000.0f };
		float upscaling = 1.0f;
		
		// input
		uint32_t key_repeat = 200;
		uint32_t ldouble_click_time = 200;
		uint32_t mdouble_click_time = 200;
		uint32_t rdouble_click_time = 200;
		
		// compute
		std::string backend;
		bool debug = false;
		bool profiling = false;
		bool log_binaries = false;
		bool keep_temp = false;
		bool keep_binaries = true;
		bool use_cache = true;
		bool log_commands = false;
		bool internal_skip_toolchain_check = false;
		uint32_t internal_claim_toolchain_version = 0u;
		
		// memory
		float heap_private_size { 0.25f };
		float heap_shared_size { 0.25f };
		bool metal_shared_only_with_unified_memory { false };
		
		// compute toolchain
		std::string default_compiler = "clang";
		std::string default_as = "llvm-as";
		std::string default_dis = "llvm-dis";
		std::string default_objdump = "llvm-objdump";
		
		// opencl
		bool opencl_toolchain_exists = false;
		uint32_t opencl_toolchain_version = 0;
		std::string opencl_base_path;
		uint32_t opencl_platform = 0;
		bool opencl_verify_spir = false;
		bool opencl_validate_spirv = false;
		bool opencl_force_spirv_check = false;
		bool opencl_disable_spirv = false;
		bool opencl_spirv_param_workaround = true;
		std::vector<std::string> opencl_whitelist;
		std::string opencl_compiler = default_compiler;
		std::string opencl_as = default_as;
		std::string opencl_dis = default_dis;
		std::string opencl_objdump = default_objdump;
		std::string opencl_spirv_encoder = "llvm-spirv";
		std::string opencl_spirv_as = "spirv-as";
		std::string opencl_spirv_dis = "spirv-dis";
		std::string opencl_spirv_validator = "spirv-val";
		
		// cuda
		bool cuda_toolchain_exists = false;
		uint32_t cuda_toolchain_version = 0;
		std::string cuda_base_path;
		std::vector<std::string> cuda_whitelist;
		std::string cuda_compiler = default_compiler;
		std::string cuda_as = default_as;
		std::string cuda_dis = default_dis;
		std::string cuda_objdump = default_objdump;
		std::string cuda_force_driver_sm;
		std::string cuda_force_compile_sm;
		std::string cuda_force_ptx;
		uint32_t cuda_max_registers = 32;
		bool cuda_jit_verbose = false;
		uint32_t cuda_jit_opt_level = 4;
		bool cuda_use_internal_api = true;
		
		// metal
		bool metal_toolchain_exists = false;
		uint32_t metal_toolchain_version = 0;
		std::string metal_base_path;
		std::vector<std::string> metal_whitelist;
		std::string metal_compiler = default_compiler;
		std::string metal_as = default_as;
		std::string metal_dis = default_dis;
		std::string metallib_dis = "metallib-dis";
		std::string metal_objdump = default_objdump;
		uint32_t metal_force_version = 0;
		bool metal_soft_printf = false;
		bool metal_dump_reflection_info = false;
		
		// host
		bool host_toolchain_exists = false;
		uint32_t host_toolchain_version = 0;
		std::string host_base_path;
		std::string host_compiler = default_compiler;
		std::string host_as = default_as;
		std::string host_dis = default_dis;
		std::string host_objdump = default_objdump;
		bool host_run_on_device = true;
		uint32_t host_max_core_count = 0;
		
		// vulkan
		bool vulkan_toolchain_exists = false;
		uint32_t vulkan_toolchain_version = 0;
		std::string vulkan_base_path;
		bool vulkan_validation = false;
		bool vulkan_validate_spirv = false;
		std::vector<std::string> vulkan_whitelist;
		std::string vulkan_compiler = default_compiler;
		std::string vulkan_as = default_as;
		std::string vulkan_dis = default_dis;
		std::string vulkan_objdump = default_objdump;
		std::string vulkan_spirv_encoder = "llvm-spirv";
		std::string vulkan_spirv_as = "spirv-as";
		std::string vulkan_spirv_dis = "spirv-dis";
		std::string vulkan_spirv_validator = "spirv-val";
		bool vulkan_soft_printf = false;
		std::vector<std::string> vulkan_log_binary_filter;
		bool vulkan_nvidia_device_diagnostics = false;
		bool vulkan_debug_labels = false;
		bool vulkan_sema_wait_polling = true;
		
		// initial window flags
		floor::init_state::window_flags_t window_flags {};
	} config;
	static json::document config_doc;
	
	// globals
	static std::unique_ptr<event> evt;
	static std::shared_ptr<device_context> dev_ctx;
	static RENDERER renderer;
	static SDL_Window* window;
	
	// VR
	static std::shared_ptr<vr_context> vr_ctx;
	
	// Metal
	static std::shared_ptr<metal_context> metal_ctx;
	
	// Vulkan
	static std::shared_ptr<vulkan_context> vulkan_ctx;
	static uint3 vulkan_api_version;
	
	// path variables
	static std::string datapath;
	static std::string rel_datapath;
	static std::string callpath;
	static std::string abs_bin_path;
	static std::string config_name;
	
	// fps counting
	static uint32_t fps;
	static uint32_t fps_counter;
	static uint64_t fps_time;
	static float frame_time;
	static uint64_t frame_time_sum;
	static uint64_t frame_time_counter;
	static bool new_fps_count;
	
	// window event handlers
	static event::handler event_handler_fnctr;
	static bool event_handler(EVENT_TYPE type, std::shared_ptr<event_object> obj);
	
	// misc
	static std::string app_name;
	static uint32_t app_version;
	static bool console_only;
	static bool cursor_visible;
	static bool x11_forwarding;
	
};

} // namespace fl
