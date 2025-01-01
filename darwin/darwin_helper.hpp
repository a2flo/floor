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

#include <CoreGraphics/CoreGraphics.h>
#include <floor/core/platform.hpp>
#include <floor/core/cpp_headers.hpp>

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
@class metal_view;
#import <QuartzCore/CAMetalLayer.h>
#import <Metal/Metal.h>
#endif

struct hdr_metadata_t;

class darwin_helper {
public:
	// macOS and iOS
	static uint32_t get_dpi(SDL_Window* wnd);
	static float get_scale_factor(SDL_Window* wnd, const bool force_query = false);
	static size_t get_system_version();
	static size_t get_compiled_system_version();
	static string get_computer_name();
	static string utf8_decomp_to_precomp(const string& str);
	static int64_t get_memory_size();
	static string get_bundle_identifier();
	static string get_pref_path();
	static bool is_running_in_debugger();
	
	//! wrapper around SDL_PollEvent with an autoreleasepool
	static bool sdl_poll_event_wrapper(SDL_Event& event_handle);

#if !defined(FLOOR_IOS) && !defined(FLOOR_VISIONOS)
	// macOS specific
	static void create_app_delegate();
	static float get_menu_bar_height();
#endif
	
public:
	// metal functions (only available in objective-c/c++ mode)
#if defined(__OBJC__)
	static metal_view* create_metal_view(SDL_Window* wnd, id <MTLDevice> device, const hdr_metadata_t& hdr_metadata);
	static CAMetalLayer* get_metal_layer(metal_view* view);
	static id <CAMetalDrawable> get_metal_next_drawable(metal_view* view, id <MTLCommandBuffer> cmd_buffer);
	static MTLPixelFormat get_metal_pixel_format(metal_view* view);
	static uint2 get_metal_view_dim(metal_view* view);
	static void set_metal_view_hdr_metadata(metal_view* view, const hdr_metadata_t& hdr_metadata);
	static float get_metal_view_edr_max(metal_view* view);
	static float get_metal_view_hdr_max_nits(metal_view* view);
#endif
	
};
