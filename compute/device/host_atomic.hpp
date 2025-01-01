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

#if defined(FLOOR_COMPUTE_HOST)

// already include this here, since the host uses a vanilla stl/libc++
#include <atomic>

// when using a full llvm toolchain, use the __c11_atomic_* builtins
#define floor_host_atomic_exchange(...) __c11_atomic_exchange(__VA_ARGS__)
#define floor_host_atomic_compare_exchange_strong(...) __c11_atomic_compare_exchange_strong(__VA_ARGS__)
#define floor_host_atomic_fetch_add(...) __c11_atomic_fetch_add(__VA_ARGS__)
#define floor_host_atomic_fetch_sub(...) __c11_atomic_fetch_sub(__VA_ARGS__)
#define floor_host_atomic_fetch_and(...) __c11_atomic_fetch_and(__VA_ARGS__)
#define floor_host_atomic_fetch_or(...) __c11_atomic_fetch_or(__VA_ARGS__)
#define floor_host_atomic_fetch_xor(...) __c11_atomic_fetch_xor(__VA_ARGS__)
#define floor_host_atomic_fetch_min(...) __c11_atomic_fetch_min(__VA_ARGS__)
#define floor_host_atomic_fetch_max(...) __c11_atomic_fetch_max(__VA_ARGS__)
#define floor_host_atomic_load(...) __c11_atomic_load(__VA_ARGS__)
#define floor_host_atomic_store(...) __c11_atomic_store(__VA_ARGS__)

// for compat with old and new libc++
enum floor_memory_order {
	floor_memory_order_relaxed,
	floor_memory_order_consume,
	floor_memory_order_acquire,
	floor_memory_order_release,
	floor_memory_order_acq_rel,
	floor_memory_order_seq_cst,
};

// cmpxchg (up here, because it's needed by the fallback implementations)
floor_inline_always int32_t atomic_cmpxchg(volatile int32_t* p, int32_t cmp, int32_t val) {
	floor_host_atomic_compare_exchange_strong((volatile _Atomic(int32_t)*)p, &cmp, val,
											  floor_memory_order_seq_cst, floor_memory_order_seq_cst);
	return cmp;
}
floor_inline_always uint32_t atomic_cmpxchg(volatile uint32_t* p, uint32_t cmp, uint32_t val) {
	floor_host_atomic_compare_exchange_strong((volatile _Atomic(uint32_t)*)p, &cmp, val,
											  floor_memory_order_seq_cst, floor_memory_order_seq_cst);
	return cmp;
}
floor_inline_always float atomic_cmpxchg(volatile float* p, float cmp, float val) {
	const auto ret = atomic_cmpxchg((volatile uint32_t*)p, *(uint32_t*)&cmp, *(uint32_t*)&val);
	return *(const float*)&ret;
}
floor_inline_always int64_t atomic_cmpxchg(volatile int64_t* p, int64_t cmp, int64_t val) {
	floor_host_atomic_compare_exchange_strong((volatile _Atomic(int64_t)*)p, &cmp, val,
											  floor_memory_order_seq_cst, floor_memory_order_seq_cst);
	return cmp;
}
floor_inline_always uint64_t atomic_cmpxchg(volatile uint64_t* p, uint64_t cmp, uint64_t val) {
	floor_host_atomic_compare_exchange_strong((volatile _Atomic(uint64_t)*)p, &cmp, val,
											  floor_memory_order_seq_cst, floor_memory_order_seq_cst);
	return cmp;
}
floor_inline_always double atomic_cmpxchg(volatile double* p, double cmp, double val) {
	const auto ret = atomic_cmpxchg((volatile uint64_t*)p, *(uint64_t*)&cmp, *(uint64_t*)&val);
	return *(const double*)&ret;
}

// add
floor_inline_always int32_t atomic_add(volatile int32_t* p, int32_t val) {
	return floor_host_atomic_fetch_add((volatile _Atomic(int32_t)*)p, val, floor_memory_order_seq_cst);
}
floor_inline_always uint32_t atomic_add(volatile uint32_t* p, uint32_t val) {
	return floor_host_atomic_fetch_add((volatile _Atomic(uint32_t)*)p, val, floor_memory_order_seq_cst);
}
floor_inline_always float atomic_add(volatile float* p, float val) {
	return floor_host_atomic_fetch_add((volatile _Atomic(float)*)p, val, floor_memory_order_seq_cst);
}
floor_inline_always int64_t atomic_add(volatile int64_t* p, int64_t val) {
	return floor_host_atomic_fetch_add((volatile _Atomic(int64_t)*)p, val, floor_memory_order_seq_cst);
}
floor_inline_always uint64_t atomic_add(volatile uint64_t* p, uint64_t val) {
	return floor_host_atomic_fetch_add((volatile _Atomic(uint64_t)*)p, val, floor_memory_order_seq_cst);
}
floor_inline_always double atomic_add(volatile double* p, double val) {
	return floor_host_atomic_fetch_add((volatile _Atomic(double)*)p, val, floor_memory_order_seq_cst);
}

