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

// note that this is identical to sampler_t (when compiling in metal mode), but also necessary here, b/c of conversion requirements
typedef constant struct _sampler_t* metal_sampler_t;

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
			ALWAYS			= 7,
			NEVER			= 8
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
		
		constexpr sampler(const sampler& s) :
		s_address(s.s_address), t_address(s.t_address), r_address(s.r_address),
		mag_filter(s.mag_filter), min_filter(s.min_filter), mip_filter(s.mip_filter),
		coord_mode(s.coord_mode),
		compare_func(s.compare_func),
		_unused(0u), is_constant(1u) {}
		
		// provide metal_sampler_t conversion, so the builtin sampler_t can be initialized
		constexpr operator metal_sampler_t() const { return (metal_sampler_t)value; }
		
		// unavailable
		void operator=(const sampler&) = delete;
		void operator&() = delete;
	};
}

namespace {
	//////////////////////////////////////////
	// metal/air image function wrappers/forwarders
	// NOTE: I've decided to go with an OpenCL-like approach of function naming, which makes this *a lot* easier to use
	// NOTE: in the original metal texture API, it seems like some well-meaning fool decided it was a good idea to have _unsigned_
	//       coordinates, but forgot that there are modes like "clamp to edge" or "repeat". However, LLVM only knows signed types
	//       and all air.* functions are essentially LLVM builtin functions.
	//       -> can simply use signed int types here w/o any issues (which actually makes this more correct than apples api ;))
	
