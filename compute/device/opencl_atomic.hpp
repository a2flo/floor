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

#ifndef __FLOOR_COMPUTE_DEVICE_OPENCL_ATOMIC_HPP__
#define __FLOOR_COMPUTE_DEVICE_OPENCL_ATOMIC_HPP__

#if defined(FLOOR_COMPUTE_OPENCL)

// add
int32_t atomic_add(volatile global int32_t* p, int32_t val);
uint32_t atomic_add(volatile global uint32_t* p, uint32_t val);
int32_t atomic_add(volatile local int32_t* p, int32_t val);
uint32_t atomic_add(volatile local uint32_t* p, uint32_t val);
#if defined(FLOOR_COMPUTE_INFO_HAS_64_BIT_ATOMICS_1)
uint64_t atom_add(volatile global uint64_t* p, uint64_t val);
int64_t atom_add(volatile global int64_t* p, int64_t val);
uint64_t atom_add(volatile local uint64_t* p, uint64_t val);
int64_t atom_add(volatile local int64_t* p, int64_t val);
// cl_khr_int64_base_atomics only requires atom_* functions to exist
uint64_t atomic_add(volatile global uint64_t* p, uint64_t val) { return atom_add(p, val); }
int64_t atomic_add(volatile global int64_t* p, int64_t val) { return atom_add(p, val); }
uint64_t atomic_add(volatile local uint64_t* p, uint64_t val) { return atom_add(p, val); }
int64_t atomic_add(volatile local int64_t* p, int64_t val) { return atom_add(p, val); }
#endif

// sub
int32_t atomic_sub(volatile global int32_t* p, int32_t val);
uint32_t atomic_sub(volatile global uint32_t* p, uint32_t val);
int32_t atomic_sub(volatile local int32_t* p, int32_t val);
uint32_t atomic_sub(volatile local uint32_t* p, uint32_t val);
#if defined(FLOOR_COMPUTE_INFO_HAS_64_BIT_ATOMICS_1)
uint64_t atom_sub(volatile global uint64_t* p, uint64_t val);
int64_t atom_sub(volatile global int64_t* p, int64_t val);
uint64_t atom_sub(volatile local uint64_t* p, uint64_t val);
int64_t atom_sub(volatile local int64_t* p, int64_t val);
uint64_t atomic_sub(volatile global uint64_t* p, uint64_t val) { return atom_sub(p, val); }
int64_t atomic_sub(volatile global int64_t* p, int64_t val) { return atom_sub(p, val); }
uint64_t atomic_sub(volatile local uint64_t* p, uint64_t val) { return atom_sub(p, val); }
int64_t atomic_sub(volatile local int64_t* p, int64_t val) { return atom_sub(p, val); }
#endif

// inc
int32_t atomic_inc(volatile global int32_t* p);
uint32_t atomic_inc(volatile global uint32_t* p);
int32_t atomic_inc(volatile local int32_t* p);
uint32_t atomic_inc(volatile local uint32_t* p);
#if defined(FLOOR_COMPUTE_INFO_HAS_64_BIT_ATOMICS_1)
uint64_t atom_inc(volatile global uint64_t* p);
int64_t atom_inc(volatile global int64_t* p);
uint64_t atom_inc(volatile local uint64_t* p);
int64_t atom_inc(volatile local int64_t* p);
uint64_t atomic_inc(volatile global uint64_t* p) { return atom_inc(p); }
int64_t atomic_inc(volatile global int64_t* p) { return atom_inc(p); }
uint64_t atomic_inc(volatile local uint64_t* p) { return atom_inc(p); }
int64_t atomic_inc(volatile local int64_t* p) { return atom_inc(p); }
#endif

