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

#include <floor/threading/thread_base.hpp>
#include <floor/threading/atomic_spin_lock.hpp>
#include <floor/core/event_objects.hpp>
#include <SDL3/SDL_events.h>
#include <queue>
#include <memory>
#include <set>

namespace fl {

class vr_context;

//! SDL and VR event handler
class event : public thread_base {
public:
	event();
	~event() override;
	
	//! handles the SDL events
	void handle_events() REQUIRES(!queued_events_lock, !handler_lock);
	void add_event(const EVENT_TYPE type, std::shared_ptr<event_object> obj) REQUIRES(!queued_events_lock);
	
	void set_vr_context(vr_context* vr_ctx_) {
		vr_ctx = vr_ctx_;
	}
	
	// <returns true if handled, pointer to object, event type>
	using handler = std::function<bool(EVENT_TYPE, std::shared_ptr<event_object>)>;
	void add_event_handler(handler& handler_, EVENT_TYPE type) REQUIRES(!handler_lock);
	template<typename... event_types> void add_event_handler(handler& handler_, event_types&&... types) REQUIRES(!handler_lock) {
		// unwind types, always call the simple add handler for each type
		unwind_add_event_handler(handler_, std::forward<event_types>(types)...);
	}
	void add_internal_event_handler(handler& handler_, EVENT_TYPE type) REQUIRES(!handler_lock);
	template<typename... event_types> void add_internal_event_handler(handler& handler_, event_types&&... types) REQUIRES(!handler_lock) {
		// unwind types, always call the simple add handler for each type
		unwind_add_internal_event_handler(handler_, std::forward<event_types>(types)...);
	}
	
	// completely remove an event handler or only remove event types that are handled by an event handler
	void remove_event_handler(const handler& handler_) REQUIRES(!handler_lock);
	void remove_event_types_from_handler(const handler& handler_, const std::set<EVENT_TYPE>& types) REQUIRES(!handler_lock);
	
	//! returns the mouse position
	float2 get_mouse_pos() const;
	
	void set_ldouble_click_time(uint32_t dctime);
	void set_rdouble_click_time(uint32_t dctime);
	void set_mdouble_click_time(uint32_t dctime);
	
protected:
	SDL_Event event_handle;
	vr_context* vr_ctx { nullptr };
	
	void run() override REQUIRES(!handler_lock);
	
	//
	atomic_spin_lock handler_lock;
	std::unordered_multimap<EVENT_TYPE, handler&> internal_handlers GUARDED_BY(handler_lock);
	std::unordered_multimap<EVENT_TYPE, handler&> handlers GUARDED_BY(handler_lock);
	std::queue<std::pair<EVENT_TYPE, std::shared_ptr<event_object>>> user_event_queue, user_event_queue_processing;
	std::recursive_mutex user_queue_lock;
	void handle_user_events() REQUIRES(!handler_lock);
	void handle_event(const EVENT_TYPE& type, std::shared_ptr<event_object> obj) REQUIRES(!handler_lock);
	
	safe_mutex queued_events_lock;
	std::vector<std::pair<EVENT_TYPE, std::shared_ptr<event_object>>> queued_events;
	
	//
	std::unordered_map<EVENT_TYPE, std::shared_ptr<event_object>> prev_events;
	
	//! timer that decides if there is a * mouse double click
	uint64_t lm_double_click_timer { 0u };
	uint64_t rm_double_click_timer { 0u };
	uint64_t mm_double_click_timer { 0u };
	
	//! config setting for * mouse double click "timeframe"
	uint32_t ldouble_click_time { 200u };
	uint32_t rdouble_click_time { 200u };
	uint32_t mdouble_click_time { 200u };
	
	//
	void unwind_add_event_handler(handler& handler_, EVENT_TYPE type) REQUIRES(!handler_lock) {
		add_event_handler(handler_, type);
	}
	template<typename... event_types> void unwind_add_event_handler(handler& handler_, EVENT_TYPE type, event_types&&... types) REQUIRES(!handler_lock) {
		// unwind types, always call the simple add handler for each type
		add_event_handler(handler_, type);
		unwind_add_event_handler(handler_, std::forward<event_types>(types)...);
	}
	void unwind_add_internal_event_handler(handler& handler_, EVENT_TYPE type) REQUIRES(!handler_lock) {
		add_internal_event_handler(handler_, type);
	}
	template<typename... event_types> void unwind_add_internal_event_handler(handler& handler_, EVENT_TYPE type, event_types&&... types) REQUIRES(!handler_lock) {
		// unwind types, always call the simple add handler for each type
		add_internal_event_handler(handler_, type);
		unwind_add_internal_event_handler(handler_, std::forward<event_types>(types)...);
	}
	
};

} // namespace fl
