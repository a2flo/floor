/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2020 Florian Ziesche
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

#elif defined(FLOOR_COMPUTE_VULKAN)
// similar to metal, vulkan id handling functions will be handled on the compiler side and replaced with builtin variables
FLOOR_GLOBAL_ID_RANGE_ATTR const_func uint32_t get_global_id(uint32_t dim) asm("floor.builtin.global_id.i32");
FLOOR_GLOBAL_SIZE_RANGE_ATTR const_func uint32_t get_global_size(uint32_t dim) asm("floor.builtin.global_size.i32");
FLOOR_LOCAL_ID_RANGE_ATTR const_func uint32_t get_local_id(uint32_t dim) asm("floor.builtin.local_id.i32");
FLOOR_LOCAL_SIZE_RANGE_ATTR const_func uint32_t get_local_size(uint32_t dim) asm("floor.builtin.local_size.i32");
FLOOR_GROUP_ID_RANGE_ATTR const_func uint32_t get_group_id(uint32_t dim) asm("floor.builtin.group_id.i32");
FLOOR_GROUP_SIZE_RANGE_ATTR const_func uint32_t get_group_size(uint32_t dim) asm("floor.builtin.group_size.i32");
[[range(1u, 3u)]] const_func uint32_t get_work_dim() asm("floor.builtin.work_dim.i32");
#endif

// NOTE: in C, these must be declared overloadable, but since this is compiled in C++,
// it is provided automatically (same mangling)
const_func float fmod(float x, float y);
const_func float sqrt(float x);
const_func float rsqrt(float x);
const_func float fabs(float x);
const_func float floor(float x);
const_func float ceil(float x);
const_func float round(float x);
const_func float trunc(float x);
const_func float rint(float x);
const_func float sin(float x);
const_func float cos(float x);
const_func float tan(float x);
const_func float asin(float x);
const_func float acos(float x);
const_func float atan(float x);
const_func float atan2(float x, float y);
const_func float fma(float a, float b, float c);
const_func float sinh(float x);
const_func float cosh(float x);
const_func float tanh(float x);
const_func float asinh(float x);
const_func float acosh(float x);
const_func float atanh(float x);
const_func float exp(float x);
const_func float exp2(float x);
const_func float log(float x);
const_func float log2(float x);
const_func float pow(float x, float y);
const_func float pown(float x, int y);
const_func float copysign(float x, float y);
const_func float fmin(float x, float y);
const_func float fmax(float x, float y);

const_func half fmod(half x, half y);
const_func half sqrt(half x);
const_func half rsqrt(half x);
const_func half fabs(half x);
const_func half floor(half x);
const_func half ceil(half x);
const_func half round(half x);
const_func half trunc(half x);
const_func half rint(half x);
const_func half sin(half x);
const_func half cos(half x);
const_func half tan(half x);
const_func half asin(half x);
const_func half acos(half x);
const_func half atan(half x);
const_func half atan2(half x, half y);
const_func half sinh(half x);
const_func half cosh(half x);
const_func half tanh(half x);
const_func half asinh(half x);
const_func half acosh(half x);
const_func half atanh(half x);
const_func half fma(half a, half b, half c);
const_func half exp(half x);
const_func half exp2(half x);
const_func half log(half x);
const_func half log2(half x);
const_func half pow(half x, half y);
const_func half copysign(half x, half y);
const_func half fmin(half x, half y);
const_func half fmax(half x, half y);

#if !defined(FLOOR_COMPUTE_NO_DOUBLE)
const_func double fmod(double x, double y);
const_func double sqrt(double x);
const_func double rsqrt(double x);
const_func double fabs(double x);
const_func double floor(double x);
const_func double ceil(double x);
const_func double round(double x);
const_func double trunc(double x);
const_func double rint(double x);
const_func double sin(double x);
const_func double cos(double x);
const_func double tan(double x);
const_func double asin(double x);
const_func double acos(double x);
const_func double atan(double x);
const_func double atan2(double x, double y);
const_func double sinh(double x);
const_func double cosh(double x);
const_func double tanh(double x);
const_func double asinh(double x);
const_func double acosh(double x);
const_func double atanh(double x);
const_func double fma(double a, double b, double c);
const_func double exp(double x);
const_func double exp2(double x);
const_func double log(double x);
const_func double log2(double x);
const_func double pow(double x, double y);
const_func double copysign(double x, double y);
const_func double fmin(double x, double y);
const_func double fmax(double x, double y);
#endif

const_func int8_t abs(int8_t x);
const_func int16_t abs(int16_t x);
const_func int32_t abs(int32_t x);
const_func int64_t abs(int64_t x);
const_func uint8_t abs(uint8_t x);
const_func uint16_t abs(uint16_t x);
const_func uint32_t abs(uint32_t x);
const_func uint64_t abs(uint64_t x);

