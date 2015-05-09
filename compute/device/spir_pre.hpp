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

#ifndef __FLOOR_COMPUTE_DEVICE_SPIR_PRE_HPP__
#define __FLOOR_COMPUTE_DEVICE_SPIR_PRE_HPP__

#if defined(FLOOR_COMPUTE_SPIR)

#if !defined(FLOOR_COMPUTE_NO_DOUBLE)
#pragma OPENCL EXTENSION cl_khr_fp64 : enable
#endif

// misc types
typedef char int8_t;
typedef short int int16_t;
typedef int int32_t;
typedef long int int64_t;
typedef unsigned char uint8_t;
typedef unsigned short int uint16_t;
typedef unsigned int uint32_t;
typedef unsigned long int uint64_t;

typedef unsigned char uchar;
typedef unsigned short int ushort;
typedef unsigned int uint;
typedef unsigned long int ulong;

#if defined(__SPIR32__)
typedef uint size_t;
typedef int ssize_t;
#elif defined (__SPIR64__)
typedef unsigned long int size_t;
typedef long int ssize_t;
#endif

// opencl/spir vector types
typedef char spir_char2 __attribute__((ext_vector_type(2)));
typedef char spir_char3 __attribute__((ext_vector_type(3)));
typedef char spir_char4 __attribute__((ext_vector_type(4)));
typedef uchar spir_uchar2 __attribute__((ext_vector_type(2)));
typedef uchar spir_uchar3 __attribute__((ext_vector_type(3)));
typedef uchar spir_uchar4 __attribute__((ext_vector_type(4)));
typedef short spir_short2 __attribute__((ext_vector_type(2)));
typedef short spir_short3 __attribute__((ext_vector_type(3)));
typedef short spir_short4 __attribute__((ext_vector_type(4)));
typedef ushort spir_ushort2 __attribute__((ext_vector_type(2)));
typedef ushort spir_ushort3 __attribute__((ext_vector_type(3)));
typedef ushort spir_ushort4 __attribute__((ext_vector_type(4)));
typedef int spir_int2 __attribute__((ext_vector_type(2)));
typedef int spir_int3 __attribute__((ext_vector_type(3)));
typedef int spir_int4 __attribute__((ext_vector_type(4)));
typedef uint spir_uint2 __attribute__((ext_vector_type(2)));
typedef uint spir_uint3 __attribute__((ext_vector_type(3)));
typedef uint spir_uint4 __attribute__((ext_vector_type(4)));
typedef long spir_long2 __attribute__((ext_vector_type(2)));
typedef long spir_long3 __attribute__((ext_vector_type(3)));
typedef long spir_long4 __attribute__((ext_vector_type(4)));
typedef ulong spir_ulong2 __attribute__((ext_vector_type(2)));
typedef ulong spir_ulong3 __attribute__((ext_vector_type(3)));
typedef ulong spir_ulong4 __attribute__((ext_vector_type(4)));
typedef float spir_float2 __attribute__((ext_vector_type(2)));
typedef float spir_float3 __attribute__((ext_vector_type(3)));
typedef float spir_float4 __attribute__((ext_vector_type(4)));
typedef half spir_half2 __attribute__((ext_vector_type(2)));
typedef half spir_half3 __attribute__((ext_vector_type(3)));
typedef half spir_half4 __attribute__((ext_vector_type(4)));
typedef double spir_double2 __attribute__((ext_vector_type(2)));
typedef double spir_double3 __attribute__((ext_vector_type(3)));
typedef double spir_double4 __attribute__((ext_vector_type(4)));

// NOTE: I purposefully didn't enable these as aliases in clang,
// so that they can be properly redirected on any other target (cuda/metal/host)
// -> need to add simple macro aliases here
#define global __attribute__((opencl_global))
#define constant __attribute__((opencl_constant))
#define local __attribute__((opencl_local))

#if !defined(FLOOR_COMPUTE_APPLECL)
// abuse the section attribute for now, because clang/llvm won't emit kernel functions with "spir_kernel" calling convention
#define kernel extern "C" __kernel __attribute__((section("spir_kernel")))
#else
// applecl doesn't expect a specific calling convention
#define kernel extern "C" __kernel
#endif

#endif

#endif
