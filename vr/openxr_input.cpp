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

#include <floor/vr/openxr_context.hpp>

#if !defined(FLOOR_NO_OPENXR) && !defined(FLOOR_NO_VULKAN)

bool openxr_context::input_setup() {
	// create base input set
	const XrActionSetCreateInfo action_set_create_info {
		.type = XR_TYPE_ACTION_SET_CREATE_INFO,
		.next = nullptr,
		.actionSetName = "input",
		.localizedActionSetName = "input",
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

	{
		const XrActionCreateInfo action_create_info {
			.type = XR_TYPE_ACTION_CREATE_INFO,
			.next = nullptr,
			.actionName = "hand_pose",
			.actionType = XR_ACTION_TYPE_POSE_INPUT,
			.countSubactionPaths = uint32_t(hand_paths.size()),
			.subactionPaths = hand_paths.data(),
			.localizedActionName = "hand_pose"
		};
		XR_CALL_RET(xrCreateAction(input_action_set, &action_create_info, &hand_pose_action),
					"failed to create hand pose action ", false);
	}
	for (uint32_t i = 0; i < 2; ++i) {
		const XrActionSpaceCreateInfo hand_space_create_info {
			.type = XR_TYPE_ACTION_SPACE_CREATE_INFO,
			.next = nullptr,
			.action = hand_pose_action,
			.subactionPath = (i == 0 ? *hand_left_path : *hand_right_path),
			.poseInActionSpace = {
				.orientation = { 0.0f, 0.0f, 0.0f, 1.0f },
				.position = { 0.0f, 0.0f, 0.0f },
			},
		};
		XR_CALL_RET(xrCreateActionSpace(session, &hand_space_create_info, &hand_spaces[i]),
					"failed to create hand action space", false);
	}

	struct base_action_definition_t {
		string name;
		ACTION_TYPE action_type;
		EVENT_TYPE event_type;
	};
	{
		static const vector<base_action_definition_t> base_actions_defs {
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
		};
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
				// TODO: support for actions other than hands
				.countSubactionPaths = uint32_t(hand_paths.size()),
				.subactionPaths = hand_paths.data(),
			};
			memcpy(action_create_info.actionName, base_action_def.name.c_str(), base_action_def.name.size());
			action_create_info.actionName[action_name_size] = '\0';
			memcpy(action_create_info.localizedActionName, base_action_def.name.c_str(), base_action_def.name.size());
			action_create_info.localizedActionName[action_name_size] = '\0';
			XrAction action { nullptr };
			XR_CALL_RET(xrCreateAction(input_action_set, &action_create_info, &action),
						"failed to create action " + base_action_def.name, false);
			base_actions.emplace(base_action_def.event_type,
								 action_t {
									 .action = action,
									 .input_type = INPUT_TYPE::HAND_LEFT | INPUT_TYPE::HAND_RIGHT,
									 .action_type = base_action_def.action_type,
								 });
		}
	}

	// create controller bindings
	struct action_definition_t {
		string path;
		EVENT_TYPE event_type;
	};
	{
		const vector<action_definition_t> index_hand_actions {
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
			{ "/input/thumbstick/x", EVENT_TYPE::VR_THUMBSTICK_MOVE },
			{ "/input/thumbstick/y", EVENT_TYPE::VR_THUMBSTICK_MOVE },
			{ "/input/thumbstick/click", EVENT_TYPE::VR_THUMBSTICK_PRESS },
			{ "/input/thumbstick/touch", EVENT_TYPE::VR_THUMBSTICK_TOUCH },
			{ "/input/trackpad/x", EVENT_TYPE::VR_TRACKPAD_MOVE },
			{ "/input/trackpad/y", EVENT_TYPE::VR_TRACKPAD_MOVE },
			{ "/input/trackpad/force", EVENT_TYPE::VR_TRACKPAD_FORCE },
			{ "/input/trackpad/touch", EVENT_TYPE::VR_TRACKPAD_TOUCH },
		};
		// TODO: emulate VR_GRIP_PRESS support via VR_GRIP_FORCE
		// TODO: /input/aim/pose, /output/haptic
		vector<XrActionSuggestedBinding> bindings;
		for (uint32_t i = 0; i < 2; ++i) {
			const auto hand_path = "/user/hand/"s + (i == 0 ? "left" : "right");
			for (const auto& hand_action : index_hand_actions) {
				bindings.emplace_back(XrActionSuggestedBinding {
					.action = base_actions.at(hand_action.event_type).action,
					.binding = to_path_or_throw(hand_path + hand_action.path),
				});
			}
		}
		const XrInteractionProfileSuggestedBinding suggested_binding {
			.type = XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING,
			.next = nullptr,
			.interactionProfile = to_path_or_throw("/interaction_profiles/valve/index_controller"),
			.countSuggestedBindings = uint32_t(bindings.size()),
			.suggestedBindings = bindings.data(),
		};
		XR_CALL_IGNORE(xrSuggestInteractionProfileBindings(instance, &suggested_binding),
					   "failed to set suggested interaction profile for Valve Index controller");
	}

	// finally: attach
	const XrSessionActionSetsAttachInfo attach_info {
		.type = XR_TYPE_SESSION_ACTION_SETS_ATTACH_INFO,
		.next = nullptr,
		.countActionSets = 1,
		.actionSets = &input_action_set,
	};
	XR_CALL_RET(xrAttachSessionActionSets(session, &attach_info),
				"failed to attach session action sets", false);

	return true;
}

