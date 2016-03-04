/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2016 Florian Ziesche
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

#ifndef __FLOOR_COMPUTE_DEVICE_OPENCL_HPP__
#define __FLOOR_COMPUTE_DEVICE_OPENCL_HPP__

#if defined(FLOOR_COMPUTE_OPENCL) || defined(FLOOR_COMPUTE_VULKAN)

const_func opencl_c_func size_t get_global_id(uint32_t dim);
const_func opencl_c_func size_t get_global_size(uint32_t dim);
const_func opencl_c_func size_t get_local_id(uint32_t dim);
const_func opencl_c_func size_t get_local_size(uint32_t dim);
const_func opencl_c_func size_t get_group_id(uint32_t dim);
const_func opencl_c_func size_t get_num_groups(uint32_t dim);
const_func opencl_c_func uint32_t get_work_dim();

// wrap opencl id handling functions so that uint32_t is always returned
floor_inline_always uint32_t cl_get_global_id(uint32_t dim) { return uint32_t(get_global_id(dim)); }
floor_inline_always uint32_t cl_get_global_size(uint32_t dim) { return uint32_t(get_global_size(dim)); }
floor_inline_always uint32_t cl_get_local_id(uint32_t dim) { return uint32_t(get_local_id(dim)); }
floor_inline_always uint32_t cl_get_local_size(uint32_t dim) { return uint32_t(get_local_size(dim)); }
floor_inline_always uint32_t cl_get_group_id(uint32_t dim) { return uint32_t(get_group_id(dim)); }
floor_inline_always uint32_t cl_get_num_groups(uint32_t dim) { return uint32_t(get_num_groups(dim)); }
#define get_global_id(x) cl_get_global_id(x)
#define get_global_size(x) cl_get_global_size(x)
#define get_local_id(x) cl_get_local_id(x)
#define get_local_size(x) cl_get_local_size(x)
#define get_group_id(x) cl_get_group_id(x)
#define get_num_groups(x) cl_get_num_groups(x)

#if defined(FLOOR_COMPUTE_APPLECL)
const_func float __cl_fmod(float, float);
const_func float __cl_sqrt(float);
const_func float __cl_fabs(float);
const_func float __cl_floor(float);
const_func float __cl_ceil(float);
const_func float __cl_round(float);
const_func float __cl_trunc(float);
const_func float __cl_rint(float);
const_func float __cl_sin(float);
const_func float __cl_cos(float);
const_func float __cl_tan(float);
const_func float __cl_asin(float);
const_func float __cl_acos(float);
const_func float __cl_atan(float);
const_func float __cl_atan2(float, float);
const_func float __cl_fma(float, float, float);
const_func float __cl_exp(float);
const_func float __cl_exp2(float);
const_func float __cl_log(float);
const_func float __cl_log2(float);
const_func float __cl_pow(float, float);
const_func float __cl_copysign(float, float);
const_func float __cl_fmin(float, float);
const_func float __cl_fmax(float, float);
const_func int16_t __cl_abs(int16_t);
const_func int32_t __cl_abs(int32_t);
const_func int64_t __cl_abs(int64_t);
const_func int16_t __cl_min(int16_t, int16_t);
const_func int32_t __cl_min(int32_t, int32_t);
const_func int64_t __cl_min(int64_t, int64_t);
const_func uint16_t __cl_min(uint16_t, uint16_t);
const_func uint32_t __cl_min(uint32_t, uint32_t);
const_func uint64_t __cl_min(uint64_t, uint64_t);
const_func int16_t __cl_max(int16_t, int16_t);
const_func int32_t __cl_max(int32_t, int32_t);
const_func int64_t __cl_max(int64_t, int64_t);
const_func uint16_t __cl_max(uint16_t, uint16_t);
const_func uint32_t __cl_max(uint32_t, uint32_t);
const_func uint64_t __cl_max(uint64_t, uint64_t);

