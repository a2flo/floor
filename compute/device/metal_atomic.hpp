/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2022 Florian Ziesche
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

metal_func void metal_atomic_store(volatile global uint32_t* p, uint32_t desired, uint32_t mem_order, uint32_t scope, bool is_volatile = false) asm("air.atomic.global.store.i32");
metal_func void metal_atomic_store(volatile local uint32_t* p, uint32_t desired, uint32_t mem_order, uint32_t scope, bool is_volatile = false) asm("air.atomic.local.store.i32");
#if defined(FLOOR_COMPUTE_INFO_HAS_32_BIT_FLOAT_ATOMICS_1)
metal_func void metal_atomic_store(volatile global float* p, float desired, uint32_t mem_order, uint32_t scope, bool is_volatile = false) asm("air.atomic.global.store.f32");
metal_func void metal_atomic_store(volatile local float* p, float desired, uint32_t mem_order, uint32_t scope, bool is_volatile = false) asm("air.atomic.local.store.f32");
#endif

metal_func uint32_t metal_atomic_load(const volatile global uint32_t* p, uint32_t mem_order, uint32_t scope, bool is_volatile = false) asm("air.atomic.global.load.i32");
metal_func uint32_t metal_atomic_load(const volatile local uint32_t* p, uint32_t mem_order, uint32_t scope, bool is_volatile = false) asm("air.atomic.local.load.i32");
#if defined(FLOOR_COMPUTE_INFO_HAS_32_BIT_FLOAT_ATOMICS_1)
metal_func float metal_atomic_load(const volatile global float* p, uint32_t mem_order, uint32_t scope, bool is_volatile = false) asm("air.atomic.global.load.f32");
metal_func float metal_atomic_load(const volatile local float* p, uint32_t mem_order, uint32_t scope, bool is_volatile = false) asm("air.atomic.local.load.f32");
#endif

metal_func uint32_t metal_atomic_xchg(volatile global uint32_t* p, uint32_t desired, uint32_t mem_order, uint32_t scope, bool is_volatile = false) asm("air.atomic.global.xchg.i32");
metal_func uint32_t metal_atomic_xchg(volatile local uint32_t* p, uint32_t desired, uint32_t mem_order, uint32_t scope, bool is_volatile = false) asm("air.atomic.local.xchg.i32");
#if defined(FLOOR_COMPUTE_INFO_HAS_32_BIT_FLOAT_ATOMICS_1)
metal_func float metal_atomic_xchg(volatile global float* p, float desired, uint32_t mem_order, uint32_t scope, bool is_volatile = false) asm("air.atomic.global.xchg.f32");
metal_func float metal_atomic_xchg(volatile local float* p, float desired, uint32_t mem_order, uint32_t scope, bool is_volatile = false) asm("air.atomic.local.xchg.f32");
#endif

metal_func uint32_t metal_atomic_cmpxchg(volatile global uint32_t* p, uint32_t* expected, uint32_t desired,
										 uint32_t mem_order_success, uint32_t mem_order_failure, uint32_t scope, bool is_volatile = false) asm("air.atomic.global.cmpxchg.weak.i32"); //!< weak
metal_func uint32_t metal_atomic_cmpxchg(volatile local uint32_t* p, uint32_t* expected, uint32_t desired,
										 uint32_t mem_order_success, uint32_t mem_order_failure, uint32_t scope, bool is_volatile = false) asm("air.atomic.local.cmpxchg.weak.i32"); //!< weak
#if defined(FLOOR_COMPUTE_INFO_HAS_32_BIT_FLOAT_ATOMICS_1)
metal_func float metal_atomic_cmpxchg(volatile global float* p, float* expected, float desired,
									  uint32_t mem_order_success, uint32_t mem_order_failure, uint32_t scope, bool is_volatile = false) asm("air.atomic.global.cmpxchg.weak.f32"); //!< weak
metal_func float metal_atomic_cmpxchg(volatile local float* p, float* expected, float desired,
									  uint32_t mem_order_success, uint32_t mem_order_failure, uint32_t scope, bool is_volatile = false) asm("air.atomic.local.cmpxchg.weak.f32"); //!< weak
#endif

