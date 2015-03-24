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

#ifndef __FLOOR_COMPUTE_DEVICE_CUDA_PRE_HPP__
#define __FLOOR_COMPUTE_DEVICE_CUDA_PRE_HPP__

#if defined(FLOOR_COMPUTE_CUDA)

//
#define kernel extern "C" __attribute__((cuda_kernel))

// map address space keywords
#define global
#define local __attribute__((cuda_local))
#define constant __attribute__((cuda_constant))

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

#endif

#endif
