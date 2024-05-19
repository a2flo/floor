/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2024 Florian Ziesche
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

#include <floor/vr/openvr_context.hpp>

#if !defined(FLOOR_NO_OPENVR)
#include <floor/floor/floor.hpp>
#include <floor/core/logger.hpp>
#include <floor/core/core.hpp>
#include <floor/compute/compute_queue.hpp>

#if !defined(MINGW)
#include <openvr.h>
#else
#include <openvr_mingw.hpp>
#endif

#if !defined(FLOOR_NO_VULKAN)
#include <floor/compute/vulkan/vulkan_compute.hpp>
#include <floor/compute/vulkan/vulkan_queue.hpp>
#include <floor/compute/vulkan/vulkan_image.hpp>
#include <floor/compute/vulkan/vulkan_device.hpp>
#endif

#if !defined(FLOOR_NO_METAL)
#include <floor/compute/metal/metal_compute.hpp>
#include <floor/compute/metal/metal_device.hpp>
#include <floor/compute/metal/metal_image.hpp>
#endif

static_assert(openvr_context::max_tracked_devices == vr::k_unMaxTrackedDeviceCount);

static constexpr inline vr::EVREye eye_to_eye(const VR_EYE& eye) {
	return (eye == VR_EYE::LEFT ? vr::EVREye::Eye_Left : vr::EVREye::Eye_Right);
}

template <typename property_type>
static property_type get_vr_property(vr::IVRSystem& system,
									 const vr::ETrackedDeviceProperty& prop,
									 const vr::TrackedDeviceIndex_t idx = 0) {
	if constexpr (is_same_v<property_type, string>) {
		vr::ETrackedPropertyError err = vr::TrackedProp_Success;
		const auto len = system.GetStringTrackedDeviceProperty(idx, prop, nullptr, 0, &err);
		if (err != vr::TrackedProp_Success && err != vr::TrackedProp_BufferTooSmall) {
			log_error("OpenVR: failed to read string property $: $", prop, err);
			return {};
		}
		if (len == 0) {
			return {};
		}
		string str(len, '\0');
		system.GetStringTrackedDeviceProperty(idx, prop, str.data(), len, &err);
		if (err != vr::TrackedProp_Success) {
			log_error("OpenVR: failed to read string property $: $", prop, err);
			return {};
		}
		return core::trim(str);
	} else if constexpr (is_same_v<property_type, bool>) {
		return system.GetBoolTrackedDeviceProperty(idx, prop, nullptr);
	} else if constexpr (is_same_v<property_type, float>) {
		return system.GetFloatTrackedDeviceProperty(idx, prop, nullptr);
	} else if constexpr (is_same_v<property_type, int32_t>) {
		return system.GetInt32TrackedDeviceProperty(idx, prop, nullptr);
	} else if constexpr (is_same_v<property_type, uint64_t>) {
		return system.GetUint64TrackedDeviceProperty(idx, prop, nullptr);
	} else {
		instantiation_trap_dependent_type(property_type, "not implemented yet or invalid type");
	}
}

//! tries to match the specified tracker name/type against a known tracker POSE_TYPE
static optional<vr_context::POSE_TYPE> tracker_type_from_name(const string& name) {
	if (name.ends_with("handed")) {
		return vr_context::POSE_TYPE::TRACKER_HANDHELD_OBJECT;
	} else if (name.ends_with("left_foot")) {
		return vr_context::POSE_TYPE::TRACKER_FOOT_LEFT;
	} else if (name.ends_with("right_foot")) {
		return vr_context::POSE_TYPE::TRACKER_FOOT_RIGHT;
	} else if (name.ends_with("left_shoulder")) {
		return vr_context::POSE_TYPE::TRACKER_SHOULDER_LEFT;
	} else if (name.ends_with("right_shoulder")) {
		return vr_context::POSE_TYPE::TRACKER_SHOULDER_RIGHT;
	} else if (name.ends_with("left_elbow")) {
		return vr_context::POSE_TYPE::TRACKER_ELBOW_LEFT;
	} else if (name.ends_with("right_elbow")) {
		return vr_context::POSE_TYPE::TRACKER_ELBOW_RIGHT;
	} else if (name.ends_with("left_knee")) {
		return vr_context::POSE_TYPE::TRACKER_KNEE_LEFT;
	} else if (name.ends_with("right_knee")) {
		return vr_context::POSE_TYPE::TRACKER_KNEE_RIGHT;
	} else if (name.ends_with("waist")) {
		return vr_context::POSE_TYPE::TRACKER_WAIST;
	} else if (name.ends_with("chest")) {
		return vr_context::POSE_TYPE::TRACKER_CHEST;
	} else if (name.ends_with("camera")) {
		return vr_context::POSE_TYPE::TRACKER_CAMERA;
	} else if (name.ends_with("keyboard")) {
		return vr_context::POSE_TYPE::TRACKER_KEYBOARD;
	} else if (name.ends_with("left_wrist")) {
		return vr_context::POSE_TYPE::TRACKER_WRIST_LEFT;
	} else if (name.ends_with("right_wrist")) {
		return vr_context::POSE_TYPE::TRACKER_WRIST_RIGHT;
	} else if (name.ends_with("left_ankle")) {
		return vr_context::POSE_TYPE::TRACKER_ANKLE_LEFT;
	} else if (name.ends_with("right_ankle")) {
		return vr_context::POSE_TYPE::TRACKER_ANKLE_RIGHT;
	}
	return {};
}

//! OpenVR hand skeleton/bone indices are fixed
//! ref: https://github.com/ValveSoftware/openvr/wiki/Hand-Skeleton
enum class BONE : uint32_t {
	//! ROOT -> PINKY_FINGER_4 match the OpenXR and POSE_TYPE order (HAND_JOINT_PALM_LEFT -> HAND_JOINT_LITTLE_TIP_LEFT)
	ROOT,
	WRIST,
	THUMB_0,
	THUMB_1,
	THUMB_2,
	THUMB_3,
	INDEX_FINGER_0,
	INDEX_FINGER_1,
	INDEX_FINGER_2,
	INDEX_FINGER_3,
	INDEX_FINGER_4,
	MIDDLE_FINGER_0,
	MIDDLE_FINGER_1,
	MIDDLE_FINGER_2,
	MIDDLE_FINGER_3,
	MIDDLE_FINGER_4,
	RING_FINGER_0,
	RING_FINGER_1,
	RING_FINGER_2,
	RING_FINGER_3,
	RING_FINGER_4,
	PINKY_FINGER_0,
	PINKY_FINGER_1,
	PINKY_FINGER_2,
	PINKY_FINGER_3,
	PINKY_FINGER_4,

