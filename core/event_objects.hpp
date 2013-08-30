/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2013 Florian Ziesche
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

#ifndef __FLOOR_EVENT_OBJECTS_HPP__
#define __FLOOR_EVENT_OBJECTS_HPP__

#if !defined(OCLRASTER_USER_EVENT_TYPES)
#define OCLRASTER_USER_EVENT_TYPES
#endif

#include "core/vector2.h"

// general/global event types
enum class EVENT_TYPE : unsigned int {
	__MOUSE_EVENT	= (1u << 31u),
	__KEY_EVENT		= (1u << 30u),
	__TOUCH_EVENT	= (1u << 29u),
	__GUI_EVENT		= (1u << 28u),
	__OTHER_EVENT	= (1u << 27u),
	__USER_EVENT	= (1u << 26u),
	
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
	KERNEL_RELOAD,
	CLIPBOARD_UPDATE,
	
	__USER_EVENT_START = __USER_EVENT + 1,
	OCLRASTER_USER_EVENT_TYPES
};
OCLRASTER_API EVENT_TYPE operator&(const EVENT_TYPE& e0, const EVENT_TYPE& e1);
namespace std {
	template <> struct hash<EVENT_TYPE> : public hash<unsigned int> {
		size_t operator()(EVENT_TYPE type) const throw() {
			return hash<unsigned int>::operator()((unsigned int)type);
		}
	};
}

//
struct event_object {
	const unsigned int time;
	event_object(const unsigned int& time_) : time(time_) {}
};
template<EVENT_TYPE event_type> struct event_object_base : public event_object {
	const EVENT_TYPE type;
	event_object_base(const unsigned int& time_) : event_object(time_), type(event_type) {}
};

// mouse events
template<EVENT_TYPE event_type> struct mouse_event_base : public event_object_base<event_type> {
	const ipnt position;
	mouse_event_base(const unsigned int& time_, const ipnt& position_) : event_object_base<event_type>(time_), position(position_) {}
};

template<EVENT_TYPE event_type, EVENT_TYPE down_event_type, EVENT_TYPE up_event_type> struct mouse_click_event : public event_object_base<event_type> {
	shared_ptr<mouse_event_base<down_event_type>> down;
	shared_ptr<mouse_event_base<up_event_type>> up;
	mouse_click_event(const unsigned int& time_,
					  shared_ptr<event_object> down_evt,
					  shared_ptr<event_object> up_evt)
	: event_object_base<event_type>(time_),
	down(*(shared_ptr<mouse_event_base<down_event_type>>*)&down_evt),
	up(*(shared_ptr<mouse_event_base<up_event_type>>*)&up_evt) {}
};

template<EVENT_TYPE event_type> struct mouse_move_event_base : public mouse_event_base<event_type> {
	const ipnt move;
	mouse_move_event_base(const unsigned int& time_,
						  const ipnt& position_,
						  const ipnt& move_)
	: mouse_event_base<event_type>(time_, position_), move(move_) {}
};

template<EVENT_TYPE event_type> struct mouse_wheel_event_base : public mouse_event_base<event_type> {
	const unsigned int amount;
	mouse_wheel_event_base(const unsigned int& time_,
						   const ipnt& position_,
						   const unsigned int& amount_)
	: mouse_event_base<event_type>(time_, position_), amount(amount_) {}
};

// mouse event typedefs
typedef mouse_event_base<EVENT_TYPE::MOUSE_LEFT_DOWN> mouse_left_down_event;
typedef mouse_event_base<EVENT_TYPE::MOUSE_LEFT_UP> mouse_left_up_event;
typedef mouse_event_base<EVENT_TYPE::MOUSE_LEFT_HOLD> mouse_left_hold_event;
typedef mouse_click_event<EVENT_TYPE::MOUSE_LEFT_CLICK, EVENT_TYPE::MOUSE_LEFT_DOWN, EVENT_TYPE::MOUSE_LEFT_UP> mouse_left_click_event;
typedef mouse_click_event<EVENT_TYPE::MOUSE_LEFT_DOUBLE_CLICK, EVENT_TYPE::MOUSE_LEFT_DOWN, EVENT_TYPE::MOUSE_LEFT_UP> mouse_left_double_click_event;

