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

#include <atomic>
#include <thread>
#include <floor/threading/thread_safety.hpp>
#include <floor/core/essentials.hpp>

namespace fl {

// improved atomic spin lock based on:
// https://probablydance.com/2019/12/30/measuring-mutexes-spinlocks-and-how-bad-the-linux-scheduler-really-is/
// https://gpuopen.com/gdc-presentations/2019/gdc-2019-s2-amd-ryzen-processor-software-optimization.pdf
// https://github.com/skarupke/mutex_benchmarks/blob/master/BenchmarkMutex.cpp
template <bool aligned = true>
class CAPABILITY("mutex") alignas(aligned ? 64u : 4u) atomic_spin_lock_t {
public:
	constexpr floor_inline_always atomic_spin_lock_t() noexcept = default;
	
	floor_inline_always atomic_spin_lock_t(atomic_spin_lock_t&& spin_lock) noexcept {
		mtx = spin_lock.mtx.load();
		spin_lock.mtx = false;
	}
	
	floor_inline_always atomic_spin_lock_t& operator=(atomic_spin_lock_t&& spin_lock) noexcept {
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
#if defined(__x86_64__)
				asm volatile("pause" : : : "memory");
#elif defined(__aarch64__)
				asm volatile("yield" : : : "memory");
#else
#error "unknown arch"
#endif
			} else {
				// ... and after the 16th attempt: actually yield the thread (then start again)
				std::this_thread::yield();
				trial = 0;
			}
		}
	}
	floor_inline_always bool try_lock() TRY_ACQUIRE(true) {
		// AMD recommendation to prevent unnecessary cache line invalidation (due to write):
		// load/read first (and fail if lock is taken), only then try to exchange/write memory
		// when we know that the lock is potentially not taken right now
		return (!mtx.load(std::memory_order_relaxed) && !mtx.exchange(true, std::memory_order_acquire));
	}
	floor_inline_always void unlock() RELEASE() {
		// resets flag to false
		mtx.store(false, std::memory_order_release);
	}
	
	// for negative capabilities
	floor_inline_always const atomic_spin_lock_t& operator!() const { return *this; }
	
protected:
	std::atomic<bool> mtx { false };
	static_assert(sizeof(std::atomic<bool>) <= 4u);
	
};

//! atomic spin lock with default ideal 64-byte alignment
using atomic_spin_lock = atomic_spin_lock_t<true>;
//! atomic spin lock with only 4 byte alignment (to contain the atomic)
//! NOTE: use this if you want to specify the alignment externally or encompass this within an already aligned structure
using atomic_spin_lock_unaligned = atomic_spin_lock_t<false>;

} // namespace fl