	//! additional bones that we will ignore
	AUX_THUMB,
	AUX_INDEX_FINGER,
	AUX_MIDDLE_FINGER,
	AUX_RING_FINGER,
	AUX_PINKY_FINGER,
};
static_assert(uint32_t(BONE::PINKY_FINGER_4) + 1u == openvr_context::handled_bone_count);

openvr_context::openvr_context() : vr_context() {
	backend = VR_BACKEND::OPENVR;
	
	// check pre-conditions
	if (!vr::VR_IsHmdPresent()) {
		log_error("no HMD present");
		return;
	}
	if (!vr::VR_IsRuntimeInstalled()) {
		log_error("no VR runtime installed");
		return;
	}

	// init VR
	vr::EVRInitError err { vr::EVRInitError::VRInitError_None };
	system = vr::VR_Init(&err, vr::EVRApplicationType::VRApplication_Scene, nullptr);
	if (system == nullptr) {
		log_error("failed to initialize VR: $", VR_GetVRInitErrorAsEnglishDescription(err));
		return;
	}

	ctx = make_unique<vr::COpenVRContext>();
	compositor = ctx->VRCompositor();
	if (compositor == nullptr) {
		log_error("no VR compositor");
		return;
	}
	input = ctx->VRInput();
	if (input == nullptr) {
		log_error("no VR input");
		return;
	}

	hmd_name = get_vr_property<string>(*system, vr::Prop_ModelNumber_String);
	vendor_name = get_vr_property<string>(*system, vr::Prop_ManufacturerName_String);
	display_frequency = get_vr_property<float>(*system, vr::Prop_DisplayFrequency_Float);
	if (display_frequency <= 0.0f) {
		display_frequency = -1.0f;
	}
	system->GetRecommendedRenderTargetSize(&recommended_render_size.x, &recommended_render_size.y);
	log_debug("VR HMD: $ ($) -> $*$ @$Hz", hmd_name, vendor_name,
			  recommended_render_size.x, recommended_render_size.y, display_frequency);

	has_hand_tracking_support = floor::get_vr_hand_tracking();

	// input setup
	const auto vr_action_manifest_file = floor::data_path("vr_action_manifest.json");
	if (file_io::is_file(vr_action_manifest_file)) {
		const auto input_err = input->SetActionManifestPath(vr_action_manifest_file.c_str());
		if (input_err != vr::EVRInputError::VRInputError_None) {
			log_error("VR: failed to set action manifest: $", input_err);
			return;
		}

		const auto action_set_err = input->GetActionSetHandle("/actions/main", &main_action_set);
		if (action_set_err != vr::EVRInputError::VRInputError_None || main_action_set == vr::k_ulInvalidActionSetHandle) {
			log_error("VR: failed to get main action set: $", action_set_err);
			return;
		}

		actions = {
			{ "/actions/main/in/left_applicationmenu_press", { ACTION_TYPE::DIGITAL, false, EVENT_TYPE::VR_APP_MENU_PRESS, 0 } },
			{ "/actions/main/in/left_applicationmenu_touch", { ACTION_TYPE::DIGITAL, false, EVENT_TYPE::VR_APP_MENU_TOUCH, 0 } },
			{ "/actions/main/in/left_main_button_press", { ACTION_TYPE::DIGITAL, false, EVENT_TYPE::VR_MAIN_PRESS, 0 } },
			{ "/actions/main/in/left_main_button_touch", { ACTION_TYPE::DIGITAL, false, EVENT_TYPE::VR_MAIN_TOUCH, 0 } },
			{ "/actions/main/in/left_system_press", { ACTION_TYPE::DIGITAL, false, EVENT_TYPE::VR_SYSTEM_PRESS, 0 } },
			{ "/actions/main/in/left_system_touch", { ACTION_TYPE::DIGITAL, false, EVENT_TYPE::VR_SYSTEM_TOUCH, 0 } },
			{ "/actions/main/in/left_trackpad_press", { ACTION_TYPE::DIGITAL, false, EVENT_TYPE::VR_TRACKPAD_PRESS, 0 } },
			{ "/actions/main/in/left_trackpad_touch", { ACTION_TYPE::DIGITAL, false, EVENT_TYPE::VR_TRACKPAD_TOUCH, 0 } },
			{ "/actions/main/in/left_trackpad_value", { ACTION_TYPE::ANALOG, false, EVENT_TYPE::VR_TRACKPAD_MOVE, 0 } },
			{ "/actions/main/in/left_trackpad_force", { ACTION_TYPE::ANALOG, false, EVENT_TYPE::VR_TRACKPAD_FORCE, 0 } },
			{ "/actions/main/in/left_trigger_press", { ACTION_TYPE::DIGITAL, false, EVENT_TYPE::VR_TRIGGER_PRESS, 0 } },
			{ "/actions/main/in/left_trigger_touch", { ACTION_TYPE::DIGITAL, false, EVENT_TYPE::VR_TRIGGER_TOUCH, 0 } },
			{ "/actions/main/in/left_trigger_pull", { ACTION_TYPE::ANALOG, false, EVENT_TYPE::VR_TRIGGER_PULL, 0 } },
			{ "/actions/main/in/left_thumbstick_press", { ACTION_TYPE::DIGITAL, false, EVENT_TYPE::VR_THUMBSTICK_PRESS, 0 } },
			{ "/actions/main/in/left_thumbstick_touch", { ACTION_TYPE::DIGITAL, false, EVENT_TYPE::VR_THUMBSTICK_TOUCH, 0 } },
			{ "/actions/main/in/left_thumbstick_value", { ACTION_TYPE::ANALOG, false, EVENT_TYPE::VR_THUMBSTICK_MOVE, 0 } },
			{ "/actions/main/in/left_grip_press", { ACTION_TYPE::DIGITAL, false, EVENT_TYPE::VR_GRIP_PRESS, 0 } },
			{ "/actions/main/in/left_grip_touch", { ACTION_TYPE::DIGITAL, false, EVENT_TYPE::VR_GRIP_TOUCH, 0 } },
			{ "/actions/main/in/left_grip_force", { ACTION_TYPE::ANALOG, false, EVENT_TYPE::VR_GRIP_FORCE, 0 } },
			{ "/actions/main/in/left_grip_pull", { ACTION_TYPE::ANALOG, false, EVENT_TYPE::VR_GRIP_PULL, 0 } },
			{ "/actions/main/in/left_pose", { ACTION_TYPE::POSE, false, EVENT_TYPE::VR_INTERNAL_HAND_POSE_LEFT, 0 } },
			{ "/actions/main/in/left_pose_aim", { ACTION_TYPE::POSE, false, EVENT_TYPE::VR_INTERNAL_HAND_AIM_LEFT, 0 } },
			{ "/actions/main/out/left_haptic", { ACTION_TYPE::HAPTIC, false, EVENT_TYPE::__VR_CONTROLLER_EVENT, 0 } }, // TODO
			{ "/actions/main/in/right_applicationmenu_press", { ACTION_TYPE::DIGITAL, true, EVENT_TYPE::VR_APP_MENU_PRESS, 0 } },
			{ "/actions/main/in/right_applicationmenu_touch", { ACTION_TYPE::DIGITAL, true, EVENT_TYPE::VR_APP_MENU_TOUCH, 0 } },
			{ "/actions/main/in/right_main_button_press", { ACTION_TYPE::DIGITAL, true, EVENT_TYPE::VR_MAIN_PRESS, 0 } },
			{ "/actions/main/in/right_main_button_touch", { ACTION_TYPE::DIGITAL, true, EVENT_TYPE::VR_MAIN_TOUCH, 0 } },
			{ "/actions/main/in/right_system_press", { ACTION_TYPE::DIGITAL, true, EVENT_TYPE::VR_SYSTEM_PRESS, 0 } },
			{ "/actions/main/in/right_system_touch", { ACTION_TYPE::DIGITAL, true, EVENT_TYPE::VR_SYSTEM_TOUCH, 0 } },
			{ "/actions/main/in/right_trackpad_press", { ACTION_TYPE::DIGITAL, true, EVENT_TYPE::VR_TRACKPAD_PRESS, 0 } },
			{ "/actions/main/in/right_trackpad_touch", { ACTION_TYPE::DIGITAL, true, EVENT_TYPE::VR_TRACKPAD_TOUCH, 0 } },
			{ "/actions/main/in/right_trackpad_value", { ACTION_TYPE::ANALOG, true, EVENT_TYPE::VR_TRACKPAD_MOVE, 0 } },
			{ "/actions/main/in/right_trackpad_force", { ACTION_TYPE::ANALOG, true, EVENT_TYPE::VR_TRACKPAD_FORCE, 0 } },
			{ "/actions/main/in/right_trigger_press", { ACTION_TYPE::DIGITAL, true, EVENT_TYPE::VR_TRIGGER_PRESS, 0 } },
			{ "/actions/main/in/right_trigger_touch", { ACTION_TYPE::DIGITAL, true, EVENT_TYPE::VR_TRIGGER_TOUCH, 0 } },
			{ "/actions/main/in/right_trigger_pull", { ACTION_TYPE::ANALOG, true, EVENT_TYPE::VR_TRIGGER_PULL, 0 } },
			{ "/actions/main/in/right_thumbstick_press", { ACTION_TYPE::DIGITAL, true, EVENT_TYPE::VR_THUMBSTICK_PRESS, 0 } },
			{ "/actions/main/in/right_thumbstick_touch", { ACTION_TYPE::DIGITAL, true, EVENT_TYPE::VR_THUMBSTICK_TOUCH, 0 } },
			{ "/actions/main/in/right_thumbstick_value", { ACTION_TYPE::ANALOG, true, EVENT_TYPE::VR_THUMBSTICK_MOVE, 0 } },
			{ "/actions/main/in/right_grip_press", { ACTION_TYPE::DIGITAL, true, EVENT_TYPE::VR_GRIP_PRESS, 0 } },
			{ "/actions/main/in/right_grip_touch", { ACTION_TYPE::DIGITAL, true, EVENT_TYPE::VR_GRIP_TOUCH, 0 } },
			{ "/actions/main/in/right_grip_force", { ACTION_TYPE::ANALOG, true, EVENT_TYPE::VR_GRIP_FORCE, 0 } },
			{ "/actions/main/in/right_grip_pull", { ACTION_TYPE::ANALOG, true, EVENT_TYPE::VR_GRIP_PULL, 0 } },
			{ "/actions/main/in/right_pose", { ACTION_TYPE::POSE, true, EVENT_TYPE::VR_INTERNAL_HAND_POSE_RIGHT, 0 } },
			{ "/actions/main/in/right_pose_aim", { ACTION_TYPE::POSE, true, EVENT_TYPE::VR_INTERNAL_HAND_AIM_RIGHT, 0 } },
			{ "/actions/main/out/right_haptic", { ACTION_TYPE::HAPTIC, true, EVENT_TYPE::__VR_CONTROLLER_EVENT, 0 } }, // TODO
		};
		if (has_hand_tracking_support) {
			actions.emplace("/actions/main/in/left_hand_skeleton", action_t { ACTION_TYPE::SKELETAL, false, EVENT_TYPE::__VR_CONTROLLER_EVENT, 0 });
			actions.emplace("/actions/main/in/right_hand_skeleton", action_t { ACTION_TYPE::SKELETAL, true, EVENT_TYPE::__VR_CONTROLLER_EVENT, 0 });
		}
		for (auto& action : actions) {
			const auto handle_err = input->GetActionHandle(action.first.c_str(), &action.second.handle);
			if (handle_err != vr::EVRInputError::VRInputError_None || action.second.handle == vr::k_ulInvalidActionHandle) {
				log_error("VR: failed to get action handle $: $", action.first, handle_err);
				return;
			}
		}

		while (has_hand_tracking_support) {
			const auto left_hand_skel_iter = actions.find("/actions/main/in/left_hand_skeleton");
			const auto right_hand_skel_iter = actions.find("/actions/main/in/right_hand_skeleton");
			if (left_hand_skel_iter == actions.end() || right_hand_skel_iter == actions.end()) {
				has_hand_tracking_support = false;
				log_warn("left/right hand skeleton action not found - disabling hand-tracking support");
				break;
			}

			uint32_t lh_bone_count = 0, rh_bone_count = 0;
			if (input->GetBoneCount(left_hand_skel_iter->second.handle, &lh_bone_count) != vr::EVRInputError::VRInputError_None ||
				input->GetBoneCount(right_hand_skel_iter->second.handle, &rh_bone_count) != vr::EVRInputError::VRInputError_None) {
				log_warn("failed to retrieve left/right bone counts");
				break;
			}

			// if not properly initialized yet, the returned bone count will be zero (-> don't disable yet)
			if ((lh_bone_count > 0 && lh_bone_count != expected_bone_count) ||
				(rh_bone_count > 0 && rh_bone_count != expected_bone_count)) {
				has_hand_tracking_support = false;
				log_warn("invalid left/right skeleton bones ($, $) - disabling hand-tracking support",
						 lh_bone_count, rh_bone_count);
			}
			break;
		}
	}

	// initial setup / get initial poses / tracked devices
	device_type_map.fill(POSE_TYPE::UNKNOWN);
	openvr_context::handle_input();
	// must call this after the initial setup (fills necessary device_type_map)
	update_hand_controller_types();

	// all done
	valid = true;
}

