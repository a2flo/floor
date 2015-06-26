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

#ifndef __FLOOR_COMPUTE_DEVICE_METAL_ATOMIC_HPP__
#define __FLOOR_COMPUTE_DEVICE_METAL_ATOMIC_HPP__

#if defined(FLOOR_COMPUTE_METAL)

// underlying atomic functions
// NOTE: only supported memory order is _AIR_MEM_ORDER_RELAXED (0)
#define FLOOR_METAL_MEM_ORDER_RELAXED 0
metal_func void metal_atomic_store(global uint32_t* p, uint32_t desired, uint32_t mem_order, uint32_t scope) asm("air.atomic.global.store.i32");
metal_func void metal_atomic_store(local uint32_t* p, uint32_t desired, uint32_t mem_order, uint32_t scope) asm("air.atomic.local.store.i32");

metal_func uint32_t metal_atomic_load(global uint32_t* p, uint32_t mem_order, uint32_t scope) asm("air.atomic.global.load.i32");
metal_func uint32_t metal_atomic_load(local uint32_t* p, uint32_t mem_order, uint32_t scope) asm("air.atomic.local.load.i32");

metal_func uint32_t metal_atomic_xchg(global uint32_t* p, uint32_t desired, uint32_t mem_order, uint32_t scope) asm("air.atomic.global.xchg.i32");
metal_func uint32_t metal_atomic_xchg(local uint32_t* p, uint32_t desired, uint32_t mem_order, uint32_t scope) asm("air.atomic.local.xchg.i32");

metal_func uint32_t metal_atomic_cmpxchg(global uint32_t* p, uint32_t* expected, uint32_t desired,
										 uint32_t mem_order_success, uint32_t mem_order_failure, uint32_t scope) asm("air.atomic.global.cmpxchg.weak.i32"); //!< weak
metal_func uint32_t metal_atomic_cmpxchg(local uint32_t* p, uint32_t* expected, uint32_t desired,
										 uint32_t mem_order_success, uint32_t mem_order_failure, uint32_t scope) asm("air.atomic.local.cmpxchg.weak.i32"); //!< weak

metal_func uint32_t metal_atomic_add(global uint32_t* p, uint32_t val, uint32_t mem_order, uint32_t scope) asm("air.atomic.global.add.u.i32");
metal_func int32_t metal_atomic_add(global int32_t* p, int32_t val, uint32_t mem_order, uint32_t scope) asm("air.atomic.global.add.s.i32");
metal_func uint32_t metal_atomic_add(local uint32_t* p, uint32_t val, uint32_t mem_order, uint32_t scope) asm("air.atomic.local.add.u.i32");
metal_func int32_t metal_atomic_add(local int32_t* p, int32_t val, uint32_t mem_order, uint32_t scope) asm("air.atomic.local.add.s.i32");

metal_func uint32_t metal_atomic_sub(global uint32_t* p, uint32_t val, uint32_t mem_order, uint32_t scope) asm("air.atomic.global.sub.u.i32");
metal_func int32_t metal_atomic_sub(global int32_t* p, int32_t val, uint32_t mem_order, uint32_t scope) asm("air.atomic.global.sub.s.i32");
metal_func uint32_t metal_atomic_sub(local uint32_t* p, uint32_t val, uint32_t mem_order, uint32_t scope) asm("air.atomic.local.sub.u.i32");
metal_func int32_t metal_atomic_sub(local int32_t* p, int32_t val, uint32_t mem_order, uint32_t scope) asm("air.atomic.local.sub.s.i32");

metal_func uint32_t metal_atomic_min(global uint32_t* p, uint32_t val, uint32_t mem_order, uint32_t scope) asm("air.atomic.global.min.u.i32");
metal_func int32_t metal_atomic_min(global int32_t* p, int32_t val, uint32_t mem_order, uint32_t scope) asm("air.atomic.global.min.s.i32");
metal_func uint32_t metal_atomic_min(local uint32_t* p, uint32_t val, uint32_t mem_order, uint32_t scope) asm("air.atomic.local.min.u.i32");
metal_func int32_t metal_atomic_min(local int32_t* p, int32_t val, uint32_t mem_order, uint32_t scope) asm("air.atomic.local.min.s.i32");

