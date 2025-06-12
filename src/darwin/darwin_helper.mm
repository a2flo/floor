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

#if !defined(FLOOR_IOS) && !defined(FLOOR_VISIONOS)
#include <Cocoa/Cocoa.h>
#endif
#include <SDL3/SDL.h>
#include <SDL3/SDL_events.h>
#include <sys/types.h>
#include <sys/sysctl.h>
#include <dlfcn.h>
#include <floor/math/vector_lib.hpp>
#include <floor/core/logger.hpp>
#include <floor/core/cpp_ext.hpp>
#include <floor/darwin/darwin_helper.hpp>
#include <floor/floor.hpp>
#include <floor/constexpr/const_math.hpp>
#include <floor/core/hdr_metadata.hpp>

#if !defined(FLOOR_IOS) && !defined(FLOOR_VISIONOS)
#import <AppKit/AppKit.h>
#import <AppKit/NSApplication.h>
#define UI_VIEW_CLASS NSView
#else
#import <UIKit/UIKit.h>
#define UI_VIEW_CLASS UIView
#endif

#import <Foundation/NSData.h>

// cocoa or uikit window type
#if !defined(FLOOR_IOS) && !defined(FLOOR_VISIONOS)
using wnd_type_ptr = NSWindow*;
#else
using wnd_type_ptr = UIWindow*;
#endif

using namespace std::literals;

#if !defined(FLOOR_IOS) && !defined(FLOOR_VISIONOS)
// we need to create an app delegate to disable applicationSupportsSecureRestorableState warnings ...
// however, if we do this, we also need to handle SDL things (https://wiki.libsdl.org/SDL3/README/macos/raw)
@interface libfloor_app_delegate : NSObject <NSApplicationDelegate>
- (NSApplicationTerminateReply)applicationShouldTerminate:(NSApplication *)sender;
- (BOOL)application:(NSApplication *)theApplication openFile:(NSString *)filename;
- (BOOL)applicationSupportsSecureRestorableState:(NSApplication *)app;
@end

@implementation libfloor_app_delegate
- (NSApplicationTerminateReply)applicationShouldTerminate:(NSApplication *)sender
{
	if (SDL_EventEnabled(SDL_EVENT_QUIT)) {
		SDL_Event event;
		event.type = SDL_EVENT_QUIT;
		SDL_PushEvent(&event);
	}
	return NSTerminateCancel;
}

- (BOOL)application:(NSApplication *)theApplication openFile:(NSString *)filename
{
	if (SDL_EventEnabled(SDL_EVENT_DROP_FILE)) {
		SDL_Event event;
		event.type = SDL_EVENT_DROP_FILE;
		event.drop.data = SDL_strdup([filename UTF8String]);
		return SDL_PushEvent(&event);
	}
	return NO;
}

- (BOOL)applicationSupportsSecureRestorableState:(NSApplication *)app {
	return YES;
}
@end

namespace fl {

static libfloor_app_delegate* libfloor_app_delegate_instance = nil;
void darwin_helper::create_app_delegate() {
	if (libfloor_app_delegate_instance) {
		return;
	}
	@autoreleasepool {
		libfloor_app_delegate_instance = [libfloor_app_delegate new];
		[[NSApplication sharedApplication] setDelegate:libfloor_app_delegate_instance];
	}
}
} // namespace fl
#endif

// returns the underlying native NSWindow/UIWindow window
static auto get_native_window(SDL_Window* wnd) {
#if !defined(FLOOR_IOS) && !defined(FLOOR_VISIONOS)
	if (NSWindow* cocoa_wnd = (__bridge NSWindow*)SDL_GetPointerProperty(SDL_GetWindowProperties(wnd), SDL_PROP_WINDOW_COCOA_WINDOW_POINTER, nullptr);
		cocoa_wnd) {
		return cocoa_wnd;
	}
	log_error("failed to retrieve native window: $", SDL_GetError());
	return (NSWindow*)nullptr;
#else
	if (UIWindow* uikit_wnd = (__bridge UIWindow*)SDL_GetPointerProperty(SDL_GetWindowProperties(wnd), SDL_PROP_WINDOW_UIKIT_WINDOW_POINTER, nullptr);
		uikit_wnd) {
		return uikit_wnd;
	}
	log_error("failed to retrieve native window: $", SDL_GetError());
	return (UIWindow*)nullptr;
#endif
}