openvr_context::~openvr_context() {
	if (system == nullptr) {
		return;
	}
	vr::VR_Shutdown();
}

string openvr_context::get_vulkan_instance_extensions() const {
	if (compositor == nullptr) {
		return {};
	}

	const auto vk_instance_exts_len = compositor->GetVulkanInstanceExtensionsRequired(nullptr, 0);
	if (vk_instance_exts_len == 0) {
		return {};
	}

	string vk_instance_exts(vk_instance_exts_len, '\0');
	compositor->GetVulkanInstanceExtensionsRequired(vk_instance_exts.data(), vk_instance_exts_len);
	return vk_instance_exts;
}

string openvr_context::get_vulkan_device_extensions(VkPhysicalDevice_T* physical_device) const {
	if (compositor == nullptr) {
		return {};
	}

	const auto vk_dev_exts_len = compositor->GetVulkanDeviceExtensionsRequired(physical_device, nullptr, 0);
	if (vk_dev_exts_len == 0) {
		return {};
	}

	string vk_dev_exts(vk_dev_exts_len, '\0');
	compositor->GetVulkanDeviceExtensionsRequired(physical_device, vk_dev_exts.data(), vk_dev_exts_len);
	return vk_dev_exts;
}

vector<shared_ptr<event_object>> openvr_context::handle_input() REQUIRES(!pose_state_lock) {
	vector<shared_ptr<event_object>> events;

	// action event/input handling
	vr::VRActiveActionSet_t active_action_set {
		.ulActionSet = main_action_set,
		.ulRestrictedToDevice = vr::k_ulInvalidInputValueHandle,
		.ulSecondaryActionSet = vr::k_ulInvalidActionSetHandle,
		.unPadding = 0,
		.nPriority = 0,
	};
	array<array<pose_t, handled_bone_count>, 2> hand_bone_poses {};

	bool2 hand_bone_poses_valid { false, false };
	const auto update_act_err = input->UpdateActionState(&active_action_set, sizeof(vr::VRActiveActionSet_t), 1);
	if (update_act_err != vr::EVRInputError::VRInputError_None) {
		log_error("failed to update action state: $", update_act_err);
	} else {
		const auto cur_time = SDL_GetTicks();
		for (auto& action : actions) {
			vr::EVRInputError err = vr::EVRInputError::VRInputError_None;
			switch (action.second.type) {
				case ACTION_TYPE::DIGITAL: {
					vr::InputDigitalActionData_t data{};
					err = input->GetDigitalActionData(action.second.handle, &data, sizeof(vr::InputDigitalActionData_t),
													  vr::k_ulInvalidInputValueHandle);
					if (err != vr::EVRInputError::VRInputError_None) {
						break;
					}
					if (data.bActive && data.bChanged) {
						switch (action.second.event_type) {
							case EVENT_TYPE::VR_APP_MENU_PRESS:
								events.emplace_back(
									make_shared<vr_app_menu_press_event>(cur_time, action.second.side, data.bState));
								break;
							case EVENT_TYPE::VR_APP_MENU_TOUCH:
								events.emplace_back(
									make_shared<vr_app_menu_touch_event>(cur_time, action.second.side, data.bState));
								break;
							case EVENT_TYPE::VR_MAIN_PRESS:
								events.emplace_back(
									make_shared<vr_main_press_event>(cur_time, action.second.side, data.bState));
								break;
							case EVENT_TYPE::VR_MAIN_TOUCH:
								events.emplace_back(
									make_shared<vr_main_touch_event>(cur_time, action.second.side, data.bState));
								break;
							case EVENT_TYPE::VR_SYSTEM_PRESS:
								events.emplace_back(
									make_shared<vr_system_press_event>(cur_time, action.second.side, data.bState));
								break;
							case EVENT_TYPE::VR_SYSTEM_TOUCH:
								events.emplace_back(
									make_shared<vr_system_touch_event>(cur_time, action.second.side, data.bState));
								break;
							case EVENT_TYPE::VR_TRACKPAD_PRESS:
								events.emplace_back(
									make_shared<vr_trackpad_press_event>(cur_time, action.second.side, data.bState));
								break;
							case EVENT_TYPE::VR_TRACKPAD_TOUCH:
								events.emplace_back(
									make_shared<vr_trackpad_touch_event>(cur_time, action.second.side, data.bState));
								break;
							case EVENT_TYPE::VR_THUMBSTICK_PRESS:
								events.emplace_back(
									make_shared<vr_thumbstick_press_event>(cur_time, action.second.side, data.bState));
								break;
							case EVENT_TYPE::VR_THUMBSTICK_TOUCH:
								events.emplace_back(
									make_shared<vr_thumbstick_touch_event>(cur_time, action.second.side, data.bState));
								break;
							case EVENT_TYPE::VR_TRIGGER_PRESS:
								events.emplace_back(
									make_shared<vr_trigger_press_event>(cur_time, action.second.side, data.bState));
								break;
							case EVENT_TYPE::VR_TRIGGER_TOUCH:
								events.emplace_back(
									make_shared<vr_trigger_touch_event>(cur_time, action.second.side, data.bState));
								break;
							case EVENT_TYPE::VR_GRIP_PRESS:
								events.emplace_back(
									make_shared<vr_grip_press_event>(cur_time, action.second.side, data.bState));
								break;
							case EVENT_TYPE::VR_GRIP_TOUCH:
								events.emplace_back(
									make_shared<vr_grip_touch_event>(cur_time, action.second.side, data.bState));
								break;
							default:
								log_error("unknown/unhandled VR event: $", action.first);
								break;
						}
					}
					break;
				}
				case ACTION_TYPE::ANALOG: {
					vr::InputAnalogActionData_t data{};
					err = input->GetAnalogActionData(action.second.handle, &data, sizeof(vr::InputAnalogActionData_t),
													 vr::k_ulInvalidInputValueHandle);
					if (err != vr::EVRInputError::VRInputError_None) {
						break;
					}
					if (data.bActive && (data.deltaX != 0.0f || data.deltaY != 0.0f || data.deltaZ != 0.0f)) {
						switch (action.second.event_type) {
							case EVENT_TYPE::VR_TRACKPAD_MOVE:
								events.emplace_back(make_shared<vr_trackpad_move_event>(cur_time, action.second.side,
																						float2{ data.x, data.y },
																						float2{ data.deltaX, data.deltaY }));
								break;
							case EVENT_TYPE::VR_THUMBSTICK_MOVE:
								events.emplace_back(make_shared<vr_thumbstick_move_event>(cur_time, action.second.side,
																						  float2{ data.x, data.y },
																						  float2{ data.deltaX, data.deltaY }));
								break;
							case EVENT_TYPE::VR_TRIGGER_PULL:
								events.emplace_back(
									make_shared<vr_trigger_pull_event>(cur_time, action.second.side, data.x, data.deltaX));
								break;
							case EVENT_TYPE::VR_GRIP_PULL:
								events.emplace_back(
									make_shared<vr_grip_pull_event>(cur_time, action.second.side, data.x, data.deltaX));
								break;
							case EVENT_TYPE::VR_TRACKPAD_FORCE:
								events.emplace_back(
									make_shared<vr_trackpad_force_event>(cur_time, action.second.side, data.x, data.deltaX));
								break;
							case EVENT_TYPE::VR_GRIP_FORCE:
								events.emplace_back(
									make_shared<vr_grip_force_event>(cur_time, action.second.side, data.x, data.deltaX));
								break;
							default:
								log_error("unknown/unhandled VR event: $", action.first);
								break;
						}
					}
					break;
				}
				case ACTION_TYPE::SKELETAL: {
					if (!has_hand_tracking_support) {
						break;
					}

					vr::InputSkeletalActionData_t data{};
					err = input->GetSkeletalActionData(action.second.handle, &data, sizeof(vr::InputSkeletalActionData_t));
					if (err != vr::EVRInputError::VRInputError_None) {
						break;
					}
					if (data.bActive) {
						// check tracking level before retrieving bone transforms
						vr::EVRSkeletalTrackingLevel tracking_level = vr::VRSkeletalTracking_Estimated;
						if (auto tracking_err = input->GetSkeletalTrackingLevel(action.second.handle, &tracking_level);
							tracking_err != vr::EVRInputError::VRInputError_None) {
							log_warn("failed to retrieve bone tracking level: $", tracking_err);
							break;
						}
						// ignore if not at least partial
						if (tracking_level < vr::VRSkeletalTracking_Partial) {
							break;
						}

						// we have active hand tracking/skeletal info for this hand
						// -> we need to query all bones, but we'll handle the ones that we actually want
						vector<vr::VRBoneTransform_t> bone_transforms(expected_bone_count);
						if (auto skel_err = input->GetSkeletalBoneData(action.second.handle, vr::VRSkeletalTransformSpace_Model,
																	   vr::VRSkeletalMotionRange_WithoutController,
																	   bone_transforms.data(), (uint32_t)bone_transforms.size());
							skel_err != vr::EVRInputError::VRInputError_None) {
							log_warn("failed to retrieve bone data: $", skel_err);
							break;
						}

						// add/create all bone poses, however, these will only be added if we actually have a resp. hand pose
						auto bone_pose_type = uint32_t(!action.second.side ? POSE_TYPE::HAND_JOINT_PALM_LEFT : POSE_TYPE::HAND_JOINT_PALM_RIGHT);
						hand_bone_poses_valid[!action.second.side ? 0 : 1] = true;
						for (uint32_t bone_idx = 0; bone_idx < handled_bone_count; ++bone_idx) {
							const auto& bone_transform = bone_transforms[bone_idx];
							// NOTE: we don't get any per-bone velocity info
							auto& pose = hand_bone_poses[!action.second.side ? 0 : 1][bone_idx];
							pose.type = POSE_TYPE(bone_pose_type + bone_idx);
							pose.flags.is_active = 1;
							pose.flags.position_valid = 1;
							pose.flags.orientation_valid = 1;
							pose.flags.position_tracked = 1;
							pose.flags.orientation_tracked = 1;
							pose.position = { bone_transform.position.v[0], bone_transform.position.v[1], bone_transform.position.v[2] };
							pose.orientation = { bone_transform.orientation.x, bone_transform.orientation.y, bone_transform.orientation.z, bone_transform.orientation.w };
						}
					}
					break;
				}
				default:
					// not handled yet
					break;
			}
			if (err != vr::EVRInputError::VRInputError_None) {
				log_error("failed to update action $: $", action.first, err);
				continue;
			}
		}
	}

	// poses / tracked device handling
	vector<pose_t> updated_pose_state;
	updated_pose_state.reserve(std::max(prev_pose_state_size, size_t(4u)));

	static const auto ignore_trackers = !floor::get_vr_trackers();
	array<vr::TrackedDevicePose_t, vr::k_unMaxTrackedDeviceCount> vr_poses {};
	const auto err = compositor->WaitGetPoses(&vr_poses[0], vr::k_unMaxTrackedDeviceCount, nullptr, 0);
	if (err != vr::EVRCompositorError::VRCompositorError_None) {
		log_error("failed to update VR poses: $", err);
	} else {
		for (uint32_t i = 0; i < vr::k_unMaxTrackedDeviceCount; ++i) {
			const auto& vr_pose = vr_poses[i];
			device_active[i] = vr_pose.bDeviceIsConnected;

			// ignore of disconnected
			if (!vr_pose.bDeviceIsConnected) {
				continue;
			}

			// -> still set post type if pose is invalid
			pose_t pose { .type = device_index_to_pose_type(i), .flags.all = 0u };

			// now also abort if pose is invalid
			if (!vr_pose.bPoseIsValid || pose.type == vr_context::POSE_TYPE::UNKNOWN) {
				continue;
			}

			// ignore tracker devices if trackers support is disabled
			if (ignore_trackers && pose.type == vr_context::POSE_TYPE::TRACKER) {
				continue;
			}

			// handle activity
			switch (vr_pose.eTrackingResult) {
				case vr::TrackingResult_Uninitialized:
				case vr::TrackingResult_Calibrating_InProgress:
				case vr::TrackingResult_Calibrating_OutOfRange:
					pose.flags.is_active = 0;
					break;
				case vr::TrackingResult_Running_OK:
				case vr::TrackingResult_Running_OutOfRange:
				case vr::TrackingResult_Fallback_RotationOnly:
					pose.flags.is_active = 1;
					break;
			}

			if (pose.flags.is_active) {
				// handle velocity validity based on pose type
				switch (pose.type) {
					case vr_context::POSE_TYPE::UNKNOWN:
					case vr_context::POSE_TYPE::SPECIAL:
					case vr_context::POSE_TYPE::REFERENCE:
						// nope
						break;
					case vr_context::POSE_TYPE::HEAD:
					case vr_context::POSE_TYPE::HAND_LEFT:
					case vr_context::POSE_TYPE::HAND_RIGHT:
					case vr_context::POSE_TYPE::HAND_LEFT_AIM: // TODO: actually support this somehow
					case vr_context::POSE_TYPE::HAND_RIGHT_AIM: // TODO: actually support this somehow
					case vr_context::POSE_TYPE::TRACKER...vr_context::POSE_TYPE::TRACKER_ANKLE_RIGHT:
					case vr_context::POSE_TYPE::HAND_JOINT_PALM_LEFT...vr_context::POSE_TYPE::HAND_FOREARM_JOINT_ELBOW_RIGHT:
						pose.flags.linear_velocity_valid = 1;
						pose.flags.angular_velocity_valid = 1;
						pose.flags.linear_velocity_tracked = 1;
						pose.flags.angular_velocity_tracked = 1;
						pose.linear_velocity = { vr_pose.vVelocity.v[0], vr_pose.vVelocity.v[1], vr_pose.vVelocity.v[2] };
						pose.angular_velocity = { vr_pose.vAngularVelocity.v[0], vr_pose.vAngularVelocity.v[1],
												  vr_pose.vAngularVelocity.v[2] };
						break;
				}

				// handle pose orientation + position
				assert(vr_pose.bPoseIsValid); // already checked above
				const auto& vr_pose_mat = vr_pose.mDeviceToAbsoluteTracking.m;
				pose.flags.position_valid = 1;
				pose.flags.orientation_valid = 1;
				pose.flags.position_tracked = 1;
				pose.flags.orientation_tracked = 1;
				pose.position = { vr_pose_mat[0][3], vr_pose_mat[1][3], vr_pose_mat[2][3] };
				// https://github.com/ValveSoftware/openvr/wiki/Matrix-Usage-Example
				// "axes and translation vectors are represented as column vectors, while their memory layout is row-major"
				pose.orientation = quaternionf::from_matrix4(matrix4f {
					vr_pose_mat[0][0], vr_pose_mat[1][0], vr_pose_mat[2][0], 0.0f,
					vr_pose_mat[0][1], vr_pose_mat[1][1], vr_pose_mat[2][1], 0.0f,
					vr_pose_mat[0][2], vr_pose_mat[1][2], vr_pose_mat[2][2], 0.0f,
					0.0f, 0.0f, 0.0f, 1.0f
				});
				
				// add hand bone poses if the left/right hand pose is actually active
				if ((pose.type == vr_context::POSE_TYPE::HAND_LEFT && hand_bone_poses_valid.x) ||
					(pose.type == vr_context::POSE_TYPE::HAND_RIGHT && hand_bone_poses_valid.y)) {
					updated_pose_state.insert(updated_pose_state.end(),
											  hand_bone_poses[pose.type == vr_context::POSE_TYPE::HAND_LEFT ? 0 : 1].begin(),
											  hand_bone_poses[pose.type == vr_context::POSE_TYPE::HAND_LEFT ? 0 : 1].end());
				}
			}

			updated_pose_state.emplace_back(std::move(pose));
		}

		const auto& hmd_pose = vr_poses[vr::k_unTrackedDeviceIndex_Hmd];
		if (hmd_pose.bPoseIsValid) {
			const auto& vr_mat = hmd_pose.mDeviceToAbsoluteTracking;
			hmd_mat = { vr_mat.m[0][0], vr_mat.m[1][0], vr_mat.m[2][0], 0.0f,
						vr_mat.m[0][1], vr_mat.m[1][1], vr_mat.m[2][1], 0.0f,
						vr_mat.m[0][2], vr_mat.m[1][2], vr_mat.m[2][2], 0.0f,
						vr_mat.m[0][3], vr_mat.m[1][3], vr_mat.m[2][3], 1.0f };
			hmd_mat.invert();
		} else {
			hmd_mat.identity();
		}
	}

	// update pose state
	prev_pose_state_size = updated_pose_state.size();
	{
		GUARD(pose_state_lock);
		pose_state = std::move(updated_pose_state);
	}

	// system event handling
	vr::VREvent_t evt {};
	while (system->PollNextEvent(&evt, sizeof(vr::VREvent_t))) {
		switch (evt.eventType) {
			case vr::VREvent_TrackedDeviceActivated:
			case vr::VREvent_TrackedDeviceUpdated:
				// ensure this is up-to-date
				(void)device_index_to_pose_type(evt.trackedDeviceIndex);
				[[fallthrough]];
			case vr::VREvent_TrackedDeviceDeactivated:
				update_hand_controller_types();
				break;
			default:
				break;
		}
	}

	if (force_update_controller_types) {
		force_update_controller_types = false;
		update_hand_controller_types();
	}

	return events;
}

