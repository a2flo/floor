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

#pragma once

#if defined(FLOOR_COMPUTE_OPENCL) || defined(FLOOR_COMPUTE_VULKAN)

// always enable these (required by opencl 1.2 anyways, so easier to just set them instead of looking for uses of them)
#pragma OPENCL EXTENSION cl_khr_global_int32_base_atomics : enable
#pragma OPENCL EXTENSION cl_khr_global_int32_extended_atomics : enable
#pragma OPENCL EXTENSION cl_khr_local_int32_base_atomics : enable
#pragma OPENCL EXTENSION cl_khr_local_int32_extended_atomics : enable

// atomic_* -> atom_* macro aliases
// certain implementations don't have proper atomic_* function implementations and the standard isn't sure about this either
// -> use atom_* for everything (which should always exist), but add atomic_* aliases, because they're nicer
#define atomic_add(...) atom_add(__VA_ARGS__)
#define atomic_sub(...) atom_sub(__VA_ARGS__)
#define atomic_inc(...) atom_inc(__VA_ARGS__)
#define atomic_dec(...) atom_dec(__VA_ARGS__)
#define atomic_xchg(...) atom_xchg(__VA_ARGS__)
#define atomic_cmpxchg(...) atom_cmpxchg(__VA_ARGS__)
#define atomic_min(...) atom_min(__VA_ARGS__)
#define atomic_max(...) atom_max(__VA_ARGS__)
#define atomic_and(...) atom_and(__VA_ARGS__)
#define atomic_or(...) atom_or(__VA_ARGS__)
#define atomic_xor(...) atom_xor(__VA_ARGS__)
#define atomic_store(...) atom_store(__VA_ARGS__)
#define atomic_load(...) atom_load(__VA_ARGS__)

// add
int32_t atom_add(global int32_t* p, int32_t val);
uint32_t atom_add(global uint32_t* p, uint32_t val);
int32_t atom_add(local int32_t* p, int32_t val);
uint32_t atom_add(local uint32_t* p, uint32_t val);
#if defined(FLOOR_COMPUTE_INFO_HAS_32_BIT_FLOAT_ATOMICS_1)
float atom_add(global float* p, float val);
float atom_add(local float* p, float val);
#endif
#if defined(FLOOR_COMPUTE_INFO_HAS_64_BIT_ATOMICS_1)
uint64_t atom_add(global uint64_t* p, uint64_t val);
int64_t atom_add(global int64_t* p, int64_t val);
uint64_t atom_add(local uint64_t* p, uint64_t val);
int64_t atom_add(local int64_t* p, int64_t val);
#endif

// sub
int32_t atom_sub(global int32_t* p, int32_t val);
uint32_t atom_sub(global uint32_t* p, uint32_t val);
int32_t atom_sub(local int32_t* p, int32_t val);
uint32_t atom_sub(local uint32_t* p, uint32_t val);
#if defined(FLOOR_COMPUTE_INFO_HAS_32_BIT_FLOAT_ATOMICS_1)
float atom_sub(global float* p, float val) { return atom_add(p, -val); }
float atom_sub(local float* p, float val) { return atom_add(p, -val); }
#endif
#if defined(FLOOR_COMPUTE_INFO_HAS_64_BIT_ATOMICS_1)
uint64_t atom_sub(global uint64_t* p, uint64_t val);
int64_t atom_sub(global int64_t* p, int64_t val);
uint64_t atom_sub(local uint64_t* p, uint64_t val);
int64_t atom_sub(local int64_t* p, int64_t val);
#endif

// inc
int32_t atom_inc(global int32_t* p);
uint32_t atom_inc(global uint32_t* p);
int32_t atom_inc(local int32_t* p);
uint32_t atom_inc(local uint32_t* p);
#if defined(FLOOR_COMPUTE_INFO_HAS_32_BIT_FLOAT_ATOMICS_1)
float atom_inc(global float* p, float val) { return atom_add(p, 1.0f); }
float atom_inc(local float* p, float val) { return atom_add(p, 1.0f); }
#endif
#if defined(FLOOR_COMPUTE_INFO_HAS_64_BIT_ATOMICS_1)
uint64_t atom_inc(global uint64_t* p);
int64_t atom_inc(global int64_t* p);
uint64_t atom_inc(local uint64_t* p);
int64_t atom_inc(local int64_t* p);
#endif

