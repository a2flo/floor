/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2020 Florian Ziesche
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

#include <floor/core/util.hpp>
#include <floor/core/platform.hpp>
#include <floor/core/event_objects.hpp>

#if !defined(FLOOR_NO_EXCEPTIONS)
const char* floor_exception::what() const noexcept {
	return error_str.c_str();
}
#endif

// from event_objects.h:
EVENT_TYPE operator&(const EVENT_TYPE& e0, const EVENT_TYPE& e1) {
	return (EVENT_TYPE)((underlying_type_t<EVENT_TYPE>)e0 &
						(underlying_type_t<EVENT_TYPE>)e1);
}