vr_context::POSE_TYPE openvr_context::device_index_to_pose_type(const uint32_t idx) REQUIRES(!device_type_map_lock) {
	if (idx == vr::k_unTrackedDeviceIndex_Hmd) {
		return POSE_TYPE::HEAD;
	}

	// it is very likely that we already know this mapping (-> double locking not an issue)
	{
		GUARD(device_type_map_lock);
		if (auto pose_type = device_type_map[idx]; pose_type != vr_context::POSE_TYPE::UNKNOWN) {
			return pose_type;
		}
	}

	const auto dev_class = system->GetTrackedDeviceClass(idx);
	auto pose_type = POSE_TYPE::UNKNOWN;
	switch (dev_class) {
		case vr::TrackedDeviceClass_Invalid:
			// keep invalid/unknown
			break;
		case vr::TrackedDeviceClass_DisplayRedirect:
			pose_type = POSE_TYPE::SPECIAL;
			break;
		case vr::TrackedDeviceClass_TrackingReference:
			pose_type = POSE_TYPE::REFERENCE;
			break;
		case vr::TrackedDeviceClass_Controller: {
			const auto ctrl_role = system->GetControllerRoleForTrackedDeviceIndex(idx);
			switch (ctrl_role) {
				case vr::TrackedControllerRole_Invalid:
					// keep invalid/unknown
					break;
				case vr::TrackedControllerRole_LeftHand:
					pose_type = POSE_TYPE::HAND_LEFT;
					// actually knowing that a tracked device index is a controller may take some time -> force update
					if (hand_controller_types[0] == CONTROLLER_TYPE::NONE) {
						force_update_controller_types = true;
					}
					break;
				case vr::TrackedControllerRole_RightHand:
					pose_type = POSE_TYPE::HAND_RIGHT;
					// actually knowing that a tracked device index is a controller may take some time -> force update
					if (hand_controller_types[1] == CONTROLLER_TYPE::NONE) {
						force_update_controller_types = true;
					}
					break;
				case vr::TrackedControllerRole_OptOut:
				case vr::TrackedControllerRole_Treadmill: // TODO: handle this
				case vr::TrackedControllerRole_Stylus: // TODO: handle this
					pose_type = POSE_TYPE::SPECIAL;
					break;
			}
			break;
		}
		case vr::TrackedDeviceClass_GenericTracker: {
			pose_type = POSE_TYPE::TRACKER;
			if (floor::get_vr_trackers()) {
				const auto tracker_name = get_vr_property<string>(*system, vr::Prop_ControllerType_String, idx);
				if (auto tracker_type = tracker_type_from_name(tracker_name); tracker_type) {
					pose_type = *tracker_type;
				}
				log_msg("OpenVR: now using tracker: \"$\" -> $", tracker_name, pose_type_to_string(pose_type));
			}
			break;
		}
		case vr::TrackedDeviceClass_HMD:
		case vr::TrackedDeviceClass_Max:
			assert(false && "shouldn't be here");
			break;
	}

	{
		GUARD(device_type_map_lock);
		device_type_map[idx] = pose_type;
	}
	return pose_type;
}

