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

#ifndef __FLOOR_COMPUTE_DEVICE_SPIR_HPP__
#define __FLOOR_COMPUTE_DEVICE_SPIR_HPP__

#if defined(FLOOR_COMPUTE_SPIR)

#define const_func __attribute__((const))
size_t const_func get_global_id(uint dimindx);

float const_func __attribute__((overloadable)) fmod(float, float);
float const_func __attribute__((overloadable)) sqrt(float);
float const_func __attribute__((overloadable)) rsqrt(float);
float const_func __attribute__((overloadable)) fabs(float);
float const_func __attribute__((overloadable)) floor(float);
float const_func __attribute__((overloadable)) ceil(float);
float const_func __attribute__((overloadable)) round(float);
float const_func __attribute__((overloadable)) trunc(float);
float const_func __attribute__((overloadable)) rint(float);
float const_func __attribute__((overloadable)) sin(float);
float const_func __attribute__((overloadable)) cos(float);
float const_func __attribute__((overloadable)) tan(float);
float const_func __attribute__((overloadable)) asin(float);
float const_func __attribute__((overloadable)) acos(float);
float const_func __attribute__((overloadable)) atan(float);
float const_func __attribute__((overloadable)) atan2(float, float);
float const_func __attribute__((overloadable)) fma(float, float, float);
float const_func __attribute__((overloadable)) exp(float x);
float const_func __attribute__((overloadable)) log(float x);
float const_func __attribute__((overloadable)) pow(float x, float y);

#if !defined(FLOOR_COMPUTE_NO_DOUBLE)
double const_func __attribute__((overloadable)) fmod(double, double);
double const_func __attribute__((overloadable)) sqrt(double);
double const_func __attribute__((overloadable)) rsqrt(double);
double const_func __attribute__((overloadable)) fabs(double);
double const_func __attribute__((overloadable)) floor(double);
double const_func __attribute__((overloadable)) ceil(double);
double const_func __attribute__((overloadable)) round(double);
double const_func __attribute__((overloadable)) trunc(double);
double const_func __attribute__((overloadable)) rint(double);
double const_func __attribute__((overloadable)) sin(double);
double const_func __attribute__((overloadable)) cos(double);
double const_func __attribute__((overloadable)) tan(double);
double const_func __attribute__((overloadable)) asin(double);
double const_func __attribute__((overloadable)) acos(double);
double const_func __attribute__((overloadable)) atan(double);
double const_func __attribute__((overloadable)) atan2(double, double);
double const_func __attribute__((overloadable)) fma(double, double, double);
double const_func __attribute__((overloadable)) exp(double x);
double const_func __attribute__((overloadable)) log(double x);
double const_func __attribute__((overloadable)) pow(double x, double y);
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

#endif

#endif