metal_func uint32_t metal_atomic_add(volatile global uint32_t* p, uint32_t val, uint32_t mem_order, uint32_t scope, bool is_volatile = false) asm("air.atomic.global.add.u.i32");
metal_func int32_t metal_atomic_add(volatile global int32_t* p, int32_t val, uint32_t mem_order, uint32_t scope, bool is_volatile = false) asm("air.atomic.global.add.s.i32");
metal_func uint32_t metal_atomic_add(volatile local uint32_t* p, uint32_t val, uint32_t mem_order, uint32_t scope, bool is_volatile = false) asm("air.atomic.local.add.u.i32");
metal_func int32_t metal_atomic_add(volatile local int32_t* p, int32_t val, uint32_t mem_order, uint32_t scope, bool is_volatile = false) asm("air.atomic.local.add.s.i32");

#if defined(FLOOR_COMPUTE_INFO_HAS_32_BIT_FLOAT_ATOMICS_1)
metal_func uint32_t metal_atomic_add(volatile global float* p, float val, uint32_t mem_order, uint32_t scope, bool is_volatile = false) asm("air.atomic.global.add.f32");
metal_func uint32_t metal_atomic_add(volatile local float* p, float val, uint32_t mem_order, uint32_t scope, bool is_volatile = false) asm("air.atomic.local.add.f32");
#endif

metal_func uint32_t metal_atomic_sub(volatile global uint32_t* p, uint32_t val, uint32_t mem_order, uint32_t scope, bool is_volatile = false) asm("air.atomic.global.sub.u.i32");
metal_func int32_t metal_atomic_sub(volatile global int32_t* p, int32_t val, uint32_t mem_order, uint32_t scope, bool is_volatile = false) asm("air.atomic.global.sub.s.i32");
metal_func uint32_t metal_atomic_sub(volatile local uint32_t* p, uint32_t val, uint32_t mem_order, uint32_t scope, bool is_volatile = false) asm("air.atomic.local.sub.u.i32");
metal_func int32_t metal_atomic_sub(volatile local int32_t* p, int32_t val, uint32_t mem_order, uint32_t scope, bool is_volatile = false) asm("air.atomic.local.sub.s.i32");

#if defined(FLOOR_COMPUTE_INFO_HAS_32_BIT_FLOAT_ATOMICS_1)
metal_func uint32_t metal_atomic_sub(volatile global float* p, float val, uint32_t mem_order, uint32_t scope, bool is_volatile = false) asm("air.atomic.global.sub.f32");
metal_func uint32_t metal_atomic_sub(volatile local float* p, float val, uint32_t mem_order, uint32_t scope, bool is_volatile = false) asm("air.atomic.local.sub.f32");
#endif

#if !defined(FLOOR_COMPUTE_INFO_VENDOR_INTEL)
metal_func uint32_t metal_atomic_min(volatile global uint32_t* p, uint32_t val, uint32_t mem_order, uint32_t scope, bool is_volatile = false) asm("air.atomic.global.min.u.i32");
metal_func int32_t metal_atomic_min(volatile global int32_t* p, int32_t val, uint32_t mem_order, uint32_t scope, bool is_volatile = false) asm("air.atomic.global.min.s.i32");
metal_func uint32_t metal_atomic_min(volatile local uint32_t* p, uint32_t val, uint32_t mem_order, uint32_t scope, bool is_volatile = false) asm("air.atomic.local.min.u.i32");
metal_func int32_t metal_atomic_min(volatile local int32_t* p, int32_t val, uint32_t mem_order, uint32_t scope, bool is_volatile = false) asm("air.atomic.local.min.s.i32");

metal_func uint32_t metal_atomic_max(volatile global uint32_t* p, uint32_t val, uint32_t mem_order, uint32_t scope, bool is_volatile = false) asm("air.atomic.global.max.u.i32");
metal_func int32_t metal_atomic_max(volatile global int32_t* p, int32_t val, uint32_t mem_order, uint32_t scope, bool is_volatile = false) asm("air.atomic.global.max.s.i32");
metal_func uint32_t metal_atomic_max(volatile local uint32_t* p, uint32_t val, uint32_t mem_order, uint32_t scope, bool is_volatile = false) asm("air.atomic.local.max.u.i32");
metal_func int32_t metal_atomic_max(volatile local int32_t* p, int32_t val, uint32_t mem_order, uint32_t scope, bool is_volatile = false) asm("air.atomic.local.max.s.i32");
#else
// workaround for broken atomic_min/atomic_max on intel gpus (backend doesn't differentiate between signed and unsigned)
metal_func uint32_t igil_atomic_min(int32_t zero, volatile global uint32_t* p, uint32_t val) asm("llvm.igil.atom.minu32.global");
metal_func int32_t igil_atomic_min(int32_t zero, volatile global int32_t* p, int32_t val) asm("llvm.igil.atom.mini32.global");
metal_func uint32_t igil_atomic_min(int32_t zero, volatile local uint32_t* p, uint32_t val) asm("llvm.igil.atom.minu32.local");
metal_func int32_t igil_atomic_min(int32_t zero, volatile local int32_t* p, int32_t val) asm("llvm.igil.atom.mini32.local");