bool openvr_context::present(const compute_queue& cqueue, const compute_image* image) {
	if (!image) {
		log_error("OpenVR present image must not be nullptr");
		return false;
	}

#if !defined(FLOOR_NO_VULKAN) && !defined(__APPLE__)
	// check if specified queue and image are actually from Vulkan
	if (const auto vk_queue_ptr = dynamic_cast<const vulkan_queue*>(&cqueue); !vk_queue_ptr) {
		log_error("specified queue is not a Vulkan queue");
		return false;
	}
	if (const auto vk_image_ptr = dynamic_cast<const vulkan_image*>(image); !vk_image_ptr) {
		log_error("specified queue is not a Vulkan image");
		return false;
	}

	const auto& vk_queue = (const vulkan_queue&)cqueue;
	const auto& vk_dev = (const vulkan_device&)vk_queue.get_device();
	const auto& vk_image = *(const vulkan_image*)image;

	const auto left_eye_image = vk_image.get_vulkan_aliased_layer_image(0);
	const auto right_eye_image = vk_image.get_vulkan_aliased_layer_image(1);
	if (left_eye_image == nullptr || right_eye_image == nullptr) {
		log_error("failed to retrieve aliased Vulkan layer image");
		return false;
	}

	// present VR images
	vr::VRVulkanTextureData_t vr_vk_image {
		.m_nImage = uint64_t(left_eye_image),
		.m_pDevice = vk_dev.device,
		.m_pPhysicalDevice = vk_dev.physical_device,
		.m_pInstance = ((const vulkan_compute*)vk_dev.context)->get_vulkan_context(),
		.m_pQueue = (VkQueue)const_cast<void*>(vk_queue.get_queue_ptr()),
		.m_nQueueFamilyIndex = vk_queue.get_family_index(),
		.m_nWidth = image->get_image_dim().x,
		.m_nHeight = image->get_image_dim().y,
		.m_nFormat = uint32_t(vk_image.get_vulkan_format()),
		.m_nSampleCount = 1,
	};
	vr::Texture_t vr_image {
		.handle = &vr_vk_image,
		.eType = vr::ETextureType::TextureType_Vulkan,
		// TODO: properly determine color space we're rendering in and how it is presented to the HMD
		.eColorSpace = vr::EColorSpace::ColorSpace_Gamma,
	};

	auto err = compositor->Submit(vr::EVREye::Eye_Left, &vr_image, nullptr, vr::EVRSubmitFlags::Submit_Default);
	if (err != vr::EVRCompositorError::VRCompositorError_None) {
		log_error("failed to submit left VR eye image: $", err);
		return false;
	}

	vr_vk_image.m_nImage = uint64_t(right_eye_image);
	err = compositor->Submit(vr::EVREye::Eye_Right, &vr_image, nullptr, vr::EVRSubmitFlags::Submit_Default);
	if (err != vr::EVRCompositorError::VRCompositorError_None) {
		log_error("failed to submit right VR eye image: $", err);
		return false;
	}

	return true;
#elif !defined(FLOOR_NO_METAL)
	(void)cqueue; // unused
	
	// check if specified image is actually from Metal
	if (const auto mtl_image_ptr = dynamic_cast<const metal_image*>(image); !mtl_image_ptr) {
		log_error("specified queue is not a Metal image");
		return false;
	}
	const auto& mtl_image = *(const metal_image*)image;

	// present VR image
	// NOTE: with TextureType_Metal we can directly present a layered 2D image
	vr::Texture_t vr_image {
		.handle = mtl_image.get_metal_image_void_ptr(),
		.eType = vr::ETextureType::TextureType_Metal,
		.eColorSpace = vr::EColorSpace::ColorSpace_Gamma,
	};

	auto err = compositor->Submit(vr::EVREye::Eye_Left, &vr_image, nullptr, vr::EVRSubmitFlags::Submit_Default);
	if (err != vr::EVRCompositorError::VRCompositorError_None) {
		log_error("failed to submit left VR eye image: $", err);
		return false;
	}

	err = compositor->Submit(vr::EVREye::Eye_Right, &vr_image, nullptr, vr::EVRSubmitFlags::Submit_Default);
	if (err != vr::EVRCompositorError::VRCompositorError_None) {
		log_error("failed to submit right VR eye image: $", err);
		return false;
	}
	
	return true;
#else
	log_error("non-Vulkan present path not implemented");
	return false;
#endif
}