	// metal/air read functions
	const_func clang_float4 read_imagef(image1d_array_t image, metal_sampler_t smplr, float coord, uint32_t layer, bool enable_offset = true, int32_t offset = 0, bool lod_or_bias = false, float lod_or_bias_value = 0.0f) asm("air.sample_texture_1d_array.v4f32");
	const_func clang_float4 read_imagef(image1d_array_t image, int32_t coord, uint32_t layer, uint32_t lod = 0) asm("air.read_texture_1d_array.v4f32");
	const_func clang_float4 read_imagef(image1d_t image, metal_sampler_t smplr, float coord, bool enable_offset = true, int32_t offset = 0, bool lod_or_bias = false, float lod_or_bias_value = 0.0f) asm("air.sample_texture_1d.v4f32");
	const_func clang_float4 read_imagef(image1d_t image, int32_t coord, uint32_t lod = 0) asm("air.read_texture_1d.v4f32");
	const_func float read_imagef(image2d_array_depth_t image, metal_sampler_t smplr, uint32_t depth_format, clang_float2 coord, uint32_t layer, bool enable_offset = true, clang_int2 offset = 0, bool lod_or_bias = false, float lod_or_bias_value = 0.0f) asm("air.sample_depth_2d_array.f32");
	const_func float read_imagef(image2d_array_depth_t image, metal_sampler_t smplr, uint32_t depth_format, clang_float2 coord, uint32_t layer, float compare_value, bool enable_offset = true, clang_int2 offset = 0, bool lod_or_bias = false, float lod_or_bias_value = 0.0f) asm("air.sample_compare_depth_2d_array.f32");
	const_func float read_imagef(image2d_array_depth_t image, uint32_t depth_format, clang_int2 coord, uint32_t layer, uint32_t lod = 0) asm("air.read_depth_2d_array.f32");
	const_func clang_float4 read_imagef(image2d_array_t image, clang_int2 coord, uint32_t layer, uint32_t lod = 0) asm("air.read_texture_2d_array.v4f32");
	const_func clang_float4 read_imagef(image2d_array_t image, metal_sampler_t smplr, clang_float2 coord, uint32_t layer, bool enable_offset = true, clang_int2 offset = 0, bool lod_or_bias = false, float lod_or_bias_value = 0.0f) asm("air.sample_texture_2d_array.v4f32");
	const_func float read_imagef(image2d_depth_t image, metal_sampler_t smplr, uint32_t depth_format, clang_float2 coord, bool enable_offset = true, clang_int2 offset = 0, bool lod_or_bias = false, float lod_or_bias_value = 0.0f) asm("air.sample_depth_2d.f32");
	const_func float read_imagef(image2d_depth_t image, metal_sampler_t smplr, uint32_t depth_format, clang_float2 coord, float compare_value, bool enable_offset = true, clang_int2 offset = 0, bool lod_or_bias = false, float lod_or_bias_value = 0.0f) asm("air.sample_compare_depth_2d.f32");
	const_func float read_imagef(image2d_depth_t image, uint32_t depth_format, clang_int2 coord, uint32_t lod = 0) asm("air.read_depth_2d.f32");
	const_func float read_imagef(image2d_msaa_depth_t image, uint32_t depth_format, clang_int2 coord, uint32_t sample) asm("air.read_depth_2d_ms.f32");
	const_func clang_float4 read_imagef(image2d_msaa_t image, clang_int2 coord, uint32_t sample) asm("air.read_texture_2d_ms.v4f32");
	const_func clang_float4 read_imagef(image2d_t image, clang_int2 coord, uint32_t lod = 0) asm("air.read_texture_2d.v4f32");
	const_func clang_float4 read_imagef(image2d_t image, metal_sampler_t smplr, clang_float2 coord, bool enable_offset = true, clang_int2 offset = 0, bool lod_or_bias = false, float lod_or_bias_value = 0.0f) asm("air.sample_texture_2d.v4f32");
	const_func clang_float4 read_imagef(image3d_t image, clang_int3 coord, uint32_t lod = 0) asm("air.read_texture_3d.v4f32");
	const_func clang_float4 read_imagef(image3d_t image, metal_sampler_t smplr, clang_float3 coord, bool enable_offset = true, clang_int3 offset = 0, bool lod_or_bias = false, float lod_or_bias_value = 0.0f) asm("air.sample_texture_3d.v4f32");
	const_func float read_imagef(imagecube_array_depth_t image, metal_sampler_t smplr, uint32_t depth_format, clang_float3 coord, uint32_t layer, bool lod_or_bias = false, float lod_or_bias_value = 0.0f) asm("air.sample_depth_cube_array.f32");
	const_func float read_imagef(imagecube_array_depth_t image, metal_sampler_t smplr, uint32_t depth_format, clang_float3 coord, uint32_t layer, float compare_value, bool lod_or_bias = false, float lod_or_bias_value = 0.0f) asm("air.sample_compare_depth_cube_array.f32");
	const_func float read_imagef(imagecube_array_depth_t image, uint32_t depth_format, clang_int2 coord, uint32_t face, uint32_t layer, uint32_t lod = 0) asm("air.read_depth_cube_array.f32");
	const_func clang_float4 read_imagef(imagecube_array_t image, clang_int2 coord, uint32_t face, uint32_t layer, uint32_t lod = 0) asm("air.read_texture_cube_array.v4f32");
	const_func clang_float4 read_imagef(imagecube_array_t image, metal_sampler_t smplr, clang_float3 coord, uint32_t layer, bool lod_or_bias = false, float lod_or_bias_value = 0.0f) asm("air.sample_texture_cube_array.v4f32");
	const_func float read_imagef(imagecube_depth_t image, metal_sampler_t smplr, uint32_t depth_format, clang_float3 coord, bool lod_or_bias = false, float lod_or_bias_value = 0.0f) asm("air.sample_depth_cube.f32");
	const_func float read_imagef(imagecube_depth_t image, metal_sampler_t smplr, uint32_t depth_format, clang_float3 coord, float compare_value, bool lod_or_bias = false, float lod_or_bias_value = 0.0f) asm("air.sample_compare_depth_cube.f32");
	const_func float read_imagef(imagecube_depth_t image, uint32_t depth_format, clang_int2 coord, uint32_t face, uint32_t lod = 0) asm("air.read_depth_cube.f32");
	const_func clang_float4 read_imagef(imagecube_t image, clang_int2 coord, uint32_t face, uint32_t lod = 0) asm("air.read_texture_cube.v4f32");
	const_func clang_float4 read_imagef(imagecube_t image, metal_sampler_t smplr, clang_float3 coord, bool lod_or_bias = false, float lod_or_bias_value = 0.0f) asm("air.sample_texture_cube.v4f32");
	const_func clang_int4 read_imagei(image1d_array_t image, metal_sampler_t smplr, float coord, uint32_t layer, bool enable_offset = true, int32_t offset = 0, bool lod_or_bias = false, float lod_or_bias_value = 0.0f) asm("air.sample_texture_1d_array.s.v4i32");
	const_func clang_int4 read_imagei(image1d_array_t image, int32_t coord, uint32_t layer, uint32_t lod = 0) asm("air.read_texture_1d_array.s.v4i32");
	const_func clang_int4 read_imagei(image1d_t image, metal_sampler_t smplr, float coord, bool enable_offset = true, int32_t offset = 0, bool lod_or_bias = false, float lod_or_bias_value = 0.0f) asm("air.sample_texture_1d.s.v4i32");
	const_func clang_int4 read_imagei(image1d_t image, int32_t coord, uint32_t lod = 0) asm("air.read_texture_1d.s.v4i32");
	const_func clang_int4 read_imagei(image2d_array_t image, clang_int2 coord, uint32_t layer, uint32_t lod = 0) asm("air.read_texture_2d_array.s.v4i32");
	const_func clang_int4 read_imagei(image2d_array_t image, metal_sampler_t smplr, clang_float2 coord, uint32_t layer, bool enable_offset = true, clang_int2 offset = 0, bool lod_or_bias = false, float lod_or_bias_value = 0.0f) asm("air.sample_texture_2d_array.s.v4i32");
	const_func clang_int4 read_imagei(image2d_msaa_t image, clang_int2 coord, uint32_t sample) asm("air.read_texture_2d_ms.s.v4i32");
	const_func clang_int4 read_imagei(image2d_t image, clang_int2 coord, uint32_t lod = 0) asm("air.read_texture_2d.s.v4i32");
	const_func clang_int4 read_imagei(image2d_t image, metal_sampler_t smplr, clang_float2 coord, bool enable_offset = true, clang_int2 offset = 0, bool lod_or_bias = false, float lod_or_bias_value = 0.0f) asm("air.sample_texture_2d.s.v4i32");
	const_func clang_int4 read_imagei(image3d_t image, clang_int3 coord, uint32_t lod = 0) asm("air.read_texture_3d.s.v4i32");
	const_func clang_int4 read_imagei(image3d_t image, metal_sampler_t smplr, clang_float3 coord, bool enable_offset = true, clang_int3 offset = 0, bool lod_or_bias = false, float lod_or_bias_value = 0.0f) asm("air.sample_texture_3d.s.v4i32");
	const_func clang_int4 read_imagei(imagecube_array_t image, clang_int2 coord, uint32_t face, uint32_t layer, uint32_t lod = 0) asm("air.read_texture_cube_array.s.v4i32");
	const_func clang_int4 read_imagei(imagecube_array_t image, metal_sampler_t smplr, clang_float3 coord, uint32_t layer, bool lod_or_bias = false, float lod_or_bias_value = 0.0f) asm("air.sample_texture_cube_array.s.v4i32");
	const_func clang_int4 read_imagei(imagecube_t image, clang_int2 coord, uint32_t face, uint32_t lod = 0) asm("air.read_texture_cube.s.v4i32");
	const_func clang_int4 read_imagei(imagecube_t image, metal_sampler_t smplr, clang_float3 coord, bool lod_or_bias = false, float lod_or_bias_value = 0.0f) asm("air.sample_texture_cube.s.v4i32");
	const_func clang_uint4 read_imageui(image1d_array_t image, metal_sampler_t smplr, float coord, uint32_t layer, bool enable_offset = true, int32_t offset = 0, bool lod_or_bias = false, float lod_or_bias_value = 0.0f) asm("air.sample_texture_1d_array.u.v4i32");
	const_func clang_uint4 read_imageui(image1d_array_t image, int32_t coord, uint32_t layer, uint32_t lod = 0) asm("air.read_texture_1d_array.u.v4i32");
	const_func clang_uint4 read_imageui(image1d_t image, metal_sampler_t smplr, float coord, bool enable_offset = true, int32_t offset = 0, bool lod_or_bias = false, float lod_or_bias_value = 0.0f) asm("air.sample_texture_1d.u.v4i32");
	const_func clang_uint4 read_imageui(image1d_t image, int32_t coord, uint32_t lod = 0) asm("air.read_texture_1d.u.v4i32");
	const_func clang_uint4 read_imageui(image2d_array_t image, clang_int2 coord, uint32_t layer, uint32_t lod = 0) asm("air.read_texture_2d_array.u.v4i32");
	const_func clang_uint4 read_imageui(image2d_array_t image, metal_sampler_t smplr, clang_float2 coord, uint32_t layer, bool enable_offset = true, clang_int2 offset = 0, bool lod_or_bias = false, float lod_or_bias_value = 0.0f) asm("air.sample_texture_2d_array.u.v4i32");
	const_func clang_uint4 read_imageui(image2d_msaa_t image, clang_int2 coord, uint32_t sample) asm("air.read_texture_2d_ms.u.v4i32");
	const_func clang_uint4 read_imageui(image2d_t image, clang_int2 coord, uint32_t lod = 0) asm("air.read_texture_2d.u.v4i32");
	const_func clang_uint4 read_imageui(image2d_t image, metal_sampler_t smplr, clang_float2 coord, bool enable_offset = true, clang_int2 offset = 0, bool lod_or_bias = false, float lod_or_bias_value = 0.0f) asm("air.sample_texture_2d.u.v4i32");
	const_func clang_uint4 read_imageui(image3d_t image, clang_int3 coord, uint32_t lod = 0) asm("air.read_texture_3d.u.v4i32");
	const_func clang_uint4 read_imageui(image3d_t image, metal_sampler_t smplr, clang_float3 coord, bool enable_offset = true, clang_int3 offset = 0, bool lod_or_bias = false, float lod_or_bias_value = 0.0f) asm("air.sample_texture_3d.u.v4i32");
	const_func clang_uint4 read_imageui(imagecube_array_t image, clang_int2 coord, uint32_t face, uint32_t layer, uint32_t lod = 0) asm("air.read_texture_cube_array.u.v4i32");
	const_func clang_uint4 read_imageui(imagecube_array_t image, metal_sampler_t smplr, clang_float3 coord, uint32_t layer, bool lod_or_bias = false, float lod_or_bias_value = 0.0f) asm("air.sample_texture_cube_array.u.v4i32");
	const_func clang_uint4 read_imageui(imagecube_t image, clang_int2 coord, uint32_t face, uint32_t lod = 0) asm("air.read_texture_cube.u.v4i32");
	const_func clang_uint4 read_imageui(imagecube_t image, metal_sampler_t smplr, clang_float3 coord, bool lod_or_bias = false, float lod_or_bias_value = 0.0f) asm("air.sample_texture_cube.u.v4i32");
	
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
	const_func clang_float4 gather_imagef(image2d_array_depth_t image, metal_sampler_t smplr, uint32_t depth_format, clang_float2 coord, uint32_t layer, bool enable_offset = true, clang_int2 offset = 0) asm("air.gather_depth_2d_array.v4f32");
	const_func clang_float4 gather_imagef(image2d_array_depth_t image, metal_sampler_t smplr, uint32_t depth_format, clang_float2 coord, uint32_t layer, float compare_value, bool enable_offset = true, clang_int2 offset = 0) asm("air.gather_compare_depth_2d_array.f32");
	const_func clang_float4 gather_imagef(image2d_array_t image, metal_sampler_t smplr, clang_float2 coord, uint32_t layer, bool enable_offset = true, clang_int2 offset = 0, int32_t component = 0) asm("air.gather_texture_2d_array.v4f32");
	const_func clang_float4 gather_imagef(image2d_depth_t image, metal_sampler_t smplr, uint32_t depth_format, clang_float2 coord, bool enable_offset = true, clang_int2 offset = 0) asm("air.gather_depth_2d.v4f32");
	const_func clang_float4 gather_imagef(image2d_depth_t image, metal_sampler_t smplr, uint32_t depth_format, clang_float2 coord, float compare_value, bool enable_offset = true, clang_int2 offset = 0) asm("air.gather_compare_depth_2d.f32");
	const_func clang_float4 gather_imagef(image2d_t image, metal_sampler_t smplr, clang_float2 coord, bool enable_offset = true, clang_int2 offset = 0, int32_t component = 0) asm("air.gather_texture_2d.v4f32");
	const_func clang_float4 gather_imagef(imagecube_array_depth_t image, metal_sampler_t smplr, uint32_t depth_format, clang_float3 coord, uint32_t layer) asm("air.gather_depth_cube_array.v4f32");
	const_func clang_float4 gather_imagef(imagecube_array_depth_t image, metal_sampler_t smplr, uint32_t depth_format, clang_float3 coord, uint32_t layer, float compare_value) asm("air.gather_compare_depth_cube_array.f32");
	const_func clang_float4 gather_imagef(imagecube_array_t image, metal_sampler_t smplr, clang_float3 coord, uint32_t layer, int32_t component = 0) asm("air.gather_texture_cube_array.v4f32");
	const_func clang_float4 gather_imagef(imagecube_depth_t image, metal_sampler_t smplr, uint32_t depth_format, clang_float3 coord) asm("air.gather_depth_cube.v4f32");
	const_func clang_float4 gather_imagef(imagecube_depth_t image, metal_sampler_t smplr, uint32_t depth_format, clang_float3 coord, float compare_value) asm("air.gather_compare_depth_cube.f32");
	const_func clang_float4 gather_imagef(imagecube_t image, metal_sampler_t smplr, clang_float3 coord, int32_t component = 0) asm("air.gather_texture_cube.v4f32");
	const_func clang_int4 gather_imagei(image2d_array_t image, metal_sampler_t smplr, clang_float2 coord, uint32_t layer, bool enable_offset = true, clang_int2 offset = 0, int32_t component = 0) asm("air.gather_texture_2d_array.s.v4i32");
	const_func clang_int4 gather_imagei(image2d_t image, metal_sampler_t smplr, clang_float2 coord, bool enable_offset = true, clang_int2 offset = 0, int32_t component = 0) asm("air.gather_texture_2d.s.v4i32");
	const_func clang_int4 gather_imagei(imagecube_array_t image, metal_sampler_t smplr, clang_float3 coord, uint32_t layer, int32_t component = 0) asm("air.gather_texture_cube_array.s.v4i32");
	const_func clang_int4 gather_imagei(imagecube_t image, metal_sampler_t smplr, clang_float3 coord, int32_t component = 0) asm("air.gather_texture_cube.s.v4i32");
	const_func clang_uint4 gather_imageui(image2d_array_t image, metal_sampler_t smplr, clang_float2 coord, uint32_t layer, bool enable_offset = true, clang_int2 offset = 0, int32_t component = 0) asm("air.gather_texture_2d_array.u.v4i32");
	const_func clang_uint4 gather_imageui(image2d_t image, metal_sampler_t smplr, clang_float2 coord, bool enable_offset = true, clang_int2 offset = 0, int32_t component = 0) asm("air.gather_texture_2d.u.v4i32");
	const_func clang_uint4 gather_imageui(imagecube_array_t image, metal_sampler_t smplr, clang_float3 coord, uint32_t layer, int32_t component = 0) asm("air.gather_texture_cube_array.u.v4i32");
	const_func clang_uint4 gather_imageui(imagecube_t image, metal_sampler_t smplr, clang_float3 coord, int32_t component = 0) asm("air.gather_texture_cube.u.v4i32");
#endif
	
}

#endif

#endif
