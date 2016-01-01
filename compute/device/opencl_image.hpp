/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2016 Florian Ziesche
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

#ifndef __FLOOR_COMPUTE_DEVICE_OPENCL_IMAGE_HPP__
#define __FLOOR_COMPUTE_DEVICE_OPENCL_IMAGE_HPP__

#if defined(FLOOR_COMPUTE_OPENCL)

// opencl image read functions
const_func clang_float4 read_imagef(image1d_t image, sampler_t smplr, int coord);
const_func clang_float4 read_imagef(image1d_t image, sampler_t smplr, float coord);
const_func clang_int4 read_imagei(image1d_t image, sampler_t smplr, int coord);
const_func clang_int4 read_imagei(image1d_t image, sampler_t smplr, float coord);
const_func clang_uint4 read_imageui(image1d_t image, sampler_t smplr, int coord);
const_func clang_uint4 read_imageui(image1d_t image, sampler_t smplr, float coord);
const_func clang_float4 read_imagef(image1d_t image, int coord);
const_func clang_int4 read_imagei(image1d_t image, int coord);
const_func clang_uint4 read_imageui(image1d_t image, int coord);

const_func clang_float4 read_imagef(image1d_buffer_t image, int coord);
const_func clang_int4 read_imagei(image1d_buffer_t image, int coord);
const_func clang_uint4 read_imageui(image1d_buffer_t image, int coord);

const_func clang_float4 read_imagef(image1d_array_t image, sampler_t smplr, clang_int2 coord);
const_func clang_float4 read_imagef(image1d_array_t image, sampler_t smplr, clang_float2 coord);
const_func clang_int4 read_imagei(image1d_array_t image, sampler_t smplr, clang_int2 coord);
const_func clang_int4 read_imagei(image1d_array_t image, sampler_t smplr, clang_float2 coord);
const_func clang_uint4 read_imageui(image1d_array_t image, sampler_t smplr, clang_int2 coord);
const_func clang_uint4 read_imageui(image1d_array_t image, sampler_t smplr, clang_float2 coord);
const_func clang_float4 read_imagef(image1d_array_t image, clang_int2 coord);
const_func clang_int4 read_imagei(image1d_array_t image, clang_int2 coord);
const_func clang_uint4 read_imageui(image1d_array_t image, clang_int2 coord);

const_func clang_float4 read_imagef(image2d_t image, sampler_t smplr, clang_int2 coord);
const_func clang_float4 read_imagef(image2d_t image, sampler_t smplr, clang_float2 coord);
const_func clang_half4 read_imageh(image2d_t image, sampler_t smplr, clang_int2 coord);
const_func clang_half4 read_imageh(image2d_t image, sampler_t smplr, clang_float2 coord);
const_func clang_int4 read_imagei(image2d_t image, sampler_t smplr, clang_int2 coord);
const_func clang_int4 read_imagei(image2d_t image, sampler_t smplr, clang_float2 coord);
const_func clang_uint4 read_imageui(image2d_t image, sampler_t smplr, clang_int2 coord);
const_func clang_uint4 read_imageui(image2d_t image, sampler_t smplr, clang_float2 coord);
const_func clang_float4 read_imagef (image2d_t image, clang_int2 coord);
const_func clang_int4 read_imagei(image2d_t image, clang_int2 coord);
const_func clang_uint4 read_imageui(image2d_t image, clang_int2 coord);

const_func clang_float4 read_imagef(image2d_array_t image, clang_int4 coord);
const_func clang_int4 read_imagei(image2d_array_t image, clang_int4 coord);
const_func clang_uint4 read_imageui(image2d_array_t image, clang_int4 coord);
const_func clang_float4 read_imagef(image2d_array_t image_array, sampler_t smplr, clang_int4 coord);
const_func clang_float4 read_imagef(image2d_array_t image_array, sampler_t smplr, clang_float4 coord);
const_func clang_int4 read_imagei(image2d_array_t image_array, sampler_t smplr, clang_int4 coord);
const_func clang_int4 read_imagei(image2d_array_t image_array, sampler_t smplr, clang_float4 coord);
const_func clang_uint4 read_imageui(image2d_array_t image_array, sampler_t smplr, clang_int4 coord);
const_func clang_uint4 read_imageui(image2d_array_t image_array, sampler_t smplr, clang_float4 coord);

