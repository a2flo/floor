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

// NOTE: in C, these must be declared overloadable, but since this is compiled in C++,
// it is provided automatically (same mangling)
float opencl_const_func fmod(float, float);
float opencl_const_func sqrt(float);
float opencl_const_func rsqrt(float);
float opencl_const_func fabs(float);
float opencl_const_func floor(float);
float opencl_const_func ceil(float);
float opencl_const_func round(float);
float opencl_const_func trunc(float);
float opencl_const_func rint(float);
float opencl_const_func sin(float);
float opencl_const_func cos(float);
float opencl_const_func tan(float);
float opencl_const_func asin(float);
float opencl_const_func acos(float);
float opencl_const_func atan(float);
float opencl_const_func atan2(float, float);
float opencl_const_func fma(float, float, float);
float opencl_const_func exp(float x);
float opencl_const_func log(float x);
float opencl_const_func pow(float x, float y);

#if !defined(FLOOR_COMPUTE_NO_DOUBLE)
double opencl_const_func fmod(double, double);
double opencl_const_func sqrt(double);
double opencl_const_func rsqrt(double);
double opencl_const_func fabs(double);
double opencl_const_func floor(double);
double opencl_const_func ceil(double);
double opencl_const_func round(double);
double opencl_const_func trunc(double);
double opencl_const_func rint(double);
double opencl_const_func sin(double);
double opencl_const_func cos(double);
double opencl_const_func tan(double);
double opencl_const_func asin(double);
double opencl_const_func acos(double);
double opencl_const_func atan(double);
double opencl_const_func atan2(double, double);
double opencl_const_func fma(double, double, double);
double opencl_const_func exp(double x);
double opencl_const_func log(double x);
double opencl_const_func pow(double x, double y);
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

// can't match/produce _Z6printfPrU3AS2cz with clang/llvm 3.5, because a proper "restrict" is missing in c++ mode,
// but apparently extern c printf is working fine with intels and amds implementation, so just use that ...
extern "C" int printf(const char __constant* st, ...);

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

#endif

#endif