// sub
floor_inline_always int32_t atomic_sub(volatile int32_t* p, int32_t val) {
	return floor_host_atomic_fetch_sub((volatile _Atomic(int32_t)*)p, val, floor_memory_order_seq_cst);
}
floor_inline_always uint32_t atomic_sub(volatile uint32_t* p, uint32_t val) {
	return floor_host_atomic_fetch_sub((volatile _Atomic(uint32_t)*)p, val, floor_memory_order_seq_cst);
}
floor_inline_always float atomic_sub(volatile float* p, float val) {
	return floor_host_atomic_fetch_sub((volatile _Atomic(float)*)p, val, floor_memory_order_seq_cst);
}
floor_inline_always int64_t atomic_sub(volatile int64_t* p, int64_t val) {
	return floor_host_atomic_fetch_sub((volatile _Atomic(int64_t)*)p, val, floor_memory_order_seq_cst);
}
floor_inline_always uint64_t atomic_sub(volatile uint64_t* p, uint64_t val) {
	return floor_host_atomic_fetch_sub((volatile _Atomic(uint64_t)*)p, val, floor_memory_order_seq_cst);
}
floor_inline_always double atomic_sub(volatile double* p, double val) {
	return floor_host_atomic_fetch_sub((volatile _Atomic(double)*)p, val, floor_memory_order_seq_cst);
}

// inc
floor_inline_always int32_t atomic_inc(volatile int32_t* p) {
	return floor_host_atomic_fetch_add((volatile _Atomic(int32_t)*)p, 1, floor_memory_order_seq_cst);
}
floor_inline_always uint32_t atomic_inc(volatile uint32_t* p) {
	return floor_host_atomic_fetch_add((volatile _Atomic(uint32_t)*)p, 1u, floor_memory_order_seq_cst);
}
floor_inline_always float atomic_inc(volatile float* p) {
	return floor_host_atomic_fetch_add((volatile _Atomic(float)*)p, 1.0f, floor_memory_order_seq_cst);
}
floor_inline_always int64_t atomic_inc(volatile int64_t* p) {
	return floor_host_atomic_fetch_add((volatile _Atomic(int64_t)*)p, 1ll, floor_memory_order_seq_cst);
}
floor_inline_always uint64_t atomic_inc(volatile uint64_t* p) {
	return floor_host_atomic_fetch_add((volatile _Atomic(uint64_t)*)p, 1ull, floor_memory_order_seq_cst);
}
floor_inline_always double atomic_inc(volatile double* p) {
	return floor_host_atomic_fetch_add((volatile _Atomic(double)*)p, 1.0, floor_memory_order_seq_cst);
}

// dec
floor_inline_always int32_t atomic_dec(volatile int32_t* p) {
	return floor_host_atomic_fetch_sub((volatile _Atomic(int32_t)*)p, 1, floor_memory_order_seq_cst);
}
floor_inline_always uint32_t atomic_dec(volatile uint32_t* p) {
	return floor_host_atomic_fetch_sub((volatile _Atomic(uint32_t)*)p, 1u, floor_memory_order_seq_cst);
}
floor_inline_always float atomic_dec(volatile float* p) {
	return floor_host_atomic_fetch_sub((volatile _Atomic(float)*)p, 1.0f, floor_memory_order_seq_cst);
}
floor_inline_always int64_t atomic_dec(volatile int64_t* p) {
	return floor_host_atomic_fetch_sub((volatile _Atomic(int64_t)*)p, 1ll, floor_memory_order_seq_cst);
}
floor_inline_always uint64_t atomic_dec(volatile uint64_t* p) {
	return floor_host_atomic_fetch_sub((volatile _Atomic(uint64_t)*)p, 1ull, floor_memory_order_seq_cst);
}
floor_inline_always double atomic_dec(volatile double* p) {
	return floor_host_atomic_fetch_sub((volatile _Atomic(double)*)p, 1.0, floor_memory_order_seq_cst);
}