// global Metal view variables
static constexpr const uint32_t max_drawables_in_flight { 6 };
static std::atomic<bool> window_did_resize { true };

// Metal renderer NSView/UIView implementation
@interface floor_metal_view : UI_VIEW_CLASS <NSCoding> {
@public
FLOOR_PUSH_WARNINGS()
FLOOR_IGNORE_WARNING(objc-interface-ivars)
	//! current amount of frames that can be scheduled
	std::atomic<uint32_t> max_scheduled_frames;
	//! required lock for "available_frame_cv"
	std::mutex available_frame_cv_lock;
	//! if all frames are in flight, this is the CV we will wait on
	//! NOTE: this is more efficient and faster than spin looping
	std::condition_variable available_frame_cv;
	
	std::chrono::time_point<std::chrono::high_resolution_clock> tp_prev_frame;
	CGColorSpaceRef colorspace_ref;
FLOOR_POP_WARNINGS()
}
@property (unsafe_unretained, nonatomic) CAMetalLayer* metal_layer;
@property (assign, nonatomic) bool is_hidpi;
@property (assign, nonatomic) bool is_vsync;
@property (assign, nonatomic) bool is_wide_gamut;
@property (assign, nonatomic) bool is_hdr;
@property (assign, nonatomic) bool is_hdr_linear;
@property (unsafe_unretained, nonatomic) wnd_type_ptr wnd;
@property (assign, nonatomic) uint32_t refresh_rate;
@end

@implementation floor_metal_view
@synthesize metal_layer = _metal_layer;
@synthesize is_hidpi = _is_hidpi;
@synthesize is_vsync = _is_vsync;
@synthesize is_wide_gamut = _is_wide_gamut;
@synthesize is_hdr = _is_hdr;
@synthesize is_hdr_linear = _is_hdr_linear;
@synthesize wnd = _wnd;
@synthesize refresh_rate = _refresh_rate;

// override to signal this is a CAMetalLayer
+ (Class)layerClass {
	return [CAMetalLayer class];
}

// override to signal this is a CAMetalLayer
- (CALayer*)makeBackingLayer {
	return [CAMetalLayer layer];
}

- (CGRect)create_frame {
	const CGRect wnd_frame = {
#if !defined(FLOOR_IOS) && !defined(FLOOR_VISIONOS)
		[self.wnd contentRectForFrameRect:[self.wnd frame]]
#else
		[self.wnd frame]
#endif
	};
	CGRect frame { { 0.0, 0.0 }, { wnd_frame.size.width, wnd_frame.size.height } };
	return frame;
}

- (CGFloat)get_scale_factor {
	return (self.is_hidpi ?
#if defined(FLOOR_IOS)
			[[self.wnd screen] scale]
#elif defined(FLOOR_VISIONOS)
			2.0 // TODO: can we query this somehow? does it make sense?
#else
			[self.wnd backingScaleFactor]
#endif
			: 1.0);
}

