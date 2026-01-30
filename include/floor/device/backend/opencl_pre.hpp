/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2026 Florian Ziesche
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

#if defined(FLOOR_DEVICE_OPENCL) || defined(FLOOR_DEVICE_VULKAN)

#if !defined(__SPIR64__)
#error "only 64-bit device compilation is supported"
#endif

#pragma OPENCL EXTENSION cl_khr_fp64 : enable
#pragma OPENCL EXTENSION cl_khr_fp16 : enable
#pragma OPENCL EXTENSION cl_khr_gl_msaa_sharing : enable

#if defined(FLOOR_DEVICE_INFO_HAS_IMAGE_MIPMAP_SUPPORT_1)
#pragma OPENCL EXTENSION cl_khr_mipmap_image : enable
#endif

#if defined(FLOOR_DEVICE_INFO_HAS_IMAGE_MIPMAP_WRITE_SUPPORT_1)
#pragma OPENCL EXTENSION cl_khr_mipmap_image_writes : enable
#endif

// misc types
using int8_t = char;
using int16_t = short int;
using int32_t = int;
using int64_t = long int;
using uint8_t = unsigned char;
using uint16_t = unsigned short int;
using uint32_t = unsigned int;
using uint64_t = unsigned long int;

using uchar = unsigned char;
using ushort = unsigned short int;
using uint = unsigned int;
using ulong = unsigned long int;

using size_t = unsigned long int;
using ssize_t = long int;

using uintptr_t = __SIZE_TYPE__;
using intptr_t = __PTRDIFF_TYPE__;
using ptrdiff_t = __PTRDIFF_TYPE__;

// NOTE: I purposefully didn't enable these as aliases in clang,
// so that they can be properly redirected on any other target (cuda/metal/host)
// -> need to add simple macro aliases here
#define global __attribute__((global_as))
#define constant __attribute__((constant_as))
#define local __attribute__((local_as))
#define kernel_1d(... /* x */) extern "C" __attribute__((compute_kernel, kernel_dim(1), kernel_work_group_size(__VA_ARGS__)))
#define kernel_2d(... /* x, y */) extern "C" __attribute__((compute_kernel, kernel_dim(2), kernel_work_group_size(__VA_ARGS__)))
#define kernel_3d(... /* x, y, z */) extern "C" __attribute__((compute_kernel, kernel_dim(3), kernel_work_group_size(__VA_ARGS__)))
#define kernel_1d_simd(req_simd_width, ... /* x */) extern "C" __attribute__((compute_kernel, kernel_dim(1), kernel_simd_width(req_simd_width), kernel_work_group_size(__VA_ARGS__)))
#define kernel_2d_simd(req_simd_width, ... /* x, y */) extern "C" __attribute__((compute_kernel, kernel_dim(2), kernel_simd_width(req_simd_width), kernel_work_group_size(__VA_ARGS__)))
#define kernel_3d_simd(req_simd_width, ... /* x, y, z */) extern "C" __attribute__((compute_kernel, kernel_dim(3), kernel_simd_width(req_simd_width), kernel_work_group_size(__VA_ARGS__)))
#define kernel kernel_1d()
#if defined(FLOOR_DEVICE_VULKAN)
#define vertex extern "C" __attribute__((vertex_shader))
#define fragment extern "C" __attribute__((fragment_shader))
#define tessellation_control extern "C" __attribute__((tessellation_control_shader, kernel_dim(1)))
#define tessellation_evaluation extern "C" __attribute__((tessellation_evaluation_shader))
#endif

#endif
