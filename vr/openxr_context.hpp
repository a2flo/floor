/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2023 Florian Ziesche
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

#ifndef __FLOOR_VR_OPENXR_CONTEXT_HPP__
#define __FLOOR_VR_OPENXR_CONTEXT_HPP__

#include <floor/core/essentials.hpp>

// NOTE: this requires both VR and Vulkan support (no point in anything else)
#if !defined(FLOOR_NO_OPENXR) && !defined(FLOOR_NO_VULKAN)
#include <floor/vr/vr_context.hpp>
#include <floor/compute/vulkan/vulkan_common.hpp>
#include <floor/math/quaternion.hpp>
#include <floor/threading/atomic_spin_lock.hpp>
#include <floor/vr/openxr_common.hpp>

class vulkan_compute;
class vulkan_device;
class vulkan_queue;

class openxr_context : public vr_context {
public:
	openxr_context();
	~openxr_context() override;

	//! Vulkan instance creation must be done via the OpenXR wrapper
	VkResult create_vulkan_instance(const VkInstanceCreateInfo& vk_create_info, VkInstance& vk_instance);

	//! Vulkan device creation must be done via the OpenXR wrapper
	VkResult create_vulkan_device(const VkDeviceCreateInfo& vk_create_info, VkDevice& vk_dev,
								  const VkPhysicalDevice& vk_phys_dev, VkInstance& vk_instance);

	//! creates the OpenXR session, using the specified Vulkan context
	bool create_session(vulkan_compute& vk_ctx, const vulkan_device& vk_dev, const vulkan_queue& vk_queue);
	
	string get_vulkan_instance_extensions() const override;
	string get_vulkan_device_extensions(VkPhysicalDevice_T* physical_device) const override;
	
	vector<shared_ptr<event_object>> handle_input() override;

	bool ignore_vulkan_validation() const override {
		return !is_known_good_vulkan_backend;
	}

	bool has_swapchain() const override {
		return true;
	}

	swapchain_info_t get_swapchain_info() const override;

	compute_image* acquire_next_image() override;
	
	bool present(const compute_queue& cqueue, const compute_image* image) override;
	
	frame_view_state_t get_frame_view_state(const float& z_near, const float& z_far,
											const bool with_position_in_mvm) const override;

	vector<pose_t> get_pose_state() const override;
	
protected:
	XrInstance instance { nullptr };
	XrSystemId system_id {};
	XrSession session { nullptr };

	// view and rendering handling
	vulkan_compute* vk_ctx { nullptr };
	const vulkan_device* vk_dev { nullptr };
	bool is_known_good_vulkan_backend { false };

	uint32_t swapchain_layer_count { 0u };
	vector<XrReferenceSpaceType> spaces;
	XrSpace scene_space { nullptr };
	XrSpace view_space { nullptr };
	bool mutable_fov { false };
	vector<XrViewConfigurationView> view_configs;
	vector<XrView> views;

	struct multi_layer_swapchain_t {
		XrSwapchain swapchain{ nullptr };
		vector<unique_ptr<compute_image>> swapchain_images;
		vector<XrSwapchainImageVulkan2KHR> swapchain_vk_images;
		vector<VkImageView> swapchain_vk_image_views;
	};
	multi_layer_swapchain_t swapchain {};

	XrFrameState cur_frame_state {};
	array<XrCompositionLayerProjectionView, 2> cur_layer_projection_views {{}};
	XrCompositionLayerProjection cur_layer_projection {};
	compute_image* cur_swapchain_image { nullptr };

	struct view_state_t {
		float3 position;
		quaternionf orientation;
		float4 fov;
	};
	struct hmd_view_state_t {
		float3 position;
		quaternionf orientation;
	};
	mutable atomic_spin_lock view_states_lock;
	vector<view_state_t> view_states GUARDED_BY(view_states_lock);
	hmd_view_state_t hmd_view_state GUARDED_BY(view_states_lock);

	PFN_xrCreateVulkanInstanceKHR CreateVulkanInstanceKHR { nullptr };
	PFN_xrCreateVulkanDeviceKHR CreateVulkanDeviceKHR { nullptr };
	PFN_xrGetVulkanGraphicsDevice2KHR GetVulkanGraphicsDevice2KHR { nullptr };
	PFN_xrGetVulkanGraphicsRequirements2KHR GetVulkanGraphicsRequirements2KHR { nullptr };

	bool can_query_display_refresh_rate { false };
	PFN_xrGetDisplayRefreshRateFB GetDisplayRefreshRateFB { nullptr };

