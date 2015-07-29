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

#ifndef __FLOOR_COMPUTE_DEVICE_METAL_IMAGE_HPP__
#define __FLOOR_COMPUTE_DEVICE_METAL_IMAGE_HPP__

#if defined(FLOOR_COMPUTE_METAL)

// COMPUTE_IMAGE_TYPE -> metal image type map
#include <floor/compute/device/opaque_image_map.hpp>

//
namespace metal_image {
	//////////////////////////////////////////
	// sampler type, as defined by apple
	// NOTE: only the constexpr version is supported right now
	struct sampler {
		enum ADDRESS_MODE {
			CLAMP_TO_ZERO	= 0,
			CLAMP_TO_EDGE	= 1,
			REPEAT			= 2,
			MIRRORED_REPEAT	= 3
		};
		enum FILTER_MODE {
			NEAREST			= 0,
			LINEAR			= 1
		};
		enum MIP_FILTER_MODE {
			MIP_NONE		= 0,
			MIP_NEAREST		= 1,
			MIP_LINEAR		= 2
		};
		enum COORD_MODE {
			NORMALIZED		= 0,
			PIXEL			= 1
		};
		enum COMPARE_FUNC {
			NONE			= 0,
			LESS			= 1,
			LESS_EQUAL		= 2,
			GREATER			= 3,
			GREATER_EQUAL	= 4,
			EQUAL			= 5,
			NOT_EQUAL		= 6,
			// unavailable right now
			// ALWAYS		= 7,
			// NEVER		= 8
		};
		
		union {
			struct {
				// address modes
				uint64_t s_address : 3;
				uint64_t t_address : 3;
				uint64_t r_address : 3;
				
				// filter modes
				uint64_t mag_filter : 2;
				uint64_t min_filter : 2;
				uint64_t mip_filter : 2;
				
				// coord mode
				uint64_t coord_mode : 1;
				
				// compare function
				uint64_t compare_func : 4;
				
				// currently unused/reserved
				uint64_t _unused : 43;
				
				// constant sampler flag
				uint64_t is_constant : 1;
			};
			uint64_t value;
		};
		
		// must be known at compile-time for now
		constexpr sampler(const ADDRESS_MODE address_mode = CLAMP_TO_EDGE,
						  const COORD_MODE coord_mode_ = PIXEL,
						  const FILTER_MODE filter_mode = NEAREST,
						  const MIP_FILTER_MODE mip_filter_mode = MIP_NONE,
						  const COMPARE_FUNC compare_func_ = NONE) :
		s_address(address_mode), t_address(address_mode), r_address(address_mode),
		coord_mode(coord_mode_),
		mag_filter(filter_mode), min_filter(filter_mode),
		mip_filter(mip_filter_mode),
		compare_func(compare_func_),
		_unused(0u), is_constant(1u) {}
		
		// unavailable
		sampler(const sampler&) = delete;
		void operator=(const sampler&) = delete;
		void operator&() = delete;
	};
	
	//////////////////////////////////////////
	// metal/air image function wrappers/forwarders
	// NOTE: I've decided to go with an OpenCL-like approach of function naming, which makes this *a lot* easier to use
	// NOTE: in the original metal texture API, it seems like some well-meaning fool decided it was a good idea to have _unsigned_
	//       coordinates, but forgot that there are modes like "clamp to edge" or "repeat". However, LLVM only knows signed types
	//       and all air.* functions are essentially LLVM builtin functions.
	//       -> can simply use signed int types here w/o any issues (which actually makes this more correct than apples api ;))
	