const_func clang_float4 read_imagef(image2d_msaa_t image, clang_int2 coord, int sample);
const_func clang_int4 read_imagei(image2d_msaa_t image, clang_int2 coord, int sample);
const_func clang_uint4 read_imageui(image2d_msaa_t image, clang_int2 coord, int sample);

const_func clang_float4 read_imagef(image2d_array_msaa_t image, clang_int4 coord, int sample);
const_func clang_int4 read_imagei(image2d_array_msaa_t image, clang_int4 coord, int sample);
const_func clang_uint4 read_imageui(image2d_array_msaa_t image, clang_int4 coord, int sample);

const_func float read_imagef(image2d_msaa_depth_t image, clang_int2 coord, int sample);
const_func float read_imagef(image2d_array_msaa_depth_t image, clang_int4 coord, int sample);

const_func float read_imagef(image2d_depth_t image, sampler_t smplr, clang_int2 coord);
const_func float read_imagef(image2d_depth_t image, sampler_t smplr, clang_float2 coord);
const_func float read_imagef(image2d_depth_t image, clang_int2 coord);

const_func float read_imagef(image2d_array_depth_t image, sampler_t smplr, clang_int4 coord);
const_func float read_imagef(image2d_array_depth_t image, sampler_t smplr, clang_float4 coord);
const_func float read_imagef(image2d_array_depth_t image, clang_int4 coord);

const_func clang_float4 read_imagef(image3d_t image, sampler_t smplr, clang_int4 coord);
const_func clang_float4 read_imagef(image3d_t image, sampler_t smplr, clang_float4 coord);
const_func clang_int4 read_imagei(image3d_t image, sampler_t smplr, clang_int4 coord);
const_func clang_int4 read_imagei(image3d_t image, sampler_t smplr, clang_float4 coord);
const_func clang_uint4 read_imageui(image3d_t image, sampler_t smplr, clang_int4 coord);
const_func clang_uint4 read_imageui(image3d_t image, sampler_t smplr, clang_float4 coord);
const_func clang_float4 read_imagef(image3d_t image, clang_int4 coord);
const_func clang_int4 read_imagei(image3d_t image, clang_int4 coord);
const_func clang_uint4 read_imageui(image3d_t image, clang_int4 coord);

const_func clang_float4 read_imagef(image2d_array_t image, sampler_t smplr, clang_int4 coord);
const_func clang_float4 read_imagef(image2d_array_t image, sampler_t smplr, clang_float4 coord);
const_func clang_int4 read_imagei(image2d_array_t image, sampler_t smplr, clang_int4 coord);
const_func clang_int4 read_imagei(image2d_array_t image, sampler_t smplr, clang_float4 coord);
const_func clang_uint4 read_imageui(image2d_array_t image, sampler_t smplr, clang_int4 coord);
const_func clang_uint4 read_imageui(image2d_array_t image, sampler_t smplr, clang_float4 coord);

// opencl image write functions
void write_imagef(image1d_t image, int coord, clang_float4 color);
void write_imagei(image1d_t image, int coord, clang_int4 color);
void write_imageui(image1d_t image, int coord, clang_uint4 color);

void write_imagef(image1d_buffer_t image, int coord, clang_float4 color);
void write_imagei(image1d_buffer_t image, int coord, clang_int4 color);
void write_imageui(image1d_buffer_t image, int coord, clang_uint4 color);

void write_imagef(image1d_array_t image, clang_int2 coord, clang_float4 color);
void write_imagei(image1d_array_t image, clang_int2 coord, clang_int4 color);
void write_imageui(image1d_array_t image, clang_int2 coord, clang_uint4 color);

