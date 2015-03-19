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

#ifndef __FLOOR_COMPUTE_DEVICE_CUDA_HPP__
#define __FLOOR_COMPUTE_DEVICE_CUDA_HPP__

#if defined(FLOOR_COMPUTE_CUDA)

//
#define kernel extern "C" __attribute__((cuda_kernel))

// map address space keywords
#define global
#define local __attribute__((cuda_local))
#define constant __attribute__((cuda_constant))

#define cuda_lane_id __builtin_ptx_read_laneid()
#define cuda_warp_id __builtin_ptx_read_warpid()
#define cuda_warp_size __builtin_ptx_read_nwarpid()

// misc (not directly defined by cuda?)
#define cuda_sm_id __builtin_ptx_read_smid()
#define cuda_sm_dim __builtin_ptx_read_nsmid()
#define cuda_grid_id __builtin_ptx_read_gridid()

#define cuda_lanemask_eq __builtin_ptx_read_lanemask_eq()
#define cuda_lanemask_le __builtin_ptx_read_lanemask_le()
#define cuda_lanemask_lt __builtin_ptx_read_lanemask_lt()
#define cuda_lanemask_ge __builtin_ptx_read_lanemask_ge()
#define cuda_lanemask_gt __builtin_ptx_read_lanemask_gt()

#define cuda_clock __builtin_ptx_read_clock()
#define cuda_clock64 __builtin_ptx_read_clock64()

// provided by libcudart:
/*extern "C" {
	extern __device__ __device_builtin__ int printf(const char*, ...);
};*/

// misc types
typedef __signed char int8_t;
typedef short int int16_t;
typedef int int32_t;
typedef long long int int64_t;
typedef unsigned char uint8_t;
typedef unsigned short int uint16_t;
typedef unsigned int uint32_t;
typedef unsigned long long int uint64_t;

// TODO: arch size support
#if defined(PLATFORM_X32)
typedef uint32_t size_t;
typedef int32_t ssize_t;
#elif defined(PLATFORM_X64)
typedef uint64_t size_t;
typedef int64_t ssize_t;
#endif

namespace std {
	// float math functions
	float sqrt(float a) { return __nvvm_sqrt_rz_ftz_f(a); }
	float rsqrt(float a) { return __nvvm_rsqrt_approx_ftz_f(a); }
	float fmod(float x, float y) { return x - y * __nvvm_trunc_ftz_f(x / y); }
	float fabs(float a) { return __nvvm_fabs_ftz_f(a); }
	float floor(float a) { return __nvvm_floor_ftz_f(a); }
	float ceil(float a) { return __nvvm_ceil_ftz_f(a); }
	float round(float a) { return __nvvm_round_ftz_f(a); }
	float trunc(float a) { return __nvvm_trunc_ftz_f(a); }
	float rint(float a) { return __nvvm_trunc_ftz_f(a); }
	
	float sin(float a) { return __nvvm_sin_approx_ftz_f(a); }
	float cos(float a) { return __nvvm_cos_approx_ftz_f(a); }
	float tan(float a) { return __nvvm_sin_approx_ftz_f(a) / __nvvm_cos_approx_ftz_f(a); }
	// TODO: not supported in h/w, write proper rt computation
	float asin(float a) { return 0.0f; }
	float acos(float a) { return 0.0f; }
	float atan(float a) { return 0.0f; }
	float atan2(float y, float x) { return 0.0f; }
	
	float fma(float a, float b, float c) { return __nvvm_fma_rz_ftz_f(a, b, c); }
	float pow(float a, float b) { return __nvvm_ex2_approx_ftz_f(b * __nvvm_lg2_approx_ftz_f(a)); }
	float exp(float a) { return __nvvm_ex2_approx_ftz_f(a * 1.442695041f); } // 2^(x / ln(2))
	float log(float a) { return __nvvm_lg2_approx_ftz_f(a) * 1.442695041f; } // log_e = log_2(x) / log_2(e)
	
	// double math functions
	double sqrt(double a) { return __nvvm_sqrt_rz_d(a); }
	double rsqrt(double a) { return __nvvm_rsqrt_approx_d(a); }
	double fmod(double x, double y) { return x - y * __nvvm_trunc_d(x / y); }
	double fabs(double a) { return __nvvm_fabs_d(a); }
	double floor(double a) { return __nvvm_floor_d(a); }
	double ceil(double a) { return __nvvm_ceil_d(a); }
	double round(double a) { return __nvvm_round_d(a); }
	double trunc(double a) { return __nvvm_trunc_d(a); }
	double rint(double a) { return __nvvm_trunc_d(a); }
	
