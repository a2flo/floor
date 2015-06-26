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

// sub
int32_t atomic_sub(volatile global int32_t* p, int32_t val);
uint32_t atomic_sub(volatile global uint32_t* p, uint32_t val);
int32_t atomic_sub(volatile local int32_t* p, int32_t val);
uint32_t atomic_sub(volatile local uint32_t* p, uint32_t val);

// inc
int32_t atomic_inc(volatile global int32_t* p);
uint32_t atomic_inc(volatile global uint32_t* p);
int32_t atomic_inc(volatile local int32_t* p);
uint32_t atomic_inc(volatile local uint32_t* p);

// dec
int32_t atomic_dec(volatile global int32_t* p);
uint32_t atomic_dec(volatile global uint32_t* p);
int32_t atomic_dec(volatile local int32_t* p);
uint32_t atomic_dec(volatile local uint32_t* p);

// xchg
int32_t atomic_xchg(volatile global int32_t* p, int32_t val);
uint32_t atomic_xchg(volatile global uint32_t* p, uint32_t val);
float atomic_xchg(volatile global float* p, float val);
int32_t atomic_xchg(volatile local int32_t* p, int32_t val);
uint32_t atomic_xchg(volatile local uint32_t* p, uint32_t val);
float atomic_xchg(volatile local float* p, float val);

// cmpxchg
int32_t atomic_cmpxchg(volatile global int32_t* p, int32_t cmp, int32_t val);
uint32_t atomic_cmpxchg(volatile global uint32_t* p, uint32_t cmp, uint32_t val);
int32_t atomic_cmpxchg(volatile local int32_t* p, int32_t cmp, int32_t val);
uint32_t atomic_cmpxchg(volatile local uint32_t* p, uint32_t cmp, uint32_t val);

// min
int32_t atomic_min(volatile global int32_t* p, int32_t val);
uint32_t atomic_min(volatile global uint32_t* p, uint32_t val);
int32_t atomic_min(volatile local int32_t* p, int32_t val);
uint32_t atomic_min(volatile local uint32_t* p, uint32_t val);

// max
int32_t atomic_max(volatile global int32_t* p, int32_t val);
uint32_t atomic_max(volatile global uint32_t* p, uint32_t val);
int32_t atomic_max(volatile local int32_t* p, int32_t val);
uint32_t atomic_max(volatile local uint32_t* p, uint32_t val);

// and
int32_t atomic_and(volatile global int32_t* p, int32_t val);
uint32_t atomic_and(volatile global uint32_t* p, uint32_t val);
int32_t atomic_and(volatile local int32_t* p, int32_t val);
uint32_t atomic_and(volatile local uint32_t* p, uint32_t val);

// or
int32_t atomic_or(volatile global int32_t* p, int32_t val);
uint32_t atomic_or(volatile global uint32_t* p, uint32_t val);
int32_t atomic_or(volatile local int32_t* p, int32_t val);
uint32_t atomic_or(volatile local uint32_t* p, uint32_t val);

// xor
int32_t atomic_xor(volatile global int32_t* p, int32_t val);
uint32_t atomic_xor(volatile global uint32_t* p, uint32_t val);
int32_t atomic_xor(volatile local int32_t* p, int32_t val);
uint32_t atomic_xor(volatile local uint32_t* p, uint32_t val);

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

// fallback for non-natively supported float atomics
#define FLOOR_OPENCL_ATOMIC_FLOAT_OP(op, as, ptr, val) for(;;) { \
	const auto expected = *ptr; \
	const auto wanted = expected op val; \
	if(atomic_cmpxchg((volatile as uint32_t*)ptr, *(uint32_t*)&expected, *(uint32_t*)&wanted) == expected) { \
		return expected; \
	} \
}

floor_inline_always float atomic_add(volatile global float* p, float val) { FLOOR_OPENCL_ATOMIC_FLOAT_OP(+, global, p, val) }
floor_inline_always float atomic_add(volatile local float* p, float val) { FLOOR_OPENCL_ATOMIC_FLOAT_OP(+, local, p, val) }
floor_inline_always float atomic_sub(volatile global float* p, float val) { FLOOR_OPENCL_ATOMIC_FLOAT_OP(-, global, p, val) }
floor_inline_always float atomic_sub(volatile local float* p, float val) { FLOOR_OPENCL_ATOMIC_FLOAT_OP(-, local, p, val) }
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

#undef FLOOR_OPENCL_ATOMIC_FLOAT_OP

#endif

#endif