uint64_t openxr_context::convert_time_to_ticks(XrTime time) {
#if defined(__WINDOWS__)
	int64_t perf_counter = 0;
	XR_CALL_RET(ConvertTimeToWin32PerformanceCounterKHR(instance, time, &perf_counter),
				"failed to convert OpenXR time to Win32 perf counter", SDL_GetTicks64());
	const auto perf_since_start = uint64_t(perf_counter) - win_start_perf_counter;
	return (perf_since_start * 1000ull) / win_perf_counter_freq;
#elif defined(__linux__)
	timespec timespec_time {};
	XR_CALL_RET(ConvertTimeToTimespecTimeKHR(instance, time, &timespec_time),
				"failed to convert OpenXR time to timespec time", SDL_GetTicks64());
	const auto time_in_ns = uint64_t(timespec_time.tv_sec) * unix_perf_counter_freq + uint64_t(timespec_time.tv_nsec);
	const auto time_since_start_in_ns = time_in_ns - unix_start_time;
	return time_since_start_in_ns / 1'000'000ull; // ns -> ms
#else
#error "unsupported OS"
#endif
}

void openxr_context::add_bool_event(vector<shared_ptr<event_object>>& events, const EVENT_TYPE event_type,
									const XrActionStateBoolean& state, const bool side) {
	const auto cur_time = convert_time_to_ticks(state.lastChangeTime);
	switch (event_type) {
		case EVENT_TYPE::VR_SYSTEM_PRESS:
			events.emplace_back(make_shared<vr_system_press_event>(cur_time, side, state.currentState));
			break;
		case EVENT_TYPE::VR_SYSTEM_TOUCH:
			events.emplace_back(make_shared<vr_system_touch_event>(cur_time, side, state.currentState));
			break;
		case EVENT_TYPE::VR_MAIN_PRESS:
			events.emplace_back(make_shared<vr_main_press_event>(cur_time, side, state.currentState));
			break;
		case EVENT_TYPE::VR_MAIN_TOUCH:
			events.emplace_back(make_shared<vr_main_touch_event>(cur_time, side, state.currentState));
			break;
		case EVENT_TYPE::VR_APP_MENU_PRESS:
			events.emplace_back(make_shared<vr_app_menu_press_event>(cur_time, side, state.currentState));
			break;
		case EVENT_TYPE::VR_APP_MENU_TOUCH:
			events.emplace_back(make_shared<vr_app_menu_touch_event>(cur_time, side, state.currentState));
			break;
		case EVENT_TYPE::VR_GRIP_PRESS:
			events.emplace_back(make_shared<vr_grip_press_event>(cur_time, side, state.currentState));
			break;
		case EVENT_TYPE::VR_GRIP_TOUCH:
			events.emplace_back(make_shared<vr_grip_touch_event>(cur_time, side, state.currentState));
			break;
		case EVENT_TYPE::VR_TRIGGER_PRESS:
			events.emplace_back(make_shared<vr_trigger_press_event>(cur_time, side, state.currentState));
			break;
		case EVENT_TYPE::VR_TRIGGER_TOUCH:
			events.emplace_back(make_shared<vr_trigger_touch_event>(cur_time, side, state.currentState));
			break;
		case EVENT_TYPE::VR_THUMBSTICK_PRESS:
			events.emplace_back(make_shared<vr_thumbstick_press_event>(cur_time, side, state.currentState));
			break;
		case EVENT_TYPE::VR_THUMBSTICK_TOUCH:
			events.emplace_back(make_shared<vr_thumbstick_touch_event>(cur_time, side, state.currentState));
			break;
		case EVENT_TYPE::VR_TRACKPAD_PRESS:
			events.emplace_back(make_shared<vr_trackpad_press_event>(cur_time, side, state.currentState));
			break;
		case EVENT_TYPE::VR_TRACKPAD_TOUCH:
			events.emplace_back(make_shared<vr_trackpad_touch_event>(cur_time, side, state.currentState));
			break;
		default:
			log_error("unknown/unhandled VR event: $", event_type);
			break;
	}
}