// dec
int32_t atom_dec(global int32_t* p);
uint32_t atom_dec(global uint32_t* p);
int32_t atom_dec(local int32_t* p);
uint32_t atom_dec(local uint32_t* p);
#if defined(FLOOR_COMPUTE_INFO_HAS_32_BIT_FLOAT_ATOMICS_1)
float atom_dec(global float* p, float val) { return atom_add(p, -1.0f); }
float atom_dec(local float* p, float val) { return atom_add(p, -1.0f); }
#endif
#if defined(FLOOR_COMPUTE_INFO_HAS_64_BIT_ATOMICS_1)
uint64_t atom_dec(global uint64_t* p);
int64_t atom_dec(global int64_t* p);
uint64_t atom_dec(local uint64_t* p);
int64_t atom_dec(local int64_t* p);
#endif

// xchg
int32_t atom_xchg(global int32_t* p, int32_t val);
uint32_t atom_xchg(global uint32_t* p, uint32_t val);
float atom_xchg(global float* p, float val);
int32_t atom_xchg(local int32_t* p, int32_t val);
uint32_t atom_xchg(local uint32_t* p, uint32_t val);
float atom_xchg(local float* p, float val);
#if defined(FLOOR_COMPUTE_INFO_HAS_64_BIT_ATOMICS_1)
uint64_t atom_xchg(global uint64_t* p, uint64_t val);
int64_t atom_xchg(global int64_t* p, int64_t val);
uint64_t atom_xchg(local uint64_t* p, uint64_t val);
int64_t atom_xchg(local int64_t* p, int64_t val);
#endif

// cmpxchg
int32_t atom_cmpxchg(global int32_t* p, int32_t cmp, int32_t val);
uint32_t atom_cmpxchg(global uint32_t* p, uint32_t cmp, uint32_t val);
int32_t atom_cmpxchg(local int32_t* p, int32_t cmp, int32_t val);
uint32_t atom_cmpxchg(local uint32_t* p, uint32_t cmp, uint32_t val);
#if defined(FLOOR_COMPUTE_INFO_HAS_64_BIT_ATOMICS_1)
uint64_t atom_cmpxchg(global uint64_t* p, uint64_t cmp, uint64_t val);
int64_t atom_cmpxchg(global int64_t* p, int64_t cmp, int64_t val);
uint64_t atom_cmpxchg(local uint64_t* p, uint64_t cmp, uint64_t val);
int64_t atom_cmpxchg(local int64_t* p, int64_t cmp, int64_t val);
#endif

// min
int32_t atom_min(global int32_t* p, int32_t val);
uint32_t atom_min(global uint32_t* p, uint32_t val);
int32_t atom_min(local int32_t* p, int32_t val);
uint32_t atom_min(local uint32_t* p, uint32_t val);
#if defined(FLOOR_COMPUTE_INFO_HAS_NATIVE_EXTENDED_64_BIT_ATOMICS_1)
uint64_t atom_min(global uint64_t* p, uint64_t val);
int64_t atom_min(global int64_t* p, int64_t val);
uint64_t atom_min(local uint64_t* p, uint64_t val);
int64_t atom_min(local int64_t* p, int64_t val);
#elif defined(FLOOR_COMPUTE_INFO_HAS_64_BIT_ATOMICS_1) // fallback if 64-bit atomics are supported at all
floor_inline_always uint64_t atom_min(global uint64_t* p, uint64_t val) {
	FLOOR_ATOMIC_FALLBACK_FUNC_OP_64(min, global, p, val)
}
floor_inline_always int64_t atom_min(global int64_t* p, int64_t val) {
	FLOOR_ATOMIC_FALLBACK_FUNC_OP_64(min, global, p, val)
}
floor_inline_always uint64_t atom_min(local uint64_t* p, uint64_t val) {
	FLOOR_ATOMIC_FALLBACK_FUNC_OP_64(min, local, p, val)
}
floor_inline_always int64_t atom_min(local int64_t* p, int64_t val) {
	FLOOR_ATOMIC_FALLBACK_FUNC_OP_64(min, local, p, val)
}
#endif

