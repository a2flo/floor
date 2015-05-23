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

#ifndef __FLOOR_COMPUTE_DEVICE_OPENCL_IMAGE_HPP__
#define __FLOOR_COMPUTE_DEVICE_OPENCL_IMAGE_HPP__

#if defined(FLOOR_COMPUTE_OPENCL)

// opencl filtering modes
#define FLOOR_OPENCL_ADDRESS_NONE                0
#define FLOOR_OPENCL_ADDRESS_CLAMP_TO_EDGE       2
#define FLOOR_OPENCL_ADDRESS_CLAMP               4
#define FLOOR_OPENCL_ADDRESS_REPEAT              6
#define FLOOR_OPENCL_ADDRESS_MIRRORED_REPEAT     8
#define FLOOR_OPENCL_NORMALIZED_COORDS_FALSE     0
#define FLOOR_OPENCL_NORMALIZED_COORDS_TRUE      1
#define FLOOR_OPENCL_FILTER_NEAREST              0x10
#define FLOOR_OPENCL_FILTER_LINEAR               0x20

// opencl image read functions
opencl_float4 opencl_const_func read_imagef(image1d_t image, sampler_t sampler, int coord);
opencl_float4 opencl_const_func read_imagef(image1d_t image, sampler_t sampler, float coord);
opencl_int4 opencl_const_func read_imagei(image1d_t image, sampler_t sampler, int coord);
opencl_int4 opencl_const_func read_imagei(image1d_t image, sampler_t sampler, float coord);
opencl_uint4 opencl_const_func read_imageui(image1d_t image, sampler_t sampler, int coord);
opencl_uint4 opencl_const_func read_imageui(image1d_t image, sampler_t sampler, float coord);
opencl_float4 opencl_const_func read_imagef(image1d_t image, int coord);
opencl_int4 opencl_const_func read_imagei(image1d_t image, int coord);
opencl_uint4 opencl_const_func read_imageui(image1d_t image, int coord);

opencl_float4 opencl_const_func read_imagef(image1d_buffer_t image, int coord);
opencl_int4 opencl_const_func read_imagei(image1d_buffer_t image, int coord);
opencl_uint4 opencl_const_func read_imageui(image1d_buffer_t image, int coord);

opencl_float4 opencl_const_func read_imagef(image1d_array_t image, sampler_t sampler, opencl_int2 coord);
opencl_float4 opencl_const_func read_imagef(image1d_array_t image, sampler_t sampler, opencl_float2 coord);
opencl_int4 opencl_const_func read_imagei(image1d_array_t image, sampler_t sampler, opencl_int2 coord);
opencl_int4 opencl_const_func read_imagei(image1d_array_t image, sampler_t sampler, opencl_float2 coord);
opencl_uint4 opencl_const_func read_imageui(image1d_array_t image, sampler_t sampler, opencl_int2 coord);
opencl_uint4 opencl_const_func read_imageui(image1d_array_t image, sampler_t sampler, opencl_float2 coord);
opencl_float4 opencl_const_func read_imagef(image1d_array_t image, opencl_int2 coord);
opencl_int4 opencl_const_func read_imagei(image1d_array_t image, opencl_int2 coord);
opencl_uint4 opencl_const_func read_imageui(image1d_array_t image, opencl_int2 coord);

opencl_float4 opencl_const_func read_imagef(image2d_t image, sampler_t sampler, opencl_int2 coord);
opencl_float4 opencl_const_func read_imagef(image2d_t image, sampler_t sampler, opencl_float2 coord);
opencl_half4 opencl_const_func read_imageh(image2d_t image, sampler_t sampler, opencl_int2 coord);
opencl_half4 opencl_const_func read_imageh(image2d_t image, sampler_t sampler, opencl_float2 coord);
opencl_int4 opencl_const_func read_imagei(image2d_t image, sampler_t sampler, opencl_int2 coord);
opencl_int4 opencl_const_func read_imagei(image2d_t image, sampler_t sampler, opencl_float2 coord);
opencl_uint4 opencl_const_func read_imageui(image2d_t image, sampler_t sampler, opencl_int2 coord);
opencl_uint4 opencl_const_func read_imageui(image2d_t image, sampler_t sampler, opencl_float2 coord);
opencl_float4 opencl_const_func read_imagef (image2d_t image, opencl_int2 coord);
opencl_int4 opencl_const_func read_imagei(image2d_t image, opencl_int2 coord);
opencl_uint4 opencl_const_func read_imageui(image2d_t image, opencl_int2 coord);

