/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2019 Florian Ziesche
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

#include <floor/floor/floor.hpp>
#include <floor/floor/floor_version.hpp>
#include <floor/core/gl_support.hpp>
#include <floor/audio/audio_controller.hpp>
#include <floor/core/sig_handler.hpp>
#include <floor/core/json.hpp>
#include <floor/compute/opencl/opencl_compute.hpp>
#include <floor/compute/cuda/cuda_compute.hpp>
#include <floor/compute/metal/metal_compute.hpp>
#include <floor/compute/host/host_compute.hpp>
#include <floor/compute/vulkan/vulkan_compute.hpp>

#if defined(__APPLE__)
#include <floor/darwin/darwin_helper.hpp>
#endif

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
static atomic<FLOOR_INIT_STATUS> floor_init_status { FLOOR_INIT_STATUS::UNINITIALIZED };
unique_ptr<event> floor::evt;
shared_ptr<compute_context> floor::compute_ctx;
floor::RENDERER floor::renderer { floor::RENDERER::DEFAULT };
SDL_Window* floor::window { nullptr };

// OpenGL
SDL_GLContext floor::opengl_ctx { nullptr };
unordered_set<string> floor::gl_extensions;
bool floor::use_gl_context { false };
uint32_t floor::global_vao { 0u };
string floor::gl_vendor { "" };

// Vulkan
shared_ptr<vulkan_compute> floor::vulkan_ctx;
uint3 floor::vulkan_api_version;

// for use with acquire_context/release_context
recursive_mutex floor::ctx_lock;
atomic<uint32_t> floor::ctx_active_locks { 0 };

// path variables
string floor::datapath = "";
string floor::rel_datapath = "";
string floor::callpath = "";
string floor::kernelpath = "";
string floor::abs_bin_path = "";
string floor::config_name = "config.json";

// fps counting
uint32_t floor::fps = 0;
uint32_t floor::fps_counter = 0;
uint32_t floor::fps_time = 0;
float floor::frame_time = 0.0f;
uint32_t floor::frame_time_sum = 0;
uint32_t floor::frame_time_counter = 0;
bool floor::new_fps_count = false;

// window event handlers
event::handler floor::event_handler_fnctr { &floor::event_handler };

// misc
string floor::app_name { "libfloor" };
uint32_t floor::app_version { 1 };
bool floor::console_only = false;
bool floor::cursor_visible = true;
bool floor::x11_forwarding = false;
atomic<bool> floor::reload_kernels_flag { false };

