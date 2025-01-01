/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2025 Florian Ziesche
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

#pragma once

#if defined(FLOOR_COMPUTE_METAL)

#pragma OPENCL EXTENSION cl_khr_fp64 : enable
#pragma OPENCL EXTENSION cl_khr_fp16 : enable
#pragma OPENCL EXTENSION cl_khr_gl_msaa_sharing : enable

#define global __attribute__((global_as))
#define constant __attribute__((constant_as))
#define local __attribute__((local_as))
#define kernel_1d(... /* x */) extern "C" __attribute__((compute_kernel, kernel_dim(1), kernel_work_group_size(__VA_ARGS__)))
#define kernel_2d(... /* x, y */) extern "C" __attribute__((compute_kernel, kernel_dim(2), kernel_work_group_size(__VA_ARGS__)))
#define kernel_3d(... /* x, y, z */) extern "C" __attribute__((compute_kernel, kernel_dim(3), kernel_work_group_size(__VA_ARGS__)))
#define kernel kernel_1d()
#define vertex extern "C" __attribute__((vertex_shader))
#define fragment extern "C" __attribute__((fragment_shader))
#define tessellation_control extern "C" __attribute__((tessellation_control_shader, kernel_dim(1)))
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
#define FLOOR_METAL_SYNC_SCOPE_LOCAL 1
#define FLOOR_METAL_SYNC_SCOPE_GLOBAL 2
#define FLOOR_METAL_SYNC_SCOPE_SUB_GROUP 4

#define FLOOR_METAL_MEM_FLAGS_NONE 0
#define FLOOR_METAL_MEM_FLAGS_GLOBAL 1
#define FLOOR_METAL_MEM_FLAGS_LOCAL 2
#define FLOOR_METAL_MEM_FLAGS_ALL 3
#define FLOOR_METAL_MEM_FLAGS_TEXTURE 4
#define FLOOR_METAL_MEM_FLAGS_LOCAL_IMAGE_BLOCK 8
#define FLOOR_METAL_MEM_FLAGS_OBJECT_DATA 16

#endif