metal_func uint32_t metal_atomic_max(global uint32_t* p, uint32_t val, uint32_t mem_order, uint32_t scope) asm("air.atomic.global.max.u.i32");
metal_func int32_t metal_atomic_max(global int32_t* p, int32_t val, uint32_t mem_order, uint32_t scope) asm("air.atomic.global.max.s.i32");
metal_func uint32_t metal_atomic_max(local uint32_t* p, uint32_t val, uint32_t mem_order, uint32_t scope) asm("air.atomic.local.max.u.i32");
metal_func int32_t metal_atomic_max(local int32_t* p, int32_t val, uint32_t mem_order, uint32_t scope) asm("air.atomic.local.max.s.i32");

metal_func uint32_t metal_atomic_and(global uint32_t* p, uint32_t val, uint32_t mem_order, uint32_t scope) asm("air.atomic.global.and.u.i32");
metal_func int32_t metal_atomic_and(global int32_t* p, int32_t val, uint32_t mem_order, uint32_t scope) asm("air.atomic.global.and.s.i32");
metal_func uint32_t metal_atomic_and(local uint32_t* p, uint32_t val, uint32_t mem_order, uint32_t scope) asm("air.atomic.local.and.u.i32");
metal_func int32_t metal_atomic_and(local int32_t* p, int32_t val, uint32_t mem_order, uint32_t scope) asm("air.atomic.local.and.s.i32");

metal_func uint32_t metal_atomic_or(global uint32_t* p, uint32_t val, uint32_t mem_order, uint32_t scope) asm("air.atomic.global.or.u.i32");
metal_func int32_t metal_atomic_or(global int32_t* p, int32_t val, uint32_t mem_order, uint32_t scope) asm("air.atomic.global.or.s.i32");
metal_func uint32_t metal_atomic_or(local uint32_t* p, uint32_t val, uint32_t mem_order, uint32_t scope) asm("air.atomic.local.or.u.i32");
metal_func int32_t metal_atomic_or(local int32_t* p, int32_t val, uint32_t mem_order, uint32_t scope) asm("air.atomic.local.or.s.i32");

metal_func uint32_t metal_atomic_xor(global uint32_t* p, uint32_t val, uint32_t mem_order, uint32_t scope) asm("air.atomic.global.xor.u.i32");
metal_func int32_t metal_atomic_xor(global int32_t* p, int32_t val, uint32_t mem_order, uint32_t scope) asm("air.atomic.global.xor.s.i32");
metal_func uint32_t metal_atomic_xor(local uint32_t* p, uint32_t val, uint32_t mem_order, uint32_t scope) asm("air.atomic.local.xor.u.i32");
metal_func int32_t metal_atomic_xor(local int32_t* p, int32_t val, uint32_t mem_order, uint32_t scope) asm("air.atomic.local.xor.s.i32");

// add
floor_inline_always int32_t atomic_add(global int32_t* p, int32_t val) {
	return metal_atomic_add(p, val, FLOOR_METAL_MEM_ORDER_RELAXED, FLOOR_METAL_SCOPE_GLOBAL);
}
floor_inline_always uint32_t atomic_add(global uint32_t* p, uint32_t val) {
	return metal_atomic_add(p, val, FLOOR_METAL_MEM_ORDER_RELAXED, FLOOR_METAL_SCOPE_GLOBAL);
}
floor_inline_always int32_t atomic_add(local int32_t* p, int32_t val) {
	return metal_atomic_add(p, val, FLOOR_METAL_MEM_ORDER_RELAXED, FLOOR_METAL_SCOPE_LOCAL);
}
floor_inline_always uint32_t atomic_add(local uint32_t* p, uint32_t val) {
	return metal_atomic_add(p, val, FLOOR_METAL_MEM_ORDER_RELAXED, FLOOR_METAL_SCOPE_LOCAL);
}

