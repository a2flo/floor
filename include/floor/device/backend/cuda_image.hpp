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

#if defined(FLOOR_DEVICE_CUDA)

#include <floor/device/backend/cuda_sampler.hpp>

namespace fl::cuda_image {
template <typename scalar_type> using clang_vector_type = __attribute__((ext_vector_type(4))) scalar_type;

//////////////////////////////////////////
// CUDA image function wrappers/forwarders
template <typename scalar_type>
const_func clang_vector_type<scalar_type> read_image(uint64_t tex, IMAGE_TYPE type, clang_int1 coord, uint32_t layer = 0, uint32_t sample = 0, clang_int1 offset = { 0 },
													 int32_t lod_i = 0, float lod_or_bias_f = 0.0f, bool is_lod = false, bool is_lod_float = false, bool is_bias = true,
													 clang_float1 dpdx = { 0.0f }, clang_float1 dpdy = { 0.0f }, bool is_gradient = false,
													 COMPARE_FUNCTION compare_function = COMPARE_FUNCTION::NEVER, float compare_value = 0.0f, bool is_compare = false);

template <typename scalar_type>
const_func clang_vector_type<scalar_type> read_image(uint64_t tex, IMAGE_TYPE type, clang_float1 coord, uint32_t layer = 0, uint32_t sample = 0, clang_int1 offset = { 0 },
													 int32_t lod_i = 0, float lod_or_bias_f = 0.0f, bool is_lod = false, bool is_lod_float = false, bool is_bias = true,
													 clang_float1 dpdx = { 0.0f }, clang_float1 dpdy = { 0.0f }, bool is_gradient = false,
													 COMPARE_FUNCTION compare_function = COMPARE_FUNCTION::NEVER, float compare_value = 0.0f, bool is_compare = false);

template <typename scalar_type>
const_func clang_vector_type<scalar_type> read_image(uint64_t tex, IMAGE_TYPE type, clang_int2 coord, uint32_t layer = 0, uint32_t sample = 0, clang_int2 offset = { 0, 0 },
													 int32_t lod_i = 0, float lod_or_bias_f = 0.0f, bool is_lod = false, bool is_lod_float = false, bool is_bias = true,
													 clang_float2 dpdx = { 0.0f, 0.0f }, clang_float2 dpdy = { 0.0f, 0.0f }, bool is_gradient = false,
													 COMPARE_FUNCTION compare_function = COMPARE_FUNCTION::NEVER, float compare_value = 0.0f, bool is_compare = false);

template <typename scalar_type>
const_func clang_vector_type<scalar_type> read_image(uint64_t tex, IMAGE_TYPE type, clang_float2 coord, uint32_t layer = 0, uint32_t sample = 0, clang_int2 offset = { 0, 0 },
													 int32_t lod_i = 0, float lod_or_bias_f = 0.0f, bool is_lod = false, bool is_lod_float = false, bool is_bias = true,
													 clang_float2 dpdx = { 0.0f, 0.0f }, clang_float2 dpdy = { 0.0f, 0.0f }, bool is_gradient = false,
													 COMPARE_FUNCTION compare_function = COMPARE_FUNCTION::NEVER, float compare_value = 0.0f, bool is_compare = false);

template <typename scalar_type>
const_func clang_vector_type<scalar_type> read_image(uint64_t tex, IMAGE_TYPE type, clang_int3 coord, uint32_t layer = 0, uint32_t sample = 0, clang_int3 offset = { 0, 0, 0 },
													 int32_t lod_i = 0, float lod_or_bias_f = 0.0f, bool is_lod = false, bool is_lod_float = false, bool is_bias = true,
													 clang_float3 dpdx = { 0.0f, 0.0f, 0.0f }, clang_float3 dpdy = { 0.0f, 0.0f, 0.0f }, bool is_gradient = false,
													 COMPARE_FUNCTION compare_function = COMPARE_FUNCTION::NEVER, float compare_value = 0.0f, bool is_compare = false);

template <typename scalar_type>
const_func clang_vector_type<scalar_type> read_image(uint64_t tex, IMAGE_TYPE type, clang_float3 coord, uint32_t layer = 0, uint32_t sample = 0, clang_int3 offset = { 0, 0, 0 },
													 int32_t lod_i = 0, float lod_or_bias_f = 0.0f, bool is_lod = false, bool is_lod_float = false, bool is_bias = true,
													 clang_float3 dpdx = { 0.0f, 0.0f, 0.0f }, clang_float3 dpdy = { 0.0f, 0.0f, 0.0f }, bool is_gradient = false,
													 COMPARE_FUNCTION compare_function = COMPARE_FUNCTION::NEVER, float compare_value = 0.0f, bool is_compare = false);

template <> const_func clang_vector_type<float> read_image(uint64_t tex, IMAGE_TYPE type, clang_int1 coord, uint32_t layer, uint32_t sample, clang_int1 offset,
														   int32_t lod_i, float lod_or_bias_f, bool is_lod, bool is_lod_float, bool is_bias,
														   clang_float1 dpdx, clang_float1 dpdy, bool is_gradient,
														   COMPARE_FUNCTION compare_function, float compare_value, bool is_compare) asm("floor.cuda.read_image.float.i1");
template <> const_func clang_vector_type<float> read_image(uint64_t tex, IMAGE_TYPE type, clang_float1 coord, uint32_t layer, uint32_t sample, clang_int1 offset,
														   int32_t lod_i, float lod_or_bias_f, bool is_lod, bool is_lod_float, bool is_bias,
														   clang_float1 dpdx, clang_float1 dpdy, bool is_gradient,
														   COMPARE_FUNCTION compare_function, float compare_value, bool is_compare) asm("floor.cuda.read_image.float.f1");
template <> const_func clang_vector_type<float> read_image(uint64_t tex, IMAGE_TYPE type, clang_int2 coord, uint32_t layer, uint32_t sample, clang_int2 offset,
														   int32_t lod_i, float lod_or_bias_f, bool is_lod, bool is_lod_float, bool is_bias,
														   clang_float2 dpdx, clang_float2 dpdy, bool is_gradient,
														   COMPARE_FUNCTION compare_function, float compare_value, bool is_compare) asm("floor.cuda.read_image.float.i2");
template <> const_func clang_vector_type<float> read_image(uint64_t tex, IMAGE_TYPE type, clang_float2 coord, uint32_t layer, uint32_t sample, clang_int2 offset,
														   int32_t lod_i, float lod_or_bias_f, bool is_lod, bool is_lod_float, bool is_bias,
														   clang_float2 dpdx, clang_float2 dpdy, bool is_gradient,
														   COMPARE_FUNCTION compare_function, float compare_value, bool is_compare) asm("floor.cuda.read_image.float.f2");
template <> const_func clang_vector_type<float> read_image(uint64_t tex, IMAGE_TYPE type, clang_int3 coord, uint32_t layer, uint32_t sample, clang_int3 offset,
														   int32_t lod_i, float lod_or_bias_f, bool is_lod, bool is_lod_float, bool is_bias,
														   clang_float3 dpdx, clang_float3 dpdy, bool is_gradient,
														   COMPARE_FUNCTION compare_function, float compare_value, bool is_compare) asm("floor.cuda.read_image.float.i3");
template <> const_func clang_vector_type<float> read_image(uint64_t tex, IMAGE_TYPE type, clang_float3 coord, uint32_t layer, uint32_t sample, clang_int3 offset,
														   int32_t lod_i, float lod_or_bias_f, bool is_lod, bool is_lod_float, bool is_bias,
														   clang_float3 dpdx, clang_float3 dpdy, bool is_gradient,
														   COMPARE_FUNCTION compare_function, float compare_value, bool is_compare) asm("floor.cuda.read_image.float.f3");

template <> const_func clang_vector_type<half> read_image(uint64_t tex, IMAGE_TYPE type, clang_int1 coord, uint32_t layer, uint32_t sample, clang_int1 offset,
														  int32_t lod_i, float lod_or_bias_f, bool is_lod, bool is_lod_float, bool is_bias,
														  clang_float1 dpdx, clang_float1 dpdy, bool is_gradient,
														  COMPARE_FUNCTION compare_function, float compare_value, bool is_compare) asm("floor.cuda.read_image.half.i1");
template <> const_func clang_vector_type<half> read_image(uint64_t tex, IMAGE_TYPE type, clang_float1 coord, uint32_t layer, uint32_t sample, clang_int1 offset,
														  int32_t lod_i, float lod_or_bias_f, bool is_lod, bool is_lod_float, bool is_bias,
														  clang_float1 dpdx, clang_float1 dpdy, bool is_gradient,
														  COMPARE_FUNCTION compare_function, float compare_value, bool is_compare) asm("floor.cuda.read_image.half.f1");
template <> const_func clang_vector_type<half> read_image(uint64_t tex, IMAGE_TYPE type, clang_int2 coord, uint32_t layer, uint32_t sample, clang_int2 offset,
														  int32_t lod_i, float lod_or_bias_f, bool is_lod, bool is_lod_float, bool is_bias,
														  clang_float2 dpdx, clang_float2 dpdy, bool is_gradient,
														  COMPARE_FUNCTION compare_function, float compare_value, bool is_compare) asm("floor.cuda.read_image.half.i2");
template <> const_func clang_vector_type<half> read_image(uint64_t tex, IMAGE_TYPE type, clang_float2 coord, uint32_t layer, uint32_t sample, clang_int2 offset,
														  int32_t lod_i, float lod_or_bias_f, bool is_lod, bool is_lod_float, bool is_bias,
														  clang_float2 dpdx, clang_float2 dpdy, bool is_gradient,
														  COMPARE_FUNCTION compare_function, float compare_value, bool is_compare) asm("floor.cuda.read_image.half.f2");
template <> const_func clang_vector_type<half> read_image(uint64_t tex, IMAGE_TYPE type, clang_int3 coord, uint32_t layer, uint32_t sample, clang_int3 offset,
														  int32_t lod_i, float lod_or_bias_f, bool is_lod, bool is_lod_float, bool is_bias,
														  clang_float3 dpdx, clang_float3 dpdy, bool is_gradient,
														  COMPARE_FUNCTION compare_function, float compare_value, bool is_compare) asm("floor.cuda.read_image.half.i3");
template <> const_func clang_vector_type<half> read_image(uint64_t tex, IMAGE_TYPE type, clang_float3 coord, uint32_t layer, uint32_t sample, clang_int3 offset,
														  int32_t lod_i, float lod_or_bias_f, bool is_lod, bool is_lod_float, bool is_bias,
														  clang_float3 dpdx, clang_float3 dpdy, bool is_gradient,
														  COMPARE_FUNCTION compare_function, float compare_value, bool is_compare) asm("floor.cuda.read_image.half.f3");

template <> const_func clang_vector_type<int> read_image(uint64_t tex, IMAGE_TYPE type, clang_int1 coord, uint32_t layer, uint32_t sample, clang_int1 offset,
														 int32_t lod_i, float lod_or_bias_f, bool is_lod, bool is_lod_float, bool is_bias,
														 clang_float1 dpdx, clang_float1 dpdy, bool is_gradient,
														 COMPARE_FUNCTION compare_function, float compare_value, bool is_compare) asm("floor.cuda.read_image.int.i1");
template <> const_func clang_vector_type<int> read_image(uint64_t tex, IMAGE_TYPE type, clang_float1 coord, uint32_t layer, uint32_t sample, clang_int1 offset,
														 int32_t lod_i, float lod_or_bias_f, bool is_lod, bool is_lod_float, bool is_bias,
														 clang_float1 dpdx, clang_float1 dpdy, bool is_gradient,
														 COMPARE_FUNCTION compare_function, float compare_value, bool is_compare) asm("floor.cuda.read_image.int.f1");
template <> const_func clang_vector_type<int> read_image(uint64_t tex, IMAGE_TYPE type, clang_int2 coord, uint32_t layer, uint32_t sample, clang_int2 offset,
														 int32_t lod_i, float lod_or_bias_f, bool is_lod, bool is_lod_float, bool is_bias,
														 clang_float2 dpdx, clang_float2 dpdy, bool is_gradient,
														 COMPARE_FUNCTION compare_function, float compare_value, bool is_compare) asm("floor.cuda.read_image.int.i2");
template <> const_func clang_vector_type<int> read_image(uint64_t tex, IMAGE_TYPE type, clang_float2 coord, uint32_t layer, uint32_t sample, clang_int2 offset,
														 int32_t lod_i, float lod_or_bias_f, bool is_lod, bool is_lod_float, bool is_bias,
														 clang_float2 dpdx, clang_float2 dpdy, bool is_gradient,
														 COMPARE_FUNCTION compare_function, float compare_value, bool is_compare) asm("floor.cuda.read_image.int.f2");
template <> const_func clang_vector_type<int> read_image(uint64_t tex, IMAGE_TYPE type, clang_int3 coord, uint32_t layer, uint32_t sample, clang_int3 offset,
														 int32_t lod_i, float lod_or_bias_f, bool is_lod, bool is_lod_float, bool is_bias,
														 clang_float3 dpdx, clang_float3 dpdy, bool is_gradient,
														 COMPARE_FUNCTION compare_function, float compare_value, bool is_compare) asm("floor.cuda.read_image.int.i3");
template <> const_func clang_vector_type<int> read_image(uint64_t tex, IMAGE_TYPE type, clang_float3 coord, uint32_t layer, uint32_t sample, clang_int3 offset,
														 int32_t lod_i, float lod_or_bias_f, bool is_lod, bool is_lod_float, bool is_bias,
														 clang_float3 dpdx, clang_float3 dpdy, bool is_gradient,
														 COMPARE_FUNCTION compare_function, float compare_value, bool is_compare) asm("floor.cuda.read_image.int.f3");

template <> const_func clang_vector_type<short> read_image(uint64_t tex, IMAGE_TYPE type, clang_int1 coord, uint32_t layer, uint32_t sample, clang_int1 offset,
														   int32_t lod_i, float lod_or_bias_f, bool is_lod, bool is_lod_float, bool is_bias,
														   clang_float1 dpdx, clang_float1 dpdy, bool is_gradient,
														   COMPARE_FUNCTION compare_function, float compare_value, bool is_compare) asm("floor.cuda.read_image.short.i1");
template <> const_func clang_vector_type<short> read_image(uint64_t tex, IMAGE_TYPE type, clang_float1 coord, uint32_t layer, uint32_t sample, clang_int1 offset,
														   int32_t lod_i, float lod_or_bias_f, bool is_lod, bool is_lod_float, bool is_bias,
														   clang_float1 dpdx, clang_float1 dpdy, bool is_gradient,
														   COMPARE_FUNCTION compare_function, float compare_value, bool is_compare) asm("floor.cuda.read_image.short.f1");
template <> const_func clang_vector_type<short> read_image(uint64_t tex, IMAGE_TYPE type, clang_int2 coord, uint32_t layer, uint32_t sample, clang_int2 offset,
														   int32_t lod_i, float lod_or_bias_f, bool is_lod, bool is_lod_float, bool is_bias,
														   clang_float2 dpdx, clang_float2 dpdy, bool is_gradient,
														   COMPARE_FUNCTION compare_function, float compare_value, bool is_compare) asm("floor.cuda.read_image.short.i2");
template <> const_func clang_vector_type<short> read_image(uint64_t tex, IMAGE_TYPE type, clang_float2 coord, uint32_t layer, uint32_t sample, clang_int2 offset,
														   int32_t lod_i, float lod_or_bias_f, bool is_lod, bool is_lod_float, bool is_bias,
														   clang_float2 dpdx, clang_float2 dpdy, bool is_gradient,
														   COMPARE_FUNCTION compare_function, float compare_value, bool is_compare) asm("floor.cuda.read_image.short.f2");
template <> const_func clang_vector_type<short> read_image(uint64_t tex, IMAGE_TYPE type, clang_int3 coord, uint32_t layer, uint32_t sample, clang_int3 offset,
														   int32_t lod_i, float lod_or_bias_f, bool is_lod, bool is_lod_float, bool is_bias,
														   clang_float3 dpdx, clang_float3 dpdy, bool is_gradient,
														   COMPARE_FUNCTION compare_function, float compare_value, bool is_compare) asm("floor.cuda.read_image.short.i3");
template <> const_func clang_vector_type<short> read_image(uint64_t tex, IMAGE_TYPE type, clang_float3 coord, uint32_t layer, uint32_t sample, clang_int3 offset,
														   int32_t lod_i, float lod_or_bias_f, bool is_lod, bool is_lod_float, bool is_bias,
														   clang_float3 dpdx, clang_float3 dpdy, bool is_gradient,
														   COMPARE_FUNCTION compare_function, float compare_value, bool is_compare) asm("floor.cuda.read_image.short.f3");

template <> const_func clang_vector_type<uint32_t> read_image(uint64_t tex, IMAGE_TYPE type, clang_int1 coord, uint32_t layer, uint32_t sample, clang_int1 offset,
															  int32_t lod_i, float lod_or_bias_f, bool is_lod, bool is_lod_float, bool is_bias,
															  clang_float1 dpdx, clang_float1 dpdy, bool is_gradient,
															  COMPARE_FUNCTION compare_function, float compare_value, bool is_compare) asm("floor.cuda.read_image.uint.i1");
template <> const_func clang_vector_type<uint32_t> read_image(uint64_t tex, IMAGE_TYPE type, clang_float1 coord, uint32_t layer, uint32_t sample, clang_int1 offset,
															  int32_t lod_i, float lod_or_bias_f, bool is_lod, bool is_lod_float, bool is_bias,
															  clang_float1 dpdx, clang_float1 dpdy, bool is_gradient,
															  COMPARE_FUNCTION compare_function, float compare_value, bool is_compare) asm("floor.cuda.read_image.uint.f1");
template <> const_func clang_vector_type<uint32_t> read_image(uint64_t tex, IMAGE_TYPE type, clang_int2 coord, uint32_t layer, uint32_t sample, clang_int2 offset,
															  int32_t lod_i, float lod_or_bias_f, bool is_lod, bool is_lod_float, bool is_bias,
															  clang_float2 dpdx, clang_float2 dpdy, bool is_gradient,
															  COMPARE_FUNCTION compare_function, float compare_value, bool is_compare) asm("floor.cuda.read_image.uint.i2");
template <> const_func clang_vector_type<uint32_t> read_image(uint64_t tex, IMAGE_TYPE type, clang_float2 coord, uint32_t layer, uint32_t sample, clang_int2 offset,
															  int32_t lod_i, float lod_or_bias_f, bool is_lod, bool is_lod_float, bool is_bias,
															  clang_float2 dpdx, clang_float2 dpdy, bool is_gradient,
															  COMPARE_FUNCTION compare_function, float compare_value, bool is_compare) asm("floor.cuda.read_image.uint.f2");
template <> const_func clang_vector_type<uint32_t> read_image(uint64_t tex, IMAGE_TYPE type, clang_int3 coord, uint32_t layer, uint32_t sample, clang_int3 offset,
															  int32_t lod_i, float lod_or_bias_f, bool is_lod, bool is_lod_float, bool is_bias,
															  clang_float3 dpdx, clang_float3 dpdy, bool is_gradient,
															  COMPARE_FUNCTION compare_function, float compare_value, bool is_compare) asm("floor.cuda.read_image.uint.i3");
template <> const_func clang_vector_type<uint32_t> read_image(uint64_t tex, IMAGE_TYPE type, clang_float3 coord, uint32_t layer, uint32_t sample, clang_int3 offset,
															  int32_t lod_i, float lod_or_bias_f, bool is_lod, bool is_lod_float, bool is_bias,
															  clang_float3 dpdx, clang_float3 dpdy, bool is_gradient,
															  COMPARE_FUNCTION compare_function, float compare_value, bool is_compare) asm("floor.cuda.read_image.uint.f3");

template <> const_func clang_vector_type<uint16_t> read_image(uint64_t tex, IMAGE_TYPE type, clang_int1 coord, uint32_t layer, uint32_t sample, clang_int1 offset,
															  int32_t lod_i, float lod_or_bias_f, bool is_lod, bool is_lod_float, bool is_bias,
															  clang_float1 dpdx, clang_float1 dpdy, bool is_gradient,
															  COMPARE_FUNCTION compare_function, float compare_value, bool is_compare) asm("floor.cuda.read_image.ushort.i1");
template <> const_func clang_vector_type<uint16_t> read_image(uint64_t tex, IMAGE_TYPE type, clang_float1 coord, uint32_t layer, uint32_t sample, clang_int1 offset,
															  int32_t lod_i, float lod_or_bias_f, bool is_lod, bool is_lod_float, bool is_bias,
															  clang_float1 dpdx, clang_float1 dpdy, bool is_gradient,
															  COMPARE_FUNCTION compare_function, float compare_value, bool is_compare) asm("floor.cuda.read_image.ushort.f1");
template <> const_func clang_vector_type<uint16_t> read_image(uint64_t tex, IMAGE_TYPE type, clang_int2 coord, uint32_t layer, uint32_t sample, clang_int2 offset,
															  int32_t lod_i, float lod_or_bias_f, bool is_lod, bool is_lod_float, bool is_bias,
															  clang_float2 dpdx, clang_float2 dpdy, bool is_gradient,
															  COMPARE_FUNCTION compare_function, float compare_value, bool is_compare) asm("floor.cuda.read_image.ushort.i2");
template <> const_func clang_vector_type<uint16_t> read_image(uint64_t tex, IMAGE_TYPE type, clang_float2 coord, uint32_t layer, uint32_t sample, clang_int2 offset,
															  int32_t lod_i, float lod_or_bias_f, bool is_lod, bool is_lod_float, bool is_bias,
															  clang_float2 dpdx, clang_float2 dpdy, bool is_gradient,
															  COMPARE_FUNCTION compare_function, float compare_value, bool is_compare) asm("floor.cuda.read_image.ushort.f2");
template <> const_func clang_vector_type<uint16_t> read_image(uint64_t tex, IMAGE_TYPE type, clang_int3 coord, uint32_t layer, uint32_t sample, clang_int3 offset,
															  int32_t lod_i, float lod_or_bias_f, bool is_lod, bool is_lod_float, bool is_bias,
															  clang_float3 dpdx, clang_float3 dpdy, bool is_gradient,
															  COMPARE_FUNCTION compare_function, float compare_value, bool is_compare) asm("floor.cuda.read_image.ushort.i3");
template <> const_func clang_vector_type<uint16_t> read_image(uint64_t tex, IMAGE_TYPE type, clang_float3 coord, uint32_t layer, uint32_t sample, clang_int3 offset,
															  int32_t lod_i, float lod_or_bias_f, bool is_lod, bool is_lod_float, bool is_bias,
															  clang_float3 dpdx, clang_float3 dpdy, bool is_gradient,
															  COMPARE_FUNCTION compare_function, float compare_value, bool is_compare) asm("floor.cuda.read_image.ushort.f3");

void write_image_float(uint64_t surf, IMAGE_TYPE fixed_type, clang_int1 coord, uint32_t layer, uint32_t lod, bool is_lod,
					   clang_float4 data, IMAGE_TYPE rt_type) asm("floor.cuda.write_image.float.i1");
void write_image_float(uint64_t surf, IMAGE_TYPE fixed_type, clang_int2 coord, uint32_t layer, uint32_t lod, bool is_lod,
					   clang_float4 data, IMAGE_TYPE rt_type) asm("floor.cuda.write_image.float.i2");
void write_image_float(uint64_t surf, IMAGE_TYPE fixed_type, clang_int3 coord, uint32_t layer, uint32_t lod, bool is_lod,
					   clang_float4 data, IMAGE_TYPE rt_type) asm("floor.cuda.write_image.float.i3");

void write_image_half(uint64_t surf, IMAGE_TYPE fixed_type, clang_int1 coord, uint32_t layer, uint32_t lod, bool is_lod,
					  clang_half4 data, IMAGE_TYPE rt_type) asm("floor.cuda.write_image.half.i1");
void write_image_half(uint64_t surf, IMAGE_TYPE fixed_type, clang_int2 coord, uint32_t layer, uint32_t lod, bool is_lod,
					  clang_half4 data, IMAGE_TYPE rt_type) asm("floor.cuda.write_image.half.i2");
void write_image_half(uint64_t surf, IMAGE_TYPE fixed_type, clang_int3 coord, uint32_t layer, uint32_t lod, bool is_lod,
					  clang_half4 data, IMAGE_TYPE rt_type) asm("floor.cuda.write_image.half.i3");

void write_image_int(uint64_t surf, IMAGE_TYPE fixed_type, clang_int1 coord, uint32_t layer, uint32_t lod, bool is_lod,
					 clang_int4 data, IMAGE_TYPE rt_type) asm("floor.cuda.write_image.int.i1");
void write_image_int(uint64_t surf, IMAGE_TYPE fixed_type, clang_int2 coord, uint32_t layer, uint32_t lod, bool is_lod,
					 clang_int4 data, IMAGE_TYPE rt_type) asm("floor.cuda.write_image.int.i2");
void write_image_int(uint64_t surf, IMAGE_TYPE fixed_type, clang_int3 coord, uint32_t layer, uint32_t lod, bool is_lod,
					 clang_int4 data, IMAGE_TYPE rt_type) asm("floor.cuda.write_image.int.i3");

void write_image_short(uint64_t surf, IMAGE_TYPE fixed_type, clang_int1 coord, uint32_t layer, uint32_t lod, bool is_lod,
					   clang_short4 data, IMAGE_TYPE rt_type) asm("floor.cuda.write_image.short.i1");
void write_image_short(uint64_t surf, IMAGE_TYPE fixed_type, clang_int2 coord, uint32_t layer, uint32_t lod, bool is_lod,
					   clang_short4 data, IMAGE_TYPE rt_type) asm("floor.cuda.write_image.short.i2");
void write_image_short(uint64_t surf, IMAGE_TYPE fixed_type, clang_int3 coord, uint32_t layer, uint32_t lod, bool is_lod,
					   clang_short4 data, IMAGE_TYPE rt_type) asm("floor.cuda.write_image.short.i3");

void write_image_uint(uint64_t surf, IMAGE_TYPE fixed_type, clang_int1 coord, uint32_t layer, uint32_t lod, bool is_lod,
					  clang_uint4 data, IMAGE_TYPE rt_type) asm("floor.cuda.write_image.uint.i1");
void write_image_uint(uint64_t surf, IMAGE_TYPE fixed_type, clang_int2 coord, uint32_t layer, uint32_t lod, bool is_lod,
					  clang_uint4 data, IMAGE_TYPE rt_type) asm("floor.cuda.write_image.uint.i2");
void write_image_uint(uint64_t surf, IMAGE_TYPE fixed_type, clang_int3 coord, uint32_t layer, uint32_t lod, bool is_lod,
					  clang_uint4 data, IMAGE_TYPE rt_type) asm("floor.cuda.write_image.uint.i3");

void write_image_ushort(uint64_t surf, IMAGE_TYPE fixed_type, clang_int1 coord, uint32_t layer, uint32_t lod, bool is_lod,
						clang_ushort4 data, IMAGE_TYPE rt_type) asm("floor.cuda.write_image.ushort.i1");
void write_image_ushort(uint64_t surf, IMAGE_TYPE fixed_type, clang_int2 coord, uint32_t layer, uint32_t lod, bool is_lod,
						clang_ushort4 data, IMAGE_TYPE rt_type) asm("floor.cuda.write_image.ushort.i2");
void write_image_ushort(uint64_t surf, IMAGE_TYPE fixed_type, clang_int3 coord, uint32_t layer, uint32_t lod, bool is_lod,
						clang_ushort4 data, IMAGE_TYPE rt_type) asm("floor.cuda.write_image.ushort.i3");

const_func clang_uint4 get_image_dim(uint64_t tex_or_surf, IMAGE_TYPE type, uint32_t lod) asm("floor.cuda.get_image_dim");

//////////////////////////////////////////
// CUDA image write functions with run-time selection

// float write with fixed channel count or run-time variable channel count
template <IMAGE_TYPE image_type, typename clang_coord_type>
floor_inline_always static void write_float(const uint64_t surf, const IMAGE_TYPE runtime_image_type, const clang_coord_type coord,
											const uint32_t layer, const uint32_t lod, const bool is_lod, const clang_float4 data) {
	write_image_float(surf, image_type, coord, layer, lod, is_lod, data, runtime_image_type);
}

// half write with fixed channel count or run-time variable channel count
template <IMAGE_TYPE image_type, typename clang_coord_type>
floor_inline_always static void write_half(const uint64_t surf, const IMAGE_TYPE runtime_image_type, const clang_coord_type coord,
										   const uint32_t layer, const uint32_t lod, const bool is_lod, const clang_half4 data) {
	write_image_half(surf, image_type, coord, layer, lod, is_lod, data, runtime_image_type);
}

// int write with fixed channel count or run-time variable channel count
template <IMAGE_TYPE image_type, typename clang_coord_type>
floor_inline_always static void write_int(const uint64_t surf, const IMAGE_TYPE runtime_image_type, const clang_coord_type coord,
										  const uint32_t layer, const uint32_t lod, const bool is_lod, const clang_int4 data) {
	write_image_int(surf, image_type, coord, layer, lod, is_lod, data, runtime_image_type);
}

// short write with fixed channel count or run-time variable channel count
template <IMAGE_TYPE image_type, typename clang_coord_type>
floor_inline_always static void write_short(const uint64_t surf, const IMAGE_TYPE runtime_image_type, const clang_coord_type coord,
											const uint32_t layer, const uint32_t lod, const bool is_lod, const clang_short4 data) {
	write_image_short(surf, image_type, coord, layer, lod, is_lod, data, runtime_image_type);
}

// uint write with fixed channel count or run-time variable channel count
template <IMAGE_TYPE image_type, typename clang_coord_type>
floor_inline_always static void write_uint(const uint64_t surf, const IMAGE_TYPE runtime_image_type, const clang_coord_type coord,
										   const uint32_t layer, const uint32_t lod, const bool is_lod, const clang_uint4 data) {
	write_image_uint(surf, image_type, coord, layer, lod, is_lod, data, runtime_image_type);
}

// ushort write with fixed channel count or run-time variable channel count
template <IMAGE_TYPE image_type, typename clang_coord_type>
floor_inline_always static void write_ushort(const uint64_t surf, const IMAGE_TYPE runtime_image_type, const clang_coord_type coord,
											 const uint32_t layer, const uint32_t lod, const bool is_lod, const clang_ushort4 data) {
	write_image_ushort(surf, image_type, coord, layer, lod, is_lod, data, runtime_image_type);
}

} // namespace fl::cuda_image

#endif
