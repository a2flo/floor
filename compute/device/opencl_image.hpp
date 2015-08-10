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
clang_float4 opencl_const_func read_imagef(image1d_t image, sampler_t sampler, int coord);
clang_float4 opencl_const_func read_imagef(image1d_t image, sampler_t sampler, float coord);
clang_int4 opencl_const_func read_imagei(image1d_t image, sampler_t sampler, int coord);
clang_int4 opencl_const_func read_imagei(image1d_t image, sampler_t sampler, float coord);
clang_uint4 opencl_const_func read_imageui(image1d_t image, sampler_t sampler, int coord);
clang_uint4 opencl_const_func read_imageui(image1d_t image, sampler_t sampler, float coord);
clang_float4 opencl_const_func read_imagef(image1d_t image, int coord);
clang_int4 opencl_const_func read_imagei(image1d_t image, int coord);
clang_uint4 opencl_const_func read_imageui(image1d_t image, int coord);

clang_float4 opencl_const_func read_imagef(image1d_buffer_t image, int coord);
clang_int4 opencl_const_func read_imagei(image1d_buffer_t image, int coord);
clang_uint4 opencl_const_func read_imageui(image1d_buffer_t image, int coord);

clang_float4 opencl_const_func read_imagef(image1d_array_t image, sampler_t sampler, clang_int2 coord);
clang_float4 opencl_const_func read_imagef(image1d_array_t image, sampler_t sampler, clang_float2 coord);
clang_int4 opencl_const_func read_imagei(image1d_array_t image, sampler_t sampler, clang_int2 coord);
clang_int4 opencl_const_func read_imagei(image1d_array_t image, sampler_t sampler, clang_float2 coord);
clang_uint4 opencl_const_func read_imageui(image1d_array_t image, sampler_t sampler, clang_int2 coord);
clang_uint4 opencl_const_func read_imageui(image1d_array_t image, sampler_t sampler, clang_float2 coord);
clang_float4 opencl_const_func read_imagef(image1d_array_t image, clang_int2 coord);
clang_int4 opencl_const_func read_imagei(image1d_array_t image, clang_int2 coord);
clang_uint4 opencl_const_func read_imageui(image1d_array_t image, clang_int2 coord);

clang_float4 opencl_const_func read_imagef(image2d_t image, sampler_t sampler, clang_int2 coord);
clang_float4 opencl_const_func read_imagef(image2d_t image, sampler_t sampler, clang_float2 coord);
clang_half4 opencl_const_func read_imageh(image2d_t image, sampler_t sampler, clang_int2 coord);
clang_half4 opencl_const_func read_imageh(image2d_t image, sampler_t sampler, clang_float2 coord);
clang_int4 opencl_const_func read_imagei(image2d_t image, sampler_t sampler, clang_int2 coord);
clang_int4 opencl_const_func read_imagei(image2d_t image, sampler_t sampler, clang_float2 coord);
clang_uint4 opencl_const_func read_imageui(image2d_t image, sampler_t sampler, clang_int2 coord);
clang_uint4 opencl_const_func read_imageui(image2d_t image, sampler_t sampler, clang_float2 coord);
clang_float4 opencl_const_func read_imagef (image2d_t image, clang_int2 coord);
clang_int4 opencl_const_func read_imagei(image2d_t image, clang_int2 coord);
clang_uint4 opencl_const_func read_imageui(image2d_t image, clang_int2 coord);