void write_imagef(image2d_t image, clang_int2 coord, clang_float4 color);
void write_imagei(image2d_t image, clang_int2 coord, clang_int4 color);
void write_imageui(image2d_t image, clang_int2 coord, clang_uint4 color);
void write_imageh(image2d_t image, clang_int2 coord, clang_half4 color);

void write_imagef(image2d_array_t image, clang_int4 coord, clang_float4 color);
void write_imagei(image2d_array_t image, clang_int4 coord, clang_int4 color);
void write_imageui(image2d_array_t image, clang_int4 coord, clang_uint4 color);
void write_imagef(image2d_array_t image_array, clang_int4 coord, clang_float4 color);
void write_imagei(image2d_array_t image_array, clang_int4 coord, clang_int4 color);
void write_imageui(image2d_array_t image_array, clang_int4 coord, clang_uint4 color);

void write_imagef(image2d_depth_t image, clang_int2 coord, float depth);

void write_imagef(image2d_array_depth_t image, clang_int4 coord, float depth);

void write_imagef(image3d_t image, clang_int4 coord, clang_float4 color);
void write_imagei(image3d_t image, clang_int4 coord, clang_int4 color);
void write_imageui(image3d_t image, clang_int4 coord, clang_uint4 color);
void write_imageh(image3d_t image, clang_int4 coord, clang_half4 color);

namespace opencl_image {
	//////////////////////////////////////////
	// opencl sampler types/modes
	namespace sampler {
		enum ADDRESS_MODE : uint32_t {
			NONE			= 0,
			CLAMP_TO_ZERO	= 4,
			CLAMP_TO_EDGE	= 2,
			REPEAT			= 6,
			MIRRORED_REPEAT	= 8
		};
		enum FILTER_MODE : uint32_t {
			NEAREST			= 0x10,
			LINEAR			= 0x20
		};
		enum COORD_MODE : uint32_t {
			NORMALIZED		= 0,
			PIXEL			= 1
		};
	};
	
	//////////////////////////////////////////
	// opencl/spir image forwarders (for convenience)
	// NOTE: unavailable functions exist for compilation purposes only (sema checking) and will obviously never function
	// NOTE: MSAA write support is nonexistent, so no sample parameter is needed
	// NOTE: cube map support is nonexistent, so no cube functions exist
	
	// read float with int coords
	floor_inline_always const_func clang_float4 read_float(image1d_t image, sampler_t smplr, int32_t coord, uint32_t, uint32_t) { return read_imagef(image, smplr, coord); }
	floor_inline_always const_func clang_float4 read_float(image1d_array_t image, sampler_t smplr, int32_t coord, uint32_t layer, uint32_t) { return read_imagef(image, smplr, (clang_int2)(coord, layer)); }
	floor_inline_always const_func clang_float4 read_float(image2d_t image, sampler_t smplr, clang_int2 coord, uint32_t, uint32_t) { return read_imagef(image, smplr, coord); }
	floor_inline_always const_func clang_float4 read_float(image2d_array_t image, sampler_t smplr, clang_int2 coord, uint32_t layer, uint32_t) { return read_imagef(image, smplr, (clang_int4)(coord.x, coord.y, layer, 0)); }
	floor_inline_always const_func float read_float(image2d_depth_t image, sampler_t smplr, clang_int2 coord, uint32_t, uint32_t) { return read_imagef(image, smplr, coord); }
	floor_inline_always const_func float read_float(image2d_array_depth_t image, sampler_t smplr, clang_int2 coord, uint32_t layer, uint32_t) { return read_imagef(image, smplr, (clang_int4)(coord.x, coord.y, layer, 0)); }
	floor_inline_always const_func clang_float4 read_float(image2d_msaa_t image, sampler_t smplr, clang_int2 coord, uint32_t, uint32_t sample) { return read_imagef(image, coord, sample); }
	floor_inline_always const_func float read_float(image2d_msaa_depth_t image, sampler_t smplr, clang_int2 coord, uint32_t, uint32_t sample) { return read_imagef(image, coord, sample); }
	floor_inline_always const_func clang_float4 read_float(image3d_t image, sampler_t smplr, clang_int3 coord, uint32_t, uint32_t) { return read_imagef(image, smplr, (clang_int4)(coord.x, coord.y, coord.z, 0)); }
	floor_inline_always const_func clang_float4 read_float(image2d_array_msaa_t image, sampler_t smplr, clang_int2 coord, uint32_t layer, uint32_t sample) { return read_imagef(image, (clang_int4)(coord.x, coord.y, layer, 0), sample); }
	floor_inline_always const_func float read_float(image2d_array_msaa_depth_t image, sampler_t smplr, clang_int2 coord, uint32_t layer, uint32_t sample) { return read_imagef(image, (clang_int4)(coord.x, coord.y, layer, 0), sample); }
	
