/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2021 Florian Ziesche
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

#ifndef __FLOOR_COMPUTE_DEVICE_OPAQUE_IMAGE_HPP__
#define __FLOOR_COMPUTE_DEVICE_OPAQUE_IMAGE_HPP__

#if defined(FLOOR_COMPUTE_OPENCL) || defined(FLOOR_COMPUTE_METAL) || defined(FLOOR_COMPUTE_VULKAN)

namespace opaque_image {
	//////////////////////////////////////////
	// opaque image function wrappers/forwarders (opencl/metal)
	
#if defined(FLOOR_COMPUTE_OPENCL)
	typedef sampler_t sampler_type;
#elif defined(FLOOR_COMPUTE_VULKAN)
	typedef decltype(vulkan_image::sampler::value) sampler_type;
#elif defined(FLOOR_COMPUTE_METAL)
	typedef metal_sampler_t sampler_type;
#endif
	
	// this needs to be overloaded for each image type
#define FLOOR_OPAQUE_IMAGE_FUNCTIONS(image_type) \
	const_func clang_float4 read_image_float(image_type img, sampler_type smplr, COMPUTE_IMAGE_TYPE type, clang_int1 coord, uint32_t layer = 0, uint32_t sample = 0, clang_int1 offset = { 0 }, \
											 int32_t lod_i = 0, float lod_or_bias_f = 0.0f, bool is_lod = false, bool is_lod_float = false, bool is_bias = true, \
											 clang_float1 dpdx = { 0.0f }, clang_float1 dpdy = { 0.0f }, bool is_gradient = false, \
											 COMPARE_FUNCTION compare_function = COMPARE_FUNCTION::NEVER, float compare_value = 0.0f, bool is_compare = false) asm("floor.opaque.read_image." #image_type ".float.i1"); \
	const_func clang_float4 read_image_float(image_type img, sampler_type smplr, COMPUTE_IMAGE_TYPE type, clang_float1 coord, uint32_t layer = 0, uint32_t sample = 0, clang_int1 offset = { 0 }, \
											 int32_t lod_i = 0, float lod_or_bias_f = 0.0f, bool is_lod = false, bool is_lod_float = false, bool is_bias = true, \
											 clang_float1 dpdx = { 0.0f }, clang_float1 dpdy = { 0.0f }, bool is_gradient = false, \
											 COMPARE_FUNCTION compare_function = COMPARE_FUNCTION::NEVER, float compare_value = 0.0f, bool is_compare = false) asm("floor.opaque.read_image." #image_type ".float.f1"); \
	const_func clang_float4 read_image_float(image_type img, sampler_type smplr, COMPUTE_IMAGE_TYPE type, clang_int2 coord, uint32_t layer = 0, uint32_t sample = 0, clang_int2 offset = { 0, 0 }, \
											 int32_t lod_i = 0, float lod_or_bias_f = 0.0f, bool is_lod = false, bool is_lod_float = false, bool is_bias = true, \
											 clang_float2 dpdx = { 0.0f, 0.0f }, clang_float2 dpdy = { 0.0f, 0.0f }, bool is_gradient = false, \
											 COMPARE_FUNCTION compare_function = COMPARE_FUNCTION::NEVER, float compare_value = 0.0f, bool is_compare = false) asm("floor.opaque.read_image." #image_type ".float.i2"); \
	const_func clang_float4 read_image_float(image_type img, sampler_type smplr, COMPUTE_IMAGE_TYPE type, clang_float2 coord, uint32_t layer = 0, uint32_t sample = 0, clang_int2 offset = { 0, 0 }, \
											 int32_t lod_i = 0, float lod_or_bias_f = 0.0f, bool is_lod = false, bool is_lod_float = false, bool is_bias = true, \
											 clang_float2 dpdx = { 0.0f, 0.0f }, clang_float2 dpdy = { 0.0f, 0.0f }, bool is_gradient = false, \
											 COMPARE_FUNCTION compare_function = COMPARE_FUNCTION::NEVER, float compare_value = 0.0f, bool is_compare = false) asm("floor.opaque.read_image." #image_type ".float.f2"); \
	const_func clang_float4 read_image_float(image_type img, sampler_type smplr, COMPUTE_IMAGE_TYPE type, clang_int3 coord, uint32_t layer = 0, uint32_t sample = 0, clang_int3 offset = { 0, 0, 0 }, \
											 int32_t lod_i = 0, float lod_or_bias_f = 0.0f, bool is_lod = false, bool is_lod_float = false, bool is_bias = true, \
											 clang_float3 dpdx = { 0.0f, 0.0f, 0.0f }, clang_float3 dpdy = { 0.0f, 0.0f, 0.0f }, bool is_gradient = false, \
											 COMPARE_FUNCTION compare_function = COMPARE_FUNCTION::NEVER, float compare_value = 0.0f, bool is_compare = false) asm("floor.opaque.read_image." #image_type ".float.i3"); \
	const_func clang_float4 read_image_float(image_type img, sampler_type smplr, COMPUTE_IMAGE_TYPE type, clang_float3 coord, uint32_t layer = 0, uint32_t sample = 0, clang_int3 offset = { 0, 0, 0 }, \
											 int32_t lod_i = 0, float lod_or_bias_f = 0.0f, bool is_lod = false, bool is_lod_float = false, bool is_bias = true, \
											 clang_float3 dpdx = { 0.0f, 0.0f, 0.0f }, clang_float3 dpdy = { 0.0f, 0.0f, 0.0f }, bool is_gradient = false, \
											 COMPARE_FUNCTION compare_function = COMPARE_FUNCTION::NEVER, float compare_value = 0.0f, bool is_compare = false) asm("floor.opaque.read_image." #image_type ".float.f3"); \
	\
	const_func clang_int4 read_image_int(image_type img, sampler_type smplr, COMPUTE_IMAGE_TYPE type, clang_int1 coord, uint32_t layer = 0, uint32_t sample = 0, clang_int1 offset = { 0 }, \
										 int32_t lod_i = 0, float lod_or_bias_f = 0.0f, bool is_lod = false, bool is_lod_float = false, bool is_bias = true, \
										 clang_float1 dpdx = { 0.0f }, clang_float1 dpdy = { 0.0f }, bool is_gradient = false, \
										 COMPARE_FUNCTION compare_function = COMPARE_FUNCTION::NEVER, float compare_value = 0.0f, bool is_compare = false) asm("floor.opaque.read_image." #image_type ".int.i1"); \
	const_func clang_int4 read_image_int(image_type img, sampler_type smplr, COMPUTE_IMAGE_TYPE type, clang_float1 coord, uint32_t layer = 0, uint32_t sample = 0, clang_int1 offset = { 0 }, \
										 int32_t lod_i = 0, float lod_or_bias_f = 0.0f, bool is_lod = false, bool is_lod_float = false, bool is_bias = true, \
										 clang_float1 dpdx = { 0.0f }, clang_float1 dpdy = { 0.0f }, bool is_gradient = false, \
										 COMPARE_FUNCTION compare_function = COMPARE_FUNCTION::NEVER, float compare_value = 0.0f, bool is_compare = false) asm("floor.opaque.read_image." #image_type ".int.f1"); \
	const_func clang_int4 read_image_int(image_type img, sampler_type smplr, COMPUTE_IMAGE_TYPE type, clang_int2 coord, uint32_t layer = 0, uint32_t sample = 0, clang_int2 offset = { 0, 0 }, \
										 int32_t lod_i = 0, float lod_or_bias_f = 0.0f, bool is_lod = false, bool is_lod_float = false, bool is_bias = true, \
										 clang_float2 dpdx = { 0.0f, 0.0f }, clang_float2 dpdy = { 0.0f, 0.0f }, bool is_gradient = false, \
										 COMPARE_FUNCTION compare_function = COMPARE_FUNCTION::NEVER, float compare_value = 0.0f, bool is_compare = false) asm("floor.opaque.read_image." #image_type ".int.i2"); \
	const_func clang_int4 read_image_int(image_type img, sampler_type smplr, COMPUTE_IMAGE_TYPE type, clang_float2 coord, uint32_t layer = 0, uint32_t sample = 0, clang_int2 offset = { 0, 0 }, \
										 int32_t lod_i = 0, float lod_or_bias_f = 0.0f, bool is_lod = false, bool is_lod_float = false, bool is_bias = true, \
										 clang_float2 dpdx = { 0.0f, 0.0f }, clang_float2 dpdy = { 0.0f, 0.0f }, bool is_gradient = false, \
										 COMPARE_FUNCTION compare_function = COMPARE_FUNCTION::NEVER, float compare_value = 0.0f, bool is_compare = false) asm("floor.opaque.read_image." #image_type ".int.f2"); \
	const_func clang_int4 read_image_int(image_type img, sampler_type smplr, COMPUTE_IMAGE_TYPE type, clang_int3 coord, uint32_t layer = 0, uint32_t sample = 0, clang_int3 offset = { 0, 0, 0 }, \
										 int32_t lod_i = 0, float lod_or_bias_f = 0.0f, bool is_lod = false, bool is_lod_float = false, bool is_bias = true, \
										 clang_float3 dpdx = { 0.0f, 0.0f, 0.0f }, clang_float3 dpdy = { 0.0f, 0.0f, 0.0f }, bool is_gradient = false, \
										 COMPARE_FUNCTION compare_function = COMPARE_FUNCTION::NEVER, float compare_value = 0.0f, bool is_compare = false) asm("floor.opaque.read_image." #image_type ".int.i3"); \
	const_func clang_int4 read_image_int(image_type img, sampler_type smplr, COMPUTE_IMAGE_TYPE type, clang_float3 coord, uint32_t layer = 0, uint32_t sample = 0, clang_int3 offset = { 0, 0, 0 }, \
										 int32_t lod_i = 0, float lod_or_bias_f = 0.0f, bool is_lod = false, bool is_lod_float = false, bool is_bias = true, \
										 clang_float3 dpdx = { 0.0f, 0.0f, 0.0f }, clang_float3 dpdy = { 0.0f, 0.0f, 0.0f }, bool is_gradient = false, \
										 COMPARE_FUNCTION compare_function = COMPARE_FUNCTION::NEVER, float compare_value = 0.0f, bool is_compare = false) asm("floor.opaque.read_image." #image_type ".int.f3"); \
	\
	const_func clang_uint4 read_image_uint(image_type img, sampler_type smplr, COMPUTE_IMAGE_TYPE type, clang_int1 coord, uint32_t layer = 0, uint32_t sample = 0, clang_int1 offset = { 0 }, \
										   int32_t lod_i = 0, float lod_or_bias_f = 0.0f, bool is_lod = false, bool is_lod_float = false, bool is_bias = true, \
										   clang_float1 dpdx = { 0.0f }, clang_float1 dpdy = { 0.0f }, bool is_gradient = false, \
										   COMPARE_FUNCTION compare_function = COMPARE_FUNCTION::NEVER, float compare_value = 0.0f, bool is_compare = false) asm("floor.opaque.read_image." #image_type ".uint.i1"); \
	const_func clang_uint4 read_image_uint(image_type img, sampler_type smplr, COMPUTE_IMAGE_TYPE type, clang_float1 coord, uint32_t layer = 0, uint32_t sample = 0, clang_int1 offset = { 0 }, \
										   int32_t lod_i = 0, float lod_or_bias_f = 0.0f, bool is_lod = false, bool is_lod_float = false, bool is_bias = true, \
										   clang_float1 dpdx = { 0.0f }, clang_float1 dpdy = { 0.0f }, bool is_gradient = false, \
										   COMPARE_FUNCTION compare_function = COMPARE_FUNCTION::NEVER, float compare_value = 0.0f, bool is_compare = false) asm("floor.opaque.read_image." #image_type ".uint.f1"); \
	const_func clang_uint4 read_image_uint(image_type img, sampler_type smplr, COMPUTE_IMAGE_TYPE type, clang_int2 coord, uint32_t layer = 0, uint32_t sample = 0, clang_int2 offset = { 0, 0 }, \
										   int32_t lod_i = 0, float lod_or_bias_f = 0.0f, bool is_lod = false, bool is_lod_float = false, bool is_bias = true, \
										   clang_float2 dpdx = { 0.0f, 0.0f }, clang_float2 dpdy = { 0.0f, 0.0f }, bool is_gradient = false, \
										   COMPARE_FUNCTION compare_function = COMPARE_FUNCTION::NEVER, float compare_value = 0.0f, bool is_compare = false) asm("floor.opaque.read_image." #image_type ".uint.i2"); \
	const_func clang_uint4 read_image_uint(image_type img, sampler_type smplr, COMPUTE_IMAGE_TYPE type, clang_float2 coord, uint32_t layer = 0, uint32_t sample = 0, clang_int2 offset = { 0, 0 }, \
										   int32_t lod_i = 0, float lod_or_bias_f = 0.0f, bool is_lod = false, bool is_lod_float = false, bool is_bias = true, \
										   clang_float2 dpdx = { 0.0f, 0.0f }, clang_float2 dpdy = { 0.0f, 0.0f }, bool is_gradient = false, \
										   COMPARE_FUNCTION compare_function = COMPARE_FUNCTION::NEVER, float compare_value = 0.0f, bool is_compare = false) asm("floor.opaque.read_image." #image_type ".uint.f2"); \
	const_func clang_uint4 read_image_uint(image_type img, sampler_type smplr, COMPUTE_IMAGE_TYPE type, clang_int3 coord, uint32_t layer = 0, uint32_t sample = 0, clang_int3 offset = { 0, 0, 0 }, \
										   int32_t lod_i = 0, float lod_or_bias_f = 0.0f, bool is_lod = false, bool is_lod_float = false, bool is_bias = true, \
										   clang_float3 dpdx = { 0.0f, 0.0f, 0.0f }, clang_float3 dpdy = { 0.0f, 0.0f, 0.0f }, bool is_gradient = false, \
										   COMPARE_FUNCTION compare_function = COMPARE_FUNCTION::NEVER, float compare_value = 0.0f, bool is_compare = false) asm("floor.opaque.read_image." #image_type ".uint.i3"); \
	const_func clang_uint4 read_image_uint(image_type img, sampler_type smplr, COMPUTE_IMAGE_TYPE type, clang_float3 coord, uint32_t layer = 0, uint32_t sample = 0, clang_int3 offset = { 0, 0, 0 }, \
										   int32_t lod_i = 0, float lod_or_bias_f = 0.0f, bool is_lod = false, bool is_lod_float = false, bool is_bias = true, \
										   clang_float3 dpdx = { 0.0f, 0.0f, 0.0f }, clang_float3 dpdy = { 0.0f, 0.0f, 0.0f }, bool is_gradient = false, \
										   COMPARE_FUNCTION compare_function = COMPARE_FUNCTION::NEVER, float compare_value = 0.0f, bool is_compare = false) asm("floor.opaque.read_image." #image_type ".uint.f3"); \
	\
	void write_image_float(image_type img, COMPUTE_IMAGE_TYPE type, clang_int1 coord, uint32_t layer, uint32_t lod, bool is_lod, clang_float4 data) asm("floor.opaque.write_image." #image_type ".float.i1"); \
	void write_image_float(image_type img, COMPUTE_IMAGE_TYPE type, clang_int2 coord, uint32_t layer, uint32_t lod, bool is_lod, clang_float4 data) asm("floor.opaque.write_image." #image_type ".float.i2"); \
	void write_image_float(image_type img, COMPUTE_IMAGE_TYPE type, clang_int3 coord, uint32_t layer, uint32_t lod, bool is_lod, clang_float4 data) asm("floor.opaque.write_image." #image_type ".float.i3"); \
	\
	void write_image_int(image_type img, COMPUTE_IMAGE_TYPE type, clang_int1 coord, uint32_t layer, uint32_t lod, bool is_lod, clang_int4 data) asm("floor.opaque.write_image." #image_type ".int.i1"); \
	void write_image_int(image_type img, COMPUTE_IMAGE_TYPE type, clang_int2 coord, uint32_t layer, uint32_t lod, bool is_lod, clang_int4 data) asm("floor.opaque.write_image." #image_type ".int.i2"); \
	void write_image_int(image_type img, COMPUTE_IMAGE_TYPE type, clang_int3 coord, uint32_t layer, uint32_t lod, bool is_lod, clang_int4 data) asm("floor.opaque.write_image." #image_type ".int.i3"); \
	\
	void write_image_uint(image_type img, COMPUTE_IMAGE_TYPE type, clang_int1 coord, uint32_t layer, uint32_t lod, bool is_lod, clang_uint4 data) asm("floor.opaque.write_image." #image_type ".uint.i1"); \
	void write_image_uint(image_type img, COMPUTE_IMAGE_TYPE type, clang_int2 coord, uint32_t layer, uint32_t lod, bool is_lod, clang_uint4 data) asm("floor.opaque.write_image." #image_type ".uint.i2"); \
	void write_image_uint(image_type img, COMPUTE_IMAGE_TYPE type, clang_int3 coord, uint32_t layer, uint32_t lod, bool is_lod, clang_uint4 data) asm("floor.opaque.write_image." #image_type ".uint.i3"); \
	\
	const_func clang_uint4 get_image_dim(image_type img, COMPUTE_IMAGE_TYPE type, uint32_t lod) asm("floor.opaque.get_image_dim." #image_type);
	
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
	
}

#endif

#endif
