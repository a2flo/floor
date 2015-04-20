/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2015 Florian Ziesche
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

#ifndef __FLOOR_EVENT_HPP__
#define __FLOOR_EVENT_HPP__

#include <floor/core/core.hpp>
#include <floor/threading/thread_base.hpp>
#include <floor/core/event_objects.hpp>

//! (sdl) event handler
class FLOOR_API event : public thread_base {
public:
	event();
	virtual ~event();

	void handle_events();
	void add_event(const EVENT_TYPE type, shared_ptr<event_object> obj);
	
	// <returns true if handled, pointer to object, event type>
	typedef function<bool(EVENT_TYPE, shared_ptr<event_object>)> handler;
	void add_event_handler(handler& handler_, EVENT_TYPE type);
	template<typename... event_types> void add_event_handler(handler& handler_, event_types&&... types) {
		// unwind types, always call the simple add handler for each type
		unwind_add_event_handler(handler_, std::forward<event_types>(types)...);
	}
	void add_internal_event_handler(handler& handler_, EVENT_TYPE type);
	template<typename... event_types> void add_internal_event_handler(handler& handler_, event_types&&... types) {
		// unwind types, always call the simple add handler for each type
		unwind_add_internal_event_handler(handler_, std::forward<event_types>(types)...);
	}
	
	// completely remove an event handler or only remove event types that are handled by an event handler
	void remove_event_handler(const handler& handler_);
	void remove_event_types_from_handler(const handler& handler_, const set<EVENT_TYPE>& types);

	//! gets the mouses position (pnt)
	pnt get_mouse_pos() const;
	
	void set_ldouble_click_time(unsigned int dctime);
	void set_rdouble_click_time(unsigned int dctime);
	void set_mdouble_click_time(unsigned int dctime);

protected:
	SDL_Event event_handle;
	
	virtual void run();
	
	//
	unordered_multimap<EVENT_TYPE, handler&> internal_handlers;
	unordered_multimap<EVENT_TYPE, handler&> handlers;
	queue<pair<EVENT_TYPE, shared_ptr<event_object>>> user_event_queue, user_event_queue_processing;
	recursive_mutex user_queue_lock;
	atomic<int> handlers_lock { 0 };
	static constexpr int handlers_locked { (int)0x80000000 };
	void handle_user_events();
	void handle_event(const EVENT_TYPE& type, shared_ptr<event_object> obj);
	
	//
	unordered_map<EVENT_TYPE, shared_ptr<event_object>> prev_events;
	
	//! timer that decides if there is a * mouse double click
	unsigned int lm_double_click_timer;
	unsigned int rm_double_click_timer;
	unsigned int mm_double_click_timer;
	
	//! config setting for * mouse double click "timeframe"
	unsigned int ldouble_click_time = 200;
	unsigned int rdouble_click_time = 200;
	unsigned int mdouble_click_time = 200;
	
	//
	void unwind_add_event_handler(handler& handler_, EVENT_TYPE type) {
		add_event_handler(handler_, type);
	}
	template<typename... event_types> void unwind_add_event_handler(handler& handler_, EVENT_TYPE type, event_types&&... types) {
		// unwind types, always call the simple add handler for each type
		add_event_handler(handler_, type);
		unwind_add_event_handler(handler_, std::forward<event_types>(types)...);
	}
	void unwind_add_internal_event_handler(handler& handler_, EVENT_TYPE type) {
		add_internal_event_handler(handler_, type);
	}
	template<typename... event_types> void unwind_add_internal_event_handler(handler& handler_, EVENT_TYPE type, event_types&&... types) {
		// unwind types, always call the simple add handler for each type
		add_internal_event_handler(handler_, type);
		unwind_add_internal_event_handler(handler_, std::forward<event_types>(types)...);
	}

};

#endif