floor_inline_always const_func half abs(half x) { return fabs(x); }
floor_inline_always const_func float abs(float x) { return fabs(x); }
#if !defined(FLOOR_COMPUTE_NO_DOUBLE)
floor_inline_always const_func double abs(double x) { return fabs(x); }
#endif

// to not break constexpr-ness of std::min/max, these need a different name, but still forward to the correct runtime function
const_func int8_t floor_rt_min(int8_t x, int8_t y) asm("_Z3mincc");
const_func int16_t floor_rt_min(int16_t x, int16_t y) asm("_Z3minss");
const_func int32_t floor_rt_min(int32_t x, int32_t y) asm("_Z3minii");
const_func int64_t floor_rt_min(int64_t x, int64_t y) asm("_Z3minll");
const_func uint8_t floor_rt_min(uint8_t x, uint8_t y) asm("_Z3minhh");
const_func uint16_t floor_rt_min(uint16_t x, uint16_t y) asm("_Z3mintt");
const_func uint32_t floor_rt_min(uint32_t x, uint32_t y) asm("_Z3minjj");
const_func uint64_t floor_rt_min(uint64_t x, uint64_t y) asm("_Z3minmm");
const_func int8_t floor_rt_max(int8_t x, int8_t y) asm("_Z3maxcc");
const_func int16_t floor_rt_max(int16_t x, int16_t y) asm("_Z3maxss");
const_func int32_t floor_rt_max(int32_t x, int32_t y) asm("_Z3maxii");
const_func int64_t floor_rt_max(int64_t x, int64_t y) asm("_Z3maxll");
const_func uint8_t floor_rt_max(uint8_t x, uint8_t y) asm("_Z3maxhh");
const_func uint16_t floor_rt_max(uint16_t x, uint16_t y) asm("_Z3maxtt");
const_func uint32_t floor_rt_max(uint32_t x, uint32_t y) asm("_Z3maxjj");
const_func uint64_t floor_rt_max(uint64_t x, uint64_t y) asm("_Z3maxmm");
const_func half floor_rt_min(half x, half y) { return fmin(x, y); }
const_func half floor_rt_max(half x, half y) { return fmax(x, y); }
const_func float floor_rt_min(float x, float y) { return fmin(x, y); }
const_func float floor_rt_max(float x, float y) { return fmax(x, y); }
#if !defined(FLOOR_COMPUTE_NO_DOUBLE)
const_func double floor_rt_min(double x, double y) { return fmin(x, y); }
const_func double floor_rt_max(double x, double y) { return fmax(x, y); }
#endif

// non-standard bit counting functions (don't use these directly, use math::func instead)
#if !defined(FLOOR_COMPUTE_VULKAN) // -> opencl
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
#else // -> vulkan
// we don't have direct clz/ctz support
#if defined(FLOOR_COMPUTE_INFO_VULKAN_HAS_INT16_SUPPORT_1)
const_func int16_t floor_vulkan_find_int_lsb(uint16_t x) asm("floor.find_int_lsb.u16");
const_func int16_t floor_vulkan_find_int_lsb(int16_t x) asm("floor.find_int_lsb.s16");
#endif
const_func int32_t floor_vulkan_find_int_lsb(uint32_t x) asm("floor.find_int_lsb.u32");
const_func int32_t floor_vulkan_find_int_lsb(int32_t x) asm("floor.find_int_lsb.s32");
const_func int64_t floor_vulkan_find_int_lsb(uint64_t x) asm("floor.find_int_lsb.u64");
const_func int64_t floor_vulkan_find_int_lsb(int64_t x) asm("floor.find_int_lsb.s64");

const_func int32_t floor_vulkan_find_int_msb(uint32_t x) asm("floor.find_int_msb.u32"); // 32-bit only
const_func int32_t floor_vulkan_find_int_msb(int32_t x) asm("floor.find_int_msb.s32"); // 32-bit only

#if defined(FLOOR_COMPUTE_INFO_VULKAN_HAS_INT16_SUPPORT_1)
const_func uint16_t floor_rt_reverse_bits(uint16_t x) asm("floor.bit_reverse.u16");
#endif
const_func uint32_t floor_rt_reverse_bits(uint32_t x) asm("floor.bit_reverse.u32");
const_func uint64_t floor_rt_reverse_bits(uint64_t x) asm("floor.bit_reverse.u64");

