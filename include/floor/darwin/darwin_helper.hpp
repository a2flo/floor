/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2026 Florian Ziesche
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

#if defined(FLOOR_IOS)
#define floor_unused_on_ios floor_unused
#define floor_unused_on_ios_and_visionos floor_unused
#define floor_unused_on_visionos
#define floor_unused_on_macos
#elif defined(FLOOR_VISIONOS)
#define floor_unused_on_ios
#define floor_unused_on_ios_and_visionos floor_unused
#define floor_unused_on_visionos floor_unused
#define floor_unused_on_macos
#else
#define floor_unused_on_ios
#define floor_unused_on_ios_and_visionos
#define floor_unused_on_visionos
#define floor_unused_on_macos floor_unused
#endif

#if defined(__OBJC__)
@class floor_metal_view;
@class CAMetalLayer;
@protocol MTLCommandBuffer;
@protocol CAMetalDrawable;
#endif

namespace fl {

struct hdr_metadata_t;

class darwin_helper {
public:
	// macOS and iOS
	static uint32_t get_dpi(SDL_Window* wnd);
	static float get_scale_factor(SDL_Window* wnd, const bool force_query = false);
	static size_t get_system_version();
	static size_t get_compiled_system_version();
	static std::string get_computer_name();
	static std::string utf8_decomp_to_precomp(const std::string& str);
	static int64_t get_memory_size();
	static std::string get_bundle_identifier();
	static std::string get_pref_path();
	static bool is_running_in_debugger();
	
	//! wrapper around SDL_PollEvent with an autoreleasepool
	static bool sdl_poll_event_wrapper(SDL_Event& event_handle);
	
#if !defined(FLOOR_IOS) && !defined(FLOOR_VISIONOS)
	// macOS specific
	static void create_app_delegate();
	static float get_menu_bar_height();
#endif
	
public:
	// Metal functions (only available in Objective-C/C++ mode)
#if defined(__OBJC__)
	static floor_metal_view* create_metal_view(SDL_Window* wnd, id <MTLDevice> device, const hdr_metadata_t& hdr_metadata);
	static CAMetalLayer* get_metal_layer(floor_metal_view* view);
	static id <CAMetalDrawable> get_metal_next_drawable(floor_metal_view* view, id <MTLCommandBuffer> cmd_buffer);
	static MTLPixelFormat get_metal_pixel_format(floor_metal_view* view);
	static uint2 get_metal_view_dim(floor_metal_view* view);
	static void set_metal_view_hdr_metadata(floor_metal_view* view, const hdr_metadata_t& hdr_metadata);
	static float get_metal_view_edr_max(floor_metal_view* view);
	static float get_metal_view_hdr_max_nits(floor_metal_view* view);
#endif
	
};

} // namespace fl