opencl_float4 opencl_const_func read_imagef(image2d_array_t image, opencl_int4 coord);
opencl_int4 opencl_const_func read_imagei(image2d_array_t image, opencl_int4 coord);
opencl_uint4 opencl_const_func read_imageui(image2d_array_t image, opencl_int4 coord);
opencl_float4 opencl_const_func read_imagef(image2d_array_t image_array, sampler_t sampler, opencl_int4 coord);
opencl_float4 opencl_const_func read_imagef(image2d_array_t image_array, sampler_t sampler, opencl_float4 coord);
opencl_int4 opencl_const_func read_imagei(image2d_array_t image_array, sampler_t sampler, opencl_int4 coord);
opencl_int4 opencl_const_func read_imagei(image2d_array_t image_array, sampler_t sampler, opencl_float4 coord);
opencl_uint4 opencl_const_func read_imageui(image2d_array_t image_array, sampler_t sampler, opencl_int4 coord);
opencl_uint4 opencl_const_func read_imageui(image2d_array_t image_array, sampler_t sampler, opencl_float4 coord);

opencl_float4 opencl_const_func read_imagef(image2d_msaa_t image, opencl_int2 coord, int sample);
opencl_int4 opencl_const_func read_imagei(image2d_msaa_t image, opencl_int2 coord, int sample);
opencl_uint4 opencl_const_func read_imageui(image2d_msaa_t image, opencl_int2 coord, int sample);

opencl_float4 opencl_const_func read_imagef(image2d_array_msaa_t image, opencl_int4 coord, int sample);
opencl_int4 opencl_const_func read_imagei(image2d_array_msaa_t image, opencl_int4 coord, int sample);
opencl_uint4 opencl_const_func read_imageui(image2d_array_msaa_t image, opencl_int4 coord, int sample);

float opencl_const_func read_imagef(image2d_msaa_depth_t image, opencl_int2 coord, int sample);
float opencl_const_func read_imagef(image2d_array_msaa_depth_t image, opencl_int4 coord, int sample);

float opencl_const_func read_imagef(image2d_depth_t image, sampler_t sampler, opencl_int2 coord);
float opencl_const_func read_imagef(image2d_depth_t image, sampler_t sampler, opencl_float2 coord);
float opencl_const_func read_imagef(image2d_depth_t image, opencl_int2 coord);

float opencl_const_func read_imagef(image2d_array_depth_t image, sampler_t sampler, opencl_int4 coord);
float opencl_const_func read_imagef(image2d_array_depth_t image, sampler_t sampler, opencl_float4 coord);
float opencl_const_func read_imagef(image2d_array_depth_t image, opencl_int4 coord);

opencl_float4 opencl_const_func read_imagef(image3d_t image, sampler_t sampler, opencl_int4 coord);
opencl_float4 opencl_const_func read_imagef(image3d_t image, sampler_t sampler, opencl_float4 coord);
opencl_int4 opencl_const_func read_imagei(image3d_t image, sampler_t sampler, opencl_int4 coord);
opencl_int4 opencl_const_func read_imagei(image3d_t image, sampler_t sampler, opencl_float4 coord);
opencl_uint4 opencl_const_func read_imageui(image3d_t image, sampler_t sampler, opencl_int4 coord);
opencl_uint4 opencl_const_func read_imageui(image3d_t image, sampler_t sampler, opencl_float4 coord);
opencl_float4 opencl_const_func read_imagef(image3d_t image, opencl_int4 coord);
opencl_int4 opencl_const_func read_imagei(image3d_t image, opencl_int4 coord);
opencl_uint4 opencl_const_func read_imageui(image3d_t image, opencl_int4 coord);

opencl_float4 opencl_const_func read_imagef(image2d_array_t image, sampler_t sampler, opencl_int4 coord);
opencl_float4 opencl_const_func read_imagef(image2d_array_t image, sampler_t sampler, opencl_float4 coord);
opencl_int4 opencl_const_func read_imagei(image2d_array_t image, sampler_t sampler, opencl_int4 coord);
opencl_int4 opencl_const_func read_imagei(image2d_array_t image, sampler_t sampler, opencl_float4 coord);
opencl_uint4 opencl_const_func read_imageui(image2d_array_t image, sampler_t sampler, opencl_int4 coord);
opencl_uint4 opencl_const_func read_imageui(image2d_array_t image, sampler_t sampler, opencl_float4 coord);