	// needed for metal image functions
	typedef int32_t clang_int2 __attribute__((ext_vector_type(2)));
	typedef int32_t clang_int3 __attribute__((ext_vector_type(3)));
	typedef int32_t clang_int4 __attribute__((ext_vector_type(4)));
	typedef uint32_t clang_uint2 __attribute__((ext_vector_type(2)));
	typedef uint32_t clang_uint3 __attribute__((ext_vector_type(3)));
	typedef uint32_t clang_uint4 __attribute__((ext_vector_type(4)));
	typedef float clang_float2 __attribute__((ext_vector_type(2)));
	typedef float clang_float3 __attribute__((ext_vector_type(3)));
	typedef float clang_float4 __attribute__((ext_vector_type(4)));
	typedef half clang_half2 __attribute__((ext_vector_type(2)));
	typedef half clang_half3 __attribute__((ext_vector_type(3)));
	typedef half clang_half4 __attribute__((ext_vector_type(4)));
	
	// metal/air read functions
	clang_float4 read_imagef(image1d_array_t image, sampler smplr, float coord, uint32_t layer, bool enable_offset = false, int32_t offset = 0, bool lod_or_bias = true, float lod_or_bias_value = 0.0f) asm("air.sample_texture_1d_array.v4f32");
	clang_float4 read_imagef(image1d_array_t image, int32_t coord, uint32_t layer, uint32_t lod = 0) asm("air.read_texture_1d_array.v4f32");
	clang_float4 read_imagef(image1d_t image, sampler smplr, float coord, bool enable_offset = false, int32_t offset = 0, bool lod_or_bias = true, float lod_or_bias_value = 0.0f) asm("air.sample_texture_1d.v4f32");
	clang_float4 read_imagef(image1d_t image, int32_t coord, uint32_t lod = 0) asm("air.read_texture_1d.v4f32");
	float read_imagef(image2d_array_depth_t image, sampler smplr, uint32_t depth_format, clang_float2 coord, uint32_t layer, bool enable_offset = false, clang_int2 offset = 0, bool lod_or_bias = true, float lod_or_bias_value = 0.0f) asm("air.sample_depth_2d_array.f32");
	float read_imagef(image2d_array_depth_t image, sampler smplr, uint32_t depth_format, clang_float2 coord, uint32_t layer, float compare_value, bool enable_offset = false, clang_int2 offset = 0, bool lod_or_bias = true, float lod_or_bias_value = 0.0f) asm("air.sample_compare_depth_2d_array.f32");
	float read_imagef(image2d_array_depth_t image, uint32_t depth_format, clang_int2 coord, uint32_t layer, uint32_t lod = 0) asm("air.read_depth_2d_array.f32");
	clang_float4 read_imagef(image2d_array_t image, clang_int2 coord, uint32_t layer, uint32_t lod = 0) asm("air.read_texture_2d_array.v4f32");
	clang_float4 read_imagef(image2d_array_t image, sampler smplr, clang_float2 coord, uint32_t layer, bool enable_offset = false, clang_int2 offset = 0, bool lod_or_bias = true, float lod_or_bias_value = 0.0f) asm("air.sample_texture_2d_array.v4f32");
	float read_imagef(image2d_depth_t image, sampler smplr, uint32_t depth_format, clang_float2 coord, bool enable_offset = false, clang_int2 offset = 0, bool lod_or_bias = true, float lod_or_bias_value = 0.0f) asm("air.sample_depth_2d.f32");
	float read_imagef(image2d_depth_t image, sampler smplr, uint32_t depth_format, clang_float2 coord, float compare_value, bool enable_offset = false, clang_int2 offset = 0, bool lod_or_bias = true, float lod_or_bias_value = 0.0f) asm("air.sample_compare_depth_2d.f32");
	float read_imagef(image2d_depth_t image, uint32_t depth_format, clang_int2 coord, uint32_t lod = 0) asm("air.read_depth_2d.f32");
	float read_imagef(image2d_msaa_depth_t image, uint32_t depth_format, clang_int2 coord, uint32_t sample) asm("air.read_depth_2d_ms.f32");
	clang_float4 read_imagef(image2d_msaa_t image, clang_int2 coord, uint32_t sample) asm("air.read_texture_2d_ms.v4f32");
	clang_float4 read_imagef(image2d_t image, clang_int2 coord, uint32_t lod = 0) asm("air.read_texture_2d.v4f32");
	clang_float4 read_imagef(image2d_t image, sampler smplr, clang_float2 coord, bool enable_offset = false, clang_int2 offset = 0, bool lod_or_bias = true, float lod_or_bias_value = 0.0f) asm("air.sample_texture_2d.v4f32");
	clang_float4 read_imagef(image3d_t image, clang_int3 coord, uint32_t lod = 0) asm("air.read_texture_3d.v4f32");
	clang_float4 read_imagef(image3d_t image, sampler smplr, clang_float3 coord, bool enable_offset = false, clang_int3 offset = 0, bool lod_or_bias = true, float lod_or_bias_value = 0.0f) asm("air.sample_texture_3d.v4f32");
	float read_imagef(imagecube_array_depth_t image, sampler smplr, uint32_t depth_format, clang_float3 coord, uint32_t layer, bool lod_or_bias = true, float lod_or_bias_value = 0.0f) asm("air.sample_depth_cube_array.f32");
	float read_imagef(imagecube_array_depth_t image, sampler smplr, uint32_t depth_format, clang_float3 coord, uint32_t layer, float compare_value, bool lod_or_bias = true, float lod_or_bias_value = 0.0f) asm("air.sample_compare_depth_cube_array.f32");
	float read_imagef(imagecube_array_depth_t image, uint32_t depth_format, clang_int2 coord, uint32_t face, uint32_t layer, uint32_t lod = 0) asm("air.read_depth_cube_array.f32");
	clang_float4 read_imagef(imagecube_array_t image, clang_int2 coord, uint32_t face, uint32_t layer, uint32_t lod = 0) asm("air.read_texture_cube_array.v4f32");
	clang_float4 read_imagef(imagecube_array_t image, sampler smplr, clang_float3 coord, uint32_t layer, bool lod_or_bias = true, float lod_or_bias_value = 0.0f) asm("air.sample_texture_cube_array.v4f32");
	float read_imagef(imagecube_depth_t image, sampler smplr, uint32_t depth_format, clang_float3 coord, bool lod_or_bias = true, float lod_or_bias_value = 0.0f) asm("air.sample_depth_cube.f32");
	float read_imagef(imagecube_depth_t image, sampler smplr, uint32_t depth_format, clang_float3 coord, float compare_value, bool lod_or_bias = true, float lod_or_bias_value = 0.0f) asm("air.sample_compare_depth_cube.f32");
	float read_imagef(imagecube_depth_t image, uint32_t depth_format, clang_int2 coord, uint32_t face, uint32_t lod = 0) asm("air.read_depth_cube.f32");
	clang_float4 read_imagef(imagecube_t image, clang_int2 coord, uint32_t face, uint32_t lod = 0) asm("air.read_texture_cube.v4f32");
	clang_float4 read_imagef(imagecube_t image, sampler smplr, clang_float3 coord, bool lod_or_bias = true, float lod_or_bias_value = 0.0f) asm("air.sample_texture_cube.v4f32");
	clang_int4 read_imagei(image1d_array_t image, sampler smplr, float coord, uint32_t layer, bool enable_offset = false, int32_t offset = 0, bool lod_or_bias = true, float lod_or_bias_value = 0.0f) asm("air.sample_texture_1d_array.s.v4i32");
	clang_int4 read_imagei(image1d_array_t image, int32_t coord, uint32_t layer, uint32_t lod = 0) asm("air.read_texture_1d_array.s.v4i32");
	clang_int4 read_imagei(image1d_t image, sampler smplr, float coord, bool enable_offset = false, int32_t offset = 0, bool lod_or_bias = true, float lod_or_bias_value = 0.0f) asm("air.sample_texture_1d.s.v4i32");
	clang_int4 read_imagei(image1d_t image, int32_t coord, uint32_t lod = 0) asm("air.read_texture_1d.s.v4i32");
	clang_int4 read_imagei(image2d_array_t image, clang_int2 coord, uint32_t layer, uint32_t lod = 0) asm("air.read_texture_2d_array.s.v4i32");
	clang_int4 read_imagei(image2d_array_t image, sampler smplr, clang_float2 coord, uint32_t layer, bool enable_offset = false, clang_int2 offset = 0, bool lod_or_bias = true, float lod_or_bias_value = 0.0f) asm("air.sample_texture_2d_array.s.v4i32");
	clang_int4 read_imagei(image2d_msaa_t image, clang_int2 coord, uint32_t sample) asm("air.read_texture_2d_ms.s.v4i32");
	clang_int4 read_imagei(image2d_t image, clang_int2 coord, uint32_t lod = 0) asm("air.read_texture_2d.s.v4i32");
	clang_int4 read_imagei(image2d_t image, sampler smplr, clang_float2 coord, bool enable_offset = false, clang_int2 offset = 0, bool lod_or_bias = true, float lod_or_bias_value = 0.0f) asm("air.sample_texture_2d.s.v4i32");
	clang_int4 read_imagei(image3d_t image, clang_int3 coord, uint32_t lod = 0) asm("air.read_texture_3d.s.v4i32");
	clang_int4 read_imagei(image3d_t image, sampler smplr, clang_float3 coord, bool enable_offset = false, clang_int3 offset = 0, bool lod_or_bias = true, float lod_or_bias_value = 0.0f) asm("air.sample_texture_3d.s.v4i32");
	clang_int4 read_imagei(imagecube_array_t image, clang_int2 coord, uint32_t face, uint32_t layer, uint32_t lod = 0) asm("air.read_texture_cube_array.s.v4i32");
	clang_int4 read_imagei(imagecube_array_t image, sampler smplr, clang_float3 coord, uint32_t layer, bool lod_or_bias = true, float lod_or_bias_value = 0.0f) asm("air.sample_texture_cube_array.s.v4i32");
	clang_int4 read_imagei(imagecube_t image, clang_int2 coord, uint32_t face, uint32_t lod = 0) asm("air.read_texture_cube.s.v4i32");
	clang_int4 read_imagei(imagecube_t image, sampler smplr, clang_float3 coord, bool lod_or_bias = true, float lod_or_bias_value = 0.0f) asm("air.sample_texture_cube.s.v4i32");
	clang_uint4 read_imageui(image1d_array_t image, sampler smplr, float coord, uint32_t layer, bool enable_offset = false, int32_t offset = 0, bool lod_or_bias = true, float lod_or_bias_value = 0.0f) asm("air.sample_texture_1d_array.u.v4i32");
	clang_uint4 read_imageui(image1d_array_t image, int32_t coord, uint32_t layer, uint32_t lod = 0) asm("air.read_texture_1d_array.u.v4i32");
	clang_uint4 read_imageui(image1d_t image, sampler smplr, float coord, bool enable_offset = false, int32_t offset = 0, bool lod_or_bias = true, float lod_or_bias_value = 0.0f) asm("air.sample_texture_1d.u.v4i32");
	clang_uint4 read_imageui(image1d_t image, int32_t coord, uint32_t lod = 0) asm("air.read_texture_1d.u.v4i32");
	clang_uint4 read_imageui(image2d_array_t image, clang_int2 coord, uint32_t layer, uint32_t lod = 0) asm("air.read_texture_2d_array.u.v4i32");
	clang_uint4 read_imageui(image2d_array_t image, sampler smplr, clang_float2 coord, uint32_t layer, bool enable_offset = false, clang_int2 offset = 0, bool lod_or_bias = true, float lod_or_bias_value = 0.0f) asm("air.sample_texture_2d_array.u.v4i32");
	clang_uint4 read_imageui(image2d_msaa_t image, clang_int2 coord, uint32_t sample) asm("air.read_texture_2d_ms.u.v4i32");
	clang_uint4 read_imageui(image2d_t image, clang_int2 coord, uint32_t lod = 0) asm("air.read_texture_2d.u.v4i32");
	clang_uint4 read_imageui(image2d_t image, sampler smplr, clang_float2 coord, bool enable_offset = false, clang_int2 offset = 0, bool lod_or_bias = true, float lod_or_bias_value = 0.0f) asm("air.sample_texture_2d.u.v4i32");
	clang_uint4 read_imageui(image3d_t image, clang_int3 coord, uint32_t lod = 0) asm("air.read_texture_3d.u.v4i32");
	clang_uint4 read_imageui(image3d_t image, sampler smplr, clang_float3 coord, bool enable_offset = false, clang_int3 offset = 0, bool lod_or_bias = true, float lod_or_bias_value = 0.0f) asm("air.sample_texture_3d.u.v4i32");
	clang_uint4 read_imageui(imagecube_array_t image, clang_int2 coord, uint32_t face, uint32_t layer, uint32_t lod = 0) asm("air.read_texture_cube_array.u.v4i32");
	clang_uint4 read_imageui(imagecube_array_t image, sampler smplr, clang_float3 coord, uint32_t layer, bool lod_or_bias = true, float lod_or_bias_value = 0.0f) asm("air.sample_texture_cube_array.u.v4i32");
	clang_uint4 read_imageui(imagecube_t image, clang_int2 coord, uint32_t face, uint32_t lod = 0) asm("air.read_texture_cube.u.v4i32");
	clang_uint4 read_imageui(imagecube_t image, sampler smplr, clang_float3 coord, bool lod_or_bias = true, float lod_or_bias_value = 0.0f) asm("air.sample_texture_cube.u.v4i32");
	
