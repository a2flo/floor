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

#if defined(FLOOR_COMPUTE_OPENCL) || defined(FLOOR_COMPUTE_METAL) || defined(FLOOR_COMPUTE_VULKAN)

namespace opaque_image {
//////////////////////////////////////////
// opaque image function wrappers/forwarders (OpenCL/Vulkan/Metal)

#if defined(FLOOR_COMPUTE_OPENCL)
using sampler_type = sampler_t;
#elif defined(FLOOR_COMPUTE_VULKAN)
using sampler_type = decltype(vulkan_image::sampler::value);
#elif defined(FLOOR_COMPUTE_METAL)
using sampler_type = metal_sampler_t;
#endif

template <typename scalar_type> using clang_vector_type = __attribute__((ext_vector_type(4))) scalar_type;

// these need to be overloaded for each image type
template <typename scalar_type, typename image_type>
const_func clang_vector_type<scalar_type> read_image(image_type img, sampler_type smplr, COMPUTE_IMAGE_TYPE type, clang_int1 coord, uint32_t layer = 0, uint32_t sample = 0, clang_int1 offset = { 0 },
													 int32_t lod_i = 0, float lod_or_bias_f = 0.0f, bool is_lod = false, bool is_lod_float = false, bool is_bias = true,
													 clang_float1 dpdx = { 0.0f }, clang_float1 dpdy = { 0.0f }, bool is_gradient = false,
													 COMPARE_FUNCTION compare_function = COMPARE_FUNCTION::NEVER, float compare_value = 0.0f, bool is_compare = false);

template <typename scalar_type, typename image_type>
const_func clang_vector_type<scalar_type> read_image(image_type img, sampler_type smplr, COMPUTE_IMAGE_TYPE type, clang_float1 coord, uint32_t layer = 0, uint32_t sample = 0, clang_int1 offset = { 0 },
													 int32_t lod_i = 0, float lod_or_bias_f = 0.0f, bool is_lod = false, bool is_lod_float = false, bool is_bias = true,
													 clang_float1 dpdx = { 0.0f }, clang_float1 dpdy = { 0.0f }, bool is_gradient = false,
													 COMPARE_FUNCTION compare_function = COMPARE_FUNCTION::NEVER, float compare_value = 0.0f, bool is_compare = false);

template <typename scalar_type, typename image_type>
const_func clang_vector_type<scalar_type> read_image(image_type img, sampler_type smplr, COMPUTE_IMAGE_TYPE type, clang_int2 coord, uint32_t layer = 0, uint32_t sample = 0, clang_int2 offset = { 0, 0 },
													 int32_t lod_i = 0, float lod_or_bias_f = 0.0f, bool is_lod = false, bool is_lod_float = false, bool is_bias = true,
													 clang_float2 dpdx = { 0.0f, 0.0f }, clang_float2 dpdy = { 0.0f, 0.0f }, bool is_gradient = false,
													 COMPARE_FUNCTION compare_function = COMPARE_FUNCTION::NEVER, float compare_value = 0.0f, bool is_compare = false);

template <typename scalar_type, typename image_type>
const_func clang_vector_type<scalar_type> read_image(image_type img, sampler_type smplr, COMPUTE_IMAGE_TYPE type, clang_float2 coord, uint32_t layer = 0, uint32_t sample = 0, clang_int2 offset = { 0, 0 },
													 int32_t lod_i = 0, float lod_or_bias_f = 0.0f, bool is_lod = false, bool is_lod_float = false, bool is_bias = true,
													 clang_float2 dpdx = { 0.0f, 0.0f }, clang_float2 dpdy = { 0.0f, 0.0f }, bool is_gradient = false,
													 COMPARE_FUNCTION compare_function = COMPARE_FUNCTION::NEVER, float compare_value = 0.0f, bool is_compare = false);

template <typename scalar_type, typename image_type>
const_func clang_vector_type<scalar_type> read_image(image_type img, sampler_type smplr, COMPUTE_IMAGE_TYPE type, clang_int3 coord, uint32_t layer = 0, uint32_t sample = 0, clang_int3 offset = { 0, 0, 0 },
													 int32_t lod_i = 0, float lod_or_bias_f = 0.0f, bool is_lod = false, bool is_lod_float = false, bool is_bias = true,
													 clang_float3 dpdx = { 0.0f, 0.0f, 0.0f }, clang_float3 dpdy = { 0.0f, 0.0f, 0.0f }, bool is_gradient = false,
													 COMPARE_FUNCTION compare_function = COMPARE_FUNCTION::NEVER, float compare_value = 0.0f, bool is_compare = false);

template <typename scalar_type, typename image_type>
const_func clang_vector_type<scalar_type> read_image(image_type img, sampler_type smplr, COMPUTE_IMAGE_TYPE type, clang_float3 coord, uint32_t layer = 0, uint32_t sample = 0, clang_int3 offset = { 0, 0, 0 },
													 int32_t lod_i = 0, float lod_or_bias_f = 0.0f, bool is_lod = false, bool is_lod_float = false, bool is_bias = true,
													 clang_float3 dpdx = { 0.0f, 0.0f, 0.0f }, clang_float3 dpdy = { 0.0f, 0.0f, 0.0f }, bool is_gradient = false,
													 COMPARE_FUNCTION compare_function = COMPARE_FUNCTION::NEVER, float compare_value = 0.0f, bool is_compare = false);

#define FLOOR_OPAQUE_IMAGE_FUNCTIONS(image_type) \
template <> const_func clang_vector_type<float> read_image(image_type img, sampler_type smplr, COMPUTE_IMAGE_TYPE type, clang_int1 coord, uint32_t layer, uint32_t sample, clang_int1 offset, \
														   int32_t lod_i, float lod_or_bias_f, bool is_lod, bool is_lod_float, bool is_bias, clang_float1 dpdx, clang_float1 dpdy, bool is_gradient, \
														   COMPARE_FUNCTION compare_function, float compare_value, bool is_compare) asm("floor.opaque.read_image." #image_type ".float.i1"); \
template <> const_func clang_vector_type<float> read_image(image_type img, sampler_type smplr, COMPUTE_IMAGE_TYPE type, clang_float1 coord, uint32_t layer, uint32_t sample, clang_int1 offset, \
														   int32_t lod_i, float lod_or_bias_f, bool is_lod, bool is_lod_float, bool is_bias, clang_float1 dpdx, clang_float1 dpdy, bool is_gradient, \
														   COMPARE_FUNCTION compare_function, float compare_value, bool is_compare) asm("floor.opaque.read_image." #image_type ".float.f1"); \
template <> const_func clang_vector_type<float> read_image(image_type img, sampler_type smplr, COMPUTE_IMAGE_TYPE type, clang_int2 coord, uint32_t layer, uint32_t sample, clang_int2 offset, \
														   int32_t lod_i, float lod_or_bias_f, bool is_lod, bool is_lod_float, bool is_bias, clang_float2 dpdx, clang_float2 dpdy, bool is_gradient, \
														   COMPARE_FUNCTION compare_function, float compare_value, bool is_compare) asm("floor.opaque.read_image." #image_type ".float.i2"); \
template <> const_func clang_vector_type<float> read_image(image_type img, sampler_type smplr, COMPUTE_IMAGE_TYPE type, clang_float2 coord, uint32_t layer, uint32_t sample, clang_int2 offset, \
														   int32_t lod_i, float lod_or_bias_f, bool is_lod, bool is_lod_float, bool is_bias, clang_float2 dpdx, clang_float2 dpdy, bool is_gradient, \
														   COMPARE_FUNCTION compare_function, float compare_value, bool is_compare) asm("floor.opaque.read_image." #image_type ".float.f2"); \
template <> const_func clang_vector_type<float> read_image(image_type img, sampler_type smplr, COMPUTE_IMAGE_TYPE type, clang_int3 coord, uint32_t layer, uint32_t sample, clang_int3 offset, \
														   int32_t lod_i, float lod_or_bias_f, bool is_lod, bool is_lod_float, bool is_bias, clang_float3 dpdx, clang_float3 dpdy, bool is_gradient, \
														   COMPARE_FUNCTION compare_function, float compare_value, bool is_compare) asm("floor.opaque.read_image." #image_type ".float.i3"); \
template <> const_func clang_vector_type<float> read_image(image_type img, sampler_type smplr, COMPUTE_IMAGE_TYPE type, clang_float3 coord, uint32_t layer, uint32_t sample, clang_int3 offset, \
														   int32_t lod_i, float lod_or_bias_f, bool is_lod, bool is_lod_float, bool is_bias, clang_float3 dpdx, clang_float3 dpdy, bool is_gradient, \
														   COMPARE_FUNCTION compare_function, float compare_value, bool is_compare) asm("floor.opaque.read_image." #image_type ".float.f3"); \
\
template <> const_func clang_vector_type<half> read_image(image_type img, sampler_type smplr, COMPUTE_IMAGE_TYPE type, clang_int1 coord, uint32_t layer, uint32_t sample, clang_int1 offset, \
														  int32_t lod_i, float lod_or_bias_f, bool is_lod, bool is_lod_float, bool is_bias, clang_float1 dpdx, clang_float1 dpdy, bool is_gradient, \
														  COMPARE_FUNCTION compare_function, float compare_value, bool is_compare) asm("floor.opaque.read_image." #image_type ".half.i1"); \
template <> const_func clang_vector_type<half> read_image(image_type img, sampler_type smplr, COMPUTE_IMAGE_TYPE type, clang_float1 coord, uint32_t layer, uint32_t sample, clang_int1 offset, \
														  int32_t lod_i, float lod_or_bias_f, bool is_lod, bool is_lod_float, bool is_bias, clang_float1 dpdx, clang_float1 dpdy, bool is_gradient, \
														  COMPARE_FUNCTION compare_function, float compare_value, bool is_compare) asm("floor.opaque.read_image." #image_type ".half.f1"); \
template <> const_func clang_vector_type<half> read_image(image_type img, sampler_type smplr, COMPUTE_IMAGE_TYPE type, clang_int2 coord, uint32_t layer, uint32_t sample, clang_int2 offset, \
														  int32_t lod_i, float lod_or_bias_f, bool is_lod, bool is_lod_float, bool is_bias, clang_float2 dpdx, clang_float2 dpdy, bool is_gradient, \
														  COMPARE_FUNCTION compare_function, float compare_value, bool is_compare) asm("floor.opaque.read_image." #image_type ".half.i2"); \
template <> const_func clang_vector_type<half> read_image(image_type img, sampler_type smplr, COMPUTE_IMAGE_TYPE type, clang_float2 coord, uint32_t layer, uint32_t sample, clang_int2 offset, \
														  int32_t lod_i, float lod_or_bias_f, bool is_lod, bool is_lod_float, bool is_bias, clang_float2 dpdx, clang_float2 dpdy, bool is_gradient, \
														  COMPARE_FUNCTION compare_function, float compare_value, bool is_compare) asm("floor.opaque.read_image." #image_type ".half.f2"); \
template <> const_func clang_vector_type<half> read_image(image_type img, sampler_type smplr, COMPUTE_IMAGE_TYPE type, clang_int3 coord, uint32_t layer, uint32_t sample, clang_int3 offset, \
														  int32_t lod_i, float lod_or_bias_f, bool is_lod, bool is_lod_float, bool is_bias, clang_float3 dpdx, clang_float3 dpdy, bool is_gradient, \
														  COMPARE_FUNCTION compare_function, float compare_value, bool is_compare) asm("floor.opaque.read_image." #image_type ".half.i3"); \
template <> const_func clang_vector_type<half> read_image(image_type img, sampler_type smplr, COMPUTE_IMAGE_TYPE type, clang_float3 coord, uint32_t layer, uint32_t sample, clang_int3 offset, \
														  int32_t lod_i, float lod_or_bias_f, bool is_lod, bool is_lod_float, bool is_bias, clang_float3 dpdx, clang_float3 dpdy, bool is_gradient, \
														  COMPARE_FUNCTION compare_function, float compare_value, bool is_compare) asm("floor.opaque.read_image." #image_type ".half.f3"); \
\
template <> const_func clang_vector_type<int> read_image(image_type img, sampler_type smplr, COMPUTE_IMAGE_TYPE type, clang_int1 coord, uint32_t layer, uint32_t sample, clang_int1 offset, \
														 int32_t lod_i, float lod_or_bias_f, bool is_lod, bool is_lod_float, bool is_bias, clang_float1 dpdx, clang_float1 dpdy, bool is_gradient, \
														 COMPARE_FUNCTION compare_function, float compare_value, bool is_compare) asm("floor.opaque.read_image." #image_type ".int.i1"); \
template <> const_func clang_vector_type<int> read_image(image_type img, sampler_type smplr, COMPUTE_IMAGE_TYPE type, clang_float1 coord, uint32_t layer, uint32_t sample, clang_int1 offset, \
														 int32_t lod_i, float lod_or_bias_f, bool is_lod, bool is_lod_float, bool is_bias, clang_float1 dpdx, clang_float1 dpdy, bool is_gradient, \
														 COMPARE_FUNCTION compare_function, float compare_value, bool is_compare) asm("floor.opaque.read_image." #image_type ".int.f1"); \
template <> const_func clang_vector_type<int> read_image(image_type img, sampler_type smplr, COMPUTE_IMAGE_TYPE type, clang_int2 coord, uint32_t layer, uint32_t sample, clang_int2 offset, \
														 int32_t lod_i, float lod_or_bias_f, bool is_lod, bool is_lod_float, bool is_bias, clang_float2 dpdx, clang_float2 dpdy, bool is_gradient, \
														 COMPARE_FUNCTION compare_function, float compare_value, bool is_compare) asm("floor.opaque.read_image." #image_type ".int.i2"); \
template <> const_func clang_vector_type<int> read_image(image_type img, sampler_type smplr, COMPUTE_IMAGE_TYPE type, clang_float2 coord, uint32_t layer, uint32_t sample, clang_int2 offset, \
														 int32_t lod_i, float lod_or_bias_f, bool is_lod, bool is_lod_float, bool is_bias, clang_float2 dpdx, clang_float2 dpdy, bool is_gradient, \
														 COMPARE_FUNCTION compare_function, float compare_value, bool is_compare) asm("floor.opaque.read_image." #image_type ".int.f2"); \
template <> const_func clang_vector_type<int> read_image(image_type img, sampler_type smplr, COMPUTE_IMAGE_TYPE type, clang_int3 coord, uint32_t layer, uint32_t sample, clang_int3 offset, \
														 int32_t lod_i, float lod_or_bias_f, bool is_lod, bool is_lod_float, bool is_bias, clang_float3 dpdx, clang_float3 dpdy, bool is_gradient, \
														 COMPARE_FUNCTION compare_function, float compare_value, bool is_compare) asm("floor.opaque.read_image." #image_type ".int.i3"); \
template <> const_func clang_vector_type<int> read_image(image_type img, sampler_type smplr, COMPUTE_IMAGE_TYPE type, clang_float3 coord, uint32_t layer, uint32_t sample, clang_int3 offset, \
														 int32_t lod_i, float lod_or_bias_f, bool is_lod, bool is_lod_float, bool is_bias, clang_float3 dpdx, clang_float3 dpdy, bool is_gradient, \
														 COMPARE_FUNCTION compare_function, float compare_value, bool is_compare) asm("floor.opaque.read_image." #image_type ".int.f3"); \
\
template <> const_func clang_vector_type<short> read_image(image_type img, sampler_type smplr, COMPUTE_IMAGE_TYPE type, clang_int1 coord, uint32_t layer, uint32_t sample, clang_int1 offset, \
														   int32_t lod_i, float lod_or_bias_f, bool is_lod, bool is_lod_float, bool is_bias, clang_float1 dpdx, clang_float1 dpdy, bool is_gradient, \
														   COMPARE_FUNCTION compare_function, float compare_value, bool is_compare) asm("floor.opaque.read_image." #image_type ".short.i1"); \
template <> const_func clang_vector_type<short> read_image(image_type img, sampler_type smplr, COMPUTE_IMAGE_TYPE type, clang_float1 coord, uint32_t layer, uint32_t sample, clang_int1 offset, \
														   int32_t lod_i, float lod_or_bias_f, bool is_lod, bool is_lod_float, bool is_bias, clang_float1 dpdx, clang_float1 dpdy, bool is_gradient, \
														   COMPARE_FUNCTION compare_function, float compare_value, bool is_compare) asm("floor.opaque.read_image." #image_type ".short.f1"); \
template <> const_func clang_vector_type<short> read_image(image_type img, sampler_type smplr, COMPUTE_IMAGE_TYPE type, clang_int2 coord, uint32_t layer, uint32_t sample, clang_int2 offset, \
														   int32_t lod_i, float lod_or_bias_f, bool is_lod, bool is_lod_float, bool is_bias, clang_float2 dpdx, clang_float2 dpdy, bool is_gradient, \
														   COMPARE_FUNCTION compare_function, float compare_value, bool is_compare) asm("floor.opaque.read_image." #image_type ".short.i2"); \
template <> const_func clang_vector_type<short> read_image(image_type img, sampler_type smplr, COMPUTE_IMAGE_TYPE type, clang_float2 coord, uint32_t layer, uint32_t sample, clang_int2 offset, \
														   int32_t lod_i, float lod_or_bias_f, bool is_lod, bool is_lod_float, bool is_bias, clang_float2 dpdx, clang_float2 dpdy, bool is_gradient, \
														   COMPARE_FUNCTION compare_function, float compare_value, bool is_compare) asm("floor.opaque.read_image." #image_type ".short.f2"); \
template <> const_func clang_vector_type<short> read_image(image_type img, sampler_type smplr, COMPUTE_IMAGE_TYPE type, clang_int3 coord, uint32_t layer, uint32_t sample, clang_int3 offset, \
														   int32_t lod_i, float lod_or_bias_f, bool is_lod, bool is_lod_float, bool is_bias, clang_float3 dpdx, clang_float3 dpdy, bool is_gradient, \
														   COMPARE_FUNCTION compare_function, float compare_value, bool is_compare) asm("floor.opaque.read_image." #image_type ".short.i3"); \
template <> const_func clang_vector_type<short> read_image(image_type img, sampler_type smplr, COMPUTE_IMAGE_TYPE type, clang_float3 coord, uint32_t layer, uint32_t sample, clang_int3 offset, \
														   int32_t lod_i, float lod_or_bias_f, bool is_lod, bool is_lod_float, bool is_bias, clang_float3 dpdx, clang_float3 dpdy, bool is_gradient, \
														   COMPARE_FUNCTION compare_function, float compare_value, bool is_compare) asm("floor.opaque.read_image." #image_type ".short.f3"); \
\
template <> const_func clang_vector_type<uint32_t> read_image(image_type img, sampler_type smplr, COMPUTE_IMAGE_TYPE type, clang_int1 coord, uint32_t layer, uint32_t sample, clang_int1 offset, \
															  int32_t lod_i, float lod_or_bias_f, bool is_lod, bool is_lod_float, bool is_bias, clang_float1 dpdx, clang_float1 dpdy, bool is_gradient, \
															  COMPARE_FUNCTION compare_function, float compare_value, bool is_compare) asm("floor.opaque.read_image." #image_type ".uint.i1"); \
template <> const_func clang_vector_type<uint32_t> read_image(image_type img, sampler_type smplr, COMPUTE_IMAGE_TYPE type, clang_float1 coord, uint32_t layer, uint32_t sample, clang_int1 offset, \
															  int32_t lod_i, float lod_or_bias_f, bool is_lod, bool is_lod_float, bool is_bias, clang_float1 dpdx, clang_float1 dpdy, bool is_gradient, \
															  COMPARE_FUNCTION compare_function, float compare_value, bool is_compare) asm("floor.opaque.read_image." #image_type ".uint.f1"); \
template <> const_func clang_vector_type<uint32_t> read_image(image_type img, sampler_type smplr, COMPUTE_IMAGE_TYPE type, clang_int2 coord, uint32_t layer, uint32_t sample, clang_int2 offset, \
															  int32_t lod_i, float lod_or_bias_f, bool is_lod, bool is_lod_float, bool is_bias, clang_float2 dpdx, clang_float2 dpdy, bool is_gradient, \
															  COMPARE_FUNCTION compare_function, float compare_value, bool is_compare) asm("floor.opaque.read_image." #image_type ".uint.i2"); \
template <> const_func clang_vector_type<uint32_t> read_image(image_type img, sampler_type smplr, COMPUTE_IMAGE_TYPE type, clang_float2 coord, uint32_t layer, uint32_t sample, clang_int2 offset, \
															  int32_t lod_i, float lod_or_bias_f, bool is_lod, bool is_lod_float, bool is_bias, clang_float2 dpdx, clang_float2 dpdy, bool is_gradient, \
															  COMPARE_FUNCTION compare_function, float compare_value, bool is_compare) asm("floor.opaque.read_image." #image_type ".uint.f2"); \
template <> const_func clang_vector_type<uint32_t> read_image(image_type img, sampler_type smplr, COMPUTE_IMAGE_TYPE type, clang_int3 coord, uint32_t layer, uint32_t sample, clang_int3 offset, \
															  int32_t lod_i, float lod_or_bias_f, bool is_lod, bool is_lod_float, bool is_bias, clang_float3 dpdx, clang_float3 dpdy, bool is_gradient, \
															  COMPARE_FUNCTION compare_function, float compare_value, bool is_compare) asm("floor.opaque.read_image." #image_type ".uint.i3"); \
template <> const_func clang_vector_type<uint32_t> read_image(image_type img, sampler_type smplr, COMPUTE_IMAGE_TYPE type, clang_float3 coord, uint32_t layer, uint32_t sample, clang_int3 offset, \
															  int32_t lod_i, float lod_or_bias_f, bool is_lod, bool is_lod_float, bool is_bias, clang_float3 dpdx, clang_float3 dpdy, bool is_gradient, \
															  COMPARE_FUNCTION compare_function, float compare_value, bool is_compare) asm("floor.opaque.read_image." #image_type ".uint.f3"); \
\
template <> const_func clang_vector_type<uint16_t> read_image(image_type img, sampler_type smplr, COMPUTE_IMAGE_TYPE type, clang_int1 coord, uint32_t layer, uint32_t sample, clang_int1 offset, \
															  int32_t lod_i, float lod_or_bias_f, bool is_lod, bool is_lod_float, bool is_bias, clang_float1 dpdx, clang_float1 dpdy, bool is_gradient, \
															  COMPARE_FUNCTION compare_function, float compare_value, bool is_compare) asm("floor.opaque.read_image." #image_type ".ushort.i1"); \
template <> const_func clang_vector_type<uint16_t> read_image(image_type img, sampler_type smplr, COMPUTE_IMAGE_TYPE type, clang_float1 coord, uint32_t layer, uint32_t sample, clang_int1 offset, \
															  int32_t lod_i, float lod_or_bias_f, bool is_lod, bool is_lod_float, bool is_bias, clang_float1 dpdx, clang_float1 dpdy, bool is_gradient, \
															  COMPARE_FUNCTION compare_function, float compare_value, bool is_compare) asm("floor.opaque.read_image." #image_type ".ushort.f1"); \
template <> const_func clang_vector_type<uint16_t> read_image(image_type img, sampler_type smplr, COMPUTE_IMAGE_TYPE type, clang_int2 coord, uint32_t layer, uint32_t sample, clang_int2 offset, \
															  int32_t lod_i, float lod_or_bias_f, bool is_lod, bool is_lod_float, bool is_bias, clang_float2 dpdx, clang_float2 dpdy, bool is_gradient, \
															  COMPARE_FUNCTION compare_function, float compare_value, bool is_compare) asm("floor.opaque.read_image." #image_type ".ushort.i2"); \
template <> const_func clang_vector_type<uint16_t> read_image(image_type img, sampler_type smplr, COMPUTE_IMAGE_TYPE type, clang_float2 coord, uint32_t layer, uint32_t sample, clang_int2 offset, \
															  int32_t lod_i, float lod_or_bias_f, bool is_lod, bool is_lod_float, bool is_bias, clang_float2 dpdx, clang_float2 dpdy, bool is_gradient, \
															  COMPARE_FUNCTION compare_function, float compare_value, bool is_compare) asm("floor.opaque.read_image." #image_type ".ushort.f2"); \
template <> const_func clang_vector_type<uint16_t> read_image(image_type img, sampler_type smplr, COMPUTE_IMAGE_TYPE type, clang_int3 coord, uint32_t layer, uint32_t sample, clang_int3 offset, \
															  int32_t lod_i, float lod_or_bias_f, bool is_lod, bool is_lod_float, bool is_bias, clang_float3 dpdx, clang_float3 dpdy, bool is_gradient, \
															  COMPARE_FUNCTION compare_function, float compare_value, bool is_compare) asm("floor.opaque.read_image." #image_type ".ushort.i3"); \
template <> const_func clang_vector_type<uint16_t> read_image(image_type img, sampler_type smplr, COMPUTE_IMAGE_TYPE type, clang_float3 coord, uint32_t layer, uint32_t sample, clang_int3 offset, \
															  int32_t lod_i, float lod_or_bias_f, bool is_lod, bool is_lod_float, bool is_bias, clang_float3 dpdx, clang_float3 dpdy, bool is_gradient, \
															  COMPARE_FUNCTION compare_function, float compare_value, bool is_compare) asm("floor.opaque.read_image." #image_type ".ushort.f3"); \
\
void write_image_float(image_type img, COMPUTE_IMAGE_TYPE type, clang_int1 coord, uint32_t layer, uint32_t lod, bool is_lod, clang_float4 data) asm("floor.opaque.write_image." #image_type ".float.i1"); \
void write_image_float(image_type img, COMPUTE_IMAGE_TYPE type, clang_int2 coord, uint32_t layer, uint32_t lod, bool is_lod, clang_float4 data) asm("floor.opaque.write_image." #image_type ".float.i2"); \
void write_image_float(image_type img, COMPUTE_IMAGE_TYPE type, clang_int3 coord, uint32_t layer, uint32_t lod, bool is_lod, clang_float4 data) asm("floor.opaque.write_image." #image_type ".float.i3"); \
void write_image_float(image_type img, COMPUTE_IMAGE_TYPE type, clang_int2 coord, uint32_t layer, uint32_t lod, bool is_lod, float data) asm("floor.opaque.write_image." #image_type ".float.depth.i2"); \
\
void write_image_half(image_type img, COMPUTE_IMAGE_TYPE type, clang_int1 coord, uint32_t layer, uint32_t lod, bool is_lod, clang_half4 data) asm("floor.opaque.write_image." #image_type ".half.i1"); \
void write_image_half(image_type img, COMPUTE_IMAGE_TYPE type, clang_int2 coord, uint32_t layer, uint32_t lod, bool is_lod, clang_half4 data) asm("floor.opaque.write_image." #image_type ".half.i2"); \
void write_image_half(image_type img, COMPUTE_IMAGE_TYPE type, clang_int3 coord, uint32_t layer, uint32_t lod, bool is_lod, clang_half4 data) asm("floor.opaque.write_image." #image_type ".half.i3"); \
void write_image_half(image_type img, COMPUTE_IMAGE_TYPE type, clang_int2 coord, uint32_t layer, uint32_t lod, bool is_lod, half data) asm("floor.opaque.write_image." #image_type ".half.depth.i2"); \
\
void write_image_int(image_type img, COMPUTE_IMAGE_TYPE type, clang_int1 coord, uint32_t layer, uint32_t lod, bool is_lod, clang_int4 data) asm("floor.opaque.write_image." #image_type ".int.i1"); \
void write_image_int(image_type img, COMPUTE_IMAGE_TYPE type, clang_int2 coord, uint32_t layer, uint32_t lod, bool is_lod, clang_int4 data) asm("floor.opaque.write_image." #image_type ".int.i2"); \
void write_image_int(image_type img, COMPUTE_IMAGE_TYPE type, clang_int3 coord, uint32_t layer, uint32_t lod, bool is_lod, clang_int4 data) asm("floor.opaque.write_image." #image_type ".int.i3"); \
\
void write_image_short(image_type img, COMPUTE_IMAGE_TYPE type, clang_int1 coord, uint32_t layer, uint32_t lod, bool is_lod, clang_short4 data) asm("floor.opaque.write_image." #image_type ".short.i1"); \
void write_image_short(image_type img, COMPUTE_IMAGE_TYPE type, clang_int2 coord, uint32_t layer, uint32_t lod, bool is_lod, clang_short4 data) asm("floor.opaque.write_image." #image_type ".short.i2"); \
void write_image_short(image_type img, COMPUTE_IMAGE_TYPE type, clang_int3 coord, uint32_t layer, uint32_t lod, bool is_lod, clang_short4 data) asm("floor.opaque.write_image." #image_type ".short.i3"); \
\
void write_image_uint(image_type img, COMPUTE_IMAGE_TYPE type, clang_int1 coord, uint32_t layer, uint32_t lod, bool is_lod, clang_uint4 data) asm("floor.opaque.write_image." #image_type ".uint.i1"); \
void write_image_uint(image_type img, COMPUTE_IMAGE_TYPE type, clang_int2 coord, uint32_t layer, uint32_t lod, bool is_lod, clang_uint4 data) asm("floor.opaque.write_image." #image_type ".uint.i2"); \
void write_image_uint(image_type img, COMPUTE_IMAGE_TYPE type, clang_int3 coord, uint32_t layer, uint32_t lod, bool is_lod, clang_uint4 data) asm("floor.opaque.write_image." #image_type ".uint.i3"); \
\
void write_image_ushort(image_type img, COMPUTE_IMAGE_TYPE type, clang_int1 coord, uint32_t layer, uint32_t lod, bool is_lod, clang_ushort4 data) asm("floor.opaque.write_image." #image_type ".ushort.i1"); \
void write_image_ushort(image_type img, COMPUTE_IMAGE_TYPE type, clang_int2 coord, uint32_t layer, uint32_t lod, bool is_lod, clang_ushort4 data) asm("floor.opaque.write_image." #image_type ".ushort.i2"); \
void write_image_ushort(image_type img, COMPUTE_IMAGE_TYPE type, clang_int3 coord, uint32_t layer, uint32_t lod, bool is_lod, clang_ushort4 data) asm("floor.opaque.write_image." #image_type ".ushort.i3"); \
\
const_func clang_uint4 get_image_dim(image_type img, COMPUTE_IMAGE_TYPE type, uint32_t lod) asm("floor.opaque.get_image_dim." #image_type); \
\
const_func float query_image_lod(image_type img, sampler_type smplr, COMPUTE_IMAGE_TYPE type, clang_float1 coord) asm("floor.opaque.query_image_lod." #image_type ".f1"); \
const_func float query_image_lod(image_type img, sampler_type smplr, COMPUTE_IMAGE_TYPE type, clang_float2 coord) asm("floor.opaque.query_image_lod." #image_type ".f2"); \
const_func float query_image_lod(image_type img, sampler_type smplr, COMPUTE_IMAGE_TYPE type, clang_float3 coord) asm("floor.opaque.query_image_lod." #image_type ".f3");

#define FLOOR_OPAQUE_IMAGE_TYPES(F) \
F(image1d_t) \
F(image1d_array_t) \
F(image1d_buffer_t) \
F(image2d_t) \
F(image2d_array_t) \
F(image2d_msaa_t) \
F(image2d_array_msaa_t) \
F(image2d_depth_t) \
F(image2d_array_depth_t) \
F(image2d_msaa_depth_t) \
F(image2d_array_msaa_depth_t) \
F(image3d_t) \
F(imagecube_t) \
F(imagecube_array_t) \
F(imagecube_depth_t) \
F(imagecube_array_depth_t)
	
FLOOR_OPAQUE_IMAGE_TYPES(FLOOR_OPAQUE_IMAGE_FUNCTIONS)

#undef FLOOR_OPAQUE_IMAGE_FUNCTIONS
#undef FLOOR_OPAQUE_IMAGE_TYPES

} // namespace opaque_image

#endif