metal_func uint32_t igil_atomic_max(int32_t zero, volatile global uint32_t* p, uint32_t val) asm("llvm.igil.atom.maxu32.global");
metal_func int32_t igil_atomic_max(int32_t zero, volatile global int32_t* p, int32_t val) asm("llvm.igil.atom.maxi32.global");
metal_func uint32_t igil_atomic_max(int32_t zero, volatile local uint32_t* p, uint32_t val) asm("llvm.igil.atom.maxu32.local");
metal_func int32_t igil_atomic_max(int32_t zero, volatile local int32_t* p, int32_t val) asm("llvm.igil.atom.maxi32.local");

floor_inline_always metal_func uint32_t metal_atomic_min(volatile global uint32_t* p, uint32_t val, uint32_t, uint32_t) {
	return igil_atomic_min(0, p, val);
}
floor_inline_always metal_func int32_t metal_atomic_min(volatile global int32_t* p, int32_t val, uint32_t, uint32_t) {
	return igil_atomic_min(0, p, val);
}
floor_inline_always metal_func uint32_t metal_atomic_min(volatile local uint32_t* p, uint32_t val, uint32_t, uint32_t) {
	return igil_atomic_min(0, p, val);
}
floor_inline_always metal_func int32_t metal_atomic_min(volatile local int32_t* p, int32_t val, uint32_t, uint32_t) {
	return igil_atomic_min(0, p, val);
}

floor_inline_always metal_func uint32_t metal_atomic_max(volatile global uint32_t* p, uint32_t val, uint32_t, uint32_t) {
	return igil_atomic_max(0, p, val);
}
floor_inline_always metal_func int32_t metal_atomic_max(volatile global int32_t* p, int32_t val, uint32_t, uint32_t) {
	return igil_atomic_max(0, p, val);
}
floor_inline_always metal_func uint32_t metal_atomic_max(volatile local uint32_t* p, uint32_t val, uint32_t, uint32_t) {
	return igil_atomic_max(0, p, val);
}
floor_inline_always metal_func int32_t metal_atomic_max(volatile local int32_t* p, int32_t val, uint32_t, uint32_t) {
	return igil_atomic_max(0, p, val);
}
#endif

metal_func uint32_t metal_atomic_and(volatile global uint32_t* p, uint32_t val, uint32_t mem_order, uint32_t scope, bool is_volatile = false) asm("air.atomic.global.and.u.i32");
metal_func int32_t metal_atomic_and(volatile global int32_t* p, int32_t val, uint32_t mem_order, uint32_t scope, bool is_volatile = false) asm("air.atomic.global.and.s.i32");
metal_func uint32_t metal_atomic_and(volatile local uint32_t* p, uint32_t val, uint32_t mem_order, uint32_t scope, bool is_volatile = false) asm("air.atomic.local.and.u.i32");
metal_func int32_t metal_atomic_and(volatile local int32_t* p, int32_t val, uint32_t mem_order, uint32_t scope, bool is_volatile = false) asm("air.atomic.local.and.s.i32");

metal_func uint32_t metal_atomic_or(volatile global uint32_t* p, uint32_t val, uint32_t mem_order, uint32_t scope, bool is_volatile = false) asm("air.atomic.global.or.u.i32");
metal_func int32_t metal_atomic_or(volatile global int32_t* p, int32_t val, uint32_t mem_order, uint32_t scope, bool is_volatile = false) asm("air.atomic.global.or.s.i32");
metal_func uint32_t metal_atomic_or(volatile local uint32_t* p, uint32_t val, uint32_t mem_order, uint32_t scope, bool is_volatile = false) asm("air.atomic.local.or.u.i32");
metal_func int32_t metal_atomic_or(volatile local int32_t* p, int32_t val, uint32_t mem_order, uint32_t scope, bool is_volatile = false) asm("air.atomic.local.or.s.i32");