// dec
int32_t atomic_dec(volatile global int32_t* p);
uint32_t atomic_dec(volatile global uint32_t* p);
int32_t atomic_dec(volatile local int32_t* p);
uint32_t atomic_dec(volatile local uint32_t* p);
#if defined(FLOOR_COMPUTE_INFO_HAS_64_BIT_ATOMICS_1)
uint64_t atom_dec(volatile global uint64_t* p);
int64_t atom_dec(volatile global int64_t* p);
uint64_t atom_dec(volatile local uint64_t* p);
int64_t atom_dec(volatile local int64_t* p);
uint64_t atomic_dec(volatile global uint64_t* p) { return atom_dec(p); }
int64_t atomic_dec(volatile global int64_t* p) { return atom_dec(p); }
uint64_t atomic_dec(volatile local uint64_t* p) { return atom_dec(p); }
int64_t atomic_dec(volatile local int64_t* p) { return atom_dec(p); }
#endif

// xchg
int32_t atomic_xchg(volatile global int32_t* p, int32_t val);
uint32_t atomic_xchg(volatile global uint32_t* p, uint32_t val);
float atomic_xchg(volatile global float* p, float val);
int32_t atomic_xchg(volatile local int32_t* p, int32_t val);
uint32_t atomic_xchg(volatile local uint32_t* p, uint32_t val);
float atomic_xchg(volatile local float* p, float val);
#if defined(FLOOR_COMPUTE_INFO_HAS_64_BIT_ATOMICS_1)
uint64_t atom_xchg(volatile global uint64_t* p, uint64_t val);
int64_t atom_xchg(volatile global int64_t* p, int64_t val);
uint64_t atom_xchg(volatile local uint64_t* p, uint64_t val);
int64_t atom_xchg(volatile local int64_t* p, int64_t val);
uint64_t atomic_xchg(volatile global uint64_t* p, uint64_t val) { return atom_xchg(p, val); }
int64_t atomic_xchg(volatile global int64_t* p, int64_t val) { return atom_xchg(p, val); }
uint64_t atomic_xchg(volatile local uint64_t* p, uint64_t val) { return atom_xchg(p, val); }
int64_t atomic_xchg(volatile local int64_t* p, int64_t val) { return atom_xchg(p, val); }
#endif

// cmpxchg
int32_t atomic_cmpxchg(volatile global int32_t* p, int32_t cmp, int32_t val);
uint32_t atomic_cmpxchg(volatile global uint32_t* p, uint32_t cmp, uint32_t val);
int32_t atomic_cmpxchg(volatile local int32_t* p, int32_t cmp, int32_t val);
uint32_t atomic_cmpxchg(volatile local uint32_t* p, uint32_t cmp, uint32_t val);
#if defined(FLOOR_COMPUTE_INFO_HAS_64_BIT_ATOMICS_1)
uint64_t atom_cmpxchg(volatile global uint64_t* p, uint64_t cmp, uint64_t val);
int64_t atom_cmpxchg(volatile global int64_t* p, int64_t cmp, int64_t val);
uint64_t atom_cmpxchg(volatile local uint64_t* p, uint64_t cmp, uint64_t val);
int64_t atom_cmpxchg(volatile local int64_t* p, int64_t cmp, int64_t val);
uint64_t atomic_cmpxchg(volatile global uint64_t* p, uint64_t cmp, uint64_t val) { return atom_cmpxchg(p, cmp, val); }
int64_t atomic_cmpxchg(volatile global int64_t* p, int64_t cmp, int64_t val) { return atom_cmpxchg(p, cmp, val); }
uint64_t atomic_cmpxchg(volatile local uint64_t* p, uint64_t cmp, uint64_t val) { return atom_cmpxchg(p, cmp, val); }
int64_t atomic_cmpxchg(volatile local int64_t* p, int64_t cmp, int64_t val) { return atom_cmpxchg(p, cmp, val); }
#endif