// xchg
floor_inline_always int32_t atomic_xchg(volatile int32_t* p, int32_t val) {
	return floor_host_atomic_exchange((volatile _Atomic(int32_t)*)p, val, floor_memory_order_seq_cst);
}
floor_inline_always uint32_t atomic_xchg(volatile uint32_t* p, uint32_t val) {
	return floor_host_atomic_exchange((volatile _Atomic(uint32_t)*)p, val, floor_memory_order_seq_cst);
}
floor_inline_always float atomic_xchg(volatile float* p, float val) {
	const uint32_t ret = floor_host_atomic_exchange((volatile _Atomic(uint32_t)*)p, *(uint32_t*)&val, floor_memory_order_seq_cst);
	return *(const float*)&ret;
}
floor_inline_always int64_t atomic_xchg(volatile int64_t* p, int64_t val) {
	return floor_host_atomic_exchange((volatile _Atomic(int64_t)*)p, val, floor_memory_order_seq_cst);
}
floor_inline_always uint64_t atomic_xchg(volatile uint64_t* p, uint64_t val) {
	return floor_host_atomic_exchange((volatile _Atomic(uint64_t)*)p, val, floor_memory_order_seq_cst);
}
floor_inline_always double atomic_xchg(volatile double* p, double val) {
	const uint64_t ret = floor_host_atomic_exchange((volatile _Atomic(uint64_t)*)p, *(uint64_t*)&val, floor_memory_order_seq_cst);
	return *(const double*)&ret;
}

// min
floor_inline_always int32_t atomic_min(volatile int32_t* p, int32_t val) {
	return floor_host_atomic_fetch_min((volatile _Atomic(int32_t)*)p, val, floor_memory_order_seq_cst);
}
floor_inline_always uint32_t atomic_min(volatile uint32_t* p, uint32_t val) {
	return floor_host_atomic_fetch_min((volatile _Atomic(uint32_t)*)p, val, floor_memory_order_seq_cst);
}
floor_inline_always float atomic_min(volatile float* p, float val) {
	FLOOR_ATOMIC_FALLBACK_FUNC_OP_32(min, , p, val)
}
floor_inline_always int64_t atomic_min(volatile int64_t* p, int64_t val) {
	return floor_host_atomic_fetch_min((volatile _Atomic(int64_t)*)p, val, floor_memory_order_seq_cst);
}
floor_inline_always uint64_t atomic_min(volatile uint64_t* p, uint64_t val) {
	return floor_host_atomic_fetch_min((volatile _Atomic(uint64_t)*)p, val, floor_memory_order_seq_cst);
}
floor_inline_always double atomic_min(volatile double* p, double val) {
	FLOOR_ATOMIC_FALLBACK_FUNC_OP_64(min, , p, val)
}

// max
floor_inline_always int32_t atomic_max(volatile int32_t* p, int32_t val) {
	return floor_host_atomic_fetch_max((volatile _Atomic(int32_t)*)p, val, floor_memory_order_seq_cst);
}
floor_inline_always uint32_t atomic_max(volatile uint32_t* p, uint32_t val) {
	return floor_host_atomic_fetch_max((volatile _Atomic(uint32_t)*)p, val, floor_memory_order_seq_cst);
}
floor_inline_always float atomic_max(volatile float* p, float val) {
	FLOOR_ATOMIC_FALLBACK_FUNC_OP_32(max, , p, val)
}
floor_inline_always int64_t atomic_max(volatile int64_t* p, int64_t val) {
	return floor_host_atomic_fetch_max((volatile _Atomic(int64_t)*)p, val, floor_memory_order_seq_cst);
}
floor_inline_always uint64_t atomic_max(volatile uint64_t* p, uint64_t val) {
	return floor_host_atomic_fetch_max((volatile _Atomic(uint64_t)*)p, val, floor_memory_order_seq_cst);
}
floor_inline_always double atomic_max(volatile double* p, double val) {
	FLOOR_ATOMIC_FALLBACK_FUNC_OP_64(max, , p, val)
}

// and
floor_inline_always int32_t atomic_and(volatile int32_t* p, int32_t val) {
	return floor_host_atomic_fetch_and((volatile _Atomic(int32_t)*)p, val, floor_memory_order_seq_cst);
}
floor_inline_always uint32_t atomic_and(volatile uint32_t* p, uint32_t val) {
	return floor_host_atomic_fetch_and((volatile _Atomic(uint32_t)*)p, val, floor_memory_order_seq_cst);
}
floor_inline_always int64_t atomic_and(volatile int64_t* p, int64_t val) {
	return floor_host_atomic_fetch_and((volatile _Atomic(int64_t)*)p, val, floor_memory_order_seq_cst);
}
floor_inline_always uint64_t atomic_and(volatile uint64_t* p, uint64_t val) {
	return floor_host_atomic_fetch_and((volatile _Atomic(uint64_t)*)p, val, floor_memory_order_seq_cst);
}