// sub
floor_inline_always int32_t atomic_sub(global int32_t* p, int32_t val) {
	return metal_atomic_sub(p, val, FLOOR_METAL_MEM_ORDER_RELAXED, FLOOR_METAL_SCOPE_GLOBAL);
}
floor_inline_always uint32_t atomic_sub(global uint32_t* p, uint32_t val) {
	return metal_atomic_sub(p, val, FLOOR_METAL_MEM_ORDER_RELAXED, FLOOR_METAL_SCOPE_GLOBAL);
}
floor_inline_always int32_t atomic_sub(local int32_t* p, int32_t val) {
	return metal_atomic_sub(p, val, FLOOR_METAL_MEM_ORDER_RELAXED, FLOOR_METAL_SCOPE_LOCAL);
}
floor_inline_always uint32_t atomic_sub(local uint32_t* p, uint32_t val) {
	return metal_atomic_sub(p, val, FLOOR_METAL_MEM_ORDER_RELAXED, FLOOR_METAL_SCOPE_LOCAL);
}

// inc
floor_inline_always int32_t atomic_inc(global int32_t* p) {
	return atomic_add(p, 1);
}
floor_inline_always uint32_t atomic_inc(global uint32_t* p) {
	return atomic_add(p, 1u);
}
floor_inline_always int32_t atomic_inc(local int32_t* p) {
	return atomic_add(p, 1);
}
floor_inline_always uint32_t atomic_inc(local uint32_t* p) {
	return atomic_add(p, 1u);
}

// dec
floor_inline_always int32_t atomic_dec(global int32_t* p) {
	return atomic_sub(p, 1);
}
floor_inline_always uint32_t atomic_dec(global uint32_t* p) {
	return atomic_sub(p, 1u);
}
floor_inline_always int32_t atomic_dec(local int32_t* p) {
	return atomic_sub(p, 1);
}
floor_inline_always uint32_t atomic_dec(local uint32_t* p) {
	return atomic_sub(p, 1u);
}

// xchg
floor_inline_always int32_t atomic_xchg(global int32_t* p, int32_t val) {
	const uint32_t ret = metal_atomic_xchg((global uint32_t*)p, *(uint32_t*)&val, FLOOR_METAL_MEM_ORDER_RELAXED, FLOOR_METAL_SCOPE_GLOBAL);
	return *(int32_t*)&ret;
}
floor_inline_always uint32_t atomic_xchg(global uint32_t* p, uint32_t val) {
	return metal_atomic_xchg(p, val, FLOOR_METAL_MEM_ORDER_RELAXED, FLOOR_METAL_SCOPE_GLOBAL);
}
floor_inline_always float atomic_xchg(global float* p, float val) {
	const uint32_t ret = metal_atomic_xchg((global uint32_t*)p, *(uint32_t*)&val, FLOOR_METAL_MEM_ORDER_RELAXED, FLOOR_METAL_SCOPE_GLOBAL);
	return *(float*)&ret;
}
floor_inline_always int32_t atomic_xchg(local int32_t* p, int32_t val) {
	const uint32_t ret = metal_atomic_xchg((local uint32_t*)p, *(uint32_t*)&val, FLOOR_METAL_MEM_ORDER_RELAXED, FLOOR_METAL_SCOPE_LOCAL);
	return *(int32_t*)&ret;
}
floor_inline_always uint32_t atomic_xchg(local uint32_t* p, uint32_t val) {
	return metal_atomic_xchg(p, val, FLOOR_METAL_MEM_ORDER_RELAXED, FLOOR_METAL_SCOPE_LOCAL);
}
floor_inline_always float atomic_xchg(local float* p, float val) {
	const uint32_t ret = metal_atomic_xchg((local uint32_t*)p, *(uint32_t*)&val, FLOOR_METAL_MEM_ORDER_RELAXED, FLOOR_METAL_SCOPE_LOCAL);
	return *(float*)&ret;
}

