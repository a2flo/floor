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

#include <floor/vr/openxr_context.hpp>

#if !defined(FLOOR_NO_OPENXR) && !defined(FLOOR_NO_VULKAN)

#include "device/vulkan/internal/vulkan_headers.hpp"
#include "device/vulkan/internal/vulkan_image_internal.hpp"

#define XR_USE_GRAPHICS_API_VULKAN 1
#if defined(__linux__)
#define XR_USE_TIMESPEC 1
#endif
#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>

#include "internal/openxr_internal.hpp"

#include <SDL3/SDL_timer.h>
#include <cassert>

#if !defined(__WINDOWS__)
#include <string.h>
#define strcpy_s(dst, len, src) strncpy(dst, src, len)
#endif

namespace fl {
using namespace std::literals;

const std::array<openxr_context::input_event_emulation_t, size_t(openxr_context::CONTROLLER_TYPE::__MAX_CONTROLLER_TYPE)>
openxr_context::controller_input_emulation_lut {{
	// NONE
	{},
	// KHRONOS_SIMPLE
	// NOTE: can't emulate any of the non-existing inputs
	{},
	// INDEX
	{
		.grip_press = 1,
		.trackpad_press = 1,
		.grip_touch = 1,
	},
	// HTC_VIVE
	// NOTE: no VR_THUMBSTICK_* or VR_GRIP_* and can't emulate them
	// NOTE: no touch events except VR_TRACKPAD -> can't emulate them
	// NOTE: VR_TRACKPAD_FORCE -> can't emulate
	{},
	// GOOGLE_DAYDREAM
	// NOTE: can't emulate any of the non-existing inputs
	{},
	// MICROSOFT_MIXED_REALITY
	// NOTE: no VR_SYSTEM_* or VR_GRIP_* -> can't emulate this
	// NOTE: no VR_TRIGGER_TOUCH/VR_THUMBSTICK_TOUCH/VR_TRACKPAD_FORCE -> can't emulate this
	{
		.trigger_press = 1,
	},
	// OCULUS_GO
	// NOTE: no VR_THUMBSTICK_*, VR_TRIGGER_* or VR_GRIP_* -> can't emulate these
	{},
	// OCULUS_TOUCH
	// NOTE: no VR_TRACKPAD_* -> can't emulate this
	{
		.grip_press = 1,
		.trigger_press = 1,
	},
	// HP_MIXED_REALITY
	// NOTE: no VR_TRACKPAD_* -> can't emulate this
	{
		.grip_press = 1,
		.trigger_press = 1,
	},
	// HTC_VIVE_COSMOS
	// NOTE: no VR_TRACKPAD_* -> can't emulate this
	{},
	// HTC_VIVE_FOCUS3
	// NOTE: no VR_TRACKPAD_* -> can't emulate this
	{},
	// HUAWEI
	// NOTE: no VR_THUMBSTICK_*, VR_GRIP_* -> can't emulate these
	{},
	// SAMSUNG_ODYSSEY
	// NOTE: no VR_SYSTEM_* or VR_GRIP_* -> can't emulate this
	// NOTE: no VR_TRIGGER_TOUCH/VR_THUMBSTICK_TOUCH/VR_TRACKPAD_FORCE -> can't emulate this
	{
		.trigger_press = 1,
	},
	// MAGIC_LEAP2
	// NOTE: no VR_THUMBSTICK_*, VR_GRIP_* -> can't emulate these
	{},
	// OCULUS_TOUCH_PRO
	// NOTE: no VR_TRACKPAD_* -> can't emulate this
	{
		.grip_press = 1,
		.trigger_press = 1,
	},
	// PICO_NEO3
	// NOTE: no VR_TRACKPAD_* -> can't emulate this
	{},
	// PICO4
	// NOTE: no VR_TRACKPAD_* -> can't emulate this
	{},
}};

static std::string tracker_role_to_string(const vr_context::POSE_TYPE tracker_type) {
	std::string role_str;
	switch (tracker_type) {
		case vr_context::POSE_TYPE::TRACKER_HANDHELD_OBJECT:
			role_str = "handheld_object";
			break;
		case vr_context::POSE_TYPE::TRACKER_FOOT_LEFT:
			role_str = "left_foot";
			break;
		case vr_context::POSE_TYPE::TRACKER_FOOT_RIGHT:
			role_str = "right_foot";
			break;
		case vr_context::POSE_TYPE::TRACKER_SHOULDER_LEFT:
			role_str = "left_shoulder";
			break;
		case vr_context::POSE_TYPE::TRACKER_SHOULDER_RIGHT:
			role_str = "right_shoulder";
			break;
		case vr_context::POSE_TYPE::TRACKER_ELBOW_LEFT:
			role_str = "left_elbow";
			break;
		case vr_context::POSE_TYPE::TRACKER_ELBOW_RIGHT:
			role_str = "right_elbow";
			break;
		case vr_context::POSE_TYPE::TRACKER_KNEE_LEFT:
			role_str = "left_knee";
			break;
		case vr_context::POSE_TYPE::TRACKER_KNEE_RIGHT:
			role_str = "right_knee";
			break;
		case vr_context::POSE_TYPE::TRACKER_WAIST:
			role_str = "waist";
			break;
		case vr_context::POSE_TYPE::TRACKER_CHEST:
			role_str = "chest";
			break;
		case vr_context::POSE_TYPE::TRACKER_CAMERA:
			role_str = "camera";
			break;
		case vr_context::POSE_TYPE::TRACKER_KEYBOARD:
			role_str = "keyboard";
			break;
		case vr_context::POSE_TYPE::TRACKER_WRIST_LEFT:
			role_str = "left_wrist";
			break;
		case vr_context::POSE_TYPE::TRACKER_WRIST_RIGHT:
			role_str = "right_wrist";
			break;
		case vr_context::POSE_TYPE::TRACKER_ANKLE_LEFT:
			role_str = "left_ankle";
			break;
		case vr_context::POSE_TYPE::TRACKER_ANKLE_RIGHT:
			role_str = "right_ankle";
			break;
		default:
			throw std::runtime_error("invalid tracker role");
	}
	return role_str;
}

struct action_definition_t {
	std::string path;
	EVENT_TYPE event_type;
};

bool openxr_context::input_setup() {
	// create base input set
	const XrActionSetCreateInfo action_set_create_info {
		.type = XR_TYPE_ACTION_SET_CREATE_INFO,
		.next = nullptr,
		.actionSetName = "vr_input_default",
		.localizedActionSetName = "vr_input_default",
		.priority = 0,
	};
	XR_CALL_RET(xrCreateActionSet(instance, &action_set_create_info, &input_action_set),
				"failed to create input action set", false);
	
	// create base actions
	auto hand_left_path = to_path("/user/hand/left");
	auto hand_right_path = to_path("/user/hand/right");
	auto head_path = to_path("/user/head");
	auto gamepad_path = to_path("/user/gamepad");
	if (!hand_left_path || !hand_right_path || !head_path || !gamepad_path) {
		return false;
	}
	hand_paths = { *hand_left_path, *hand_right_path };
	input_paths = { *hand_left_path, *hand_right_path, *head_path, *gamepad_path };
	
	for (uint32_t i = 0; i < 2; ++i) {
		XrActionCreateInfo action_create_info {
			.type = XR_TYPE_ACTION_CREATE_INFO,
			.next = nullptr,
			.actionType = XR_ACTION_TYPE_POSE_INPUT,
			.countSubactionPaths = 1u,
			.subactionPaths = &hand_paths[i],
		};
		if (i == 0) {
			strcpy_s(action_create_info.actionName, std::size(action_create_info.actionName), "hand_pose_left");
			strcpy_s(action_create_info.localizedActionName, std::size(action_create_info.localizedActionName), "hand_pose_left");
		} else {
			strcpy_s(action_create_info.actionName, std::size(action_create_info.actionName), "hand_pose_right");
			strcpy_s(action_create_info.localizedActionName, std::size(action_create_info.localizedActionName), "hand_pose_right");
		}
		XR_CALL_RET(xrCreateAction(input_action_set, &action_create_info, &hand_pose_actions[i]),
					"failed to create hand pose action", false);
		
		if (i == 0) {
			strcpy_s(action_create_info.actionName, std::size(action_create_info.actionName), "hand_aim_pose_left");
			strcpy_s(action_create_info.localizedActionName, std::size(action_create_info.localizedActionName), "hand_aim_pose_left");
		} else {
			strcpy_s(action_create_info.actionName, std::size(action_create_info.actionName), "hand_aim_pose_right");
			strcpy_s(action_create_info.localizedActionName, std::size(action_create_info.localizedActionName), "hand_aim_pose_right");
		}
		XR_CALL_RET(xrCreateAction(input_action_set, &action_create_info, &hand_aim_pose_actions[i]),
					"failed to create hand aim pose action", false);
		
		XrActionSpaceCreateInfo hand_space_create_info {
			.type = XR_TYPE_ACTION_SPACE_CREATE_INFO,
			.next = nullptr,
			.action = hand_pose_actions[i],
			.subactionPath = hand_paths[i],
			.poseInActionSpace = {
				.orientation = { 0.0f, 0.0f, 0.0f, 1.0f },
				.position = { 0.0f, 0.0f, 0.0f },
			},
		};
		XR_CALL_RET(xrCreateActionSpace(session, &hand_space_create_info, &hand_spaces[i]),
					"failed to create hand action space", false);
		
		hand_space_create_info.action = hand_aim_pose_actions[i];
		XR_CALL_RET(xrCreateActionSpace(session, &hand_space_create_info, &hand_aim_spaces[i]),
					"failed to create hand aim action space", false);
		
		base_actions.emplace((i == 0 ? EVENT_TYPE::VR_INTERNAL_HAND_POSE_LEFT : EVENT_TYPE::VR_INTERNAL_HAND_POSE_RIGHT),
							 action_t {
			.action = hand_pose_actions[i],
			.input_type = (i == 0 ? INPUT_TYPE::HAND_LEFT : INPUT_TYPE::HAND_RIGHT),
			.action_type = ACTION_TYPE::POSE,
		});
		
		base_actions.emplace((i == 0 ? EVENT_TYPE::VR_INTERNAL_HAND_AIM_LEFT : EVENT_TYPE::VR_INTERNAL_HAND_AIM_RIGHT),
							 action_t {
			.action = hand_aim_pose_actions[i],
			.input_type = (i == 0 ? INPUT_TYPE::HAND_LEFT : INPUT_TYPE::HAND_RIGHT),
			.action_type = ACTION_TYPE::POSE,
		});
	}
	
	struct base_action_definition_t {
		std::string_view name;
		ACTION_TYPE action_type;
		EVENT_TYPE event_type;
	};
	{
		static constexpr const std::array<base_action_definition_t, 23> base_actions_defs {{
			{ "system_press", ACTION_TYPE::BOOLEAN, EVENT_TYPE::VR_SYSTEM_PRESS },
			{ "system_touch", ACTION_TYPE::BOOLEAN, EVENT_TYPE::VR_SYSTEM_TOUCH },
			{ "main_press", ACTION_TYPE::BOOLEAN, EVENT_TYPE::VR_MAIN_PRESS },
			{ "main_touch", ACTION_TYPE::BOOLEAN, EVENT_TYPE::VR_MAIN_TOUCH },
			{ "app_menu_press", ACTION_TYPE::BOOLEAN, EVENT_TYPE::VR_APP_MENU_PRESS },
			{ "app_menu_touch", ACTION_TYPE::BOOLEAN, EVENT_TYPE::VR_APP_MENU_TOUCH },
			{ "grip_pull", ACTION_TYPE::FLOAT, EVENT_TYPE::VR_GRIP_PULL },
			{ "grip_force", ACTION_TYPE::FLOAT, EVENT_TYPE::VR_GRIP_FORCE },
			{ "grip_press", ACTION_TYPE::BOOLEAN, EVENT_TYPE::VR_GRIP_PRESS },
			{ "grip_touch", ACTION_TYPE::BOOLEAN, EVENT_TYPE::VR_GRIP_TOUCH },
			{ "trigger_press", ACTION_TYPE::BOOLEAN, EVENT_TYPE::VR_TRIGGER_PRESS },
			{ "trigger_pull", ACTION_TYPE::FLOAT, EVENT_TYPE::VR_TRIGGER_PULL },
			{ "trigger_touch", ACTION_TYPE::BOOLEAN, EVENT_TYPE::VR_TRIGGER_TOUCH },
			{ "thumbstick_move", ACTION_TYPE::FLOAT2, EVENT_TYPE::VR_THUMBSTICK_MOVE },
			{ "thumbstick_press", ACTION_TYPE::BOOLEAN, EVENT_TYPE::VR_THUMBSTICK_PRESS },
			{ "thumbstick_touch", ACTION_TYPE::BOOLEAN, EVENT_TYPE::VR_THUMBSTICK_TOUCH },
			{ "trackpad_move", ACTION_TYPE::FLOAT2, EVENT_TYPE::VR_TRACKPAD_MOVE },
			{ "trackpad_force", ACTION_TYPE::FLOAT, EVENT_TYPE::VR_TRACKPAD_FORCE },
			{ "trackpad_press", ACTION_TYPE::BOOLEAN, EVENT_TYPE::VR_TRACKPAD_PRESS },
			{ "trackpad_touch", ACTION_TYPE::BOOLEAN, EVENT_TYPE::VR_TRACKPAD_TOUCH },
			{ "thumbrest_touch", ACTION_TYPE::BOOLEAN, EVENT_TYPE::VR_THUMBREST_TOUCH },
			{ "thumbrest_force", ACTION_TYPE::FLOAT, EVENT_TYPE::VR_THUMBREST_FORCE },
			{ "shoulder_press", ACTION_TYPE::BOOLEAN, EVENT_TYPE::VR_SHOULDER_PRESS },
		}};
		for (const auto& base_action_def : base_actions_defs) {
			XrActionType action_type {};
			switch (base_action_def.action_type) {
				case ACTION_TYPE::BOOLEAN:
					action_type = XR_ACTION_TYPE_BOOLEAN_INPUT;
					break;
				case ACTION_TYPE::FLOAT:
					action_type = XR_ACTION_TYPE_FLOAT_INPUT;
					break;
				case ACTION_TYPE::FLOAT2:
					action_type = XR_ACTION_TYPE_VECTOR2F_INPUT;
					break;
				case ACTION_TYPE::POSE:
					action_type = XR_ACTION_TYPE_POSE_INPUT;
					break;
				case ACTION_TYPE::HAPTIC:
					action_type = XR_ACTION_TYPE_VIBRATION_OUTPUT;
					break;
			}
			const auto action_name_size = base_action_def.name.size();
			if (action_name_size + 1 >= XR_MAX_ACTION_SET_NAME_SIZE ||
				action_name_size + 1 >= XR_MAX_LOCALIZED_ACTION_NAME_SIZE) {
				log_error("action name \"$\" is too long: $", base_action_def.name, action_name_size);
				return false;
			}
			XrActionCreateInfo action_create_info {
				.type = XR_TYPE_ACTION_CREATE_INFO,
				.next = nullptr,
				.actionType = action_type,
				.countSubactionPaths = uint32_t(hand_paths.size()),
				.subactionPaths = hand_paths.data(),
			};
			memcpy(action_create_info.actionName, base_action_def.name.data(), base_action_def.name.size());
			action_create_info.actionName[action_name_size] = '\0';
			memcpy(action_create_info.localizedActionName, base_action_def.name.data(), base_action_def.name.size());
			action_create_info.localizedActionName[action_name_size] = '\0';
			XrAction action { nullptr };
			XR_CALL_RET(xrCreateAction(input_action_set, &action_create_info, &action),
						"failed to create action " + std::string(base_action_def.name), false);
			base_actions.emplace(base_action_def.event_type,
								 action_t {
				.action = action,
				.input_type = INPUT_TYPE::HAND_LEFT | INPUT_TYPE::HAND_RIGHT,
				.action_type = base_action_def.action_type,
			});
		}
	}
	
	// create controller bindings
	const auto suggest_binding = [this](const std::string& interaction_profile,
										const CONTROLLER_TYPE& controller_type,
										const std::vector<action_definition_t>& both_hands,
										const std::vector<action_definition_t>& left_hand,
										const std::vector<action_definition_t>& right_hand) {
		std::vector<XrActionSuggestedBinding> bindings;
		for (uint32_t i = 0; i < 2; ++i) {
			const auto hand_path = "/user/hand/"s + (i == 0 ? "left" : "right");
			for (const auto& hand_action : both_hands) {
				bindings.emplace_back(XrActionSuggestedBinding {
					.action = base_actions.at(hand_action.event_type).action,
					.binding = to_path_or_throw(hand_path + hand_action.path),
				});
			}
		}
		for (const auto& hand_action : left_hand) {
			bindings.emplace_back(XrActionSuggestedBinding {
				.action = base_actions.at(hand_action.event_type).action,
				.binding = to_path_or_throw("/user/hand/left" + hand_action.path),
			});
		}
		for (const auto& hand_action : right_hand) {
			bindings.emplace_back(XrActionSuggestedBinding {
				.action = base_actions.at(hand_action.event_type).action,
				.binding = to_path_or_throw("/user/hand/right" + hand_action.path),
			});
		}
		const XrInteractionProfileSuggestedBinding suggested_binding {
			.type = XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING,
			.next = nullptr,
			.interactionProfile = to_path_or_throw(interaction_profile),
			.countSuggestedBindings = uint32_t(bindings.size()),
			.suggestedBindings = bindings.data(),
		};
		XR_CALL_RET(xrSuggestInteractionProfileBindings(instance, &suggested_binding),
					"failed to set suggested interaction profile for " +
					std::string(controller_type_to_string(controller_type)), false);
		
		// valid binding -> add to known interaction profile map
		interaction_profile_controller_lut.insert_or_assign(interaction_profile, controller_type);
		return true;
	};
	
	std::vector<action_definition_t> both_hand_actions;
	std::vector<action_definition_t> left_hand_actions;
	std::vector<action_definition_t> right_hand_actions;
	
	// Khronos simple controller
	{
		both_hand_actions = {
			{ "/input/select/click", EVENT_TYPE::VR_MAIN_PRESS },
			{ "/input/menu/click", EVENT_TYPE::VR_APP_MENU_PRESS },
		};
		left_hand_actions = {
			{ "/input/grip/pose", EVENT_TYPE::VR_INTERNAL_HAND_POSE_LEFT },
			{ "/input/aim/pose", EVENT_TYPE::VR_INTERNAL_HAND_AIM_LEFT },
		};
		right_hand_actions = {
			{ "/input/grip/pose", EVENT_TYPE::VR_INTERNAL_HAND_POSE_RIGHT },
			{ "/input/aim/pose", EVENT_TYPE::VR_INTERNAL_HAND_AIM_RIGHT },
		};
		suggest_binding("/interaction_profiles/khr/simple_controller",
						CONTROLLER_TYPE::KHRONOS_SIMPLE,
						both_hand_actions, left_hand_actions, right_hand_actions);
		
		// TODO: /output/haptic
	}
	// Valve Index
	{
		both_hand_actions = {
			{ "/input/system/click", EVENT_TYPE::VR_SYSTEM_PRESS },
			{ "/input/system/touch", EVENT_TYPE::VR_SYSTEM_TOUCH },
			{ "/input/a/click", EVENT_TYPE::VR_MAIN_PRESS },
			{ "/input/a/touch", EVENT_TYPE::VR_MAIN_TOUCH },
			{ "/input/b/click", EVENT_TYPE::VR_APP_MENU_PRESS },
			{ "/input/b/touch", EVENT_TYPE::VR_APP_MENU_TOUCH },
			{ "/input/squeeze/value", EVENT_TYPE::VR_GRIP_PULL },
			{ "/input/squeeze/force", EVENT_TYPE::VR_GRIP_FORCE },
			{ "/input/trigger/click", EVENT_TYPE::VR_TRIGGER_PRESS },
			{ "/input/trigger/value", EVENT_TYPE::VR_TRIGGER_PULL },
			{ "/input/trigger/touch", EVENT_TYPE::VR_TRIGGER_TOUCH },
			{ "/input/thumbstick", EVENT_TYPE::VR_THUMBSTICK_MOVE },
			{ "/input/thumbstick/click", EVENT_TYPE::VR_THUMBSTICK_PRESS },
			{ "/input/thumbstick/touch", EVENT_TYPE::VR_THUMBSTICK_TOUCH },
			{ "/input/trackpad", EVENT_TYPE::VR_TRACKPAD_MOVE },
			{ "/input/trackpad/force", EVENT_TYPE::VR_TRACKPAD_FORCE },
			{ "/input/trackpad/touch", EVENT_TYPE::VR_TRACKPAD_TOUCH },
		};
		left_hand_actions = {
			{ "/input/grip/pose", EVENT_TYPE::VR_INTERNAL_HAND_POSE_LEFT },
			{ "/input/aim/pose", EVENT_TYPE::VR_INTERNAL_HAND_AIM_LEFT },
		};
		right_hand_actions = {
			{ "/input/grip/pose", EVENT_TYPE::VR_INTERNAL_HAND_POSE_RIGHT },
			{ "/input/aim/pose", EVENT_TYPE::VR_INTERNAL_HAND_AIM_RIGHT },
		};
		suggest_binding("/interaction_profiles/valve/index_controller",
						CONTROLLER_TYPE::INDEX,
						both_hand_actions, left_hand_actions, right_hand_actions);
		
		// TODO: /output/haptic
	}
	// HTC Vive
	{
		both_hand_actions = {
			{ "/input/system/click", EVENT_TYPE::VR_SYSTEM_PRESS },
			{ "/input/squeeze/click", EVENT_TYPE::VR_MAIN_PRESS },
			{ "/input/menu/click", EVENT_TYPE::VR_APP_MENU_PRESS },
			{ "/input/trigger/click", EVENT_TYPE::VR_TRIGGER_PRESS },
			{ "/input/trigger/value", EVENT_TYPE::VR_TRIGGER_PULL },
			{ "/input/trackpad", EVENT_TYPE::VR_TRACKPAD_MOVE },
			{ "/input/trackpad/click", EVENT_TYPE::VR_TRACKPAD_PRESS },
			{ "/input/trackpad/touch", EVENT_TYPE::VR_TRACKPAD_TOUCH },
		};
		left_hand_actions = {
			{ "/input/grip/pose", EVENT_TYPE::VR_INTERNAL_HAND_POSE_LEFT },
			{ "/input/aim/pose", EVENT_TYPE::VR_INTERNAL_HAND_AIM_LEFT },
		};
		right_hand_actions = {
			{ "/input/grip/pose", EVENT_TYPE::VR_INTERNAL_HAND_POSE_RIGHT },
			{ "/input/aim/pose", EVENT_TYPE::VR_INTERNAL_HAND_AIM_RIGHT },
		};
		suggest_binding("/interaction_profiles/htc/vive_controller",
						CONTROLLER_TYPE::HTC_VIVE,
						both_hand_actions, left_hand_actions, right_hand_actions);
		
		// TODO: /output/haptic
	}
	// Google Daydream
	{
		both_hand_actions = {
			{ "/input/select/click", EVENT_TYPE::VR_APP_MENU_PRESS },
			{ "/input/trackpad", EVENT_TYPE::VR_TRACKPAD_MOVE },
			// use this as VR_MAIN_PRESS, because it's more important than VR_TRACKPAD_PRESS
			{ "/input/trackpad/click", EVENT_TYPE::VR_MAIN_PRESS },
			{ "/input/trackpad/touch", EVENT_TYPE::VR_TRACKPAD_TOUCH },
		};
		left_hand_actions = {
			{ "/input/grip/pose", EVENT_TYPE::VR_INTERNAL_HAND_POSE_LEFT },
			{ "/input/aim/pose", EVENT_TYPE::VR_INTERNAL_HAND_AIM_LEFT },
		};
		right_hand_actions = {
			{ "/input/grip/pose", EVENT_TYPE::VR_INTERNAL_HAND_POSE_RIGHT },
			{ "/input/aim/pose", EVENT_TYPE::VR_INTERNAL_HAND_AIM_RIGHT },
		};
		suggest_binding("/interaction_profiles/google/daydream_controller",
						CONTROLLER_TYPE::GOOGLE_DAYDREAM,
						both_hand_actions, left_hand_actions, right_hand_actions);
		
		// TODO: /output/haptic
	}
	// Microsoft Mixed Reality Motion
	{
		both_hand_actions = {
			{ "/input/squeeze/click", EVENT_TYPE::VR_MAIN_PRESS },
			{ "/input/menu/click", EVENT_TYPE::VR_APP_MENU_PRESS },
			{ "/input/trigger/value", EVENT_TYPE::VR_TRIGGER_PULL },
			{ "/input/thumbstick", EVENT_TYPE::VR_THUMBSTICK_MOVE },
			{ "/input/thumbstick/click", EVENT_TYPE::VR_THUMBSTICK_PRESS },
			{ "/input/trackpad", EVENT_TYPE::VR_TRACKPAD_MOVE },
			{ "/input/trackpad/click", EVENT_TYPE::VR_TRACKPAD_PRESS },
			{ "/input/trackpad/touch", EVENT_TYPE::VR_TRACKPAD_TOUCH },
		};
		left_hand_actions = {
			{ "/input/grip/pose", EVENT_TYPE::VR_INTERNAL_HAND_POSE_LEFT },
			{ "/input/aim/pose", EVENT_TYPE::VR_INTERNAL_HAND_AIM_LEFT },
		};
		right_hand_actions = {
			{ "/input/grip/pose", EVENT_TYPE::VR_INTERNAL_HAND_POSE_RIGHT },
			{ "/input/aim/pose", EVENT_TYPE::VR_INTERNAL_HAND_AIM_RIGHT },
		};
		suggest_binding("/interaction_profiles/microsoft/motion_controller",
						CONTROLLER_TYPE::MICROSOFT_MIXED_REALITY,
						both_hand_actions, left_hand_actions, right_hand_actions);
		
		// TODO: /output/haptic
	}
	// Oculus Go
	{
		both_hand_actions = {
			{ "/input/system/click", EVENT_TYPE::VR_SYSTEM_PRESS },
			{ "/input/trigger/click", EVENT_TYPE::VR_MAIN_PRESS },
			{ "/input/back/click", EVENT_TYPE::VR_APP_MENU_PRESS },
			{ "/input/trackpad", EVENT_TYPE::VR_TRACKPAD_MOVE },
			{ "/input/trackpad/click", EVENT_TYPE::VR_TRACKPAD_PRESS },
			{ "/input/trackpad/touch", EVENT_TYPE::VR_TRACKPAD_TOUCH },
		};
		left_hand_actions = {
			{ "/input/grip/pose", EVENT_TYPE::VR_INTERNAL_HAND_POSE_LEFT },
			{ "/input/aim/pose", EVENT_TYPE::VR_INTERNAL_HAND_AIM_LEFT },
		};
		right_hand_actions = {
			{ "/input/grip/pose", EVENT_TYPE::VR_INTERNAL_HAND_POSE_RIGHT },
			{ "/input/aim/pose", EVENT_TYPE::VR_INTERNAL_HAND_AIM_RIGHT },
		};
		suggest_binding("/interaction_profiles/oculus/go_controller",
						CONTROLLER_TYPE::OCULUS_GO,
						both_hand_actions, left_hand_actions, right_hand_actions);
		
		// TODO: /output/haptic
	}
	// Oculus Touch
	{
		both_hand_actions = {
			{ "/input/squeeze/value", EVENT_TYPE::VR_GRIP_PULL },
			{ "/input/trigger/value", EVENT_TYPE::VR_TRIGGER_PULL },
			{ "/input/trigger/touch", EVENT_TYPE::VR_TRIGGER_TOUCH },
			{ "/input/thumbstick", EVENT_TYPE::VR_THUMBSTICK_MOVE },
			{ "/input/thumbstick/click", EVENT_TYPE::VR_THUMBSTICK_PRESS },
			{ "/input/thumbstick/touch", EVENT_TYPE::VR_THUMBSTICK_TOUCH },
			{ "/input/thumbrest/touch", EVENT_TYPE::VR_THUMBREST_TOUCH },
		};
		left_hand_actions = {
			{ "/input/grip/pose", EVENT_TYPE::VR_INTERNAL_HAND_POSE_LEFT },
			{ "/input/aim/pose", EVENT_TYPE::VR_INTERNAL_HAND_AIM_LEFT },
			{ "/input/menu/click", EVENT_TYPE::VR_APP_MENU_PRESS },
			{ "/input/x/click", EVENT_TYPE::VR_MAIN_PRESS },
			{ "/input/x/touch", EVENT_TYPE::VR_MAIN_TOUCH },
			// no real good match for this -> just map to trigger
			{ "/input/y/click", EVENT_TYPE::VR_TRIGGER_PRESS },
			{ "/input/y/touch", EVENT_TYPE::VR_TRIGGER_TOUCH },
		};
		right_hand_actions = {
			{ "/input/grip/pose", EVENT_TYPE::VR_INTERNAL_HAND_POSE_RIGHT },
			{ "/input/aim/pose", EVENT_TYPE::VR_INTERNAL_HAND_AIM_RIGHT },
			{ "/input/system/click", EVENT_TYPE::VR_SYSTEM_PRESS },
			{ "/input/a/click", EVENT_TYPE::VR_MAIN_PRESS },
			{ "/input/a/touch", EVENT_TYPE::VR_MAIN_TOUCH },
			// no real good match for this -> just map to trigger
			{ "/input/b/click", EVENT_TYPE::VR_TRIGGER_PRESS },
			{ "/input/b/touch", EVENT_TYPE::VR_TRIGGER_TOUCH },
		};
		suggest_binding("/interaction_profiles/oculus/touch_controller",
						CONTROLLER_TYPE::OCULUS_TOUCH,
						both_hand_actions, left_hand_actions, right_hand_actions);
		
		// TODO: /output/haptic
	}
	if (has_hp_mixed_reality_controller_support) {
		both_hand_actions = {
			{ "/input/menu/click", EVENT_TYPE::VR_APP_MENU_PRESS },
			{ "/input/squeeze/value", EVENT_TYPE::VR_GRIP_PULL },
			{ "/input/trigger/value", EVENT_TYPE::VR_TRIGGER_PULL },
			{ "/input/thumbstick", EVENT_TYPE::VR_THUMBSTICK_MOVE },
			{ "/input/thumbstick/click", EVENT_TYPE::VR_THUMBSTICK_PRESS },
		};
		left_hand_actions = {
			{ "/input/grip/pose", EVENT_TYPE::VR_INTERNAL_HAND_POSE_LEFT },
			{ "/input/aim/pose", EVENT_TYPE::VR_INTERNAL_HAND_AIM_LEFT },
			{ "/input/x/click", EVENT_TYPE::VR_MAIN_PRESS },
			// no real good match for this -> just map to system
			{ "/input/y/click", EVENT_TYPE::VR_SYSTEM_PRESS },
		};
		right_hand_actions = {
			{ "/input/grip/pose", EVENT_TYPE::VR_INTERNAL_HAND_POSE_RIGHT },
			{ "/input/aim/pose", EVENT_TYPE::VR_INTERNAL_HAND_AIM_RIGHT },
			{ "/input/a/click", EVENT_TYPE::VR_MAIN_PRESS },
			// no real good match for this -> just map to trigger
			{ "/input/b/click", EVENT_TYPE::VR_TRIGGER_PRESS },
		};
		suggest_binding("/interaction_profiles/hp/mixed_reality_controller",
						CONTROLLER_TYPE::HP_MIXED_REALITY,
						both_hand_actions, left_hand_actions, right_hand_actions);
		
		// TODO: /output/haptic
	}
	if (has_htc_vive_cosmos_controller_support) {
		both_hand_actions = {
			{ "/input/shoulder/click", EVENT_TYPE::VR_SHOULDER_PRESS },
			{ "/input/squeeze/click", EVENT_TYPE::VR_GRIP_PRESS },
			{ "/input/trigger/click", EVENT_TYPE::VR_TRIGGER_PRESS },
			{ "/input/trigger/value", EVENT_TYPE::VR_TRIGGER_PULL },
			{ "/input/thumbstick", EVENT_TYPE::VR_THUMBSTICK_MOVE },
			{ "/input/thumbstick/click", EVENT_TYPE::VR_THUMBSTICK_PRESS },
			{ "/input/thumbstick/touch", EVENT_TYPE::VR_THUMBSTICK_TOUCH },
		};
		left_hand_actions = {
			{ "/input/grip/pose", EVENT_TYPE::VR_INTERNAL_HAND_POSE_LEFT },
			{ "/input/aim/pose", EVENT_TYPE::VR_INTERNAL_HAND_AIM_LEFT },
			{ "/input/menu/click", EVENT_TYPE::VR_APP_MENU_PRESS },
			{ "/input/x/click", EVENT_TYPE::VR_MAIN_PRESS },
			// no real good match for this -> just map to trigger
			{ "/input/y/click", EVENT_TYPE::VR_TRIGGER_PRESS },
		};
		right_hand_actions = {
			{ "/input/grip/pose", EVENT_TYPE::VR_INTERNAL_HAND_POSE_RIGHT },
			{ "/input/aim/pose", EVENT_TYPE::VR_INTERNAL_HAND_AIM_RIGHT },
			{ "/input/system/click", EVENT_TYPE::VR_SYSTEM_PRESS },
			{ "/input/a/click", EVENT_TYPE::VR_MAIN_PRESS },
			// no real good match for this -> just map to trigger
			{ "/input/b/click", EVENT_TYPE::VR_TRIGGER_PRESS },
		};
		suggest_binding("/interaction_profiles/htc/vive_cosmos_controller",
						CONTROLLER_TYPE::HTC_VIVE_COSMOS,
						both_hand_actions, left_hand_actions, right_hand_actions);
		
		// TODO: /output/haptic
	}
	if (has_htc_vive_focus3_controller_support) {
		both_hand_actions = {
			{ "/input/squeeze/click", EVENT_TYPE::VR_GRIP_PRESS },
			{ "/input/squeeze/value", EVENT_TYPE::VR_GRIP_PULL },
			{ "/input/squeeze/touch", EVENT_TYPE::VR_GRIP_TOUCH },
			{ "/input/trigger/click", EVENT_TYPE::VR_TRIGGER_PRESS },
			{ "/input/trigger/value", EVENT_TYPE::VR_TRIGGER_PULL },
			{ "/input/trigger/touch", EVENT_TYPE::VR_TRIGGER_TOUCH },
			{ "/input/thumbstick", EVENT_TYPE::VR_THUMBSTICK_MOVE },
			{ "/input/thumbstick/click", EVENT_TYPE::VR_THUMBSTICK_PRESS },
			{ "/input/thumbstick/touch", EVENT_TYPE::VR_THUMBSTICK_TOUCH },
			{ "/input/thumbrest/touch", EVENT_TYPE::VR_THUMBREST_TOUCH },
		};
		left_hand_actions = {
			{ "/input/grip/pose", EVENT_TYPE::VR_INTERNAL_HAND_POSE_LEFT },
			{ "/input/aim/pose", EVENT_TYPE::VR_INTERNAL_HAND_AIM_LEFT },
			{ "/input/menu/click", EVENT_TYPE::VR_APP_MENU_PRESS },
			{ "/input/x/click", EVENT_TYPE::VR_MAIN_PRESS },
			// no real good match for this -> just map to trigger
			{ "/input/y/click", EVENT_TYPE::VR_TRIGGER_PRESS },
		};
		right_hand_actions = {
			{ "/input/grip/pose", EVENT_TYPE::VR_INTERNAL_HAND_POSE_RIGHT },
			{ "/input/aim/pose", EVENT_TYPE::VR_INTERNAL_HAND_AIM_RIGHT },
			{ "/input/system/click", EVENT_TYPE::VR_SYSTEM_PRESS },
			{ "/input/a/click", EVENT_TYPE::VR_MAIN_PRESS },
			// no real good match for this -> just map to trigger
			{ "/input/b/click", EVENT_TYPE::VR_TRIGGER_PRESS },
		};
		suggest_binding("/interaction_profiles/htc/vive_focus3_controller",
						CONTROLLER_TYPE::HTC_VIVE_FOCUS3,
						both_hand_actions, left_hand_actions, right_hand_actions);
		
		// TODO: /output/haptic
	}
	if (has_huawei_controller_support) {
		both_hand_actions = {
			{ "/input/home/click", EVENT_TYPE::VR_MAIN_PRESS },
			{ "/input/back/click", EVENT_TYPE::VR_APP_MENU_PRESS },
			{ "/input/trigger/click", EVENT_TYPE::VR_TRIGGER_PRESS },
			{ "/input/trigger/value", EVENT_TYPE::VR_TRIGGER_PULL },
			{ "/input/trackpad", EVENT_TYPE::VR_TRACKPAD_MOVE },
			{ "/input/trackpad/click", EVENT_TYPE::VR_TRACKPAD_PRESS },
			{ "/input/trackpad/touch", EVENT_TYPE::VR_TRACKPAD_TOUCH },
		};
		left_hand_actions = {
			{ "/input/grip/pose", EVENT_TYPE::VR_INTERNAL_HAND_POSE_LEFT },
			{ "/input/aim/pose", EVENT_TYPE::VR_INTERNAL_HAND_AIM_LEFT },
		};
		right_hand_actions = {
			{ "/input/grip/pose", EVENT_TYPE::VR_INTERNAL_HAND_POSE_RIGHT },
			{ "/input/aim/pose", EVENT_TYPE::VR_INTERNAL_HAND_AIM_RIGHT },
		};
		suggest_binding("/interaction_profiles/huawei/controller",
						CONTROLLER_TYPE::HUAWEI,
						both_hand_actions, left_hand_actions, right_hand_actions);
		
		// TODO: /output/haptic
	}
	if (has_samsung_odyssey_controller_support) {
		// NOTE: same as Microsoft Mixed Reality Motion controller
		both_hand_actions = {
			{ "/input/squeeze/click", EVENT_TYPE::VR_MAIN_PRESS },
			{ "/input/menu/click", EVENT_TYPE::VR_APP_MENU_PRESS },
			{ "/input/trigger/value", EVENT_TYPE::VR_TRIGGER_PULL },
			{ "/input/thumbstick", EVENT_TYPE::VR_THUMBSTICK_MOVE },
			{ "/input/thumbstick/click", EVENT_TYPE::VR_THUMBSTICK_PRESS },
			{ "/input/trackpad", EVENT_TYPE::VR_TRACKPAD_MOVE },
			{ "/input/trackpad/click", EVENT_TYPE::VR_TRACKPAD_PRESS },
			{ "/input/trackpad/touch", EVENT_TYPE::VR_TRACKPAD_TOUCH },
		};
		left_hand_actions = {
			{ "/input/grip/pose", EVENT_TYPE::VR_INTERNAL_HAND_POSE_LEFT },
			{ "/input/aim/pose", EVENT_TYPE::VR_INTERNAL_HAND_AIM_LEFT },
		};
		right_hand_actions = {
			{ "/input/grip/pose", EVENT_TYPE::VR_INTERNAL_HAND_POSE_RIGHT },
			{ "/input/aim/pose", EVENT_TYPE::VR_INTERNAL_HAND_AIM_RIGHT },
		};
		suggest_binding("/interaction_profiles/samsung/odyssey_controller",
						CONTROLLER_TYPE::SAMSUNG_ODYSSEY,
						both_hand_actions, left_hand_actions, right_hand_actions);
		
		// TODO: /output/haptic
	}
	if (has_ml2_controller_support) {
		both_hand_actions = {
			{ "/input/home/click", EVENT_TYPE::VR_SYSTEM_PRESS },
			{ "/input/menu/click", EVENT_TYPE::VR_MAIN_PRESS },
			{ "/input/trigger/click", EVENT_TYPE::VR_TRIGGER_PRESS },
			{ "/input/trigger/value", EVENT_TYPE::VR_TRIGGER_PULL },
			{ "/input/thumbstick", EVENT_TYPE::VR_THUMBSTICK_MOVE },
			{ "/input/thumbstick/click", EVENT_TYPE::VR_THUMBSTICK_PRESS },
			{ "/input/thumbstick/touch", EVENT_TYPE::VR_THUMBSTICK_TOUCH },
			{ "/input/trackpad", EVENT_TYPE::VR_TRACKPAD_MOVE },
			{ "/input/trackpad/click", EVENT_TYPE::VR_TRACKPAD_PRESS },
			{ "/input/trackpad/force", EVENT_TYPE::VR_TRACKPAD_FORCE },
			{ "/input/trackpad/touch", EVENT_TYPE::VR_TRACKPAD_TOUCH },
			// use VR_APP_MENU_PRESS instead of VR_SHOULDER_PRESS, since it's more important
			{ "/input/shoulder/click", EVENT_TYPE::VR_APP_MENU_PRESS },
		};
		left_hand_actions = {
			{ "/input/grip/pose", EVENT_TYPE::VR_INTERNAL_HAND_POSE_LEFT },
			{ "/input/aim/pose", EVENT_TYPE::VR_INTERNAL_HAND_AIM_LEFT },
		};
		right_hand_actions = {
			{ "/input/grip/pose", EVENT_TYPE::VR_INTERNAL_HAND_POSE_RIGHT },
			{ "/input/aim/pose", EVENT_TYPE::VR_INTERNAL_HAND_AIM_RIGHT },
		};
		suggest_binding("/interaction_profiles/ml/ml2_controller",
						CONTROLLER_TYPE::MAGIC_LEAP2,
						both_hand_actions, left_hand_actions, right_hand_actions);
		
		// TODO: /output/haptic
	}
	if (has_fb_touch_controller_pro_support) {
		both_hand_actions = {
			{ "/input/squeeze/value", EVENT_TYPE::VR_GRIP_PULL },
			{ "/input/trigger/value", EVENT_TYPE::VR_TRIGGER_PULL },
			{ "/input/trigger/touch", EVENT_TYPE::VR_TRIGGER_TOUCH },
			{ "/input/thumbstick", EVENT_TYPE::VR_THUMBSTICK_MOVE },
			{ "/input/thumbstick/click", EVENT_TYPE::VR_THUMBSTICK_PRESS },
			{ "/input/thumbstick/touch", EVENT_TYPE::VR_THUMBSTICK_TOUCH },
			{ "/input/thumbrest/touch", EVENT_TYPE::VR_THUMBREST_TOUCH },
			{ "/input/thumbrest/force", EVENT_TYPE::VR_THUMBREST_FORCE },
		};
		left_hand_actions = {
			{ "/input/grip/pose", EVENT_TYPE::VR_INTERNAL_HAND_POSE_LEFT },
			{ "/input/aim/pose", EVENT_TYPE::VR_INTERNAL_HAND_AIM_LEFT },
			{ "/input/menu/click", EVENT_TYPE::VR_APP_MENU_PRESS },
			{ "/input/x/click", EVENT_TYPE::VR_MAIN_PRESS },
			{ "/input/x/touch", EVENT_TYPE::VR_MAIN_TOUCH },
			// no real good match for this -> just map to trigger
			{ "/input/y/click", EVENT_TYPE::VR_TRIGGER_PRESS },
			{ "/input/y/touch", EVENT_TYPE::VR_TRIGGER_TOUCH },
		};
		right_hand_actions = {
			{ "/input/grip/pose", EVENT_TYPE::VR_INTERNAL_HAND_POSE_RIGHT },
			{ "/input/aim/pose", EVENT_TYPE::VR_INTERNAL_HAND_AIM_RIGHT },
			{ "/input/system/click", EVENT_TYPE::VR_SYSTEM_PRESS },
			{ "/input/a/click", EVENT_TYPE::VR_MAIN_PRESS },
			{ "/input/a/touch", EVENT_TYPE::VR_MAIN_TOUCH },
			// no real good match for this -> just map to trigger
			{ "/input/b/click", EVENT_TYPE::VR_TRIGGER_PRESS },
			{ "/input/b/touch", EVENT_TYPE::VR_TRIGGER_TOUCH },
		};
		suggest_binding("/interaction_profiles/oculus/touch_controller",
						CONTROLLER_TYPE::OCULUS_TOUCH_PRO,
						both_hand_actions, left_hand_actions, right_hand_actions);
		
		// TODO: /output/haptic
		// TODO: /input/stylus_fb/*, /input/trigger/*_fb, input/thumb_fb/proximity_fb, /output/haptic_*_fb
	}
	if (has_bd_controller_support) {
		// enables both PICO Neo3 and PICO 4 controller support
		{
			both_hand_actions = {
				{ "/input/system/click", EVENT_TYPE::VR_SYSTEM_PRESS },
				{ "/input/menu/click", EVENT_TYPE::VR_APP_MENU_PRESS },
				{ "/input/squeeze/click", EVENT_TYPE::VR_GRIP_PRESS },
				{ "/input/squeeze/value", EVENT_TYPE::VR_GRIP_PULL },
				{ "/input/trigger/click", EVENT_TYPE::VR_TRIGGER_PRESS },
				{ "/input/trigger/value", EVENT_TYPE::VR_TRIGGER_PULL },
				{ "/input/trigger/touch", EVENT_TYPE::VR_TRIGGER_TOUCH },
				{ "/input/thumbstick", EVENT_TYPE::VR_THUMBSTICK_MOVE },
				{ "/input/thumbstick/click", EVENT_TYPE::VR_THUMBSTICK_PRESS },
				{ "/input/thumbstick/touch", EVENT_TYPE::VR_THUMBSTICK_TOUCH },
			};
			left_hand_actions = {
				{ "/input/grip/pose", EVENT_TYPE::VR_INTERNAL_HAND_POSE_LEFT },
				{ "/input/aim/pose", EVENT_TYPE::VR_INTERNAL_HAND_AIM_LEFT },
				{ "/input/x/click", EVENT_TYPE::VR_MAIN_PRESS },
				{ "/input/x/touch", EVENT_TYPE::VR_MAIN_TOUCH },
				// no real good match for this -> just map to trigger
				{ "/input/y/click", EVENT_TYPE::VR_TRIGGER_PRESS },
				{ "/input/y/touch", EVENT_TYPE::VR_TRIGGER_TOUCH },
			};
			right_hand_actions = {
				{ "/input/grip/pose", EVENT_TYPE::VR_INTERNAL_HAND_POSE_RIGHT },
				{ "/input/aim/pose", EVENT_TYPE::VR_INTERNAL_HAND_AIM_RIGHT },
				{ "/input/a/click", EVENT_TYPE::VR_MAIN_PRESS },
				{ "/input/a/touch", EVENT_TYPE::VR_MAIN_TOUCH },
				// no real good match for this -> just map to trigger
				{ "/input/b/click", EVENT_TYPE::VR_TRIGGER_PRESS },
				{ "/input/b/touch", EVENT_TYPE::VR_TRIGGER_TOUCH },
			};
			suggest_binding("/interaction_profiles/bytedance/pico_neo3_controller",
							CONTROLLER_TYPE::PICO_NEO3,
							both_hand_actions, left_hand_actions, right_hand_actions);
			
			// TODO: /output/haptic
		}
		{
			both_hand_actions = {
				{ "/input/system/click", EVENT_TYPE::VR_SYSTEM_PRESS },
				{ "/input/squeeze/click", EVENT_TYPE::VR_GRIP_PRESS },
				{ "/input/squeeze/value", EVENT_TYPE::VR_GRIP_PULL },
				{ "/input/trigger/click", EVENT_TYPE::VR_TRIGGER_PRESS },
				{ "/input/trigger/value", EVENT_TYPE::VR_TRIGGER_PULL },
				{ "/input/trigger/touch", EVENT_TYPE::VR_TRIGGER_TOUCH },
				{ "/input/thumbstick", EVENT_TYPE::VR_THUMBSTICK_MOVE },
				{ "/input/thumbstick/click", EVENT_TYPE::VR_THUMBSTICK_PRESS },
				{ "/input/thumbstick/touch", EVENT_TYPE::VR_THUMBSTICK_TOUCH },
			};
			left_hand_actions = {
				{ "/input/grip/pose", EVENT_TYPE::VR_INTERNAL_HAND_POSE_LEFT },
				{ "/input/aim/pose", EVENT_TYPE::VR_INTERNAL_HAND_AIM_LEFT },
				{ "/input/menu/click", EVENT_TYPE::VR_APP_MENU_PRESS },
				{ "/input/x/click", EVENT_TYPE::VR_MAIN_PRESS },
				{ "/input/x/touch", EVENT_TYPE::VR_MAIN_TOUCH },
				// no real good match for this -> just map to trigger
				{ "/input/y/click", EVENT_TYPE::VR_TRIGGER_PRESS },
				{ "/input/y/touch", EVENT_TYPE::VR_TRIGGER_TOUCH },
			};
			right_hand_actions = {
				{ "/input/grip/pose", EVENT_TYPE::VR_INTERNAL_HAND_POSE_RIGHT },
				{ "/input/aim/pose", EVENT_TYPE::VR_INTERNAL_HAND_AIM_RIGHT },
				{ "/input/a/click", EVENT_TYPE::VR_MAIN_PRESS },
				{ "/input/a/touch", EVENT_TYPE::VR_MAIN_TOUCH },
				// no real good match for this -> just map to trigger
				{ "/input/b/click", EVENT_TYPE::VR_TRIGGER_PRESS },
				{ "/input/b/touch", EVENT_TYPE::VR_TRIGGER_TOUCH },
			};
			suggest_binding("/interaction_profiles/bytedance/pico4_controller",
							CONTROLLER_TYPE::PICO4,
							both_hand_actions, left_hand_actions, right_hand_actions);
			
			// TODO: /output/haptic
		}
	}
	
	// tracker init
	if (has_tracker_interaction_support) {
		if (create_tracker_actions_and_spaces()) {
			if (!tracker_enumerate()) {
				log_warn("tracker enumeration failed - disabling now");
				has_tracker_interaction_support = false;
			}
		} else {
			log_warn("tracker action/space creation failed - disabling now");
			has_tracker_interaction_support = false;
		}
	}
	
	// hand-tracking init
	if (has_hand_tracking_support) {
		if (!hand_tracking_setup()) {
			log_warn("hand-tracking setup failed - disabling now");
			has_hand_tracking_support = false;
			has_hand_tracking_forearm_support = false;
		}
	}
	
	// finally: attach
	std::vector<XrActionSet> attach_action_sets {
		input_action_set,
	};
	if (has_tracker_interaction_support && tracker_input_action_set) {
		attach_action_sets.emplace_back(tracker_input_action_set);
	}
	const XrSessionActionSetsAttachInfo attach_info {
		.type = XR_TYPE_SESSION_ACTION_SETS_ATTACH_INFO,
		.next = nullptr,
		.countActionSets = uint32_t(attach_action_sets.size()),
		.actionSets = attach_action_sets.data(),
	};
	XR_CALL_RET(xrAttachSessionActionSets(session, &attach_info),
				"failed to attach session action sets", false);
	
	// query/update which controller types are actually used
	update_hand_controller_types();
	
	return true;
}

bool openxr_context::create_tracker_actions_and_spaces() {
	// create role paths
	for (uint32_t i = 0; i < tracker_role_count; ++i) {
		const auto role_str = tracker_role_to_string(POSE_TYPE(uint32_t(POSE_TYPE::TRACKER_HANDHELD_OBJECT) + i));
		const std::string role_path_str = "/user/vive_tracker_htcx/role/" + role_str;
		tracker_role_paths[i] = to_path_or_throw(role_path_str);
	}
	
	// create separate action set
	const XrActionSetCreateInfo action_set_create_info {
		.type = XR_TYPE_ACTION_SET_CREATE_INFO,
		.next = nullptr,
		.actionSetName = "vr_tracker_default",
		.localizedActionSetName = "vr_tracker_default",
		.priority = 0,
	};
	XR_CALL_RET(xrCreateActionSet(instance, &action_set_create_info, &tracker_input_action_set),
				"failed to create tracker input action set", false);
	
	for (uint32_t i = 0; i < tracker_role_count; ++i) {
		const auto role_str = tracker_role_to_string(POSE_TYPE(uint32_t(POSE_TYPE::TRACKER_HANDHELD_OBJECT) + i));
		XrActionCreateInfo action_create_info{
			.type = XR_TYPE_ACTION_CREATE_INFO,
			.next = nullptr,
			.actionType = XR_ACTION_TYPE_POSE_INPUT,
			.countSubactionPaths = 1u,
			.subactionPaths = &tracker_role_paths[i],
		};
		const auto tracker_action_name = "tracker_" + role_str;
		strcpy_s(action_create_info.actionName, std::size(action_create_info.actionName), tracker_action_name.c_str());
		strcpy_s(action_create_info.localizedActionName, std::size(action_create_info.localizedActionName), tracker_action_name.c_str());
		XR_CALL_RET(xrCreateAction(tracker_input_action_set, &action_create_info, &tracker_pose_actions[i]),
					"failed to create tracker " + role_str + " pose action", false);
		
		XrActionSpaceCreateInfo tracker_space_create_info {
			.type = XR_TYPE_ACTION_SPACE_CREATE_INFO,
			.next = nullptr,
			.action = tracker_pose_actions[i],
			.subactionPath = tracker_role_paths[i],
			.poseInActionSpace = {
				.orientation = { 0.0f, 0.0f, 0.0f, 1.0f },
				.position = { 0.0f, 0.0f, 0.0f },
			},
		};
		XR_CALL_RET(xrCreateActionSpace(session, &tracker_space_create_info, &tracker_spaces[i]),
					"failed to create tracker " + role_str + " action space", false);
		
		tracker_actions.emplace(EVENT_TYPE(uint32_t(EVENT_TYPE::VR_INTERNAL_TRACKER_HANDHELD_OBJECT) + i),
								action_t {
			.action = tracker_pose_actions[i],
			.input_type = INPUT_TYPE::TRACKER,
			.action_type = ACTION_TYPE::POSE,
		});
	}
	
	// create tracker bindings
	const auto interaction_profile = "/interaction_profiles/htc/vive_tracker_htcx";
	std::vector<action_definition_t> tracker_action_definitions {
		{ "/input/grip/pose", EVENT_TYPE::VR_INTERNAL_TRACKER_HANDHELD_OBJECT },
		{ "/input/grip/pose", EVENT_TYPE::VR_INTERNAL_TRACKER_FOOT_LEFT },
		{ "/input/grip/pose", EVENT_TYPE::VR_INTERNAL_TRACKER_FOOT_RIGHT },
		{ "/input/grip/pose", EVENT_TYPE::VR_INTERNAL_TRACKER_SHOULDER_LEFT },
		{ "/input/grip/pose", EVENT_TYPE::VR_INTERNAL_TRACKER_SHOULDER_RIGHT },
		{ "/input/grip/pose", EVENT_TYPE::VR_INTERNAL_TRACKER_ELBOW_LEFT },
		{ "/input/grip/pose", EVENT_TYPE::VR_INTERNAL_TRACKER_ELBOW_RIGHT },
		{ "/input/grip/pose", EVENT_TYPE::VR_INTERNAL_TRACKER_KNEE_LEFT },
		{ "/input/grip/pose", EVENT_TYPE::VR_INTERNAL_TRACKER_KNEE_RIGHT },
		{ "/input/grip/pose", EVENT_TYPE::VR_INTERNAL_TRACKER_WAIST },
		{ "/input/grip/pose", EVENT_TYPE::VR_INTERNAL_TRACKER_CHEST },
		{ "/input/grip/pose", EVENT_TYPE::VR_INTERNAL_TRACKER_CAMERA },
		{ "/input/grip/pose", EVENT_TYPE::VR_INTERNAL_TRACKER_KEYBOARD },
#if 0 // disabled until this is fixed
		{ "/input/grip/pose", EVENT_TYPE::VR_INTERNAL_TRACKER_WRIST_LEFT },
		{ "/input/grip/pose", EVENT_TYPE::VR_INTERNAL_TRACKER_WRIST_RIGHT },
		{ "/input/grip/pose", EVENT_TYPE::VR_INTERNAL_TRACKER_ANKLE_LEFT },
		{ "/input/grip/pose", EVENT_TYPE::VR_INTERNAL_TRACKER_ANKLE_RIGHT },
#endif
	};
	std::vector<XrActionSuggestedBinding> bindings;
	for (const auto& tracker_action : tracker_action_definitions) {
		const auto role_str = tracker_role_to_string(POSE_TYPE(uint32_t(tracker_action.event_type) -
															   uint32_t(EVENT_TYPE::VR_INTERNAL_TRACKER_HANDHELD_OBJECT) +
															   uint32_t(POSE_TYPE::TRACKER_HANDHELD_OBJECT)));
		const auto role_path = "/user/vive_tracker_htcx/role/" + role_str;
		bindings.emplace_back(XrActionSuggestedBinding {
			.action = tracker_actions.at(tracker_action.event_type).action,
			.binding = to_path_or_throw(role_path + tracker_action.path),
		});
	}
	const XrInteractionProfileSuggestedBinding suggested_binding {
		.type = XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING,
		.next = nullptr,
		.interactionProfile = to_path_or_throw(interaction_profile),
		.countSuggestedBindings = uint32_t(bindings.size()),
		.suggestedBindings = bindings.data(),
	};
	XR_CALL_RET(xrSuggestInteractionProfileBindings(instance, &suggested_binding),
				"failed to set suggested interaction profile for trackers", false);
	
	return true;
}

bool openxr_context::tracker_enumerate() {
	uint32_t path_count = 0;
	XR_CALL_RET(internal->EnumerateViveTrackerPaths(instance, 0, &path_count, nullptr),
				"failed to enumerate tracker paths count", false);
	if (path_count == 0) {
		log_msg("OpenXR: no trackers connected");
		return true; // nothing to do here
	}
	
	std::vector<XrViveTrackerPathsHTCX> tracker_paths(path_count, { .type = XR_TYPE_VIVE_TRACKER_PATHS_HTCX, .next = nullptr });
	XR_CALL_RET(internal->EnumerateViveTrackerPaths(instance, path_count, &path_count, tracker_paths.data()),
				"failed to enumerate tracker paths", false);
	
	// -> we have at least one tracker at this point
	for (const auto& tracker_path : tracker_paths) {
		const auto pers_path = path_to_string(tracker_path.persistentPath);
		const auto role = path_to_string(tracker_path.rolePath);
		if (!pers_path || !role) {
			log_error("failed to convert tracker path");
			continue;
		}
		log_msg("OpenXR: tracker: $ -> $", *pers_path, *role);
	}
	
	// call xrGetCurrentInteractionProfile to query current profile + possibly enable tracker
	for (uint32_t tracker_idx = 0; tracker_idx < tracker_role_count; ++tracker_idx) {
		const auto& tracker_role_path = tracker_role_paths[tracker_idx];
		if (!tracker_role_path) {
			continue;
		}
		
		// query current interaction profile
		XrInteractionProfileState profile{
			.type = XR_TYPE_INTERACTION_PROFILE_STATE,
			.next = nullptr,
		};
		XR_CALL_CONT(xrGetCurrentInteractionProfile(session, tracker_role_path, &profile),
					 "failed to get current interaction profile for tracker " +
					 tracker_role_to_string(POSE_TYPE(tracker_idx + uint32_t(POSE_TYPE::TRACKER_HANDHELD_OBJECT))));
		// NOTE/TODO: do anything with this? really just need to query this to poke trackers
	}
	
	return true;
}

bool openxr_context::hand_tracking_setup() {
	XR_CALL_RET(xrGetInstanceProcAddr(instance, "xrCreateHandTrackerEXT",
									  reinterpret_cast<PFN_xrVoidFunction*>(&internal->CreateHandTracker)),
				"failed to query xrCreateHandTrackerEXT function pointer", false);
	XR_CALL_RET(xrGetInstanceProcAddr(instance, "xrDestroyHandTrackerEXT",
									  reinterpret_cast<PFN_xrVoidFunction*>(&internal->DestroyHandTracker)),
				"failed to query xrDestroyHandTrackerEXT function pointer", false);
	XR_CALL_RET(xrGetInstanceProcAddr(instance, "xrLocateHandJointsEXT",
									  reinterpret_cast<PFN_xrVoidFunction*>(&internal->LocateHandJoints)),
				"failed to query xrLocateHandJointsEXT function pointer", false);
	
	for (uint32_t hand_idx = 0; hand_idx < 2; ++hand_idx) {
		const XrHandTrackerCreateInfoEXT create_info{
			.type = XR_TYPE_HAND_TRACKER_CREATE_INFO_EXT,
			.next = nullptr,
			.hand = (hand_idx == 0 ? XR_HAND_LEFT_EXT : XR_HAND_RIGHT_EXT),
			.handJointSet = (!has_hand_tracking_forearm_support ?
							 XR_HAND_JOINT_SET_DEFAULT_EXT : XR_HAND_JOINT_SET_HAND_WITH_FOREARM_ULTRALEAP),
		};
		XR_CALL_RET(internal->CreateHandTracker(session, &create_info, &hand_trackers[hand_idx]),
					"failed to create hand tracker", false);
	}
	
	return true;
}

void openxr_context::update_hand_controller_types() {
	// query interaction profile and update controller type of each hand
	for (size_t hand_idx = 0; hand_idx < hand_paths.size(); ++hand_idx) {
		const auto& hand_path = hand_paths[hand_idx];
		if (!hand_path) {
			continue;
		}
		
		// query current interaction profile (-> controller type)
		XrInteractionProfileState profile{
			.type = XR_TYPE_INTERACTION_PROFILE_STATE,
			.next = nullptr,
		};
		XR_CALL_BREAK(xrGetCurrentInteractionProfile(session, hand_path, &profile),
					  "failed to get current interaction profile for "s + (hand_idx == 0 ? "left" : "right") + " hand");
		
		const auto profile_str = path_to_string(profile.interactionProfile);
		if (!profile_str) {
			// fallback
			log_warn("interaction profile path for $ hand is invalid", (hand_idx == 0 ? "left" : "right"));
			hand_controller_types[hand_idx] = CONTROLLER_TYPE::NONE;
			continue;
		}
		
		const auto type_iter = interaction_profile_controller_lut.find(*profile_str);
		if (type_iter == interaction_profile_controller_lut.end()) {
			log_warn("unknown/unhandled interaction profile for $ hand: $",
					 (hand_idx == 0 ? "left" : "right"), *profile_str);
			hand_controller_types[hand_idx] = CONTROLLER_TYPE::NONE;
			continue;
		}
		hand_controller_types[hand_idx] = type_iter->second;
	}
	
	for (size_t hand_idx = 0; hand_idx < hand_paths.size(); ++hand_idx) {
		log_msg("OpenXR: now using $ hand controller: $", hand_idx == 0 ? "left" : "right",
				controller_type_to_string(hand_controller_types[hand_idx]));
	}
	
	// update required input emulation for each hand/controller
	for (size_t hand_idx = 0; hand_idx < hand_paths.size(); ++hand_idx) {
		hand_input_emulation[hand_idx] = controller_input_emulation_lut[size_t(hand_controller_types[hand_idx])];
	}
}

uint64_t openxr_context::convert_time_to_ticks(XrTime time) {
#if defined(__WINDOWS__)
	int64_t perf_counter = 0;
	XR_CALL_RET(internal->ConvertTimeToWin32PerformanceCounterKHR(instance, time, &perf_counter),
				"failed to convert OpenXR time to Win32 perf counter", SDL_GetTicks());
	const auto perf_since_start = uint64_t(perf_counter) - internal->win_start_perf_counter;
	return (perf_since_start * 1000ull) / internal->win_perf_counter_freq;
#elif defined(__linux__)
	timespec timespec_time {};
	XR_CALL_RET(internal->ConvertTimeToTimespecTimeKHR(instance, time, &timespec_time),
				"failed to convert OpenXR time to timespec time", SDL_GetTicks());
	const auto time_in_ns = uint64_t(timespec_time.tv_sec) * internal->unix_perf_counter_freq + uint64_t(timespec_time.tv_nsec);
	const auto time_since_start_in_ns = time_in_ns - internal->unix_start_time;
	return time_since_start_in_ns / 1'000'000ull; // ns -> ms
#else
#error "unsupported OS"
#endif
}

XrTime openxr_context::convert_perf_counter_to_time(const uint64_t perf_counter) {
	XrTime ret = 0;
#if defined(__WINDOWS__)
	XR_CALL_RET(internal->ConvertWin32PerformanceCounterToTimeKHR(instance, (const int64_t*)&perf_counter, &ret),
				"failed to convert performance counter to OpenXR time", 0);
#elif defined(__linux__)
	const timespec timespec_time {
		.tv_sec = int64_t(perf_counter / internal->unix_perf_counter_freq),
		.tv_nsec = perf_counter % internal->unix_perf_counter_freq,
	};
	XR_CALL_RET(internal->ConvertTimespecTimeToTimeKHR(instance, &timespec_time, &ret),
				"failed to convert performance counter to OpenXR time", 0);
#else
#error "unsupported OS"
#endif
	return ret;
}

void openxr_context::add_hand_bool_event(std::vector<std::shared_ptr<event_object>>& events, const EVENT_TYPE event_type,
										 const XrActionStateBoolean& state, const bool side) {
	const auto cur_time = convert_time_to_ticks(state.lastChangeTime);
	switch (event_type) {
		case EVENT_TYPE::VR_SYSTEM_PRESS:
			events.emplace_back(std::make_shared<vr_system_press_event>(cur_time, side, state.currentState));
			break;
		case EVENT_TYPE::VR_SYSTEM_TOUCH:
			events.emplace_back(std::make_shared<vr_system_touch_event>(cur_time, side, state.currentState));
			break;
		case EVENT_TYPE::VR_MAIN_PRESS:
			events.emplace_back(std::make_shared<vr_main_press_event>(cur_time, side, state.currentState));
			break;
		case EVENT_TYPE::VR_MAIN_TOUCH:
			events.emplace_back(std::make_shared<vr_main_touch_event>(cur_time, side, state.currentState));
			break;
		case EVENT_TYPE::VR_APP_MENU_PRESS:
			events.emplace_back(std::make_shared<vr_app_menu_press_event>(cur_time, side, state.currentState));
			break;
		case EVENT_TYPE::VR_APP_MENU_TOUCH:
			events.emplace_back(std::make_shared<vr_app_menu_touch_event>(cur_time, side, state.currentState));
			break;
		case EVENT_TYPE::VR_GRIP_PRESS:
			events.emplace_back(std::make_shared<vr_grip_press_event>(cur_time, side, state.currentState));
			break;
		case EVENT_TYPE::VR_GRIP_TOUCH:
			events.emplace_back(std::make_shared<vr_grip_touch_event>(cur_time, side, state.currentState));
			break;
		case EVENT_TYPE::VR_TRIGGER_PRESS:
			events.emplace_back(std::make_shared<vr_trigger_press_event>(cur_time, side, state.currentState));
			break;
		case EVENT_TYPE::VR_TRIGGER_TOUCH:
			events.emplace_back(std::make_shared<vr_trigger_touch_event>(cur_time, side, state.currentState));
			break;
		case EVENT_TYPE::VR_THUMBSTICK_PRESS:
			events.emplace_back(std::make_shared<vr_thumbstick_press_event>(cur_time, side, state.currentState));
			break;
		case EVENT_TYPE::VR_THUMBSTICK_TOUCH:
			events.emplace_back(std::make_shared<vr_thumbstick_touch_event>(cur_time, side, state.currentState));
			break;
		case EVENT_TYPE::VR_TRACKPAD_PRESS:
			events.emplace_back(std::make_shared<vr_trackpad_press_event>(cur_time, side, state.currentState));
			break;
		case EVENT_TYPE::VR_TRACKPAD_TOUCH:
			events.emplace_back(std::make_shared<vr_trackpad_touch_event>(cur_time, side, state.currentState));
			break;
		case EVENT_TYPE::VR_THUMBREST_TOUCH:
			events.emplace_back(std::make_shared<vr_thumbrest_touch_event>(cur_time, side, state.currentState));
			break;
		case EVENT_TYPE::VR_SHOULDER_PRESS:
			events.emplace_back(std::make_shared<vr_shoulder_press_event>(cur_time, side, state.currentState));
			break;
		default:
			log_error("unknown/unhandled VR event: $", event_type);
			break;
	}
}

void openxr_context::add_hand_float_event(std::vector<std::shared_ptr<event_object>>& events, const EVENT_TYPE event_type,
										  const XrActionStateFloat& state, const bool side) {
	const auto cur_time = convert_time_to_ticks(state.lastChangeTime);
	const auto cur_state = state.currentState;
	auto& prev_state = hand_event_states[!side ? 0 : 1][event_type];
	const auto delta = cur_state - prev_state.f;
	const auto& emulate = hand_input_emulation[!side ? 0 : 1];
	switch (event_type) {
		case EVENT_TYPE::VR_TRIGGER_PULL:
			events.emplace_back(std::make_shared<vr_trigger_pull_event>(cur_time, side, state.currentState, delta));
			if (emulate.trigger_press) {
				if (prev_state.f < emulation_trigger_force && cur_state >= emulation_trigger_force) {
					events.emplace_back(std::make_shared<vr_trigger_press_event>(cur_time, side, true));
				} else if (prev_state.f >= emulation_trigger_force && cur_state < emulation_trigger_force) {
					events.emplace_back(std::make_shared<vr_trigger_press_event>(cur_time, side, false));
				}
			}
			break;
		case EVENT_TYPE::VR_GRIP_PULL:
			events.emplace_back(std::make_shared<vr_grip_pull_event>(cur_time, side, state.currentState, delta));
			if (emulate.grip_touch) {
				if (prev_state.f == 0.0f && cur_state > 0.0f) {
					events.emplace_back(std::make_shared<vr_grip_touch_event>(cur_time, side, true));
				} else if (prev_state.f > 0.0f && cur_state == 0.0f) {
					events.emplace_back(std::make_shared<vr_grip_touch_event>(cur_time, side, false));
				}
			}
			break;
		case EVENT_TYPE::VR_GRIP_FORCE:
			events.emplace_back(std::make_shared<vr_grip_force_event>(cur_time, side, state.currentState, delta));
			if (emulate.grip_press) {
				if (prev_state.f < emulation_trigger_force && cur_state >= emulation_trigger_force) {
					events.emplace_back(std::make_shared<vr_grip_press_event>(cur_time, side, true));
				} else if (prev_state.f >= emulation_trigger_force && cur_state < emulation_trigger_force) {
					events.emplace_back(std::make_shared<vr_grip_press_event>(cur_time, side, false));
				}
			}
			break;
		case EVENT_TYPE::VR_TRACKPAD_FORCE:
			events.emplace_back(std::make_shared<vr_trackpad_force_event>(cur_time, side, state.currentState, delta));
			if (emulate.trackpad_press) {
				if (prev_state.f < emulation_trigger_force && cur_state >= emulation_trigger_force) {
					events.emplace_back(std::make_shared<vr_trackpad_press_event>(cur_time, side, true));
				} else if (prev_state.f >= emulation_trigger_force && cur_state < emulation_trigger_force) {
					events.emplace_back(std::make_shared<vr_trackpad_press_event>(cur_time, side, false));
				}
			}
			break;
		case EVENT_TYPE::VR_THUMBREST_FORCE:
			events.emplace_back(std::make_shared<vr_thumbrest_force_event>(cur_time, side, state.currentState, delta));
			break;
		default:
			log_error("unknown/unhandled VR event: $", event_type);
			break;
	}
	prev_state.f = cur_state;
}

void openxr_context::add_hand_float2_event(std::vector<std::shared_ptr<event_object>>& events, const EVENT_TYPE event_type,
										   const XrActionStateVector2f& state, const bool side) {
	const auto cur_time = convert_time_to_ticks(state.lastChangeTime);
	const float2 cur_state { state.currentState.x, state.currentState.y };
	auto& prev_state = hand_event_states[!side ? 0 : 1][event_type];
	const auto delta = cur_state - prev_state.f2;
	switch (event_type) {
		case EVENT_TYPE::VR_TRACKPAD_MOVE:
			events.emplace_back(std::make_shared<vr_trackpad_move_event>(cur_time, side, cur_state, delta));
			break;
		case EVENT_TYPE::VR_THUMBSTICK_MOVE:
			events.emplace_back(std::make_shared<vr_thumbstick_move_event>(cur_time, side, cur_state, delta));
			break;
		default:
			log_error("unknown/unhandled VR event: $", event_type);
			break;
	}
	prev_state.f2 = cur_state;
}

template <bool has_radius = false, typename location_type = XrSpaceLocation, typename velocity_type = XrSpaceVelocity>
static vr_context::pose_t make_pose(const vr_context::POSE_TYPE type,
									const location_type& location,
									const velocity_type& velocity) {
	vr_context::pose_t pose { .type = type, .flags.all = 0u };
	
	if ((location.locationFlags & XR_SPACE_LOCATION_POSITION_VALID_BIT) == XR_SPACE_LOCATION_POSITION_VALID_BIT) {
		pose.position = { location.pose.position.x, location.pose.position.y, location.pose.position.z };
		pose.flags.position_valid = 1;
	} else {
		pose.flags.position_valid = 0;
	}
	pose.flags.position_tracked = ((location.locationFlags & XR_SPACE_LOCATION_POSITION_TRACKED_BIT) != 0);
	
	if ((location.locationFlags & XR_SPACE_LOCATION_ORIENTATION_VALID_BIT) == XR_SPACE_LOCATION_ORIENTATION_VALID_BIT) {
		pose.orientation = {
			location.pose.orientation.x,
			location.pose.orientation.y,
			location.pose.orientation.z,
			location.pose.orientation.w
		};
		pose.flags.orientation_valid = 1;
	} else {
		pose.flags.orientation_valid = 0;
	}
	pose.flags.orientation_tracked = ((location.locationFlags & XR_SPACE_LOCATION_ORIENTATION_TRACKED_BIT) != 0);
	
	if ((velocity.velocityFlags & XR_SPACE_VELOCITY_LINEAR_VALID_BIT) == XR_SPACE_VELOCITY_LINEAR_VALID_BIT) {
		pose.linear_velocity = { velocity.linearVelocity.x, velocity.linearVelocity.y, velocity.linearVelocity.z };
		pose.flags.linear_velocity_valid = 1;
		pose.flags.linear_velocity_tracked = 1;
	} else {
		pose.flags.linear_velocity_valid = 0;
		pose.flags.linear_velocity_tracked = 0;
	}
	
	if ((velocity.velocityFlags & XR_SPACE_VELOCITY_ANGULAR_VALID_BIT) == XR_SPACE_VELOCITY_ANGULAR_VALID_BIT) {
		pose.angular_velocity = { velocity.linearVelocity.x, velocity.linearVelocity.y, velocity.linearVelocity.z };
		pose.flags.angular_velocity_valid = 1;
		pose.flags.angular_velocity_tracked = 1;
	} else {
		pose.flags.angular_velocity_valid = 0;
		pose.flags.angular_velocity_tracked = 0;
	}
	
	if constexpr (has_radius) {
		pose.radius = location.radius;
		pose.flags.radius_valid = 1;
	} else {
		pose.flags.radius_valid = 0;
	}
	
	pose.flags.is_active = (pose.flags.position_valid || pose.flags.position_tracked ||
							pose.flags.orientation_valid || pose.flags.orientation_tracked);
	
	return pose;
}

static std::optional<vr_context::pose_t> pose_from_space(const vr_context::POSE_TYPE type,
													XrSpace& space, XrSpace& base_space,
													const XrTime& time) {
	XrSpaceVelocity space_velocity {
		.type = XR_TYPE_SPACE_VELOCITY,
		.next = nullptr,
		.velocityFlags = 0,
	};
	XrSpaceLocation space_location {
		.type = XR_TYPE_SPACE_LOCATION,
		.next = &space_velocity,
	};
	XR_CALL_RET(xrLocateSpace(space, base_space, time, &space_location),
				"failed to locate pose space", {});
	
	return make_pose(type, space_location, space_velocity);
}

void openxr_context::add_hand_tracking_poses(std::vector<pose_t>& poses, const XrSpace& base_space, const XrTime& time) {
	const auto joint_count = uint32_t(has_hand_tracking_forearm_support ? XR_HAND_FOREARM_JOINT_COUNT_ULTRALEAP : XR_HAND_JOINT_COUNT_EXT);
	std::vector<XrHandJointLocationEXT> joint_locations(joint_count);
	std::vector<XrHandJointVelocityEXT> joint_velocities(joint_count);
	for (size_t hand_idx = 0; hand_idx < hand_trackers.size(); ++hand_idx) {
		const XrHandJointsLocateInfoEXT locate_info {
			.type = XR_TYPE_HAND_JOINTS_LOCATE_INFO_EXT,
			.next = nullptr,
			.baseSpace = base_space,
			.time = time,
		};
		XrHandJointVelocitiesEXT velocities {
			.type = XR_TYPE_HAND_JOINT_VELOCITIES_EXT,
			.next = nullptr,
			.jointCount = uint32_t(joint_velocities.size()),
			.jointVelocities = joint_velocities.data(),
		};
		XrHandJointLocationsEXT locations {
			.type = XR_TYPE_HAND_JOINT_LOCATIONS_EXT,
			.next = &velocities,
			.isActive = false,
			.jointCount = uint32_t(joint_locations.size()),
			.jointLocations = joint_locations.data(),
		};
		XR_CALL_CONT(internal->LocateHandJoints(hand_trackers[hand_idx], &locate_info, &locations),
					 "failed to locate "s + (hand_idx == 0 ? "left" : "right") + " hand joints");
		
		if (!locations.isActive) {
			// -> hand is not tracked right now
			continue;
		}
		
		for (uint32_t i = 0; i < joint_count; ++i) {
			poses.emplace_back(make_pose<true>(POSE_TYPE(uint32_t(hand_idx == 0 ? POSE_TYPE::HAND_JOINT_PALM_LEFT :
																				  POSE_TYPE::HAND_JOINT_PALM_RIGHT) + i),
											   locations.jointLocations[i], velocities.jointVelocities[i]));
		}
	}
}

bool openxr_context::handle_input_internal(std::vector<std::shared_ptr<event_object>>& events) REQUIRES(!pose_state_lock) {
	if (!session || !input_action_set || !is_focused) {
		return true;
	}
	
	// sync
	std::array<XrActiveActionSet, 2> active_action_sets {
		XrActiveActionSet {
			.actionSet = input_action_set,
			.subactionPath = XR_NULL_PATH,
		},
		XrActiveActionSet {
			.actionSet = nullptr,
			.subactionPath = XR_NULL_PATH,
		}
	};
	uint32_t active_action_set_count = 1;
	if (has_tracker_interaction_support && tracker_input_action_set) {
		active_action_sets[1].actionSet = tracker_input_action_set;
		active_action_set_count = 2;
	}
	const XrActionsSyncInfo sync_info{
		.type = XR_TYPE_ACTIONS_SYNC_INFO,
		.next = nullptr,
		.countActiveActionSets = active_action_set_count,
		.activeActionSets = active_action_sets.data(),
	};
	XR_CALL_RET(xrSyncActions(session, &sync_info),
				"failed to sync actions", false);
	
	// handle poses (tracked devices, ...)
	std::vector<pose_t> updated_pose_state;
	updated_pose_state.reserve(std::max(prev_pose_state_size, size_t(4u)));
	const auto current_time = convert_perf_counter_to_time(SDL_GetPerformanceCounter());
	
	// head state
	if (auto head_pose = pose_from_space(vr_context::POSE_TYPE::HEAD, view_space, scene_space, current_time); head_pose) {
		updated_pose_state.emplace_back(std::move(*head_pose));
	}
	
	// hand/arm joints
	if (has_hand_tracking_support) {
		add_hand_tracking_poses(updated_pose_state, scene_space, current_time);
	}
	
	// must iterate over all actions to figure out if something changed
	for (const auto& base_action : base_actions) {
		const auto event_type = base_action.first;
		const auto& action = base_action.second;
		
		// NOTE: will only handle hand events/actions here
		for (auto input_type_val = uint32_t(INPUT_TYPE::HAND_LEFT);
			 input_type_val <= uint32_t(INPUT_TYPE::HAND_RIGHT); input_type_val <<= 1u) {
			const auto input_type = INPUT_TYPE(input_type_val);
			if ((action.input_type & input_type) != input_type) {
				continue;
			}
			
			const XrActionStateGetInfo get_info {
				.type = XR_TYPE_ACTION_STATE_GET_INFO,
				.next = nullptr,
				.action = action.action,
				.subactionPath = hand_paths[input_type == INPUT_TYPE::HAND_LEFT ? 0 : 1],
			};
			
			const auto side = (input_type != INPUT_TYPE::HAND_LEFT);
			switch (action.action_type) {
				case ACTION_TYPE::BOOLEAN: {
					XrActionStateBoolean state {
						.type = XR_TYPE_ACTION_STATE_BOOLEAN,
						.next = nullptr,
					};
					XR_CALL_BREAK(xrGetActionStateBoolean(session, &get_info, &state),
								  "failed to get bool action state");
					if (state.changedSinceLastSync) {
						add_hand_bool_event(events, event_type, state, side);
					}
					break;
				}
				case ACTION_TYPE::FLOAT: {
					XrActionStateFloat state {
						.type = XR_TYPE_ACTION_STATE_FLOAT,
						.next = nullptr,
					};
					XR_CALL_BREAK(xrGetActionStateFloat(session, &get_info, &state),
								  "failed to get float action state");
					if (state.changedSinceLastSync) {
						add_hand_float_event(events, event_type, state, side);
					}
					break;
				}
				case ACTION_TYPE::FLOAT2: {
					XrActionStateVector2f state {
						.type = XR_TYPE_ACTION_STATE_VECTOR2F,
						.next = nullptr,
					};
					XR_CALL_BREAK(xrGetActionStateVector2f(session, &get_info, &state),
								  "failed to get float2 action state");
					if (state.changedSinceLastSync) {
						add_hand_float2_event(events, event_type, state, side);
					}
					break;
				}
				case ACTION_TYPE::POSE: {
					XrActionStatePose state {
						.type = XR_TYPE_ACTION_STATE_POSE,
						.next = nullptr,
					};
					XR_CALL_CONT(xrGetActionStatePose(session, &get_info, &state),
								 "failed to get pose action state");
					if (state.isActive) {
						XrSpace pose_space = nullptr;
						auto pose_type = vr_context::POSE_TYPE::UNKNOWN;
						switch (event_type) {
							case EVENT_TYPE::VR_INTERNAL_HAND_POSE_LEFT:
								pose_space = hand_spaces[0];
								pose_type = vr_context::POSE_TYPE::HAND_LEFT;
								break;
							case EVENT_TYPE::VR_INTERNAL_HAND_POSE_RIGHT:
								pose_space = hand_spaces[1];
								pose_type = vr_context::POSE_TYPE::HAND_RIGHT;
								break;
							case EVENT_TYPE::VR_INTERNAL_HAND_AIM_LEFT:
								pose_space = hand_aim_spaces[0];
								pose_type = vr_context::POSE_TYPE::HAND_LEFT_AIM;
								break;
							case EVENT_TYPE::VR_INTERNAL_HAND_AIM_RIGHT:
								pose_space = hand_aim_spaces[1];
								pose_type = vr_context::POSE_TYPE::HAND_RIGHT_AIM;
								break;
							default:
								assert(false && "should not be here");
								break;
						}
						if (pose_space) {
							if (auto pose = pose_from_space(pose_type, pose_space, scene_space, current_time); pose) {
								updated_pose_state.emplace_back(std::move(*pose));
							}
						}
					}
					break;
				}
				case ACTION_TYPE::HAPTIC:
					assert(false && "should not be here");
					break;
			}
		}
	}
	
	// handle tracker actions
	if (has_tracker_interaction_support) {
		for (const auto& tracker_action : tracker_actions) {
			const auto event_type = tracker_action.first;
			const auto& action = tracker_action.second;
			if ((action.input_type & INPUT_TYPE::TRACKER) == INPUT_TYPE::TRACKER) {
				if (action.action_type != ACTION_TYPE::POSE) {
					assert(false && "should not be here");
					continue;
				}
				
				const auto tracker_idx = uint32_t(event_type) - uint32_t(EVENT_TYPE::VR_INTERNAL_TRACKER_HANDHELD_OBJECT);
				const XrActionStateGetInfo get_info {
					.type = XR_TYPE_ACTION_STATE_GET_INFO,
					.next = nullptr,
					.action = action.action,
					.subactionPath = tracker_role_paths[tracker_idx],
				};
				
				XrActionStatePose state {
					.type = XR_TYPE_ACTION_STATE_POSE,
					.next = nullptr,
				};
				XR_CALL_CONT(xrGetActionStatePose(session, &get_info, &state),
							 "failed to get tracker pose action state");
				
				if (state.isActive) {
					const auto pose_type = POSE_TYPE(uint32_t(POSE_TYPE::TRACKER_HANDHELD_OBJECT) + tracker_idx);
					if (auto pose = pose_from_space(pose_type, tracker_spaces[tracker_idx], scene_space, current_time); pose) {
						updated_pose_state.emplace_back(std::move(*pose));
					}
				}
			}
		}
	}
	
	// update pose state
	prev_pose_state_size = updated_pose_state.size();
	{
		GUARD(pose_state_lock);
		pose_state = std::move(updated_pose_state);
	}
	
	return true;
}

std::vector<vr_context::pose_t> openxr_context::get_pose_state() const REQUIRES(!pose_state_lock) {
	GUARD(pose_state_lock);
	return pose_state;
}

} // namespace fl

#endif
