/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2025 Florian Ziesche
 *  
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; version 2 of the License only
 *  
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include <floor/floor.hpp>
#include <floor/floor_version.hpp>
#include <floor/core/sig_handler.hpp>
#include <floor/core/json.hpp>
#include <floor/core/aligned_ptr.hpp>
#include <floor/core/cpp_ext.hpp>
#include <floor/device/opencl/opencl_context.hpp>
#include <floor/device/cuda/cuda_context.hpp>
#include <floor/device/metal/metal_context.hpp>
#include <floor/device/host/host_context.hpp>
#include <floor/device/vulkan/vulkan_context.hpp>
#include <floor/vr/vr_context.hpp>
#include <floor/vr/openvr_context.hpp>
#include <floor/vr/openxr_context.hpp>
#include <chrono>

#if defined(__APPLE__)
#include <floor/darwin/darwin_helper.hpp>
#endif

#if defined(__WINDOWS__)
#include <floor/core/platform_windows.hpp>
#include <memoryapi.h>
#include <floor/core/essentials.hpp> // cleanup

#if defined(_MSC_VER)
#include <direct.h>
#endif
#endif

#if defined(__linux__)
#include <sys/resource.h>
#include <X11/Xlib.h>
#endif

namespace fl {
using namespace std::literals;

//// init statics
struct floor::floor_config floor::config;
json::document floor::config_doc;

// globals
enum class FLOOR_INIT_STATUS : uint32_t {
	UNINITIALIZED = 0,
	IN_PROGRESS = 1,
	DESTROYING = 2,
	SUCCESSFUL = 3,
	FAILURE = 4,
};
static std::atomic<FLOOR_INIT_STATUS> floor_init_status { FLOOR_INIT_STATUS::UNINITIALIZED };
std::unique_ptr<event> floor::evt;
std::shared_ptr<device_context> floor::dev_ctx;
floor::RENDERER floor::renderer { floor::RENDERER::DEFAULT };
SDL_Window* floor::window { nullptr };

// VR
std::shared_ptr<vr_context> floor::vr_ctx;

// Metal
std::shared_ptr<metal_context> floor::metal_ctx;

// Vulkan
std::shared_ptr<vulkan_context> floor::vulkan_ctx;
uint3 floor::vulkan_api_version;

// path variables
std::string floor::datapath {};
std::string floor::rel_datapath {};
std::string floor::callpath {};
std::string floor::abs_bin_path  {};
std::string floor::config_name = "config.json";

// fps counting
uint32_t floor::fps = 0u;
uint32_t floor::fps_counter = 0u;
uint64_t floor::fps_time = 0u;
float floor::frame_time = 0.0f;
uint64_t floor::frame_time_sum = 0u;
uint64_t floor::frame_time_counter = 0u;
bool floor::new_fps_count = false;

// window event handlers
event::handler floor::event_handler_fnctr { &floor::event_handler };

// misc
std::string floor::app_name { "libfloor" };
uint32_t floor::app_version { 1 };
bool floor::console_only = false;
bool floor::cursor_visible = true;
bool floor::x11_forwarding = false;

bool floor::init(const init_state& state) {
	// return if already initialized
	FLOOR_INIT_STATUS expected_init { FLOOR_INIT_STATUS::UNINITIALIZED };
	if(!floor_init_status.compare_exchange_strong(expected_init, FLOOR_INIT_STATUS::IN_PROGRESS)) {
		// wait while someone else is initializing libfloor
		while(floor_init_status == FLOOR_INIT_STATUS::IN_PROGRESS) {
			std::this_thread::sleep_for(5ms);
		}
		return (floor_init_status == FLOOR_INIT_STATUS::SUCCESSFUL);
	}
	// else: we're now IN_PROGRESS
	
	// sanity check
#if defined(__APPLE__)
	if(state.renderer == RENDERER::VULKAN) {
		std::cerr << "Vulkan is not supported on macOS / iOS" << std::endl;
		floor_init_status = FLOOR_INIT_STATUS::FAILURE;
		return false;
	}
#endif
#if defined(FLOOR_NO_VULKAN)
	if(state.renderer == RENDERER::VULKAN) {
		std::cerr << "can't use the Vulkan renderer when libfloor was compiled without Vulkan support" << std::endl;
		floor_init_status = FLOOR_INIT_STATUS::FAILURE;
		return false;
	}
#endif
#if defined(FLOOR_NO_METAL)
	if(state.renderer == RENDERER::METAL) {
		std::cerr << "can't use the Metal renderer when libfloor was compiled without Metal support" << std::endl;
		floor_init_status = FLOOR_INIT_STATUS::FAILURE;
		return false;
	}
#endif
	
#if defined(_SC_PAGESIZE)
	{
		const auto page_size = uint64_t(sysconf(_SC_PAGESIZE));
		if (page_size != aligned_ptr<int>::page_size) {
			log_error("unexpected page size: $' (expected $')", page_size, aligned_ptr<int>::page_size);
			floor_init_status = FLOOR_INIT_STATUS::FAILURE;
			return false;
		}
	}
#elif defined(__WINDOWS__)
	{
		SYSTEM_INFO info;
		GetNativeSystemInfo(&info);
		const auto page_size = uint64_t(info.dwPageSize);
		if (page_size != aligned_ptr<int>::page_size) {
			log_error("unexpected page size: $' (expected $')", page_size, aligned_ptr<int>::page_size);
			floor_init_status = FLOOR_INIT_STATUS::FAILURE;
			return false;
		}
	}
#else
#error "can not retrieve page size"
#endif
	
	//
	register_segfault_handler();
	
	floor::callpath = state.call_path;
	floor::datapath = state.call_path;
	floor::rel_datapath = state.data_path;
	floor::abs_bin_path = state.call_path;
	floor::config_name = state.config_name;
	floor::console_only = state.console_only;
	floor::app_name = state.app_name;
	floor::app_version = state.app_version;
	floor::vulkan_api_version = state.vulkan_api_version;
	
	// get working directory
	char working_dir[4096];
	memset(working_dir, 0, 4096);
	if(getcwd(working_dir, 4095) == nullptr) {
		std::cerr << "failed to retrieve the current working directory" << std::endl;
		floor_init_status = FLOOR_INIT_STATUS::FAILURE;
		return false;
	}
	
	// no '/' -> relative path
	if(rel_datapath[0] != '/') {
		datapath = datapath.substr(0, datapath.rfind(FLOOR_OS_DIR_SLASH) + 1) + rel_datapath;
	}
	// absolute path
	else datapath = rel_datapath;
	
	// same for abs bin path (no '/' -> relative path)
	if(abs_bin_path[0] != '/') {
		bool direct_rel_path = false;
		if(abs_bin_path.size() > 2 && abs_bin_path[0] == '.' && abs_bin_path[1] == '/') {
			direct_rel_path = true;
		}
		abs_bin_path = (FLOOR_OS_DIR_SLASH +
						abs_bin_path.substr(direct_rel_path ? 2 : 0,
											abs_bin_path.rfind(FLOOR_OS_DIR_SLASH) + 1 - (direct_rel_path ? 2 : 0)));
		abs_bin_path = working_dir + abs_bin_path; // just add the working dir -> done
	}
	// else: we already have the absolute path
	
#if defined(CYGWIN)
	callpath = "./";
	datapath = callpath_;
	datapath = datapath.substr(0, datapath.rfind('/') + 1) + rel_datapath;
#endif
	
	// create
#if !defined(__WINDOWS__) && !defined(CYGWIN)
	if(datapath.size() > 0 && datapath[0] == '.') {
		// strip leading '.' from datapath if there is one
		datapath.erase(0, 1);
		datapath = working_dir + datapath;
	}
#elif defined(CYGWIN)
	// do nothing
#else
	size_t strip_pos = datapath.find("\\.\\");
	if(strip_pos != std::string::npos) {
		datapath.erase(strip_pos, 3);
	}
	
	bool add_bin_path = (working_dir == datapath.substr(0, datapath.length() - 1)) ? false : true;
	if(!add_bin_path) datapath = working_dir + std::string("\\") + (add_bin_path ? datapath : "");
	else {
		if(datapath[datapath.length() - 1] == '/') {
			datapath = datapath.substr(0, datapath.length() - 1);
		}
		datapath += std::string("\\");
	}
	
#endif
	
#if defined(__APPLE__)
#if !defined(FLOOR_IOS) && !defined(FLOOR_VISIONOS)
	// check if datapath contains a 'MacOS' string (indicates that the binary is called from within an macOS .app or via complete path from the shell)
	if(datapath.find("MacOS") != std::string::npos) {
		// if so, add "../../../" to the datapath, since we have to relocate the datapath if the binary is inside an .app
		datapath.insert(datapath.find("MacOS") + 6, "../../../");
	}
#else
	// reset
	datapath = state.data_path;
	rel_datapath = datapath;
#endif
#endif
	
	// condense datapath and abs_bin_path
	datapath = core::strip_path(datapath);
	abs_bin_path = core::strip_path(abs_bin_path);
	
	cursor_visible = true;
	x11_forwarding = false;
	
	fps = 0;
	fps_counter = 0;
	fps_time = 0;
	frame_time = 0.0f;
	frame_time_sum = 0;
	frame_time_counter = 0;
	new_fps_count = false;
	
	// load config
#if !defined(_MSC_VER)
	const auto floor_sys_path = [](const std::string& str) {
		return "/opt/floor/data/" + str;
	};
#else
	const auto floor_sys_path = [](const std::string& str) {
		return core::expand_path_with_env("%ProgramW6432%/floor/data/") + str;
	};
#endif
	
	std::string config_filename = config_name;
	if(file_io::is_file(data_path(config_name + ".local"))) config_filename = data_path(config_name + ".local");
	else if(file_io::is_file(data_path(config_name))) config_filename = data_path(config_name);
	else if(file_io::is_file(floor_sys_path(config_name + ".local"))) config_filename = floor_sys_path(config_name + ".local");
	else if(file_io::is_file(floor_sys_path(config_name))) config_filename = floor_sys_path(config_name);
	config_doc = json::create_document(config_filename);
	
	json::json_array default_toolchain_paths {
		json::json_value("/opt/floor/toolchain"),
		json::json_value("/c/msys/opt/floor/toolchain"),
		json::json_value("%ProgramW6432%/floor/toolchain"),
		json::json_value("%ProgramFiles%/floor/toolchain")
	};
	json::json_array opencl_toolchain_paths, cuda_toolchain_paths, metal_toolchain_paths, vulkan_toolchain_paths, host_toolchain_paths;
	if(config_doc.valid) {
		config.width = config_doc.get<uint32_t>("screen.width", 1280);
		config.height = config_doc.get<uint32_t>("screen.height", 720);
		config.position.x = config_doc.get<int64_t>("screen.position.x", SDL_WINDOWPOS_UNDEFINED);
		config.position.y = config_doc.get<int64_t>("screen.position.y", SDL_WINDOWPOS_UNDEFINED);
		config.fullscreen = config_doc.get<bool>("screen.fullscreen", false);
		config.vsync = config_doc.get<bool>("screen.vsync", false);
		config.dpi = config_doc.get<uint32_t>("screen.dpi", 0);
		config.hidpi = config_doc.get<bool>("screen.hidpi", true);
		config.wide_gamut = config_doc.get<bool>("screen.wide_gamut", true);
		config.hdr = config_doc.get<bool>("screen.hdr", true);
		config.hdr_linear = config_doc.get<bool>("screen.hdr_linear",
#if defined(__APPLE__)
												 true
#else
												 false
#endif
												 );
		config.prefer_native_device_resolution = config_doc.get<bool>("screen.prefer_native_device_resolution", true);
		
#if !defined(FLOOR_NO_OPENVR) || !defined(FLOOR_NO_OPENXR)
		config.vr = config_doc.get<bool>("screen.vr.enabled", false);
#else
		config.vr = false;
#endif
		config.vr_companion = config_doc.get<bool>("screen.vr.companion", true);
		config.vr_width = config_doc.get<uint32_t>("screen.vr.width", 0);
		config.vr_height = config_doc.get<uint32_t>("screen.vr.height", 0);
		config.vr_backend = config_doc.get<std::string>("screen.vr.backend", "");
		config.vr_validation = config_doc.get<bool>("screen.vr.validation", false);
		config.vr_trackers = config_doc.get<bool>("screen.vr.trackers", true);
		config.vr_hand_tracking = config_doc.get<bool>("screen.vr.hand_tracking", true);
		
		config.verbosity = config_doc.get<uint32_t>("logging.verbosity", (uint32_t)logger::LOG_TYPE::UNDECORATED);
		config.separate_msg_file = config_doc.get<bool>("logging.separate_msg_file", false);
		config.append_mode = config_doc.get<bool>("logging.append_mode", false);
		config.log_use_time = config_doc.get<bool>("logging.use_time", true);
		config.log_use_color = config_doc.get<bool>("logging.use_color", true);
		config.log_synchronous = config_doc.get<bool>("logging.synchronous", false);
		config.log_filename = config_doc.get<std::string>("logging.log_filename", "");
		config.msg_filename = config_doc.get<std::string>("logging.msg_filename", "");
		
		config.fov = config_doc.get<float>("projection.fov", 72.0f);
		config.near_far_plane.x = config_doc.get<float>("projection.near", 1.0f);
		config.near_far_plane.y = config_doc.get<float>("projection.far", 1000.0f);
		config.upscaling = config_doc.get<float>("projection.upscaling", -1.0f);
		
		config.key_repeat = config_doc.get<uint32_t>("input.key_repeat", 200);
		config.ldouble_click_time = config_doc.get<uint32_t>("input.ldouble_click_time", 200);
		config.mdouble_click_time = config_doc.get<uint32_t>("input.mdouble_click_time", 200);
		config.rdouble_click_time = config_doc.get<uint32_t>("input.rdouble_click_time", 200);
		
		config.heap_private_size = math::clamp(config_doc.get<float>("memory.heap.private_size", 0.25f), 0.0f, 1.0f);
		config.heap_shared_size = math::clamp(config_doc.get<float>("memory.heap.shared_size", 0.25f), 0.0f, 1.0f);
		config.metal_shared_only_with_unified_memory = config_doc.get<bool>("memory.metal.shared_only_with_unified_memory", false);
		
		config.backend = config_doc.get<std::string>("toolchain.backend", "");
		config.debug = config_doc.get<bool>("toolchain.debug", false);
		config.profiling = config_doc.get<bool>("toolchain.profiling", false);
		config.log_binaries = config_doc.get<bool>("toolchain.log_binaries", false);
		config.keep_temp = config_doc.get<bool>("toolchain.keep_temp", false);
		config.keep_binaries = config_doc.get<bool>("toolchain.keep_binaries", false);
		config.use_cache = config_doc.get<bool>("toolchain.use_cache", true);
		config.log_commands = config_doc.get<bool>("toolchain.log_commands", false);
		config.internal_skip_toolchain_check = config_doc.get<bool>("toolchain._skip_toolchain_check", false);
		config.internal_claim_toolchain_version = config_doc.get<uint32_t>("toolchain._claim_toolchain_version", 0u);
		
		//
		const auto extract_string_array_set = []<bool convert_to_lower = true>(std::vector<std::string>& ret, const std::string& config_entry_name) {
			std::unordered_set<std::string> str_set;
			const auto elems = config_doc.get<json::json_array>(config_entry_name);
			for (const auto& elem : elems) {
				auto [is_str, str] = elem.get<std::string>();
				if (!is_str) {
					log_error("array element must be a string!");
					continue;
				}
				if (str == "") {
					continue;
				}
				if constexpr (convert_to_lower) {
					core::str_to_lower_inplace(str);
				}
				str_set.emplace(std::move(str));
			}
			
			for (const auto& elem : str_set) {
				ret.push_back(elem);
			}
		};
		
		config.default_compiler = config_doc.get<std::string>("toolchain.generic.compiler", "clang");
		config.default_as = config_doc.get<std::string>("toolchain.generic.as", "llvm-as");
		config.default_dis = config_doc.get<std::string>("toolchain.generic.dis", "llvm-dis");
		
		const auto config_toolchain_paths = config_doc.get<json::json_array>("toolchain.generic.paths");
		if(!config_toolchain_paths.empty()) default_toolchain_paths = config_toolchain_paths;
		
		opencl_toolchain_paths = config_doc.get<json::json_array>("toolchain.opencl.paths", default_toolchain_paths);
		config.opencl_platform = config_doc.get<uint32_t>("toolchain.opencl.platform", 0);
		config.opencl_verify_spir = config_doc.get<bool>("toolchain.opencl.verify_spir", false);
		config.opencl_validate_spirv = config_doc.get<bool>("toolchain.opencl.validate_spirv", false);
		config.opencl_force_spirv_check = config_doc.get<bool>("toolchain.opencl.force_spirv", false);
		config.opencl_disable_spirv = config_doc.get<bool>("toolchain.opencl.disable_spirv", false);
		config.opencl_spirv_param_workaround = config_doc.get<bool>("toolchain.opencl.spirv_param_workaround", false);
		extract_string_array_set(config.opencl_whitelist, "toolchain.opencl.whitelist");
		config.opencl_compiler = config_doc.get<std::string>("toolchain.opencl.compiler", config.default_compiler);
		config.opencl_as = config_doc.get<std::string>("toolchain.opencl.as", config.default_as);
		config.opencl_dis = config_doc.get<std::string>("toolchain.opencl.dis", config.default_dis);
		config.opencl_spirv_encoder = config_doc.get<std::string>("toolchain.opencl.spirv-encoder", config.opencl_spirv_encoder);
		config.opencl_spirv_as= config_doc.get<std::string>("toolchain.opencl.spirv-as", config.opencl_spirv_as);
		config.opencl_spirv_dis = config_doc.get<std::string>("toolchain.opencl.spirv-dis", config.opencl_spirv_dis);
		config.opencl_spirv_validator = config_doc.get<std::string>("toolchain.opencl.spirv-validator", config.opencl_spirv_validator);
		
		cuda_toolchain_paths = config_doc.get<json::json_array>("toolchain.cuda.paths", default_toolchain_paths);
		config.cuda_force_driver_sm = config_doc.get<std::string>("toolchain.cuda.force_driver_sm", "");
		config.cuda_force_compile_sm = config_doc.get<std::string>("toolchain.cuda.force_compile_sm", "");
		config.cuda_force_ptx = config_doc.get<std::string>("toolchain.cuda.force_ptx", "");
		config.cuda_max_registers = config_doc.get<uint32_t>("toolchain.cuda.max_registers", 32);
		config.cuda_jit_verbose = config_doc.get<bool>("toolchain.cuda.jit_verbose", false);
		config.cuda_jit_opt_level = config_doc.get<uint32_t>("toolchain.cuda.jit_opt_level", 4);
		config.cuda_use_internal_api = config_doc.get<bool>("toolchain.cuda.use_internal_api", true);
		extract_string_array_set(config.cuda_whitelist, "toolchain.cuda.whitelist");
		config.cuda_compiler = config_doc.get<std::string>("toolchain.cuda.compiler", config.default_compiler);
		config.cuda_as = config_doc.get<std::string>("toolchain.cuda.as", config.default_as);
		config.cuda_dis = config_doc.get<std::string>("toolchain.cuda.dis", config.default_dis);
		
		metal_toolchain_paths = config_doc.get<json::json_array>("toolchain.metal.paths", default_toolchain_paths);
		extract_string_array_set(config.metal_whitelist, "toolchain.metal.whitelist");
		config.metal_compiler = config_doc.get<std::string>("toolchain.metal.compiler", config.default_compiler);
		config.metal_as = config_doc.get<std::string>("toolchain.metal.as", config.default_as);
		config.metal_dis = config_doc.get<std::string>("toolchain.metal.dis", config.default_dis);
		config.metallib_dis = config_doc.get<std::string>("toolchain.metal.metallib_dis", config.metallib_dis);
		config.metal_force_version = config_doc.get<uint32_t>("toolchain.metal.force_version", 0);
		config.metal_soft_printf = config_doc.get<bool>("toolchain.metal.soft_printf", false);
		config.metal_dump_reflection_info = config_doc.get<bool>("toolchain.metal.dump_reflection_info", false);
		
		vulkan_toolchain_paths = config_doc.get<json::json_array>("toolchain.vulkan.paths", default_toolchain_paths);
		config.vulkan_validation = config_doc.get<bool>("toolchain.vulkan.validation", true);
		config.vulkan_validate_spirv = config_doc.get<bool>("toolchain.vulkan.validate_spirv", false);
		extract_string_array_set(config.vulkan_whitelist, "toolchain.vulkan.whitelist");
		config.vulkan_compiler = config_doc.get<std::string>("toolchain.vulkan.compiler", config.default_compiler);
		config.vulkan_as = config_doc.get<std::string>("toolchain.vulkan.as", config.default_as);
		config.vulkan_dis = config_doc.get<std::string>("toolchain.vulkan.dis", config.default_dis);
		config.vulkan_spirv_encoder = config_doc.get<std::string>("toolchain.vulkan.spirv-encoder", config.vulkan_spirv_encoder);
		config.vulkan_spirv_as= config_doc.get<std::string>("toolchain.vulkan.spirv-as", config.vulkan_spirv_as);
		config.vulkan_spirv_dis = config_doc.get<std::string>("toolchain.vulkan.spirv-dis", config.vulkan_spirv_dis);
		config.vulkan_spirv_validator = config_doc.get<std::string>("toolchain.vulkan.spirv-validator", config.vulkan_spirv_validator);
		config.vulkan_soft_printf = config_doc.get<bool>("toolchain.vulkan.soft_printf", false);
		extract_string_array_set.operator()<false>(config.vulkan_log_binary_filter, "toolchain.vulkan.log_binary_filter");
		config.vulkan_nvidia_device_diagnostics = config_doc.get<bool>("toolchain.vulkan.nvidia_device_diagnostics", false);
		config.vulkan_debug_labels = config_doc.get<bool>("toolchain.vulkan.debug_labels", false);
		config.vulkan_sema_wait_polling = config_doc.get<bool>("toolchain.vulkan.sema_wait_polling", true);
		
		host_toolchain_paths = config_doc.get<json::json_array>("toolchain.host.paths", default_toolchain_paths);
		config.host_compiler = config_doc.get<std::string>("toolchain.host.compiler", config.default_compiler);
		config.host_as = config_doc.get<std::string>("toolchain.host.as", config.default_as);
		config.host_dis = config_doc.get<std::string>("toolchain.host.dis", config.default_dis);
		config.host_run_on_device = config_doc.get<bool>("toolchain.host.device", true);
		config.host_max_core_count = config_doc.get<uint32_t>("toolchain.host.max_core_count", 0);
	}
	
	// handle toolchain paths
	if(opencl_toolchain_paths.empty()) opencl_toolchain_paths = default_toolchain_paths;
	if(cuda_toolchain_paths.empty()) cuda_toolchain_paths = default_toolchain_paths;
	if(metal_toolchain_paths.empty()) metal_toolchain_paths = default_toolchain_paths;
	if(vulkan_toolchain_paths.empty()) vulkan_toolchain_paths = default_toolchain_paths;
	if(host_toolchain_paths.empty()) host_toolchain_paths = default_toolchain_paths;
	
	const auto get_viable_toolchain_path = [](const json::json_array& paths,
											  uint32_t& toolchain_version,
											  std::string& compiler,
											  std::string& as,
											  std::string& dis,
											  std::string& objdump,
											  // <min/max required toolchain version, bin name>
											  std::vector<std::pair<uint2, std::string*>> additional_bins = {}) {
#if defined(__WINDOWS__)
		// on windows: always add .exe to all binaries + expand paths (handles "%Something%/path/to/sth")
		compiler = core::expand_path_with_env(compiler + ".exe");
		as = core::expand_path_with_env(as + ".exe");
		dis = core::expand_path_with_env(dis + ".exe");
		objdump = core::expand_path_with_env(objdump + ".exe");
		for(auto& bin : additional_bins) {
			*bin.second = core::expand_path_with_env(*bin.second + ".exe");
		}
#endif
		
		for(const auto& path : paths) {
			auto [path_is_string, path_str_] = path.get<std::string>();
			if (!path_is_string) {
				log_error("toolchain path must be a string!");
				continue;
			}
			
			const auto path_str = core::expand_path_with_env(path_str_);
			
			if(!file_io::is_file(path_str + "/bin/" + compiler)) continue;
			if(!file_io::is_file(path_str + "/bin/" + as)) continue;
			if(!file_io::is_file(path_str + "/bin/" + dis)) continue;
			if(!file_io::is_file(path_str + "/bin/" + objdump)) continue;
			if(!file_io::is_directory(path_str + "/clang")) continue;
			if(!file_io::is_directory(path_str + "/include")) continue;
			if(!file_io::is_directory(path_str + "/libcxx")) continue;
			
			if (!config.internal_skip_toolchain_check) {
				// get the toolchain (clang) version
				// NOTE: this also checks if clang is actually callable (-> non-viable if not)
				std::string version_output = "";
				core::system("\"" + path_str + "/bin/" + compiler + "\" --version", version_output);
				
				// e.g. "clang version 3.5.2 (...)" -> want 3.5.2
				static constexpr const char clang_version_str[] { "clang version " };
				const auto clang_version_pos = version_output.find(clang_version_str);
				if(clang_version_pos == std::string::npos) continue;
				const auto version_start_pos = clang_version_pos + std::size(clang_version_str) - 1 /* \0 */;
				const auto next_space_pos = version_output.find(' ', version_start_pos);
				if(next_space_pos == std::string::npos) continue;
				if(next_space_pos - version_start_pos < 5 /* at least len("3.5.2") */) continue;
				
				const auto major_dot_pos = version_output.find('.', version_start_pos + 1);
				if(major_dot_pos == std::string::npos || major_dot_pos > next_space_pos) continue;
				
				const auto minor_dot_pos = version_output.find('.', major_dot_pos + 1);
				if(minor_dot_pos == std::string::npos || minor_dot_pos > next_space_pos) continue;
				
				toolchain_version = 10000u * stou(version_output.substr(version_start_pos, major_dot_pos - version_start_pos));
				toolchain_version += 100u * stou(version_output.substr(major_dot_pos + 1, minor_dot_pos - major_dot_pos));
				toolchain_version += stou(version_output.substr(minor_dot_pos + 1, next_space_pos - minor_dot_pos));
			} else {
				toolchain_version = config.internal_claim_toolchain_version;
			}
			
			// check additional bins after getting the toolchain version
			bool found_additional_bins = true;
			for(const auto& bin : additional_bins) {
				// ignore all additional binaries which are only required by a later toolchain version
				if(toolchain_version < bin.first.x ||
				   toolchain_version > bin.first.y) {
					continue;
				}
				if(!file_io::is_file(path_str + "/bin/" + *bin.second)) {
					found_additional_bins = false;
					break;
				}
			}
			if(!found_additional_bins) continue;
			
			return path_str + "/";
		}
		return ""s;
	};
	
	{
		// -> OpenCL toolchain
		config.opencl_base_path = get_viable_toolchain_path(opencl_toolchain_paths,
															config.opencl_toolchain_version,
															config.opencl_compiler,
															config.opencl_as, config.opencl_dis, config.opencl_objdump,
															std::vector<std::pair<uint2, std::string*>> {
			{ { 140006u, ~0u }, &config.opencl_spirv_encoder },
			{ { 140006u, ~0u }, &config.opencl_spirv_as },
			{ { 140006u, ~0u }, &config.opencl_spirv_dis },
			{ { 140006u, ~0u }, &config.opencl_spirv_validator },
		});
		if (config.opencl_base_path == "") {
#if !defined(FLOOR_IOS) && !defined(FLOOR_VISIONOS) // not available on iOS/visionOS anyways
			log_error("OpenCL toolchain is unavailable - could not find a complete toolchain in any specified toolchain path!");
#endif
		} else {
			config.opencl_toolchain_exists = true;
			config.opencl_compiler.insert(0, config.opencl_base_path + "bin/");
			config.opencl_as.insert(0, config.opencl_base_path + "bin/");
			config.opencl_dis.insert(0, config.opencl_base_path + "bin/");
			config.opencl_spirv_encoder.insert(0, config.opencl_base_path + "bin/");
			config.opencl_spirv_as.insert(0, config.opencl_base_path + "bin/");
			config.opencl_spirv_dis.insert(0, config.opencl_base_path + "bin/");
			config.opencl_spirv_validator.insert(0, config.opencl_base_path + "bin/");
		}
		
		// -> CUDA toolchain
		config.cuda_base_path = get_viable_toolchain_path(cuda_toolchain_paths,
														  config.cuda_toolchain_version,
														  config.cuda_compiler,
														  config.cuda_as, config.cuda_dis, config.cuda_objdump);
		if (config.cuda_base_path == "") {
#if !defined(FLOOR_IOS) && !defined(FLOOR_VISIONOS) // not available on iOS/visionOS anyways
			log_error("CUDA toolchain is unavailable - could not find a complete toolchain in any specified toolchain path!");
#endif
		} else {
			config.cuda_toolchain_exists = true;
			config.cuda_compiler.insert(0, config.cuda_base_path + "bin/");
			config.cuda_as.insert(0, config.cuda_base_path + "bin/");
			config.cuda_dis.insert(0, config.cuda_base_path + "bin/");
		}
		
		// -> Metal toolchain
		config.metal_base_path = get_viable_toolchain_path(metal_toolchain_paths,
														   config.metal_toolchain_version,
														   config.metal_compiler,
														   config.metal_as, config.metal_dis, config.metal_objdump,
														   std::vector<std::pair<uint2, std::string*>> {
			{ { 140006u, ~0u }, &config.metallib_dis },
		});
#if defined(FLOOR_IOS) || defined(FLOOR_VISIONOS)
		// toolchain doesn't exist on an iOS/visionOS device (usually), so just pretend and don't fail
		if (config.metal_base_path == "") {
			config.metal_base_path = "/opt/floor/toolchain";
		}
#else
		if (config.metal_base_path == "") {
			log_error("Metal toolchain is unavailable - could not find a complete toolchain in any specified toolchain path!");
		} else
#endif
		{
			config.metal_toolchain_exists = true;
			config.metal_compiler.insert(0, config.metal_base_path + "bin/");
			config.metal_as.insert(0, config.metal_base_path + "bin/");
			config.metal_dis.insert(0, config.metal_base_path + "bin/");
		}
		
		// -> Vulkan toolchain
		config.vulkan_base_path = get_viable_toolchain_path(vulkan_toolchain_paths,
															config.vulkan_toolchain_version,
															config.vulkan_compiler,
															config.vulkan_as, config.vulkan_dis, config.vulkan_objdump,
															std::vector<std::pair<uint2, std::string*>> {
			{ { 140006u, ~0u }, &config.vulkan_spirv_encoder },
			{ { 140006u, ~0u }, &config.vulkan_spirv_as },
			{ { 140006u, ~0u }, &config.vulkan_spirv_dis },
			{ { 140006u, ~0u }, &config.vulkan_spirv_validator },
		});
		if (config.vulkan_base_path == "") {
#if !defined(FLOOR_IOS) && !defined(FLOOR_VISIONOS) // not available on iOS/visionOS anyways
			log_error("Vulkan toolchain is unavailable - could not find a complete toolchain in any specified toolchain path!");
#endif
		} else {
			config.vulkan_toolchain_exists = true;
			config.vulkan_compiler.insert(0, config.vulkan_base_path + "bin/");
			config.vulkan_as.insert(0, config.vulkan_base_path + "bin/");
			config.vulkan_dis.insert(0, config.vulkan_base_path + "bin/");
			config.vulkan_spirv_encoder.insert(0, config.vulkan_base_path + "bin/");
			config.vulkan_spirv_as.insert(0, config.vulkan_base_path + "bin/");
			config.vulkan_spirv_dis.insert(0, config.vulkan_base_path + "bin/");
			config.vulkan_spirv_validator.insert(0, config.vulkan_base_path + "bin/");
		}
		
		// -> host toolchain
		config.host_base_path = get_viable_toolchain_path(host_toolchain_paths,
														  config.host_toolchain_version,
														  config.host_compiler,
														  config.host_as, config.host_dis, config.host_objdump);
		if (config.host_base_path == "") {
#if !defined(FLOOR_IOS) && !defined(FLOOR_VISIONOS) // not available on iOS/visionOS anyways
			log_error("Host-Compute toolchain is unavailable - could not find a complete toolchain in any specified toolchain path!");
#endif
		} else {
			config.host_toolchain_exists = true;
			config.host_compiler.insert(0, config.host_base_path + "bin/");
			config.host_as.insert(0, config.host_base_path + "bin/");
			config.host_dis.insert(0, config.host_base_path + "bin/");
		}
	}
	
	// init logger and print out floor info
	logger::init((uint32_t)config.verbosity, config.separate_msg_file, config.append_mode,
				 config.log_use_time, config.log_use_color, config.log_synchronous,
				 config.log_filename, config.msg_filename);
	log_debug("$", (FLOOR_VERSION_STRING).c_str());
	
	[[maybe_unused]] const uint64_t wanted_locked_memory_size = std::max(core::get_hw_thread_count(), 1u) * 32u * 1024u * 1024u;
#if defined(__linux__)
	// change max open files and max locked memory to a reasonable limit on Linux
	struct rlimit nofile_limit;
	memset(&nofile_limit, 0, sizeof(rlimit));
	if (getrlimit(RLIMIT_NOFILE, &nofile_limit) == 0) {
		const auto wanted_nofile = (decltype(nofile_limit.rlim_cur))core::get_hw_thread_count() * 256u;
		if (nofile_limit.rlim_cur < wanted_nofile) {
			nofile_limit.rlim_cur = wanted_nofile;
			if (nofile_limit.rlim_cur > nofile_limit.rlim_max) {
				log_warn("wanted to set NOFILE limit to $', but can only set it to $'", wanted_nofile, nofile_limit.rlim_max);
				nofile_limit.rlim_cur = nofile_limit.rlim_max;
			}
			setrlimit(RLIMIT_NOFILE, &nofile_limit);
		}
	} else {
		log_error("failed to query NOFILE limit: $", core::get_system_error());
	}
	
	struct rlimit memlock_limit;
	memset(&memlock_limit, 0, sizeof(rlimit));
	if (getrlimit(RLIMIT_MEMLOCK, &memlock_limit) == 0) {
		const auto wanted_memlock = (decltype(memlock_limit.rlim_cur))wanted_locked_memory_size;
		if (memlock_limit.rlim_cur < wanted_memlock) {
			memlock_limit.rlim_cur = wanted_memlock;
			if (memlock_limit.rlim_cur > memlock_limit.rlim_max) {
				log_warn("wanted to set MEMLOCK limit to $', but can only set it to $'", wanted_memlock, memlock_limit.rlim_max);
				memlock_limit.rlim_cur = memlock_limit.rlim_max;
			}
			setrlimit(RLIMIT_MEMLOCK, &memlock_limit);
		}
	} else {
		log_error("failed to query MEMLOCK limit: $", core::get_system_error());
	}
#elif defined(__WINDOWS__)
	// change max locked memory / working set + disable hard quota limits on Windows
	if (SetProcessWorkingSetSizeEx(GetCurrentProcess(),
								   wanted_locked_memory_size / 2u,
								   wanted_locked_memory_size,
								   QUOTA_LIMITS_HARDWS_MIN_DISABLE |
								   QUOTA_LIMITS_HARDWS_MAX_DISABLE) == 0) {
		log_warn("failed to set max working set to $': $", wanted_locked_memory_size, core::get_system_error());
	}
#endif
	
	// choose the renderer
	if (state.renderer == RENDERER::DEFAULT) {
#if !defined(__APPLE__)
		// always use Vulkan
		renderer = RENDERER::VULKAN;
#else
		// always use Metal
		renderer = RENDERER::METAL;
#endif
	} else if (state.renderer == RENDERER::VULKAN) {
#if !defined(__APPLE__)
		renderer = RENDERER::VULKAN;
#else
		// obviously can't use Vulkan on macOS / iOS
		log_error("Vulkan renderer is not available on macOS / iOS - using Metal now");
		renderer = RENDERER::METAL;
#endif
	} else if (state.renderer == RENDERER::METAL) {
#if !defined(FLOOR_NO_METAL)
		renderer = RENDERER::METAL;
#else
		// obviously can't use Metal
		log_error("Metal renderer is not available - using VULKAN now");
		renderer = RENDERER::VULKAN;
#endif
	} else {
		renderer = RENDERER::NONE;
	}
	assert(renderer != RENDERER::DEFAULT && "must have selected a renderer");
	
	//
	evt = std::make_unique<event>();
	evt->add_internal_event_handler(event_handler_fnctr, EVENT_TYPE::WINDOW_RESIZE);
	
	//
	const auto successful_init = init_internal(state);
	floor_init_status = (successful_init ? FLOOR_INIT_STATUS::SUCCESSFUL : FLOOR_INIT_STATUS::FAILURE);
	return successful_init;
}

void floor::destroy() {
	// only destroy if initialized (either successful or failure)
	FLOOR_INIT_STATUS expected_init { FLOOR_INIT_STATUS::SUCCESSFUL };
	if(!floor_init_status.compare_exchange_strong(expected_init, FLOOR_INIT_STATUS::DESTROYING)) {
		// immediate return if in any of these states (should not have been calling destroy!)
		if(expected_init == FLOOR_INIT_STATUS::DESTROYING ||
		   expected_init == FLOOR_INIT_STATUS::IN_PROGRESS ||
		   expected_init == FLOOR_INIT_STATUS::UNINITIALIZED) {
			return;
		}
		
		expected_init = FLOOR_INIT_STATUS::FAILURE;
		if(!floor_init_status.compare_exchange_strong(expected_init, FLOOR_INIT_STATUS::DESTROYING)) {
			// wasn't in either status, abort
			return;
		}
	}
	log_debug("destroying floor ...");
	
	device_image::destroy_minify_programs();
	
	evt->set_vr_context(nullptr);
	vr_ctx = nullptr;
	metal_ctx = nullptr;
	vulkan_ctx = nullptr;
	dev_ctx = nullptr;
	
	// delete this at the end, b/c other classes will remove event handlers
	if(evt != nullptr) {
		evt->remove_event_handler(event_handler_fnctr);
		evt = nullptr;
	}
	
	if(!console_only) {
		if(window != nullptr) {
			SDL_DestroyWindow(window);
			window = nullptr;
		}
	}
	SDL_Quit();
	
	log_debug("floor destroyed!");
	floor_init_status = FLOOR_INIT_STATUS::UNINITIALIZED;
}

bool floor::init_internal(const init_state& state) {
	log_debug("initializing floor");
	
	// initialize SDL (with all sub-systems if !console-only)
	if (!console_only) {
		// enable Windows DPI awareness *before* we init the SDL video sub-system
		if (!SDL_SetHint("SDL_WINDOWS_DPI_AWARENESS", "system")) {
			log_error("failed to set Windows DPI awareness");
		}
	}
	if (!SDL_Init(console_only ? 0 : (SDL_INIT_VIDEO | SDL_INIT_JOYSTICK |
									  SDL_INIT_HAPTIC | SDL_INIT_GAMEPAD | SDL_INIT_SENSOR))) {
		log_error("failed to initialize SDL: $", SDL_GetError());
		return false;
	} else {
		log_debug("SDL initialized");
	}
	atexit(SDL_Quit);
	
	// only initialize compute/graphics backends and create a window when not in console-only mode
	if (!console_only) {
		// detect x11 forwarding
#if !defined(__WINDOWS__) && !defined(FLOOR_IOS) && !defined(FLOOR_VISIONOS)
		if (SDL_strcmp(SDL_GetCurrentVideoDriver(), "x11") == 0) {
			const auto display = getenv("DISPLAY");
			if (display != nullptr && strlen(display) > 1 && display[0] != ':') {
				if (getenv("SSH_CONNECTION") != nullptr) {
					x11_forwarding = true;
					log_debug("detected X11 forwarding");
				}
			}
		}
#endif
		
		// set window creation properties
		config.window_flags = state.window_flags;
		SDL_PropertiesID wnd_props = SDL_CreateProperties();
		SDL_SetBooleanProperty(wnd_props, SDL_PROP_WINDOW_CREATE_RESIZABLE_BOOLEAN, state.window_flags.resizable);
		SDL_SetBooleanProperty(wnd_props, SDL_PROP_WINDOW_CREATE_BORDERLESS_BOOLEAN, state.window_flags.borderless);
		SDL_SetBooleanProperty(wnd_props, SDL_PROP_WINDOW_CREATE_FULLSCREEN_BOOLEAN, state.window_flags.fullscreen);
		SDL_SetBooleanProperty(wnd_props, SDL_PROP_WINDOW_CREATE_ALWAYS_ON_TOP_BOOLEAN, state.window_flags.always_on_top);
		SDL_SetBooleanProperty(wnd_props, SDL_PROP_WINDOW_CREATE_FOCUSABLE_BOOLEAN, state.window_flags.focusable);
		SDL_SetBooleanProperty(wnd_props, SDL_PROP_WINDOW_CREATE_HIDDEN_BOOLEAN, state.window_flags.hidden);
		SDL_SetBooleanProperty(wnd_props, SDL_PROP_WINDOW_CREATE_MAXIMIZED_BOOLEAN, state.window_flags.maximized);
		SDL_SetBooleanProperty(wnd_props, SDL_PROP_WINDOW_CREATE_MINIMIZED_BOOLEAN, state.window_flags.minimized);
		SDL_SetBooleanProperty(wnd_props, SDL_PROP_WINDOW_CREATE_TRANSPARENT_BOOLEAN, state.window_flags.transparent);
		
		// we have our own Metal/Vulkan rendering
		SDL_SetBooleanProperty(wnd_props, SDL_PROP_WINDOW_CREATE_EXTERNAL_GRAPHICS_CONTEXT_BOOLEAN, true);
		
#if !defined(FLOOR_IOS) && !defined(FLOOR_VISIONOS)
		auto window_pos = state.window_position;
		if (config.position.x != SDL_WINDOWPOS_UNDEFINED) {
			window_pos.x = config.position.x;
		}
		if (config.position.y != SDL_WINDOWPOS_UNDEFINED) {
			window_pos.y = config.position.y;
		}
		
		if (config.fullscreen) {
			SDL_SetBooleanProperty(wnd_props, SDL_PROP_WINDOW_CREATE_FULLSCREEN_BOOLEAN, true);
			SDL_SetBooleanProperty(wnd_props, SDL_PROP_WINDOW_CREATE_BORDERLESS_BOOLEAN, true);
			window_pos = 0;
			log_debug("fullscreen enabled");
		} else {
			log_debug("fullscreen disabled");
		}
#endif
		
		log_debug("vsync $", config.vsync ? "enabled" : "disabled");
		
		// handle hidpi
		if (config.hidpi) {
			SDL_SetBooleanProperty(wnd_props, SDL_PROP_WINDOW_CREATE_HIGH_PIXEL_DENSITY_BOOLEAN, true);
		}
		log_debug("hidpi $", config.hidpi ? "enabled" : "disabled");
		
		if (renderer == RENDERER::METAL || renderer == RENDERER::VULKAN) {
			if (config.wide_gamut) {
				log_debug("wide-gamut enabled");
			}
		}
		
#if defined(FLOOR_IOS) || defined(FLOOR_VISIONOS)
		SDL_DisplayMode fullscreen_mode;
		SDL_zero(fullscreen_mode);
		fullscreen_mode.format = SDL_PIXELFORMAT_RGBA8888;
		fullscreen_mode.w = (int)config.width;
		fullscreen_mode.h = (int)config.height;
#endif
		
#if 0 // for debugging purposes: dump all display modes
		const auto disp_num = SDL_GetNumDisplayModes(0);
		log_debug("#disp modes: $", disp_num);
		for(int32_t i = 0; i < disp_num; ++i) {
			SDL_DisplayMode mode;
			SDL_GetDisplayMode(0, i, &mode);
			log_debug("disp mode #$: $ x $, $ Hz, format $X", i, mode.w, mode.h, mode.refresh_rate, mode.format);
		}
#endif
		
		// create screen
		SDL_SetStringProperty(wnd_props, SDL_PROP_WINDOW_CREATE_TITLE_STRING, app_name.c_str());
		SDL_SetNumberProperty(wnd_props, SDL_PROP_WINDOW_CREATE_WIDTH_NUMBER, config.width);
		SDL_SetNumberProperty(wnd_props, SDL_PROP_WINDOW_CREATE_HEIGHT_NUMBER, config.height);
#if !defined(FLOOR_IOS) && !defined(FLOOR_VISIONOS)
		SDL_SetNumberProperty(wnd_props, SDL_PROP_WINDOW_CREATE_X_NUMBER, window_pos.x);
		SDL_SetNumberProperty(wnd_props, SDL_PROP_WINDOW_CREATE_Y_NUMBER, window_pos.y);
#endif
		window = SDL_CreateWindowWithProperties(wnd_props);
		if (window == nullptr) {
			log_error("couldn't create window: $", SDL_GetError());
			return false;
		}
		
#if defined(FLOOR_IOS) || defined(FLOOR_VISIONOS)
		// on iOS/visionOS, be more insistent on the window size
		SDL_SetWindowSize(window, (int)config.width, (int)config.height);
#endif
		
		int2 wnd_size;
		SDL_GetWindowSizeInPixels(window, (int*)&wnd_size.x, (int*)&wnd_size.y);
		config.width = (wnd_size.x > 0 ? uint32_t(wnd_size.x) : 1u);
		config.height = (wnd_size.y > 0 ? uint32_t(wnd_size.y) : 1u);
		log_debug("video mode set: w$ h$", config.width, config.height);
		
#if defined(FLOOR_IOS) || defined(FLOOR_VISIONOS)
		if (config.prefer_native_device_resolution) {
			const auto primary_display = SDL_GetPrimaryDisplay();
			if (const auto desktop_display_mode = SDL_GetDesktopDisplayMode(primary_display); desktop_display_mode) {
				log_msg("using desktop display mode: $*$px $Hz, fmt: $, density: $", desktop_display_mode->w, desktop_display_mode->h,
						desktop_display_mode->refresh_rate, desktop_display_mode->format, desktop_display_mode->pixel_density);
				memcpy(&fullscreen_mode, desktop_display_mode, sizeof(SDL_DisplayMode));
			}
		}
		if (!SDL_SetWindowFullscreenMode(window, &fullscreen_mode)) {
			log_error("can't set up fullscreen display mode: $", SDL_GetError());
			return false;
		}
		SDL_GetWindowSize(window, (int*)&config.width, (int*)&config.height);
		log_debug("fullscreen mode set: w$ h$", config.width, config.height);
		SDL_ShowWindow(window);
#endif
		
#if defined(__APPLE__)
#if !defined(FLOOR_IOS) && !defined(FLOOR_VISIONOS)
		darwin_helper::create_app_delegate();
#endif
		
		// cache window scale factor while we're on the main thread
		(void)darwin_helper::get_scale_factor(window, true /* force query */);
#endif
		
		log_debug("scale factor: $", get_scale_factor());
		
#if !defined(FLOOR_NO_OPENVR) || !defined(FLOOR_NO_OPENXR)
		// create a VR context if this is enabled and we want to create a supported renderer
		if (config.vr && (renderer == RENDERER::VULKAN || renderer == RENDERER::METAL)) {
			log_debug("initializing VR");
#if !defined(FLOOR_NO_VULKAN) && !defined(FLOOR_NO_OPENXR) // Vulkan is required for this
			if ((config.vr_backend == "openxr" || config.vr_backend.empty()) &&
				renderer == RENDERER::VULKAN) {
				vr_ctx = std::make_unique<openxr_context>();
			}
#endif
#if !defined(FLOOR_NO_OPENVR)
			if (!vr_ctx && (config.vr_backend == "openvr" || config.vr_backend.empty())) {
				vr_ctx = std::make_unique<openvr_context>();
			}
#endif
			
			if (!vr_ctx) {
				log_error("unknown or unsupported VR backend: $", config.vr_backend);
			} else {
				log_msg("VR platform: $", vr_platform_to_string(vr_ctx->get_platform_type()));
				if (!vr_ctx->is_valid()) {
					vr_ctx = nullptr;
				} else {
					if (config.vr_width == 0) {
						config.vr_width = vr_ctx->get_recommended_render_size().x;
					}
					if (config.vr_height == 0) {
						config.vr_height = vr_ctx->get_recommended_render_size().y;
					}
					log_debug("VR per-eye render size: w$ h$", config.vr_width, config.vr_height);
					
					evt->set_vr_context(vr_ctx.get());
				}
			}
		}
#endif
		
		// try to create the renderer that was specified
#if !defined(FLOOR_NO_METAL)
		if (renderer == RENDERER::METAL) {
			// create the Metal context
			metal_ctx = std::make_shared<metal_context>(state.context_flags, config.metal_toolchain_exists, true,
												   vr_ctx.get(), config.metal_whitelist);
			if (metal_ctx == nullptr || !metal_ctx->is_supported()) {
				log_error("failed to create the Metal renderer context");
				metal_ctx = nullptr;
				return false;
			}
		}
#endif
#if !defined(FLOOR_NO_VULKAN)
		if (renderer == RENDERER::VULKAN) {
			// create the Vulkan context
			vulkan_ctx = std::make_shared<vulkan_context>(state.context_flags, config.vulkan_toolchain_exists, true,
													 vr_ctx.get(), config.vulkan_whitelist);
			if (vulkan_ctx == nullptr || !vulkan_ctx->is_supported()) {
				log_error("failed to create the Vulkan renderer context");
				vulkan_ctx = nullptr;
				return false;
			}
		}
#endif
	}
	
#if !defined(FLOOR_NO_OPENVR) || !defined(FLOOR_NO_OPENXR)
	// kill the VR context if we couldn't create a supported renderer
	if (vr_ctx && renderer != RENDERER::VULKAN && renderer != RENDERER::METAL) {
		evt->set_vr_context(nullptr);
		vr_ctx = nullptr;
	}
#endif
	
	if(!console_only) {
		log_debug("window $created and acquired!",
				  renderer == RENDERER::METAL ? "and Metal context " :
				  renderer == RENDERER::VULKAN ? "and Vulkan context " : "");
		
		if(SDL_GetCurrentVideoDriver() == nullptr) {
			log_error("couldn't get video driver: $!", SDL_GetError());
		}
		else log_debug("video driver: $", SDL_GetCurrentVideoDriver());
		
		// NOTE: Vulkan has already been initialized at this point
		
		evt->set_ldouble_click_time(config.ldouble_click_time);
		evt->set_rdouble_click_time(config.rdouble_click_time);
		evt->set_mdouble_click_time(config.mdouble_click_time);
		
		// retrieve dpi info
		if(config.dpi == 0) {
#if defined(__APPLE__)
			config.dpi = darwin_helper::get_dpi(window);
#else
			size2 display_res { 1.0f, 1.0f };
			float2 display_phys_size { 1.0f, 1.0f };
#if defined(__WINDOWS__)
			auto hdc = (HDC)SDL_GetPointerProperty(SDL_GetWindowProperties(window), SDL_PROP_WINDOW_WIN32_HDC_POINTER, nullptr);
			if (hdc) {
				display_res = { (size_t)GetDeviceCaps(hdc, HORZRES), (size_t)GetDeviceCaps(hdc, VERTRES) };
				display_phys_size = int2(GetDeviceCaps(hdc, HORZSIZE), GetDeviceCaps(hdc, VERTSIZE)).cast<float>();
			}
#else // x11
			if (SDL_strcmp(SDL_GetCurrentVideoDriver(), "x11") == 0) {
				Display* display = (Display*)SDL_GetPointerProperty(SDL_GetWindowProperties(window), SDL_PROP_WINDOW_X11_DISPLAY_POINTER, nullptr);
				if (display) {
					display_res = { (size_t)DisplayWidth(display, 0), (size_t)DisplayHeight(display, 0) };
					display_phys_size = { float(DisplayWidthMM(display, 0)), float(DisplayHeightMM(display, 0)) };
				}
			} else if (SDL_strcmp(SDL_GetCurrentVideoDriver(), "wayland") == 0) {
				// TODO: implement this
			}
#endif
			const float2 display_dpi((float(display_res.x) / display_phys_size.x) * 25.4f,
									 (float(display_res.y) / display_phys_size.y) * 25.4f);
			config.dpi = (uint32_t)floorf(std::max(display_dpi.x, display_dpi.y));
#endif
		}
		
		// set dpi lower bound to 72
		if(config.dpi < 72) config.dpi = 72;
		log_debug("dpi: $", config.dpi);
	}
	
	// always create and init a device context (even in console-only mode)
	{
		// get the platform that was set in the config
		PLATFORM_TYPE config_platform_type = PLATFORM_TYPE::NONE;
		if(config.backend == "opencl") config_platform_type = PLATFORM_TYPE::OPENCL;
		else if(config.backend == "cuda") config_platform_type = PLATFORM_TYPE::CUDA;
		else if(config.backend == "metal") config_platform_type = PLATFORM_TYPE::METAL;
		else if(config.backend == "host") config_platform_type = PLATFORM_TYPE::HOST;
		else if(config.backend == "vulkan") config_platform_type = PLATFORM_TYPE::VULKAN;
		
		// default compute backends (will try these in order, using the first working one)
#if defined(__APPLE__)
		// only Metal is available
		std::vector<PLATFORM_TYPE> platform_defaults { PLATFORM_TYPE::METAL };
#else // Linux, Windows, ...
		std::vector<PLATFORM_TYPE> platform_defaults { PLATFORM_TYPE::CUDA, PLATFORM_TYPE::VULKAN, PLATFORM_TYPE::OPENCL };
#endif
		// always start with the configured one (if one has been set)
		if(config_platform_type != PLATFORM_TYPE::NONE) {
			// erase existing entry first, so that we don't try to init it twice
			core::erase_if(platform_defaults, [&config_platform_type](const auto& iter) { return (*iter == config_platform_type); });
			platform_defaults.insert(platform_defaults.begin(), config_platform_type);
		}
		// always end with Host-Compute (as a fallback), if it isn't already part of the list
		if(find(platform_defaults.begin(), platform_defaults.end(), PLATFORM_TYPE::HOST) == platform_defaults.end()) {
			platform_defaults.emplace_back(PLATFORM_TYPE::HOST);
		}
		
		// iterate over all platforms in the default set, using the first one that works
		dev_ctx = nullptr; // just to be sure
		for(const auto& backend : platform_defaults) {
			switch(backend) {
				case PLATFORM_TYPE::CUDA:
#if !defined(FLOOR_NO_CUDA)
					log_debug("initializing CUDA ...");
					dev_ctx = std::make_shared<cuda_context>(state.context_flags, config.cuda_toolchain_exists, config.cuda_whitelist);
#endif
					break;
				case PLATFORM_TYPE::OPENCL:
#if !defined(FLOOR_NO_OPENCL)
					log_debug("initializing OpenCL ...");
					dev_ctx = std::make_shared<opencl_context>(state.context_flags,
															  config.opencl_toolchain_exists,
															  config.opencl_platform,
															  config.opencl_whitelist);
#endif
					break;
				case PLATFORM_TYPE::METAL:
#if !defined(FLOOR_NO_METAL)
					if (metal_ctx != nullptr) {
						dev_ctx = metal_ctx;
					} else {
						log_debug("initializing Metal ...");
						dev_ctx = std::make_shared<metal_context>(state.context_flags, config.metal_toolchain_exists, false,
																 vr_ctx.get(), config.metal_whitelist);
					}
#endif
					break;
				case PLATFORM_TYPE::HOST:
#if !defined(FLOOR_NO_HOST_COMPUTE)
					log_debug("initializing Host-Compute ...");
					dev_ctx = std::make_shared<host_context>(state.context_flags, config.host_toolchain_exists);
#endif
					break;
				case PLATFORM_TYPE::VULKAN:
#if !defined(FLOOR_NO_VULKAN)
					if (vulkan_ctx != nullptr) {
						dev_ctx = vulkan_ctx;
					} else {
						log_debug("initializing Vulkan ...");
						dev_ctx = std::make_shared<vulkan_context>(state.context_flags, config.vulkan_toolchain_exists, false,
																  vr_ctx.get(), config.vulkan_whitelist);
					}
#endif
					break;
				default: break;
			}
			
			if(dev_ctx != nullptr) {
				if(!dev_ctx->is_supported()) {
					log_error("failed to create a \"$\" context, trying next backend ...", platform_type_to_string(backend));
					dev_ctx = nullptr;
				}
				else {
					break; // success
				}
			}
		}
		
		// this is rather bad
		if(dev_ctx == nullptr) {
			log_error("failed to create any device context!");
		}
	}
	
	return true;
}

bool floor::is_initialized() {
	return (floor_init_status.load() == FLOOR_INIT_STATUS::SUCCESSFUL);
}

void floor::set_screen_size(const uint2& screen_size) {
	if(screen_size.x == config.width && screen_size.y == config.height) return;
	config.width = screen_size.x;
	config.height = screen_size.y;
	SDL_SetWindowSize(window, (int)config.width, (int)config.height);
	
#if !defined(FLOOR_IOS) && !defined(FLOOR_VISIONOS)
	SDL_SetWindowPosition(window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
#endif
}

void floor::set_fullscreen(const bool& state) {
	if(state == config.fullscreen) return;
	config.fullscreen = state;
	if (!SDL_SetWindowFullscreen(window, state)) {
		log_error("failed to $ fullscreen: $!",
				  (state ? "enable" : "disable"), SDL_GetError());
	}
	evt->add_event(EVENT_TYPE::WINDOW_RESIZE,
				   std::make_shared<window_resize_event>(SDL_GetTicks(),
													uint2(config.width, config.height)));
	// TODO: border?
}

void floor::set_vsync(const bool& state) {
	if(state == config.vsync) return;
	config.vsync = state;
}

void floor::end_frame() {
	frame_time_sum += SDL_GetTicks() - frame_time_counter;
	
	// handle fps count
	fps_counter++;
	if (SDL_GetTicks() - fps_time > 1000u) {
		fps = fps_counter;
		new_fps_count = true;
		fps_counter = 0;
		fps_time = SDL_GetTicks();
		
		frame_time = (float)frame_time_sum / (float)fps;
		frame_time_sum = 0;
	}
	frame_time_counter = SDL_GetTicks();
}

void floor::set_caption(const std::string& caption) {
	SDL_SetWindowTitle(window, caption.c_str());
}

std::string floor::get_caption() {
	return SDL_GetWindowTitle(window);
}

void floor::set_cursor_visible(const bool state) {
	floor::cursor_visible = state;
	if (floor::cursor_visible) {
		SDL_ShowCursor();
	} else {
		SDL_HideCursor();
	}
}

bool floor::get_cursor_visible() {
	return floor::cursor_visible;
}

event* floor::get_event() {
	return floor::evt.get();
}

void floor::set_data_path(const char* data_path) {
	floor::datapath = data_path;
}

std::string floor::get_data_path() {
	return datapath;
}

std::string floor::get_call_path() {
	return callpath;
}

std::string floor::data_path(const std::string& str) {
	if(str.length() == 0) return datapath;
	return datapath + str;
}

std::string floor::strip_data_path(const std::string& str) {
	if(str.length() == 0) return "";
	return core::find_and_replace(str, datapath, "");
}

uint32_t floor::get_fps() {
	new_fps_count = false;
	return floor::fps;
}

float floor::get_frame_time() {
	return floor::frame_time;
}

bool floor::is_new_fps_count() {
	return floor::new_fps_count;
}

bool floor::get_fullscreen() {
	return config.fullscreen;
}

bool floor::get_vsync() {
	return config.vsync;
}

uint32_t floor::get_width() {
	return config.width;
}

uint32_t floor::get_height() {
	return config.height;
}

uint2 floor::get_screen_size() {
	return uint2(config.width, config.height);
}

uint32_t floor::get_physical_width() {
	uint32_t width = config.width;
	if (floor::window) {
		int2 wnd_size;
		if (SDL_GetWindowSizeInPixels(floor::window, &wnd_size.x, &wnd_size.y) && wnd_size.x > 0) {
			width = uint32_t(wnd_size.x);
		}
	} else {
		width = uint32_t(double(width) * double(get_scale_factor()));
	}
	return width;
}

uint32_t floor::get_physical_height() {
	uint32_t height = config.height;
	if (floor::window) {
		int2 wnd_size;
		if (SDL_GetWindowSizeInPixels(floor::window, &wnd_size.x, &wnd_size.y) && wnd_size.y > 0) {
			height = uint32_t(wnd_size.y);
		}
	} else {
		height = uint32_t(double(height) * double(get_scale_factor()));
	}
	return height;
}

uint2 floor::get_physical_screen_size() {
	uint2 ret { config.width, config.height };
	if (floor::window) {
		int2 wnd_size;
		if (SDL_GetWindowSizeInPixels(floor::window, &wnd_size.x, &wnd_size.y) && wnd_size.x > 0 && wnd_size.y > 0) {
			ret = { uint32_t(wnd_size.x), uint32_t(wnd_size.y) };
		}
	} else {
		ret = (ret.cast<double>() * double(get_scale_factor())).cast<uint32_t>();
	}
	return ret;
}

uint32_t floor::get_key_repeat() {
	return config.key_repeat;
}

uint32_t floor::get_ldouble_click_time() {
	return config.ldouble_click_time;
}

uint32_t floor::get_mdouble_click_time() {
	return config.mdouble_click_time;
}

uint32_t floor::get_rdouble_click_time() {
	return config.rdouble_click_time;
}

float floor::get_heap_private_size() {
	return config.heap_private_size;
}

float floor::get_heap_shared_size() {
	return config.heap_shared_size;
}

bool floor::get_metal_shared_only_with_unified_memory() {
	return config.metal_shared_only_with_unified_memory;
}

SDL_Window* floor::get_window() {
	return window;
}

floor::init_state::window_flags_t floor::get_window_flags() {
	return config.window_flags;
}

uint32_t floor::get_window_refresh_rate() {
	// SDL_GetWindowFullscreenMode is useless/broken, so get the display index + retrieve the mode that way instead
	const auto display_index = SDL_GetDisplayForWindow(window);
	if (display_index == 0) {
		log_error("failed to retrieve window display index");
		return 60;
	} else {
		const SDL_DisplayMode* mode = SDL_GetCurrentDisplayMode(display_index);
		if (!mode) {
			log_error("failed to retrieve current display mode (for display #$)", display_index);
			return 60;
		}
		return (mode->refresh_rate < 0 ? 60 : uint32_t(mode->refresh_rate));
	}
}

void floor::raise_main_window() {
	(void)SDL_RaiseWindow(window);
}

std::shared_ptr<device_context> floor::get_render_context() {
#if defined(__APPLE__)
#if !defined(FLOOR_NO_METAL)
	return metal_ctx;
#else
	return nullptr;
#endif
#else
#if !defined(FLOOR_NO_VULKAN)
	return vulkan_ctx;
#else
	return nullptr;
#endif
#endif
}

std::shared_ptr<vulkan_context> floor::get_vulkan_context() {
	return vulkan_ctx;
}

std::shared_ptr<metal_context> floor::get_metal_context() {
	return metal_ctx;
}

const std::string floor::get_version() {
	return FLOOR_VERSION_STRING;
}

const float& floor::get_fov() {
	return config.fov;
}

void floor::set_fov(const float& fov) {
	if(const_math::is_equal(config.fov, fov)) return;
	config.fov = fov;
	evt->add_event(EVENT_TYPE::WINDOW_RESIZE,
				   std::make_shared<window_resize_event>(SDL_GetTicks(), uint2(config.width, config.height)));
}

const float2& floor::get_near_far_plane() {
	return config.near_far_plane;
}

const uint32_t& floor::get_dpi() {
	return config.dpi;
}

bool floor::get_hidpi() {
	return config.hidpi;
}

bool floor::get_wide_gamut() {
	return config.wide_gamut;
}

bool floor::get_hdr() {
	return config.hdr;
}

bool floor::get_hdr_linear() {
	return config.hdr_linear;
}

bool floor::get_vr() {
	return config.vr;
}

bool floor::get_vr_companion() {
	return config.vr_companion;
}

uint32_t floor::get_vr_physical_width() {
	return config.vr_width;
}

uint32_t floor::get_vr_physical_height() {
	return config.vr_height;
}

uint2 floor::get_vr_physical_screen_size() {
	return { config.vr_width, config.vr_height };
}

const std::string& floor::get_vr_backend() {
	return config.vr_backend;
}

bool floor::get_vr_validation() {
	return config.vr_validation;
}

bool floor::get_vr_trackers() {
	return config.vr_trackers;
}

bool floor::get_vr_hand_tracking() {
	return config.vr_hand_tracking;
}

json::document& floor::get_config_doc() {
	return config_doc;
}

bool floor::event_handler(EVENT_TYPE type, std::shared_ptr<event_object> obj) {
	if (type == EVENT_TYPE::WINDOW_RESIZE) {
		const auto& wnd_evt = (const window_resize_event&)*obj;
		config.width = wnd_evt.size.x;
		config.height = wnd_evt.size.y;
		
#if defined(__APPLE__)
		// cache window scale factor while we're on the main thread
		(void)darwin_helper::get_scale_factor(window, true /* force query */);
#endif
		
		return true;
	}
	return false;
}

void floor::set_upscaling(const float& upscaling_) {
	config.upscaling = upscaling_;
}

const float& floor::get_upscaling() {
	return config.upscaling;
}

float floor::get_scale_factor() {
	// don't call any window functions in console-only mode
	if (console_only) {
		return 1.0f;
	}
	
#if defined(__APPLE__)
	return darwin_helper::get_scale_factor(window);
#else
	return (config.upscaling <= 0.0f ? SDL_GetWindowDisplayScale(floor::window) : config.upscaling);
#endif
}

std::string floor::get_absolute_path() {
	return abs_bin_path;
}

const std::string& floor::get_toolchain_backend() {
	return config.backend;
}
bool floor::get_toolchain_debug() {
	return config.debug;
}
bool floor::get_toolchain_profiling() {
	return config.profiling;
}
bool floor::get_toolchain_log_binaries() {
	return config.log_binaries;
}
bool floor::get_toolchain_keep_temp() {
	return config.keep_temp;
}
bool floor::get_toolchain_keep_binaries() {
	return config.keep_binaries;
}
bool floor::get_toolchain_use_cache() {
	return config.use_cache;
}
bool floor::get_toolchain_log_commands() {
	return config.log_commands;
}

const std::string& floor::get_toolchain_default_compiler() {
	return config.default_compiler;
}
const std::string& floor::get_toolchain_default_as() {
	return config.default_as;
}
const std::string& floor::get_toolchain_default_dis() {
	return config.default_dis;
}

bool floor::has_opencl_toolchain() {
	return config.opencl_toolchain_exists;
}
const std::string& floor::get_opencl_base_path() {
	return config.opencl_base_path;
}
const uint32_t& floor::get_opencl_toolchain_version() {
	return config.opencl_toolchain_version;
}
const std::vector<std::string>& floor::get_opencl_whitelist() {
	return config.opencl_whitelist;
}
const uint32_t& floor::get_opencl_platform() {
	return config.opencl_platform;
}
bool floor::get_opencl_verify_spir() {
	return config.opencl_verify_spir;
}
bool floor::get_opencl_validate_spirv() {
	return config.opencl_validate_spirv;
}
bool floor::get_opencl_force_spirv_check() {
	return config.opencl_force_spirv_check;
}
bool floor::get_opencl_disable_spirv() {
	return config.opencl_disable_spirv;
}
bool floor::get_opencl_spirv_param_workaround() {
	return config.opencl_spirv_param_workaround;
}
const std::string& floor::get_opencl_compiler() {
	return config.opencl_compiler;
}
const std::string& floor::get_opencl_as() {
	return config.opencl_as;
}
const std::string& floor::get_opencl_dis() {
	return config.opencl_dis;
}
const std::string& floor::get_opencl_objdump() {
	return config.opencl_objdump;
}
const std::string& floor::get_opencl_spirv_encoder() {
	return config.opencl_spirv_encoder;
}
const std::string& floor::get_opencl_spirv_as() {
	return config.opencl_spirv_as;
}
const std::string& floor::get_opencl_spirv_dis() {
	return config.opencl_spirv_dis;
}
const std::string& floor::get_opencl_spirv_validator() {
	return config.opencl_spirv_validator;
}

bool floor::has_cuda_toolchain() {
	return config.cuda_toolchain_exists;
}
const std::string& floor::get_cuda_base_path() {
	return config.cuda_base_path;
}
const uint32_t& floor::get_cuda_toolchain_version() {
	return config.cuda_toolchain_version;
}
const std::vector<std::string>& floor::get_cuda_whitelist() {
	return config.cuda_whitelist;
}
const std::string& floor::get_cuda_compiler() {
	return config.cuda_compiler;
}
const std::string& floor::get_cuda_as() {
	return config.cuda_as;
}
const std::string& floor::get_cuda_dis() {
	return config.cuda_dis;
}
const std::string& floor::get_cuda_objdump() {
	return config.cuda_objdump;
}
const std::string& floor::get_cuda_force_driver_sm() {
	return config.cuda_force_driver_sm;
}
const std::string& floor::get_cuda_force_compile_sm() {
	return config.cuda_force_compile_sm;
}
const std::string& floor::get_cuda_force_ptx() {
	return config.cuda_force_ptx;
}
const uint32_t& floor::get_cuda_max_registers() {
	return config.cuda_max_registers;
}
const bool& floor::get_cuda_jit_verbose() {
	return config.cuda_jit_verbose;
}
const uint32_t& floor::get_cuda_jit_opt_level() {
	return config.cuda_jit_opt_level;
}
const bool& floor::get_cuda_use_internal_api() {
	return config.cuda_use_internal_api;
}

bool floor::has_metal_toolchain() {
	return config.metal_toolchain_exists;
}
const std::string& floor::get_metal_base_path() {
	return config.metal_base_path;
}
const uint32_t& floor::get_metal_toolchain_version() {
	return config.metal_toolchain_version;
}
const std::vector<std::string>& floor::get_metal_whitelist() {
	return config.metal_whitelist;
}
const std::string& floor::get_metal_compiler() {
	return config.metal_compiler;
}
const std::string& floor::get_metal_as() {
	return config.metal_as;
}
const std::string& floor::get_metal_dis() {
	return config.metal_dis;
}
const std::string& floor::get_metallib_dis() {
	return config.metallib_dis;
}
const std::string& floor::get_metal_objdump() {
	return config.metal_objdump;
}
const uint32_t& floor::get_metal_force_version() {
	return config.metal_force_version;
}
const bool& floor::get_metal_soft_printf() {
	return config.metal_soft_printf;
}
const bool& floor::get_metal_dump_reflection_info() {
	return config.metal_dump_reflection_info;
}

bool floor::has_vulkan_toolchain() {
	return config.vulkan_toolchain_exists;
}
const std::string& floor::get_vulkan_base_path() {
	return config.vulkan_base_path;
}
const uint32_t& floor::get_vulkan_toolchain_version() {
	return config.vulkan_toolchain_version;
}
const std::vector<std::string>& floor::get_vulkan_whitelist() {
	return config.vulkan_whitelist;
}
bool floor::get_vulkan_validation() {
	return config.vulkan_validation;
}
bool floor::get_vulkan_validate_spirv() {
	return config.vulkan_validate_spirv;
}
const std::string& floor::get_vulkan_compiler() {
	return config.vulkan_compiler;
}
const std::string& floor::get_vulkan_as() {
	return config.vulkan_as;
}
const std::string& floor::get_vulkan_dis() {
	return config.vulkan_dis;
}
const std::string& floor::get_vulkan_objdump() {
	return config.vulkan_objdump;
}
const std::string& floor::get_vulkan_spirv_encoder() {
	return config.vulkan_spirv_encoder;
}
const std::string& floor::get_vulkan_spirv_as() {
	return config.vulkan_spirv_as;
}
const std::string& floor::get_vulkan_spirv_dis() {
	return config.vulkan_spirv_dis;
}
const std::string& floor::get_vulkan_spirv_validator() {
	return config.vulkan_spirv_validator;
}
const bool& floor::get_vulkan_soft_printf() {
	return config.vulkan_soft_printf;
}
const std::vector<std::string>& floor::get_vulkan_log_binary_filter() {
	return config.vulkan_log_binary_filter;
}
const bool& floor::get_vulkan_nvidia_device_diagnostics() {
	return config.vulkan_nvidia_device_diagnostics;
}
const bool& floor::get_vulkan_debug_labels() {
	return config.vulkan_debug_labels;
}
const bool& floor::get_vulkan_sema_wait_polling() {
	return config.vulkan_sema_wait_polling;
}

bool floor::has_host_toolchain() {
	return config.host_toolchain_exists;
}
const std::string& floor::get_host_base_path() {
	return config.host_base_path;
}
const uint32_t& floor::get_host_toolchain_version() {
	return config.host_toolchain_version;
}
const std::string& floor::get_host_compiler() {
	return config.host_compiler;
}
const std::string& floor::get_host_as() {
	return config.host_as;
}
const std::string& floor::get_host_dis() {
	return config.host_dis;
}
const std::string& floor::get_host_objdump() {
	return config.host_objdump;
}
const bool& floor::get_host_run_on_device() {
	return config.host_run_on_device;
}
const uint32_t& floor::get_host_max_core_count() {
	return config.host_max_core_count;
}

std::shared_ptr<device_context> floor::get_device_context() {
	return dev_ctx;
}

bool floor::is_console_only() {
	return console_only;
}

const uint3& floor::get_vulkan_api_version() {
	return vulkan_api_version;
}

const std::string& floor::get_app_name() {
	return app_name;
}

const uint32_t& floor::get_app_version() {
	return app_version;
}

floor::RENDERER floor::get_renderer() {
	return renderer;
}

bool floor::is_x11_forwarding() {
	return x11_forwarding;
}

} // namespace fl
