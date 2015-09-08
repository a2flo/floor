/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2015 Florian Ziesche
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
#include <floor/compute/opencl/opencl_compute.hpp>
#include <floor/compute/cuda/cuda_compute.hpp>
#include <floor/compute/metal/metal_compute.hpp>
#include <floor/compute/host/host_compute.hpp>

#if defined(__APPLE__)
#include <floor/darwin/darwin_helper.hpp>
#endif

// init statics
event* floor::evt = nullptr;
bool floor::console_only = false;
shared_ptr<compute_context> floor::compute_ctx;
unordered_set<string> floor::gl_extensions;

struct floor::floor_config floor::config;
json::document floor::config_doc;

string floor::datapath = "";
string floor::rel_datapath = "";
string floor::callpath = "";
string floor::kernelpath = "";
string floor::abs_bin_path = "";
string floor::config_name = "config.json";

unsigned int floor::fps = 0;
unsigned int floor::fps_counter = 0;
unsigned int floor::fps_time = 0;
float floor::frame_time = 0.0f;
unsigned int floor::frame_time_sum = 0;
unsigned int floor::frame_time_counter = 0;
bool floor::new_fps_count = false;

bool floor::cursor_visible = true;

event::handler floor::event_handler_fnctr { &floor::event_handler };

atomic<bool> floor::reload_kernels_flag { false };
bool floor::use_gl_context { true };
uint32_t floor::global_vao { 0u };
string floor::gl_vendor { "" };

#if defined(__WINDOWS__)
static string expand_path_with_env(const string& in) {
	static thread_local char expanded_path[32768]; // 32k is the max
	const auto expanded_size = ExpandEnvironmentStringsA(in.c_str(), expanded_path, 32767);
	if(expanded_size == 0 || expanded_size == 32767) {
		log_error("failed to expand file path: %s", in);
		return in;
	}
	return string(expanded_path, std::min(uint32_t(expanded_size - 1), uint32_t(size(expanded_path) - 1)));
}
#else
#define expand_path_with_env(path) path
#endif

/*! this is used to set an absolute data path depending on call path (path from where the binary is called/started),
 *! which is mostly needed when the binary is opened via finder under os x or any file manager under linux
 */