	// input handling
	bool input_setup();
	bool handle_input_internal(vector<shared_ptr<event_object>>& events);
	void add_hand_bool_event(vector<shared_ptr<event_object>>& events, const EVENT_TYPE event_type,
							 const XrActionStateBoolean& state, const bool side);
	void add_hand_float_event(vector<shared_ptr<event_object>>& events, const EVENT_TYPE event_type,
							  const XrActionStateFloat& state, const bool side);
	void add_hand_float2_event(vector<shared_ptr<event_object>>& events, const EVENT_TYPE event_type,
							   const XrActionStateVector2f& state, const bool side);

	atomic<bool> is_focused { false };

	XrActionSet input_action_set { nullptr };

	enum class INPUT_TYPE : uint32_t {
		NONE = 0u,
		HAND_LEFT = (1u << 0u),
		HAND_RIGHT = (1u << 1u),
		HEAD = (1u << 2u),
		GAMEPAD = (1u << 3u),
		MAX_INPUT_TYPE = (1u << 4u),
	};
	floor_enum_ext(INPUT_TYPE)

	array<XrPath, 4> input_paths;

	enum class ACTION_TYPE : uint32_t {
		//! inputs
		BOOLEAN,
		FLOAT,
		FLOAT2,
		POSE,
		//! haptic / vibration output
		HAPTIC,
	};

	struct action_t {
		XrAction action { nullptr };
		INPUT_TYPE input_type { INPUT_TYPE::NONE };
		ACTION_TYPE action_type { ACTION_TYPE::BOOLEAN };
	};

	//! base action mapped to all possible VR event types
	unordered_map<EVENT_TYPE, action_t> base_actions;

	//! hand vars
	array<XrAction, 2> hand_pose_actions { nullptr, nullptr };
	array<XrSpace, 2> hand_spaces { nullptr, nullptr };
	array<XrAction, 2> hand_aim_pose_actions { nullptr, nullptr };
	array<XrSpace, 2> hand_aim_spaces { nullptr, nullptr };
	array<XrPath, 2> hand_paths { 0u, 0u };

	//! previous hand event states
	struct event_state_t {
		union {
			float f;
			float2 f2 { 0.0f, 0.0f };
		};
	};
	array<unordered_map<EVENT_TYPE, event_state_t>, 2> hand_event_states;

	//! access to "pose_state" must be thread-safe
	mutable atomic_spin_lock pose_state_lock;
	//! current pose state
	vector<pose_t> pose_state GUARDED_BY(pose_state_lock);
	//! size(pose_state) of the last update (helps with allocation)
	size_t prev_pose_state_size { 0u };

	//! input event emulation
	static constexpr const float emulation_trigger_force { 0.95f };
	struct input_event_emulation_t {
		union {
			//! via VR_GRIP_FORCE with force >= 0.95f
			uint32_t grip_press : 1;
			//! via VR_TRACKPAD_FORCE with force >= 0.95f
			uint32_t trackpad_press : 1;
			//! via VR_TRIGGER_PULL with force >= 0.95f
			uint32_t trigger_press : 1;
			//! via VR_GRIP_PULL when state was 0 and changed to > 0
			uint32_t grip_touch : 1;

			uint32_t unused : 28;
		};
		uint32_t data { 0u };
	} emulate;

	// non-core controller support flags
	bool has_hp_mixed_reality_controller_support { false };
	bool has_htc_vive_cosmos_controller_support { false };
	bool has_htc_vive_focus3_controller_support { false };
	bool has_huawei_controller_support { false };
	bool has_samsung_odyssey_controller_support { false };
	bool has_ml2_controller_support { false };
	bool has_fb_touch_controller_pro_support { false };
	bool has_bd_controller_support { false };

	//! converts "str" to an XrPath, or returns empty on failure
	optional<XrPath> to_path(const std::string& str);
	//! converts "str" to an XrPath, or returns empty on failure
	optional<XrPath> to_path(const char* str);
	//! converts "str" to an XrPath, throws on failure
	XrPath to_path_or_throw(const std::string& str);
	//! converts "str" to an XrPath, throws on failure
	XrPath to_path_or_throw(const char* str);

	//! converts the OpenXR time into SDL ticks that we need for event handling
	uint64_t convert_time_to_ticks(XrTime time);
	//! converts the SDL performance counter value to an OpenXR time
	XrTime convert_perf_counter_to_time(const uint64_t perf_counter);

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
	
};

#endif

#endif
