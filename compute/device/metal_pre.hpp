/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2022 Florian Ziesche
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
#pragma OPENCL EXTENSION cl_khr_fp16 : enable
#pragma OPENCL EXTENSION cl_khr_gl_msaa_sharing : enable

#define global __attribute__((global_as))
#define constant __attribute__((constant_as))
#define local __attribute__((local_as))
#define kernel extern "C" __attribute__((compute_kernel))
#define vertex extern "C" __attribute__((vertex_shader))
#define fragment extern "C" __attribute__((fragment_shader))
#define tessellation_control extern "C" __attribute__((tessellation_control_shader))
#define tessellation_evaluation extern "C" __attribute__((tessellation_evaluation_shader))

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
typedef __PTRDIFF_TYPE__ ptrdiff_t;

// memory and synchronization scopes
// note that the inconsistencies are intentional, apparently apple isn't sure about this either,
// cl_kernel.h and metal_compute/metal_atomic are doing the exact opposite. it also seems like that
// these values don't actually make a difference!? (this is to be expected on nvidia h/w, but not on powervr or intel?)
// -> will be using metal values now, as this is hopefully better tested than opencl
#define FLOOR_METAL_SYNC_SCOPE_GLOBAL 2
#define FLOOR_METAL_SYNC_SCOPE_LOCAL 1

#define FLOOR_METAL_MEM_SCOPE_NONE 0
#define FLOOR_METAL_MEM_SCOPE_GLOBAL 1
#define FLOOR_METAL_MEM_SCOPE_LOCAL 2
#define FLOOR_METAL_MEM_SCOPE_ALL 3
#define FLOOR_METAL_MEM_SCOPE_TEXTURE 4
#define FLOOR_METAL_MEM_SCOPE_LOCAL_IMAGE_BLOCK 8

#endif

#endif