// min
int32_t atomic_min(volatile global int32_t* p, int32_t val);
uint32_t atomic_min(volatile global uint32_t* p, uint32_t val);
int32_t atomic_min(volatile local int32_t* p, int32_t val);
uint32_t atomic_min(volatile local uint32_t* p, uint32_t val);
#if defined(FLOOR_COMPUTE_INFO_HAS_NATIVE_EXTENDED_64_BIT_ATOMICS_1)
uint64_t atom_min(volatile global uint64_t* p, uint64_t val);
int64_t atom_min(volatile global int64_t* p, int64_t val);
uint64_t atom_min(volatile local uint64_t* p, uint64_t val);
int64_t atom_min(volatile local int64_t* p, int64_t val);
// cl_khr_int64_extended_atomics only requires atom_* functions to exist
uint64_t atomic_min(volatile global uint64_t* p, uint64_t val) { return atom_min(p, val); }
int64_t atomic_min(volatile global int64_t* p, int64_t val) { return atom_min(p, val); }
uint64_t atomic_min(volatile local uint64_t* p, uint64_t val) { return atom_min(p, val); }
int64_t atomic_min(volatile local int64_t* p, int64_t val) { return atom_min(p, val); }
#elif defined(FLOOR_COMPUTE_INFO_HAS_64_BIT_ATOMICS_1) // fallback if 64-bit atomics are supported at all
uint64_t atomic_min(volatile global uint64_t* p, uint64_t val) {
	FLOOR_ATOMIC_FALLBACK_FUNC_OP_64(min, global, p, val)
}
int64_t atomic_min(volatile global int64_t* p, int64_t val) {
	FLOOR_ATOMIC_FALLBACK_FUNC_OP_64(min, global, p, val)
}
uint64_t atomic_min(volatile local uint64_t* p, uint64_t val) {
	FLOOR_ATOMIC_FALLBACK_FUNC_OP_64(min, local, p, val)
}
int64_t atomic_min(volatile local int64_t* p, int64_t val) {
	FLOOR_ATOMIC_FALLBACK_FUNC_OP_64(min, local, p, val)
}
#endif

// max
int32_t atomic_max(volatile global int32_t* p, int32_t val);
uint32_t atomic_max(volatile global uint32_t* p, uint32_t val);
int32_t atomic_max(volatile local int32_t* p, int32_t val);
uint32_t atomic_max(volatile local uint32_t* p, uint32_t val);
#if defined(FLOOR_COMPUTE_INFO_HAS_NATIVE_EXTENDED_64_BIT_ATOMICS_1)
uint64_t atom_max(volatile global uint64_t* p, uint64_t val);
int64_t atom_max(volatile global int64_t* p, int64_t val);
uint64_t atom_max(volatile local uint64_t* p, uint64_t val);
int64_t atom_max(volatile local int64_t* p, int64_t val);
uint64_t atomic_max(volatile global uint64_t* p, uint64_t val) { return atom_max(p, val); }
int64_t atomic_max(volatile global int64_t* p, int64_t val) { return atom_max(p, val); }
uint64_t atomic_max(volatile local uint64_t* p, uint64_t val) { return atom_max(p, val); }
int64_t atomic_max(volatile local int64_t* p, int64_t val) { return atom_max(p, val); }
#elif defined(FLOOR_COMPUTE_INFO_HAS_64_BIT_ATOMICS_1)
uint64_t atomic_max(volatile global uint64_t* p, uint64_t val) {
	FLOOR_ATOMIC_FALLBACK_FUNC_OP_64(max, global, p, val)
}
int64_t atomic_max(volatile global int64_t* p, int64_t val) {
	FLOOR_ATOMIC_FALLBACK_FUNC_OP_64(max, global, p, val)
}
uint64_t atomic_max(volatile local uint64_t* p, uint64_t val) {
	FLOOR_ATOMIC_FALLBACK_FUNC_OP_64(max, local, p, val)
}
int64_t atomic_max(volatile local int64_t* p, int64_t val) {
	FLOOR_ATOMIC_FALLBACK_FUNC_OP_64(max, local, p, val)
}
#endif

