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
int32_t atomic_add(volatile int32_t* addr, const int32_t& val) {
	int32_t ret;
	asm volatile("atom.add.s32 %0, [%1], %2;" : "=r"(ret) : "l"(addr), "r"(val));
	return ret;
}
uint32_t atomic_add(volatile uint32_t* addr, const uint32_t& val) {
	uint32_t ret;
	asm volatile("atom.add.u32 %0, [%1], %2;" : "=r"(ret) : "l"(addr), "r"(val));
	return ret;
}
float atomic_add(volatile float* addr, const float& val) {
	float ret;
	asm volatile("atom.add.f32 %0, [%1], %2;" : "=f"(ret) : "l"(addr), "f"(val));
	return ret;
}
uint64_t atomic_add(volatile uint64_t* addr, const uint64_t& val) {
	uint64_t ret;
	asm volatile("atom.add.u64 %0, [%1], %2;" : "=l"(ret) : "l"(addr), "l"(val));
	return ret;
}

// sub
int32_t atomic_sub(volatile int32_t* addr, const int32_t& val) {
	int32_t ret;
	asm volatile("atom.add.s32 %0, [%1], %2;" : "=r"(ret) : "l"(addr), "r"(-val));
	return ret;
}
uint32_t atomic_sub(volatile uint32_t* addr, const uint32_t& val) {
	uint32_t ret;
	asm volatile("atom.add.u32 %0, [%1], %2;" : "=r"(ret) : "l"(addr), "r"(-int32_t(val)));
	return ret;
}
float atomic_sub(volatile float* addr, const float& val) {
	float ret;
	asm volatile("atom.add.f32 %0, [%1], %2;" : "=f"(ret) : "l"(addr), "f"(-val));
	return ret;
}
uint64_t atomic_sub(volatile uint64_t* addr, const uint64_t& val) {
	uint64_t ret;
	asm volatile("atom.add.u64 %0, [%1], %2;" : "=l"(ret) : "l"(addr), "l"(-int64_t(val)));
	return ret;
}

// inc
int32_t atomic_inc(volatile int32_t* addr) {
	return atomic_add(addr, 1);
}
uint32_t atomic_inc(volatile uint32_t* addr) {
	return atomic_add(addr, 1u);
}
float atomic_inc(volatile float* addr) {
	return atomic_add(addr, 1.0f);
}
uint64_t atomic_inc(volatile uint64_t* addr) {
	return atomic_add(addr, 1ull);
}

// dec
int32_t atomic_dec(volatile int32_t* addr) {
	return atomic_sub(addr, 1);
}
uint32_t atomic_dec(volatile uint32_t* addr) {
	return atomic_sub(addr, 1u);
}
float atomic_dec(volatile float* addr) {
	return atomic_sub(addr, 1.0f);
}
uint64_t atomic_dec(volatile uint64_t* addr) {
	return atomic_sub(addr, 1ull);
}

// xchg
int32_t atomic_xchg(volatile int32_t* addr, const int32_t& val) {
	int32_t ret;
	asm volatile("atom.exch.s32 %0, [%1], %2;" : "=r"(ret) : "l"(addr), "r"(val));
	return ret;
}
uint32_t atomic_xchg(volatile uint32_t* addr, const uint32_t& val) {
	uint32_t ret;
	asm volatile("atom.exch.u32 %0, [%1], %2;" : "=r"(ret) : "l"(addr), "r"(val));
	return ret;
}
float atomic_xchg(volatile float* addr, const float& val) {
	float ret;
	asm volatile("atom.exch.f32 %0, [%1], %2;" : "=f"(ret) : "l"(addr), "f"(val));
	return ret;
}
uint64_t atomic_xchg(volatile uint64_t* addr, const uint64_t& val) {
	uint64_t ret;
	asm volatile("atom.exch.u64 %0, [%1], %2;" : "=l"(ret) : "l"(addr), "l"(val));
	return ret;
}

// cmpxchg
int32_t atomic_cmpxchg(volatile int32_t* addr, const int32_t& cmp, const int32_t& val) {
	int32_t ret;
	asm volatile("atom.cas.s32 %0, [%1], %2, %3;" : "=r"(ret) : "l"(addr), "r"(cmp), "r"(val));
	return ret;
}
uint32_t atomic_cmpxchg(volatile uint32_t* addr, const uint32_t& cmp, const uint32_t& val) {
	uint32_t ret;
	asm volatile("atom.cas.u32 %0, [%1], %2, %3;" : "=r"(ret) : "l"(addr), "r"(cmp), "r"(val));
	return ret;
}
float atomic_cmpxchg(volatile float* addr, const float& cmp, const float& val) {
	float ret;
	asm volatile("atom.cas.f32 %0, [%1], %2, %3;" : "=f"(ret) : "l"(addr), "f"(cmp), "f"(val));
	return ret;
}
uint64_t atomic_cmpxchg(volatile uint64_t* addr, const uint64_t& cmp, const uint64_t& val) {
	uint64_t ret;
	asm volatile("atom.cas.u64 %0, [%1], %2, %3;" : "=l"(ret) : "l"(addr), "l"(cmp), "l"(val));
	return ret;
}