// cmpxchg
floor_inline_always int32_t atomic_cmpxchg(global int32_t* p, int32_t cmp, int32_t val) {
	const uint32_t ret = metal_atomic_cmpxchg((global uint32_t*)p, (uint32_t*)&cmp, *(uint32_t*)&val, FLOOR_METAL_MEM_ORDER_RELAXED, FLOOR_METAL_MEM_ORDER_RELAXED, FLOOR_METAL_SCOPE_GLOBAL);
	return *(int32_t*)&ret;
}
floor_inline_always uint32_t atomic_cmpxchg(global uint32_t* p, uint32_t cmp, uint32_t val) {
	return metal_atomic_cmpxchg(p, &cmp, val, FLOOR_METAL_MEM_ORDER_RELAXED, FLOOR_METAL_MEM_ORDER_RELAXED, FLOOR_METAL_SCOPE_GLOBAL);
}
floor_inline_always int32_t atomic_cmpxchg(local int32_t* p, int32_t cmp, int32_t val) {
	const uint32_t ret = metal_atomic_cmpxchg((local uint32_t*)p, (uint32_t*)&cmp, *(uint32_t*)&val, FLOOR_METAL_MEM_ORDER_RELAXED, FLOOR_METAL_MEM_ORDER_RELAXED, FLOOR_METAL_SCOPE_LOCAL);
	return *(int32_t*)&ret;
}
floor_inline_always uint32_t atomic_cmpxchg(local uint32_t* p, uint32_t cmp, uint32_t val) {
	return metal_atomic_cmpxchg(p, &cmp, val, FLOOR_METAL_MEM_ORDER_RELAXED, FLOOR_METAL_MEM_ORDER_RELAXED, FLOOR_METAL_SCOPE_LOCAL);
}

// min
floor_inline_always int32_t atomic_min(global int32_t* p, int32_t val) {
	return metal_atomic_min(p, val, FLOOR_METAL_MEM_ORDER_RELAXED, FLOOR_METAL_SCOPE_GLOBAL);
}
floor_inline_always uint32_t atomic_min(global uint32_t* p, uint32_t val) {
	return metal_atomic_min(p, val, FLOOR_METAL_MEM_ORDER_RELAXED, FLOOR_METAL_SCOPE_GLOBAL);
}
floor_inline_always int32_t atomic_min(local int32_t* p, int32_t val) {
	return metal_atomic_min(p, val, FLOOR_METAL_MEM_ORDER_RELAXED, FLOOR_METAL_SCOPE_LOCAL);
}
floor_inline_always uint32_t atomic_min(local uint32_t* p, uint32_t val) {
	return metal_atomic_min(p, val, FLOOR_METAL_MEM_ORDER_RELAXED, FLOOR_METAL_SCOPE_LOCAL);
}

// max
floor_inline_always int32_t atomic_max(global int32_t* p, int32_t val) {
	return metal_atomic_max(p, val, FLOOR_METAL_MEM_ORDER_RELAXED, FLOOR_METAL_SCOPE_GLOBAL);
}
floor_inline_always uint32_t atomic_max(global uint32_t* p, uint32_t val) {
	return metal_atomic_max(p, val, FLOOR_METAL_MEM_ORDER_RELAXED, FLOOR_METAL_SCOPE_GLOBAL);
}
floor_inline_always int32_t atomic_max(local int32_t* p, int32_t val) {
	return metal_atomic_max(p, val, FLOOR_METAL_MEM_ORDER_RELAXED, FLOOR_METAL_SCOPE_LOCAL);
}
floor_inline_always uint32_t atomic_max(local uint32_t* p, uint32_t val) {
	return metal_atomic_max(p, val, FLOOR_METAL_MEM_ORDER_RELAXED, FLOOR_METAL_SCOPE_LOCAL);
}

