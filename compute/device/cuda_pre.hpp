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

#ifndef __FLOOR_COMPUTE_DEVICE_CUDA_PRE_HPP__
#define __FLOOR_COMPUTE_DEVICE_CUDA_PRE_HPP__

#if defined(FLOOR_COMPUTE_CUDA)

//
#define kernel extern "C" __attribute__((compute_kernel))

// map address space keywords
#define global
#define local __attribute__((local_cuda))
#define constant __attribute__((constant_cuda))

// misc types
typedef char int8_t;
typedef short int int16_t;
typedef int int32_t;
typedef long long int int64_t;
typedef unsigned char uint8_t;
typedef unsigned short int uint16_t;
typedef unsigned int uint32_t;
typedef unsigned long long int uint64_t;
typedef __SIZE_TYPE__ size_t;
typedef int64_t ssize_t;

typedef __SIZE_TYPE__ uintptr_t;
typedef __PTRDIFF_TYPE__ intptr_t;
typedef __PTRDIFF_TYPE__ ptrdiff_t;

#endif

#endif