- (void)set_hdr_metadata:(const fl::hdr_metadata_t&)hdr_metadata {
	if (!self.is_hdr) {
		return;
	}
	
	// set HDR metadata (thanks for making this complicated Apple ...)
	
	// SEI mastering display colour volume
	struct __attribute__((packed)) SEI_MDCV_message_t {
		struct __attribute__((packed)) {
			uint16_t display_primaries_x;
			uint16_t display_primaries_y;
		} display_primaries[3];
		uint16_t white_point_x;
		uint16_t white_point_y;
		uint32_t max_display_mastering_luminance;
		uint32_t min_display_mastering_luminance;
	};
	static_assert(sizeof(SEI_MDCV_message_t) == 24);
	
	// SEI content light level information
	struct __attribute__((packed)) SEI_CLLI_message_t {
		uint16_t max_content_light_level;
		uint16_t max_pic_average_light_level;
	};
	static_assert(sizeof(SEI_CLLI_message_t) == 4);
	
	// color conversion for display_primaries and white_point_*
	static constexpr const fl::uint2 color_x_range { 5u, 37'000u };
	static constexpr const fl::uint2 color_y_range { 5u, 42'000u };
	const auto convert_color = [](const float& color, const fl::uint2& range) {
		// as defined by CIE 1931 / ISO 11664-1
		// 1 unit == 0.00002
		auto conv = uint32_t(color * 50000.0f);
		if (conv < range.x || conv > range.y) {
			log_error("invalid color $, must be in [$, $]", conv, range.x, range.y);
			conv = fl::math::clamp(conv, range.x, range.y);
		}
		return SDL_Swap16(uint16_t(conv));
	};
	
	// luminance conversion for *_display_mastering_luminance
	static constexpr const fl::uint2 lum_max_range { 50'000u, 100'000'000u };
	static constexpr const fl::uint2 lum_min_range { 1u, 50'000u };
	static constexpr const uint32_t lum_max_req_multiple { 10'000u };
	static constexpr const uint32_t lum_min_req_multiple { 1u };
	const auto convert_luminance = [](const float& cd, const fl::uint2& range, const uint32_t& req_multiple) {
		// 1 unit == 0.0001 cd
		auto conv = uint32_t(cd * 10000.0f);
		if (conv < range.x || conv > range.y) {
			log_error("invalid luminance $, must be in [$, $]", conv, range.x, range.y);
			conv = fl::math::clamp(conv, range.x, range.y);
		}
		// SMPTE ST 2086 requires that luminance is a certain multiple
		const auto multiple_mod = conv % req_multiple;
		if (multiple_mod != 0u) {
			if (multiple_mod < req_multiple / 2u) {
				// round down
				conv -= multiple_mod;
			} else {
				// round up
				conv += req_multiple - multiple_mod;
			}
		}
		return SDL_Swap32(conv);
	};
	
	// init structs (note that all uints are big endian)
	const SEI_MDCV_message_t mdvc {
		.display_primaries = {
			// NOTE: order is GBR
			// green
			{
				convert_color(hdr_metadata.primaries[1].x, color_x_range),
				convert_color(hdr_metadata.primaries[1].y, color_y_range)
			},
			// blue
			{
				convert_color(hdr_metadata.primaries[2].x, color_x_range),
				convert_color(hdr_metadata.primaries[2].y, color_y_range)
			},
			// red
			{
				convert_color(hdr_metadata.primaries[0].x, color_x_range),
				convert_color(hdr_metadata.primaries[0].y, color_y_range)
			},
		},
			.white_point_x = convert_color(hdr_metadata.white_point.x, color_x_range),
			.white_point_y = convert_color(hdr_metadata.white_point.y, color_y_range),
			.max_display_mastering_luminance = convert_luminance(hdr_metadata.luminance.y, lum_max_range, lum_max_req_multiple),
			.min_display_mastering_luminance = convert_luminance(hdr_metadata.luminance.x, lum_min_range, lum_min_req_multiple)
	};
	const SEI_CLLI_message_t clli {
		// NOTE: luminance here is actually in cd
		.max_content_light_level = SDL_Swap16(uint16_t(hdr_metadata.max_content_light_level)),
		.max_pic_average_light_level = SDL_Swap16(uint16_t(hdr_metadata.max_average_light_level))
	};
	// normalize to max nominal luminance so that 1.0 signals max luminance (still can go higher than this)
	const float optical_output_scale = hdr_metadata.luminance.y;
	
	// set metadata
	auto mdvc_data = [NSData dataWithBytesNoCopy:(void*)&mdvc
										  length:sizeof(SEI_MDCV_message_t)
									freeWhenDone:false];
	auto clli_data = [NSData dataWithBytesNoCopy:(void*)&clli
										  length:sizeof(SEI_CLLI_message_t)
									freeWhenDone:false];
	self.metal_layer.EDRMetadata = [CAEDRMetadata HDR10MetadataWithDisplayInfo:mdvc_data
																   contentInfo:clli_data
															opticalOutputScale:optical_output_scale];
}

- (instancetype)initWithWindow:(wnd_type_ptr)wnd
					withDevice:(id <MTLDevice>)device
					 withHiDPI:(bool)hidpi
					 withVSync:(bool)vsync
				 withWideGamut:(bool)wide_gamut
					   withHDR:(bool)hdr
				 withHDRLinear:(bool)hdr_linear
			   withHDRMetadata:(const fl::hdr_metadata_t&)hdr_metadata {
	self.wnd = wnd;
	self.is_hidpi = hidpi;
	self.is_vsync = vsync;
	self.is_hdr = hdr;
	self.is_hdr_linear = hdr_linear;
	// enable if directly set or implicitly if HDR is set
	self.is_wide_gamut = (wide_gamut || self.is_hdr);
	const auto frame = [self create_frame];
	
	self = [super initWithFrame:frame];
	if(self) {
#if !defined(FLOOR_IOS) && !defined(FLOOR_VISIONOS)
		[self setWantsLayer:true];
#endif
		self.metal_layer = (CAMetalLayer*)self.layer;
		self.metal_layer.device = device;
#if !defined(FLOOR_IOS) && !defined(FLOOR_VISIONOS)
		self.metal_layer.displaySyncEnabled = self.is_vsync;
#endif
		if (!self.is_wide_gamut) {
			self.metal_layer.pixelFormat = MTLPixelFormatBGRA8Unorm;
		} else {
#if !defined(FLOOR_IOS) && !defined(FLOOR_VISIONOS)
			// NOTE: float (-> non-normalized) won't do any scaling for wide-gamut or HDR (-> luminance is linear)
			//       normalized uint will do scaling by itself, but rather intransparently
			// -> use float format
			self.metal_layer.pixelFormat = MTLPixelFormatRGBA16Float;
			self.metal_layer.wantsExtendedDynamicRangeContent = true;
			if (self.is_hdr) {
				if (!self.is_hdr_linear) {
					// use BT.2020/2100 colorspace with PQ transfer function
					// NOTE: same as Vulkan "HDR10 (BT2020 color) space to be displayed using the SMPTE ST2084 Perceptual Quantizer (PQ) EOTF"
					colorspace_ref = CGColorSpaceCreateWithName(kCGColorSpaceITUR_2100_PQ);
					self.metal_layer.colorspace = colorspace_ref;
					[self set_hdr_metadata:hdr_metadata];
				} else {
					// use BT.2020 colorspace that is "linearly extended" and has no output mapping (PQ must be applied manually)
					// NOTE: while Apple doesn't state exactly what this is, this more or less looks like scRGB, where 1.0 == 80 nits and 12.5 == 1000 nits,
					//       which however contradicts the value returned by "maximumExtendedDynamicRangeColorComponentValue" that actually returns the
					//       "max frame-average luminance" with 1.0 == 100 nits
					colorspace_ref = CGColorSpaceCreateWithName(kCGColorSpaceExtendedLinearITUR_2020);
					self.metal_layer.colorspace = colorspace_ref;
				}
			} else {
				// use BT.2020 if not HDR
				colorspace_ref = CGColorSpaceCreateWithName(kCGColorSpaceITUR_2020);
				self.metal_layer.colorspace = colorspace_ref;
			}
#else
			self.metal_layer.pixelFormat = MTLPixelFormatBGRA10_XR_sRGB;
			// always use sRGB for now
			// TODO: consider using kCGColorSpaceExtendedSRGB
			colorspace_ref = CGColorSpaceCreateWithName(kCGColorSpaceSRGB);
			self.metal_layer.colorspace = colorspace_ref;
#endif
		}
#if defined(FLOOR_IOS) || defined(FLOOR_VISIONOS)
		self.metal_layer.opaque = true;
		self.metal_layer.backgroundColor = nil;
#endif
		self.metal_layer.framebufferOnly = true; // note: must be false if used for compute processing
		self.metal_layer.contentsScale = [self get_scale_factor];
		
		max_scheduled_frames = max_drawables_in_flight;
		tp_prev_frame = std::chrono::high_resolution_clock::now();
		
#if defined(FLOOR_IOS)
		// need to listen for device orientation change events on iOS (won't happen for macOS)
		[[NSNotificationCenter defaultCenter] addObserver:self
												 selector:@selector(orientationChanged:)
													 name:UIDeviceOrientationDidChangeNotification
												   object:[UIDevice currentDevice]];
		
#elif defined(FLOOR_VISIONOS)
		// nothing to listen to?
#else
		// need to listen for window resize events on macOS (won't happen for iOS)
		[[NSNotificationCenter defaultCenter] addObserver:self
												 selector:@selector(windowDidResize:)
													 name:NSWindowDidResizeNotification
												   object:self.wnd];
#endif
		
		// force reshape after init
		[self reshapeWithForceFrameChange:true];
	}
	return self;
}

- (void)dealloc {
	[[NSNotificationCenter defaultCenter] removeObserver:self];
	CGColorSpaceRelease(colorspace_ref);
}

#if !defined(FLOOR_IOS) && !defined(FLOOR_VISIONOS) // not applicable to iOS/visionOS
- (void)viewDidChangeBackingProperties {
	[super viewDidChangeBackingProperties];
	[self reshape];
}

- (void)windowDidResize:(NSNotification*) floor_unused notification {
	[self reshape];
}
#else // not applicable to macOS
- (void)orientationChanged:(NSNotification*) floor_unused notification {
	[self reshape];
}
#endif

- (void)reshape {
	[self reshapeWithForceFrameChange:false];
}

- (void)reshapeWithForceFrameChange:(bool)force_frame_change {
	self.refresh_rate = fl::floor::get_window_refresh_rate();
	if (self.refresh_rate == 0) {
		// sane fallback
		self.refresh_rate = 60;
	}
	
	bool scale_change = false;
	const auto new_scale = [self get_scale_factor];
	if (fl::const_math::is_unequal(self.metal_layer.contentsScale, new_scale)) {
		scale_change = true;
	}
	
	bool frame_change = force_frame_change;
	auto frame = [self create_frame];
	if (fl::const_math::is_unequal(frame.size.width, self.frame.size.width) ||
		fl::const_math::is_unequal(frame.size.height, self.frame.size.height)) {
		frame_change = true;
	}
	
	if (scale_change) {
		self.metal_layer.contentsScale = new_scale;
	}
	
	if (frame_change) {
		self.frame = frame;
		self.metal_layer.frame = frame;
	}
	
	if (frame_change || scale_change) {
		frame.size.width *= self.metal_layer.contentsScale;
		frame.size.height *= self.metal_layer.contentsScale;
		self.metal_layer.drawableSize = frame.size;
	}
	
	// will force query of scaling factor in get_scale_factor()
	window_did_resize = true;
}
@end

namespace fl {

floor_metal_view* darwin_helper::create_metal_view(SDL_Window* wnd, id <MTLDevice> device, const hdr_metadata_t& hdr_metadata) {
	@autoreleasepool {
		// we always create our own Metal view
#if defined(FLOOR_IOS)
		UIWindow* uikit_wnd = (__bridge UIWindow*)SDL_GetPointerProperty(SDL_GetWindowProperties(wnd), SDL_PROP_WINDOW_UIKIT_WINDOW_POINTER, nullptr);
		if (!uikit_wnd) {
			log_error("failed to retrieve window: $", SDL_GetError());
			return nullptr;
		}
		const bool can_do_hdr = ([[uikit_wnd screen] potentialEDRHeadroom] > 1.0);
#elif defined(FLOOR_VISIONOS)
		UIWindow* uikit_wnd = (__bridge UIWindow*)SDL_GetPointerProperty(SDL_GetWindowProperties(wnd), SDL_PROP_WINDOW_UIKIT_WINDOW_POINTER, nullptr);
		// no way to query this, but we can always query this
		const bool can_do_hdr = true;
#else
		NSWindow* cocoa_wnd = (__bridge NSWindow*)SDL_GetPointerProperty(SDL_GetWindowProperties(wnd), SDL_PROP_WINDOW_COCOA_WINDOW_POINTER, nullptr);
		if (!cocoa_wnd) {
			log_error("failed to retrieve window: $", SDL_GetError());
			return nullptr;
		}
		const bool can_do_hdr = ([[cocoa_wnd screen] maximumPotentialExtendedDynamicRangeColorComponentValue] > 1.0);
#endif
		
		floor_metal_view* view = [[floor_metal_view alloc]
#if !defined(FLOOR_IOS) && !defined(FLOOR_VISIONOS)
								  initWithWindow:cocoa_wnd
#else
								  initWithWindow:uikit_wnd
#endif
								  withDevice:device
								  withHiDPI:floor::get_hidpi()
								  withVSync:floor::get_vsync()
								  withWideGamut:floor::get_wide_gamut()
								  withHDR:(floor::get_hdr() && can_do_hdr)
								  withHDRLinear:floor::get_hdr_linear()
								  withHDRMetadata:hdr_metadata];
#if !defined(FLOOR_IOS) && !defined(FLOOR_VISIONOS)
		[[cocoa_wnd contentView] addSubview:view];
#else
		[uikit_wnd addSubview:view];
#endif
		return view;
	}
}

CAMetalLayer* darwin_helper::get_metal_layer(floor_metal_view* view) {
	return [view metal_layer];
}

id <CAMetalDrawable> darwin_helper::get_metal_next_drawable(floor_metal_view* view, id <MTLCommandBuffer> cmd_buffer) {
FLOOR_PUSH_WARNINGS()
FLOOR_IGNORE_WARNING(direct-ivar-access)
	@autoreleasepool {
		while (view->max_scheduled_frames == 0) {
			// wait until woken up or 250ms timeout
			std::unique_lock<std::mutex> start_render_lock_guard(view->available_frame_cv_lock);
			if (view->available_frame_cv.wait_for(start_render_lock_guard, 250ms) == std::cv_status::timeout) {
				continue;
			}
		}
		
		// take away one frame/drawable
		--view->max_scheduled_frames;
		
		__block std::atomic<uint32_t>& max_scheduled_frames_ = view->max_scheduled_frames;
		[cmd_buffer addCompletedHandler:^(id <MTLCommandBuffer> buffer floor_unused) {
			// free up frame + signal that a new frame can be rendered again
			++max_scheduled_frames_;
			view->available_frame_cv.notify_one();
		}];
		auto drawable = [[view metal_layer] nextDrawable];
#if !TARGET_OS_SIMULATOR // in the simulator, doing this will lead to issues
		// since macOS 13.0: must manually set this to non-volatile
		[[drawable texture] setPurgeableState:MTLPurgeableStateNonVolatile];
#endif
		return drawable;
	}
FLOOR_POP_WARNINGS()
}

MTLPixelFormat darwin_helper::get_metal_pixel_format(floor_metal_view* view) {
	return [[view metal_layer] pixelFormat];
}

uint2 darwin_helper::get_metal_view_dim(floor_metal_view* view) {
	const auto& drawable_size = [view metal_layer].drawableSize;
	return { uint32_t(drawable_size.width), uint32_t(drawable_size.height) };
}

void darwin_helper::set_metal_view_hdr_metadata(floor_metal_view* view, const hdr_metadata_t& hdr_metadata) {
	[view set_hdr_metadata:hdr_metadata];
}

float darwin_helper::get_metal_view_edr_max(floor_metal_view* view) {
#if defined(FLOOR_IOS)
	return (float)[[[view window] screen] potentialEDRHeadroom];
#elif defined(FLOOR_VISIONOS)
	(void)view;
	return 1.0f; // TODO: how to query this?
#else
	return (float)[[[view window] screen] maximumExtendedDynamicRangeColorComponentValue];
#endif
}

float darwin_helper::get_metal_view_hdr_max_nits(floor_metal_view* view) {
#if !defined(FLOOR_IOS) && !defined(FLOOR_VISIONOS)
	using get_nominal_pixel_nits_func_type = float (*)(int32_t);
	static const auto get_nominal_pixel_nits = []() -> get_nominal_pixel_nits_func_type {
		static const auto core_display_lib = dlopen("/System/Library/Frameworks/CoreDisplay.framework/CoreDisplay", RTLD_NOW);
		if (!core_display_lib) {
			log_error("failed to get CoreDisplay lib");
			return nullptr;
		}
		
		static const auto get_nominal_pixel_nits_ = (get_nominal_pixel_nits_func_type)dlsym(core_display_lib, "CoreDisplay_Display_GetNominalPixelNits");
		if (!get_nominal_pixel_nits_) {
			log_error("failed to get CoreDisplay_Display_GetNominalPixelNits");
			return nullptr;
		}
		return get_nominal_pixel_nits_;
	}();
	
	const auto edr_max = get_metal_view_edr_max(view);
	if (!get_nominal_pixel_nits) {
		return 100.0f * edr_max;
	}
	const NSNumber* screen_idx = [[[[view window] screen] deviceDescription] objectForKey:@"NSScreenNumber"];
	return get_nominal_pixel_nits([screen_idx intValue]) * edr_max;
#else
	const auto edr_max = get_metal_view_edr_max(view);
	return 100.0f * edr_max;
#endif
}

uint32_t darwin_helper::get_dpi(SDL_Window* wnd floor_unused_on_ios_and_visionos) {
#if !defined(FLOOR_IOS) && !defined(FLOOR_VISIONOS) // on macOS this can be done properly through querying CoreGraphics
	float2 display_res(float2(CGDisplayPixelsWide(CGMainDisplayID()),
							  CGDisplayPixelsHigh(CGMainDisplayID())) * get_scale_factor(wnd));
	const CGSize display_phys_size(CGDisplayScreenSize(CGMainDisplayID()));
	const float2 display_dpi((display_res.x / (float)display_phys_size.width) * 25.4f,
							 (display_res.y / (float)display_phys_size.height) * 25.4f);
	return (uint32_t)floorf(std::max(display_dpi.x, display_dpi.y));
#else // on iOS there is now way of doing this properly, so it must be hardcoded for each device type
	if([[UIDevice currentDevice] userInterfaceIdiom] == UIUserInterfaceIdiomPhone) {
		// iphone / ipod touch: all supported devices have 326 dpi
		return 326;
	}
	else {
		// TODO: there seems to be a way to get the product-name via iokit (arm device -> product -> product-name)
		// ipad air/5+ or ipad mini retina
		constexpr const size_t max_machine_len { 10u };
		size_t machine_len { max_machine_len };
		char machine[max_machine_len + 1u];
		memset(machine, 0, machine_len + 1u);
		sysctlbyname("hw.machine", &machine[0], &machine_len, nullptr, 0);
		machine[max_machine_len] = 0;
		const std::string machine_str { machine };
		if(machine_str == "iPad4,4" || machine_str == "iPad4,5" || machine_str == "iPad4,6" ||
		   machine_str == "iPad4,7" || machine_str == "iPad4,8") {
			// ipad mini retina (for now ...)
			return 326;
		}
		// else: ipad air/5+ (for now ...)
		return 264;
	}
#endif
}

float darwin_helper::get_scale_factor(SDL_Window* wnd, const bool force_query) {
	auto native_wnd = get_native_window(wnd);
	if (!native_wnd) {
		return 1.0f;
	}
	
	// NOTE: this is cached and first called on a main thread -> this prevents "calling from background thread" errors
	static std::atomic<float> scale_factor;
	static std::atomic<bool> is_set { false };
	if (!is_set || force_query || window_did_resize) {
		scale_factor = (
#if defined(FLOOR_IOS)
						(float)[[native_wnd screen] scale]
#elif defined(FLOOR_VISIONOS)
						2.0f // TODO: again, how to query this?
#else
						(float)[native_wnd backingScaleFactor]
#endif
						);
		is_set = true;
		window_did_resize = false;
	}
	return scale_factor;
}

#if !defined(FLOOR_IOS) && !defined(FLOOR_VISIONOS)
float darwin_helper::get_menu_bar_height() {
	return (float)[[[NSApplication sharedApplication] mainMenu] menuBarHeight];
}
#endif

size_t darwin_helper::get_system_version() {
	// add a define for the run time macOS version
	std::string osrelease(16, 0);
	size_t size = osrelease.size() - 1;
	sysctlbyname("kern.osrelease", &osrelease[0], &size, nullptr, 0);
	osrelease.back() = 0;
	const size_t major_dot = osrelease.find(".");
	const size_t minor_dot = (major_dot != std::string::npos ? osrelease.find(".", major_dot + 1) : std::string::npos);
	if(major_dot != std::string::npos && minor_dot != std::string::npos) {
		const size_t major_version = stosize(osrelease.substr(0, major_dot));
		const size_t os_minor_version = stosize(osrelease.substr(major_dot + 1, major_dot - minor_dot - 1));
		
		// osrelease = kernel version
#if !defined(FLOOR_IOS) && !defined(FLOOR_VISIONOS)
		// * >= 26.0 (osrelease 25+): 260000 + x * 10000
		// * >= 11.0 (osrelease 20+): 110000 + x * 10000
		const size_t os_major_version = (major_version >= 25 ? major_version + 1 : 11 + (major_version - 20));
#elif defined(FLOOR_IOS)
		const size_t os_major_version = (major_version >= 25 ? major_version + 1 : major_version - 6);
#elif defined(FLOOR_VISIONOS)
		const size_t os_major_version = (major_version >= 25 ? major_version + 1 : major_version - 22);
#endif
		
		// mimic the compiled version string: xxxyyy, x = major, y = minor
		return (os_major_version * 10000 + os_minor_version * 100);
	}
	
	// just return lowest supported version
#if defined(FLOOR_IOS)
	log_error("unable to retrieve iOS version!");
	return __IPHONE_16_0;
#elif defined(FLOOR_VISIONOS)
	log_error("unable to retrieve visionOS version!");
	return __VISIONOS_2_0;
#else
	log_error("unable to retrieve macOS version!");
	return __MAC_13_0;
#endif
}

size_t darwin_helper::get_compiled_system_version() {
#if defined(FLOOR_IOS)
	// this is a number: 70000 (7.0), 60100 (6.1), ...
	return __IPHONE_OS_VERSION_MAX_ALLOWED;
#elif defined(FLOOR_VISIONOS)
	return __VISION_OS_VERSION_MAX_ALLOWED;
#else
	// this is a number: 101000 (10.10), 1090 (10.9), 1080 (10.8), ...
	return MAC_OS_X_VERSION_MAX_ALLOWED;
#endif
}

std::string darwin_helper::get_computer_name() {
#if defined(FLOOR_IOS) || defined(FLOOR_VISIONOS)
	return [[[UIDevice currentDevice] name] UTF8String];
#else
	return [[[NSHost currentHost] localizedName] UTF8String];
#endif
}

std::string darwin_helper::utf8_decomp_to_precomp(const std::string& str) {
	return [[[NSString stringWithUTF8String:str.c_str()] precomposedStringWithCanonicalMapping] UTF8String];
}

int64_t darwin_helper::get_memory_size() {
	int64_t mem_size { 0 };
	static int sysctl_cmd[2] { CTL_HW, HW_MEMSIZE };
	static size_t size = sizeof(mem_size);
	sysctl(&sysctl_cmd[0], 2, &mem_size, &size, nullptr, 0);
	return mem_size;
}

std::string darwin_helper::get_bundle_identifier() {
	NSString* bundle_id = [[NSBundle mainBundle] objectForInfoDictionaryKey:@"CFBundleIdentifier"];
	return [bundle_id UTF8String];
}

std::string darwin_helper::get_pref_path() {
	// get the bundle identifier of this .app and get the preferences path from sdl
	const auto bundle_id = darwin_helper::get_bundle_identifier();
	const auto bundle_dot = bundle_id.rfind('.');
	std::string ret { "" };
	if(bundle_dot != std::string::npos) {
		char* pref_path = SDL_GetPrefPath(bundle_id.substr(0, bundle_dot).c_str(),
										  bundle_id.substr(bundle_dot + 1, bundle_id.size() - bundle_dot - 1).c_str());
		if(pref_path != nullptr) {
			ret = pref_path;
			SDL_free(pref_path);
		}
	}
	return ret;
}

bool darwin_helper::is_running_in_debugger() {
#if !defined(FLOOR_DEBUG)
	return false;
#else
	static const auto in_debugger = []() {
		kinfo_proc info { .kp_proc.p_flag = 0 };
		int mib[] { CTL_KERN, KERN_PROC, KERN_PROC_PID, getpid() };
		auto info_size = sizeof(info);
		if (sysctl(mib, std::size(mib), &info, &info_size, nullptr, 0) != 0) {
			return false;
		}
		return ((info.kp_proc.p_flag & P_TRACED) != 0);
	}();
	return in_debugger;
#endif
}

bool darwin_helper::sdl_poll_event_wrapper(SDL_Event& event_handle) {
	@autoreleasepool {
		return SDL_PollEvent(&event_handle);
	}
}

} // namespace fl