// opencl image write functions
void write_imagef(image1d_t image, int coord, opencl_float4 color);
void write_imagei(image1d_t image, int coord, opencl_int4 color);
void write_imageui(image1d_t image, int coord, opencl_uint4 color);

void write_imagef(image1d_buffer_t image, int coord, opencl_float4 color);
void write_imagei(image1d_buffer_t image, int coord, opencl_int4 color);
void write_imageui(image1d_buffer_t image, int coord, opencl_uint4 color);

void write_imagef(image1d_array_t image, opencl_int2 coord, opencl_float4 color);
void write_imagei(image1d_array_t image, opencl_int2 coord, opencl_int4 color);
void write_imageui(image1d_array_t image, opencl_int2 coord, opencl_uint4 color);

void write_imagef(image2d_t image, opencl_int2 coord, opencl_float4 color);
void write_imagei(image2d_t image, opencl_int2 coord, opencl_int4 color);
void write_imageui(image2d_t image, opencl_int2 coord, opencl_uint4 color);
void write_imageh(image2d_t image, opencl_int2 coord, opencl_half4 color);

void write_imagef(image2d_array_t image, opencl_int4 coord, opencl_float4 color);
void write_imagei(image2d_array_t image, opencl_int4 coord, opencl_int4 color);
void write_imageui(image2d_array_t image, opencl_int4 coord, opencl_uint4 color);
void write_imagef(image2d_array_t image_array, opencl_int4 coord, opencl_float4 color);
void write_imagei(image2d_array_t image_array, opencl_int4 coord, opencl_int4 color);
void write_imageui(image2d_array_t image_array, opencl_int4 coord, opencl_uint4 color);

void write_imagef(image2d_depth_t image, opencl_int2 coord, float depth);

void write_imagef(image2d_array_depth_t image, opencl_int4 coord, float depth);

void write_imagef(image3d_t image, opencl_int4 coord, opencl_float4 color);
void write_imagei(image3d_t image, opencl_int4 coord, opencl_int4 color);
void write_imageui(image3d_t image, opencl_int4 coord, opencl_uint4 color);
void write_imageh(image3d_t image, opencl_int4 coord, opencl_half4 color);

// floor image read/write wrappers
//template <typename ret_type = typename image_texel_data_type<>::type>
floor_inline_always auto read(const image2d_t& img, const int2& coord) {
#if defined(FLOOR_COMPUTE_SPIR)
	const sampler_t smplr = FLOOR_OPENCL_NORMALIZED_COORDS_FALSE | FLOOR_OPENCL_ADDRESS_CLAMP_TO_EDGE | FLOOR_OPENCL_FILTER_NEAREST;
	return float4::from_clang_vector(read_imagef(img, smplr, coord));
#else
	return float4::from_clang_vector(read_imagef(img, coord));
#endif
}
floor_inline_always auto read(const image2d_t& img, const float2& coord) {
	const sampler_t smplr = FLOOR_OPENCL_NORMALIZED_COORDS_TRUE | FLOOR_OPENCL_ADDRESS_CLAMP_TO_EDGE | FLOOR_OPENCL_FILTER_NEAREST;
	return float4::from_clang_vector(read_imagef(img, smplr, coord));
}

floor_inline_always void write(const image2d_t& img, const int2& coord, const float4& data) {
	write_imagef(img, coord, data);
}

// COMPUTE_IMAGE_TYPE -> opencl image*d_*t type mapping
#define OCL_IMAGE_MASK (COMPUTE_IMAGE_TYPE::__DIM_MASK | COMPUTE_IMAGE_TYPE::__DIM_STORAGE_MASK | COMPUTE_IMAGE_TYPE::FLAG_DEPTH | COMPUTE_IMAGE_TYPE::FLAG_ARRAY | COMPUTE_IMAGE_TYPE::FLAG_BUFFER | COMPUTE_IMAGE_TYPE::FLAG_CUBE | COMPUTE_IMAGE_TYPE::FLAG_MSAA)

template <COMPUTE_IMAGE_TYPE image_type, typename = void> struct ocl_image_type {};

template <COMPUTE_IMAGE_TYPE image_type>
struct ocl_image_type<image_type, enable_if_t<(image_type & OCL_IMAGE_MASK) == COMPUTE_IMAGE_TYPE::IMAGE_1D>> {
	typedef image1d_t type;
};

template <COMPUTE_IMAGE_TYPE image_type>
struct ocl_image_type<image_type, enable_if_t<(image_type & OCL_IMAGE_MASK) == COMPUTE_IMAGE_TYPE::IMAGE_1D_ARRAY>> {
	typedef image1d_array_t type;
};