void floor::init(const char* callpath_, const char* datapath_,
				 const bool console_only_, const string config_name_,
				 const bool use_gl33_, const unsigned int window_flags) {
	//
	register_segfault_handler();
	
	floor::callpath = callpath_;
	floor::datapath = callpath_;
	floor::rel_datapath = datapath_;
	floor::abs_bin_path = callpath_;
	floor::config_name = config_name_;
	floor::console_only = console_only_;
	
	// get working directory
	char working_dir[16384];
	memset(working_dir, 0, 16384);
	getcwd(working_dir, 16383);
	
	// no '/' -> relative path
	if(rel_datapath[0] != '/') {
		datapath = datapath.substr(0, datapath.rfind(FLOOR_OS_DIR_SLASH)+1) + rel_datapath;
	}
	// absolute path
	else datapath = rel_datapath;
	
	// same for abs bin path (no '/' -> relative path)
	if(abs_bin_path[0] != '/') {
		bool direct_rel_path = false;
		if(abs_bin_path.size() > 2 && abs_bin_path[0] == '.' && abs_bin_path[1] == '/') {
			direct_rel_path = true;
		}
		abs_bin_path = FLOOR_OS_DIR_SLASH + abs_bin_path.substr(direct_rel_path ? 2 : 0,
																abs_bin_path.rfind(FLOOR_OS_DIR_SLASH) + 1
																- (direct_rel_path ? 2 : 0));
		abs_bin_path = working_dir + abs_bin_path; // just add the working dir -> done
	}
	// else: we already have the absolute path
	
#if defined(CYGWIN)
	callpath = "./";
	datapath = callpath_;
	datapath = datapath.substr(0, datapath.rfind("/")+1) + rel_datapath;
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
	
	bool add_bin_path = (working_dir == datapath.substr(0, datapath.length()-1)) ? false : true;
	if(!add_bin_path) datapath = working_dir + string("\\") + (add_bin_path ? datapath : "");
	else {
		if(datapath[datapath.length()-1] == '/') {
			datapath = datapath.substr(0, datapath.length()-1);
		}
		datapath += string("\\");
	}
	
#endif
	
#if defined(__APPLE__)
#if !defined(FLOOR_IOS)
	// check if datapath contains a 'MacOS' string (indicates that the binary is called from within an OS X .app or via complete path from the shell)
	if(datapath.find("MacOS") != string::npos) {
		// if so, add "../../../" to the datapath, since we have to relocate the datapath if the binary is inside an .app
		datapath.insert(datapath.find("MacOS")+6, "../../../");
	}
#else
	datapath = datapath_;
	rel_datapath = datapath;
#endif
#endif
	
	// condense datapath and abs_bin_path
	datapath = core::strip_path(datapath);
	abs_bin_path = core::strip_path(abs_bin_path);
	
	kernelpath = "kernels/";
	cursor_visible = true;
	
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
	const string config_filename(config_name +
								 (file_io::is_file(data_path(config_name + ".local")) ? ".local" : ""));
	config_doc = json::create_document(data_path(config_filename));
	if(config_doc.valid) {
		config.width = config_doc.get<uint64_t>("screen.width", 1280);
		config.height = config_doc.get<uint64_t>("screen.height", 720);
		config.fullscreen = config_doc.get<bool>("screen.fullscreen", false);
		config.vsync = config_doc.get<bool>("screen.vsync", false);
		config.stereo = config_doc.get<bool>("screen.stereo", false);
		config.dpi = config_doc.get<uint64_t>("screen.dpi", 0);
		config.hidpi = config_doc.get<bool>("screen.hidpi", false);
		
		config.audio_disabled = config_doc.get<bool>("audio.disabled", true);
		config.music_volume = const_math::clamp(config_doc.get<float>("audio.music", 1.0f), 0.0f, 1.0f);
		config.sound_volume = const_math::clamp(config_doc.get<float>("audio.sound", 1.0f), 0.0f, 1.0f);
		config.audio_device_name = config_doc.get<string>("audio.device", "");
		
		config.verbosity = config_doc.get<uint64_t>("logging.verbosity", (size_t)logger::LOG_TYPE::UNDECORATED);
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
		
		config.key_repeat = config_doc.get<uint64_t>("input.key_repeat", 200);
		config.ldouble_click_time = config_doc.get<uint64_t>("input.ldouble_click_time", 200);
		config.mdouble_click_time = config_doc.get<uint64_t>("input.mdouble_click_time", 200);
		config.rdouble_click_time = config_doc.get<uint64_t>("input.rdouble_click_time", 200);
		
		config.backend = config_doc.get<string>("compute.backend", "");
		config.gl_sharing = config_doc.get<bool>("compute.gl_sharing", false);
		config.debug = config_doc.get<bool>("compute.debug", false);
		config.profiling = config_doc.get<bool>("compute.profiling", false);
		config.log_binaries = config_doc.get<bool>("compute.log_binaries", false);
		config.keep_temp = config_doc.get<bool>("compute.keep_temp", false);
		config.keep_binaries = config_doc.get<bool>("compute.keep_binaries", true);
		config.use_cache = config_doc.get<bool>("compute.use_cache", true);
		config.log_commands = config_doc.get<bool>("compute.log_commands", false);
		
		config.default_compiler = config_doc.get<string>("compute.toolchain.compiler", "clang");
		config.default_llc = config_doc.get<string>("compute.toolchain.llc", "llc");
		config.default_as = config_doc.get<string>("compute.toolchain.as", "llvm-as");
		config.default_dis = config_doc.get<string>("compute.toolchain.dis", "llvm-dis");
		
		//
		static const auto get_viable_toolchain_path = [](const json::json_array& paths,
														 string& compiler,
														 string& llc,
														 string& as,
														 string& dis,
														 vector<string*> additional_bins = {}) {
#if defined(__WINDOWS__)
			// on windows: always add .exe to all binaries + expand paths (handles "%Something%/path/to/sth")
			compiler = expand_path_with_env(compiler + ".exe");
			llc = expand_path_with_env(llc + ".exe");
			as = expand_path_with_env(as + ".exe");
			dis = expand_path_with_env(dis + ".exe");
			for(auto& bin : additional_bins) {
				*bin = expand_path_with_env(*bin + ".exe");
			}
#endif

			for(const auto& path : paths) {
				if(path.type != json::json_value::VALUE_TYPE::STRING) {
					log_error("toolchain path must be a string!");
					continue;
				}

				const auto path_str = expand_path_with_env(path.str);
				
				if(!file_io::is_file(path_str + "/bin/" + compiler)) continue;
				if(!file_io::is_file(path_str + "/bin/" + llc)) continue;
				if(!file_io::is_file(path_str + "/bin/" + as)) continue;
				if(!file_io::is_file(path_str + "/bin/" + dis)) continue;
				if(!file_io::is_directory(path_str + "/clang")) continue;
				if(!file_io::is_directory(path_str + "/floor")) continue;
				if(!file_io::is_directory(path_str + "/libcxx")) continue;
				bool found_additional_bins = true;
				for(const auto& bin : additional_bins) {
					if(!file_io::is_file(path_str + "/bin/" + *bin)) {
						found_additional_bins = false;
						break;
					}
				}
				if(!found_additional_bins) continue;
				return path_str + "/";
			}
			return ""s;
		};
		
		static const auto extract_whitelist = [](unordered_set<string>& whitelist, const string& config_entry_name) {
			const auto whitelist_elems = config_doc.get<json::json_array>(config_entry_name);
			for(const auto& elem : whitelist_elems) {
				if(elem.type != json::json_value::VALUE_TYPE::STRING) {
					log_error("whitelist element must be a string!");
					continue;
				}
				if(elem.str == "") continue;
				whitelist.emplace(core::str_to_lower(elem.str));
			}
		};
		
		const auto default_toolchain_paths = config_doc.get<json::json_array>("compute.toolchain.paths");
		
		const auto opencl_toolchain_paths = config_doc.get<json::json_array>("compute.opencl.paths", default_toolchain_paths);
		config.opencl_platform = config_doc.get<uint64_t>("compute.opencl.platform", 0);
		config.opencl_verify_spir = config_doc.get<bool>("compute.opencl.verify_spir", false);
		extract_whitelist(config.opencl_whitelist, "compute.opencl.whitelist");
		config.opencl_compiler = config_doc.get<string>("compute.opencl.compiler", config.default_compiler);
		config.opencl_llc = config_doc.get<string>("compute.opencl.llc", config.default_llc);
		config.opencl_as = config_doc.get<string>("compute.opencl.as", config.default_as);
		config.opencl_dis = config_doc.get<string>("compute.opencl.dis", config.default_dis);
		config.opencl_spir_encoder = config_doc.get<string>("compute.opencl.spir-encoder", config.opencl_spir_encoder);
		config.opencl_spir_verifier = config_doc.get<string>("compute.opencl.spir-verifier", config.opencl_spir_verifier);
		config.opencl_applecl_encoder = config_doc.get<string>("compute.opencl.applecl-encoder", config.opencl_applecl_encoder);
		config.opencl_base_path = get_viable_toolchain_path(opencl_toolchain_paths,
															config.opencl_compiler, config.opencl_llc,
															config.opencl_as, config.opencl_dis,
															vector<string*> {
																&config.opencl_spir_encoder,
																&config.opencl_spir_verifier,
																&config.opencl_applecl_encoder
															});
		if(config.opencl_base_path == "") {
			log_error("opencl toolchain is unavailable - could not find a complete toolchain in any specified toolchain path!");
		}
		else {
			config.opencl_toolchain_exists = true;
			config.opencl_compiler.insert(0, config.opencl_base_path + "bin/");
			config.opencl_llc.insert(0, config.opencl_base_path + "bin/");
			config.opencl_as.insert(0, config.opencl_base_path + "bin/");
			config.opencl_dis.insert(0, config.opencl_base_path + "bin/");
			config.opencl_spir_encoder.insert(0, config.opencl_base_path + "bin/");
			config.opencl_spir_verifier.insert(0, config.opencl_base_path + "bin/");
			config.opencl_applecl_encoder.insert(0, config.opencl_base_path + "bin/");
		}
		
		const auto cuda_toolchain_paths = config_doc.get<json::json_array>("compute.cuda.paths", default_toolchain_paths);
		config.cuda_force_driver_sm = config_doc.get<string>("compute.cuda.force_driver_sm", "");
		config.cuda_force_compile_sm = config_doc.get<string>("compute.cuda.force_compile_sm", "");
		extract_whitelist(config.cuda_whitelist, "compute.cuda.whitelist");
		config.cuda_compiler = config_doc.get<string>("compute.cuda.compiler", config.default_compiler);
		config.cuda_llc = config_doc.get<string>("compute.cuda.llc", config.default_llc);
		config.cuda_as = config_doc.get<string>("compute.cuda.as", config.default_as);
		config.cuda_dis = config_doc.get<string>("compute.cuda.dis", config.default_dis);
		config.cuda_base_path = get_viable_toolchain_path(cuda_toolchain_paths,
														  config.cuda_compiler, config.cuda_llc,
														  config.cuda_as, config.cuda_dis);
		if(config.cuda_base_path == "") {
			log_error("cuda toolchain is unavailable - could not find a complete toolchain in any specified toolchain path!");
		}
		else {
			config.cuda_toolchain_exists = true;
			config.cuda_compiler.insert(0, config.cuda_base_path + "bin/");
			config.cuda_llc.insert(0, config.cuda_base_path + "bin/");
			config.cuda_as.insert(0, config.cuda_base_path + "bin/");
			config.cuda_dis.insert(0, config.cuda_base_path + "bin/");
		}
		
		const auto metal_toolchain_paths = config_doc.get<json::json_array>("compute.metal.paths", default_toolchain_paths);
		extract_whitelist(config.metal_whitelist, "compute.metal.whitelist");
		config.metal_compiler = config_doc.get<string>("compute.metal.compiler", config.default_compiler);
		config.metal_llc = config_doc.get<string>("compute.metal.llc", config.default_llc);
		config.metal_as = config_doc.get<string>("compute.metal.as", config.default_as);
		config.metal_dis = config_doc.get<string>("compute.metal.dis", config.default_dis);
		config.metal_base_path = get_viable_toolchain_path(metal_toolchain_paths,
														   config.metal_compiler, config.metal_llc,
														   config.metal_as, config.metal_dis);
		if(config.metal_base_path == "") {
			log_error("metal toolchain is unavailable - could not find a complete toolchain in any specified toolchain path!");
		}
		else {
			config.metal_toolchain_exists = true;
			config.metal_compiler.insert(0, config.metal_base_path + "bin/");
			config.metal_llc.insert(0, config.metal_base_path + "bin/");
			config.metal_as.insert(0, config.metal_base_path + "bin/");
			config.metal_dis.insert(0, config.metal_base_path + "bin/");
		}
		
		config.execution_model = config_doc.get<string>("compute.host.exec_model", "mt-group");
		extract_whitelist(config.host_whitelist, "compute.host.whitelist");
	}
	
	// init logger and print out floor info
	logger::init(config.verbosity, config.separate_msg_file, config.append_mode,
				 config.log_use_time, config.log_use_color,
				 config.log_filename, config.msg_filename);
	log_debug("%s", (FLOOR_VERSION_STRING).c_str());
	
	//
	evt = new event();
	evt->add_internal_event_handler(event_handler_fnctr, EVENT_TYPE::WINDOW_RESIZE, EVENT_TYPE::KERNEL_RELOAD);
	
	//
	init_internal(use_gl33_, window_flags);
}

