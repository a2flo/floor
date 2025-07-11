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
#include <functional>
#include <floor/threading/thread_safety.hpp>
#include <floor/core/essentials.hpp>

//! instruct/hint the CPU that a spin-loop wait is performed
#if defined(__x86_64__)
#define FLOOR_SPIN_WAIT() asm volatile("pause" : : : "memory")
#elif defined(__aarch64__)
#define FLOOR_SPIN_WAIT() asm volatile("yield" : : : "memory")
#else
#error "unknown arch"
#endif

namespace fl {

//! performs a spin-wait loop until "conditional()" returns true
template <typename conditional_type, typename... Args>
static floor_inline_always void spin_wait_condition(conditional_type&& conditional, Args&&... args) {
#pragma nounroll
	for (uint32_t trial = 0; ; ++trial) {
		// "conditional" is an invocable function
		if constexpr (std::is_invocable_v<conditional_type, Args...>) {
			if (conditional(args...)) {
				break;
			}
		}
		// "conditional" evaluates to a bool (no args)
		else {
			static_assert(sizeof...(args) == 0);
			if (conditional) {
				break;
			}
		}
		
		if (trial < 16) {
			// AMD recommendation: "pause" when lock could not be acquired (b/c SMT)
			// Malte recommendation: to improve latency, only try this 16 times ...
			FLOOR_SPIN_WAIT();
		} else {
			// ... and after the 16th attempt: actually yield the thread (then start again)
			std::this_thread::yield();
			trial = 0;
		}
	}
}

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
		spin_wait_condition([this]() { return this->try_lock(); });
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
