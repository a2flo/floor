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

#if !defined(FLOOR_USER_EVENT_TYPES)
#define FLOOR_USER_EVENT_TYPES
#endif

#include <memory>
#include <floor/math/vector_lib.hpp>
#include <floor/core/enum_helpers.hpp>

namespace fl {

// general/global event types
enum class EVENT_TYPE : uint32_t {
	__USER_EVENT			= (1u << 31u),
	__OTHER_EVENT			= (1u << 30u),
	__MOUSE_EVENT			= (1u << 29u),
	__KEY_EVENT				= (1u << 28u),
	__TOUCH_EVENT			= (1u << 27u),
	__UI_EVENT				= (1u << 26u),
	__VR_CONTROLLER_EVENT	= (1u << 25u),
	__VR_INTERNAL_EVENT		= (1u << 24u),
	
	MOUSE_LEFT_DOWN = __MOUSE_EVENT + 1,
	MOUSE_LEFT_UP,
	MOUSE_LEFT_CLICK,
	MOUSE_LEFT_DOUBLE_CLICK,
	MOUSE_LEFT_HOLD,
	
	MOUSE_RIGHT_DOWN,
	MOUSE_RIGHT_UP,
	MOUSE_RIGHT_CLICK,
	MOUSE_RIGHT_DOUBLE_CLICK,
	MOUSE_RIGHT_HOLD,
	
	MOUSE_MIDDLE_DOWN,
	MOUSE_MIDDLE_UP,
	MOUSE_MIDDLE_CLICK,
	MOUSE_MIDDLE_DOUBLE_CLICK,
	MOUSE_MIDDLE_HOLD,
	
	MOUSE_MOVE,
	
	MOUSE_WHEEL_UP,
	MOUSE_WHEEL_DOWN,
	
	KEY_DOWN = __KEY_EVENT + 1,
	KEY_UP,
	KEY_HOLD,
	UNICODE_INPUT,
	
	FINGER_DOWN = __TOUCH_EVENT + 1,
	FINGER_UP,
	FINGER_MOVE,
	
	QUIT = __OTHER_EVENT + 1,
	WINDOW_RESIZE,
	CLIPBOARD_UPDATE,
	
	// NOTE: VR controller events are for both the left and right controller, differentiation is part of the event object
	VR_APP_MENU_PRESS = __VR_CONTROLLER_EVENT + 1,
	VR_APP_MENU_TOUCH,
	VR_MAIN_PRESS,
	VR_MAIN_TOUCH,
	VR_SYSTEM_PRESS,
	VR_SYSTEM_TOUCH,
	VR_TRACKPAD_PRESS,
	VR_TRACKPAD_TOUCH,
	VR_TRACKPAD_MOVE,
	VR_TRACKPAD_FORCE,
	VR_THUMBSTICK_PRESS,
	VR_THUMBSTICK_TOUCH,
	VR_THUMBSTICK_MOVE,
	VR_TRIGGER_TOUCH,
	VR_TRIGGER_PRESS,
	VR_TRIGGER_PULL,
	VR_GRIP_PRESS,
	VR_GRIP_TOUCH,
	VR_GRIP_PULL,
	VR_GRIP_FORCE,
	VR_THUMBREST_TOUCH,
	VR_THUMBREST_FORCE,
	VR_SHOULDER_PRESS,
	
	// NOTE: these are only used internally and will not actually be emitted by events
	VR_INTERNAL_HAND_POSE_LEFT = __VR_INTERNAL_EVENT + 1,
	VR_INTERNAL_HAND_POSE_RIGHT,
	VR_INTERNAL_HAND_AIM_LEFT,
	VR_INTERNAL_HAND_AIM_RIGHT,
	VR_INTERNAL_TRACKER_HANDHELD_OBJECT,
	VR_INTERNAL_TRACKER_FOOT_LEFT,
	VR_INTERNAL_TRACKER_FOOT_RIGHT,
	VR_INTERNAL_TRACKER_SHOULDER_LEFT,
	VR_INTERNAL_TRACKER_SHOULDER_RIGHT,
	VR_INTERNAL_TRACKER_ELBOW_LEFT,
	VR_INTERNAL_TRACKER_ELBOW_RIGHT,
	VR_INTERNAL_TRACKER_KNEE_LEFT,
	VR_INTERNAL_TRACKER_KNEE_RIGHT,
	VR_INTERNAL_TRACKER_WAIST,
	VR_INTERNAL_TRACKER_CHEST,
	VR_INTERNAL_TRACKER_CAMERA,
	VR_INTERNAL_TRACKER_KEYBOARD,
	VR_INTERNAL_TRACKER_WRIST_LEFT,
	VR_INTERNAL_TRACKER_WRIST_RIGHT,
	VR_INTERNAL_TRACKER_ANKLE_LEFT,
	VR_INTERNAL_TRACKER_ANKLE_RIGHT,
	