metal_func uint32_t metal_atomic_xor(volatile global uint32_t* p, uint32_t val, uint32_t mem_order, uint32_t scope, bool is_volatile = false) asm("air.atomic.global.xor.u.i32");
metal_func int32_t metal_atomic_xor(volatile global int32_t* p, int32_t val, uint32_t mem_order, uint32_t scope, bool is_volatile = false) asm("air.atomic.global.xor.s.i32");
metal_func uint32_t metal_atomic_xor(volatile local uint32_t* p, uint32_t val, uint32_t mem_order, uint32_t scope, bool is_volatile = false) asm("air.atomic.local.xor.u.i32");
metal_func int32_t metal_atomic_xor(volatile local int32_t* p, int32_t val, uint32_t mem_order, uint32_t scope, bool is_volatile = false) asm("air.atomic.local.xor.s.i32");

// add
floor_inline_always int32_t atomic_add(volatile global int32_t* p, int32_t val) {
	return metal_atomic_add(p, val, FLOOR_METAL_MEM_ORDER_RELAXED, FLOOR_METAL_SYNC_SCOPE_GLOBAL);
}
floor_inline_always uint32_t atomic_add(volatile global uint32_t* p, uint32_t val) {
	return metal_atomic_add(p, val, FLOOR_METAL_MEM_ORDER_RELAXED, FLOOR_METAL_SYNC_SCOPE_GLOBAL);
}
floor_inline_always int32_t atomic_add(volatile local int32_t* p, int32_t val) {
	return metal_atomic_add(p, val, FLOOR_METAL_MEM_ORDER_RELAXED, FLOOR_METAL_SYNC_SCOPE_LOCAL);
}
floor_inline_always uint32_t atomic_add(volatile local uint32_t* p, uint32_t val) {
	return metal_atomic_add(p, val, FLOOR_METAL_MEM_ORDER_RELAXED, FLOOR_METAL_SYNC_SCOPE_LOCAL);
}
#if defined(FLOOR_COMPUTE_INFO_HAS_32_BIT_FLOAT_ATOMICS_1)
floor_inline_always float atomic_add(volatile global float* p, float val) {
	return metal_atomic_add(p, val, FLOOR_METAL_MEM_ORDER_RELAXED, FLOOR_METAL_SYNC_SCOPE_GLOBAL);
}
floor_inline_always float atomic_add(volatile local float* p, float val) {
	return metal_atomic_add(p, val, FLOOR_METAL_MEM_ORDER_RELAXED, FLOOR_METAL_SYNC_SCOPE_LOCAL);
}
#endif

// sub
floor_inline_always int32_t atomic_sub(volatile global int32_t* p, int32_t val) {
	return metal_atomic_sub(p, val, FLOOR_METAL_MEM_ORDER_RELAXED, FLOOR_METAL_SYNC_SCOPE_GLOBAL);
}
floor_inline_always uint32_t atomic_sub(volatile global uint32_t* p, uint32_t val) {
	return metal_atomic_sub(p, val, FLOOR_METAL_MEM_ORDER_RELAXED, FLOOR_METAL_SYNC_SCOPE_GLOBAL);
}
floor_inline_always int32_t atomic_sub(volatile local int32_t* p, int32_t val) {
	return metal_atomic_sub(p, val, FLOOR_METAL_MEM_ORDER_RELAXED, FLOOR_METAL_SYNC_SCOPE_LOCAL);
}
floor_inline_always uint32_t atomic_sub(volatile local uint32_t* p, uint32_t val) {
	return metal_atomic_sub(p, val, FLOOR_METAL_MEM_ORDER_RELAXED, FLOOR_METAL_SYNC_SCOPE_LOCAL);
}
#if defined(FLOOR_COMPUTE_INFO_HAS_32_BIT_FLOAT_ATOMICS_1)
floor_inline_always float atomic_sub(volatile global float* p, float val) {
	return metal_atomic_sub(p, val, FLOOR_METAL_MEM_ORDER_RELAXED, FLOOR_METAL_SYNC_SCOPE_GLOBAL);
}
floor_inline_always float atomic_sub(volatile local float* p, float val) {
	return metal_atomic_sub(p, val, FLOOR_METAL_MEM_ORDER_RELAXED, FLOOR_METAL_SYNC_SCOPE_LOCAL);
}
#endif

// inc
floor_inline_always int32_t atomic_inc(volatile global int32_t* p) {
	return atomic_add(p, 1);
}
floor_inline_always uint32_t atomic_inc(volatile global uint32_t* p) {
	return atomic_add(p, 1u);
}
floor_inline_always int32_t atomic_inc(volatile local int32_t* p) {
	return atomic_add(p, 1);
}
floor_inline_always uint32_t atomic_inc(volatile local uint32_t* p) {
	return atomic_add(p, 1u);
}
#if defined(FLOOR_COMPUTE_INFO_HAS_32_BIT_FLOAT_ATOMICS_1)
floor_inline_always float atomic_inc(volatile global float* p) {
	return atomic_add(p, 1.0f);
}
floor_inline_always float atomic_inc(volatile local float* p) {
	return atomic_add(p, 1.0f);
}
#endif

