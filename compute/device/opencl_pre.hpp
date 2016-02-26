/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2016 Florian Ziesche
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

#ifndef __FLOOR_COMPUTE_DEVICE_OPENCL_PRE_HPP__
#define __FLOOR_COMPUTE_DEVICE_OPENCL_PRE_HPP__

#if defined(FLOOR_COMPUTE_OPENCL)

#pragma OPENCL EXTENSION cl_khr_fp64 : enable
#pragma OPENCL EXTENSION cl_khr_fp16 : enable

#if defined(FLOOR_COMPUTE_TOOLCHAIN_VERSION) && (FLOOR_COMPUTE_TOOLCHAIN_VERSION >= 380u)
#pragma OPENCL EXTENSION cl_khr_gl_msaa_sharing : enable
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

// NOTE: I purposefully didn't enable these as aliases in clang,
// so that they can be properly redirected on any other target (cuda/metal/host)
// -> need to add simple macro aliases here
#define global __attribute__((global_as))
#define constant __attribute__((constant_as))
#define local __attribute__((local_as))
#define kernel extern "C" __attribute__((compute_kernel))

// proper function mangling (default to c++ mangling on spir, c on applecl)
#if defined(FLOOR_COMPUTE_APPLECL)
#define opencl_c_func extern "C"
#else
#define opencl_c_func
#endif

#endif

#endif