// -> forward to lsb/msb functions
const_func uint16_t floor_rt_clz(uint16_t x) {
	// same if we do or do not have 16-bit support
	const auto msb_bit_idx = floor_vulkan_find_int_msb(uint32_t(x));
	return uint16_t(msb_bit_idx < 0 ? 16 : (15 - msb_bit_idx));
}
const_func uint32_t floor_rt_clz(uint32_t x) {
	const auto msb_bit_idx = floor_vulkan_find_int_msb(x);
	return (msb_bit_idx < 0 ? 32 : (31 - msb_bit_idx));
}
const_func uint64_t floor_rt_clz(uint64_t x) {
	// can't use "find_int_msb", b/c it's 32-bit only
	// -> reverse the bits and find the lsb instead
	const auto rev_lsb_bit_idx = floor_vulkan_find_int_lsb(floor_rt_reverse_bits(x));
	return (rev_lsb_bit_idx < 0 ? 64 : rev_lsb_bit_idx);
}

#if defined(FLOOR_COMPUTE_INFO_VULKAN_HAS_INT16_SUPPORT_1)
const_func uint16_t floor_rt_ctz(uint16_t x) {
	const auto lsb_bit_idx = floor_vulkan_find_int_lsb(x);
	return (lsb_bit_idx < 0 ? 16 : lsb_bit_idx);
}
#endif
const_func uint32_t floor_rt_ctz(uint32_t x) {
	const auto lsb_bit_idx = floor_vulkan_find_int_lsb(x);
	return (lsb_bit_idx < 0 ? 32 : lsb_bit_idx);
}
const_func uint64_t floor_rt_ctz(uint64_t x) {
	const auto lsb_bit_idx = floor_vulkan_find_int_lsb(x);
	return (lsb_bit_idx < 0 ? 64 : lsb_bit_idx);
}

#if !defined(FLOOR_COMPUTE_INFO_VULKAN_HAS_INT16_SUPPORT_1)
// can emulate this if no 16-bit support
const_func uint16_t floor_rt_ctz(uint16_t x) {
	const auto lsb_bit_idx = floor_vulkan_find_int_lsb(uint32_t(x));
	return uint16_t(lsb_bit_idx < 0 ? 16 : lsb_bit_idx);
}
#endif

// we have direct support for these
#if defined(FLOOR_COMPUTE_INFO_VULKAN_HAS_INT16_SUPPORT_1)
const_func uint16_t floor_rt_popcount(uint16_t x) asm("floor.bit_count.u16");
#endif
const_func uint32_t floor_rt_popcount(uint32_t x) asm("floor.bit_count.u32");
const_func uint64_t floor_rt_popcount(uint64_t x) asm("floor.bit_count.u64");

#if !defined(FLOOR_COMPUTE_INFO_VULKAN_HAS_INT16_SUPPORT_1)
// can emulate this if no 16-bit support
const_func uint16_t floor_rt_popcount(uint16_t x) { return uint16_t(floor_rt_popcount(uint32_t(x))); }
#endif
#endif

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
	using ::sinh;
	using ::cosh;
	using ::tanh;
	using ::asinh;
	using ::acosh;
	using ::atanh;
	using ::fma;
	using ::exp;
	using ::exp2;
	using ::log;
	using ::log2;
	using ::pow;
	using ::copysign;
	using ::abs;
}

// NOTE: builtin printf is not supported with vulkan
#if !defined(FLOOR_COMPUTE_VULKAN)
// can't normally produce _Z6printfPrU3AS2cz with clang/llvm, because a proper "restrict" keyword is missing in c++ mode
// -> slay it with an asm label
int printf(const char constant* st, ...) asm("_Z6printfPrU3AS2cz");

#else // Vulkan
#if !defined(FLOOR_COMPUTE_HAS_SOFT_PRINTF) || (FLOOR_COMPUTE_HAS_SOFT_PRINTF == 0)
// not supported
#define printf(...)
#else // software printf implementation

global uint32_t* floor_get_printf_buffer() asm("floor.builtin.get_printf_buffer");
#include <floor/compute/device/soft_printf.hpp>

template <size_t format_N, typename... Args>
static void printf(constant const char (&format)[format_N], const Args&... args) {
	soft_printf::as::printf_impl(format, args...);
}
#endif

#endif

// barrier and mem_fence functionality
// NOTE: local = 1, global = 2, image = 4
void cl_barrier(uint32_t flags) __attribute__((noduplicate)) asm("_Z7barrierj");
void cl_mem_fence(uint32_t flags) __attribute__((noduplicate)) asm("_Z9mem_fencej");
void cl_read_mem_fence(uint32_t flags) __attribute__((noduplicate)) asm("_Z14read_mem_fencej");
void cl_write_mem_fence(uint32_t flags) __attribute__((noduplicate)) asm("_Z14read_mem_fencej");

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

#endif
