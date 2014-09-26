/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2014 Florian Ziesche
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

#ifndef __FLOOR_COMPUTE_SUPPORT_HPP__
#define __FLOOR_COMPUTE_SUPPORT_HPP__

#if defined(__CUDA_CLANG__) && defined(__CUDA_CLANG_PREPROCESS__)

// kill opencl style "global" keywords
#define global

// also kill this (used to workaround global var address space issues in opencl)
#define opencl_constant

// TODO: properly do this
#define get_global_id(dim) (bid_x * bdim_x + tid_x)

// have some special magic:
// NOTE: this should be compiled with at least -O1, otherwise dead code won't be eliminated
// NOTE: there is sadly no other way of doing this (short of pre-preprocessing the source code)
struct __special_reg { int x, y, z; };

#define tid_x __builtin_ptx_read_tid_x()
#define tid_y __builtin_ptx_read_tid_y()
#define tid_z __builtin_ptx_read_tid_z()
__attribute__((device, always_inline, flatten)) static inline __special_reg __get_thread_idx() {
	return __special_reg { tid_x, tid_y, tid_z };
}
#define threadIdx __get_thread_idx()

#define ctaid_x __builtin_ptx_read_ctaid_x()
#define ctaid_y __builtin_ptx_read_ctaid_y()
#define ctaid_z __builtin_ptx_read_ctaid_z()
__attribute__((device, always_inline, flatten)) static inline __special_reg __get_block_idx() {
	return __special_reg { ctaid_x, ctaid_y, ctaid_z };
}
#define blockIdx __get_block_idx()

#define ntid_x __builtin_ptx_read_ntid_x()
#define ntid_y __builtin_ptx_read_ntid_y()
#define ntid_z __builtin_ptx_read_ntid_z()
__attribute__((device, always_inline, flatten)) static inline __special_reg __get_block_dim() {
	return __special_reg { ntid_x, ntid_y, ntid_z };
}
#define blockDim __get_block_dim()

#define nctaid_x __builtin_ptx_read_nctaid_x()
#define nctaid_y __builtin_ptx_read_nctaid_y()
#define nctaid_z __builtin_ptx_read_nctaid_z()
__attribute__((device, always_inline, flatten)) static inline __special_reg __get_grid_dim() {
	return __special_reg { nctaid_x, nctaid_y, nctaid_z };
}
#define gridDim __get_grid_dim()

#define laneId __builtin_ptx_read_laneid()
#define warpId __builtin_ptx_read_warpid()
#define warpSize __builtin_ptx_read_nwarpid()

// misc (not directly defined by cuda?)
#define smId __builtin_ptx_read_smid()
#define smDim __builtin_ptx_read_nsmid()
#define gridId __builtin_ptx_read_gridid()

#define lanemask_eq __builtin_ptx_read_lanemask_eq()
#define lanemask_le __builtin_ptx_read_lanemask_le()
#define lanemask_lt __builtin_ptx_read_lanemask_lt()
#define lanemask_ge __builtin_ptx_read_lanemask_ge()
#define lanemask_gt __builtin_ptx_read_lanemask_gt()

#define ptx_clock __builtin_ptx_read_clock()
#define ptx_clock64 __builtin_ptx_read_clock64()

// some aliases for easier use:
// (tid_* already defined above)
#define bid_x ctaid_x
#define bid_y ctaid_y
#define bid_z ctaid_z
#define bdim_x ntid_x
#define bdim_y ntid_y
#define bdim_z ntid_z
#define gdim_x nctaid_x
#define gdim_y nctaid_y
#define gdim_z nctaid_z
#define lane_id laneId
#define warp_id warpId
#define warp_size warpSize
#define sm_id smId
#define sm_dim smDim
#define grid_id gridId

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

#elif defined(__CUDA_CLANG__) && !defined(__CUDA_CLANG_PREPROCESS__)
//
#define local __attribute__((shared))
#define constant __attribute__((constant))

#elif defined(__SPIR_CLANG__)
// TODO: !
//#include "opencl_spir.h"
#pragma OPENCL EXTENSION cl_khr_fp64 : enable

// misc types
typedef char int8_t;
typedef short int int16_t;
typedef int int32_t;
typedef long long int int64_t;
typedef unsigned char uint8_t;
typedef unsigned short int uint16_t;
typedef unsigned int uint32_t;
typedef unsigned long int uint64_t;

typedef unsigned short int ushort;
typedef unsigned int uint;
typedef unsigned long int ulong;

#if defined(__SPIR32__)
typedef uint size_t;
typedef int ssize_t;
#elif defined (__SPIR64__)
typedef unsigned long int size_t;
typedef long ssize_t;
#endif

size_t __attribute__((const)) get_global_id(uint dimindx);

// NOTE: I purposefully didn't enable these as aliases in clang,
// so that they can be properly redirected in cuda mode
// -> need to add simple macro aliases here
//#define private __private
//#define global __global
// there are some weird issues with using "__global" in clang,
// but simply using address_space attributes work around these issues
#define global __attribute__((address_space(1)))
#define constant __constant
#define local __local
#define kernel __kernel
#define opencl_constant constant

#endif

#if defined(__CUDA_CLANG_PREPROCESS__) || defined(__SPIR_CLANG__)
// libc++ stl functionality without (most of) the baggage
#include <utility>

_LIBCPP_BEGIN_NAMESPACE_STD

// <array> replacement
template <class data_type, size_t array_size>
struct _LIBCPP_TYPE_VIS_ONLY array {
	data_type elems[array_size > 0 ? array_size : 1];

	constexpr size_t size() const {
		return array_size;
	}

	constexpr data_type& operator[](const size_t& index) { return elems[index]; }
	constexpr const data_type& operator[](const size_t& index) const { return elems[index]; }
};

// std::min / std::max replacements
template <class data_type>
constexpr data_type min(const data_type& lhs, const data_type& rhs) {
	return (lhs < rhs ? lhs : rhs);
}
template <class data_type>
constexpr data_type max(const data_type& lhs, const data_type& rhs) {
	return (lhs > rhs ? lhs : rhs);
}

_LIBCPP_END_NAMESPACE_STD
#endif

#endif
