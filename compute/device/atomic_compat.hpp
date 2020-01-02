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

#ifndef __FLOOR_COMPUTE_DEVICE_ATOMIC_COMPAT_HPP__
#define __FLOOR_COMPUTE_DEVICE_ATOMIC_COMPAT_HPP__

// provide compatibility/alias functions for libc++s <atomic> header (replacing the non-functional __c11_* builtins)

#if !defined(FLOOR_COMPUTE_HOST)
typedef enum memory_order {
	memory_order_relaxed,
	
	// by default, none of these should be used and an error will be emitted if they are.
	// however, for the sake of compatibility, this behavior can be disabled and all enums will simply alias to memory_order_relaxed
#if !defined(FLOOR_COMPUTE_MEMORY_ORDER_UNSAFE)
	memory_order_consume __attribute__((unavailable("not supported"))),
	memory_order_acquire __attribute__((unavailable("not supported"))),
	memory_order_release __attribute__((unavailable("not supported"))),
	memory_order_acq_rel __attribute__((unavailable("not supported"))),
	memory_order_seq_cst __attribute__((unavailable("not supported")))
#else
	memory_order_consume = memory_order_relaxed,
	memory_order_acquire = memory_order_relaxed,
	memory_order_release = memory_order_relaxed,
	memory_order_acq_rel = memory_order_relaxed,
	memory_order_seq_cst = memory_order_relaxed
#endif
} memory_order;
#endif

constexpr bool floor_atomic_is_lock_free(const size_t& size) {
#if !defined(FLOOR_COMPUTE_INFO_HAS_64_BIT_ATOMICS_1)
	return (size == 4u);
#else
	return (size == 4u || size == 8u);
#endif
}

#if !defined(FLOOR_COMPUTE_INFO_HAS_64_BIT_ATOMICS_1)
#define FLOOR_ATOMIC_LONG_LOCK_FREE false
#define FLOOR_ATOMIC_LLONG_LOCK_FREE false
#define FLOOR_ATOMIC_POINTER_LOCK_FREE false
#else
#define FLOOR_ATOMIC_LONG_LOCK_FREE true
#define FLOOR_ATOMIC_LLONG_LOCK_FREE true
#define FLOOR_ATOMIC_POINTER_LOCK_FREE true
#endif

template <typename T> T floor_atomic_fetch_add(global T* addr, const T& val, memory_order) { return atomic_add(addr, val); }
template <typename T> T floor_atomic_fetch_sub(global T* addr, const T& val, memory_order) { return atomic_sub(addr, val); }
template <typename T> T floor_atomic_fetch_inc(global T* addr, memory_order) { return atomic_inc(addr); }
template <typename T> T floor_atomic_fetch_dec(global T* addr, memory_order) { return atomic_dec(addr); }
template <typename T> T floor_atomic_fetch_and(global T* addr, const T& val, memory_order) { return atomic_and(addr, val); }
template <typename T> T floor_atomic_fetch_or(global T* addr, const T& val, memory_order) { return atomic_or(addr, val); }
template <typename T> T floor_atomic_fetch_xor(global T* addr, const T& val, memory_order) { return atomic_xor(addr, val); }
template <typename T> T floor_atomic_exchange(global T* addr, const T& val, memory_order) { return atomic_xchg(addr, val); }
template <typename T> void floor_atomic_store(global T* addr, const T& val, memory_order) { atomic_store(addr, val); }
template <typename T> void floor_atomic_init(global T* addr, const T& val, memory_order) { atomic_store(addr, val); }
template <typename T> T floor_atomic_load(global T* addr, memory_order) { return atomic_load(addr); }
template <typename T> bool floor_atomic_compare_exchange_weak(global T* addr, const T* expected, const T& desired,
															  memory_order, memory_order) {
	const auto old = *expected;
	return (atomic_cmpxchg(addr, *expected, desired) == old);
}

#if !defined(FLOOR_COMPUTE_CUDA) && !defined(FLOOR_COMPUTE_HOST) // cuda and host don't require address space specialization
template <typename T> T floor_atomic_fetch_add(local T* addr, const T& val, memory_order) { return atomic_add(addr, val); }
template <typename T> T floor_atomic_fetch_sub(local T* addr, const T& val, memory_order) { return atomic_sub(addr, val); }
template <typename T> T floor_atomic_fetch_inc(local T* addr, memory_order) { return atomic_inc(addr); }
template <typename T> T floor_atomic_fetch_dec(local T* addr, memory_order) { return atomic_dec(addr); }
template <typename T> T floor_atomic_fetch_and(local T* addr, const T& val, memory_order) { return atomic_and(addr, val); }
template <typename T> T floor_atomic_fetch_or(local T* addr, const T& val, memory_order) { return atomic_or(addr, val); }
template <typename T> T floor_atomic_fetch_xor(local T* addr, const T& val, memory_order) { return atomic_xor(addr, val); }
template <typename T> T floor_atomic_exchange(local T* addr, const T& val, memory_order) { return atomic_xchg(addr, val); }
template <typename T> void floor_atomic_store(local T* addr, const T& val, memory_order) { atomic_store(addr, val); }
template <typename T> void floor_atomic_init(local T* addr, const T& val, memory_order) { atomic_store(addr, val); }
template <typename T> T floor_atomic_load(local T* addr, memory_order) { return atomic_load(addr); }
template <typename T> bool floor_atomic_compare_exchange_weak(local T* addr, const T* expected, const T& desired,
															  memory_order, memory_order) {
	const auto old = *expected;
	return (atomic_cmpxchg(addr, *expected, desired) == old);
}
#endif

// this is not supported (or in any way useful with just memory_order_relaxed)
__attribute__((deprecated("not supported"))) floor_inline_always static void floor_atomic_thread_fence(memory_order) {
	// nop
}

// this is a compiler instruction, try using it (no guarantees though)
#define floor_atomic_signal_fence(mem_order) __c11_atomic_signal_fence(mem_order)

#endif
