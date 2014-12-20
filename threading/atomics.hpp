/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2014 Florian Ziesche
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

#ifndef __FLOOR_ATOMICS_HPP__
#define __FLOOR_ATOMICS_HPP__

#include <atomic>
#include <floor/core/essentials.hpp>

//! performs an atomic "object-val = max(object-val, desired-val)" operation
template <class T>
floor_inline_always void atomic_max(atomic<T>& dst, const T& desired) {
	T expected = dst.load();
	while(!dst.compare_exchange_weak(expected, desired)) {
		// if this has already been updated in the meantime, we don't need to modify it any more
		if(expected >= desired) {
			break;
		}
	}
}

#endif