// max
int32_t atom_max(global int32_t* p, int32_t val);
uint32_t atom_max(global uint32_t* p, uint32_t val);
int32_t atom_max(local int32_t* p, int32_t val);
uint32_t atom_max(local uint32_t* p, uint32_t val);
#if defined(FLOOR_COMPUTE_INFO_HAS_NATIVE_EXTENDED_64_BIT_ATOMICS_1)
uint64_t atom_max(global uint64_t* p, uint64_t val);
int64_t atom_max(global int64_t* p, int64_t val);
uint64_t atom_max(local uint64_t* p, uint64_t val);
int64_t atom_max(local int64_t* p, int64_t val);
#elif defined(FLOOR_COMPUTE_INFO_HAS_64_BIT_ATOMICS_1)
floor_inline_always uint64_t atom_max(global uint64_t* p, uint64_t val) {
	FLOOR_ATOMIC_FALLBACK_FUNC_OP_64(max, global, p, val)
}
floor_inline_always int64_t atom_max(global int64_t* p, int64_t val) {
	FLOOR_ATOMIC_FALLBACK_FUNC_OP_64(max, global, p, val)
}
floor_inline_always uint64_t atom_max(local uint64_t* p, uint64_t val) {
	FLOOR_ATOMIC_FALLBACK_FUNC_OP_64(max, local, p, val)
}
floor_inline_always int64_t atom_max(local int64_t* p, int64_t val) {
	FLOOR_ATOMIC_FALLBACK_FUNC_OP_64(max, local, p, val)
}
#endif

// and
int32_t atom_and(global int32_t* p, int32_t val);
uint32_t atom_and(global uint32_t* p, uint32_t val);
int32_t atom_and(local int32_t* p, int32_t val);
uint32_t atom_and(local uint32_t* p, uint32_t val);
#if defined(FLOOR_COMPUTE_INFO_HAS_NATIVE_EXTENDED_64_BIT_ATOMICS_1)
uint64_t atom_and(global uint64_t* p, uint64_t val);
int64_t atom_and(global int64_t* p, int64_t val);
uint64_t atom_and(local uint64_t* p, uint64_t val);
int64_t atom_and(local int64_t* p, int64_t val);
#elif defined(FLOOR_COMPUTE_INFO_HAS_64_BIT_ATOMICS_1)
floor_inline_always uint64_t atom_and(global uint64_t* p, uint64_t val) {
	FLOOR_ATOMIC_FALLBACK_OP_64(&, global, p, val)
}
floor_inline_always int64_t atom_and(volatie global int64_t* p, int64_t val) {
	FLOOR_ATOMIC_FALLBACK_OP_64(&, global, p, val)
}
floor_inline_always uint64_t atom_and(local uint64_t* p, uint64_t val) {
	FLOOR_ATOMIC_FALLBACK_OP_64(&, local, p, val)
}
floor_inline_always int64_t atom_and(local int64_t* p, int64_t val) {
	FLOOR_ATOMIC_FALLBACK_OP_64(&, local, p, val)
}
#endif

// or
int32_t atom_or(global int32_t* p, int32_t val);
uint32_t atom_or(global uint32_t* p, uint32_t val);
int32_t atom_or(local int32_t* p, int32_t val);
uint32_t atom_or(local uint32_t* p, uint32_t val);
#if defined(FLOOR_COMPUTE_INFO_HAS_NATIVE_EXTENDED_64_BIT_ATOMICS_1)
uint64_t atom_or(global uint64_t* p, uint64_t val);
int64_t atom_or(global int64_t* p, int64_t val);
uint64_t atom_or(local uint64_t* p, uint64_t val);
int64_t atom_or(local int64_t* p, int64_t val);
#elif defined(FLOOR_COMPUTE_INFO_HAS_64_BIT_ATOMICS_1)
floor_inline_always uint64_t atom_or(global uint64_t* p, uint64_t val) {
	FLOOR_ATOMIC_FALLBACK_OP_64(|, global, p, val)
}
floor_inline_always int64_t atom_or(volatie global int64_t* p, int64_t val) {
	FLOOR_ATOMIC_FALLBACK_OP_64(|, global, p, val)
}
floor_inline_always uint64_t atom_or(local uint64_t* p, uint64_t val) {
	FLOOR_ATOMIC_FALLBACK_OP_64(|, local, p, val)
}
floor_inline_always int64_t atom_or(local int64_t* p, int64_t val) {
	FLOOR_ATOMIC_FALLBACK_OP_64(|, local, p, val)
}
#endif

