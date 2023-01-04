/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2023 Florian Ziesche
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

#ifndef __FLOOR_COMPUTE_DEVICE_HOST_HPP__
#define __FLOOR_COMPUTE_DEVICE_HOST_HPP__

#if defined(FLOOR_COMPUTE_HOST)

#if !defined(FLOOR_COMPUTE_HOST_DEVICE)
#if !defined(const_func)
#define const_func __attribute__((const))
#endif
#endif

// math functions
#if !defined(FLOOR_COMPUTE_HOST_DEVICE)
#include <cmath>
#endif
namespace std {
// if we're compiling for a generic host-compute device, math.h doesn't exist / can't be used
// -> define all supported math functions in here
#if defined(FLOOR_COMPUTE_HOST_DEVICE)

floor_inline_always const_func half sqrt(half x) { return (half)__builtin_sqrtf(float(x)); }
floor_inline_always const_func half rsqrt(half x) { return (half)(1.0f / __builtin_sqrtf(float(x))); }
floor_inline_always const_func half fabs(half x) { return (half)__builtin_fabsf(float(x)); }
floor_inline_always const_func half abs(half x) { return (half)__builtin_fabsf(float(x)); }
floor_inline_always const_func half fmin(half a, half b) { return (half)__builtin_fminf(float(a), float(b)); }
floor_inline_always const_func half min(half a, half b) { return (half)__builtin_fminf(float(a), float(b)); }
floor_inline_always const_func half fmax(half a, half b) { return (half)__builtin_fmaxf(float(a), float(b)); }
floor_inline_always const_func half max(half a, half b) { return (half)__builtin_fmaxf(float(a), float(b)); }
floor_inline_always const_func half floor(half x) { return (half)__builtin_floorf(float(x)); }
floor_inline_always const_func half ceil(half x) { return (half)__builtin_ceilf(float(x)); }
floor_inline_always const_func half round(half x) { return (half)__builtin_roundf(float(x)); }
floor_inline_always const_func half trunc(half x) { return (half)__builtin_truncf(float(x)); }
floor_inline_always const_func half rint(half x) { return (half)__builtin_rintf(float(x)); }
floor_inline_always const_func half sin(half x) { return (half)__builtin_sinf(float(x)); }
floor_inline_always const_func half cos(half x) { return (half)__builtin_cosf(float(x)); }
floor_inline_always const_func half tan(half x) { return (half)__builtin_tanf(float(x)); }
floor_inline_always const_func half asin(half x) { return (half)__builtin_asinf(float(x)); }
floor_inline_always const_func half acos(half x) { return (half)__builtin_acosf(float(x)); }
floor_inline_always const_func half atan(half x) { return (half)__builtin_atanf(float(x)); }
floor_inline_always const_func half atan2(half a, half b) { return (half)__builtin_atan2f(float(a), float(b)); }
floor_inline_always const_func half sinh(half x) { return (half)__builtin_sinhf(float(x)); }
floor_inline_always const_func half cosh(half x) { return (half)__builtin_coshf(float(x)); }
floor_inline_always const_func half tanh(half x) { return (half)__builtin_tanhf(float(x)); }
floor_inline_always const_func half asinh(half x) { return (half)__builtin_asinhf(float(x)); }
floor_inline_always const_func half acosh(half x) { return (half)__builtin_acoshf(float(x)); }
floor_inline_always const_func half atanh(half x) { return (half)__builtin_atanhf(float(x)); }
floor_inline_always const_func half fma(half a, half b, half c) { return (a * b + c); }
floor_inline_always const_func half exp(half x) { return (half)__builtin_expf(float(x)); }
floor_inline_always const_func half exp2(half x) { return (half)__builtin_exp2f(float(x)); }
floor_inline_always const_func half log(half x) { return (half)__builtin_logf(float(x)); }
floor_inline_always const_func half log2(half x) { return (half)__builtin_log2f(float(x)); }
floor_inline_always const_func half pow(half a, half b) { return (half)__builtin_powf(float(a), float(b)); }
#if 0 // TODO: directly translate to asm instruction
floor_inline_always const_func half fmod(half a, half b) { return (half)__builtin_fmodf(float(a), float(b)); }
#else
floor_inline_always const_func half fmod(half a, half b) { return a - half(__builtin_truncf(float(a) / float(b)) * float(b)); }
#endif
floor_inline_always const_func half copysign(half a, half b) { return (half)__builtin_copysignf(float(a), float(b)); }

floor_inline_always const_func float sqrt(float x) { return __builtin_sqrtf(x); }
floor_inline_always const_func float rsqrt(float x) { return 1.0f / __builtin_sqrtf(x); }
floor_inline_always const_func float fabs(float x) { return __builtin_fabsf(x); }
floor_inline_always const_func float abs(float x) { return __builtin_fabsf(x); }
floor_inline_always const_func float fmin(float a, float b) { return __builtin_fminf(a, b); }
floor_inline_always const_func float min(float a, float b) { return __builtin_fminf(a, b); }
floor_inline_always const_func float fmax(float a, float b) { return __builtin_fmaxf(a, b); }
floor_inline_always const_func float max(float a, float b) { return __builtin_fmaxf(a, b); }
floor_inline_always const_func float floor(float x) { return __builtin_floorf(x); }
floor_inline_always const_func float ceil(float x) { return __builtin_ceilf(x); }
floor_inline_always const_func float round(float x) { return __builtin_roundf(x); }
floor_inline_always const_func float trunc(float x) { return __builtin_truncf(x); }
floor_inline_always const_func float rint(float x) { return __builtin_rintf(x); }
floor_inline_always const_func float sin(float x) { return __builtin_sinf(x); }
floor_inline_always const_func float cos(float x) { return __builtin_cosf(x); }
floor_inline_always const_func float tan(float x) { return __builtin_tanf(x); }
floor_inline_always const_func float asin(float x) { return __builtin_asinf(x); }
floor_inline_always const_func float acos(float x) { return __builtin_acosf(x); }
floor_inline_always const_func float atan(float x) { return __builtin_atanf(x); }
floor_inline_always const_func float atan2(float a, float b) { return __builtin_atan2f(a, b); }
floor_inline_always const_func float sinh(float x) { return __builtin_sinhf(x); }
floor_inline_always const_func float cosh(float x) { return __builtin_coshf(x); }
floor_inline_always const_func float tanh(float x) { return __builtin_tanhf(x); }
floor_inline_always const_func float asinh(float x) { return __builtin_asinhf(x); }
floor_inline_always const_func float acosh(float x) { return __builtin_acoshf(x); }
floor_inline_always const_func float atanh(float x) { return __builtin_atanhf(x); }
floor_inline_always const_func float fma(float a, float b, float c) { return (a * b + c); }
floor_inline_always const_func float exp(float x) { return __builtin_expf(x); }
floor_inline_always const_func float exp2(float x) { return __builtin_exp2f(x); }
floor_inline_always const_func float log(float x) { return __builtin_logf(x); }
floor_inline_always const_func float log2(float x) { return __builtin_log2f(x); }
floor_inline_always const_func float pow(float a, float b) { return __builtin_powf(a, b); }
#if 0 // TODO: directly translate to asm instruction
floor_inline_always const_func float fmod(float a, float b) { return __builtin_fmodf(a, b); }
#else
floor_inline_always const_func float fmod(float a, float b) { return a - __builtin_truncf(a / b) * b; }
#endif
floor_inline_always const_func float copysign(float a, float b) { return __builtin_copysignf(a, b); }

floor_inline_always const_func int8_t abs(int8_t x) { return __builtin_abs(x); }
floor_inline_always const_func int16_t abs(int16_t x) { return __builtin_abs(x); }
floor_inline_always const_func int32_t abs(int32_t x) { return __builtin_abs(x); }
floor_inline_always const_func int64_t abs(int64_t x) { return __builtin_abs(x); }
floor_inline_always const_func uint8_t abs(uint8_t x) { return x; }
floor_inline_always const_func uint16_t abs(uint16_t x) { return x; }
floor_inline_always const_func uint32_t abs(uint32_t x) { return x; }
floor_inline_always const_func uint64_t abs(uint64_t x) { return x; }

floor_inline_always const_func int8_t floor_rt_min(int8_t a, int8_t b) { return a <= b ? a : b; }
floor_inline_always const_func uint8_t floor_rt_min(uint8_t a, uint8_t b) { return a <= b ? a : b; }
floor_inline_always const_func int16_t floor_rt_min(int16_t a, int16_t b) { return a <= b ? a : b; }
floor_inline_always const_func uint16_t floor_rt_min(uint16_t a, uint16_t b) { return a <= b ? a : b; }
floor_inline_always const_func int32_t floor_rt_min(int32_t a, int32_t b) { return a <= b ? a : b; }
floor_inline_always const_func uint32_t floor_rt_min(uint32_t a, uint32_t b) { return a <= b ? a : b; }
floor_inline_always const_func int64_t floor_rt_min(int64_t a, int64_t b) { return a <= b ? a : b; }
floor_inline_always const_func uint64_t floor_rt_min(uint64_t a, uint64_t b) { return a <= b ? a : b; }
floor_inline_always const_func half floor_rt_min(half a, half b) { return (half)__builtin_fminf(float(a), float(b)); }
floor_inline_always const_func float floor_rt_min(float a, float b) { return __builtin_fminf(a, b); }
floor_inline_always const_func int8_t floor_rt_max(int8_t a, int8_t b) { return a >= b ? a : b; }
floor_inline_always const_func uint8_t floor_rt_max(uint8_t a, uint8_t b) { return a >= b ? a : b; }
floor_inline_always const_func int16_t floor_rt_max(int16_t a, int16_t b) { return a >= b ? a : b; }
floor_inline_always const_func uint16_t floor_rt_max(uint16_t a, uint16_t b) { return a >= b ? a : b; }
floor_inline_always const_func int32_t floor_rt_max(int32_t a, int32_t b) { return a >= b ? a : b; }
floor_inline_always const_func uint32_t floor_rt_max(uint32_t a, uint32_t b) { return a >= b ? a : b; }
floor_inline_always const_func int64_t floor_rt_max(int64_t a, int64_t b) { return a >= b ? a : b; }
floor_inline_always const_func uint64_t floor_rt_max(uint64_t a, uint64_t b) { return a >= b ? a : b; }
floor_inline_always const_func half floor_rt_max(half a, half b) { return (half)__builtin_fmaxf(float(a), float(b)); }
floor_inline_always const_func float floor_rt_max(float a, float b) { return __builtin_fmaxf(a, b); }

floor_inline_always const_func uint16_t floor_rt_clz(uint16_t x) { return __builtin_clzs(x); }
floor_inline_always const_func uint32_t floor_rt_clz(uint32_t x) { return __builtin_clz(x); }
floor_inline_always const_func uint64_t floor_rt_clz(uint64_t x) { return __builtin_clzll(x); }
floor_inline_always const_func uint16_t floor_rt_ctz(uint16_t x) { return __builtin_ctzs(x); }
floor_inline_always const_func uint32_t floor_rt_ctz(uint32_t x) { return __builtin_ctz(x); }
floor_inline_always const_func uint64_t floor_rt_ctz(uint64_t x) { return __builtin_ctzll(x); }
floor_inline_always const_func uint16_t floor_rt_popcount(uint16_t x) { return __builtin_popcount(uint32_t(x)); }
floor_inline_always const_func uint32_t floor_rt_popcount(uint32_t x) { return __builtin_popcount(x); }
floor_inline_always const_func uint64_t floor_rt_popcount(uint64_t x) { return __builtin_popcountll(x); }

floor_inline_always const_func static double rsqrt(double x) { return 1.0 / __builtin_sqrt(x); }

#else

floor_inline_always const_func static float rsqrt(float x) { return 1.0f / sqrt(x); }
floor_inline_always const_func static double rsqrt(double x) { return 1.0 / sqrt(x); }

#endif // FLOOR_COMPUTE_HOST_DEVICE
} // namespace std

