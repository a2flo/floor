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

#if defined(FLOOR_COMPUTE_OPENCL)

// wrap opencl id handling functions so that uint32_t is always returned
FLOOR_GLOBAL_ID_RANGE_ATTR const_func size_t cl_get_global_id(uint32_t dim) asm("_Z13get_global_idj");
FLOOR_GLOBAL_SIZE_RANGE_ATTR const_func size_t cl_get_global_size(uint32_t dim) asm("_Z15get_global_sizej");
FLOOR_LOCAL_ID_RANGE_ATTR const_func size_t cl_get_local_id(uint32_t dim) asm("_Z12get_local_idj");
FLOOR_LOCAL_SIZE_RANGE_ATTR const_func size_t cl_get_local_size(uint32_t dim) asm("_Z14get_local_sizej");
FLOOR_GROUP_ID_RANGE_ATTR const_func size_t cl_get_group_id(uint32_t dim) asm("_Z12get_group_idj");
FLOOR_GROUP_SIZE_RANGE_ATTR const_func size_t cl_get_group_size(uint32_t dim) asm("_Z14get_num_groupsj");
[[range(1u, 3u)]] const_func uint32_t get_work_dim();

#define get_global_id(x) uint32_t(cl_get_global_id(x))
#define get_global_size(x) uint32_t(cl_get_global_size(x))
#define get_local_id(x) uint32_t(cl_get_local_id(x))
#define get_local_size(x) uint32_t(cl_get_local_size(x))
#define get_group_id(x) uint32_t(cl_get_group_id(x))
#define get_group_size(x) uint32_t(cl_get_group_size(x))

#include <floor/compute/device/opencl_common.hpp>

// non-standard bit counting functions (don't use these directly, use math::func instead)
const_func uint16_t floor_rt_clz(uint16_t x) asm("_Z3clzt");
const_func uint32_t floor_rt_clz(uint32_t x) asm("_Z3clzj");
const_func uint64_t floor_rt_clz(uint64_t x) asm("_Z3clzm");
const_func uint16_t floor_rt_popcount(uint16_t x) asm("_Z8popcountt");
const_func uint32_t floor_rt_popcount(uint32_t x) asm("_Z8popcountj");
const_func uint64_t floor_rt_popcount(uint64_t x) asm("_Z8popcountm");
// ctz was only added in opencl c 2.0, but also exists for spir-v (must also be supported by 1.2 devices)
#if defined(FLOOR_COMPUTE_SPIRV) || (FLOOR_COMPUTE_OPENCL_MAJOR >= 2)
const_func uint16_t floor_rt_ctz(uint16_t x) asm("_Z3ctzt");
const_func uint32_t floor_rt_ctz(uint32_t x) asm("_Z3ctzj");
const_func uint64_t floor_rt_ctz(uint64_t x) asm("_Z3ctzm");
#else
static floor_inline_always const_func uint32_t floor_rt_ctz(uint16_t x) {
	// ref: https://graphics.stanford.edu/~seander/bithacks.html#ZerosOnRightFloatCast
	const uint32_t widened_val = uint32_t(x) | 0xFFFF0000u;
	const auto f = float(widened_val & -widened_val);
	return (*(uint32_t*)&f >> 23u) - 0x7Fu; // widened_val is never 0
}
static floor_inline_always const_func uint32_t floor_rt_ctz(uint32_t x) {
	// ref: https://graphics.stanford.edu/~seander/bithacks.html#ZerosOnRightFloatCast
	const auto f = float(x & -x);
	return (x != 0 ? (*(uint32_t*)&f >> 23u) - 0x7Fu : 32);
}
static floor_inline_always const_func uint32_t floor_rt_ctz(uint64_t x) {
	const auto upper = uint32_t(x >> 32ull);
	const auto lower = uint32_t(x & 0xFFFFFFFFull);
	const auto ctz_lower = floor_rt_ctz(lower);
	const auto ctz_upper = floor_rt_ctz(upper);
	return (ctz_lower < 32 ? ctz_lower : (ctz_upper + ctz_lower));
}
#endif

// can't normally produce _Z6printfPrU3AS2cz with clang/llvm, because a proper "restrict" keyword is missing in c++ mode
// -> slay it with an asm label
int printf(const char constant* st, ...) asm("_Z6printfPrU3AS2cz");