#if !defined(FLOOR_COMPUTE_NO_DOUBLE)
const_func double __cl_fmod(double, double);
const_func double __cl_sqrt(double);
const_func double __cl_fabs(double);
const_func double __cl_floor(double);
const_func double __cl_ceil(double);
const_func double __cl_round(double);
const_func double __cl_trunc(double);
const_func double __cl_rint(double);
const_func double __cl_sin(double);
const_func double __cl_cos(double);
const_func double __cl_tan(double);
const_func double __cl_asin(double);
const_func double __cl_acos(double);
const_func double __cl_atan(double);
const_func double __cl_atan2(double, double);
const_func double __cl_fma(double, double, double);
const_func double __cl_exp(double);
const_func double __cl_exp2(double);
const_func double __cl_log(double);
const_func double __cl_log2(double);
const_func double __cl_pow(double, double);
const_func double __cl_copysign(double, double);
const_func double __cl_fmin(double, double);
const_func double __cl_fmax(double, double);
#endif

#define ACL_FWD(func, ...) { return func(__VA_ARGS__); }
#define SPIR_FWD(func)
#else // no need for this on opencl/spir
#define ACL_FWD(func, ...) ;
#define SPIR_FWD(func) asm(func)
#endif

// NOTE: in C, these must be declared overloadable, but since this is compiled in C++,
// it is provided automatically (same mangling)
const_func float fmod(float x, float y) ACL_FWD(__cl_fmod, x, y)
const_func float sqrt(float x) ACL_FWD(__cl_sqrt, x);
const_func float rsqrt(float x);
const_func float fabs(float x) ACL_FWD(__cl_fabs, x)
const_func float floor(float x) ACL_FWD(__cl_floor, x)
const_func float ceil(float x) ACL_FWD(__cl_ceil, x)
const_func float round(float x) ACL_FWD(__cl_round, x)
const_func float trunc(float x) ACL_FWD(__cl_trunc, x)
const_func float rint(float x) ACL_FWD(__cl_rint, x)
const_func float sin(float x) ACL_FWD(__cl_sin, x)
const_func float cos(float x) ACL_FWD(__cl_cos, x)
const_func float tan(float x) ACL_FWD(__cl_tan, x)
const_func float asin(float x) ACL_FWD(__cl_asin, x)
const_func float acos(float x) ACL_FWD(__cl_acos, x)
const_func float atan(float x) ACL_FWD(__cl_atan, x)
const_func float atan2(float x, float y) ACL_FWD(__cl_atan2, x, y)
const_func float fma(float a, float b, float c) ACL_FWD(__cl_fma, a, b, c)
const_func float exp(float x) ACL_FWD(__cl_exp, x)
const_func float exp2(float x) ACL_FWD(__cl_exp2, x)
const_func float log(float x) ACL_FWD(__cl_log, x)
const_func float log2(float x) ACL_FWD(__cl_log2, x)
const_func float pow(float x, float y) ACL_FWD(__cl_pow, x, y)
const_func float pown(float x, int y) ACL_FWD(__cl_pown, x, y)
const_func float copysign(float x, float y) ACL_FWD(__cl_copysign, x, y)
const_func float fmin(float x, float y) ACL_FWD(__cl_fmin, x, y)
const_func float fmax(float x, float y) ACL_FWD(__cl_fmax, x, y)

const_func int8_t abs(int8_t x) ACL_FWD(__cl_abs, x)
const_func int16_t abs(int16_t x) ACL_FWD(__cl_abs, x)
const_func int32_t abs(int32_t x) ACL_FWD(__cl_abs, x)
const_func int64_t abs(int64_t x) ACL_FWD(__cl_abs, x)
const_func uint8_t abs(uint8_t x) ACL_FWD(__cl_abs, x)
const_func uint16_t abs(uint16_t x) ACL_FWD(__cl_abs, x)
const_func uint32_t abs(uint32_t x) ACL_FWD(__cl_abs, x)
const_func uint64_t abs(uint64_t x) ACL_FWD(__cl_abs, x)