// printf
#if !defined(FLOOR_COMPUTE_HOST_DEVICE)
#include <cstdio>
#else
// only printf and puts are no supported (resolved at load time)
extern "C" int printf(const char* __restrict format, ...);
extern "C" int puts(const char* __restrict str);
#endif

// already need this here
#include <floor/math/vector_lib.hpp>

// id handling
#include <floor/compute/device/host_id.hpp>

floor_inline_always const_func static uint32_t get_global_id(uint32_t dim) {
#if defined(FLOOR_DEBUG) && !defined(FLOOR_COMPUTE_HOST_DEVICE)
	if(dim >= floor_work_dim) return 0;
#endif
	return floor_global_idx[dim];
}
floor_inline_always const_func static uint32_t get_global_size(uint32_t dim) {
#if defined(FLOOR_DEBUG) && !defined(FLOOR_COMPUTE_HOST_DEVICE)
	if(dim >= floor_work_dim) return 1;
#endif
	return floor_global_work_size[dim];
}
floor_inline_always const_func static uint32_t get_local_id(uint32_t dim) {
#if defined(FLOOR_DEBUG) && !defined(FLOOR_COMPUTE_HOST_DEVICE)
	if(dim >= floor_work_dim) return 0;
#endif
	return floor_local_idx[dim];
}
floor_inline_always const_func static uint32_t get_local_size(uint32_t dim) {
#if defined(FLOOR_DEBUG) && !defined(FLOOR_COMPUTE_HOST_DEVICE)
	if(dim >= floor_work_dim) return 1;
#endif
	return floor_local_work_size[dim];
}
floor_inline_always const_func static uint32_t get_group_id(uint32_t dim) {
#if defined(FLOOR_DEBUG) && !defined(FLOOR_COMPUTE_HOST_DEVICE)
	if(dim >= floor_work_dim) return 0;
#endif
	return floor_group_idx[dim];
}
floor_inline_always const_func static uint32_t get_group_size(uint32_t dim) {
#if defined(FLOOR_DEBUG) && !defined(FLOOR_COMPUTE_HOST_DEVICE)
	if(dim >= floor_work_dim) return 1;
#endif
	return floor_group_size[dim];
}
floor_inline_always const_func static uint32_t get_work_dim() {
	return floor_work_dim;
}