// xor
int32_t atom_xor(global int32_t* p, int32_t val);
uint32_t atom_xor(global uint32_t* p, uint32_t val);
int32_t atom_xor(local int32_t* p, int32_t val);
uint32_t atom_xor(local uint32_t* p, uint32_t val);
#if defined(FLOOR_COMPUTE_INFO_HAS_NATIVE_EXTENDED_64_BIT_ATOMICS_1)
uint64_t atom_xor(global uint64_t* p, uint64_t val);
int64_t atom_xor(global int64_t* p, int64_t val);
uint64_t atom_xor(local uint64_t* p, uint64_t val);
int64_t atom_xor(local int64_t* p, int64_t val);
#elif defined(FLOOR_COMPUTE_INFO_HAS_64_BIT_ATOMICS_1)
floor_inline_always uint64_t atom_xor(global uint64_t* p, uint64_t val) {
	FLOOR_ATOMIC_FALLBACK_OP_64(^, global, p, val)
}
floor_inline_always int64_t atom_xor(volatie global int64_t* p, int64_t val) {
	FLOOR_ATOMIC_FALLBACK_OP_64(^, global, p, val)
}
floor_inline_always uint64_t atom_xor(local uint64_t* p, uint64_t val) {
	FLOOR_ATOMIC_FALLBACK_OP_64(^, local, p, val)
}
floor_inline_always int64_t atom_xor(local int64_t* p, int64_t val) {
	FLOOR_ATOMIC_FALLBACK_OP_64(^, local, p, val)
}
#endif

// TODO: use actual load/store instructions for Vulkan and more modern OpenCL

// store (simple alias of xchg)
floor_inline_always void atom_store(global int32_t* addr, const int32_t& val) {
	atom_xchg(addr, val);
}
floor_inline_always void atom_store(global uint32_t* addr, const uint32_t& val) {
	atom_xchg(addr, val);
}
floor_inline_always void atom_store(global float* addr, const float& val) {
	atom_xchg(addr, val);
}
floor_inline_always void atom_store(local int32_t* addr, const int32_t& val) {
	atom_xchg(addr, val);
}
floor_inline_always void atom_store(local uint32_t* addr, const uint32_t& val) {
	atom_xchg(addr, val);
}
floor_inline_always void atom_store(local float* addr, const float& val) {
	atom_xchg(addr, val);
}
#if defined(FLOOR_COMPUTE_INFO_HAS_64_BIT_ATOMICS_1)
floor_inline_always void atom_store(global uint64_t* addr, const uint64_t& val) {
	atom_xchg(addr, val);
}
floor_inline_always void atom_store(global int64_t* addr, const int64_t& val) {
	atom_xchg(addr, val);
}
floor_inline_always void atom_store(local uint64_t* addr, const uint64_t& val) {
	atom_xchg(addr, val);
}
floor_inline_always void atom_store(local int64_t* addr, const int64_t& val) {
	atom_xchg(addr, val);
}
#endif

// load (no proper instruction for this, so just perform a "+ 0")
floor_inline_always int32_t atom_load(const global int32_t* addr) {
	return atom_add((global int32_t*)addr, 0);
}
floor_inline_always uint32_t atom_load(const global uint32_t* addr) {
	return atom_add((global uint32_t*)addr, 0u);
}
floor_inline_always float atom_load(const global float* addr) {
	const uint32_t ret = atom_add((global uint32_t*)addr, 0u);
	return *(float*)&ret;
}
floor_inline_always int32_t atom_load(const local int32_t* addr) {
	return atom_add((local int32_t*)addr, 0);
}
floor_inline_always uint32_t atom_load(const local uint32_t* addr) {
	return atom_add((local uint32_t*)addr, 0u);
}
floor_inline_always float atom_load(const local float* addr) {
	const uint32_t ret = atom_add((local uint32_t*)addr, 0u);
	return *(float*)&ret;
}
#if defined(FLOOR_COMPUTE_INFO_HAS_64_BIT_ATOMICS_1)
floor_inline_always uint64_t atom_load(const global uint64_t* addr) {
	return atom_add((global uint64_t*)addr, 0ull);
}
floor_inline_always int64_t atom_load(const global int64_t* addr) {
	return atom_add((global int64_t*)addr, 0ll);
}
floor_inline_always uint64_t atom_load(const local uint64_t* addr) {
	return atom_add((local uint64_t*)addr, 0ull);
}
floor_inline_always int64_t atom_load(const local int64_t* addr) {
	return atom_add((local int64_t*)addr, 0ll);
}
#endif