vr_context::frame_view_state_t openvr_context::get_frame_view_state(const float& z_near, const float& z_far,
																	const bool with_position_in_mvm) const {
	auto mview_hmd = get_hmd_matrix();
	if (!with_position_in_mvm) {
		mview_hmd.set_translation(0.0f, 0.0f, 0.0f);
	}
	const auto eye_mat_left = get_eye_matrix(VR_EYE::LEFT);
	const auto eye_mat_right = get_eye_matrix(VR_EYE::RIGHT);
	const float eye_distance {
		(float3 { eye_mat_left.data[12], eye_mat_left.data[13], eye_mat_left.data[14] } -
		 float3 { eye_mat_right.data[12], eye_mat_right.data[13], eye_mat_right.data[14] }).length()
	};
	const auto hmd_inv_mat = get_hmd_matrix().inverted();
	const float3 hmd_position { -hmd_inv_mat.data[12], -hmd_inv_mat.data[13], -hmd_inv_mat.data[14] };

	return {
		hmd_position,
		eye_distance,
		mview_hmd * eye_mat_left,
		mview_hmd * eye_mat_right,
		get_projection_matrix(VR_EYE::LEFT, z_near, z_far),
		get_projection_matrix(VR_EYE::RIGHT, z_near, z_far)
	};
}