void openxr_context::add_float_event(vector<shared_ptr<event_object>>& events, const EVENT_TYPE event_type,
									 const XrActionStateFloat& state, const bool side) {
	const auto cur_time = convert_time_to_ticks(state.lastChangeTime);
	// TODO: can we get a delta (or simulate it?) with OpenXR?
	const float delta = 0.0f;
	switch (event_type) {
		case EVENT_TYPE::VR_TRIGGER_PULL:
			events.emplace_back(make_shared<vr_trigger_pull_event>(cur_time, side, state.currentState, delta));
			break;
		case EVENT_TYPE::VR_GRIP_PULL:
			events.emplace_back(make_shared<vr_grip_pull_event>(cur_time, side, state.currentState, delta));
			break;
		case EVENT_TYPE::VR_GRIP_FORCE:
			events.emplace_back(make_shared<vr_grip_force_event>(cur_time, side, state.currentState, delta));
			break;
		case EVENT_TYPE::VR_TRACKPAD_FORCE:
			events.emplace_back(make_shared<vr_trackpad_force_event>(cur_time, side, state.currentState, delta));
			break;
		default:
			log_error("unknown/unhandled VR event: $", event_type);
			break;
	}
}

void openxr_context::add_float2_event(vector<shared_ptr<event_object>>& events, const EVENT_TYPE event_type,
									  const XrActionStateVector2f& state, const bool side) {
	const auto cur_time = convert_time_to_ticks(state.lastChangeTime);
	const float2 cur_state { state.currentState.x, state.currentState.y };
	// TODO: can we get a delta (or simulate it?) with OpenXR?
	const float2 delta;
	switch (event_type) {
		case EVENT_TYPE::VR_TRACKPAD_MOVE:
			events.emplace_back(make_shared<vr_trackpad_move_event>(cur_time, side, cur_state, delta));
			break;
		case EVENT_TYPE::VR_THUMBSTICK_MOVE:
			events.emplace_back(make_shared<vr_thumbstick_move_event>(cur_time, side, cur_state, delta));
			break;
		default:
			log_error("unknown/unhandled VR event: $", event_type);
			break;
	}
}

bool openxr_context::handle_input_internal(vector<shared_ptr<event_object>>& events) {
	if (!session || !input_action_set || !is_focused) {
		return true;
	}

	// TODO: poses / tracked device handling
	
	// sync
	const XrActiveActionSet active_action_set{
		.actionSet = input_action_set,
		.subactionPath = XR_NULL_PATH,
	};
	const XrActionsSyncInfo sync_info{
		.type = XR_TYPE_ACTIONS_SYNC_INFO,
		.next = nullptr,
		.countActiveActionSets = 1,
		.activeActionSets = &active_action_set,
	};
	XR_CALL_RET(xrSyncActions(session, &sync_info),
				"failed to sync actions", false);

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
						add_bool_event(events, event_type, state, side);
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
						add_float_event(events, event_type, state, side);
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
						add_float2_event(events, event_type, state, side);
					}
					break;
				}
				case ACTION_TYPE::POSE:
				case ACTION_TYPE::HAPTIC:
					assert(false && "should not be here");
					break;
			}
		}
	}

	return true;
}

#endif