// barrier and mem_fence functionality (NOTE: implemented in host_kernel.cpp)
#if !defined(FLOOR_COMPUTE_HOST_DEVICE)
void global_barrier();
void global_mem_fence();
void global_read_mem_fence();
void global_write_mem_fence();
void local_barrier();
void local_mem_fence();
void local_read_mem_fence();
void local_write_mem_fence();
void barrier();

void image_barrier();
void image_mem_fence();
void image_read_mem_fence();
void image_write_mem_fence();
#else

extern "C" {
// host-compute device handling is slightly different:
// since all barriers are identical in function, forward everything to a single barrier function
void host_compute_device_barrier() __attribute__((noduplicate, FLOOR_COMPUTE_HOST_CALLING_CONV));

#pragma clang attribute push (__attribute__((noduplicate)), apply_to=function)
#pragma clang attribute push (__attribute__((internal_linkage)), apply_to=function)
#pragma clang attribute push (__attribute__((weakref("host_compute_device_barrier"))), apply_to=function)

void global_barrier();
void global_mem_fence();
void global_read_mem_fence();
void global_write_mem_fence();

void local_barrier();
void local_mem_fence();
void local_read_mem_fence();
void local_write_mem_fence();

void barrier();

void image_barrier();
void image_mem_fence();
void image_read_mem_fence();
void image_write_mem_fence();

#pragma clang attribute pop
#pragma clang attribute pop
#pragma clang attribute pop
}

#endif

#if !defined(FLOOR_COMPUTE_HOST_DEVICE) // host-only (host-device deals with local memory differently)
// local memory management (NOTE: implemented in host_kernel.cpp)
uint8_t* __attribute__((aligned(1024))) floor_requisition_local_memory(const size_t size, uint32_t& offset) noexcept;
#endif

// tessellation

// TODO: implement this
template <typename point_data_t>
class host_patch_control_point {
public:
	size_t size() const {
		return 0;
	}
	
	point_data_t operator[](const size_t idx floor_unused) const {
		return {};
	}
	
protected:
	
};

#endif

#endif
