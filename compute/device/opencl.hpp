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

#ifndef __FLOOR_COMPUTE_DEVICE_OPENCL_HPP__
#define __FLOOR_COMPUTE_DEVICE_OPENCL_HPP__

#if defined(FLOOR_COMPUTE_OPENCL)

#define opencl_const_func __attribute__((const))

#if defined(FLOOR_COMPUTE_APPLECL)
#define opencl_c_func extern "C"
#else
#define opencl_c_func
#endif

opencl_c_func size_t opencl_const_func get_global_id(uint dimindx);
opencl_c_func size_t opencl_const_func get_global_size(uint dimindx);
opencl_c_func size_t opencl_const_func get_local_id(uint dimindx);
opencl_c_func size_t opencl_const_func get_local_size(uint dimindx);
opencl_c_func size_t opencl_const_func get_group_id(uint dimindx);
opencl_c_func size_t opencl_const_func get_num_groups(uint dimindx);
opencl_c_func uint opencl_const_func get_work_dim();
opencl_c_func size_t opencl_const_func get_global_offset(uint dimindx);

#define global_id size3 { get_global_id(0), get_global_id(1), get_global_id(2) }
#define global_size size3 { get_global_size(0), get_global_size(1), get_global_size(2) }
#define local_id size3 { get_local_id(0), get_local_id(1), get_local_id(2) }
#define local_size size3 { get_local_size(0), get_local_size(1), get_local_size(2) }
#define group_id size3 { get_group_id(0), get_group_id(1), get_group_id(2) }
#define group_size size3 { get_num_groups(0), get_num_groups(1), get_num_groups(2) }

#if defined(FLOOR_COMPUTE_APPLECL)
float opencl_const_func __cl_fmod(float, float);
float opencl_const_func native_sqrt(float);
float opencl_const_func native_rsqrt(float);
float opencl_const_func __cl_fabs(float);
float opencl_const_func __cl_floor(float);
float opencl_const_func __cl_ceil(float);
float opencl_const_func __cl_round(float);
float opencl_const_func __cl_trunc(float);
float opencl_const_func __cl_rint(float);
float opencl_const_func __cl_sin(float);
float opencl_const_func __cl_cos(float);
float opencl_const_func __cl_tan(float);
float opencl_const_func __cl_asin(float);
float opencl_const_func __cl_acos(float);
float opencl_const_func __cl_atan(float);
float opencl_const_func __cl_atan2(float, float);
float opencl_const_func __cl_fma(float, float, float);
float opencl_const_func __cl_exp(float);
float opencl_const_func __cl_log(float);
float opencl_const_func __cl_pow(float, float);
int16_t opencl_const_func __cl_abs(int16_t);
int32_t opencl_const_func __cl_abs(int32_t);
int64_t opencl_const_func __cl_abs(int64_t);

#if !defined(FLOOR_COMPUTE_NO_DOUBLE)
double opencl_const_func __cl_fmod(double, double);
double opencl_const_func __cl_sqrt(double);
double opencl_const_func __cl_rsqrt(double);
double opencl_const_func __cl_fabs(double);
double opencl_const_func __cl_floor(double);
double opencl_const_func __cl_ceil(double);
double opencl_const_func __cl_round(double);
double opencl_const_func __cl_trunc(double);
double opencl_const_func __cl_rint(double);
double opencl_const_func __cl_sin(double);
double opencl_const_func __cl_cos(double);
double opencl_const_func __cl_tan(double);
double opencl_const_func __cl_asin(double);
double opencl_const_func __cl_acos(double);
double opencl_const_func __cl_atan(double);
double opencl_const_func __cl_atan2(double, double);
double opencl_const_func __cl_fma(double, double, double);
double opencl_const_func __cl_exp(double);
double opencl_const_func __cl_log(double);
double opencl_const_func __cl_pow(double, double);
#endif

#define CL_FWD(func, ...) { return func(__VA_ARGS__); }
#else // no need for this on opencl/spir
#define CL_FWD(func, ...) ;
#endif

