/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2013 Florian Ziesche
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

#include "floor/floor.hpp"
#include "floor/floor_version.hpp"
#include "cl/opencl.hpp"
#include "core/gl_support.hpp"

#if defined(__APPLE__)
#if !defined(FLOOR_IOS)
#include "osx/osx_helper.hpp"
#else
#include "ios/ios_helper.hpp"
#endif
#endif

#if defined(FLOOR_NO_OPENCL)
class opencl_base {};
#endif

// init statics
event* floor::evt = nullptr;
xml* floor::x = nullptr;
opencl_base* ocl = nullptr;
bool floor::console_only = false;

struct floor::floor_config floor::config;
xml::xml_doc floor::config_doc;

string floor::datapath = "";
string floor::rel_datapath = "";
string floor::callpath = "";
string floor::kernelpath = "";
string floor::abs_bin_path = "";
string floor::config_name = "config.xml";

unsigned int floor::fps = 0;
unsigned int floor::fps_counter = 0;
unsigned int floor::fps_time = 0;
float floor::frame_time = 0.0f;
unsigned int floor::frame_time_sum = 0;
unsigned int floor::frame_time_counter = 0;
bool floor::new_fps_count = false;

bool floor::cursor_visible = true;

event::handler* floor::event_handler_fnctr = nullptr;

atomic<bool> floor::reload_kernels_flag { false };

// dll main for windows dll export
#if defined(__WINDOWS__)
BOOL APIENTRY DllMain(HANDLE hModule floor_unused, DWORD ul_reason_for_call, LPVOID lpReserved floor_unused);
BOOL APIENTRY DllMain(HANDLE hModule floor_unused, DWORD ul_reason_for_call, LPVOID lpReserved floor_unused) {
	switch(ul_reason_for_call) {
		case DLL_PROCESS_ATTACH:
		case DLL_THREAD_ATTACH:
		case DLL_THREAD_DETACH:
		case DLL_PROCESS_DETACH:
			break;
	}
	return TRUE;
}
#endif // __WINDOWS__

/*! this is used to set an absolute data path depending on call path (path from where the binary is called/started),
 *! which is mostly needed when the binary is opened via finder under os x or any file manager under linux
 */
void floor::init(const char* callpath_, const char* datapath_,
				 const bool console_only_, const string config_name_,
				 const bool use_gl32_core_, const unsigned int window_flags) {
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
	
	// init xml and load config
	x = new xml();
	const string config_filename(config_name +
								 (file_io::is_file(data_path(config_name + ".local")) ? ".local" : ""));
	config_doc = x->process_file(data_path(config_filename));
	if(config_doc.valid) {
		config.width = config_doc.get<size_t>("config.screen.width", 1280);
		config.height = config_doc.get<size_t>("config.screen.height", 720);
		config.fullscreen = config_doc.get<bool>("config.screen.fullscreen", false);
		config.vsync = config_doc.get<bool>("config.screen.vsync", false);
		config.stereo = config_doc.get<bool>("config.screen.stereo", false);
		
		config.verbosity = config_doc.get<size_t>("config.logging.verbosity", 4);
		config.separate_msg_file = config_doc.get<bool>("config.logging.separate_msg_file", false);
		config.append_mode = config_doc.get<bool>("config.logging.append_mode", false);
		config.log_filename = config_doc.get<string>("config.logging.log_filename", "log.txt");
		config.msg_filename = config_doc.get<string>("config.logging.msg_filename", "msg.txt");
		
		config.fov = config_doc.get<float>("config.projection.fov", 72.0f);
		config.near_far_plane.x = config_doc.get<float>("config.projection.near", 1.0f);
		config.near_far_plane.y = config_doc.get<float>("config.projection.far", 1000.0f);
		config.upscaling = config_doc.get<float>("config.projection.upscaling", 1.0f);
		
		config.key_repeat = config_doc.get<size_t>("config.input.key_repeat", 200);
		config.ldouble_click_time = config_doc.get<size_t>("config.input.ldouble_click_time", 200);
		config.mdouble_click_time = config_doc.get<size_t>("config.input.mdouble_click_time", 200);
		config.rdouble_click_time = config_doc.get<size_t>("config.input.rdouble_click_time", 200);
		
		config.opencl_platform = config_doc.get<string>("config.opencl.platform", "0");
		config.clear_cache = config_doc.get<bool>("config.opencl.clear_cache", false);
		config.gl_sharing = config_doc.get<bool>("config.opencl.gl_sharing", true);
		config.log_binaries = config_doc.get<bool>("config.opencl.log_binaries", false);
		const auto cl_dev_tokens(core::tokenize(config_doc.get<string>("config.opencl.restrict", ""), ','));
		for(const auto& dev_token : cl_dev_tokens) {
			if(dev_token == "") continue;
			config.cl_device_restriction.insert(dev_token);
		}
		
		config.cuda_base_dir = config_doc.get<string>("config.cuda.base_dir", "/usr/local/cuda");
		config.cuda_debug = config_doc.get<bool>("config.cuda.debug", false);
		config.cuda_profiling = config_doc.get<bool>("config.cuda.profiling", false);
		config.cuda_keep_temp = config_doc.get<bool>("config.cuda.keep_temp", false);
		config.cuda_keep_binaries = config_doc.get<bool>("config.cuda.keep_binaries", true);
		config.cuda_use_cache = config_doc.get<bool>("config.cuda.use_cache", true);
	}
	
	// init logger and print out floor info
	logger::init(config.verbosity, config.separate_msg_file, config.append_mode,
				 config.log_filename, config.msg_filename);
	log_debug("%s", (FLOOR_VERSION_STRING).c_str());
	
	//
	evt = new event();
	event_handler_fnctr = new event::handler(&floor::event_handler);
	evt->add_internal_event_handler(*event_handler_fnctr, EVENT_TYPE::WINDOW_RESIZE, EVENT_TYPE::KERNEL_RELOAD);
	
	//
	init_internal(use_gl32_core_, window_flags);
}