// fallback for non-natively supported float atomics
#if !defined(FLOOR_COMPUTE_INFO_HAS_32_BIT_FLOAT_ATOMICS_1)
floor_inline_always float atom_add(global float* p, float val) { FLOOR_ATOMIC_FALLBACK_OP_32(+, global, p, val) }
floor_inline_always float atom_add(local float* p, float val) { FLOOR_ATOMIC_FALLBACK_OP_32(+, local, p, val) }
floor_inline_always float atom_sub(global float* p, float val) { FLOOR_ATOMIC_FALLBACK_OP_32(-, global, p, val) }
floor_inline_always float atom_sub(local float* p, float val) { FLOOR_ATOMIC_FALLBACK_OP_32(-, local, p, val) }
floor_inline_always float atom_inc(global float* p) { return atom_add(p, 1.0f); }
floor_inline_always float atom_inc(local float* p) { return atom_add(p, 1.0f); }
floor_inline_always float atom_dec(global float* p) { return atom_sub(p, 1.0f); }
floor_inline_always float atom_dec(local float* p) { return atom_sub(p, 1.0f); }
#endif
// NOTE: the trouble with these is that pointer bitcasts are problematic in Vulkan
floor_inline_always float atom_cmpxchg(global float* p, float cmp, float val) {
	const auto ret = atom_cmpxchg((global uint32_t*)p, *(uint32_t*)&cmp, *(uint32_t*)&val);
	return *(float*)ret;
}
floor_inline_always float atom_cmpxchg(local float* p, float cmp, float val) {
	const auto ret = atom_cmpxchg((local uint32_t*)p, *(uint32_t*)&cmp, *(uint32_t*)&val);
	return *(float*)ret;
}
#if !defined(FLOOR_COMPUTE_INFO_HAS_32_BIT_FLOAT_ATOMICS_1)
floor_inline_always float atom_min(global float* p, float val) {
	if(val < 0.0f) {
		return atom_max((global uint32_t*)p, *(uint32_t*)&val);
	}
	return atom_min((global int32_t*)p, *(int32_t*)&val);
}
floor_inline_always float atom_min(local float* p, float val) {
	if(val < 0.0f) {
		return atom_max((local uint32_t*)p, *(uint32_t*)&val);
	}
	return atom_min((local int32_t*)p, *(int32_t*)&val);
}
floor_inline_always float atom_max(global float* p, float val) {
	if(val < 0.0f) {
		return atom_min((global uint32_t*)p, *(uint32_t*)&val);
	}
	return atom_max((global int32_t*)p, *(int32_t*)&val);
}
floor_inline_always float atom_max(local float* p, float val) {
	if(val < 0.0f) {
		return atom_min((local uint32_t*)p, *(uint32_t*)&val);
	}
	return atom_max((local int32_t*)p, *(int32_t*)&val);
}
#else // even with float32 atomic support, we can't actually use atom_cmpxchg -> use atomic_xchg loop instead
floor_inline_always float atom_min(global float* p, float val) {
	auto cur_min = min(*p, val);
	for (;;) {
		auto old_min = atomic_xchg(p, cur_min);
		if (old_min >= cur_min) {
			return;
		}
		cur_min = old_min;
	}
}
floor_inline_always float atom_min(local float* p, float val) {
	auto cur_min = min(*p, val);
	for (;;) {
		auto old_min = atomic_xchg(p, cur_min);
		if (old_min >= cur_min) {
			return;
		}
		cur_min = old_min;
	}
}
floor_inline_always float atom_max(global float* p, float val) {
	auto cur_max = max(*p, val);
	for (;;) {
		auto old_max = atomic_xchg(p, cur_max);
		if (old_max <= cur_max) {
			return;
		}
		cur_max = old_max;
	}
}
floor_inline_always float atom_max(local float* p, float val) {
	auto cur_max = max(*p, val);
	for (;;) {
		auto old_max = atomic_xchg(p, cur_max);
		if (old_max <= cur_max) {
			return;
		}
		cur_max = old_max;
	}
}
#endif

#endif