// and
int32_t atomic_and(volatile global int32_t* p, int32_t val);
uint32_t atomic_and(volatile global uint32_t* p, uint32_t val);
int32_t atomic_and(volatile local int32_t* p, int32_t val);
uint32_t atomic_and(volatile local uint32_t* p, uint32_t val);
#if defined(FLOOR_COMPUTE_INFO_HAS_NATIVE_EXTENDED_64_BIT_ATOMICS_1)
uint64_t atom_and(volatile global uint64_t* p, uint64_t val);
int64_t atom_and(volatile global int64_t* p, int64_t val);
uint64_t atom_and(volatile local uint64_t* p, uint64_t val);
int64_t atom_and(volatile local int64_t* p, int64_t val);
uint64_t atomic_and(volatile global uint64_t* p, uint64_t val) { return atom_and(p, val); }
int64_t atomic_and(volatile global int64_t* p, int64_t val) { return atom_and(p, val); }
uint64_t atomic_and(volatile local uint64_t* p, uint64_t val) { return atom_and(p, val); }
int64_t atomic_and(volatile local int64_t* p, int64_t val) { return atom_and(p, val); }
#elif defined(FLOOR_COMPUTE_INFO_HAS_64_BIT_ATOMICS_1)
uint64_t atomic_and(volatile global uint64_t* p, uint64_t val) {
	FLOOR_ATOMIC_FALLBACK_OP_64(&, global, p, val)
}
int64_t atomic_and(volatie global int64_t* p, int64_t val) {
	FLOOR_ATOMIC_FALLBACK_OP_64(&, global, p, val)
}
uint64_t atomic_and(volatile local uint64_t* p, uint64_t val) {
	FLOOR_ATOMIC_FALLBACK_OP_64(&, local, p, val)
}
int64_t atomic_and(volatile local int64_t* p, int64_t val) {
	FLOOR_ATOMIC_FALLBACK_OP_64(&, local, p, val)
}
#endif

// or
int32_t atomic_or(volatile global int32_t* p, int32_t val);
uint32_t atomic_or(volatile global uint32_t* p, uint32_t val);
int32_t atomic_or(volatile local int32_t* p, int32_t val);
uint32_t atomic_or(volatile local uint32_t* p, uint32_t val);
#if defined(FLOOR_COMPUTE_INFO_HAS_NATIVE_EXTENDED_64_BIT_ATOMICS_1)
uint64_t atom_or(volatile global uint64_t* p, uint64_t val);
int64_t atom_or(volatile global int64_t* p, int64_t val);
uint64_t atom_or(volatile local uint64_t* p, uint64_t val);
int64_t atom_or(volatile local int64_t* p, int64_t val);
uint64_t atomic_or(volatile global uint64_t* p, uint64_t val) { return atom_or(p, val); }
int64_t atomic_or(volatile global int64_t* p, int64_t val) { return atom_or(p, val); }
uint64_t atomic_or(volatile local uint64_t* p, uint64_t val) { return atom_or(p, val); }
int64_t atomic_or(volatile local int64_t* p, int64_t val) { return atom_or(p, val); }
#elif defined(FLOOR_COMPUTE_INFO_HAS_64_BIT_ATOMICS_1)
uint64_t atomic_or(volatile global uint64_t* p, uint64_t val) {
	FLOOR_ATOMIC_FALLBACK_OP_64(|, global, p, val)
}
int64_t atomic_or(volatie global int64_t* p, int64_t val) {
	FLOOR_ATOMIC_FALLBACK_OP_64(|, global, p, val)
}
uint64_t atomic_or(volatile local uint64_t* p, uint64_t val) {
	FLOOR_ATOMIC_FALLBACK_OP_64(|, local, p, val)
}
int64_t atomic_or(volatile local int64_t* p, int64_t val) {
	FLOOR_ATOMIC_FALLBACK_OP_64(|, local, p, val)
}
#endif