	// read float with float coords + sampler
	floor_inline_always const_func clang_float4 read_float(image1d_t image, sampler_t smplr, float coord, uint32_t, uint32_t) { return read_imagef(image, smplr, coord); }
	floor_inline_always const_func clang_float4 read_float(image1d_array_t image, sampler_t smplr, float coord, uint32_t layer, uint32_t) { return read_imagef(image, smplr, (clang_float2)(coord, (float)layer)); }
	floor_inline_always const_func clang_float4 read_float(image2d_t image, sampler_t smplr, clang_float2 coord, uint32_t, uint32_t) { return read_imagef(image, smplr, coord); }
	floor_inline_always const_func clang_float4 read_float(image2d_array_t image, sampler_t smplr, clang_float2 coord, uint32_t layer, uint32_t) { return read_imagef(image, smplr, (clang_float4)(coord.x, coord.y, (float)layer, 0.0f)); }
	floor_inline_always const_func float read_float(image2d_depth_t image, sampler_t smplr, clang_float2 coord, uint32_t, uint32_t) { return read_imagef(image, smplr, coord); }
	floor_inline_always const_func float read_float(image2d_array_depth_t image, sampler_t smplr, clang_float2 coord, uint32_t layer, uint32_t) { return read_imagef(image, smplr, (clang_float4)(coord.x, coord.y, (float)layer, 0.0f)); }
	floor_inline_always const_func clang_float4 read_float(image2d_msaa_t image, sampler_t smplr, clang_float2 coord, uint32_t, uint32_t sample); // unavailable
	floor_inline_always const_func float read_float(image2d_msaa_depth_t image, sampler_t smplr, clang_float2 coord, uint32_t, uint32_t sample); // unavailable
	floor_inline_always const_func clang_float4 read_float(image3d_t image, sampler_t smplr, clang_float3 coord, uint32_t, uint32_t) { return read_imagef(image, smplr, (clang_float4)(coord.x, coord.y, coord.z, 0)); }
	floor_inline_always const_func clang_float4 read_float(image2d_array_msaa_t image, sampler_t smplr, clang_float2 coord, uint32_t layer, uint32_t sample); // unavailable
	floor_inline_always const_func float read_float(image2d_array_msaa_depth_t image, sampler_t smplr, clang_float2 coord, uint32_t layer, uint32_t sample); // unavailable
	
