/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2017 Florian Ziesche
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

#ifndef __FLOOR_ATOMIC_SPIN_LOCK_HPP__
#define __FLOOR_ATOMIC_SPIN_LOCK_HPP__

#include <atomic>
#include <floor/threading/thread_safety.hpp>
#include <floor/core/essentials.hpp>
using namespace std;

class CAPABILITY("mutex") atomic_spin_lock {
public:
	floor_inline_always void lock() ACQUIRE() {
		// as long as this succeeds (returns true), the lock is already acquired
		while(mtx.test_and_set(memory_order_acquire)) {
			// wait
		}
	}
	floor_inline_always bool try_lock() TRY_ACQUIRE(true) {
		// test_and_set returns true if already locked
		return !mtx.test_and_set(memory_order_acquire);
	}
	floor_inline_always void unlock() RELEASE() {
		// sets flag to 0/false
		mtx.clear(memory_order_release);
	}
	
	// for negative capabilities
	floor_inline_always const atomic_spin_lock& operator!() const { return *this; }
	
protected:
	atomic_flag mtx = ATOMIC_FLAG_INIT;
	
};

#endif