// dec
floor_inline_always int32_t atomic_dec(volatile global int32_t* p) {
	return atomic_sub(p, 1);
}
floor_inline_always uint32_t atomic_dec(volatile global uint32_t* p) {
	return atomic_sub(p, 1u);
}
floor_inline_always int32_t atomic_dec(volatile local int32_t* p) {
	return atomic_sub(p, 1);
}
floor_inline_always uint32_t atomic_dec(volatile local uint32_t* p) {
	return atomic_sub(p, 1u);
}
#if defined(FLOOR_COMPUTE_INFO_HAS_32_BIT_FLOAT_ATOMICS_1)
floor_inline_always float atomic_dec(volatile global float* p) {
	return atomic_sub(p, 1.0f);
}
floor_inline_always float atomic_dec(volatile local float* p) {
	return atomic_sub(p, 1.0f);
}
#endif

// xchg
floor_inline_always int32_t atomic_xchg(volatile global int32_t* p, int32_t val) {
	const uint32_t ret = metal_atomic_xchg((volatile global uint32_t*)p, *(uint32_t*)&val, FLOOR_METAL_MEM_ORDER_RELAXED, FLOOR_METAL_SYNC_SCOPE_GLOBAL);
	return *(int32_t*)&ret;
}
floor_inline_always uint32_t atomic_xchg(volatile global uint32_t* p, uint32_t val) {
	return metal_atomic_xchg(p, val, FLOOR_METAL_MEM_ORDER_RELAXED, FLOOR_METAL_SYNC_SCOPE_GLOBAL);
}
floor_inline_always int32_t atomic_xchg(volatile local int32_t* p, int32_t val) {
	const uint32_t ret = metal_atomic_xchg((volatile local uint32_t*)p, *(uint32_t*)&val, FLOOR_METAL_MEM_ORDER_RELAXED, FLOOR_METAL_SYNC_SCOPE_LOCAL);
	return *(int32_t*)&ret;
}
floor_inline_always uint32_t atomic_xchg(volatile local uint32_t* p, uint32_t val) {
	return metal_atomic_xchg(p, val, FLOOR_METAL_MEM_ORDER_RELAXED, FLOOR_METAL_SYNC_SCOPE_LOCAL);
}
#if defined(FLOOR_COMPUTE_INFO_HAS_32_BIT_FLOAT_ATOMICS_1)
floor_inline_always float atomic_xchg(volatile global float* p, float val) {
	return metal_atomic_xchg(p, val, FLOOR_METAL_MEM_ORDER_RELAXED, FLOOR_METAL_SYNC_SCOPE_GLOBAL);
}
floor_inline_always float atomic_xchg(volatile local float* p, float val) {
	return metal_atomic_xchg(p, val, FLOOR_METAL_MEM_ORDER_RELAXED, FLOOR_METAL_SYNC_SCOPE_LOCAL);
}
#else
floor_inline_always float atomic_xchg(volatile global float* p, float val) {
	const uint32_t ret = metal_atomic_xchg((volatile global uint32_t*)p, *(uint32_t*)&val, FLOOR_METAL_MEM_ORDER_RELAXED, FLOOR_METAL_SYNC_SCOPE_GLOBAL);
	return *(float*)&ret;
}
floor_inline_always float atomic_xchg(volatile local float* p, float val) {
	const uint32_t ret = metal_atomic_xchg((volatile local uint32_t*)p, *(uint32_t*)&val, FLOOR_METAL_MEM_ORDER_RELAXED, FLOOR_METAL_SYNC_SCOPE_LOCAL);
	return *(float*)&ret;
}
#endif

