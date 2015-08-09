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

opencl_c_func size_t opencl_const_func get_global_id(uint32_t dim);
opencl_c_func size_t opencl_const_func get_global_size(uint32_t dim);
opencl_c_func size_t opencl_const_func get_local_id(uint32_t dim);
opencl_c_func size_t opencl_const_func get_local_size(uint32_t dim);
opencl_c_func size_t opencl_const_func get_group_id(uint32_t dim);
opencl_c_func size_t opencl_const_func get_num_groups(uint32_t dim);
opencl_c_func uint32_t opencl_const_func get_work_dim();

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
float opencl_const_func __cl_fmod(float, float);
float opencl_const_func __cl_sqrt(float);
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
float opencl_const_func __cl_exp2(float);
float opencl_const_func __cl_log(float);
float opencl_const_func __cl_log2(float);
float opencl_const_func __cl_pow(float, float);
int16_t opencl_const_func __cl_abs(int16_t);
int32_t opencl_const_func __cl_abs(int32_t);
int64_t opencl_const_func __cl_abs(int64_t);

#if !defined(FLOOR_COMPUTE_NO_DOUBLE)
double opencl_const_func __cl_fmod(double, double);
double opencl_const_func __cl_sqrt(double);
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
double opencl_const_func __cl_exp2(double);
double opencl_const_func __cl_log(double);
double opencl_const_func __cl_log2(double);
double opencl_const_func __cl_pow(double, double);
#endif

#define CL_FWD(func, ...) { return func(__VA_ARGS__); }
#else // no need for this on opencl/spir
#define CL_FWD(func, ...) ;
#endif

// NOTE: in C, these must be declared overloadable, but since this is compiled in C++,
// it is provided automatically (same mangling)
float opencl_const_func fmod(float x, float y) CL_FWD(__cl_fmod, x, y)
float opencl_const_func sqrt(float x) CL_FWD(__cl_sqrt, x);
float opencl_const_func rsqrt(float x);
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
float opencl_const_func exp2(float x) CL_FWD(__cl_exp2, x)
float opencl_const_func log(float x) CL_FWD(__cl_log, x)
float opencl_const_func log2(float x) CL_FWD(__cl_log2, x)
float opencl_const_func pow(float x, float y) CL_FWD(__cl_pow, x, y)
int16_t opencl_const_func abs(int16_t x) CL_FWD(__cl_abs, x)
int32_t opencl_const_func abs(int32_t x) CL_FWD(__cl_abs, x)
int64_t opencl_const_func abs(int64_t x) CL_FWD(__cl_abs, x)

#if !defined(FLOOR_COMPUTE_NO_DOUBLE)
double opencl_const_func fmod(double x, double y) CL_FWD(__cl_fmod, x, y)
double opencl_const_func sqrt(double x) CL_FWD(__cl_sqrt, x);
double opencl_const_func rsqrt(double x);
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
double opencl_const_func exp2(double x) CL_FWD(__cl_exp2, x)
double opencl_const_func log(double x) CL_FWD(__cl_log, x)
double opencl_const_func log2(double x) CL_FWD(__cl_log2, x)
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
	using ::exp2;
	using ::log;
	using ::log2;
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

static floor_inline_always void global_barrier() {
	cl_barrier(2u);
}
static floor_inline_always void global_mem_fence() {
	cl_mem_fence(2u);
}
static floor_inline_always void global_read_mem_fence() {
	cl_read_mem_fence(2u);
}
static floor_inline_always void global_write_mem_fence() {
	cl_write_mem_fence(2u);
}

static floor_inline_always void local_barrier() {
	cl_barrier(1u);
}
static floor_inline_always void local_mem_fence() {
	cl_mem_fence(1u);
}
static floor_inline_always void local_read_mem_fence() {
	cl_read_mem_fence(1u);
}
static floor_inline_always void local_write_mem_fence() {
	cl_write_mem_fence(1u);
}

static floor_inline_always void barrier() {
	cl_barrier(3u);
}

#endif

#endif