// barrier and mem_fence functionality
// NOTE: local = 1, global = 2, image = 4
void cl_barrier(uint32_t flags) __attribute__((noduplicate, convergent)) asm("_Z7barrierj");
void cl_mem_fence(uint32_t flags) __attribute__((noduplicate, convergent)) asm("_Z9mem_fencej");
void cl_read_mem_fence(uint32_t flags) __attribute__((noduplicate, convergent)) asm("_Z14read_mem_fencej");
void cl_write_mem_fence(uint32_t flags) __attribute__((noduplicate, convergent)) asm("_Z14read_mem_fencej");

floor_inline_always static void global_barrier() __attribute__((noduplicate, convergent)) {
	cl_barrier(2u);
}
floor_inline_always static void global_mem_fence() __attribute__((noduplicate, convergent)) {
	cl_mem_fence(2u);
}
floor_inline_always static void global_read_mem_fence() __attribute__((noduplicate, convergent)) {
	cl_read_mem_fence(2u);
}
floor_inline_always static void global_write_mem_fence() __attribute__((noduplicate, convergent)) {
	cl_write_mem_fence(2u);
}

floor_inline_always static void local_barrier() __attribute__((noduplicate, convergent)) {
	cl_barrier(1u);
}
floor_inline_always static void local_mem_fence() __attribute__((noduplicate, convergent)) {
	cl_mem_fence(1u);
}
floor_inline_always static void local_read_mem_fence() __attribute__((noduplicate, convergent)) {
	cl_read_mem_fence(1u);
}
floor_inline_always static void local_write_mem_fence() __attribute__((noduplicate, convergent)) {
	cl_write_mem_fence(1u);
}

floor_inline_always static void barrier() __attribute__((noduplicate, convergent)) {
	cl_barrier(3u);
}

//! NOTE: not guaranteed to be available everywhere
floor_inline_always static void image_barrier() __attribute__((noduplicate, convergent)) {
	cl_barrier(4u);
}
//! NOTE: not guaranteed to be available everywhere
floor_inline_always static void image_mem_fence() __attribute__((noduplicate, convergent)) {
	cl_mem_fence(4u);
}
//! NOTE: not guaranteed to be available everywhere
floor_inline_always static void image_read_mem_fence() __attribute__((noduplicate, convergent)) {
	cl_read_mem_fence(4u);
}
//! NOTE: not guaranteed to be available everywhere
floor_inline_always static void image_write_mem_fence() __attribute__((noduplicate, convergent)) {
	cl_write_mem_fence(4u);
}

// sub-group functionality (opencl 2.1+, cl_khr_subgroups, cl_intel_subgroups)
#if FLOOR_COMPUTE_INFO_HAS_SUB_GROUPS != 0
const_func uint32_t get_sub_group_id();
const_func uint32_t get_sub_group_local_id();
const_func uint32_t get_sub_group_size();
const_func uint32_t get_num_sub_groups();

// sub_group_reduce_*/sub_group_scan_exclusive_*/sub_group_scan_inclusive_*
#if !defined(FLOOR_COMPUTE_NO_DOUBLE)
#define SUB_GROUP_TYPES(F, P) F(int32_t, P) F(int64_t, P) F(uint32_t, P) F(uint64_t, P) F(float, P)
#else
#define SUB_GROUP_TYPES(F, P) F(int32_t, P) F(int64_t, P) F(uint32_t, P) F(uint64_t, P) F(float, P) F(double, P)
#endif
#define SUB_GROUP_FUNC(type, func) type func(type);
SUB_GROUP_TYPES(SUB_GROUP_FUNC, sub_group_reduce_add)
SUB_GROUP_TYPES(SUB_GROUP_FUNC, sub_group_reduce_min)
SUB_GROUP_TYPES(SUB_GROUP_FUNC, sub_group_reduce_max)
SUB_GROUP_TYPES(SUB_GROUP_FUNC, sub_group_scan_exclusive_add)
SUB_GROUP_TYPES(SUB_GROUP_FUNC, sub_group_scan_exclusive_min)
SUB_GROUP_TYPES(SUB_GROUP_FUNC, sub_group_scan_exclusive_max)
SUB_GROUP_TYPES(SUB_GROUP_FUNC, sub_group_scan_inclusive_add)
SUB_GROUP_TYPES(SUB_GROUP_FUNC, sub_group_scan_inclusive_min)
SUB_GROUP_TYPES(SUB_GROUP_FUNC, sub_group_scan_inclusive_max)
#undef SUB_GROUP_TYPES
#undef SUB_GROUP_FUNC

#endif

#endif
