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
	
	// int math functions
	floor_inline_always int16_t abs(int16_t a) {
		int16_t ret;
		asm("abs.s16 %0, %1;" : "=r"(ret) : "r"(a));
		return ret;
	}
	floor_inline_always int32_t abs(int32_t a) {
		int32_t ret;
		asm("abs.s32 %0, %1;" : "=r"(ret) : "r"(a));
		return ret;
	}
	floor_inline_always int64_t abs(int64_t a) {
		int64_t ret;
		asm("abs.s64 %0, %1;" : "=l"(ret) : "l"(a));
		return ret;
	}
	
}

// provided by cuda runtime
extern "C" {
	// NOTE: there is no va_list support in llvm/nvptx, not even via __builtin_*, so emulate it manually via void* -> printf
	extern int vprintf(const char* format, void* vlist);
};

// floating point types are always cast to double
template <typename T, enable_if_t<is_floating_point<decay_t<T>>::value>* = nullptr>
constexpr size_t printf_arg_size() { return 8; }
// integral types < 4 bytes are always cast to a 4 byte integral type
template <typename T, enable_if_t<is_integral<decay_t<T>>::value && sizeof(decay_t<T>) <= 4>* = nullptr>
constexpr size_t printf_arg_size() { return 4; }
// remaining 8 byte integral types
template <typename T, enable_if_t<is_integral<decay_t<T>>::value && sizeof(decay_t<T>) == 8>* = nullptr>
constexpr size_t printf_arg_size() { return 8; }
// pointers are always 8 bytes (64-bit support only), this includes any kind of char* or char[]
template <typename T, enable_if_t<is_pointer<decay_t<T>>::value>* = nullptr>
constexpr size_t printf_arg_size() { return 8; }

// computes the total size of a printf argument pack (sum of sizeof of each type) + necessary alignment bytes/sizes
template <typename... Args>
constexpr size_t printf_args_total_size() {
	constexpr const size_t sizes[sizeof...(Args)] {
		printf_arg_size<Args>()...
	};
	size_t sum = 0;
	for(size_t i = 0, count = sizeof...(Args); i < count; ++i) {
		sum += sizes[i];
		
		// align if this type is 8 bytes large and the previous one was 4 bytes
		if(i > 0 && sizes[i] == 8 && sizes[i - 1] == 4) sum += 4;
	}
	return sum;
}

// dummy function needed to expand #Args... function calls
template <typename... Args> floor_inline_always constexpr static void printf_args_apply(Args&&...) {}

// casts and copies the printf argument to the correct "va_list"/buffer position + handles necessary alignment
template <typename T, enable_if_t<is_floating_point<decay_t<T>>::value>* = nullptr>
floor_inline_always static int printf_arg_copy(T&& arg, uint8_t** args_buf) {
	if(size_t(*args_buf) % 8ull != 0) *args_buf += 4;
	*(double*)*args_buf = (double)arg;
	*args_buf += 8;
	return 0;
}
template <typename T, enable_if_t<is_integral<decay_t<T>>::value && sizeof(decay_t<T>) <= 4>* = nullptr>
floor_inline_always static int printf_arg_copy(T&& arg, uint8_t** args_buf) {
	typedef conditional_t<is_signed<decay_t<T>>::value, int32_t, uint32_t> int_storage_type;
	*(int_storage_type*)*args_buf = (int_storage_type)arg;
	*args_buf += 4;
	return 0;
}
template <typename T, enable_if_t<is_integral<decay_t<T>>::value && sizeof(decay_t<T>) == 8>* = nullptr>
floor_inline_always static int printf_arg_copy(T&& arg, uint8_t** args_buf) {
	if(size_t(*args_buf) % 8ull != 0) *args_buf += 4;
	*(decay_t<T>*)*args_buf = arg;
	*args_buf += 8;
	return 0;
}
template <typename T, enable_if_t<is_pointer<T>::value>* = nullptr>
floor_inline_always static int printf_arg_copy(T&& arg, uint8_t** args_buf) {
	if(size_t(*args_buf) % 8ull != 0) *args_buf += 4;
	*(decay_t<T>*)*args_buf = arg;
	*args_buf += 8;
	return 0;
}
floor_inline_always static int printf_arg_copy(const char* arg, uint8_t** args_buf) {
	if(size_t(*args_buf) % 8ull != 0) *args_buf += 4;
	*(const char**)*args_buf = arg;
	*args_buf += 8;
	return 0;
}

// forwarder and handler of printf_arg_copy (we need to get and specialize for the actual storage type)
template <typename T> floor_inline_always static int printf_handle_arg(T&& arg, uint8_t** args_buf) {
	printf_arg_copy(forward<T>(arg), args_buf);
	return 0;
}

// printf, this builds a local buffer, copies all arguments into it and calls vprintf, which is provided by the hardware
template <typename... Args>
static int printf(const char* format, Args&&... args) {
	alignas(8) uint8_t args_buf[printf_args_total_size<Args...>()];
	uint8_t* args_buf_ptr = &args_buf[0];
	printf_args_apply(printf_handle_arg(args, &args_buf_ptr)...);
	return vprintf(format, &args_buf);
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

// barrier and mem_fence functionality
static floor_inline_always void global_barrier() {
	__syncthreads();
}
static floor_inline_always void global_mem_fence() {
	__nvvm_membar_cta();
}
static floor_inline_always void global_read_mem_fence() {
	__nvvm_membar_cta();
}
static floor_inline_always void global_write_mem_fence() {
	__nvvm_membar_cta();
}
static floor_inline_always void local_barrier() {
	__syncthreads();
}
static floor_inline_always void local_mem_fence() {
	__nvvm_membar_cta();
}
static floor_inline_always void local_read_mem_fence() {
	__nvvm_membar_cta();
}
static floor_inline_always void local_write_mem_fence() {
	__nvvm_membar_cta();
}

// atomics
#include <floor/compute/device/cuda_atomic.hpp>

// done
#undef FLOOR_CUDA_DIM0
#undef FLOOR_CUDA_DIM1
#undef FLOOR_CUDA_DIM2
#undef FLOOR_CUDA_INVALID
#undef FLOOR_CUDA_DIM_RT

#endif

#endif
