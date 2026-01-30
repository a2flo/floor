/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2026 Florian Ziesche
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

#if defined(FLOOR_DEVICE_OPENCL) || defined(FLOOR_DEVICE_VULKAN)

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

#if !defined(FLOOR_DEVICE_NO_DOUBLE)
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
#if !defined(FLOOR_DEVICE_NO_DOUBLE)
floor_inline_always const_func double abs(double x) { return fabs(x); }
#endif

namespace fl {
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
#if !defined(FLOOR_DEVICE_NO_DOUBLE)
const_func double floor_rt_min(double x, double y) { return fmin(x, y); }
const_func double floor_rt_max(double x, double y) { return fmax(x, y); }
#endif
} // namespace fl

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
} // namespace std

#endif