matrix4f openvr_context::get_projection_matrix(const VR_EYE& eye, const float& z_near, const float& z_far) const {
	// build our own projection matrix
	// NOTE: raw projection values are already pre-adjusted -> https://github.com/ValveSoftware/openvr/wiki/IVRSystem::GetProjectionRaw
	const auto fov_lrtb = get_projection_raw(eye);
	return matrix4f::perspective<true /* pre_adjusted_fov */, true /* is_right_handed */, true /* is_only_positive_z */>
								(fov_lrtb.x, fov_lrtb.y, fov_lrtb.z, fov_lrtb.w, z_near, z_far);
}

float4 openvr_context::get_projection_raw(const VR_EYE& eye) const {
	float4 ret;
	system->GetProjectionRaw(eye_to_eye(eye), &ret.x, &ret.y, &ret.z, &ret.w);
	return ret;
}

matrix4f openvr_context::get_eye_matrix(const VR_EYE& eye) const {
	const auto mat = system->GetEyeToHeadTransform(eye_to_eye(eye));
	return matrix4f(
		mat.m[0][0], mat.m[1][0], mat.m[2][0], 0.0f,
		mat.m[0][1], mat.m[1][1], mat.m[2][1], 0.0f,
		mat.m[0][2], mat.m[1][2], mat.m[2][2], 0.0f,
		mat.m[0][3], mat.m[1][3], mat.m[2][3], 1.0f
	).inverted();
}