typedef mouse_event_base<EVENT_TYPE::MOUSE_RIGHT_DOWN> mouse_right_down_event;
typedef mouse_event_base<EVENT_TYPE::MOUSE_RIGHT_UP> mouse_right_up_event;
typedef mouse_event_base<EVENT_TYPE::MOUSE_RIGHT_HOLD> mouse_right_hold_event;
typedef mouse_click_event<EVENT_TYPE::MOUSE_RIGHT_CLICK, EVENT_TYPE::MOUSE_RIGHT_DOWN, EVENT_TYPE::MOUSE_RIGHT_UP> mouse_right_click_event;
typedef mouse_click_event<EVENT_TYPE::MOUSE_RIGHT_DOUBLE_CLICK, EVENT_TYPE::MOUSE_RIGHT_DOWN, EVENT_TYPE::MOUSE_RIGHT_UP> mouse_right_double_click_event;

typedef mouse_event_base<EVENT_TYPE::MOUSE_MIDDLE_DOWN> mouse_middle_down_event;
typedef mouse_event_base<EVENT_TYPE::MOUSE_MIDDLE_UP> mouse_middle_up_event;
typedef mouse_event_base<EVENT_TYPE::MOUSE_MIDDLE_HOLD> mouse_middle_hold_event;
typedef mouse_click_event<EVENT_TYPE::MOUSE_MIDDLE_CLICK, EVENT_TYPE::MOUSE_MIDDLE_DOWN, EVENT_TYPE::MOUSE_MIDDLE_UP> mouse_middle_click_event;
typedef mouse_click_event<EVENT_TYPE::MOUSE_MIDDLE_DOUBLE_CLICK, EVENT_TYPE::MOUSE_MIDDLE_DOWN, EVENT_TYPE::MOUSE_MIDDLE_UP> mouse_middle_double_click_event;

typedef mouse_move_event_base<EVENT_TYPE::MOUSE_MOVE> mouse_move_event;

typedef mouse_wheel_event_base<EVENT_TYPE::MOUSE_WHEEL_UP> mouse_wheel_up_event;
typedef mouse_wheel_event_base<EVENT_TYPE::MOUSE_WHEEL_DOWN> mouse_wheel_down_event;

// key events
template<EVENT_TYPE event_type> struct key_event : public event_object_base<event_type> {
	const unsigned int key;
	key_event(const unsigned int& time_,
			  const unsigned int& key_) :
	event_object_base<event_type>(time_), key(key_) {}
};
typedef key_event<EVENT_TYPE::KEY_DOWN> key_down_event;
typedef key_event<EVENT_TYPE::KEY_UP> key_up_event;
typedef key_event<EVENT_TYPE::KEY_HOLD> key_hold_event;
typedef key_event<EVENT_TYPE::UNICODE_INPUT> unicode_input_event;

// touch events
template<EVENT_TYPE event_type> struct touch_event_base : public event_object_base<event_type> {
	const ipnt position;
	const unsigned int pressure;
	const unsigned long long int id;
	touch_event_base(const unsigned int& time_, const ipnt& position_, const unsigned int& pressure_, const unsigned long long int& id_) : event_object_base<event_type>(time_), position(position_), pressure(pressure_), id(id_) {}
};
template<EVENT_TYPE event_type> struct touch_move_event_base : public touch_event_base<event_type> {
	const ipnt move;
	touch_move_event_base(const unsigned int& time_,
						  const ipnt& position_,
						  const ipnt& move_,
						  const unsigned int& pressure_,
						  const unsigned long long int& id_)
	: touch_event_base<event_type>(time_, position_, pressure_, id_), move(move_) {}
};
typedef touch_event_base<EVENT_TYPE::FINGER_DOWN> finger_down_event;
typedef touch_event_base<EVENT_TYPE::FINGER_UP> finger_up_event;
typedef touch_move_event_base<EVENT_TYPE::FINGER_MOVE> finger_move_event;

// misc
typedef event_object_base<EVENT_TYPE::QUIT> quit_event;
typedef event_object_base<EVENT_TYPE::KERNEL_RELOAD> kernel_reload_event;

struct clipboard_update_event : public event_object_base<EVENT_TYPE::CLIPBOARD_UPDATE> {
	const string text;
	clipboard_update_event(const unsigned int& time_, const string& text_) : event_object_base(time_), text(text_) {}
};

template<EVENT_TYPE event_type> struct window_resize_event_base : public event_object_base<event_type> {
	const size2 size;
	window_resize_event_base(const unsigned int& time_, const size2& size_) : event_object_base<event_type>(time_), size(size_) {}
};
typedef window_resize_event_base<EVENT_TYPE::WINDOW_RESIZE> window_resize_event;

#endif