void floor::destroy() {
	log_debug("destroying floor ...");
	
	if(!console_only) acquire_context();
	
	evt->remove_event_handler(*event_handler_fnctr);
	delete event_handler_fnctr;
	
	if(x != nullptr) delete x;
#if !defined(FLOOR_NO_OPENCL)
	if(ocl != nullptr) {
		delete ocl;
		ocl = nullptr;
	}
#endif
	
	// delete this at the end, b/c other classes will remove event handlers
	if(evt != nullptr) delete evt;
	
	if(!console_only) {
		release_context();
		
		SDL_GL_DeleteContext(config.ctx);
		SDL_DestroyWindow(config.wnd);
	}
	SDL_Quit();
	
	log_debug("floor destroyed!");
	logger::destroy();
}

void floor::init_internal(const bool use_gl32_core
#if !defined(__APPLE__) || defined(FLOOR_IOS)
						  floor_unused // use_gl32_core is only used on os x
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
		
		// gl attributes
		SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
		SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
		SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
		SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 8);
		
#if !defined(FLOOR_IOS)
#if defined(__APPLE__)
		// only default to opengl 3.2 core on os x for now (opengl version doesn't really matter on other platforms)
		if(use_gl32_core) {
			SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
			SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);
			SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
		}
		else {
			SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
			SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
			SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_COMPATIBILITY);
		}
#endif
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
		if(config.vsync && SDL_GL_SetSwapInterval(1) == -1) {
			log_error("error setting the gl swap interval to 1 (vsync): %s", SDL_GetError());
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
		
		acquire_context();
		log_debug("window and opengl context created and acquired!");
		
		// initialize opengl functions (get function pointers) on non-apple platforms
#if !defined(__APPLE__)
		init_gl_funcs();
#endif
		
		// on iOS/GLES we need a simple "blit shader" to draw the opencl framebuffer
#if defined(FLOOR_IOS)
		ios_helper::compile_shaders();
		log_debug("iOS blit shader compiled");
#endif
		
#if !defined(FLOOR_NO_OPENCL)
		// check if a cudacl or pure opencl context should be created
		// use absolute path
#if defined(FLOOR_CUDA_CL)
		if(config.opencl_platform == "cuda") {
			log_debug("initializing cuda ...");
			ocl = new cudacl(core::strip_path(string(datapath + kernelpath)).c_str(), config.wnd, config.clear_cache);
		}
		else {
#else
			if(config.opencl_platform == "cuda") {
				log_error("CUDA support is not enabled!");
			}
#endif
			log_debug("initializing opencl ...");
			ocl = new opencl(core::strip_path(string(datapath + kernelpath)).c_str(), config.wnd, config.clear_cache);
#if defined(FLOOR_CUDA_CL)
		}
#endif
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
		log_debug("vendor: %s", glGetString(GL_VENDOR));
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
#if !defined(FLOOR_IOS)
			config.dpi = osx_helper::get_dpi(config.wnd);
#else
			// TODO: iOS
			config.dpi = 326;
#endif
#elif defined(__WINDOWS__)
			HDC hdc = wglGetCurrentDC();
			const size2 display_res(GetDeviceCaps(hdc, HORZRES), GetDeviceCaps(hdc, VERTRES));
			const float2 display_phys_size(GetDeviceCaps(hdc, HORZSIZE), GetDeviceCaps(hdc, VERTSIZE));
			const float2 display_dpi((float(display_res.x) / display_phys_size.x) * 25.4f,
									 (float(display_res.y) / display_phys_size.y) * 25.4f);
			config.dpi = floorf(std::max(display_dpi.x, display_dpi.y));
#else // x11
			SDL_SysWMinfo wm_info;
			SDL_VERSION(&wm_info.version);
			if(SDL_GetWindowWMInfo(config.wnd, &wm_info) == 1) {
				Display* display = wm_info.info.x11.display;
				const size2 display_res(DisplayWidth(display, 0), DisplayHeight(display, 0));
				const float2 display_phys_size(DisplayWidthMM(display, 0), DisplayHeightMM(display, 0));
				const float2 display_dpi((float(display_res.x) / display_phys_size.x) * 25.4f,
										 (float(display_res.y) / display_phys_size.y) * 25.4f);
				config.dpi = floorf(std::max(display_dpi.x, display_dpi.y));
			}
#endif
		}
		
		// set dpi lower bound to 72
		if(config.dpi < 72) config.dpi = 72;
		