clang_float4 opencl_const_func read_imagef(image2d_array_t image, clang_int4 coord);
clang_int4 opencl_const_func read_imagei(image2d_array_t image, clang_int4 coord);
clang_uint4 opencl_const_func read_imageui(image2d_array_t image, clang_int4 coord);
clang_float4 opencl_const_func read_imagef(image2d_array_t image_array, sampler_t sampler, clang_int4 coord);
clang_float4 opencl_const_func read_imagef(image2d_array_t image_array, sampler_t sampler, clang_float4 coord);
clang_int4 opencl_const_func read_imagei(image2d_array_t image_array, sampler_t sampler, clang_int4 coord);
clang_int4 opencl_const_func read_imagei(image2d_array_t image_array, sampler_t sampler, clang_float4 coord);
clang_uint4 opencl_const_func read_imageui(image2d_array_t image_array, sampler_t sampler, clang_int4 coord);
clang_uint4 opencl_const_func read_imageui(image2d_array_t image_array, sampler_t sampler, clang_float4 coord);

clang_float4 opencl_const_func read_imagef(image2d_msaa_t image, clang_int2 coord, int sample);
clang_int4 opencl_const_func read_imagei(image2d_msaa_t image, clang_int2 coord, int sample);
clang_uint4 opencl_const_func read_imageui(image2d_msaa_t image, clang_int2 coord, int sample);

clang_float4 opencl_const_func read_imagef(image2d_array_msaa_t image, clang_int4 coord, int sample);
clang_int4 opencl_const_func read_imagei(image2d_array_msaa_t image, clang_int4 coord, int sample);
clang_uint4 opencl_const_func read_imageui(image2d_array_msaa_t image, clang_int4 coord, int sample);

float opencl_const_func read_imagef(image2d_msaa_depth_t image, clang_int2 coord, int sample);
float opencl_const_func read_imagef(image2d_array_msaa_depth_t image, clang_int4 coord, int sample);

float opencl_const_func read_imagef(image2d_depth_t image, sampler_t sampler, clang_int2 coord);
float opencl_const_func read_imagef(image2d_depth_t image, sampler_t sampler, clang_float2 coord);
float opencl_const_func read_imagef(image2d_depth_t image, clang_int2 coord);

float opencl_const_func read_imagef(image2d_array_depth_t image, sampler_t sampler, clang_int4 coord);
float opencl_const_func read_imagef(image2d_array_depth_t image, sampler_t sampler, clang_float4 coord);
float opencl_const_func read_imagef(image2d_array_depth_t image, clang_int4 coord);

clang_float4 opencl_const_func read_imagef(image3d_t image, sampler_t sampler, clang_int4 coord);
clang_float4 opencl_const_func read_imagef(image3d_t image, sampler_t sampler, clang_float4 coord);
clang_int4 opencl_const_func read_imagei(image3d_t image, sampler_t sampler, clang_int4 coord);
clang_int4 opencl_const_func read_imagei(image3d_t image, sampler_t sampler, clang_float4 coord);
clang_uint4 opencl_const_func read_imageui(image3d_t image, sampler_t sampler, clang_int4 coord);
clang_uint4 opencl_const_func read_imageui(image3d_t image, sampler_t sampler, clang_float4 coord);
clang_float4 opencl_const_func read_imagef(image3d_t image, clang_int4 coord);
clang_int4 opencl_const_func read_imagei(image3d_t image, clang_int4 coord);
clang_uint4 opencl_const_func read_imageui(image3d_t image, clang_int4 coord);

clang_float4 opencl_const_func read_imagef(image2d_array_t image, sampler_t sampler, clang_int4 coord);
clang_float4 opencl_const_func read_imagef(image2d_array_t image, sampler_t sampler, clang_float4 coord);
clang_int4 opencl_const_func read_imagei(image2d_array_t image, sampler_t sampler, clang_int4 coord);
clang_int4 opencl_const_func read_imagei(image2d_array_t image, sampler_t sampler, clang_float4 coord);
clang_uint4 opencl_const_func read_imageui(image2d_array_t image, sampler_t sampler, clang_int4 coord);
clang_uint4 opencl_const_func read_imageui(image2d_array_t image, sampler_t sampler, clang_float4 coord);

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

// actual image implementation is shared with metal
#include <floor/compute/device/opaque_image.hpp>

#endif

#endif
