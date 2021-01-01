/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2021 Florian Ziesche
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
#include <thread>
#include <floor/threading/thread_safety.hpp>
#include <floor/core/essentials.hpp>
using namespace std;

// improved atomic spin lock based on:
// https://probablydance.com/2019/12/30/measuring-mutexes-spinlocks-and-how-bad-the-linux-scheduler-really-is/
// https://gpuopen.com/gdc-presentations/2019/gdc-2019-s2-amd-ryzen-processor-software-optimization.pdf
// https://github.com/skarupke/mutex_benchmarks/blob/master/BenchmarkMutex.cpp
class CAPABILITY("mutex") atomic_spin_lock {
public:
	constexpr floor_inline_always atomic_spin_lock() noexcept = default;

	floor_inline_always atomic_spin_lock(atomic_spin_lock&& spin_lock) noexcept {
		mtx = spin_lock.mtx.load();
		spin_lock.mtx = false;
	}
	
	floor_inline_always atomic_spin_lock& operator=(atomic_spin_lock&& spin_lock) noexcept {
		mtx = spin_lock.mtx.load();
		spin_lock.mtx = false;
		return *this;
	}

	floor_inline_always void lock() ACQUIRE() {
#pragma nounroll
		for (uint32_t trial = 0; !try_lock(); ++trial) {
			if (trial < 16) {
				// AMD recommendation: "pause" when lock could not be acquired (b/c SMT)
				// Malte recommendation: to improve latency, only try this 16 times ...
#if !defined(FLOOR_IOS)
				asm volatile("pause" : : : "memory"); // x86
#else
				asm volatile("yield" : : : "memory"); // ARM
#endif
			} else {
				// ... and after the 16th attempt: actually yield the thread (then start again)
				this_thread::yield();
				trial = 0;
			}
		}
	}
	floor_inline_always bool try_lock() TRY_ACQUIRE(true) {
		// AMD recommendation to prevent unnecessary cache line invalidation (due to write):
		// load/read first (and fail if lock is taken), only then try to exchange/write memory
		// when we know that the lock is potentially not taken right now
		return (!mtx.load(memory_order_relaxed) && !mtx.exchange(true, memory_order_acquire));
	}
	floor_inline_always void unlock() RELEASE() {
		// resets flag to false
		mtx.store(false, memory_order_release);
	}
	
	// for negative capabilities
	floor_inline_always const atomic_spin_lock& operator!() const { return *this; }
	
protected:
	alignas(64) atomic<bool> mtx { false };
	
};

#endif
