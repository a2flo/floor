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

#if defined(FLOOR_COMPUTE_VULKAN)

// still need to include opencl stuff
#include <floor/compute/device/opencl_pre.hpp>

// 16-bit and 64-bit support is required
#pragma OPENCL EXTENSION vk_capability_int16 : enable
#pragma OPENCL EXTENSION vk_capability_int64 : enable

// enable optional capabilities
#if defined(FLOOR_COMPUTE_INFO_VULKAN_HAS_FLOAT16_SUPPORT_1)
#pragma OPENCL EXTENSION vk_capability_float16 : enable
#endif

#if !defined(FLOOR_COMPUTE_NO_DOUBLE)
#pragma OPENCL EXTENSION vk_capability_float64 : enable
#endif

// multi-view is always enabled
#pragma OPENCL EXTENSION vk_capability_multiview : enable

// Vulkan helper function to perform int32_t/uint32_t <-> float bitcasts on the SPIR-V side
int32_t floor_bitcast_f32_to_i32(float) asm("floor.bitcast.f32.i32");
uint32_t floor_bitcast_f32_to_u32(float) asm("floor.bitcast.f32.u32");
float floor_bitcast_i32_to_f32(int32_t) asm("floor.bitcast.i32.f32");
float floor_bitcast_u32_to_f32(uint32_t) asm("floor.bitcast.u32.f32");

#endif