// to not break constexpr-ness of std::min/max, these need a different name, but still forward to the correct runtime function
const_func int16_t floor_rt_min(int16_t x, int16_t y) SPIR_FWD("_Z3minss") ACL_FWD(__cl_min, x, y)
const_func int32_t floor_rt_min(int32_t x, int32_t y) SPIR_FWD("_Z3minii") ACL_FWD(__cl_min, x, y)
const_func int64_t floor_rt_min(int64_t x, int64_t y) SPIR_FWD("_Z3minll") ACL_FWD(__cl_min, x, y)
const_func uint16_t floor_rt_min(uint16_t x, uint16_t y) SPIR_FWD("_Z3mintt") ACL_FWD(__cl_min, x, y)
const_func uint32_t floor_rt_min(uint32_t x, uint32_t y) SPIR_FWD("_Z3minjj") ACL_FWD(__cl_min, x, y)
const_func uint64_t floor_rt_min(uint64_t x, uint64_t y) SPIR_FWD("_Z3minmm") ACL_FWD(__cl_min, x, y)
const_func int16_t floor_rt_max(int16_t x, int16_t y) SPIR_FWD("_Z3maxss") ACL_FWD(__cl_max, x, y)
const_func int32_t floor_rt_max(int32_t x, int32_t y) SPIR_FWD("_Z3maxii") ACL_FWD(__cl_max, x, y)
const_func int64_t floor_rt_max(int64_t x, int64_t y) SPIR_FWD("_Z3maxll") ACL_FWD(__cl_max, x, y)
const_func uint16_t floor_rt_max(uint16_t x, uint16_t y) SPIR_FWD("_Z3maxtt") ACL_FWD(__cl_max, x, y)
const_func uint32_t floor_rt_max(uint32_t x, uint32_t y) SPIR_FWD("_Z3maxjj") ACL_FWD(__cl_max, x, y)
const_func uint64_t floor_rt_max(uint64_t x, uint64_t y) SPIR_FWD("_Z3maxmm") ACL_FWD(__cl_max, x, y)

#if !defined(FLOOR_COMPUTE_NO_DOUBLE)
const_func double fmod(double x, double y) ACL_FWD(__cl_fmod, x, y)
const_func double sqrt(double x) ACL_FWD(__cl_sqrt, x);
const_func double rsqrt(double x);
const_func double fabs(double x) ACL_FWD(__cl_fabs, x)
const_func double floor(double x) ACL_FWD(__cl_floor, x)
const_func double ceil(double x) ACL_FWD(__cl_ceil, x)
const_func double round(double x) ACL_FWD(__cl_round, x)
const_func double trunc(double x) ACL_FWD(__cl_trunc, x)
const_func double rint(double x) ACL_FWD(__cl_rint, x)
const_func double sin(double x) ACL_FWD(__cl_sin, x)
const_func double cos(double x) ACL_FWD(__cl_cos, x)
const_func double tan(double x) ACL_FWD(__cl_tan, x)
const_func double asin(double x) ACL_FWD(__cl_asin, x)
const_func double acos(double x) ACL_FWD(__cl_acos, x)
const_func double atan(double x) ACL_FWD(__cl_atan, x)
const_func double atan2(double x, double y) ACL_FWD(__cl_atan2, x, y)
const_func double fma(double a, double b, double c) ACL_FWD(__cl_fma, a, b, c)
const_func double exp(double x) ACL_FWD(__cl_exp, x)
const_func double exp2(double x) ACL_FWD(__cl_exp2, x)
const_func double log(double x) ACL_FWD(__cl_log, x)
const_func double log2(double x) ACL_FWD(__cl_log2, x)
const_func double pow(double x, double y) ACL_FWD(__cl_pow, x, y)
const_func double copysign(double x, double y) ACL_FWD(__cl_copysign, x, y)
const_func double fmin(double x, double y) ACL_FWD(__cl_fmin, x, y)
const_func double fmax(double x, double y) ACL_FWD(__cl_fmax, x, y)
#endif

// cleanup
#undef ACL_FWD
#undef SPIR_FWD