template <COMPUTE_IMAGE_TYPE image_type>
struct ocl_image_type<image_type, enable_if_t<(image_type & OCL_IMAGE_MASK) == COMPUTE_IMAGE_TYPE::IMAGE_1D_BUFFER>> {
	typedef image1d_buffer_t type;
};

template <COMPUTE_IMAGE_TYPE image_type>
struct ocl_image_type<image_type, enable_if_t<(image_type & OCL_IMAGE_MASK) == COMPUTE_IMAGE_TYPE::IMAGE_2D>> {
	typedef image2d_t type;
};

template <COMPUTE_IMAGE_TYPE image_type>
struct ocl_image_type<image_type, enable_if_t<(image_type & OCL_IMAGE_MASK) == COMPUTE_IMAGE_TYPE::IMAGE_2D_ARRAY>> {
	typedef image2d_array_t type;
};

template <COMPUTE_IMAGE_TYPE image_type>
struct ocl_image_type<image_type, enable_if_t<(image_type & OCL_IMAGE_MASK) == COMPUTE_IMAGE_TYPE::IMAGE_2D_MSAA>> {
	typedef image2d_msaa_t type;
};

template <COMPUTE_IMAGE_TYPE image_type>
struct ocl_image_type<image_type, enable_if_t<(image_type & OCL_IMAGE_MASK) == COMPUTE_IMAGE_TYPE::IMAGE_2D_MSAA_ARRAY>> {
	typedef image2d_array_msaa_t type;
};

// NOTE: also applies to combined stencil format
template <COMPUTE_IMAGE_TYPE image_type>
struct ocl_image_type<image_type, enable_if_t<(image_type & OCL_IMAGE_MASK) == (COMPUTE_IMAGE_TYPE::IMAGE_2D |
																				COMPUTE_IMAGE_TYPE::FLAG_DEPTH)>> {
	typedef image2d_depth_t type;
};

template <COMPUTE_IMAGE_TYPE image_type>
struct ocl_image_type<image_type, enable_if_t<(image_type & OCL_IMAGE_MASK) == (COMPUTE_IMAGE_TYPE::IMAGE_2D_ARRAY |
																				COMPUTE_IMAGE_TYPE::FLAG_DEPTH)>> {
	typedef image2d_array_depth_t type;
};

template <COMPUTE_IMAGE_TYPE image_type>
struct ocl_image_type<image_type, enable_if_t<(image_type & OCL_IMAGE_MASK) == (COMPUTE_IMAGE_TYPE::IMAGE_2D_MSAA |
																				COMPUTE_IMAGE_TYPE::FLAG_DEPTH)>> {
	typedef image2d_msaa_depth_t type;
};

template <COMPUTE_IMAGE_TYPE image_type>
struct ocl_image_type<image_type, enable_if_t<(image_type & OCL_IMAGE_MASK) == (COMPUTE_IMAGE_TYPE::IMAGE_2D_MSAA_ARRAY |
																				COMPUTE_IMAGE_TYPE::FLAG_DEPTH)>> {
	typedef image2d_array_msaa_depth_t type;
};

template <COMPUTE_IMAGE_TYPE image_type>
struct ocl_image_type<image_type, enable_if_t<(image_type & OCL_IMAGE_MASK) == COMPUTE_IMAGE_TYPE::IMAGE_3D>> {
	typedef image3d_t type;
};

// TODO: not sure if and how these are actually supported (considering they are both 2D arrays, use that type - not sure about filtering)
template <COMPUTE_IMAGE_TYPE image_type>
struct ocl_image_type<image_type, enable_if_t<(image_type & OCL_IMAGE_MASK) == COMPUTE_IMAGE_TYPE::IMAGE_CUBE>> {
	typedef image2d_array_t type;
};

template <COMPUTE_IMAGE_TYPE image_type>
struct ocl_image_type<image_type, enable_if_t<(image_type & OCL_IMAGE_MASK) == COMPUTE_IMAGE_TYPE::IMAGE_CUBE_ARRAY>> {
	typedef image2d_array_t type;
};

#undef OCL_IMAGE_MASK

// image access qualifiers are function parameter types/attributes that aren't inherited from a typedef or using decl
// -> macros to the rescue
#define ro_image read_only ocl_image
#define wo_image write_only ocl_image
#define rw_image read_write ocl_image
template <COMPUTE_IMAGE_TYPE image_type> using ocl_image = typename ocl_image_type<image_type>::type;

#endif

#endif