	// TODO: higher precision sin/cos/tan?
	double sin(double a) { return double(__nvvm_sin_approx_ftz_f(float(a))); }
	double cos(double a) { return double(__nvvm_cos_approx_ftz_f(float(a))); }
	double tan(double a) { return double(__nvvm_sin_approx_ftz_f(float(a))) / double(__nvvm_cos_approx_ftz_f(float(a))); }
	// TODO: not supported in h/w, write proper rt computation
	double asin(double a) { return 0.0f; }
	double acos(double a) { return 0.0f; }
	double atan(double a) { return 0.0f; }
	double atan2(double y, double x) { return 0.0f; }
	
	double fma(double a, double b, double c) { return __nvvm_fma_rz_d(a, b, c); }
	// TODO: even though there are intrinsics for this, there are no double/f64 versions supported in h/w
	double pow(double a, double b) { return double(__nvvm_ex2_approx_ftz_f(float(b) * __nvvm_lg2_approx_ftz_f(float(a)))); }
	double exp(double a) { return double(__nvvm_ex2_approx_ftz_f(float(a) * 1.442695041f)); } // 2^(x / ln(2))
	double log(double a) { return double(__nvvm_lg2_approx_ftz_f(float(a))) * 1.442695041; } // log_e = log_2(x) / log_2(e)
}

// make this a little easier to use
#define FLOOR_CUDA_DIM0 __attribute__((always_inline, flatten, pure, enable_if(dim == 0, "const 0")))
#define FLOOR_CUDA_DIM1 __attribute__((always_inline, flatten, pure, enable_if(dim == 1, "const 1")))
#define FLOOR_CUDA_DIM2 __attribute__((always_inline, flatten, pure, enable_if(dim == 2, "const 2")))
#define FLOOR_CUDA_INVALID __attribute__((always_inline, flatten, pure, enable_if(dim > 2, "invalid dim"), unavailable("invalid dim")))
#define FLOOR_CUDA_DIM_RT __attribute__((always_inline, flatten, pure))

static uint64_t get_global_id(uint32_t dim floor_unused) FLOOR_CUDA_DIM0 {
	return __builtin_ptx_read_ctaid_x() * __builtin_ptx_read_ntid_x() + __builtin_ptx_read_tid_x();
}
static uint64_t get_global_id(uint32_t dim floor_unused) FLOOR_CUDA_DIM1 {
	return __builtin_ptx_read_ctaid_y() * __builtin_ptx_read_ntid_y() + __builtin_ptx_read_tid_y();
}
static uint64_t get_global_id(uint32_t dim floor_unused) FLOOR_CUDA_DIM2 {
	return __builtin_ptx_read_ctaid_z() * __builtin_ptx_read_ntid_z() + __builtin_ptx_read_tid_z();
}
static uint64_t get_global_id(uint32_t dim floor_unused) FLOOR_CUDA_INVALID {
	return 42;
}
static uint64_t get_global_id(uint32_t dim) FLOOR_CUDA_DIM_RT {
	switch(dim) {
		case 0: return __builtin_ptx_read_ctaid_x() * __builtin_ptx_read_ntid_x() + __builtin_ptx_read_tid_x();
		case 1: return __builtin_ptx_read_ctaid_y() * __builtin_ptx_read_ntid_y() + __builtin_ptx_read_tid_y();
		case 2: return __builtin_ptx_read_ctaid_z() * __builtin_ptx_read_ntid_z() + __builtin_ptx_read_tid_z();
		default: return 0;
	}
}
static uint64_t get_global_size(uint32_t dim floor_unused) FLOOR_CUDA_DIM0 {
	return __builtin_ptx_read_nctaid_x() * __builtin_ptx_read_ntid_x();
}
static uint64_t get_global_size(uint32_t dim floor_unused) FLOOR_CUDA_DIM1 {
	return __builtin_ptx_read_nctaid_y() * __builtin_ptx_read_ntid_y();
}
static uint64_t get_global_size(uint32_t dim floor_unused) FLOOR_CUDA_DIM2 {
	return __builtin_ptx_read_nctaid_z() * __builtin_ptx_read_ntid_z();
}
static uint64_t get_global_size(uint32_t dim floor_unused) FLOOR_CUDA_INVALID {
	return 42;
}
static uint64_t get_global_size(uint32_t dim) FLOOR_CUDA_DIM_RT {
	switch(dim) {
		case 0: return __builtin_ptx_read_nctaid_x() * __builtin_ptx_read_ntid_x();
		case 1: return __builtin_ptx_read_nctaid_y() * __builtin_ptx_read_ntid_y();
		case 2: return __builtin_ptx_read_nctaid_z() * __builtin_ptx_read_ntid_z();
		default: return 1;
	}
}