// min
int32_t atomic_min(volatile int32_t* addr, const int32_t& val) {
	int32_t ret;
	asm volatile("atom.min.s32 %0, [%1], %2;" : "=r"(ret) : "l"(addr), "r"(val));
	return ret;
}
uint32_t atomic_min(volatile uint32_t* addr, const uint32_t& val) {
	uint32_t ret;
	asm volatile("atom.min.u32 %0, [%1], %2;" : "=r"(ret) : "l"(addr), "r"(val));
	return ret;
}
// NOTE: requires sm_32 (TODO: check)
uint64_t atomic_min(volatile uint64_t* addr, const uint64_t& val) {
	uint64_t ret;
	asm volatile("atom.min.u64 %0, [%1], %2;" : "=l"(ret) : "l"(addr), "l"(val));
	return ret;
}
// NOTE: requires sm_32 (TODO: check)
int64_t atomic_min(volatile int64_t* addr, const int64_t& val) {
	int64_t ret;
	asm volatile("atom.min.u64 %0, [%1], %2;" : "=l"(ret) : "l"(addr), "l"(val));
	return ret;
}

// max
int32_t atomic_max(volatile int32_t* addr, const int32_t& val) {
	int32_t ret;
	asm volatile("atom.max.s32 %0, [%1], %2;" : "=r"(ret) : "l"(addr), "r"(val));
	return ret;
}
uint32_t atomic_max(volatile uint32_t* addr, const uint32_t& val) {
	uint32_t ret;
	asm volatile("atom.max.u32 %0, [%1], %2;" : "=r"(ret) : "l"(addr), "r"(val));
	return ret;
}
// NOTE: requires sm_32 (TODO: check)
uint64_t atomic_max(volatile uint64_t* addr, const uint64_t& val) {
	uint64_t ret;
	asm volatile("atom.max.u64 %0, [%1], %2;" : "=l"(ret) : "l"(addr), "l"(val));
	return ret;
}
// NOTE: requires sm_32 (TODO: check)
int64_t atomic_max(volatile int64_t* addr, const int64_t& val) {
	int64_t ret;
	asm volatile("atom.max.u64 %0, [%1], %2;" : "=l"(ret) : "l"(addr), "l"(val));
	return ret;
}

// float min/max (not natively supported)
float atomic_min(volatile float* addr, const float& val) {
	if(val < 0.0f) {
		atomic_max((volatile uint32_t*)addr, *(uint32_t*)&val);
	}
	return atomic_min((volatile int32_t*)addr, *(int32_t*)&val);
}
float atomic_max(volatile float* addr, const float& val) {
	if(val < 0.0f) {
		atomic_min((volatile uint32_t*)addr, *(uint32_t*)&val);
	}
	return atomic_max((volatile int32_t*)addr, *(int32_t*)&val);
}

// and
int32_t atomic_and(volatile int32_t* addr, const int32_t& val) {
	int32_t ret;
	asm volatile("atom.and.s32 %0, [%1], %2;" : "=r"(ret) : "l"(addr), "r"(val));
	return ret;
}
uint32_t atomic_and(volatile uint32_t* addr, const uint32_t& val) {
	uint32_t ret;
	asm volatile("atom.and.u32 %0, [%1], %2;" : "=r"(ret) : "l"(addr), "r"(val));
	return ret;
}
// NOTE: requires sm_32 (TODO: check)
uint64_t atomic_and(volatile uint64_t* addr, const uint64_t& val) {
	uint64_t ret;
	asm volatile("atom.and.u64 %0, [%1], %2;" : "=l"(ret) : "l"(addr), "l"(val));
	return ret;
}

// or
int32_t atomic_or(volatile int32_t* addr, const int32_t& val) {
	int32_t ret;
	asm volatile("atom.or.s32 %0, [%1], %2;" : "=r"(ret) : "l"(addr), "r"(val));
	return ret;
}
uint32_t atomic_or(volatile uint32_t* addr, const uint32_t& val) {
	uint32_t ret;
	asm volatile("atom.or.u32 %0, [%1], %2;" : "=r"(ret) : "l"(addr), "r"(val));
	return ret;
}
// NOTE: requires sm_32 (TODO: check)
uint64_t atomic_or(volatile uint64_t* addr, const uint64_t& val) {
	uint64_t ret;
	asm volatile("atom.or.u64 %0, [%1], %2;" : "=l"(ret) : "l"(addr), "l"(val));
	return ret;
}

// xor
int32_t atomic_xor(volatile int32_t* addr, const int32_t& val) {
	int32_t ret;
	asm volatile("atom.xor.s32 %0, [%1], %2;" : "=r"(ret) : "l"(addr), "r"(val));
	return ret;
}
uint32_t atomic_xor(volatile uint32_t* addr, const uint32_t& val) {
	uint32_t ret;
	asm volatile("atom.xor.u32 %0, [%1], %2;" : "=r"(ret) : "l"(addr), "r"(val));
	return ret;
}
// NOTE: requires sm_32 (TODO: check)
uint64_t atomic_xor(volatile uint64_t* addr, const uint64_t& val) {
	uint64_t ret;
	asm volatile("atom.xor.u64 %0, [%1], %2;" : "=l"(ret) : "l"(addr), "l"(val));
	return ret;
}

#endif

#endif