	// metal/air write functions
	void write_imagef(image1d_array_t image, int32_t coord, uint32_t layer, clang_float4 color, uint32_t lod = 0) asm("air.write_texture_1d_array.v4f32");
	void write_imagef(image1d_t image, int32_t coord, clang_float4 color, uint32_t lod = 0) asm("air.write_texture_1d.v4f32");
	void write_imagef(image2d_array_t image, clang_int2 coord, uint32_t layer, clang_float4 color, uint32_t lod = 0) asm("air.write_texture_2d_array.v4f32");
	void write_imagef(image2d_t image, clang_int2 coord, clang_float4 color, uint32_t lod = 0) asm("air.write_texture_2d.v4f32");
	void write_imagef(image3d_t image, clang_int3 coord, clang_float4 color, uint32_t lod = 0) asm("air.write_texture_3d.v4f32");
	void write_imagef(imagecube_array_t image, clang_int2 coord, uint32_t face, uint32_t layer, clang_float4 color, uint32_t lod = 0) asm("air.write_texture_cube_array.v4f32");
	void write_imagef(imagecube_t image, clang_int2 coord, uint32_t face, clang_float4 color, uint32_t lod = 0) asm("air.write_texture_cube.v4f32");
	void write_imagei(image1d_array_t image, int32_t coord, uint32_t layer, clang_int4 color, uint32_t lod = 0) asm("air.write_texture_1d_array.s.v4i32");
	void write_imagei(image1d_t image, int32_t coord, clang_int4 color, uint32_t lod = 0) asm("air.write_texture_1d.s.v4i32");
	void write_imagei(image2d_array_t image, clang_int2 coord, uint32_t layer, clang_int4 color, uint32_t lod = 0) asm("air.write_texture_2d_array.s.v4i32");
	void write_imagei(image2d_t image, clang_int2 coord, clang_int4 color, uint32_t lod = 0) asm("air.write_texture_2d.s.v4i32");
	void write_imagei(image3d_t image, clang_int3 coord, clang_int4 color, uint32_t lod = 0) asm("air.write_texture_3d.s.v4i32");
	void write_imagei(imagecube_array_t image, clang_int2 coord, uint32_t face, uint32_t layer, clang_int4 color, uint32_t lod = 0) asm("air.write_texture_cube_array.s.v4i32");
	void write_imagei(imagecube_t image, clang_int2 coord, uint32_t face, clang_int4 color, uint32_t lod = 0) asm("air.write_texture_cube.s.v4i32");
	void write_imageui(image1d_array_t image, int32_t coord, uint32_t layer, clang_uint4 color, uint32_t lod = 0) asm("air.write_texture_1d_array.u.v4i32");
	void write_imageui(image1d_t image, int32_t coord, clang_uint4 color, uint32_t lod = 0) asm("air.write_texture_1d.u.v4i32");
	void write_imageui(image2d_array_t image, clang_int2 coord, uint32_t layer, clang_uint4 color, uint32_t lod = 0) asm("air.write_texture_2d_array.u.v4i32");
	void write_imageui(image2d_t image, clang_int2 coord, clang_uint4 color, uint32_t lod = 0) asm("air.write_texture_2d.u.v4i32");
	void write_imageui(image3d_t image, clang_int3 coord, clang_uint4 color, uint32_t lod = 0) asm("air.write_texture_3d.u.v4i32");
	void write_imageui(imagecube_array_t image, clang_int2 coord, uint32_t face, uint32_t layer, clang_uint4 color, uint32_t lod = 0) asm("air.write_texture_cube_array.u.v4i32");
	void write_imageui(imagecube_t image, clang_int2 coord, uint32_t face, clang_uint4 color, uint32_t lod = 0) asm("air.write_texture_cube.u.v4i32");
	
#if 0 // gather support is not yet enabled
	clang_float4 gather_imagef(image2d_array_depth_t image, sampler smplr, uint32_t depth_format, clang_float2 coord, uint32_t layer, bool enable_offset = false, clang_int2 offset = 0) asm("air.gather_depth_2d_array.v4f32");
	clang_float4 gather_imagef(image2d_array_depth_t image, sampler smplr, uint32_t depth_format, clang_float2 coord, uint32_t layer, float compare_value, bool enable_offset = false, clang_int2 offset = 0) asm("air.gather_compare_depth_2d_array.f32");
	clang_float4 gather_imagef(image2d_array_t image, sampler smplr, clang_float2 coord, uint32_t layer, bool enable_offset = false, clang_int2 offset = 0, int32_t component = 0) asm("air.gather_texture_2d_array.v4f32");
	clang_float4 gather_imagef(image2d_depth_t image, sampler smplr, uint32_t depth_format, clang_float2 coord, bool enable_offset = false, clang_int2 offset = 0) asm("air.gather_depth_2d.v4f32");
	clang_float4 gather_imagef(image2d_depth_t image, sampler smplr, uint32_t depth_format, clang_float2 coord, float compare_value, bool enable_offset = false, clang_int2 offset = 0) asm("air.gather_compare_depth_2d.f32");
	clang_float4 gather_imagef(image2d_t image, sampler smplr, clang_float2 coord, bool enable_offset = false, clang_int2 offset = 0, int32_t component = 0) asm("air.gather_texture_2d.v4f32");
	clang_float4 gather_imagef(imagecube_array_depth_t image, sampler smplr, uint32_t depth_format, clang_float3 coord, uint32_t layer) asm("air.gather_depth_cube_array.v4f32");
	clang_float4 gather_imagef(imagecube_array_depth_t image, sampler smplr, uint32_t depth_format, clang_float3 coord, uint32_t layer, float compare_value) asm("air.gather_compare_depth_cube_array.f32");
	clang_float4 gather_imagef(imagecube_array_t image, sampler smplr, clang_float3 coord, uint32_t layer, int32_t component = 0) asm("air.gather_texture_cube_array.v4f32");
	clang_float4 gather_imagef(imagecube_depth_t image, sampler smplr, uint32_t depth_format, clang_float3 coord) asm("air.gather_depth_cube.v4f32");
	clang_float4 gather_imagef(imagecube_depth_t image, sampler smplr, uint32_t depth_format, clang_float3 coord, float compare_value) asm("air.gather_compare_depth_cube.f32");
	clang_float4 gather_imagef(imagecube_t image, sampler smplr, clang_float3 coord, int32_t component = 0) asm("air.gather_texture_cube.v4f32");
	clang_int4 gather_imagei(image2d_array_t image, sampler smplr, clang_float2 coord, uint32_t layer, bool enable_offset = false, clang_int2 offset = 0, int32_t component = 0) asm("air.gather_texture_2d_array.s.v4i32");
	clang_int4 gather_imagei(image2d_t image, sampler smplr, clang_float2 coord, bool enable_offset = false, clang_int2 offset = 0, int32_t component = 0) asm("air.gather_texture_2d.s.v4i32");
	clang_int4 gather_imagei(imagecube_array_t image, sampler smplr, clang_float3 coord, uint32_t layer, int32_t component = 0) asm("air.gather_texture_cube_array.s.v4i32");
	clang_int4 gather_imagei(imagecube_t image, sampler smplr, clang_float3 coord, int32_t component = 0) asm("air.gather_texture_cube.s.v4i32");
	clang_uint4 gather_imageui(image2d_array_t image, sampler smplr, clang_float2 coord, uint32_t layer, bool enable_offset = false, clang_int2 offset = 0, int32_t component = 0) asm("air.gather_texture_2d_array.u.v4i32");
	clang_uint4 gather_imageui(image2d_t image, sampler smplr, clang_float2 coord, bool enable_offset = false, clang_int2 offset = 0, int32_t component = 0) asm("air.gather_texture_2d.u.v4i32");
	clang_uint4 gather_imageui(imagecube_array_t image, sampler smplr, clang_float3 coord, uint32_t layer, int32_t component = 0) asm("air.gather_texture_cube_array.u.v4i32");
	clang_uint4 gather_imageui(imagecube_t image, sampler smplr, clang_float3 coord, int32_t component = 0) asm("air.gather_texture_cube.u.v4i32");
#endif
	
