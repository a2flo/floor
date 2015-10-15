/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2015 Florian Ziesche
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

#ifndef __FLOOR_COMPUTE_DEVICE_CUDA_ATOMIC_HPP__
#define __FLOOR_COMPUTE_DEVICE_CUDA_ATOMIC_HPP__

#if defined(FLOOR_COMPUTE_CUDA)

// add
floor_inline_always int32_t atomic_add(volatile int32_t* addr, const int32_t& val) {
	int32_t ret;
	asm volatile("atom.add.s32 %0, [%1], %2;" : "=r"(ret) : "l"(addr), "r"(val));
	return ret;
}
floor_inline_always uint32_t atomic_add(volatile uint32_t* addr, const uint32_t& val) {
	uint32_t ret;
	asm volatile("atom.add.u32 %0, [%1], %2;" : "=r"(ret) : "l"(addr), "r"(val));
	return ret;
}
floor_inline_always float atomic_add(volatile float* addr, const float& val) {
	float ret;
	asm volatile("atom.add.f32 %0, [%1], %2;" : "=f"(ret) : "l"(addr), "f"(val));
	return ret;
}
floor_inline_always int64_t atomic_add(volatile int64_t* addr, const int64_t& val) {
	int64_t ret;
	asm volatile("atom.add.u64 %0, [%1], %2;" : "=l"(ret) : "l"(addr), "l"(val));
	return ret;
}
floor_inline_always uint64_t atomic_add(volatile uint64_t* addr, const uint64_t& val) {
	uint64_t ret;
	asm volatile("atom.add.u64 %0, [%1], %2;" : "=l"(ret) : "l"(addr), "l"(val));
	return ret;
}

// sub
floor_inline_always int32_t atomic_sub(volatile int32_t* addr, const int32_t& val) {
	int32_t ret;
	asm volatile("atom.add.s32 %0, [%1], %2;" : "=r"(ret) : "l"(addr), "r"(-val));
	return ret;
}
floor_inline_always uint32_t atomic_sub(volatile uint32_t* addr, const uint32_t& val) {
	uint32_t ret;
	asm volatile("atom.add.u32 %0, [%1], %2;" : "=r"(ret) : "l"(addr), "r"(-int32_t(val)));
	return ret;
}
floor_inline_always float atomic_sub(volatile float* addr, const float& val) {
	float ret;
	asm volatile("atom.add.f32 %0, [%1], %2;" : "=f"(ret) : "l"(addr), "f"(-val));
	return ret;
}
floor_inline_always int64_t atomic_sub(volatile int64_t* addr, const int64_t& val) {
	int64_t ret;
	asm volatile("atom.add.u64 %0, [%1], %2;" : "=l"(ret) : "l"(addr), "l"(-val));
	return ret;
}
floor_inline_always uint64_t atomic_sub(volatile uint64_t* addr, const uint64_t& val) {
	uint64_t ret;
	asm volatile("atom.add.u64 %0, [%1], %2;" : "=l"(ret) : "l"(addr), "l"(-int64_t(val)));
	return ret;
}

// inc
floor_inline_always int32_t atomic_inc(volatile int32_t* addr) {
	return atomic_add(addr, 1);
}
floor_inline_always uint32_t atomic_inc(volatile uint32_t* addr) {
	return atomic_add(addr, 1u);
}
floor_inline_always float atomic_inc(volatile float* addr) {
	return atomic_add(addr, 1.0f);
}
floor_inline_always int64_t atomic_inc(volatile int64_t* addr) {
	return atomic_add(addr, 1ll);
}
floor_inline_always uint64_t atomic_inc(volatile uint64_t* addr) {
	return atomic_add(addr, 1ull);
}

// dec
floor_inline_always int32_t atomic_dec(volatile int32_t* addr) {
	return atomic_sub(addr, 1);
}
floor_inline_always uint32_t atomic_dec(volatile uint32_t* addr) {
	return atomic_sub(addr, 1u);
}
floor_inline_always float atomic_dec(volatile float* addr) {
	return atomic_sub(addr, 1.0f);
}
floor_inline_always int64_t atomic_dec(volatile int64_t* addr) {
	return atomic_sub(addr, 1ll);
}
floor_inline_always uint64_t atomic_dec(volatile uint64_t* addr) {
	return atomic_sub(addr, 1ull);
}

