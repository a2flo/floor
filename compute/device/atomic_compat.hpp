/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2024 Florian Ziesche
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

#if !defined(FLOOR_COMPUTE_HOST) || defined(FLOOR_COMPUTE_HOST_DEVICE)
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
#else
#include <atomic>
#endif

constexpr bool floor_atomic_is_lock_free(const size_t& size) {
#if !defined(FLOOR_COMPUTE_INFO_HAS_64_BIT_ATOMICS_1)
	return (size == 4u);
#else
	return (size == 4u || size == 8u);
#endif
}

#if !defined(FLOOR_COMPUTE_INFO_HAS_64_BIT_ATOMICS_1)
#define FLOOR_ATOMIC_LONG_LOCK_FREE 0
#define FLOOR_ATOMIC_LLONG_LOCK_FREE 0
#define FLOOR_ATOMIC_POINTER_LOCK_FREE 0
#else
#define FLOOR_ATOMIC_LONG_LOCK_FREE 2
#define FLOOR_ATOMIC_LLONG_LOCK_FREE 2
#define FLOOR_ATOMIC_POINTER_LOCK_FREE 2
#endif

// this is not supported (or in any way useful with just memory_order_relaxed)
__attribute__((deprecated("not supported"))) floor_inline_always static void floor_atomic_thread_fence(memory_order) {
	// nop
}

// this is a compiler instruction, try using it (no guarantees though)
#define floor_atomic_signal_fence(mem_order) __c11_atomic_signal_fence(mem_order)

#endif