// xor
int32_t atomic_xor(volatile global int32_t* p, int32_t val);
uint32_t atomic_xor(volatile global uint32_t* p, uint32_t val);
int32_t atomic_xor(volatile local int32_t* p, int32_t val);
uint32_t atomic_xor(volatile local uint32_t* p, uint32_t val);
#if defined(FLOOR_COMPUTE_INFO_HAS_NATIVE_EXTENDED_64_BIT_ATOMICS_1)
uint64_t atom_xor(volatile global uint64_t* p, uint64_t val);
int64_t atom_xor(volatile global int64_t* p, int64_t val);
uint64_t atom_xor(volatile local uint64_t* p, uint64_t val);
int64_t atom_xor(volatile local int64_t* p, int64_t val);
uint64_t atomic_xor(volatile global uint64_t* p, uint64_t val) { return atom_xor(p, val); }
int64_t atomic_xor(volatile global int64_t* p, int64_t val) { return atom_xor(p, val); }
uint64_t atomic_xor(volatile local uint64_t* p, uint64_t val) { return atom_xor(p, val); }
int64_t atomic_xor(volatile local int64_t* p, int64_t val) { return atom_xor(p, val); }
#elif defined(FLOOR_COMPUTE_INFO_HAS_64_BIT_ATOMICS_1)
uint64_t atomic_xor(volatile global uint64_t* p, uint64_t val) {
	FLOOR_ATOMIC_FALLBACK_OP_64(^, global, p, val)
}
int64_t atomic_xor(volatie global int64_t* p, int64_t val) {
	FLOOR_ATOMIC_FALLBACK_OP_64(^, global, p, val)
}
uint64_t atomic_xor(volatile local uint64_t* p, uint64_t val) {
	FLOOR_ATOMIC_FALLBACK_OP_64(^, local, p, val)
}
int64_t atomic_xor(volatile local int64_t* p, int64_t val) {
	FLOOR_ATOMIC_FALLBACK_OP_64(^, local, p, val)
}
#endif

// store (simple alias of xchg)
floor_inline_always void atomic_store(volatile global int32_t* addr, const int32_t& val) {
	atomic_xchg(addr, val);
}
floor_inline_always void atomic_store(volatile global uint32_t* addr, const uint32_t& val) {
	atomic_xchg(addr, val);
}
floor_inline_always void atomic_store(volatile global float* addr, const float& val) {
	atomic_xchg(addr, val);
}
floor_inline_always void atomic_store(volatile local int32_t* addr, const int32_t& val) {
	atomic_xchg(addr, val);
}
floor_inline_always void atomic_store(volatile local uint32_t* addr, const uint32_t& val) {
	atomic_xchg(addr, val);
}
floor_inline_always void atomic_store(volatile local float* addr, const float& val) {
	atomic_xchg(addr, val);
}
#if defined(FLOOR_COMPUTE_INFO_HAS_64_BIT_ATOMICS_1)
floor_inline_always void atomic_store(volatile global uint64_t* addr, const uint64_t& val) {
	atomic_xchg(addr, val);
}
floor_inline_always void atomic_store(volatile global int64_t* addr, const int64_t& val) {
	atomic_xchg(addr, val);
}
floor_inline_always void atomic_store(volatile local uint64_t* addr, const uint64_t& val) {
	atomic_xchg(addr, val);
}
floor_inline_always void atomic_store(volatile local int64_t* addr, const int64_t& val) {
	atomic_xchg(addr, val);
}
#endif