// xchg
floor_inline_always int32_t atomic_xchg(volatile int32_t* addr, const int32_t& val) {
	int32_t ret;
	asm volatile("atom.exch.b32 %0, [%1], %2;" : "=r"(ret) : "l"(addr), "r"(val));
	return ret;
}
floor_inline_always uint32_t atomic_xchg(volatile uint32_t* addr, const uint32_t& val) {
	uint32_t ret;
	asm volatile("atom.exch.b32 %0, [%1], %2;" : "=r"(ret) : "l"(addr), "r"(val));
	return ret;
}
floor_inline_always float atomic_xchg(volatile float* addr, const float& val) {
	float ret;
	asm volatile("atom.exch.b32 %0, [%1], %2;" : "=f"(ret) : "l"(addr), "f"(val));
	return ret;
}
floor_inline_always int64_t atomic_xchg(volatile int64_t* addr, const int64_t& val) {
	int64_t ret;
	asm volatile("atom.exch.b64 %0, [%1], %2;" : "=l"(ret) : "l"(addr), "l"(val));
	return ret;
}
floor_inline_always uint64_t atomic_xchg(volatile uint64_t* addr, const uint64_t& val) {
	uint64_t ret;
	asm volatile("atom.exch.b64 %0, [%1], %2;" : "=l"(ret) : "l"(addr), "l"(val));
	return ret;
}

// cmpxchg
floor_inline_always int32_t atomic_cmpxchg(volatile int32_t* addr, const int32_t& cmp, const int32_t& val) {
	int32_t ret;
	asm volatile("atom.cas.b32 %0, [%1], %2, %3;" : "=r"(ret) : "l"(addr), "r"(cmp), "r"(val));
	return ret;
}
floor_inline_always uint32_t atomic_cmpxchg(volatile uint32_t* addr, const uint32_t& cmp, const uint32_t& val) {
	uint32_t ret;
	asm volatile("atom.cas.b32 %0, [%1], %2, %3;" : "=r"(ret) : "l"(addr), "r"(cmp), "r"(val));
	return ret;
}
floor_inline_always float atomic_cmpxchg(volatile float* addr, const float& cmp, const float& val) {
	float ret;
	asm volatile("atom.cas.b32 %0, [%1], %2, %3;" : "=f"(ret) : "l"(addr), "f"(cmp), "f"(val));
	return ret;
}
floor_inline_always int64_t atomic_cmpxchg(volatile int64_t* addr, const int64_t& cmp, const int64_t& val) {
	int64_t ret;
	asm volatile("atom.cas.b64 %0, [%1], %2, %3;" : "=l"(ret) : "l"(addr), "l"(cmp), "l"(val));
	return ret;
}
floor_inline_always uint64_t atomic_cmpxchg(volatile uint64_t* addr, const uint64_t& cmp, const uint64_t& val) {
	uint64_t ret;
	asm volatile("atom.cas.b64 %0, [%1], %2, %3;" : "=l"(ret) : "l"(addr), "l"(cmp), "l"(val));
	return ret;
}

// min
floor_inline_always int32_t atomic_min(volatile int32_t* addr, const int32_t& val) {
	int32_t ret;
	asm volatile("atom.min.s32 %0, [%1], %2;" : "=r"(ret) : "l"(addr), "r"(val));
	return ret;
}
floor_inline_always uint32_t atomic_min(volatile uint32_t* addr, const uint32_t& val) {
	uint32_t ret;
	asm volatile("atom.min.u32 %0, [%1], %2;" : "=r"(ret) : "l"(addr), "r"(val));
	return ret;
}
#if defined(FLOOR_COMPUTE_INFO_HAS_NATIVE_EXTENDED_64_BIT_ATOMICS_1)
floor_inline_always int64_t atomic_min(volatile int64_t* addr, const int64_t& val) {
	int64_t ret;
	asm volatile("atom.min.s64 %0, [%1], %2;" : "=l"(ret) : "l"(addr), "l"(val));
	return ret;
}
floor_inline_always uint64_t atomic_min(volatile uint64_t* addr, const uint64_t& val) {
	uint64_t ret;
	asm volatile("atom.min.u64 %0, [%1], %2;" : "=l"(ret) : "l"(addr), "l"(val));
	return ret;
}
#else
floor_inline_always int64_t atomic_min(volatile int64_t* addr, const int64_t& val) {
	FLOOR_ATOMIC_FALLBACK_FUNC_OP_64(min, , addr, val)
}
floor_inline_always uint64_t atomic_min(volatile uint64_t* addr, const uint64_t& val) {
	FLOOR_ATOMIC_FALLBACK_FUNC_OP_64(min, , addr, val)
}
#endif