	//////////////////////////////////////////
	// actual image object implementation
	template <COMPUTE_IMAGE_TYPE image_type, typename image_storage>
	struct image {
		floor_inline_always static constexpr COMPUTE_IMAGE_TYPE type() { return image_type; }
		
		// convert any coordinate vector type to int* or float*
		template <typename coord_type, typename ret_coord_type = vector_n<conditional_t<is_integral<typename coord_type::decayed_scalar_type>::value, int, float>, coord_type::dim>>
		static auto convert_coord(const coord_type& coord) {
			return ret_coord_type { coord };
		}
		// convert any fundamental (single value) coordinate type to int or float
		template <typename coord_type, typename ret_coord_type = conditional_t<is_integral<coord_type>::value, int, float>, enable_if_t<is_fundamental<coord_type>::value>>
		static auto convert_coord(const coord_type& coord) {
			return ret_coord_type { coord };
		}
		
		// read functions
		template <typename coord_type, COMPUTE_IMAGE_TYPE image_type_ = image_type,
				  enable_if_t<(has_flag<COMPUTE_IMAGE_TYPE::FLAG_NORMALIZED>(image_type_) ||
							   (image_type_ & COMPUTE_IMAGE_TYPE::__DATA_TYPE_MASK) == COMPUTE_IMAGE_TYPE::FLOAT)>* = nullptr>
		auto read(const coord_type& coord) const {
			//constexpr const metal_image::sampler smplrmplr {};
			const auto clang_vec = read_imagef(static_cast<const image_storage*>(this)->readable_img(), convert_coord(coord));
			return image_vec_ret_type<image_type, float>::fit(float4::from_clang_vector(clang_vec));
		}
		