// add them to std::
namespace std {
	using ::fmod;
	using ::sqrt;
	using ::rsqrt;
	using ::fabs;
	using ::floor;
	using ::ceil;
	using ::round;
	using ::trunc;
	using ::rint;
	using ::sin;
	using ::cos;
	using ::tan;
	using ::asin;
	using ::acos;
	using ::atan;
	using ::atan2;
	using ::fma;
	using ::exp;
	using ::exp2;
	using ::log;
	using ::log2;
	using ::pow;
	using ::copysign;
	using ::abs;
	
	const_func floor_inline_always float abs(float x) { return fabs(x); }
#if !defined(FLOOR_COMPUTE_NO_DOUBLE)
	const_func floor_inline_always double abs(double x) { return fabs(x); }
#endif
	
}

// NOTE: not supported with vulkan
#if !defined(FLOOR_COMPUTE_APPLECL) && !defined(FLOOR_COMPUTE_VULKAN)
// can't normally produce _Z6printfPrU3AS2cz with clang/llvm 3.5, because a proper "restrict" keyword is missing in c++ mode
// -> slay it with an asm label
int printf(const char constant* st, ...) asm("_Z6printfPrU3AS2cz");
#elif defined(FLOOR_COMPUTE_APPLECL)
// apple uses __printf_cl internally, with c naming!
extern "C" int __printf_cl(const char constant* __restrict st, ...);
#define printf __printf_cl
#endif

// barrier and mem_fence functionality
// NOTE: local = 1, global = 2, image = 4
#if !defined(FLOOR_COMPUTE_APPLECL)
void cl_barrier(uint32_t flags) __attribute__((noduplicate)) asm("_Z7barrierj");
void cl_mem_fence(uint32_t flags) __attribute__((noduplicate)) asm("_Z9mem_fencej");
void cl_read_mem_fence(uint32_t flags) __attribute__((noduplicate)) asm("_Z14read_mem_fencej");
void cl_write_mem_fence(uint32_t flags) __attribute__((noduplicate)) asm("_Z14read_mem_fencej");
#else
void cl_barrier(uint32_t flags) __attribute__((noduplicate)) asm("barrier");
void cl_mem_fence(uint32_t flags) __attribute__((noduplicate)) asm("mem_fence");
void cl_read_mem_fence(uint32_t flags) __attribute__((noduplicate)) asm("read_mem_fence");
void cl_write_mem_fence(uint32_t flags) __attribute__((noduplicate)) asm("read_mem_fence");
#endif

floor_inline_always static void global_barrier() {
	cl_barrier(2u);
}
floor_inline_always static void global_mem_fence() {
	cl_mem_fence(2u);
}
floor_inline_always static void global_read_mem_fence() {
	cl_read_mem_fence(2u);
}
floor_inline_always static void global_write_mem_fence() {
	cl_write_mem_fence(2u);
}

floor_inline_always static void local_barrier() {
	cl_barrier(1u);
}
floor_inline_always static void local_mem_fence() {
	cl_mem_fence(1u);
}
floor_inline_always static void local_read_mem_fence() {
	cl_read_mem_fence(1u);
}
floor_inline_always static void local_write_mem_fence() {
	cl_write_mem_fence(1u);
}

floor_inline_always static void barrier() {
	cl_barrier(3u);
}

//! NOTE: not guaranteed to be available everywhere
floor_inline_always static void image_barrier() {
	cl_barrier(4u);
}
//! NOTE: not guaranteed to be available everywhere
floor_inline_always static void image_mem_fence() {
	cl_mem_fence(4u);
}
//! NOTE: not guaranteed to be available everywhere
floor_inline_always static void image_read_mem_fence() {
	cl_read_mem_fence(4u);
}
//! NOTE: not guaranteed to be available everywhere
floor_inline_always static void image_write_mem_fence() {
	cl_write_mem_fence(4u);
}

// sub-group functionality (opencl 2.1+, cl_khr_subgroups, cl_intel_subgroups)
#if defined(FLOOR_COMPUTE_INFO_HAS_SUB_GROUPS)
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

#endif