// and
floor_inline_always int32_t atomic_and(global int32_t* p, int32_t val) {
	return metal_atomic_and(p, val, FLOOR_METAL_MEM_ORDER_RELAXED, FLOOR_METAL_SCOPE_GLOBAL);
}
floor_inline_always uint32_t atomic_and(global uint32_t* p, uint32_t val) {
	return metal_atomic_and(p, val, FLOOR_METAL_MEM_ORDER_RELAXED, FLOOR_METAL_SCOPE_GLOBAL);
}
floor_inline_always int32_t atomic_and(local int32_t* p, int32_t val) {
	return metal_atomic_and(p, val, FLOOR_METAL_MEM_ORDER_RELAXED, FLOOR_METAL_SCOPE_LOCAL);
}
floor_inline_always uint32_t atomic_and(local uint32_t* p, uint32_t val) {
	return metal_atomic_and(p, val, FLOOR_METAL_MEM_ORDER_RELAXED, FLOOR_METAL_SCOPE_LOCAL);
}

// or
floor_inline_always int32_t atomic_or(global int32_t* p, int32_t val) {
	return metal_atomic_or(p, val, FLOOR_METAL_MEM_ORDER_RELAXED, FLOOR_METAL_SCOPE_GLOBAL);
}
floor_inline_always uint32_t atomic_or(global uint32_t* p, uint32_t val) {
	return metal_atomic_or(p, val, FLOOR_METAL_MEM_ORDER_RELAXED, FLOOR_METAL_SCOPE_GLOBAL);
}
floor_inline_always int32_t atomic_or(local int32_t* p, int32_t val) {
	return metal_atomic_or(p, val, FLOOR_METAL_MEM_ORDER_RELAXED, FLOOR_METAL_SCOPE_LOCAL);
}
floor_inline_always uint32_t atomic_or(local uint32_t* p, uint32_t val) {
	return metal_atomic_or(p, val, FLOOR_METAL_MEM_ORDER_RELAXED, FLOOR_METAL_SCOPE_LOCAL);
}

// xor
floor_inline_always int32_t atomic_xor(global int32_t* p, int32_t val) {
	return metal_atomic_xor(p, val, FLOOR_METAL_MEM_ORDER_RELAXED, FLOOR_METAL_SCOPE_GLOBAL);
}
floor_inline_always uint32_t atomic_xor(global uint32_t* p, uint32_t val) {
	return metal_atomic_xor(p, val, FLOOR_METAL_MEM_ORDER_RELAXED, FLOOR_METAL_SCOPE_GLOBAL);
}
floor_inline_always int32_t atomic_xor(local int32_t* p, int32_t val) {
	return metal_atomic_xor(p, val, FLOOR_METAL_MEM_ORDER_RELAXED, FLOOR_METAL_SCOPE_LOCAL);
}
floor_inline_always uint32_t atomic_xor(local uint32_t* p, uint32_t val) {
	return metal_atomic_xor(p, val, FLOOR_METAL_MEM_ORDER_RELAXED, FLOOR_METAL_SCOPE_LOCAL);
}

// store
floor_inline_always void atomic_store(global int32_t* p, int32_t val) {
	metal_atomic_store((global uint32_t*)p, val, FLOOR_METAL_MEM_ORDER_RELAXED, FLOOR_METAL_SCOPE_GLOBAL);
}
floor_inline_always void atomic_store(global uint32_t* p, uint32_t val) {
	metal_atomic_store(p, val, FLOOR_METAL_MEM_ORDER_RELAXED, FLOOR_METAL_SCOPE_GLOBAL);
}
floor_inline_always void atomic_store(local int32_t* p, int32_t val) {
	metal_atomic_store((local uint32_t*)p, val, FLOOR_METAL_MEM_ORDER_RELAXED, FLOOR_METAL_SCOPE_LOCAL);
}
floor_inline_always void atomic_store(local uint32_t* p, uint32_t val) {
	metal_atomic_store(p, val, FLOOR_METAL_MEM_ORDER_RELAXED, FLOOR_METAL_SCOPE_LOCAL);
}