		template <typename coord_type, COMPUTE_IMAGE_TYPE image_type_ = image_type,
				  enable_if_t<(!has_flag<COMPUTE_IMAGE_TYPE::FLAG_NORMALIZED>(image_type_) &&
							   (image_type_ & COMPUTE_IMAGE_TYPE::__DATA_TYPE_MASK) == COMPUTE_IMAGE_TYPE::INT)>* = nullptr>
		auto read(const coord_type& coord) const {
			//constexpr const metal_image::sampler smplrmplr {};
			const auto clang_vec = read_imagei(static_cast<const image_storage*>(this)->readable_img(), convert_coord(coord));
			return image_vec_ret_type<image_type, int32_t>::fit(int4::from_clang_vector(clang_vec));
		}
		
		template <typename coord_type, COMPUTE_IMAGE_TYPE image_type_ = image_type,
				  enable_if_t<(!has_flag<COMPUTE_IMAGE_TYPE::FLAG_NORMALIZED>(image_type_) &&
							   (image_type_ & COMPUTE_IMAGE_TYPE::__DATA_TYPE_MASK) == COMPUTE_IMAGE_TYPE::UINT)>* = nullptr>
		auto read(const coord_type& coord) const {
			//constexpr const metal_image::sampler smplrmplr {};
			const auto clang_vec = read_imageui(static_cast<const image_storage*>(this)->readable_img(), convert_coord(coord));
			return image_vec_ret_type<image_type, uint32_t>::fit(uint4::from_clang_vector(clang_vec));
		}
		
