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

#if defined(FLOOR_DEVICE_CUDA)

//
#define kernel_1d(... /* x */) extern "C" __attribute__((compute_kernel, kernel_dim(1), kernel_work_group_size(__VA_ARGS__)))
#define kernel_2d(... /* x, y */) extern "C" __attribute__((compute_kernel, kernel_dim(2), kernel_work_group_size(__VA_ARGS__)))
#define kernel_3d(... /* x, y, z */) extern "C" __attribute__((compute_kernel, kernel_dim(3), kernel_work_group_size(__VA_ARGS__)))
#define kernel_1d_simd(req_simd_width, ... /* x */) extern "C" __attribute__((compute_kernel, kernel_dim(1), kernel_simd_width(req_simd_width), kernel_work_group_size(__VA_ARGS__)))
#define kernel_2d_simd(req_simd_width, ... /* x, y */) extern "C" __attribute__((compute_kernel, kernel_dim(2), kernel_simd_width(req_simd_width), kernel_work_group_size(__VA_ARGS__)))
#define kernel_3d_simd(req_simd_width, ... /* x, y, z */) extern "C" __attribute__((compute_kernel, kernel_dim(3), kernel_simd_width(req_simd_width), kernel_work_group_size(__VA_ARGS__)))
#define kernel kernel_1d()

// map address space keywords
#define global
#define local __attribute__((local_cuda))
#define constant __attribute__((constant_cuda))

// misc types
using int8_t = char;
using int16_t = short int;
using int32_t = int;
using int64_t = long long int;
using uint8_t = unsigned char;
using uint16_t = unsigned short int;
using uint32_t = unsigned int;
using uint64_t = unsigned long long int;
using size_t = __SIZE_TYPE__;
using ssize_t = int64_t;

using uintptr_t = __SIZE_TYPE__;
using intptr_t = __PTRDIFF_TYPE__;
using ptrdiff_t = __PTRDIFF_TYPE__;

#endif
