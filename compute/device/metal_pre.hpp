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

#ifndef __FLOOR_COMPUTE_DEVICE_METAL_PRE_HPP__
#define __FLOOR_COMPUTE_DEVICE_METAL_PRE_HPP__

#if defined(FLOOR_COMPUTE_METAL)

#pragma OPENCL EXTENSION cl_khr_fp64 : enable

#define global __attribute__((opencl_global))
#define constant __attribute__((opencl_constant))
#define local __attribute__((opencl_local))
#define kernel extern "C" __kernel

#define metal_func inline __attribute__((always_inline))

// misc types
typedef char int8_t;
typedef short int int16_t;
typedef int int32_t;
typedef long int int64_t;
typedef unsigned char uint8_t;
typedef unsigned short int uint16_t;
typedef unsigned int uint32_t;
typedef unsigned long int uint64_t;

typedef __SIZE_TYPE__ size_t;
typedef __PTRDIFF_TYPE__ ssize_t;
typedef __SIZE_TYPE__ uintptr_t;
typedef __PTRDIFF_TYPE__ intptr_t;

// NOTE: it would appear that either cl_kernel.h or metal_compute has a bug (global and local are mismatched)
//       -> will be assuming that mem_flags: 0 = none, 1 = global, 2 = local, 3 = global + local
// NOTE: scope: 2 = work-group, 3 = device
#define FLOOR_METAL_SCOPE_GLOBAL 3
#define FLOOR_METAL_SCOPE_LOCAL 2

#endif

#endif