// max
floor_inline_always int32_t atomic_max(volatile int32_t* addr, const int32_t& val) {
	int32_t ret;
	asm volatile("atom.max.s32 %0, [%1], %2;" : "=r"(ret) : "l"(addr), "r"(val));
	return ret;
}
floor_inline_always uint32_t atomic_max(volatile uint32_t* addr, const uint32_t& val) {
	uint32_t ret;
	asm volatile("atom.max.u32 %0, [%1], %2;" : "=r"(ret) : "l"(addr), "r"(val));
	return ret;
}
#if defined(FLOOR_COMPUTE_INFO_HAS_NATIVE_EXTENDED_64_BIT_ATOMICS_1)
floor_inline_always int64_t atomic_max(volatile int64_t* addr, const int64_t& val) {
	int64_t ret;
	asm volatile("atom.max.s64 %0, [%1], %2;" : "=l"(ret) : "l"(addr), "l"(val));
	return ret;
}
floor_inline_always uint64_t atomic_max(volatile uint64_t* addr, const uint64_t& val) {
	uint64_t ret;
	asm volatile("atom.max.u64 %0, [%1], %2;" : "=l"(ret) : "l"(addr), "l"(val));
	return ret;
}
#else
floor_inline_always int64_t atomic_max(volatile int64_t* addr, const int64_t& val) {
	FLOOR_ATOMIC_FALLBACK_FUNC_OP_64(max, , addr, val)
}
floor_inline_always uint64_t atomic_max(volatile uint64_t* addr, const uint64_t& val) {
	FLOOR_ATOMIC_FALLBACK_FUNC_OP_64(max, , addr, val)
}
#endif

// NOTE: not natively supported, but can be efficiently emulated through 32-bit min/max
floor_inline_always float atomic_min(volatile float* addr, const float& val) {
	if(val < 0.0f) {
		atomic_max((volatile uint32_t*)addr, *(uint32_t*)&val);
	}
	return atomic_min((volatile int32_t*)addr, *(int32_t*)&val);
}
floor_inline_always float atomic_max(volatile float* addr, const float& val) {
	if(val < 0.0f) {
		atomic_min((volatile uint32_t*)addr, *(uint32_t*)&val);
	}
	return atomic_max((volatile int32_t*)addr, *(int32_t*)&val);
}

// and
floor_inline_always int32_t atomic_and(volatile int32_t* addr, const int32_t& val) {
	int32_t ret;
	asm volatile("atom.and.b32 %0, [%1], %2;" : "=r"(ret) : "l"(addr), "r"(val));
	return ret;
}
floor_inline_always uint32_t atomic_and(volatile uint32_t* addr, const uint32_t& val) {
	uint32_t ret;
	asm volatile("atom.and.b32 %0, [%1], %2;" : "=r"(ret) : "l"(addr), "r"(val));
	return ret;
}
#if defined(FLOOR_COMPUTE_INFO_HAS_NATIVE_EXTENDED_64_BIT_ATOMICS_1)
floor_inline_always int64_t atomic_and(volatile int64_t* addr, const int64_t& val) {
	int64_t ret;
	asm volatile("atom.and.b64 %0, [%1], %2;" : "=l"(ret) : "l"(addr), "l"(val));
	return ret;
}
floor_inline_always uint64_t atomic_and(volatile uint64_t* addr, const uint64_t& val) {
	uint64_t ret;
	asm volatile("atom.and.b64 %0, [%1], %2;" : "=l"(ret) : "l"(addr), "l"(val));
	return ret;
}
#else
floor_inline_always int64_t atomic_and(volatile int64_t* addr, const int64_t& val) {
	FLOOR_ATOMIC_FALLBACK_OP_64(&, , addr, val)
}
floor_inline_always uint64_t atomic_and(volatile uint64_t* addr, const uint64_t& val) {
	FLOOR_ATOMIC_FALLBACK_OP_64(&, , addr, val)
}
#endif

