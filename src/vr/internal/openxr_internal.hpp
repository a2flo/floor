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

#include <floor/core/essentials.hpp>

#if !defined(FLOOR_NO_OPENXR) && !defined(FLOOR_NO_VULKAN)

namespace fl {

struct openxr_context_internal {
	std::vector<XrReferenceSpaceType> spaces;
	
	std::vector<XrViewConfigurationView> view_configs;
	std::vector<XrView> views;
	
	struct multi_layer_swapchain_t {
		XrSwapchain swapchain{ nullptr };
		std::vector<std::unique_ptr<device_image>> swapchain_images;
		std::vector<XrSwapchainImageVulkan2KHR> swapchain_vk_images;
		std::vector<VkImageView> swapchain_vk_image_views;
	};
	multi_layer_swapchain_t swapchain {};
	
	XrFrameState cur_frame_state {};
	std::array<XrCompositionLayerProjectionView, 2> cur_layer_projection_views {{}};
	XrCompositionLayerProjection cur_layer_projection {};
	device_image* cur_swapchain_image { nullptr };
	
	PFN_xrCreateVulkanInstanceKHR CreateVulkanInstanceKHR { nullptr };
	PFN_xrCreateVulkanDeviceKHR CreateVulkanDeviceKHR { nullptr };
	PFN_xrGetVulkanGraphicsDevice2KHR GetVulkanGraphicsDevice2KHR { nullptr };
	PFN_xrGetVulkanGraphicsRequirements2KHR GetVulkanGraphicsRequirements2KHR { nullptr };
	
	bool can_query_display_refresh_rate { false };
	PFN_xrGetDisplayRefreshRateFB GetDisplayRefreshRateFB { nullptr };
	
	PFN_xrEnumerateViveTrackerPathsHTCX EnumerateViveTrackerPaths { nullptr };
	
	PFN_xrCreateHandTrackerEXT CreateHandTracker { nullptr };
	PFN_xrDestroyHandTrackerEXT DestroyHandTracker { nullptr };
	PFN_xrLocateHandJointsEXT LocateHandJoints { nullptr };
	
#if defined(__WINDOWS__)
	uint64_t win_start_perf_counter { 0u };
	uint64_t win_perf_counter_freq { 0u };
	
	// NOTE: using int64_t instead of LARGE_INTEGER here, because we don't want to include Windows headers
	using xrConvertWin32PerformanceCounterToTimeKHR_f = XrResult (XRAPI_PTR*)(XrInstance instance,
																			  const int64_t* performanceCounter,
																			  XrTime* time);
	using xrConvertTimeToWin32PerformanceCounterKHR_f = XrResult (XRAPI_PTR*)(XrInstance instance,
																			  XrTime time,
																			  int64_t* performanceCounter);
	xrConvertWin32PerformanceCounterToTimeKHR_f ConvertWin32PerformanceCounterToTimeKHR { nullptr };
	xrConvertTimeToWin32PerformanceCounterKHR_f ConvertTimeToWin32PerformanceCounterKHR { nullptr };
#endif
	
#if defined(__linux__)
	//! always nanoseconds
	static constexpr const uint64_t unix_perf_counter_freq { 1'000'000'000u };
	uint64_t unix_start_time { 0u }; //! ns
	
	using xrConvertTimespecTimeToTimeKHR_f = XrResult (XRAPI_PTR*)(XrInstance instance,
																   const struct timespec* timespecTime,
																   XrTime* time);
	using xrConvertTimeToTimespecTimeKHR_f = XrResult (XRAPI_PTR*)(XrInstance instance,
																   XrTime time,
																   struct timespec* timespecTime);
	xrConvertTimespecTimeToTimeKHR_f ConvertTimespecTimeToTimeKHR { nullptr };
	xrConvertTimeToTimespecTimeKHR_f ConvertTimeToTimespecTimeKHR { nullptr };
#endif
	
#if defined(FLOOR_DEBUG)
	PFN_xrCreateDebugUtilsMessengerEXT create_debug_utils_messenger { nullptr };
	PFN_xrDestroyDebugUtilsMessengerEXT destroy_debug_utils_messenger { nullptr };
	XrDebugUtilsMessengerEXT debug_utils_messenger { nullptr };
#endif
};

} // namespace fl

#endif