bool floor::init(const init_state& state) {
	// return if already initialized
	FLOOR_INIT_STATUS expected_init { FLOOR_INIT_STATUS::UNINITIALIZED };
	if(!floor_init_status.compare_exchange_strong(expected_init, FLOOR_INIT_STATUS::IN_PROGRESS)) {
		// wait while someone else is initializing libfloor
		while(floor_init_status == FLOOR_INIT_STATUS::IN_PROGRESS) {
			this_thread::sleep_for(5ms);
		}
		return (floor_init_status == FLOOR_INIT_STATUS::SUCCESSFUL);
	}
	// else: we're now IN_PROGRESS
	
	// sanity check
#if defined(__APPLE__)
	if(state.renderer == RENDERER::VULKAN) {
		cerr << "Vulkan is not supported on OS X / iOS" << endl;
		floor_init_status = FLOOR_INIT_STATUS::FAILURE;
		return false;
	}
#endif
#if defined(FLOOR_NO_VULKAN)
	if(state.renderer == RENDERER::VULKAN) {
		cerr << "can't use the Vulkan renderer when libfloor was compiled without Vulkan support" << endl;
		floor_init_status = FLOOR_INIT_STATUS::FAILURE;
		return false;
	}
#endif
#if defined(FLOOR_NO_METAL)
	if(state.renderer == RENDERER::METAL) {
		cerr << "can't use the Metal renderer when libfloor was compiled without Metal support" << endl;
		floor_init_status = FLOOR_INIT_STATUS::FAILURE;
		return false;
	}
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
	char working_dir[16384];
	memset(working_dir, 0, 16384);
	if(getcwd(working_dir, 16383) == nullptr) {
		cerr << "failed to retrieve the current working directory" << endl;
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
	datapath = datapath.substr(0, datapath.rfind("/") + 1) + rel_datapath;
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
	if(strip_pos != string::npos) {
		datapath.erase(strip_pos, 3);
	}
	
	bool add_bin_path = (working_dir == datapath.substr(0, datapath.length() - 1)) ? false : true;
	if(!add_bin_path) datapath = working_dir + string("\\") + (add_bin_path ? datapath : "");
	else {
		if(datapath[datapath.length() - 1] == '/') {
			datapath = datapath.substr(0, datapath.length() - 1);
		}
		datapath += string("\\");
	}
	
#endif
	
#if defined(__APPLE__)
#if !defined(FLOOR_IOS)
	// check if datapath contains a 'MacOS' string (indicates that the binary is called from within an OS X .app or via complete path from the shell)
	if(datapath.find("MacOS") != string::npos) {
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
	
	kernelpath = "kernels/";
	cursor_visible = true;
	x11_forwarding = false;
	
	fps = 0;
	fps_counter = 0;
	fps_time = 0;
	frame_time = 0.0f;
	frame_time_sum = 0;
	frame_time_counter = 0;
	new_fps_count = false;
	
	// init core (atm this only initializes the rng on windows)
	core::init();
	
	// load config
#if !defined(_MSC_VER)
	const auto floor_sys_path = [](const string& str) {
		return "/opt/floor/data/" + str;
	};
#else
	const auto floor_sys_path = [](const string& str) {
		return core::expand_path_with_env("%ProgramW6432%/floor/data/") + str;
	};
#endif
	
	string config_filename = config_name;
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
	json::json_array opencl_toolchain_paths, cuda_toolchain_paths, metal_toolchain_paths, vulkan_toolchain_paths;
	if(config_doc.valid) {
		config.width = config_doc.get<uint32_t>("screen.width", 1280);
		config.height = config_doc.get<uint32_t>("screen.height", 720);
		config.position.x = config_doc.get<int32_t>("screen.position.x", SDL_WINDOWPOS_UNDEFINED);
		config.position.y = config_doc.get<int32_t>("screen.position.y", SDL_WINDOWPOS_UNDEFINED);
		config.fullscreen = config_doc.get<bool>("screen.fullscreen", false);
		config.vsync = config_doc.get<bool>("screen.vsync", false);
		config.stereo = config_doc.get<bool>("screen.stereo", false);
		config.dpi = config_doc.get<uint32_t>("screen.dpi", 0);
		config.hidpi = config_doc.get<bool>("screen.hidpi", false);
		
		config.audio_disabled = config_doc.get<bool>("audio.disabled", true);
		config.music_volume = const_math::clamp(config_doc.get<float>("audio.music", 1.0f), 0.0f, 1.0f);
		config.sound_volume = const_math::clamp(config_doc.get<float>("audio.sound", 1.0f), 0.0f, 1.0f);
		config.audio_device_name = config_doc.get<string>("audio.device", "");
		
		config.verbosity = config_doc.get<uint32_t>("logging.verbosity", (uint32_t)logger::LOG_TYPE::UNDECORATED);
		config.separate_msg_file = config_doc.get<bool>("logging.separate_msg_file", false);
		config.append_mode = config_doc.get<bool>("logging.append_mode", false);
		config.log_use_time = config_doc.get<bool>("logging.use_time", true);
		config.log_use_color = config_doc.get<bool>("logging.use_color", true);
		config.log_filename = config_doc.get<string>("logging.log_filename", "");
		config.msg_filename = config_doc.get<string>("logging.msg_filename", "");
		
		config.fov = config_doc.get<float>("projection.fov", 72.0f);
		config.near_far_plane.x = config_doc.get<float>("projection.near", 1.0f);
		config.near_far_plane.y = config_doc.get<float>("projection.far", 1000.0f);
		config.upscaling = config_doc.get<float>("projection.upscaling", 1.0f);
		
		config.key_repeat = config_doc.get<uint32_t>("input.key_repeat", 200);
		config.ldouble_click_time = config_doc.get<uint32_t>("input.ldouble_click_time", 200);
		config.mdouble_click_time = config_doc.get<uint32_t>("input.mdouble_click_time", 200);
		config.rdouble_click_time = config_doc.get<uint32_t>("input.rdouble_click_time", 200);
		
		config.backend = config_doc.get<string>("toolchain.backend", "");
		config.gl_sharing = config_doc.get<bool>("toolchain.gl_sharing", false);
		config.debug = config_doc.get<bool>("toolchain.debug", false);
		config.profiling = config_doc.get<bool>("toolchain.profiling", false);
		config.log_binaries = config_doc.get<bool>("toolchain.log_binaries", false);
		config.keep_temp = config_doc.get<bool>("toolchain.keep_temp", false);
		config.keep_binaries = config_doc.get<bool>("toolchain.keep_binaries", true);
		config.use_cache = config_doc.get<bool>("toolchain.use_cache", true);
		config.log_commands = config_doc.get<bool>("toolchain.log_commands", false);
		
		//
		const auto extract_whitelist = [](vector<string>& ret, const string& config_entry_name) {
			unordered_set<string> whitelist;
			const auto whitelist_elems = config_doc.get<json::json_array>(config_entry_name);
			for(const auto& elem : whitelist_elems) {
				if(elem.type != json::json_value::VALUE_TYPE::STRING) {
					log_error("whitelist element must be a string!");
					continue;
				}
				if(elem.str == "") continue;
				whitelist.emplace(core::str_to_lower(elem.str));
			}
			
			for(const auto& elem : whitelist) {
				ret.push_back(elem);
			}
		};
		
		config.default_compiler = config_doc.get<string>("toolchain.generic.compiler", "clang");
		config.default_llc = config_doc.get<string>("toolchain.generic.llc", "llc");
		config.default_as = config_doc.get<string>("toolchain.generic.as", "llvm-as");
		config.default_dis = config_doc.get<string>("toolchain.generic.dis", "llvm-dis");
		
		const auto config_toolchain_paths = config_doc.get<json::json_array>("toolchain.generic.paths");
		if(!config_toolchain_paths.empty()) default_toolchain_paths = config_toolchain_paths;
		
		opencl_toolchain_paths = config_doc.get<json::json_array>("toolchain.opencl.paths", default_toolchain_paths);
		config.opencl_platform = config_doc.get<uint32_t>("toolchain.opencl.platform", 0);
		config.opencl_verify_spir = config_doc.get<bool>("toolchain.opencl.verify_spir", false);
		config.opencl_validate_spirv = config_doc.get<bool>("toolchain.opencl.validate_spirv", false);
		config.opencl_force_spirv_check = config_doc.get<bool>("toolchain.opencl.force_spirv", false);
		config.opencl_disable_spirv = config_doc.get<bool>("toolchain.opencl.disable_spirv", false);
		config.opencl_spirv_param_workaround = config_doc.get<bool>("toolchain.opencl.spirv_param_workaround", false);
		extract_whitelist(config.opencl_whitelist, "toolchain.opencl.whitelist");
		config.opencl_compiler = config_doc.get<string>("toolchain.opencl.compiler", config.default_compiler);
		config.opencl_llc = config_doc.get<string>("toolchain.opencl.llc", config.default_llc);
		config.opencl_as = config_doc.get<string>("toolchain.opencl.as", config.default_as);
		config.opencl_dis = config_doc.get<string>("toolchain.opencl.dis", config.default_dis);
		config.opencl_spirv_encoder = config_doc.get<string>("toolchain.opencl.spirv-encoder", config.opencl_spirv_encoder);
		config.opencl_spirv_as= config_doc.get<string>("toolchain.opencl.spirv-as", config.opencl_spirv_as);
		config.opencl_spirv_dis = config_doc.get<string>("toolchain.opencl.spirv-dis", config.opencl_spirv_dis);
		config.opencl_spirv_validator = config_doc.get<string>("toolchain.opencl.spirv-validator", config.opencl_spirv_validator);
		
		cuda_toolchain_paths = config_doc.get<json::json_array>("toolchain.cuda.paths", default_toolchain_paths);
		config.cuda_force_driver_sm = config_doc.get<string>("toolchain.cuda.force_driver_sm", "");
		config.cuda_force_compile_sm = config_doc.get<string>("toolchain.cuda.force_compile_sm", "");
		config.cuda_force_ptx = config_doc.get<string>("toolchain.cuda.force_ptx", "");
		config.cuda_max_registers = config_doc.get<uint32_t>("toolchain.cuda.max_registers", 32);
		config.cuda_jit_verbose = config_doc.get<bool>("toolchain.cuda.jit_verbose", false);
		config.cuda_jit_opt_level = config_doc.get<uint32_t>("toolchain.cuda.jit_opt_level", 4);
		config.cuda_use_internal_api = config_doc.get<bool>("toolchain.cuda.use_internal_api", true);
		extract_whitelist(config.cuda_whitelist, "toolchain.cuda.whitelist");
		config.cuda_compiler = config_doc.get<string>("toolchain.cuda.compiler", config.default_compiler);
		config.cuda_llc = config_doc.get<string>("toolchain.cuda.llc", config.default_llc);
		config.cuda_as = config_doc.get<string>("toolchain.cuda.as", config.default_as);
		config.cuda_dis = config_doc.get<string>("toolchain.cuda.dis", config.default_dis);
		
		metal_toolchain_paths = config_doc.get<json::json_array>("toolchain.metal.paths", default_toolchain_paths);
		extract_whitelist(config.metal_whitelist, "toolchain.metal.whitelist");
		config.metal_compiler = config_doc.get<string>("toolchain.metal.compiler", config.default_compiler);
		config.metal_llc = config_doc.get<string>("toolchain.metal.llc", config.default_llc);
		config.metal_as = config_doc.get<string>("toolchain.metal.as", config.default_as);
		config.metal_dis = config_doc.get<string>("toolchain.metal.dis", config.default_dis);
		config.metal_force_version = config_doc.get<uint32_t>("toolchain.metal.force_version", 0);
		
		vulkan_toolchain_paths = config_doc.get<json::json_array>("toolchain.vulkan.paths", default_toolchain_paths);
		config.vulkan_validate_spirv = config_doc.get<bool>("toolchain.vulkan.validate_spirv", false);
		extract_whitelist(config.vulkan_whitelist, "toolchain.vulkan.whitelist");
		config.vulkan_compiler = config_doc.get<string>("toolchain.vulkan.compiler", config.default_compiler);
		config.vulkan_llc = config_doc.get<string>("toolchain.vulkan.llc", config.default_llc);
		config.vulkan_as = config_doc.get<string>("toolchain.vulkan.as", config.default_as);
		config.vulkan_dis = config_doc.get<string>("toolchain.vulkan.dis", config.default_dis);
		config.vulkan_spirv_encoder = config_doc.get<string>("toolchain.vulkan.spirv-encoder", config.vulkan_spirv_encoder);
		config.vulkan_spirv_as= config_doc.get<string>("toolchain.vulkan.spirv-as", config.vulkan_spirv_as);
		config.vulkan_spirv_dis = config_doc.get<string>("toolchain.vulkan.spirv-dis", config.vulkan_spirv_dis);
		config.vulkan_spirv_validator = config_doc.get<string>("toolchain.vulkan.spirv-validator", config.vulkan_spirv_validator);
		
		config.execution_model = config_doc.get<string>("toolchain.host.exec_model", "mt-group");
	}
	
	// handle toolchain paths
	if(opencl_toolchain_paths.empty()) opencl_toolchain_paths = default_toolchain_paths;
	if(cuda_toolchain_paths.empty()) cuda_toolchain_paths = default_toolchain_paths;
	if(metal_toolchain_paths.empty()) metal_toolchain_paths = default_toolchain_paths;
	if(vulkan_toolchain_paths.empty()) vulkan_toolchain_paths = default_toolchain_paths;
	
	const auto get_viable_toolchain_path = [](const json::json_array& paths,
											  uint32_t& toolchain_version,
											  string& compiler,
											  string& llc,
											  string& as,
											  string& dis,
											  // <min/max required toolchain version, bin name>
											  vector<pair<uint2, string*>> additional_bins = {}) {
#if defined(__WINDOWS__)
		// on windows: always add .exe to all binaries + expand paths (handles "%Something%/path/to/sth")
		compiler = core::expand_path_with_env(compiler + ".exe");
		llc = core::expand_path_with_env(llc + ".exe");
		as = core::expand_path_with_env(as + ".exe");
		dis = core::expand_path_with_env(dis + ".exe");
		for(auto& bin : additional_bins) {
			*bin.second = core::expand_path_with_env(*bin.second + ".exe");
		}
#endif
		
		for(const auto& path : paths) {
			if(path.type != json::json_value::VALUE_TYPE::STRING) {
				log_error("toolchain path must be a string!");
				continue;
			}
			
			const auto path_str = core::expand_path_with_env(path.str);
			
			if(!file_io::is_file(path_str + "/bin/" + compiler)) continue;
			if(!file_io::is_file(path_str + "/bin/" + llc)) continue;
			if(!file_io::is_file(path_str + "/bin/" + as)) continue;
			if(!file_io::is_file(path_str + "/bin/" + dis)) continue;
			if(!file_io::is_directory(path_str + "/clang")) continue;
			if(!file_io::is_directory(path_str + "/floor")) continue;
			if(!file_io::is_directory(path_str + "/libcxx")) continue;
			
			// get the toolchain (clang) version
			// NOTE: this also checks if clang is actually callable (-> non-viable if not)
			string version_output = "";
			core::system("\"" + path_str + "/bin/" + compiler + "\" --version", version_output);
			
			// e.g. "clang version 3.5.2 (...)" -> want 3.5.2
			static constexpr const char clang_version_str[] { "clang version " };
			const auto clang_version_pos = version_output.find(clang_version_str);
			if(clang_version_pos == string::npos) continue;
			const auto version_start_pos = clang_version_pos + size(clang_version_str) - 1 /* \0 */;
			const auto next_space_pos = version_output.find(' ', version_start_pos);
			if(next_space_pos == string::npos) continue;
			if(next_space_pos - version_start_pos < 5 /* at least len("3.5.2") */) continue;
			
			const auto major_dot_pos = version_output.find('.', version_start_pos + 1);
			if(major_dot_pos == string::npos || major_dot_pos > next_space_pos) continue;
			
			const auto minor_dot_pos = version_output.find('.', major_dot_pos + 1);
			if(minor_dot_pos == string::npos || minor_dot_pos > next_space_pos) continue;
			
			toolchain_version = 10000u * stou(version_output.substr(version_start_pos, major_dot_pos - version_start_pos));
			toolchain_version += 100u * stou(version_output.substr(major_dot_pos + 1, minor_dot_pos - major_dot_pos));
			toolchain_version += stou(version_output.substr(minor_dot_pos + 1, next_space_pos - minor_dot_pos));
			
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
		// -> opencl toolchain
		config.opencl_base_path = get_viable_toolchain_path(opencl_toolchain_paths,
															config.opencl_toolchain_version,
															config.opencl_compiler, config.opencl_llc,
															config.opencl_as, config.opencl_dis,
															vector<pair<uint2, string*>> {
																{ { 30800u, ~0u }, &config.opencl_spirv_encoder },
																{ { 30800u, ~0u }, &config.opencl_spirv_as },
																{ { 30800u, ~0u }, &config.opencl_spirv_dis },
																{ { 30800u, ~0u }, &config.opencl_spirv_validator },
															});
		if(config.opencl_base_path == "") {
#if !defined(FLOOR_IOS) // not available on iOS anyways
			log_error("opencl toolchain is unavailable - could not find a complete toolchain in any specified toolchain path!");
#endif
		}
		else {
			config.opencl_toolchain_exists = true;
			config.opencl_compiler.insert(0, config.opencl_base_path + "bin/");
			config.opencl_llc.insert(0, config.opencl_base_path + "bin/");
			config.opencl_as.insert(0, config.opencl_base_path + "bin/");
			config.opencl_dis.insert(0, config.opencl_base_path + "bin/");
			config.opencl_spirv_encoder.insert(0, config.opencl_base_path + "bin/");
			config.opencl_spirv_as.insert(0, config.opencl_base_path + "bin/");
			config.opencl_spirv_dis.insert(0, config.opencl_base_path + "bin/");
			config.opencl_spirv_validator.insert(0, config.opencl_base_path + "bin/");
		}
		
		// -> cuda toolchain
		config.cuda_base_path = get_viable_toolchain_path(cuda_toolchain_paths,
														  config.cuda_toolchain_version,
														  config.cuda_compiler, config.cuda_llc,
														  config.cuda_as, config.cuda_dis);
		if(config.cuda_base_path == "") {
#if !defined(FLOOR_IOS) // not available on iOS anyways
			log_error("cuda toolchain is unavailable - could not find a complete toolchain in any specified toolchain path!");
#endif
		}
		else {
			config.cuda_toolchain_exists = true;
			config.cuda_compiler.insert(0, config.cuda_base_path + "bin/");
			config.cuda_llc.insert(0, config.cuda_base_path + "bin/");
			config.cuda_as.insert(0, config.cuda_base_path + "bin/");
			config.cuda_dis.insert(0, config.cuda_base_path + "bin/");
		}
		
		// -> metal toolchain
		config.metal_base_path = get_viable_toolchain_path(metal_toolchain_paths,
														   config.metal_toolchain_version,
														   config.metal_compiler, config.metal_llc,
														   config.metal_as, config.metal_dis);
#if defined(FLOOR_IOS)
		// toolchain doesn't exist on an ios device (usually), so just pretend and don't fail
		if(config.metal_base_path == "") {
			config.metal_base_path = "/opt/floor/toolchain";
		}
#else
		if(config.metal_base_path == "") {
			log_error("metal toolchain is unavailable - could not find a complete toolchain in any specified toolchain path!");
		}
		else
#endif
		{
			config.metal_toolchain_exists = true;
			config.metal_compiler.insert(0, config.metal_base_path + "bin/");
			config.metal_llc.insert(0, config.metal_base_path + "bin/");
			config.metal_as.insert(0, config.metal_base_path + "bin/");
			config.metal_dis.insert(0, config.metal_base_path + "bin/");
		}
		
		// -> vulkan toolchain
		config.vulkan_base_path = get_viable_toolchain_path(vulkan_toolchain_paths,
															config.vulkan_toolchain_version,
															config.vulkan_compiler, config.vulkan_llc,
															config.vulkan_as, config.vulkan_dis,
															vector<pair<uint2, string*>> {
																{ { 30800u, ~0u }, &config.vulkan_spirv_encoder },
																{ { 30800u, ~0u }, &config.vulkan_spirv_as },
																{ { 30800u, ~0u }, &config.vulkan_spirv_dis },
																{ { 30800u, ~0u }, &config.vulkan_spirv_validator },
															});
		if(config.vulkan_base_path == "") {
#if defined(FLOOR_IOS)
			log_error("vulkan toolchain is unavailable - could not find a complete toolchain in any specified toolchain path!");
#endif
		}
		else {
			config.vulkan_toolchain_exists = true;
			config.vulkan_compiler.insert(0, config.vulkan_base_path + "bin/");
			config.vulkan_llc.insert(0, config.vulkan_base_path + "bin/");
			config.vulkan_as.insert(0, config.vulkan_base_path + "bin/");
			config.vulkan_dis.insert(0, config.vulkan_base_path + "bin/");
			config.vulkan_spirv_encoder.insert(0, config.vulkan_base_path + "bin/");
			config.vulkan_spirv_as.insert(0, config.vulkan_base_path + "bin/");
			config.vulkan_spirv_dis.insert(0, config.vulkan_base_path + "bin/");
			config.vulkan_spirv_validator.insert(0, config.vulkan_base_path + "bin/");
		}
	}
	
	// init logger and print out floor info
	logger::init((uint32_t)config.verbosity, config.separate_msg_file, config.append_mode,
				 config.log_use_time, config.log_use_color,
				 config.log_filename, config.msg_filename);
	log_debug((FLOOR_VERSION_STRING).c_str());
	
	// choose the renderer
	if(state.renderer == RENDERER::DEFAULT) {
		// try to use Vulkan if the backend is Vulkan and the toolchain exists
#if !defined(__APPLE__)
		if(config.backend == "vulkan") {
			if(!config.vulkan_toolchain_exists) {
				log_error("tried to use the Vulkan renderer, but toolchain doesn't exist - using OpenGL now");
				renderer = RENDERER::OPENGL;
			}
			else {
				renderer = RENDERER::VULKAN;
			}
		}
		else
#endif // always default to OpenGL on OS X / iOS
		{
			renderer = RENDERER::OPENGL;
		}
	}
	else if(state.renderer == RENDERER::OPENGL) {
		renderer = RENDERER::OPENGL;
	}
	else if(state.renderer == RENDERER::VULKAN) {
		// Vulkan was explicitly requested, still need to check if the toolchain exists
#if !defined(__APPLE__)
		if(!config.vulkan_toolchain_exists) {
			log_error("tried to use the Vulkan renderer, but toolchain doesn't exist - using OpenGL now");
			renderer = RENDERER::OPENGL;
		}
		else {
			renderer = RENDERER::VULKAN;
		}
#else
		// obviously can't use Vulkan on OS X / iOS
		log_error("Vulkan renderer is not available on OS X / iOS - using OpenGL now");
		renderer = RENDERER::OPENGL;
#endif
	}
	else if(state.renderer == RENDERER::METAL) {
		// Metal was explicitly requested, still need to check if the toolchain exists
#if !defined(FLOOR_NO_METAL)
		if(!config.metal_toolchain_exists) {
			log_error("tried to use the Metal renderer, but toolchain doesn't exist - using OpenGL now");
			renderer = RENDERER::OPENGL;
		}
		else {
			renderer = RENDERER::METAL;
		}
#else
		// obviously can't use Metal
		log_error("Metal renderer is not available - using OpenGL now");
		renderer = RENDERER::OPENGL;
#endif
	}
	else {
		renderer = RENDERER::NONE;
	}
	assert(renderer != RENDERER::DEFAULT && "must have selected a renderer");
	use_gl_context = (renderer == RENDERER::OPENGL);
	
	//
	evt = make_unique<event>();
	evt->add_internal_event_handler(event_handler_fnctr, EVENT_TYPE::WINDOW_RESIZE, EVENT_TYPE::KERNEL_RELOAD);
	
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
	
	if(!console_only) {
		use_gl_context = false;
		acquire_context();
	}
	
#if !defined(FLOOR_NO_OPENAL)
	if(!config.audio_disabled) {
		audio_controller::destroy();
	}
#endif
	
	vulkan_ctx = nullptr;
	compute_ctx = nullptr;
	
	// delete this at the end, b/c other classes will remove event handlers
	if(evt != nullptr) {
		evt->remove_event_handler(event_handler_fnctr);
		evt = nullptr;
	}
	
	if(!console_only) {
		release_context();
		
		if(opengl_ctx != nullptr) {
			SDL_GL_DeleteContext(opengl_ctx);
			opengl_ctx = nullptr;
		}
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

	// initialize sdl
	if(SDL_Init(console_only ? 0 : SDL_INIT_VIDEO) == -1) {
		log_error("failed to initialize SDL: %s", SDL_GetError());
		return false;
	}
	else {
		log_debug("SDL initialized");
	}
	atexit(SDL_Quit);

	// only initialize opengl/opencl and create a window when not in console-only mode
	if(!console_only) {
		// detect x11 forwarding
#if !defined(__WINDOWS__) && !defined(FLOOR_IOS)
		const char* cur_video_driver = SDL_GetCurrentVideoDriver();
		if(cur_video_driver != nullptr && string(cur_video_driver) == "x11") {
			if(getenv("SSH_CONNECTION") != nullptr) {
				x11_forwarding = true;
				log_debug("detected X11 forwarding");
			}
		}
#endif
		
		// set window creation flags
		config.flags = state.window_flags;
		if(renderer == RENDERER::OPENGL) {
			config.flags |= SDL_WINDOW_OPENGL;
		}
		
		config.flags |= SDL_WINDOW_ALLOW_HIGHDPI; // allow by default, disable later if necessary
#if !defined(FLOOR_IOS)
		auto window_pos = state.window_position;
		if(config.position.x != SDL_WINDOWPOS_UNDEFINED) {
			window_pos.x = config.position.x;
		}
		if(config.position.y != SDL_WINDOWPOS_UNDEFINED) {
			window_pos.y = config.position.y;
		}
		
		if(config.fullscreen) {
			config.flags |= SDL_WINDOW_FULLSCREEN;
			config.flags |= SDL_WINDOW_BORDERLESS;
			window_pos = 0;
			log_debug("fullscreen enabled");
		}
		else {
			log_debug("fullscreen disabled");
		}
#else
		// always set shown
		// NOTE: init_state::window_flags defaults to fullscreen/borderless/resizable on iOS,
		//       but let the user decide if those should be used
		config.flags |= SDL_WINDOW_SHOWN;
#endif
		
		log_debug("vsync %s", config.vsync ? "enabled" : "disabled");
		
		// disable hidpi mode?
		SDL_SetHint("SDL_VIDEO_HIGHDPI_DISABLED", config.hidpi ? "0" : "1");
		log_debug("hidpi %s", config.hidpi ? "enabled" : "disabled");
		
		if(renderer == RENDERER::OPENGL) {
			// gl attributes
			SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
			SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
			SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
			SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 8);
			SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
		}
		
#if !defined(FLOOR_IOS)
		// if x11 forwarding, don't set/request any specific opengl version,
		// otherwise try to use opengl 3.3+ (core) or 2.0
		if(!x11_forwarding && renderer == RENDERER::OPENGL) {
			if(state.use_opengl_33) {
				SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
				SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
#if defined(__APPLE__) // must request a core context on os x, doesn't matter on other platforms
				SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
#endif
			}
			else {
				SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
				SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
				SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_COMPATIBILITY);
			}
		}
#else
		if(renderer == RENDERER::OPENGL) {
			SDL_SetHint(SDL_HINT_RENDER_DRIVER, "opengles3");
			SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
			SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
			SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
		}
		
		//
		SDL_DisplayMode fullscreen_mode;
		SDL_zero(fullscreen_mode);
		fullscreen_mode.format = SDL_PIXELFORMAT_RGBA8888;
		fullscreen_mode.w = (int)config.width;
		fullscreen_mode.h = (int)config.height;
#endif
		
#if 0 // for debugging purposes: dump all display modes
		const auto disp_num = SDL_GetNumDisplayModes(0);
		log_debug("#disp modes: %u", disp_num);
		for(int32_t i = 0; i < disp_num; ++i) {
			SDL_DisplayMode mode;
			SDL_GetDisplayMode(0, i, &mode);
			log_debug("disp mode #%i: %i x %i, %i Hz, format %X", i, mode.w, mode.h, mode.refresh_rate, mode.format);
		}
#endif
		
		// create screen
#if !defined(FLOOR_IOS)
		window = SDL_CreateWindow(app_name.c_str(), window_pos.x, window_pos.y, (int)config.width, (int)config.height, config.flags);
#else
		window = SDL_CreateWindow(app_name.c_str(), 0, 0, (int)config.width, (int)config.height, config.flags);
#endif
		if(window == nullptr) {
			log_error("can't create window: %s", SDL_GetError());
			return false;
		}
		else {
#if defined(FLOOR_IOS)
			// on iOS, be more insistent on the window size
			SDL_SetWindowSize(window, (int)config.width, (int)config.height);
#endif
			
			SDL_GetWindowSize(window, (int*)&config.width, (int*)&config.height);
			log_debug("video mode set: w%u h%u", config.width, config.height);
		}
		
#if defined(FLOOR_IOS)
		if(SDL_SetWindowDisplayMode(window, &fullscreen_mode) < 0) {
			log_error("can't set up fullscreen display mode: %s", SDL_GetError());
			return false;
		}
		SDL_GetWindowSize(window, (int*)&config.width, (int*)&config.height);
		log_debug("fullscreen mode set: w%u h%u", config.width, config.height);
		SDL_ShowWindow(window);
#endif
		
		if(renderer == RENDERER::OPENGL) {
			opengl_ctx = SDL_GL_CreateContext(window);
			if(opengl_ctx == nullptr) {
				log_error("can't create OpenGL context: %s", SDL_GetError());
				return false;
			}
			
#if !defined(FLOOR_IOS)
			// has to be set after context creation
			if(SDL_GL_SetSwapInterval(config.vsync ? 1 : 0) == -1) {
				log_error("error setting the OpenGL swap interval to %v (vsync): %s", config.vsync, SDL_GetError());
				SDL_ClearError();
			}
			
			// enable multi-threaded opengl context when on os x
			// TODO: did this ever actually work?
#if defined(__APPLE__) && 0
			CGLContextObj cgl_ctx = CGLGetCurrentContext();
			CGLError cgl_err = CGLEnable(cgl_ctx, kCGLCEMPEngine);
			if(cgl_err != kCGLNoError) {
				log_error("unable to set multi-threaded opengl context (%X: %X): %s!",
						  (size_t)cgl_ctx, cgl_err, CGLErrorString(cgl_err));
			}
			else {
				log_debug("multi-threaded opengl context enabled!");
			}
#endif
#endif
		}
#if !defined(FLOOR_NO_VULKAN)
		else if(renderer == RENDERER::VULKAN) {
			// create the vulkan context
			vulkan_ctx = make_shared<vulkan_compute>(true, config.vulkan_whitelist);
			if(vulkan_ctx == nullptr ||
			   !vulkan_ctx->is_supported()) {
				log_error("failed to create the Vulkan renderer context");
				renderer = RENDERER::NONE;
				vulkan_ctx = nullptr;
			}
		}
#endif
	}
	acquire_context();
	
	if(!console_only) {
		log_debug("window %screated and acquired!",
				  renderer == RENDERER::OPENGL ? "and OpenGL context " :
				  renderer == RENDERER::VULKAN ? "and Vulkan context " : "");
		
		if(SDL_GetCurrentVideoDriver() == nullptr) {
			log_error("couldn't get video driver: %s!", SDL_GetError());
		}
		else log_debug("video driver: %s", SDL_GetCurrentVideoDriver());
		
		if(renderer == RENDERER::OPENGL) {
			// initialize opengl functions (get function pointers) on non-apple platforms
#if !defined(__APPLE__)
			init_gl_funcs();
#endif
			
			if(is_gl_version(3, 0)) {
				// create and bind vao
				glGenVertexArrays(1, &global_vao);
				glBindVertexArray(global_vao);
			}
			
			// get supported opengl extensions
#if !defined(__APPLE__)
			if(glGetStringi != nullptr)
#endif
			{
				int ext_count = 0;
				glGetIntegerv(GL_NUM_EXTENSIONS, &ext_count);
				for(int i = 0; i < ext_count; ++i) {
					gl_extensions.emplace((const char*)glGetStringi(GL_EXTENSIONS, (GLuint)i));
				}
			}
			
			// make sure GL_ARB_copy_image is explicitly set when gl version is >= 4.3
			const char* gl_version = (const char*)glGetString(GL_VERSION);
			if(gl_version != nullptr) {
				if(gl_version[0] > '4' || (gl_version[0] == '4' && gl_version[2] >= '3')) {
					gl_extensions.emplace("GL_ARB_copy_image");
				}
			}
			
			// on iOS/GLES we need a simple "blit shader" to draw the opencl framebuffer
#if defined(FLOOR_IOS)
			darwin_helper::compile_shaders();
			log_debug("iOS blit shader compiled");
#endif
			
			// make an early clear
			glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
			swap();
			evt->handle_events(); // this will effectively create/open the window on some platforms
			
			//
			int tmp = 0;
			SDL_GL_GetAttribute(SDL_GL_DOUBLEBUFFER, &tmp);
			log_debug("double buffering %s", tmp == 1 ? "enabled" : "disabled");
			
			// print out some opengl informations
			gl_vendor = (const char*)glGetString(GL_VENDOR);
			log_debug("vendor: %s", gl_vendor);
			log_debug("renderer: %s", glGetString(GL_RENDERER));
			log_debug("version: %s", glGetString(GL_VERSION));
			
			// initialize ogl
			init_gl();
			log_debug("OpenGL initialized");
			
			// resize stuff
			resize_gl_window();
		}
		// NOTE: vulkan has already been initialized at this point
		
		evt->set_ldouble_click_time(config.ldouble_click_time);
		evt->set_rdouble_click_time(config.rdouble_click_time);
		evt->set_mdouble_click_time(config.mdouble_click_time);
		
		// retrieve dpi info
		if(config.dpi == 0) {
#if defined(__APPLE__)
			config.dpi = darwin_helper::get_dpi(window);
#else
			SDL_SysWMinfo wm_info;
			SDL_VERSION(&wm_info.version)
			if(SDL_GetWindowWMInfo(window, &wm_info) == 1) {
#if defined(__WINDOWS__)
				HDC hdc = wm_info.info.win.hdc;
				const size2 display_res((size_t)GetDeviceCaps(hdc, HORZRES), (size_t)GetDeviceCaps(hdc, VERTRES));
				const float2 display_phys_size(GetDeviceCaps(hdc, HORZSIZE), GetDeviceCaps(hdc, VERTSIZE));
#else // x11
				Display* display = wm_info.info.x11.display;
				const size2 display_res((size_t)DisplayWidth(display, 0), (size_t)DisplayHeight(display, 0));
				const float2 display_phys_size(DisplayWidthMM(display, 0), DisplayHeightMM(display, 0));
#endif
				const float2 display_dpi((float(display_res.x) / display_phys_size.x) * 25.4f,
										 (float(display_res.y) / display_phys_size.y) * 25.4f);
				config.dpi = (uint32_t)floorf(std::max(display_dpi.x, display_dpi.y));
			}
#endif
		}
		
		// set dpi lower bound to 72
		if(config.dpi < 72) config.dpi = 72;
		log_debug("dpi: %u", config.dpi);
	}
	
	// always create and init compute context (even in console-only mode)
	{
		// get the backend that was set in the config
		COMPUTE_TYPE config_compute_type = COMPUTE_TYPE::NONE;
		if(config.backend == "opencl") config_compute_type = COMPUTE_TYPE::OPENCL;
		else if(config.backend == "cuda") config_compute_type = COMPUTE_TYPE::CUDA;
		else if(config.backend == "metal") config_compute_type = COMPUTE_TYPE::METAL;
		else if(config.backend == "host") config_compute_type = COMPUTE_TYPE::HOST;
		else if(config.backend == "vulkan") config_compute_type = COMPUTE_TYPE::VULKAN;
		
		// default compute backends (will try these in order, using the first working one)
#if defined(__APPLE__)
#if !defined(FLOOR_IOS) // osx
		vector<COMPUTE_TYPE> compute_defaults { COMPUTE_TYPE::METAL, COMPUTE_TYPE::CUDA };
#else // ios
		vector<COMPUTE_TYPE> compute_defaults { COMPUTE_TYPE::METAL };
#endif
#else // linux, windows, ...
		vector<COMPUTE_TYPE> compute_defaults { COMPUTE_TYPE::OPENCL, COMPUTE_TYPE::CUDA, COMPUTE_TYPE::VULKAN };
#endif
		// always start with the configured one (if one has been set)
		if(config_compute_type != COMPUTE_TYPE::NONE) {
			// erase existing entry first, so that we don't try to init it twice
			core::erase_if(compute_defaults, [&config_compute_type](const auto& iter) { return (*iter == config_compute_type); });
			compute_defaults.insert(compute_defaults.begin(), config_compute_type);
		}
		// always end with host compute (as a fallback), if it isn't already part of the list
		if(find(compute_defaults.begin(), compute_defaults.end(), COMPUTE_TYPE::HOST) == compute_defaults.end()) {
			compute_defaults.emplace_back(COMPUTE_TYPE::HOST);
		}
		
		// iterate over all backends in the default set, using the first one that works
		compute_ctx = nullptr; // just to be sure
		for(const auto& backend : compute_defaults) {
			switch(backend) {
				case COMPUTE_TYPE::CUDA:
#if !defined(FLOOR_NO_CUDA)
					if(!config.cuda_toolchain_exists) break;
					log_debug("initializing CUDA ...");
					compute_ctx = make_shared<cuda_compute>(config.cuda_whitelist);
#endif
					break;
				case COMPUTE_TYPE::OPENCL:
#if !defined(FLOOR_NO_OPENCL)
					if(!config.opencl_toolchain_exists) break;
					log_debug("initializing OpenCL ...");
					compute_ctx = make_shared<opencl_compute>(config.opencl_platform,
															  config.gl_sharing & !console_only,
															  config.opencl_whitelist);
#endif
					break;
				case COMPUTE_TYPE::METAL:
#if !defined(FLOOR_NO_METAL)
					if(!config.metal_toolchain_exists) break;
					log_debug("initializing Metal ...");
					compute_ctx = make_shared<metal_compute>(config.metal_whitelist);
#endif
					break;
				case COMPUTE_TYPE::HOST:
#if !defined(FLOOR_NO_HOST_COMPUTE)
					log_debug("initializing Host Compute ...");
					compute_ctx = make_shared<host_compute>();
#endif
					break;
				case COMPUTE_TYPE::VULKAN:
#if !defined(FLOOR_NO_VULKAN)
					if(vulkan_ctx != nullptr) {
						compute_ctx = vulkan_ctx;
					}
					else {
						log_debug("initializing Vulkan ...");
						compute_ctx = make_shared<vulkan_compute>(false, config.vulkan_whitelist);
					}
#endif
					break;
				default: break;
			}
			
			if(compute_ctx != nullptr) {
				if(!compute_ctx->is_supported()) {
					log_error("failed to create a \"%s\" context, trying next backend ...", compute_type_to_string(backend));
					compute_ctx = nullptr;
				}
				else {
					break; // success
				}
			}
		}
		
		// this is rather bad
		if(compute_ctx == nullptr) {
			log_error("failed to create any compute context!");
		}
	}
	
	// also always init openal/audio
#if !defined(FLOOR_NO_OPENAL)
	if(!config.audio_disabled) {
		// check if openal functions have been correctly initialized and initialize openal
		floor_audio::check_openal_efx_funcs();
		audio_controller::init();
	}
#endif
	
	release_context();
	
	return true;
}

void floor::set_screen_size(const uint2& screen_size) {
	if(screen_size.x == config.width && screen_size.y == config.height) return;
	config.width = screen_size.x;
	config.height = screen_size.y;
	SDL_SetWindowSize(window, (int)config.width, (int)config.height);
	
	SDL_Rect bounds;
	SDL_GetDisplayBounds(0, &bounds);
	SDL_SetWindowPosition(window,
						  bounds.x + (bounds.w - int(config.width)) / 2,
						  bounds.y + (bounds.h - int(config.height)) / 2);
}

void floor::set_fullscreen(const bool& state) {
	if(state == config.fullscreen) return;
	config.fullscreen = state;
	if(SDL_SetWindowFullscreen(window, (SDL_bool)state) != 0) {
		log_error("failed to %s fullscreen: %s!",
				  (state ? "enable" : "disable"), SDL_GetError());
	}
	evt->add_event(EVENT_TYPE::WINDOW_RESIZE,
				   make_shared<window_resize_event>(SDL_GetTicks(),
													uint2(config.width, config.height)));
	// TODO: border?
}

void floor::set_vsync(const bool& state) {
	if(state == config.vsync) return;
	config.vsync = state;
#if !defined(FLOOR_IOS)
	if(opengl_ctx != nullptr) {
		SDL_GL_SetSwapInterval(config.vsync ? 1 : 0);
	}
#endif
}

void floor::start_frame() {
	acquire_context();
}

void floor::end_frame(const bool window_swap) {
	if(renderer == RENDERER::OPENGL) {
		GLenum error = glGetError();
		switch(error) {
			case GL_NO_ERROR:
				break;
			case GL_INVALID_ENUM:
				log_error("OpenGL error: invalid enum!");
				break;
			case GL_INVALID_VALUE:
				log_error("OpenGL error: invalid value!");
				break;
			case GL_INVALID_OPERATION:
				log_error("OpenGL error: invalid operation!");
				break;
			case GL_OUT_OF_MEMORY:
				log_error("OpenGL error: out of memory!");
				break;
			case GL_INVALID_FRAMEBUFFER_OPERATION:
				log_error("OpenGL error: invalid framebuffer operation!");
				break;
			default:
				log_error("unknown OpenGL error: %u!", error);
				break;
		}
	}
	
	// optional window swap (client code might want to swap the window by itself)
	if(window_swap) swap();
	
	frame_time_sum += SDL_GetTicks() - frame_time_counter;

	// handle fps count
	fps_counter++;
	if(SDL_GetTicks() - fps_time > 1000) {
		fps = fps_counter;
		new_fps_count = true;
		fps_counter = 0;
		fps_time = SDL_GetTicks();
		
		frame_time = (float)frame_time_sum / (float)fps;
		frame_time_sum = 0;
	}
	frame_time_counter = SDL_GetTicks();
	
	// check for kernel reload (this is safe to do here)
	if(reload_kernels_flag) {
		reload_kernels_flag = false;
		if(compute_ctx != nullptr) {
			//compute_ctx->reload_kernels(); // TODO: !
		}
	}
	
	release_context();
}

/*! sets the window caption
 *  @param caption the window caption
 */
void floor::set_caption(const string& caption) {
	SDL_SetWindowTitle(window, caption.c_str());
}

/*! returns the window caption
 */
string floor::get_caption() {
	return SDL_GetWindowTitle(window);
}

/*! opengl initialization function
 */
void floor::init_gl() {
	glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
	
	glClearDepth(1.0);
	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LESS);
	
	glDisable(GL_STENCIL_TEST);
	
	glFrontFace(GL_CW);
	glCullFace(GL_BACK);
	glEnable(GL_CULL_FACE);
}

/* function to reset our viewport after a window resize
 */
void floor::resize_gl_window() {
	if(renderer == RENDERER::OPENGL) {
		// set the viewport
		glViewport(0, 0, (GLsizei)config.width, (GLsizei)config.height);
	}
}

/*! sets the cursors visibility to state
 *  @param state the cursor visibility state
 */
void floor::set_cursor_visible(const bool& state) {
	floor::cursor_visible = state;
	SDL_ShowCursor(floor::cursor_visible);
}

/*! returns the cursor visibility stateo
 */
bool floor::get_cursor_visible() {
	return floor::cursor_visible;
}

/*! returns a pointer to the event class
 */
event* floor::get_event() {
	return floor::evt.get();
}

/*! sets the data path
 *  @param data_path the data path
 */
void floor::set_data_path(const char* data_path) {
	floor::datapath = data_path;
}

/*! returns the data path
 */
string floor::get_data_path() {
	return datapath;
}

/*! returns the call path
 */
string floor::get_call_path() {
	return callpath;
}

/*! returns the kernel path
 */
string floor::get_kernel_path() {
	return kernelpath;
}

/*! returns data path + str
 *  @param str str we want to "add" to the data path
 */
string floor::data_path(const string& str) {
	if(str.length() == 0) return datapath;
	return datapath + str;
}

/*! returns data path + kernel path + str
 *  @param str str we want to "add" to the data + kernel path
 */
string floor::kernel_path(const string& str) {
	if(str.length() == 0) return datapath + kernelpath;
	return datapath + kernelpath + str;
}

/*! strips the data path from a string
 *  @param str str we want strip the data path from
 */
string floor::strip_data_path(const string& str) {
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

bool floor::get_stereo() {
	return config.stereo;
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
	uint32_t ret = config.width;
#if defined(__APPLE__) // only supported on osx and ios right now
	ret = uint32_t(double(ret) *
				   (config.hidpi ?
					double(darwin_helper::get_scale_factor(window)) : 1.0));
#endif
	return ret;
}

uint32_t floor::get_physical_height() {
	uint32_t ret = config.height;
#if defined(__APPLE__) // only supported on osx and ios right now
	ret = uint32_t(double(ret) *
				   (config.hidpi ?
					double(darwin_helper::get_scale_factor(window)) : 1.0));
#endif
	return ret;
}

uint2 floor::get_physical_screen_size() {
	uint2 ret { config.width, config.height };
#if defined(__APPLE__) // only supported on osx and ios right now
	ret = uint2(double2(ret) *
				(config.hidpi ?
				 double(darwin_helper::get_scale_factor(window)) : 1.0));
#endif
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

SDL_Window* floor::get_window() {
	return window;
}

uint32_t floor::get_window_flags() {
	return config.flags;
}

uint32_t floor::get_window_refresh_rate() {
	// SDL_GetWindowDisplayMode is useless/broken, so get the display index + retrieve the mode that way instead
	const auto display_index = SDL_GetWindowDisplayIndex(window);
	if(display_index < 0) {
		log_error("failed to retrieve window display index");
		return 60;
	}
	else {
		SDL_DisplayMode mode;
		if(SDL_GetCurrentDisplayMode(display_index, &mode) < 0) {
			log_error("failed to retrieve current display mode (for display #%u)", display_index);
			return 60;
		}
		return (mode.refresh_rate < 0 ? 60 : uint32_t(mode.refresh_rate));
	}
}

SDL_GLContext floor::get_opengl_context() {
	return opengl_ctx;
}

shared_ptr<vulkan_compute> floor::get_vulkan_context() {
	return vulkan_ctx;
}

const string floor::get_version() {
	return FLOOR_VERSION_STRING;
}

void floor::swap() {
	if(opengl_ctx != nullptr) {
		SDL_GL_SwapWindow(window);
	}
}

void floor::reload_kernels() {
	reload_kernels_flag = true;
}

const float& floor::get_fov() {
	return config.fov;
}

void floor::set_fov(const float& fov) {
	if(const_math::is_equal(config.fov, fov)) return;
	config.fov = fov;
	evt->add_event(EVENT_TYPE::WINDOW_RESIZE,
				   make_shared<window_resize_event>(SDL_GetTicks(), uint2(config.width, config.height)));
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

json::document& floor::get_config_doc() {
	return config_doc;
}

void floor::acquire_context() {
	// note: the context lock is recursive, so one thread can lock
	// it multiple times. however, SDL_GL_MakeCurrent should only
	// be called once (this is the purpose of ctx_active_locks).
	ctx_lock.lock();
	// note: not a race, since there can only be one active gl thread
	const uint32_t cur_active_locks = ctx_active_locks++;
	if(use_gl_context && opengl_ctx != nullptr) {
		if(cur_active_locks == 0) {
			if(SDL_GL_MakeCurrent(window, opengl_ctx) != 0) {
				log_error("couldn't make gl context current: %s!", SDL_GetError());
				return;
			}
		}
#if defined(FLOOR_IOS)
		glBindFramebuffer(GL_FRAMEBUFFER, FLOOR_DEFAULT_FRAMEBUFFER);
#endif
	}
}

void floor::release_context() {
	// only call SDL_GL_MakeCurrent with nullptr, when this is the last lock
	const uint32_t cur_active_locks = --ctx_active_locks;
	if(use_gl_context && opengl_ctx != nullptr && cur_active_locks == 0) {
		if(SDL_GL_MakeCurrent(window, nullptr) != 0) {
			log_error("couldn't release current gl context: %s!", SDL_GetError());
			return;
		}
	}
	ctx_lock.unlock();
}
void floor::set_use_gl_context(const bool& state) {
	use_gl_context = state;
}

const bool& floor::get_use_gl_context() {
	return use_gl_context;
}

bool floor::event_handler(EVENT_TYPE type, shared_ptr<event_object> obj) {
	if(type == EVENT_TYPE::WINDOW_RESIZE) {
		const window_resize_event& wnd_evt = (const window_resize_event&)*obj;
		config.width = wnd_evt.size.x;
		config.height = wnd_evt.size.y;
		resize_gl_window();
		return true;
	}
	else if(type == EVENT_TYPE::KERNEL_RELOAD) {
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
#if defined(__APPLE__)
	return darwin_helper::get_scale_factor(window);
#else
	return config.upscaling; // TODO: get this from somewhere ...
#endif
}

string floor::get_absolute_path() {
	return abs_bin_path;
}

void floor::set_audio_disabled(const bool& state) {
	config.audio_disabled = state;
}

bool floor::is_audio_disabled() {
	return config.audio_disabled;
}

void floor::set_music_volume(const float& volume) {
	if(config.audio_disabled) return;
	config.music_volume = volume;
#if !defined(FLOOR_NO_OPENAL)
	audio_controller::update_music_volumes();
#endif
}

const float& floor::get_music_volume() {
	return config.music_volume;
}

void floor::set_sound_volume(const float& volume) {
	if(config.audio_disabled) return;
	config.sound_volume = volume;
#if !defined(FLOOR_NO_OPENAL)
	audio_controller::update_sound_volumes();
#endif
}

const float& floor::get_sound_volume() {
	return config.sound_volume;
}

const string& floor::get_audio_device_name() {
	return config.audio_device_name;
}

const string& floor::get_toolchain_backend() {
	return config.backend;
}
bool floor::get_toolchain_gl_sharing() {
	return config.gl_sharing;
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

const string& floor::get_toolchain_default_compiler() {
	return config.default_compiler;
}
const string& floor::get_toolchain_default_llc() {
	return config.default_llc;
}
const string& floor::get_toolchain_default_as() {
	return config.default_as;
}
const string& floor::get_toolchain_default_dis() {
	return config.default_dis;
}

const string& floor::get_opencl_base_path() {
	return config.opencl_base_path;
}
const uint32_t& floor::get_opencl_toolchain_version() {
	return config.opencl_toolchain_version;
}
const vector<string>& floor::get_opencl_whitelist() {
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
const string& floor::get_opencl_compiler() {
	return config.opencl_compiler;
}
const string& floor::get_opencl_llc() {
	return config.opencl_llc;
}
const string& floor::get_opencl_as() {
	return config.opencl_as;
}
const string& floor::get_opencl_dis() {
	return config.opencl_dis;
}
const string& floor::get_opencl_spirv_encoder() {
	return config.opencl_spirv_encoder;
}
const string& floor::get_opencl_spirv_as() {
	return config.opencl_spirv_as;
}
const string& floor::get_opencl_spirv_dis() {
	return config.opencl_spirv_dis;
}
const string& floor::get_opencl_spirv_validator() {
	return config.opencl_spirv_validator;
}

const string& floor::get_cuda_base_path() {
	return config.cuda_base_path;
}
const uint32_t& floor::get_cuda_toolchain_version() {
	return config.cuda_toolchain_version;
}
const vector<string>& floor::get_cuda_whitelist() {
	return config.cuda_whitelist;
}
const string& floor::get_cuda_compiler() {
	return config.cuda_compiler;
}
const string& floor::get_cuda_llc() {
	return config.cuda_llc;
}
const string& floor::get_cuda_as() {
	return config.cuda_as;
}
const string& floor::get_cuda_dis() {
	return config.cuda_dis;
}
const string& floor::get_cuda_force_driver_sm() {
	return config.cuda_force_driver_sm;
}
const string& floor::get_cuda_force_compile_sm() {
	return config.cuda_force_compile_sm;
}
const string& floor::get_cuda_force_ptx() {
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

const string& floor::get_metal_base_path() {
	return config.metal_base_path;
}
const uint32_t& floor::get_metal_toolchain_version() {
	return config.metal_toolchain_version;
}
const vector<string>& floor::get_metal_whitelist() {
	return config.metal_whitelist;
}
const string& floor::get_metal_compiler() {
	return config.metal_compiler;
}
const string& floor::get_metal_llc() {
	return config.metal_llc;
}
const string& floor::get_metal_as() {
	return config.metal_as;
}
const string& floor::get_metal_dis() {
	return config.metal_dis;
}
const uint32_t& floor::get_metal_force_version() {
	return config.metal_force_version;
}

const string& floor::get_vulkan_base_path() {
	return config.vulkan_base_path;
}
const uint32_t& floor::get_vulkan_toolchain_version() {
	return config.vulkan_toolchain_version;
}
const vector<string>& floor::get_vulkan_whitelist() {
	return config.vulkan_whitelist;
}
bool floor::get_vulkan_validate_spirv() {
	return config.vulkan_validate_spirv;
}
const string& floor::get_vulkan_compiler() {
	return config.vulkan_compiler;
}
const string& floor::get_vulkan_llc() {
	return config.vulkan_llc;
}
const string& floor::get_vulkan_as() {
	return config.vulkan_as;
}
const string& floor::get_vulkan_dis() {
	return config.vulkan_dis;
}
const string& floor::get_vulkan_spirv_encoder() {
	return config.vulkan_spirv_encoder;
}
const string& floor::get_vulkan_spirv_as() {
	return config.vulkan_spirv_as;
}
const string& floor::get_vulkan_spirv_dis() {
	return config.vulkan_spirv_dis;
}
const string& floor::get_vulkan_spirv_validator() {
	return config.vulkan_spirv_validator;
}


const string& floor::get_execution_model() {
	return config.execution_model;
}

shared_ptr<compute_context> floor::get_compute_context() {
	return compute_ctx;
}

bool floor::has_opengl_extension(const char* name) {
	return (gl_extensions.count(name) > 0);
}

bool floor::is_console_only() {
	return console_only;
}

bool floor::is_gl_version(const uint32_t& major, const uint32_t& minor) {
	const char* version = (const char*)glGetString(GL_VERSION);
	if(uint32_t(version[0] - '0') > major) return true;
	else if(uint32_t(version[0] - '0') == major && uint32_t(version[2] - '0') >= minor) return true;
	return false;
}

const string& floor::get_gl_vendor() {
	return gl_vendor;
}

const uint3& floor::get_vulkan_api_version() {
	return vulkan_api_version;
}

const string& floor::get_app_name() {
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