void floor::destroy() {
	log_debug("destroying floor ...");
	
	if(!console_only) acquire_context();
	
#if !defined(FLOOR_NO_OPENAL)
	if(!config.audio_disabled) {
		audio_controller::destroy();
	}
#endif
	
	evt->remove_event_handler(event_handler_fnctr);
	
	compute_ctx = nullptr;
	
	// delete this at the end, b/c other classes will remove event handlers
	if(evt != nullptr) delete evt;
	
	if(!console_only) {
		release_context();
		
		SDL_GL_DeleteContext(config.ctx);
		SDL_DestroyWindow(config.wnd);
	}
	SDL_Quit();
	
	log_debug("floor destroyed!");
}

void floor::init_internal(const bool use_gl33
#if defined(FLOOR_IOS)
						  floor_unused // only used on desktop platforms
#endif
						  , const unsigned int window_flags) {
	log_debug("initializing floor");

	// initialize sdl
	if(SDL_Init(console_only ? 0 : SDL_INIT_VIDEO) == -1) {
		log_error("failed to initialize SDL: %s", SDL_GetError());
		exit(1);
	}
	else {
		log_debug("sdl initialized");
	}
	atexit(SDL_Quit);

	// only initialize opengl/opencl and create a window when not in console-only mode
	if(!console_only) {
		// set window creation flags
		config.flags = window_flags;
		
		config.flags |= SDL_WINDOW_ALLOW_HIGHDPI; // allow by default, disable later if necessary
#if !defined(FLOOR_IOS)
		int2 windows_pos(SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
		if(config.fullscreen) {
			config.flags |= SDL_WINDOW_FULLSCREEN;
			config.flags |= SDL_WINDOW_BORDERLESS;
			windows_pos.set(0, 0);
			log_debug("fullscreen enabled");
		}
		else {
			log_debug("fullscreen disabled");
		}
#else
		// always set fullscreen + borderless on iOS
		config.flags |= SDL_WINDOW_OPENGL;
		config.flags |= SDL_WINDOW_FULLSCREEN;
		config.flags |= SDL_WINDOW_BORDERLESS;
		config.flags |= SDL_WINDOW_RESIZABLE;
		config.flags |= SDL_WINDOW_SHOWN;
#endif
		
		log_debug("vsync %s", config.vsync ? "enabled" : "disabled");
		
		// disable hidpi mode?
		SDL_SetHint("SDL_VIDEO_HIGHDPI_DISABLED", config.hidpi ? "0" : "1");
		log_debug("hidpi %s", config.hidpi ? "enabled" : "disabled");
		
		// gl attributes
		SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
		SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
		SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
		SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 8);
		SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
		
#if !defined(FLOOR_IOS)
		if(use_gl33) {
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
#else
#if defined(PLATFORM_X32)
		SDL_SetHint(SDL_HINT_RENDER_DRIVER, "opengles2");
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
#else
		SDL_SetHint(SDL_HINT_RENDER_DRIVER, "opengles3");
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
#endif
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
		
		//
		SDL_DisplayMode fullscreen_mode;
		SDL_zero(fullscreen_mode);
		fullscreen_mode.format = SDL_PIXELFORMAT_RGBA8888;
		fullscreen_mode.w = (int)config.width;
		fullscreen_mode.h = (int)config.height;
#endif
		
		// create screen
#if !defined(FLOOR_IOS)
		config.wnd = SDL_CreateWindow("floor", windows_pos.x, windows_pos.y, (int)config.width, (int)config.height, config.flags);
#else
		config.wnd = SDL_CreateWindow("floor", 0, 0, (int)config.width, (int)config.height, config.flags);
#endif
		if(config.wnd == nullptr) {
			log_error("can't create window: %s", SDL_GetError());
			exit(1);
		}
		else {
			SDL_GetWindowSize(config.wnd, (int*)&config.width, (int*)&config.height);
			log_debug("video mode set: w%u h%u", config.width, config.height);
		}
		
#if defined(FLOOR_IOS)
		if(SDL_SetWindowDisplayMode(config.wnd, &fullscreen_mode) < 0) {
			log_error("can't set up fullscreen display mode: %s", SDL_GetError());
			exit(1);
		}
		SDL_GetWindowSize(config.wnd, (int*)&config.width, (int*)&config.height);
		log_debug("fullscreen mode set: w%u h%u", config.width, config.height);
		SDL_ShowWindow(config.wnd);
#endif
		
		config.ctx = SDL_GL_CreateContext(config.wnd);
		if(config.ctx == nullptr) {
			log_error("can't create opengl context: %s", SDL_GetError());
			exit(1);
		}
#if !defined(FLOOR_IOS)
		// has to be set after context creation
		if(SDL_GL_SetSwapInterval(config.vsync ? 1 : 0) == -1) {
			log_error("error setting the gl swap interval to %v (vsync): %s", config.vsync, SDL_GetError());
			SDL_ClearError();
		}
		
		// enable multi-threaded opengl context when on os x
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
	acquire_context();
	
	if(!console_only) {
		log_debug("window and opengl context created and acquired!");
		
		// initialize opengl functions (get function pointers) on non-apple platforms
#if !defined(__APPLE__)
		init_gl_funcs();
#endif
		
#if !defined(FLOOR_IOS) || defined(PLATFORM_X64)
		if(is_gl_version(3, 0)) {
			// create and bind vao
			glGenVertexArrays(1, &global_vao);
			glBindVertexArray(global_vao);
		}
#endif
		
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
		
		if(SDL_GetCurrentVideoDriver() == nullptr) {
			log_error("couldn't get video driver: %s!", SDL_GetError());
		}
		else log_debug("video driver: %s", SDL_GetCurrentVideoDriver());
		
		evt->set_ldouble_click_time((unsigned int)config.ldouble_click_time);
		evt->set_rdouble_click_time((unsigned int)config.rdouble_click_time);
		evt->set_mdouble_click_time((unsigned int)config.mdouble_click_time);
		
		// initialize ogl
		init_gl();
		log_debug("opengl initialized");
		
		// resize stuff
		resize_window();
		
		// retrieve dpi info
		if(config.dpi == 0) {
#if defined(__APPLE__)
			config.dpi = darwin_helper::get_dpi(config.wnd);
#elif defined(__WINDOWS__)
			HDC hdc = wglGetCurrentDC();
			const size2 display_res((size_t)GetDeviceCaps(hdc, HORZRES), (size_t)GetDeviceCaps(hdc, VERTRES));
			const float2 display_phys_size(GetDeviceCaps(hdc, HORZSIZE), GetDeviceCaps(hdc, VERTSIZE));
			const float2 display_dpi((float(display_res.x) / display_phys_size.x) * 25.4f,
									 (float(display_res.y) / display_phys_size.y) * 25.4f);
			config.dpi = (size_t)floorf(std::max(display_dpi.x, display_dpi.y));
#else // x11
			SDL_SysWMinfo wm_info;
			SDL_VERSION(&wm_info.version);
			if(SDL_GetWindowWMInfo(config.wnd, &wm_info) == 1) {
				Display* display = wm_info.info.x11.display;
				const size2 display_res((size_t)DisplayWidth(display, 0), (size_t)DisplayHeight(display, 0));
				const float2 display_phys_size(DisplayWidthMM(display, 0), DisplayHeightMM(display, 0));
				const float2 display_dpi((float(display_res.x) / display_phys_size.x) * 25.4f,
										 (float(display_res.y) / display_phys_size.y) * 25.4f);
				config.dpi = (size_t)floorf(std::max(display_dpi.x, display_dpi.y));
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
		
		// default compute backends (will try these in order, using the first working one)
#if defined(__APPLE__)
#if !defined(FLOOR_IOS) // osx
		vector<COMPUTE_TYPE> compute_defaults { COMPUTE_TYPE::METAL, COMPUTE_TYPE::CUDA, COMPUTE_TYPE::OPENCL };
#else // ios
		vector<COMPUTE_TYPE> compute_defaults { COMPUTE_TYPE::METAL };
#endif
#else // linux, windows, ...
		vector<COMPUTE_TYPE> compute_defaults { COMPUTE_TYPE::OPENCL, COMPUTE_TYPE::CUDA };
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
					compute_ctx = make_shared<cuda_compute>();
#endif
					break;
				case COMPUTE_TYPE::OPENCL:
#if !defined(FLOOR_NO_OPENCL)
					if(!config.opencl_toolchain_exists) break;
					log_debug("initializing OpenCL ...");
					compute_ctx = make_shared<opencl_compute>();
#endif
					break;
				case COMPUTE_TYPE::METAL:
#if !defined(FLOOR_NO_METAL)
					if(!config.metal_toolchain_exists) break;
					log_debug("initializing Metal ...");
					compute_ctx = make_shared<metal_compute>();
#endif
					break;
				case COMPUTE_TYPE::HOST:
#if !defined(FLOOR_NO_HOST_COMPUTE)
					log_debug("initializing Host Compute ...");
					compute_ctx = make_shared<host_compute>();
#endif
					break;
				default: break;
			}
			
			if(compute_ctx != nullptr) {
				compute_ctx->init(config.opencl_platform,
								  config.gl_sharing & !console_only,
								  backend == COMPUTE_TYPE::OPENCL ? config.opencl_whitelist :
								  backend == COMPUTE_TYPE::CUDA ? config.cuda_whitelist :
								  backend == COMPUTE_TYPE::METAL ? config.metal_whitelist :
								  backend == COMPUTE_TYPE::HOST ? config.host_whitelist :
								  unordered_set<string> {});
				
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
}

void floor::set_screen_size(const uint2& screen_size) {
	if(screen_size.x == config.width && screen_size.y == config.height) return;
	config.width = screen_size.x;
	config.height = screen_size.y;
	SDL_SetWindowSize(config.wnd, (int)config.width, (int)config.height);
	
	SDL_Rect bounds;
	SDL_GetDisplayBounds(0, &bounds);
	SDL_SetWindowPosition(config.wnd,
						  bounds.x + (bounds.w - int(config.width)) / 2,
						  bounds.y + (bounds.h - int(config.height)) / 2);
}

void floor::set_fullscreen(const bool& state) {
	if(state == config.fullscreen) return;
	config.fullscreen = state;
	if(SDL_SetWindowFullscreen(config.wnd, (SDL_bool)state) != 0) {
		log_error("failed to %s fullscreen: %s!",
				  (state ? "enable" : "disable"), SDL_GetError());
	}
	evt->add_event(EVENT_TYPE::WINDOW_RESIZE,
				   make_shared<window_resize_event>(SDL_GetTicks(),
													size2(config.width, config.height)));
	// TODO: border?
}

void floor::set_vsync(const bool& state) {
	if(state == config.vsync) return;
	config.vsync = state;
#if !defined(FLOOR_IOS)
	SDL_GL_SetSwapInterval(config.vsync ? 1 : 0);
#endif
}

/*! starts drawing the window
 */
void floor::start_draw() {
	acquire_context();
}

/*! stops drawing the window
 */
void floor::stop_draw(const bool window_swap) {
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
			log_error("unknown OpenGL error: %u!");
			break;
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
	SDL_SetWindowTitle(config.wnd, caption.c_str());
}

/*! returns the window caption
 */
string floor::get_caption() {
	return SDL_GetWindowTitle(config.wnd);
}

/*! opengl initialization function
 */
void floor::init_gl() {
	glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
	
	glClearDepth(1.0f);
	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LESS);
	
	glDisable(GL_STENCIL_TEST);
	
	glFrontFace(GL_CW);
	glCullFace(GL_BACK);
	glEnable(GL_CULL_FACE);
}

/* function to reset our viewport after a window resize
 */
void floor::resize_window() {
	// set the viewport
	glViewport(0, 0, (GLsizei)config.width, (GLsizei)config.height);
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
	return floor::evt;
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

unsigned int floor::get_fps() {
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

unsigned int floor::get_width() {
	return (unsigned int)config.width;
}

unsigned int floor::get_height() {
	return (unsigned int)config.height;
}

uint2 floor::get_screen_size() {
	return uint2((unsigned int)config.width, (unsigned int)config.height);
}

unsigned int floor::get_physical_width() {
	unsigned int ret = (unsigned int)config.width;
#if defined(__APPLE__) // only supported on osx and ios right now
	ret = uint32_t(double(ret) *
				   (config.hidpi ?
					double(darwin_helper::get_scale_factor(config.wnd)) : 1.0));
#endif
	return ret;
}

unsigned int floor::get_physical_height() {
	unsigned int ret = (unsigned int)config.height;
#if defined(__APPLE__) // only supported on osx and ios right now
	ret = uint32_t(double(ret) *
				   (config.hidpi ?
					double(darwin_helper::get_scale_factor(config.wnd)) : 1.0));
#endif
	return ret;
}

uint2 floor::get_physical_screen_size() {
	uint2 ret { (unsigned int)config.width, (unsigned int)config.height };
#if defined(__APPLE__) // only supported on osx and ios right now
	ret = uint2(double2(ret) *
				(config.hidpi ?
				 double(darwin_helper::get_scale_factor(config.wnd)) : 1.0));
#endif
	return ret;
}

unsigned int floor::get_key_repeat() {
	return (unsigned int)config.key_repeat;
}

unsigned int floor::get_ldouble_click_time() {
	return (unsigned int)config.ldouble_click_time;
}

unsigned int floor::get_mdouble_click_time() {
	return (unsigned int)config.mdouble_click_time;
}

unsigned int floor::get_rdouble_click_time() {
	return (unsigned int)config.rdouble_click_time;
}

SDL_Window* floor::get_window() {
	return config.wnd;
}

unsigned int floor::get_window_flags() {
	return config.flags;
}

SDL_GLContext floor::get_context() {
	return config.ctx;
}

const string floor::get_version() {
	return FLOOR_VERSION_STRING;
}

void floor::swap() {
	SDL_GL_SwapWindow(config.wnd);
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
				   make_shared<window_resize_event>(SDL_GetTicks(), size2(config.width, config.height)));
}

const float2& floor::get_near_far_plane() {
	return config.near_far_plane;
}

const uint64_t& floor::get_dpi() {
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
	config.ctx_lock.lock();
	// note: not a race, since there can only be one active gl thread
	const unsigned int cur_active_locks = config.ctx_active_locks++;
	if(use_gl_context) {
		if(cur_active_locks == 0 && config.ctx != nullptr) {
			if(SDL_GL_MakeCurrent(config.wnd, config.ctx) != 0) {
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
	const unsigned int cur_active_locks = --config.ctx_active_locks;
	if(use_gl_context) {
		if(cur_active_locks == 0 && config.ctx != nullptr) {
			if(SDL_GL_MakeCurrent(config.wnd, nullptr) != 0) {
				log_error("couldn't release current gl context: %s!", SDL_GetError());
				return;
			}
		}
	}
	config.ctx_lock.unlock();
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
		resize_window();
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
	return darwin_helper::get_scale_factor(config.wnd);
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

const string& floor::get_compute_backend() {
	return config.backend;
}
bool floor::get_compute_gl_sharing() {
	return config.gl_sharing;
}
bool floor::get_compute_debug() {
	return config.debug;
}
bool floor::get_compute_profiling() {
	return config.profiling;
}
bool floor::get_compute_log_binaries() {
	return config.log_binaries;
}
bool floor::get_compute_keep_temp() {
	return config.keep_temp;
}
bool floor::get_compute_keep_binaries() {
	return config.keep_binaries;
}
bool floor::get_compute_use_cache() {
	return config.use_cache;
}
bool floor::get_compute_log_commands() {
	return config.log_commands;
}

const string& floor::get_compute_default_compiler() {
	return config.default_compiler;
}
const string& floor::get_compute_default_llc() {
	return config.default_llc;
}
const string& floor::get_compute_default_as() {
	return config.default_as;
}
const string& floor::get_compute_default_dis() {
	return config.default_dis;
}

const string& floor::get_opencl_base_path() {
	return config.opencl_base_path;
}
const unordered_set<string>& floor::get_opencl_whitelist() {
	return config.opencl_whitelist;
}
const uint64_t& floor::get_opencl_platform() {
	return config.opencl_platform;
}
bool floor::get_opencl_verify_spir() {
	return config.opencl_verify_spir;
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
const string& floor::get_opencl_spir_encoder() {
	return config.opencl_spir_encoder;
}
const string& floor::get_opencl_spir_verifier() {
	return config.opencl_spir_verifier;
}
const string& floor::get_opencl_applecl_encoder() {
	return config.opencl_applecl_encoder;
}

const string& floor::get_cuda_base_path() {
	return config.cuda_base_path;
}
const unordered_set<string>& floor::get_cuda_whitelist() {
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

const string& floor::get_metal_base_path() {
	return config.metal_base_path;
}
const unordered_set<string>& floor::get_metal_whitelist() {
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

const unordered_set<string>& floor::get_host_whitelist() {
	return config.host_whitelist;
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
	if((uint32_t)(version[0] - '0') > major) return true;
	else if((uint32_t)(version[0] - '0') == major && (uint32_t)(version[2] - '0') >= minor) return true;
	return false;
}

const string& floor::get_gl_vendor() {
	return gl_vendor;
}