	// read int with int coords
	floor_inline_always const_func clang_int4 read_int(image1d_t image, sampler_t smplr, int32_t coord, uint32_t, uint32_t) { return read_imagei(image, smplr, coord); }
	floor_inline_always const_func clang_int4 read_int(image1d_array_t image, sampler_t smplr, int32_t coord, uint32_t layer, uint32_t) { return read_imagei(image, smplr, (clang_int2)(coord, layer)); }
	floor_inline_always const_func clang_int4 read_int(image2d_t image, sampler_t smplr, clang_int2 coord, uint32_t, uint32_t) { return read_imagei(image, smplr, coord); }
	floor_inline_always const_func clang_int4 read_int(image2d_array_t image, sampler_t smplr, clang_int2 coord, uint32_t layer, uint32_t) { return read_imagei(image, smplr, (clang_int4)(coord.x, coord.y, layer, 0)); }
	floor_inline_always const_func int32_t read_int(image2d_depth_t image, sampler_t smplr, clang_int2 coord, uint32_t, uint32_t); // unavailable
	floor_inline_always const_func int32_t read_int(image2d_array_depth_t image, sampler_t smplr, clang_int2 coord, uint32_t layer, uint32_t); // unavailable
	floor_inline_always const_func clang_int4 read_int(image2d_msaa_t image, sampler_t smplr, clang_int2 coord, uint32_t, uint32_t sample) { return read_imagei(image, coord, sample); }
	floor_inline_always const_func int32_t read_int(image2d_msaa_depth_t image, sampler_t smplr, clang_int2 coord, uint32_t, uint32_t sample); // unavailable
	floor_inline_always const_func clang_int4 read_int(image3d_t image, sampler_t smplr, clang_int3 coord, uint32_t, uint32_t) { return read_imagei(image, smplr, (clang_int4)(coord.x, coord.y, coord.z, 0)); }
	floor_inline_always const_func clang_int4 read_int(image2d_array_msaa_t image, sampler_t smplr, clang_int2 coord, uint32_t layer, uint32_t sample) { return read_imagei(image, (clang_int4)(coord.x, coord.y, layer, 0), sample); }
	floor_inline_always const_func int32_t read_int(image2d_array_msaa_depth_t image, sampler_t smplr, clang_int2 coord, uint32_t layer, uint32_t sample); // unavailable
	
	// read int with float coords + sampler
	floor_inline_always const_func clang_int4 read_int(image1d_t image, sampler_t smplr, float coord, uint32_t, uint32_t) { return read_imagei(image, smplr, coord); }
	floor_inline_always const_func clang_int4 read_int(image1d_array_t image, sampler_t smplr, float coord, uint32_t layer, uint32_t) { return read_imagei(image, smplr, (clang_float2)(coord, (float)layer)); }
	floor_inline_always const_func clang_int4 read_int(image2d_t image, sampler_t smplr, clang_float2 coord, uint32_t, uint32_t) { return read_imagei(image, smplr, coord); }
	floor_inline_always const_func clang_int4 read_int(image2d_array_t image, sampler_t smplr, clang_float2 coord, uint32_t layer, uint32_t) { return read_imagei(image, smplr, (clang_float4)(coord.x, coord.y, (float)layer, 0.0f)); }
	floor_inline_always const_func int32_t read_int(image2d_depth_t image, sampler_t smplr, clang_float2 coord, uint32_t, uint32_t); // unavailable
	floor_inline_always const_func int32_t read_int(image2d_array_depth_t image, sampler_t smplr, clang_float2 coord, uint32_t layer, uint32_t); // unavailable
	floor_inline_always const_func clang_int4 read_int(image2d_msaa_t image, sampler_t smplr, clang_float2 coord, uint32_t, uint32_t sample); // unavailable
	floor_inline_always const_func int32_t read_int(image2d_msaa_depth_t image, sampler_t smplr, clang_float2 coord, uint32_t, uint32_t sample); // unavailable
	floor_inline_always const_func clang_int4 read_int(image3d_t image, sampler_t smplr, clang_float3 coord, uint32_t, uint32_t) { return read_imagei(image, smplr, (clang_float4)(coord.x, coord.y, coord.z, 0)); }
	floor_inline_always const_func clang_int4 read_int(image2d_array_msaa_t image, sampler_t smplr, clang_float2 coord, uint32_t layer, uint32_t sample); // unavailable
	floor_inline_always const_func int32_t read_int(image2d_array_msaa_depth_t image, sampler_t smplr, clang_float2 coord, uint32_t layer, uint32_t sample); // unavailable
	