// cmpxchg
floor_inline_always int32_t atomic_cmpxchg(volatile global int32_t* p, int32_t cmp, int32_t val) {
	const uint32_t ret = metal_atomic_cmpxchg((volatile global uint32_t*)p, (uint32_t*)&cmp, *(uint32_t*)&val, FLOOR_METAL_MEM_ORDER_RELAXED, FLOOR_METAL_MEM_ORDER_RELAXED, FLOOR_METAL_SYNC_SCOPE_GLOBAL);
	return *(int32_t*)&ret;
}
floor_inline_always uint32_t atomic_cmpxchg(volatile global uint32_t* p, uint32_t cmp, uint32_t val) {
	return metal_atomic_cmpxchg(p, &cmp, val, FLOOR_METAL_MEM_ORDER_RELAXED, FLOOR_METAL_MEM_ORDER_RELAXED, FLOOR_METAL_SYNC_SCOPE_GLOBAL);
}
floor_inline_always int32_t atomic_cmpxchg(volatile local int32_t* p, int32_t cmp, int32_t val) {
	const uint32_t ret = metal_atomic_cmpxchg((volatile local uint32_t*)p, (uint32_t*)&cmp, *(uint32_t*)&val, FLOOR_METAL_MEM_ORDER_RELAXED, FLOOR_METAL_MEM_ORDER_RELAXED, FLOOR_METAL_SYNC_SCOPE_LOCAL);
	return *(int32_t*)&ret;
}
floor_inline_always uint32_t atomic_cmpxchg(volatile local uint32_t* p, uint32_t cmp, uint32_t val) {
	return metal_atomic_cmpxchg(p, &cmp, val, FLOOR_METAL_MEM_ORDER_RELAXED, FLOOR_METAL_MEM_ORDER_RELAXED, FLOOR_METAL_SYNC_SCOPE_LOCAL);
}
#if defined(FLOOR_COMPUTE_INFO_HAS_32_BIT_FLOAT_ATOMICS_1)
floor_inline_always float atomic_cmpxchg(volatile global float* p, float cmp, float val) {
	return metal_atomic_cmpxchg(p, &cmp, val, FLOOR_METAL_MEM_ORDER_RELAXED, FLOOR_METAL_MEM_ORDER_RELAXED, FLOOR_METAL_SYNC_SCOPE_GLOBAL);
}
floor_inline_always float atomic_cmpxchg(volatile local float* p, float cmp, float val) {
	return metal_atomic_cmpxchg(p, &cmp, val, FLOOR_METAL_MEM_ORDER_RELAXED, FLOOR_METAL_MEM_ORDER_RELAXED, FLOOR_METAL_SYNC_SCOPE_LOCAL);
}
#endif

// min
floor_inline_always int32_t atomic_min(volatile global int32_t* p, int32_t val) {
	return metal_atomic_min(p, val, FLOOR_METAL_MEM_ORDER_RELAXED, FLOOR_METAL_SYNC_SCOPE_GLOBAL);
}
floor_inline_always uint32_t atomic_min(volatile global uint32_t* p, uint32_t val) {
	return metal_atomic_min(p, val, FLOOR_METAL_MEM_ORDER_RELAXED, FLOOR_METAL_SYNC_SCOPE_GLOBAL);
}
floor_inline_always int32_t atomic_min(volatile local int32_t* p, int32_t val) {
	return metal_atomic_min(p, val, FLOOR_METAL_MEM_ORDER_RELAXED, FLOOR_METAL_SYNC_SCOPE_LOCAL);
}
floor_inline_always uint32_t atomic_min(volatile local uint32_t* p, uint32_t val) {
	return metal_atomic_min(p, val, FLOOR_METAL_MEM_ORDER_RELAXED, FLOOR_METAL_SYNC_SCOPE_LOCAL);
}

// max
floor_inline_always int32_t atomic_max(volatile global int32_t* p, int32_t val) {
	return metal_atomic_max(p, val, FLOOR_METAL_MEM_ORDER_RELAXED, FLOOR_METAL_SYNC_SCOPE_GLOBAL);
}
floor_inline_always uint32_t atomic_max(volatile global uint32_t* p, uint32_t val) {
	return metal_atomic_max(p, val, FLOOR_METAL_MEM_ORDER_RELAXED, FLOOR_METAL_SYNC_SCOPE_GLOBAL);
}
floor_inline_always int32_t atomic_max(volatile local int32_t* p, int32_t val) {
	return metal_atomic_max(p, val, FLOOR_METAL_MEM_ORDER_RELAXED, FLOOR_METAL_SYNC_SCOPE_LOCAL);
}
floor_inline_always uint32_t atomic_max(volatile local uint32_t* p, uint32_t val) {
	return metal_atomic_max(p, val, FLOOR_METAL_MEM_ORDER_RELAXED, FLOOR_METAL_SYNC_SCOPE_LOCAL);
}