// or
floor_inline_always int32_t atomic_or(volatile int32_t* p, int32_t val) {
	return floor_host_atomic_fetch_or((volatile _Atomic(int32_t)*)p, val, floor_memory_order_seq_cst);
}
floor_inline_always uint32_t atomic_or(volatile uint32_t* p, uint32_t val) {
	return floor_host_atomic_fetch_or((volatile _Atomic(uint32_t)*)p, val, floor_memory_order_seq_cst);
}
floor_inline_always int64_t atomic_or(volatile int64_t* p, int64_t val) {
	return floor_host_atomic_fetch_or((volatile _Atomic(int64_t)*)p, val, floor_memory_order_seq_cst);
}
floor_inline_always uint64_t atomic_or(volatile uint64_t* p, uint64_t val) {
	return floor_host_atomic_fetch_or((volatile _Atomic(uint64_t)*)p, val, floor_memory_order_seq_cst);
}

// xor
floor_inline_always int32_t atomic_xor(volatile int32_t* p, int32_t val) {
	return floor_host_atomic_fetch_xor((volatile _Atomic(int32_t)*)p, val, floor_memory_order_seq_cst);
}
floor_inline_always uint32_t atomic_xor(volatile uint32_t* p, uint32_t val) {
	return floor_host_atomic_fetch_xor((volatile _Atomic(uint32_t)*)p, val, floor_memory_order_seq_cst);
}
floor_inline_always int64_t atomic_xor(volatile int64_t* p, int64_t val) {
	return floor_host_atomic_fetch_xor((volatile _Atomic(int64_t)*)p, val, floor_memory_order_seq_cst);
}
floor_inline_always uint64_t atomic_xor(volatile uint64_t* p, uint64_t val) {
	return floor_host_atomic_fetch_xor((volatile _Atomic(uint64_t)*)p, val, floor_memory_order_seq_cst);
}

// store
floor_inline_always void atomic_store(volatile int32_t* p, int32_t val) {
	floor_host_atomic_store((volatile _Atomic(int32_t)*)p, val, floor_memory_order_seq_cst);
}
floor_inline_always void atomic_store(volatile uint32_t* p, uint32_t val) {
	floor_host_atomic_store((volatile _Atomic(uint32_t)*)p, val, floor_memory_order_seq_cst);
}
floor_inline_always void atomic_store(volatile float* p, float val) {
	floor_host_atomic_store((volatile _Atomic(uint32_t)*)p, *(uint32_t*)&val, floor_memory_order_seq_cst);
}
floor_inline_always void atomic_store(volatile int64_t* p, int64_t val) {
	floor_host_atomic_store((volatile _Atomic(int64_t)*)p, val, floor_memory_order_seq_cst);
}
floor_inline_always void atomic_store(volatile uint64_t* p, uint64_t val) {
	floor_host_atomic_store((volatile _Atomic(uint64_t)*)p, val, floor_memory_order_seq_cst);
}
floor_inline_always void atomic_store(volatile double* p, double val) {
	floor_host_atomic_store((volatile _Atomic(uint64_t)*)p, *(uint64_t*)&val, floor_memory_order_seq_cst);
}

// load
FLOOR_PUSH_WARNINGS()
FLOOR_IGNORE_WARNING(cast-qual) // ignored const -> non-const casts here (no other way of doing this)
floor_inline_always int32_t atomic_load(const volatile int32_t* p) {
	return floor_host_atomic_load((volatile _Atomic(int32_t)*)p, floor_memory_order_seq_cst);
}
floor_inline_always uint32_t atomic_load(const volatile uint32_t* p) {
	return floor_host_atomic_load((volatile _Atomic(uint32_t)*)p, floor_memory_order_seq_cst);
}
floor_inline_always float atomic_load(const volatile float* p) {
	const uint32_t ret = floor_host_atomic_load((volatile _Atomic(uint32_t)*)p, floor_memory_order_seq_cst);
	return *(const float*)&ret;
}
floor_inline_always int64_t atomic_load(const volatile int64_t* p) {
	return floor_host_atomic_load((volatile _Atomic(int64_t)*)p, floor_memory_order_seq_cst);
}
floor_inline_always uint64_t atomic_load(const volatile uint64_t* p) {
	return floor_host_atomic_load((volatile _Atomic(uint64_t)*)p, floor_memory_order_seq_cst);
}
floor_inline_always double atomic_load(const volatile double* p) {
	const uint64_t ret = floor_host_atomic_load((volatile _Atomic(uint64_t)*)p, floor_memory_order_seq_cst);
	return *(const double*)&ret;
}
FLOOR_POP_WARNINGS()

#endif