#if !defined(FLOOR_NO_OPENCL)
		// init opencl
		ocl->init(false,
				  config.opencl_platform == "cuda" ? 0 : string2size_t(config.opencl_platform),
				  config.cl_device_restriction, config.gl_sharing);
#endif
		
		release_context();
	}
}

/*! sets the windows width
 *  @param width the window width
 */
void floor::set_width(const unsigned int& width) {
	if(width == config.width) return;
	config.width = width;
	SDL_SetWindowSize(config.wnd, (int)config.width, (int)config.height);
	// TODO: make this work:
	/*SDL_SetWindowPosition(config.wnd,
						  config.fullscreen ? 0 : SDL_WINDOWPOS_CENTERED,
						  config.fullscreen ? 0 : SDL_WINDOWPOS_CENTERED);*/
}

/*! sets the window height
 *  @param height the window height
 */
void floor::set_height(const unsigned int& height) {
	if(height == config.height) return;
	config.height = height;
	SDL_SetWindowSize(config.wnd, (int)config.width, (int)config.height);
	// TODO: make this work:
	/*SDL_SetWindowPosition(config.wnd,
						  config.fullscreen ? 0 : SDL_WINDOWPOS_CENTERED,
						  config.fullscreen ? 0 : SDL_WINDOWPOS_CENTERED);*/
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
	// TODO: make this work:
	/*SDL_SetWindowPosition(config.wnd,
						  config.fullscreen ? 0 : SDL_WINDOWPOS_CENTERED,
						  config.fullscreen ? 0 : SDL_WINDOWPOS_CENTERED);*/
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
#if !defined(FLOOR_NO_OPENCL)
	if(reload_kernels_flag) {
		reload_kernels_flag = false;
		ocl->flush();
		ocl->finish();
		ocl->reload_kernels();
	}
#endif
	
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

/*! returns the xml class
 */
xml* floor::get_xml() {
	return floor::x;
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
	if(config.fov == fov) return;
	config.fov = fov;
	evt->add_event(EVENT_TYPE::WINDOW_RESIZE,
				   make_shared<window_resize_event>(SDL_GetTicks(), size2(config.width, config.height)));
}

const float2& floor::get_near_far_plane() {
	return config.near_far_plane;
}

const size_t& floor::get_dpi() {
	return config.dpi;
}

xml::xml_doc& floor::get_config_doc() {
	return config_doc;
}

void floor::acquire_context() {
	// note: the context lock is recursive, so one thread can lock
	// it multiple times. however, SDL_GL_MakeCurrent should only
	// be called once (this is the purpose of ctx_active_locks).
	config.ctx_lock.lock();
	// note: not a race, since there can only be one active gl thread
	const unsigned int cur_active_locks = config.ctx_active_locks++;
	if(cur_active_locks == 0) {
		if(SDL_GL_MakeCurrent(config.wnd, config.ctx) != 0) {
			log_error("couldn't make gl context current: %s!", SDL_GetError());
			return;
		}
#if !defined(FLOOR_NO_OPENCL)
		if(ocl != nullptr && ocl->is_supported()) {
			ocl->activate_context();
		}
#endif
	}
#if defined(FLOOR_IOS)
	glBindFramebuffer(GL_FRAMEBUFFER, FLOOR_DEFAULT_FRAMEBUFFER);
#endif
}

void floor::release_context() {
	// only call SDL_GL_MakeCurrent with nullptr, when this is the last lock
	const unsigned int cur_active_locks = --config.ctx_active_locks;
	if(cur_active_locks == 0) {
#if !defined(FLOOR_NO_OPENCL)
		if(ocl != nullptr && ocl->is_supported()) {
			ocl->finish();
			ocl->deactivate_context();
		}
#endif
		if(SDL_GL_MakeCurrent(config.wnd, nullptr) != 0) {
			log_error("couldn't release current gl context: %s!", SDL_GetError());
			return;
		}
	}
	config.ctx_lock.unlock();
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
#if defined(__APPLE__) && !defined(FLOOR_IOS)
	return osx_helper::get_scale_factor(config.wnd);
#else
	return config.upscaling; // TODO: get this from somewhere ...
#endif
}

bool floor::get_gl_sharing() {
	return config.gl_sharing;
}

bool floor::get_log_binaries() {
	return config.log_binaries;
}

const string& floor::get_cuda_base_dir() {
	return config.cuda_base_dir;
}

bool floor::get_cuda_debug() {
	return config.cuda_debug;
}

bool floor::get_cuda_profiling() {
	return config.cuda_profiling;
}

bool floor::get_cuda_keep_temp() {
	return config.cuda_keep_temp;
}

bool floor::get_cuda_keep_binaries() {
	return config.cuda_keep_binaries;
}

bool floor::get_cuda_use_cache() {
	return config.cuda_use_cache;
}

string floor::get_absolute_path() {
	return abs_bin_path;
}