// load
floor_inline_always int32_t atomic_load(global int32_t* p) {
	const uint32_t ret = metal_atomic_load((global uint32_t*)p, FLOOR_METAL_MEM_ORDER_RELAXED, FLOOR_METAL_SCOPE_GLOBAL);
	return *(int32_t*)&ret;
}
floor_inline_always uint32_t atomic_load(global uint32_t* p) {
	return metal_atomic_load(p, FLOOR_METAL_MEM_ORDER_RELAXED, FLOOR_METAL_SCOPE_GLOBAL);
}
floor_inline_always int32_t atomic_load(local int32_t* p) {
	const uint32_t ret = metal_atomic_load((local uint32_t*)p, FLOOR_METAL_MEM_ORDER_RELAXED, FLOOR_METAL_SCOPE_LOCAL);
	return *(int32_t*)&ret;
}
floor_inline_always uint32_t atomic_load(local uint32_t* p) {
	return metal_atomic_load(p, FLOOR_METAL_MEM_ORDER_RELAXED, FLOOR_METAL_SCOPE_LOCAL);
}

// fallback for non-natively supported float atomics
#define FLOOR_METAL_ATOMIC_FLOAT_OP(op, as, ptr, val) for(;;) { \
	const auto expected = *ptr; \
	const auto wanted = expected op val; \
	if(atomic_cmpxchg((as uint32_t*)ptr, *(uint32_t*)&expected, *(uint32_t*)&wanted) == expected) { \
		return expected; \
	} \
}

floor_inline_always float atomic_add(global float* p, float val) { FLOOR_METAL_ATOMIC_FLOAT_OP(+, global, p, val) }
floor_inline_always float atomic_add(local float* p, float val) { FLOOR_METAL_ATOMIC_FLOAT_OP(+, local, p, val) }
floor_inline_always float atomic_sub(global float* p, float val) { FLOOR_METAL_ATOMIC_FLOAT_OP(-, global, p, val) }
floor_inline_always float atomic_sub(local float* p, float val) { FLOOR_METAL_ATOMIC_FLOAT_OP(-, local, p, val) }
floor_inline_always float atomic_inc(global float* p) { return atomic_add(p, 1.0f); }
floor_inline_always float atomic_inc(local float* p) { return atomic_add(p, 1.0f); }
floor_inline_always float atomic_dec(global float* p) { return atomic_sub(p, 1.0f); }
floor_inline_always float atomic_dec(local float* p) { return atomic_sub(p, 1.0f); }
floor_inline_always float atomic_cmpxchg(global float* p, float cmp, float val) {
	const auto ret = atomic_cmpxchg((global uint32_t*)p, *(uint32_t*)&cmp, *(uint32_t*)&val);
	return *(float*)ret;
}
floor_inline_always float atomic_cmpxchg(local float* p, float cmp, float val) {
	const auto ret = atomic_cmpxchg((local uint32_t*)p, *(uint32_t*)&cmp, *(uint32_t*)&val);
	return *(float*)ret;
}
floor_inline_always float atomic_min(global float* p, float val) {
	if(val < 0.0f) {
		atomic_max((global uint32_t*)p, *(uint32_t*)&val);
	}
	return atomic_min((global int32_t*)p, *(int32_t*)&val);
}
floor_inline_always float atomic_min(local float* p, float val) {
	if(val < 0.0f) {
		atomic_max((local uint32_t*)p, *(uint32_t*)&val);
	}
	return atomic_min((local int32_t*)p, *(int32_t*)&val);
}
floor_inline_always float atomic_max(global float* p, float val) {
	if(val < 0.0f) {
		atomic_min((global uint32_t*)p, *(uint32_t*)&val);
	}
	return atomic_max((global int32_t*)p, *(int32_t*)&val);
}
floor_inline_always float atomic_max(local float* p, float val) {
	if(val < 0.0f) {
		atomic_min((local uint32_t*)p, *(uint32_t*)&val);
	}
	return atomic_max((local int32_t*)p, *(int32_t*)&val);
}

#undef FLOOR_METAL_ATOMIC_FLOAT_OP

#endif

#endif