	// read uint with int coords
	floor_inline_always const_func clang_uint4 read_uint(image1d_t image, sampler_t smplr, int32_t coord, uint32_t, uint32_t) { return read_imageui(image, smplr, coord); }
	floor_inline_always const_func clang_uint4 read_uint(image1d_array_t image, sampler_t smplr, int32_t coord, uint32_t layer, uint32_t) { return read_imageui(image, smplr, (clang_int2)(coord, layer)); }
	floor_inline_always const_func clang_uint4 read_uint(image2d_t image, sampler_t smplr, clang_int2 coord, uint32_t, uint32_t) { return read_imageui(image, smplr, coord); }
	floor_inline_always const_func clang_uint4 read_uint(image2d_array_t image, sampler_t smplr, clang_int2 coord, uint32_t layer, uint32_t) { return read_imageui(image, smplr, (clang_int4)(coord.x, coord.y, layer, 0)); }
	floor_inline_always const_func uint32_t read_uint(image2d_depth_t image, sampler_t smplr, clang_int2 coord, uint32_t, uint32_t); // unavailable
	floor_inline_always const_func uint32_t read_uint(image2d_array_depth_t image, sampler_t smplr, clang_int2 coord, uint32_t layer, uint32_t); // unavailable
	floor_inline_always const_func clang_uint4 read_uint(image2d_msaa_t image, sampler_t smplr, clang_int2 coord, uint32_t, uint32_t sample) { return read_imageui(image, coord, sample); }
	floor_inline_always const_func uint32_t read_uint(image2d_msaa_depth_t image, sampler_t smplr, clang_int2 coord, uint32_t, uint32_t sample); // unavailable
	floor_inline_always const_func clang_uint4 read_uint(image3d_t image, sampler_t smplr, clang_int3 coord, uint32_t, uint32_t) { return read_imageui(image, smplr, (clang_int4)(coord.x, coord.y, coord.z, 0)); }
	floor_inline_always const_func clang_uint4 read_uint(image2d_array_msaa_t image, sampler_t smplr, clang_int2 coord, uint32_t layer, uint32_t sample) { return read_imageui(image, (clang_int4)(coord.x, coord.y, layer, 0), sample); }
	floor_inline_always const_func uint32_t read_uint(image2d_array_msaa_depth_t image, sampler_t smplr, clang_int2 coord, uint32_t layer, uint32_t sample); // unavailable
	
	// read uint with float coords + sampler
	floor_inline_always const_func clang_uint4 read_uint(image1d_t image, sampler_t smplr, float coord, uint32_t, uint32_t) { return read_imageui(image, smplr, coord); }
	floor_inline_always const_func clang_uint4 read_uint(image1d_array_t image, sampler_t smplr, float coord, uint32_t layer, uint32_t) { return read_imageui(image, smplr, (clang_float2)(coord, (float)layer)); }
	floor_inline_always const_func clang_uint4 read_uint(image2d_t image, sampler_t smplr, clang_float2 coord, uint32_t, uint32_t) { return read_imageui(image, smplr, coord); }
	floor_inline_always const_func clang_uint4 read_uint(image2d_array_t image, sampler_t smplr, clang_float2 coord, uint32_t layer, uint32_t) { return read_imageui(image, smplr, (clang_float4)(coord.x, coord.y, (float)layer, 0.0f)); }
	floor_inline_always const_func uint32_t read_uint(image2d_depth_t image, sampler_t smplr, clang_float2 coord, uint32_t, uint32_t); // unavailable
	floor_inline_always const_func uint32_t read_uint(image2d_array_depth_t image, sampler_t smplr, clang_float2 coord, uint32_t layer, uint32_t); // unavailable
	floor_inline_always const_func clang_uint4 read_uint(image2d_msaa_t image, sampler_t smplr, clang_float2 coord, uint32_t, uint32_t sample); // unavailable
	floor_inline_always const_func uint32_t read_uint(image2d_msaa_depth_t image, sampler_t smplr, clang_float2 coord, uint32_t, uint32_t sample); // unavailable
	floor_inline_always const_func clang_uint4 read_uint(image3d_t image, sampler_t smplr, clang_float3 coord, uint32_t, uint32_t) { return read_imageui(image, smplr, (clang_float4)(coord.x, coord.y, coord.z, 0)); }
	floor_inline_always const_func clang_uint4 read_uint(image2d_array_msaa_t image, sampler_t smplr, clang_float2 coord, uint32_t layer, uint32_t sample); // unavailable
	floor_inline_always const_func uint32_t read_uint(image2d_array_msaa_depth_t image, sampler_t smplr, clang_float2 coord, uint32_t layer, uint32_t sample); // unavailable
	