	__USER_EVENT_START = __USER_EVENT + 1,
	FLOOR_USER_EVENT_TYPES
};
static inline constexpr EVENT_TYPE operator&(const EVENT_TYPE& e0, const EVENT_TYPE& e1) {
	return (EVENT_TYPE)((std::underlying_type_t<EVENT_TYPE>)e0 &
						(std::underlying_type_t<EVENT_TYPE>)e1);
}
static inline constexpr EVENT_TYPE& operator&=(EVENT_TYPE& e0, const EVENT_TYPE& e1) {
	e0 = e0 & e1;
	return e0;
}

//
struct event_object {
	const uint64_t time;
	event_object(const uint64_t& time_) : time(time_) {}
};
template<EVENT_TYPE event_type> struct event_object_base : public event_object {
	const EVENT_TYPE type;
	event_object_base(const uint64_t& time_) : event_object(time_), type(event_type) {}
};

// mouse events
template<EVENT_TYPE event_type> struct mouse_event_base : public event_object_base<event_type> {
	const float2 position;
	mouse_event_base(const uint64_t& time_, const float2& position_) :
	event_object_base<event_type>(time_), position(position_) {}
};

template<EVENT_TYPE event_type, EVENT_TYPE down_event_type, EVENT_TYPE up_event_type> struct mouse_click_event : public event_object_base<event_type> {
	std::shared_ptr<mouse_event_base<down_event_type>> down;
	std::shared_ptr<mouse_event_base<up_event_type>> up;
	mouse_click_event(const uint64_t& time_,
					  std::shared_ptr<event_object> down_evt,
					  std::shared_ptr<event_object> up_evt)
	: event_object_base<event_type>(time_),
	down(*(std::shared_ptr<mouse_event_base<down_event_type>>*)&down_evt),
	up(*(std::shared_ptr<mouse_event_base<up_event_type>>*)&up_evt) {}
};

template<EVENT_TYPE event_type> struct mouse_move_event_base : public mouse_event_base<event_type> {
	const float2 move;
	mouse_move_event_base(const uint64_t& time_,
						  const float2& position_,
						  const float2& move_)
	: mouse_event_base<event_type>(time_, position_), move(move_) {}
};

template<EVENT_TYPE event_type> struct mouse_wheel_event_base : public mouse_event_base<event_type> {
	const uint32_t amount;
	mouse_wheel_event_base(const uint64_t& time_,
						   const float2& position_,
						   const uint32_t& amount_)
	: mouse_event_base<event_type>(time_, position_), amount(amount_) {}
};

// mouse event typedefs
using mouse_left_down_event = mouse_event_base<EVENT_TYPE::MOUSE_LEFT_DOWN>;
using mouse_left_up_event = mouse_event_base<EVENT_TYPE::MOUSE_LEFT_UP>;
using mouse_left_hold_event = mouse_event_base<EVENT_TYPE::MOUSE_LEFT_HOLD>;
using mouse_left_click_event = mouse_click_event<EVENT_TYPE::MOUSE_LEFT_CLICK, EVENT_TYPE::MOUSE_LEFT_DOWN, EVENT_TYPE::MOUSE_LEFT_UP>;
using mouse_left_double_click_event = mouse_click_event<EVENT_TYPE::MOUSE_LEFT_DOUBLE_CLICK, EVENT_TYPE::MOUSE_LEFT_DOWN, EVENT_TYPE::MOUSE_LEFT_UP>;

using mouse_right_down_event = mouse_event_base<EVENT_TYPE::MOUSE_RIGHT_DOWN>;
using mouse_right_up_event = mouse_event_base<EVENT_TYPE::MOUSE_RIGHT_UP>;
using mouse_right_hold_event = mouse_event_base<EVENT_TYPE::MOUSE_RIGHT_HOLD>;
using mouse_right_click_event = mouse_click_event<EVENT_TYPE::MOUSE_RIGHT_CLICK, EVENT_TYPE::MOUSE_RIGHT_DOWN, EVENT_TYPE::MOUSE_RIGHT_UP>;
using mouse_right_double_click_event = mouse_click_event<EVENT_TYPE::MOUSE_RIGHT_DOUBLE_CLICK, EVENT_TYPE::MOUSE_RIGHT_DOWN, EVENT_TYPE::MOUSE_RIGHT_UP>;

using mouse_middle_down_event = mouse_event_base<EVENT_TYPE::MOUSE_MIDDLE_DOWN>;
using mouse_middle_up_event = mouse_event_base<EVENT_TYPE::MOUSE_MIDDLE_UP>;
using mouse_middle_hold_event = mouse_event_base<EVENT_TYPE::MOUSE_MIDDLE_HOLD>;
using mouse_middle_click_event = mouse_click_event<EVENT_TYPE::MOUSE_MIDDLE_CLICK, EVENT_TYPE::MOUSE_MIDDLE_DOWN, EVENT_TYPE::MOUSE_MIDDLE_UP>;
using mouse_middle_double_click_event = mouse_click_event<EVENT_TYPE::MOUSE_MIDDLE_DOUBLE_CLICK, EVENT_TYPE::MOUSE_MIDDLE_DOWN, EVENT_TYPE::MOUSE_MIDDLE_UP>;

using mouse_move_event = mouse_move_event_base<EVENT_TYPE::MOUSE_MOVE>;

using mouse_wheel_up_event = mouse_wheel_event_base<EVENT_TYPE::MOUSE_WHEEL_UP>;
using mouse_wheel_down_event = mouse_wheel_event_base<EVENT_TYPE::MOUSE_WHEEL_DOWN>;

// key events
template<EVENT_TYPE event_type> struct key_event : public event_object_base<event_type> {
	const uint32_t key;
	key_event(const uint64_t& time_,
			  const uint32_t& key_) :
	event_object_base<event_type>(time_), key(key_) {}
};
using key_down_event = key_event<EVENT_TYPE::KEY_DOWN>;
using key_up_event = key_event<EVENT_TYPE::KEY_UP>;
using key_hold_event = key_event<EVENT_TYPE::KEY_HOLD>;
using unicode_input_event = key_event<EVENT_TYPE::UNICODE_INPUT>;

// touch events
template<EVENT_TYPE event_type> struct touch_event_base : public event_object_base<event_type> {
	const float2 normalized_position;
	const float pressure;
	const uint64_t id;
	touch_event_base(const uint64_t& time_,
					 const float2& norm_position_,
					 const float& pressure_,
					 const uint64_t& id_) :
	event_object_base<event_type>(time_), normalized_position(norm_position_), pressure(pressure_), id(id_) {}
};
template<EVENT_TYPE event_type> struct touch_move_event_base : public touch_event_base<event_type> {
	const float2 normalized_move;
	touch_move_event_base(const uint64_t& time_,
						  const float2& position_,
						  const float2& move_,
						  const float& pressure_,
						  const uint64_t& id_) :
	touch_event_base<event_type>(time_, position_, pressure_, id_), normalized_move(move_) {}
};
using finger_down_event = touch_event_base<EVENT_TYPE::FINGER_DOWN>;
using finger_up_event = touch_event_base<EVENT_TYPE::FINGER_UP>;
using finger_move_event = touch_move_event_base<EVENT_TYPE::FINGER_MOVE>;

// misc
using quit_event = event_object_base<EVENT_TYPE::QUIT>;

struct clipboard_update_event : public event_object_base<EVENT_TYPE::CLIPBOARD_UPDATE> {
	const std::string text;
	clipboard_update_event(const uint64_t& time_, const std::string& text_) : event_object_base(time_), text(text_) {}
};

template<EVENT_TYPE event_type> struct window_resize_event_base : public event_object_base<event_type> {
	const uint2 size;
	window_resize_event_base(const uint64_t& time_, const uint2& size_) : event_object_base<event_type>(time_), size(size_) {}
};
using window_resize_event = window_resize_event_base<EVENT_TYPE::WINDOW_RESIZE>;

// VR controller events
template<EVENT_TYPE event_type> struct vr_event_base : public event_object_base<event_type> {
	const bool side; //! false: left, true: right
	vr_event_base(const uint64_t& time_, const bool& side_) :
	event_object_base<event_type>(time_), side(side_) {}
	
	bool is_left_controller() const {
		return !side;
	}
	bool is_right_controller() const {
		return side;
	}
};

template<EVENT_TYPE event_type> struct vr_digital_event_base : public vr_event_base<event_type> {
	const bool state;
	vr_digital_event_base(const uint64_t& time_, const bool& side_, const bool& state_) :
	vr_event_base<event_type>(time_, side_), state(state_) {}
};
using vr_app_menu_press_event = vr_digital_event_base<EVENT_TYPE::VR_APP_MENU_PRESS>;
using vr_app_menu_touch_event = vr_digital_event_base<EVENT_TYPE::VR_APP_MENU_TOUCH>;
using vr_main_press_event = vr_digital_event_base<EVENT_TYPE::VR_MAIN_PRESS>;
using vr_main_touch_event = vr_digital_event_base<EVENT_TYPE::VR_MAIN_TOUCH>;
using vr_system_press_event = vr_digital_event_base<EVENT_TYPE::VR_SYSTEM_PRESS>;
using vr_system_touch_event = vr_digital_event_base<EVENT_TYPE::VR_SYSTEM_TOUCH>;
using vr_trackpad_press_event = vr_digital_event_base<EVENT_TYPE::VR_TRACKPAD_PRESS>;
using vr_trackpad_touch_event = vr_digital_event_base<EVENT_TYPE::VR_TRACKPAD_TOUCH>;
using vr_thumbstick_press_event = vr_digital_event_base<EVENT_TYPE::VR_THUMBSTICK_PRESS>;
using vr_thumbstick_touch_event = vr_digital_event_base<EVENT_TYPE::VR_THUMBSTICK_TOUCH>;
using vr_thumbrest_touch_event = vr_digital_event_base<EVENT_TYPE::VR_THUMBREST_TOUCH>;
using vr_trigger_press_event = vr_digital_event_base<EVENT_TYPE::VR_TRIGGER_PRESS>;
using vr_trigger_touch_event = vr_digital_event_base<EVENT_TYPE::VR_TRIGGER_TOUCH>;
using vr_grip_press_event = vr_digital_event_base<EVENT_TYPE::VR_GRIP_PRESS>;
using vr_grip_touch_event = vr_digital_event_base<EVENT_TYPE::VR_GRIP_TOUCH>;
using vr_shoulder_press_event = vr_digital_event_base<EVENT_TYPE::VR_SHOULDER_PRESS>;

template<EVENT_TYPE event_type> struct vr_analog_move_event_base : public vr_event_base<event_type> {
	const float2 position;
	const float2 delta;
	vr_analog_move_event_base(const uint64_t& time_, const bool& side_, const float2& position_, const float2& delta_) :
	vr_event_base<event_type>(time_, side_), position(position_), delta(delta_) {}
};
using vr_trackpad_move_event = vr_analog_move_event_base<EVENT_TYPE::VR_TRACKPAD_MOVE>;
using vr_thumbstick_move_event = vr_analog_move_event_base<EVENT_TYPE::VR_THUMBSTICK_MOVE>;

template<EVENT_TYPE event_type> struct vr_analog_pull_event_base : public vr_event_base<event_type> {
	const float pull;
	const float delta;
	vr_analog_pull_event_base(const uint64_t& time_, const bool& side_, const float& pull_, const float& delta_) :
	vr_event_base<event_type>(time_, side_), pull(pull_), delta(delta_) {}
};
using vr_trigger_pull_event = vr_analog_pull_event_base<EVENT_TYPE::VR_TRIGGER_PULL>;
using vr_grip_pull_event = vr_analog_pull_event_base<EVENT_TYPE::VR_GRIP_PULL>;

template<EVENT_TYPE event_type> struct vr_analog_force_event_base : public vr_event_base<event_type> {
	const float force;
	const float delta;
	vr_analog_force_event_base(const uint64_t& time_, const bool& side_, const float& force_, const float& delta_) :
	vr_event_base<event_type>(time_, side_), force(force_), delta(delta_) {}
};
using vr_trackpad_force_event = vr_analog_force_event_base<EVENT_TYPE::VR_TRACKPAD_FORCE>;
using vr_grip_force_event = vr_analog_force_event_base<EVENT_TYPE::VR_GRIP_FORCE>;
using vr_thumbrest_force_event = vr_analog_force_event_base<EVENT_TYPE::VR_THUMBREST_FORCE>;

} // namespace fl