// or
floor_inline_always int32_t atomic_or(volatile int32_t* addr, const int32_t& val) {
	int32_t ret;
	asm volatile("atom.or.b32 %0, [%1], %2;" : "=r"(ret) : "l"(addr), "r"(val));
	return ret;
}
floor_inline_always uint32_t atomic_or(volatile uint32_t* addr, const uint32_t& val) {
	uint32_t ret;
	asm volatile("atom.or.b32 %0, [%1], %2;" : "=r"(ret) : "l"(addr), "r"(val));
	return ret;
}
#if defined(FLOOR_COMPUTE_INFO_HAS_NATIVE_EXTENDED_64_BIT_ATOMICS_1)
floor_inline_always int64_t atomic_or(volatile int64_t* addr, const int64_t& val) {
	int64_t ret;
	asm volatile("atom.or.b64 %0, [%1], %2;" : "=l"(ret) : "l"(addr), "l"(val));
	return ret;
}
floor_inline_always uint64_t atomic_or(volatile uint64_t* addr, const uint64_t& val) {
	uint64_t ret;
	asm volatile("atom.or.b64 %0, [%1], %2;" : "=l"(ret) : "l"(addr), "l"(val));
	return ret;
}
#else
floor_inline_always int64_t atomic_or(volatile int64_t* addr, const int64_t& val) {
	FLOOR_ATOMIC_FALLBACK_OP_64(|, , addr, val)
}
floor_inline_always uint64_t atomic_or(volatile uint64_t* addr, const uint64_t& val) {
	FLOOR_ATOMIC_FALLBACK_OP_64(|, , addr, val)
}
#endif

// xor
floor_inline_always int32_t atomic_xor(volatile int32_t* addr, const int32_t& val) {
	int32_t ret;
	asm volatile("atom.xor.b32 %0, [%1], %2;" : "=r"(ret) : "l"(addr), "r"(val));
	return ret;
}
floor_inline_always uint32_t atomic_xor(volatile uint32_t* addr, const uint32_t& val) {
	uint32_t ret;
	asm volatile("atom.xor.b32 %0, [%1], %2;" : "=r"(ret) : "l"(addr), "r"(val));
	return ret;
}
#if defined(FLOOR_COMPUTE_INFO_HAS_NATIVE_EXTENDED_64_BIT_ATOMICS_1)
floor_inline_always int64_t atomic_xor(volatile int64_t* addr, const int64_t& val) {
	int64_t ret;
	asm volatile("atom.xor.b64 %0, [%1], %2;" : "=l"(ret) : "l"(addr), "l"(val));
	return ret;
}
floor_inline_always uint64_t atomic_xor(volatile uint64_t* addr, const uint64_t& val) {
	uint64_t ret;
	asm volatile("atom.xor.b64 %0, [%1], %2;" : "=l"(ret) : "l"(addr), "l"(val));
	return ret;
}
#else
floor_inline_always int64_t atomic_xor(volatile int64_t* addr, const int64_t& val) {
	FLOOR_ATOMIC_FALLBACK_OP_64(^, , addr, val)
}
floor_inline_always uint64_t atomic_xor(volatile uint64_t* addr, const uint64_t& val) {
	FLOOR_ATOMIC_FALLBACK_OP_64(^, , addr, val)
}
#endif

// store (simple alias of xchg)
floor_inline_always void atomic_store(volatile int32_t* addr, const int32_t& val) {
	atomic_xchg(addr, val);
}
floor_inline_always void atomic_store(volatile uint32_t* addr, const uint32_t& val) {
	atomic_xchg(addr, val);
}
floor_inline_always void atomic_store(volatile float* addr, const float& val) {
	atomic_xchg(addr, val);
}
floor_inline_always void atomic_store(volatile int64_t* addr, const int64_t& val) {
	atomic_xchg(addr, val);
}
floor_inline_always void atomic_store(volatile uint64_t* addr, const uint64_t& val) {
	atomic_xchg(addr, val);
}

// load (no proper instruction for this, so just perform a "+ 0")
floor_inline_always int32_t atomic_load(const volatile int32_t* addr) {
	return atomic_add((volatile int32_t*)addr, 0);
}
floor_inline_always uint32_t atomic_load(const volatile uint32_t* addr) {
	return atomic_add((volatile uint32_t*)addr, 0u);
}
floor_inline_always float atomic_load(const volatile float* addr) {
	return atomic_add((volatile float*)addr, 0.0f);
}
floor_inline_always int64_t atomic_load(const volatile int64_t* addr) {
	return atomic_add((volatile int64_t*)addr, 0ll);
}
floor_inline_always uint64_t atomic_load(const volatile uint64_t* addr) {
	return atomic_add((volatile uint64_t*)addr, 0ull);
}

#endif

#endif