		// write functions
		template <typename coord_type>
		void write(const coord_type& coord, const float4& data) const {
			write_imagef(static_cast<const image_storage*>(this)->writable_img(), convert_coord(coord), data);
		}
		
		template <typename coord_type>
		void write(const coord_type& coord, const int4& data) const {
			write_imagef(static_cast<const image_storage*>(this)->writable_img(), convert_coord(coord), data);
		}
		
		template <typename coord_type>
		void write(const coord_type& coord, const uint4& data) const {
			write_imagef(static_cast<const image_storage*>(this)->writable_img(), convert_coord(coord), data);
		}
	};
	
	template <COMPUTE_IMAGE_TYPE image_type>
	struct read_only_image : image<image_type, read_only_image<image_type>> {
		typedef typename opaque_image_type<image_type>::type opaque_type;
		
		const opaque_type& readable_img() const { return r_img; }
		
	protected:
		read_only opaque_type r_img;
	};
	
	template <COMPUTE_IMAGE_TYPE image_type>
	struct write_only_image : image<image_type, write_only_image<image_type>> {
		typedef typename opaque_image_type<image_type>::type opaque_type;
		
		const opaque_type& writable_img() const { return w_img; }
		
	protected:
		write_only opaque_type w_img;
	};
	
	template <COMPUTE_IMAGE_TYPE image_type>
	struct read_write_image : image<image_type, read_write_image<image_type>> {
		typedef typename opaque_image_type<image_type>::type opaque_type;
		
		const opaque_type& readable_img() const { return r_img; }
		const opaque_type& writable_img() const { return w_img; }
		
	protected:
		read_only opaque_type r_img;
		write_only opaque_type w_img;
	};
}

//
template <COMPUTE_IMAGE_TYPE image_type> using ro_image = metal_image::read_only_image<image_type>;
template <COMPUTE_IMAGE_TYPE image_type> using wo_image = metal_image::write_only_image<image_type>;
template <COMPUTE_IMAGE_TYPE image_type> using rw_image = metal_image::read_write_image<image_type>;

// floor image read/write wrappers
template <COMPUTE_IMAGE_TYPE image_type, typename image_storage, typename coord_type>
floor_inline_always auto read(const metal_image::image<image_type, image_storage>& img, const coord_type& coord) {
	return img.read(coord);
}

template <COMPUTE_IMAGE_TYPE image_type, typename image_storage>
floor_inline_always void write(const metal_image::image<image_type, image_storage>& img, const int2& coord, const float4& data) {
	img.write(coord, data);
}

#endif

#endif