static uint64_t get_local_id(uint32_t dim floor_unused) FLOOR_CUDA_DIM0 {
	return __builtin_ptx_read_tid_x();
}
static uint64_t get_local_id(uint32_t dim floor_unused) FLOOR_CUDA_DIM1 {
	return __builtin_ptx_read_tid_y();
}
static uint64_t get_local_id(uint32_t dim floor_unused) FLOOR_CUDA_DIM2 {
	return __builtin_ptx_read_tid_z();
}
static uint64_t get_local_id(uint32_t dim floor_unused) FLOOR_CUDA_INVALID {
	return 42;
}
static uint64_t get_local_id(uint32_t dim) FLOOR_CUDA_DIM_RT {
	switch(dim) {
		case 0: return __builtin_ptx_read_tid_x();
		case 1: return __builtin_ptx_read_tid_y();
		case 2: return __builtin_ptx_read_tid_z();
		default: return 0;
	}
}

static uint64_t get_local_size(uint32_t dim floor_unused) FLOOR_CUDA_DIM0 {
	return __builtin_ptx_read_ntid_x();
}
static uint64_t get_local_size(uint32_t dim floor_unused) FLOOR_CUDA_DIM1 {
	return __builtin_ptx_read_ntid_y();
}
static uint64_t get_local_size(uint32_t dim floor_unused) FLOOR_CUDA_DIM2 {
	return __builtin_ptx_read_ntid_z();
}
static uint64_t get_local_size(uint32_t dim floor_unused) FLOOR_CUDA_INVALID {
	return 42;
}
static uint64_t get_local_size(uint32_t dim) FLOOR_CUDA_DIM_RT {
	switch(dim) {
		case 0: return __builtin_ptx_read_ntid_x();
		case 1: return __builtin_ptx_read_ntid_y();
		case 2: return __builtin_ptx_read_ntid_z();
		default: return 1;
	}
}

static uint64_t get_group_id(uint32_t dim floor_unused) FLOOR_CUDA_DIM0 {
	return __builtin_ptx_read_ctaid_x();
}
static uint64_t get_group_id(uint32_t dim floor_unused) FLOOR_CUDA_DIM1 {
	return __builtin_ptx_read_ctaid_y();
}
static uint64_t get_group_id(uint32_t dim floor_unused) FLOOR_CUDA_DIM2 {
	return __builtin_ptx_read_ctaid_z();
}
static uint64_t get_group_id(uint32_t dim floor_unused) FLOOR_CUDA_INVALID {
	return 42;
}
static uint64_t get_group_id(uint32_t dim) FLOOR_CUDA_DIM_RT {
	switch(dim) {
		case 0: return __builtin_ptx_read_ctaid_x();
		case 1: return __builtin_ptx_read_ctaid_y();
		case 2: return __builtin_ptx_read_ctaid_z();
		default: return 0;
	}
}

static uint64_t get_num_groups(uint32_t dim floor_unused) FLOOR_CUDA_DIM0 {
	return __builtin_ptx_read_nctaid_x();
}
static uint64_t get_num_groups(uint32_t dim floor_unused) FLOOR_CUDA_DIM1 {
	return __builtin_ptx_read_nctaid_y();
}
static uint64_t get_num_groups(uint32_t dim floor_unused) FLOOR_CUDA_DIM2 {
	return __builtin_ptx_read_nctaid_z();
}
static uint64_t get_num_groups(uint32_t dim floor_unused) FLOOR_CUDA_INVALID {
	return 42;
}
static uint64_t get_num_groups(uint32_t dim) FLOOR_CUDA_DIM_RT {
	switch(dim) {
		case 0: return __builtin_ptx_read_nctaid_x();
		case 1: return __builtin_ptx_read_nctaid_y();
		case 2: return __builtin_ptx_read_nctaid_z();
		default: return 1;
	}
}

static uint32_t get_work_dim() {
	// grid dim (X, Y, Z)
	// -> if Z is 1, must either be 1D or 2D
	if(__builtin_ptx_read_nctaid_z() == 1) {
		// -> if Y is 1 as well, it is 1D, else 2D
		return (__builtin_ptx_read_nctaid_y() == 1 ? 1 : 2);
	}
	// else: -> Z is not 1, must always be 3D
	return 3;
}

//! currently not supported by any compute implementation
static constexpr uint64_t get_global_offset(uint32_t dim floor_unused) {
	return 0;
}

// done
#undef FLOOR_CUDA_DIM0
#undef FLOOR_CUDA_DIM1
#undef FLOOR_CUDA_DIM2
#undef FLOOR_CUDA_INVALID
#undef FLOOR_CUDA_DIM_RT

#endif

#endif