// NOTE: in C, these must be declared overloadable, but since this is compiled in C++,
// it is provided automatically (same mangling)
float opencl_const_func fmod(float x, float y) CL_FWD(__cl_fmod, x, y)
float opencl_const_func sqrt(float x) CL_FWD(native_sqrt, x);
float opencl_const_func rsqrt(float x) CL_FWD(native_rsqrt, x);
float opencl_const_func fabs(float x) CL_FWD(__cl_fabs, x)
float opencl_const_func floor(float x) CL_FWD(__cl_floor, x)
float opencl_const_func ceil(float x) CL_FWD(__cl_ceil, x)
float opencl_const_func round(float x) CL_FWD(__cl_round, x)
float opencl_const_func trunc(float x) CL_FWD(__cl_trunc, x)
float opencl_const_func rint(float x) CL_FWD(__cl_rint, x)
float opencl_const_func sin(float x) CL_FWD(__cl_sin, x)
float opencl_const_func cos(float x) CL_FWD(__cl_cos, x)
float opencl_const_func tan(float x) CL_FWD(__cl_tan, x)
float opencl_const_func asin(float x) CL_FWD(__cl_asin, x)
float opencl_const_func acos(float x) CL_FWD(__cl_acos, x)
float opencl_const_func atan(float x) CL_FWD(__cl_atan, x)
float opencl_const_func atan2(float x, float y) CL_FWD(__cl_atan2, x, y)
float opencl_const_func fma(float a, float b, float c) CL_FWD(__cl_fma, a, b, c)
float opencl_const_func exp(float x) CL_FWD(__cl_exp, x)
float opencl_const_func log(float x) CL_FWD(__cl_log, x)
float opencl_const_func pow(float x, float y) CL_FWD(__cl_pow, x, y)
int16_t opencl_const_func abs(int16_t x) CL_FWD(__cl_abs, x)
int32_t opencl_const_func abs(int32_t x) CL_FWD(__cl_abs, x)
int64_t opencl_const_func abs(int64_t x) CL_FWD(__cl_abs, x)

#if !defined(FLOOR_COMPUTE_NO_DOUBLE)
double opencl_const_func fmod(double x, double y) CL_FWD(__cl_fmod, x, y)
double opencl_const_func sqrt(double x) CL_FWD(__cl_sqrt, x);
double opencl_const_func rsqrt(double x) CL_FWD(__cl_rsqrt, x);
double opencl_const_func fabs(double x) CL_FWD(__cl_fabs, x)
double opencl_const_func floor(double x) CL_FWD(__cl_floor, x)
double opencl_const_func ceil(double x) CL_FWD(__cl_ceil, x)
double opencl_const_func round(double x) CL_FWD(__cl_round, x)
double opencl_const_func trunc(double x) CL_FWD(__cl_trunc, x)
double opencl_const_func rint(double x) CL_FWD(__cl_rint, x)
double opencl_const_func sin(double x) CL_FWD(__cl_sin, x)
double opencl_const_func cos(double x) CL_FWD(__cl_cos, x)
double opencl_const_func tan(double x) CL_FWD(__cl_tan, x)
double opencl_const_func asin(double x) CL_FWD(__cl_asin, x)
double opencl_const_func acos(double x) CL_FWD(__cl_acos, x)
double opencl_const_func atan(double x) CL_FWD(__cl_atan, x)
double opencl_const_func atan2(double x, double y) CL_FWD(__cl_atan2, x, y)
double opencl_const_func fma(double a, double b, double c) CL_FWD(__cl_fma, a, b, c)
double opencl_const_func exp(double x) CL_FWD(__cl_exp, x)
double opencl_const_func log(double x) CL_FWD(__cl_log, x)
double opencl_const_func pow(double x, double y) CL_FWD(__cl_pow, x, y)
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
	using ::fma;
	using ::exp;
	using ::log;
	using ::pow;
}

#if defined(FLOOR_COMPUTE_SPIR)
// can't normally produce _Z6printfPrU3AS2cz with clang/llvm 3.5, because a proper "restrict" keyword is missing in c++ mode
// -> slay it with an asm label
int printf(const char __constant* st, ...) asm("_Z6printfPrU3AS2cz");
#elif defined(FLOOR_COMPUTE_APPLECL)
// apple uses __printf_cl internally, with c naming!
extern "C" int __printf_cl(const char __constant* __restrict st, ...);
#define printf __printf_cl
#endif

// barrier and mem_fence functionality
opencl_c_func void barrier(uint32_t flags) __attribute__((noduplicate));
opencl_c_func void mem_fence(uint32_t flags) __attribute__((noduplicate));
opencl_c_func void read_mem_fence(uint32_t flags) __attribute__((noduplicate));
opencl_c_func void write_mem_fence(uint32_t flags) __attribute__((noduplicate));

static floor_inline_always void global_barrier() {
	barrier(2u);
}
static floor_inline_always void global_mem_fence() {
	mem_fence(2u);
}
static floor_inline_always void global_read_mem_fence() {
	read_mem_fence(2u);
}
static floor_inline_always void global_write_mem_fence() {
	write_mem_fence(2u);
}
static floor_inline_always void local_barrier() {
	barrier(1u);
}
static floor_inline_always void local_mem_fence() {
	mem_fence(1u);
}
static floor_inline_always void local_read_mem_fence() {
	read_mem_fence(1u);
}
static floor_inline_always void local_write_mem_fence() {
	write_mem_fence(1u);
}

// atomics
#include <floor/compute/device/opencl_atomic.hpp>

#endif

#endif