const matrix4f& openvr_context::get_hmd_matrix() const {
	return hmd_mat;
}

vector<vr_context::pose_t> openvr_context::get_pose_state() const REQUIRES(!pose_state_lock) {
	GUARD(pose_state_lock);
	return pose_state;
}

void openvr_context::update_hand_controller_types() REQUIRES(!device_type_map_lock) {
	// search for known left/right hand controller pose type -> device index
	hand_device_indices = { 0u, 0u }; // reset
	{
		GUARD(device_type_map_lock);
		for (uint32_t dev_idx = 0; dev_idx < max_tracked_devices; ++dev_idx) {
			if (device_type_map[dev_idx] == vr_context::POSE_TYPE::HAND_LEFT) {
				hand_device_indices[0] = dev_idx;
				if (hand_device_indices[1] != 0u) {
					break; // break if both were found
				}
			} else if (device_type_map[dev_idx] == vr_context::POSE_TYPE::HAND_RIGHT) {
				hand_device_indices[1] = dev_idx;
				if (hand_device_indices[0] != 0u) {
					break; // break if both were found
				}
			}
		}
	}

	for (uint32_t hand_idx = 0; hand_idx < 2; ++hand_idx) {
		// if the device is inactive -> set to 0 again (NONE controller)
		if (hand_device_indices[hand_idx] > 0 && !device_active[hand_device_indices[hand_idx]]) {
			hand_device_indices[hand_idx] = 0u;
		}

		if (hand_device_indices[hand_idx] == 0u) {
			hand_controller_types[hand_idx] = CONTROLLER_TYPE::NONE;
			continue;
		}

		// NOTE: would prefer using Prop_AttachedDeviceId_String, but this doesn't work at all
		//       -> need to use Prop_ControllerType_String instead, but this doesn't differentiate enough between controller types
		const auto controller_name = get_vr_property<string>(*system, vr::Prop_ControllerType_String, hand_device_indices[hand_idx]);
		static const unordered_map<string, CONTROLLER_TYPE> controller_name_map {
			{ "knuckles", CONTROLLER_TYPE::INDEX },
			{ "vive_controller", CONTROLLER_TYPE::HTC_VIVE },
			{ "vive_cosmos_controller", CONTROLLER_TYPE::HTC_VIVE_COSMOS },
			{ "oculus_touch", CONTROLLER_TYPE::OCULUS_TOUCH },
			{ "holographic_controller", CONTROLLER_TYPE::MICROSOFT_MIXED_REALITY },
			{ "hpmotioncontroller", CONTROLLER_TYPE::HP_MIXED_REALITY },
			{ "pico_controller", CONTROLLER_TYPE::PICO_NEO3 },
		};
		const auto controller_iter = controller_name_map.find(controller_name);
		if (controller_iter == controller_name_map.end()) {
			log_error("unknown controller type: $", controller_name);
			hand_controller_types[hand_idx] = CONTROLLER_TYPE::NONE;
			continue;
		}
		hand_controller_types[hand_idx] = controller_iter->second;
	}

	for (uint32_t hand_idx = 0; hand_idx < 2; ++hand_idx) {
		log_msg("OpenVR: now using $ hand controller: $", hand_idx == 0 ? "left" : "right",
				controller_type_to_string(hand_controller_types[hand_idx]));
	}
}

#endif