// and
floor_inline_always int32_t atomic_and(volatile global int32_t* p, int32_t val) {
	return metal_atomic_and(p, val, FLOOR_METAL_MEM_ORDER_RELAXED, FLOOR_METAL_SYNC_SCOPE_GLOBAL);
}
floor_inline_always uint32_t atomic_and(volatile global uint32_t* p, uint32_t val) {
	return metal_atomic_and(p, val, FLOOR_METAL_MEM_ORDER_RELAXED, FLOOR_METAL_SYNC_SCOPE_GLOBAL);
}
floor_inline_always int32_t atomic_and(volatile local int32_t* p, int32_t val) {
	return metal_atomic_and(p, val, FLOOR_METAL_MEM_ORDER_RELAXED, FLOOR_METAL_SYNC_SCOPE_LOCAL);
}
floor_inline_always uint32_t atomic_and(volatile local uint32_t* p, uint32_t val) {
	return metal_atomic_and(p, val, FLOOR_METAL_MEM_ORDER_RELAXED, FLOOR_METAL_SYNC_SCOPE_LOCAL);
}

// or
floor_inline_always int32_t atomic_or(volatile global int32_t* p, int32_t val) {
	return metal_atomic_or(p, val, FLOOR_METAL_MEM_ORDER_RELAXED, FLOOR_METAL_SYNC_SCOPE_GLOBAL);
}
floor_inline_always uint32_t atomic_or(volatile global uint32_t* p, uint32_t val) {
	return metal_atomic_or(p, val, FLOOR_METAL_MEM_ORDER_RELAXED, FLOOR_METAL_SYNC_SCOPE_GLOBAL);
}
floor_inline_always int32_t atomic_or(volatile local int32_t* p, int32_t val) {
	return metal_atomic_or(p, val, FLOOR_METAL_MEM_ORDER_RELAXED, FLOOR_METAL_SYNC_SCOPE_LOCAL);
}
floor_inline_always uint32_t atomic_or(volatile local uint32_t* p, uint32_t val) {
	return metal_atomic_or(p, val, FLOOR_METAL_MEM_ORDER_RELAXED, FLOOR_METAL_SYNC_SCOPE_LOCAL);
}

// xor
floor_inline_always int32_t atomic_xor(volatile global int32_t* p, int32_t val) {
	return metal_atomic_xor(p, val, FLOOR_METAL_MEM_ORDER_RELAXED, FLOOR_METAL_SYNC_SCOPE_GLOBAL);
}
floor_inline_always uint32_t atomic_xor(volatile global uint32_t* p, uint32_t val) {
	return metal_atomic_xor(p, val, FLOOR_METAL_MEM_ORDER_RELAXED, FLOOR_METAL_SYNC_SCOPE_GLOBAL);
}
floor_inline_always int32_t atomic_xor(volatile local int32_t* p, int32_t val) {
	return metal_atomic_xor(p, val, FLOOR_METAL_MEM_ORDER_RELAXED, FLOOR_METAL_SYNC_SCOPE_LOCAL);
}
floor_inline_always uint32_t atomic_xor(volatile local uint32_t* p, uint32_t val) {
	return metal_atomic_xor(p, val, FLOOR_METAL_MEM_ORDER_RELAXED, FLOOR_METAL_SYNC_SCOPE_LOCAL);
}

// store
floor_inline_always void atomic_store(volatile global int32_t* p, int32_t val) {
	metal_atomic_store((volatile global uint32_t*)p, *(uint32_t*)&val, FLOOR_METAL_MEM_ORDER_RELAXED, FLOOR_METAL_SYNC_SCOPE_GLOBAL);
}
floor_inline_always void atomic_store(volatile global uint32_t* p, uint32_t val) {
	metal_atomic_store(p, val, FLOOR_METAL_MEM_ORDER_RELAXED, FLOOR_METAL_SYNC_SCOPE_GLOBAL);
}
floor_inline_always void atomic_store(volatile local int32_t* p, int32_t val) {
	metal_atomic_store((volatile local uint32_t*)p, *(uint32_t*)&val, FLOOR_METAL_MEM_ORDER_RELAXED, FLOOR_METAL_SYNC_SCOPE_LOCAL);
}
floor_inline_always void atomic_store(volatile local uint32_t* p, uint32_t val) {
	metal_atomic_store(p, val, FLOOR_METAL_MEM_ORDER_RELAXED, FLOOR_METAL_SYNC_SCOPE_LOCAL);
}
#if defined(FLOOR_COMPUTE_INFO_HAS_32_BIT_FLOAT_ATOMICS_1)
floor_inline_always void atomic_store(volatile global float* p, float val) {
	metal_atomic_store(p, val, FLOOR_METAL_MEM_ORDER_RELAXED, FLOOR_METAL_SYNC_SCOPE_GLOBAL);
}
floor_inline_always void atomic_store(volatile local float* p, float val) {
	metal_atomic_store(p, val, FLOOR_METAL_MEM_ORDER_RELAXED, FLOOR_METAL_SYNC_SCOPE_LOCAL);
}
#else
floor_inline_always void atomic_store(volatile global float* p, float val) {
	metal_atomic_store((volatile global uint32_t*)p, *(uint32_t*)&val, FLOOR_METAL_MEM_ORDER_RELAXED, FLOOR_METAL_SYNC_SCOPE_GLOBAL);
}
floor_inline_always void atomic_store(volatile local float* p, float val) {
	metal_atomic_store((volatile local uint32_t*)p, *(uint32_t*)&val, FLOOR_METAL_MEM_ORDER_RELAXED, FLOOR_METAL_SYNC_SCOPE_LOCAL);
}
#endif