	// write float with int coords
	floor_inline_always void write_float(image1d_t image, int32_t coord, uint32_t, clang_float4 data) { write_imagef(image, coord, data); }
	floor_inline_always void write_float(image1d_array_t image, int32_t coord, uint32_t layer, clang_float4 data) { write_imagef(image, (clang_int2)(coord, layer), data); }
	floor_inline_always void write_float(image2d_t image, clang_int2 coord, uint32_t, clang_float4 data) { write_imagef(image, coord, data); }
	floor_inline_always void write_float(image2d_array_t image, clang_int2 coord, uint32_t layer, clang_float4 data) { write_imagef(image, (clang_int4)(coord.x, coord.y, layer, 0), data); }
	floor_inline_always void write_float(image2d_depth_t image, clang_int2 coord, uint32_t, float data) { write_imagef(image, coord, data); }
	floor_inline_always void write_float(image2d_array_depth_t image, clang_int2 coord, uint32_t layer, float data) { write_imagef(image, (clang_int4)(coord.x, coord.y, layer, 0), data); }
	floor_inline_always void write_float(image3d_t image, clang_int3 coord, uint32_t, clang_float4 data) { write_imagef(image, (clang_int4)(coord.x, coord.y, coord.z, 0), data); }
	
	// write int with int coords
	floor_inline_always void write_int(image1d_t image, int32_t coord, uint32_t, clang_int4 data) { write_imagei(image, coord, data); }
	floor_inline_always void write_int(image1d_array_t image, int32_t coord, uint32_t layer, clang_int4 data) { write_imagei(image, (clang_int2)(coord, layer), data); }
	floor_inline_always void write_int(image2d_t image, clang_int2 coord, uint32_t, clang_int4 data) { write_imagei(image, coord, data); }
	floor_inline_always void write_int(image2d_array_t image, clang_int2 coord, uint32_t layer, clang_int4 data) { write_imagei(image, (clang_int4)(coord.x, coord.y, layer, 0), data); }
	floor_inline_always void write_int(image2d_depth_t image, clang_int2 coord, uint32_t, clang_int4 data); // unavailable
	floor_inline_always void write_int(image2d_array_depth_t image, clang_int2 coord, uint32_t layer, clang_int4 data); // unavailable
	floor_inline_always void write_int(image3d_t image, clang_int3 coord, uint32_t, clang_int4 data) { write_imagei(image, (clang_int4)(coord.x, coord.y, coord.z, 0), data); }
	
	// write uint with int coords
	floor_inline_always void write_uint(image1d_t image, int32_t coord, uint32_t, clang_uint4 data) { write_imageui(image, coord, data); }
	floor_inline_always void write_uint(image1d_array_t image, int32_t coord, uint32_t layer, clang_uint4 data) { write_imageui(image, (clang_int2)(coord, layer), data); }
	floor_inline_always void write_uint(image2d_t image, clang_int2 coord, uint32_t, clang_uint4 data) { write_imageui(image, coord, data); }
	floor_inline_always void write_uint(image2d_array_t image, clang_int2 coord, uint32_t layer, clang_uint4 data) { write_imageui(image, (clang_int4)(coord.x, coord.y, layer, 0), data); }
	floor_inline_always void write_uint(image2d_depth_t image, clang_int2 coord, uint32_t, clang_uint4 data); // unavailable
	floor_inline_always void write_uint(image2d_array_depth_t image, clang_int2 coord, uint32_t layer, clang_uint4 data); // unavailable
	floor_inline_always void write_uint(image3d_t image, clang_int3 coord, uint32_t, clang_uint4 data) { write_imageui(image, (clang_int4)(coord.x, coord.y, coord.z, 0), data); }
	
}

#endif

#endif