// load (no proper instruction for this, so just perform a "+ 0")
floor_inline_always int32_t atomic_load(volatile global int32_t* addr) {
	return atomic_add(addr, 0);
}
floor_inline_always uint32_t atomic_load(volatile global uint32_t* addr) {
	return atomic_add(addr, 0u);
}
floor_inline_always float atomic_load(volatile global float* addr) {
	const uint32_t ret = atomic_add((volatile global uint32_t*)addr, 0u);
	return *(float*)&ret;
}
floor_inline_always int32_t atomic_load(volatile local int32_t* addr) {
	return atomic_add(addr, 0);
}
floor_inline_always uint32_t atomic_load(volatile local uint32_t* addr) {
	return atomic_add(addr, 0u);
}
floor_inline_always float atomic_load(volatile local float* addr) {
	const uint32_t ret = atomic_add((volatile local uint32_t*)addr, 0u);
	return *(float*)&ret;
}
#if defined(FLOOR_COMPUTE_INFO_HAS_64_BIT_ATOMICS_1)
floor_inline_always uint64_t atomic_load(volatile global uint64_t* addr) {
	return atomic_add(addr, 0ull);
}
floor_inline_always int64_t atomic_load(volatile global int64_t* addr) {
	return atomic_add(addr, 0ll);
}
floor_inline_always uint64_t atomic_load(volatile local uint64_t* addr) {
	return atomic_add(addr, 0ull);
}
floor_inline_always int64_t atomic_load(volatile local int64_t* addr) {
	return atomic_add(addr, 0ll);
}
#endif

// fallback for non-natively supported float atomics
floor_inline_always float atomic_add(volatile global float* p, float val) { FLOOR_ATOMIC_FALLBACK_OP_32(+, global, p, val) }
floor_inline_always float atomic_add(volatile local float* p, float val) { FLOOR_ATOMIC_FALLBACK_OP_32(+, local, p, val) }
floor_inline_always float atomic_sub(volatile global float* p, float val) { FLOOR_ATOMIC_FALLBACK_OP_32(-, global, p, val) }
floor_inline_always float atomic_sub(volatile local float* p, float val) { FLOOR_ATOMIC_FALLBACK_OP_32(-, local, p, val) }
floor_inline_always float atomic_inc(volatile global float* p) { return atomic_add(p, 1.0f); }
floor_inline_always float atomic_inc(volatile local float* p) { return atomic_add(p, 1.0f); }
floor_inline_always float atomic_dec(volatile global float* p) { return atomic_sub(p, 1.0f); }
floor_inline_always float atomic_dec(volatile local float* p) { return atomic_sub(p, 1.0f); }
floor_inline_always float atomic_cmpxchg(volatile global float* p, float cmp, float val) {
	const auto ret = atomic_cmpxchg((volatile global uint32_t*)p, *(uint32_t*)&cmp, *(uint32_t*)&val);
	return *(float*)ret;
}
floor_inline_always float atomic_cmpxchg(volatile local float* p, float cmp, float val) {
	const auto ret = atomic_cmpxchg((volatile local uint32_t*)p, *(uint32_t*)&cmp, *(uint32_t*)&val);
	return *(float*)ret;
}
floor_inline_always float atomic_min(volatile global float* p, float val) {
	if(val < 0.0f) {
		atomic_max((volatile global uint32_t*)p, *(uint32_t*)&val);
	}
	return atomic_min((volatile global int32_t*)p, *(int32_t*)&val);
}
floor_inline_always float atomic_min(volatile local float* p, float val) {
	if(val < 0.0f) {
		atomic_max((volatile local uint32_t*)p, *(uint32_t*)&val);
	}
	return atomic_min((volatile local int32_t*)p, *(int32_t*)&val);
}
floor_inline_always float atomic_max(volatile global float* p, float val) {
	if(val < 0.0f) {
		atomic_min((volatile global uint32_t*)p, *(uint32_t*)&val);
	}
	return atomic_max((volatile global int32_t*)p, *(int32_t*)&val);
}
floor_inline_always float atomic_max(volatile local float* p, float val) {
	if(val < 0.0f) {
		atomic_min((volatile local uint32_t*)p, *(uint32_t*)&val);
	}
	return atomic_max((volatile local int32_t*)p, *(int32_t*)&val);
}

#endif

#endif