// load
floor_inline_always int32_t atomic_load(const volatile global int32_t* p) {
	const uint32_t ret = metal_atomic_load((const volatile global uint32_t*)p, FLOOR_METAL_MEM_ORDER_RELAXED, FLOOR_METAL_SYNC_SCOPE_GLOBAL);
	return *(int32_t*)&ret;
}
floor_inline_always uint32_t atomic_load(const volatile global uint32_t* p) {
	return metal_atomic_load(p, FLOOR_METAL_MEM_ORDER_RELAXED, FLOOR_METAL_SYNC_SCOPE_GLOBAL);
}
floor_inline_always int32_t atomic_load(const volatile local int32_t* p) {
	const uint32_t ret = metal_atomic_load((const volatile local uint32_t*)p, FLOOR_METAL_MEM_ORDER_RELAXED, FLOOR_METAL_SYNC_SCOPE_LOCAL);
	return *(int32_t*)&ret;
}
floor_inline_always uint32_t atomic_load(const volatile local uint32_t* p) {
	return metal_atomic_load(p, FLOOR_METAL_MEM_ORDER_RELAXED, FLOOR_METAL_SYNC_SCOPE_LOCAL);
}
#if defined(FLOOR_COMPUTE_INFO_HAS_32_BIT_FLOAT_ATOMICS_1)
floor_inline_always float atomic_load(const volatile global float* p) {
	return metal_atomic_load(p, FLOOR_METAL_MEM_ORDER_RELAXED, FLOOR_METAL_SYNC_SCOPE_GLOBAL);
}
floor_inline_always float atomic_load(const volatile local float* p) {
	return metal_atomic_load(p, FLOOR_METAL_MEM_ORDER_RELAXED, FLOOR_METAL_SYNC_SCOPE_LOCAL);
}
#else
floor_inline_always float atomic_load(const volatile global float* p) {
	const uint32_t ret = metal_atomic_load((const volatile global uint32_t*)p, FLOOR_METAL_MEM_ORDER_RELAXED, FLOOR_METAL_SYNC_SCOPE_GLOBAL);
	return *(float*)&ret;
}
floor_inline_always float atomic_load(const volatile local float* p) {
	const uint32_t ret = metal_atomic_load((const volatile local uint32_t*)p, FLOOR_METAL_MEM_ORDER_RELAXED, FLOOR_METAL_SYNC_SCOPE_LOCAL);
	return *(float*)&ret;
}
#endif

// fallback for non-natively supported float atomics
#if !defined(FLOOR_COMPUTE_INFO_HAS_32_BIT_FLOAT_ATOMICS_1)
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
#endif
floor_inline_always float atomic_min(volatile global float* p, float val) {
	if(val < 0.0f) {
		return atomic_max((volatile global uint32_t*)p, *(uint32_t*)&val);
	}
	return atomic_min((volatile global int32_t*)p, *(int32_t*)&val);
}
floor_inline_always float atomic_min(volatile local float* p, float val) {
	if(val < 0.0f) {
		return atomic_max((volatile local uint32_t*)p, *(uint32_t*)&val);
	}
	return atomic_min((volatile local int32_t*)p, *(int32_t*)&val);
}
floor_inline_always float atomic_max(volatile global float* p, float val) {
	if(val < 0.0f) {
		return atomic_min((volatile global uint32_t*)p, *(uint32_t*)&val);
	}
	return atomic_max((volatile global int32_t*)p, *(int32_t*)&val);
}
floor_inline_always float atomic_max(volatile local float* p, float val) {
	if(val < 0.0f) {
		return atomic_min((volatile local uint32_t*)p, *(uint32_t*)&val);
	}
	return atomic_max((volatile local int32_t*)p, *(int32_t*)&val);
}

#endif

#endif
