/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2022 Florian Ziesche
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

#ifndef __FLOOR_COMPUTE_DEVICE_VULKAN_POST_HPP__
#define __FLOOR_COMPUTE_DEVICE_VULKAN_POST_HPP__

#if defined(FLOOR_COMPUTE_VULKAN)

// NOTE: very wip

//////////////////////////////////////////
// general

// provide copysign implementation (not available in SPIR-V/GLSL)
const_func float copysign(float x, float y) {
	return fabs(x) * (y >= 0.0f ? 1.0f : -1.0f);
}
const_func half copysign(half x, half y) {
	return fabs(x) * (y >= 0.0h ? 1.0h : -1.0h);
}
#if !defined(FLOOR_COMPUTE_NO_DOUBLE)
const_func double copysign(double x, double y) {
	return fabs(x) * (y >= 0.0 ? 1.0 : -1.0);
}
#endif

// NOTE: could use c++ mangling here, but we need to make sure types are correctly handled
const_func uint32_t pack_snorm_4x8_clang(clang_float4) asm("floor.pack_snorm_4x8");
const_func uint32_t pack_unorm_4x8_clang(clang_float4) asm("floor.pack_unorm_4x8");
const_func uint32_t pack_snorm_2x16_clang(clang_float2) asm("floor.pack_snorm_2x16");
const_func uint32_t pack_unorm_2x16_clang(clang_float2) asm("floor.pack_unorm_2x16");
const_func uint32_t pack_half_2x16_clang(clang_float2) asm("floor.pack_half_2x16");
const_func clang_float4 unpack_snorm_4x8_clang(uint32_t) asm("floor.unpack_snorm_4x8");
const_func clang_float4 unpack_unorm_4x8_clang(uint32_t) asm("floor.unpack_unorm_4x8");
const_func clang_float2 unpack_snorm_2x16_clang(uint32_t) asm("floor.unpack_snorm_2x16");
const_func clang_float2 unpack_unorm_2x16_clang(uint32_t) asm("floor.unpack_unorm_2x16");
const_func clang_float2 unpack_half_2x16_clang(uint32_t) asm("floor.unpack_half_2x16");
#if !defined(FLOOR_COMPUTE_NO_DOUBLE)
const_func double pack_double_2x32_clang(clang_uint2) asm("floor.pack_double_2x32");
const_func clang_uint2 unpack_double_2x32_clang(double) asm("floor.unpack_double_2x32");
#endif

//! clamps the input vector to [-1, 1], then converts and scales each component to an 8-bit signed integer in [-127, 127],
//! returning a packed 32-bit unsigned integer, with vector components packed in ascending order from LSB to MSB
//! -> [comp-3][comp-2][comp-1][comp-0]
const_func uint32_t pack_snorm_4x8(const float4& vec) {
	return pack_snorm_4x8_clang(vec.to_clang_vector());
}

//! clamps the input vector to [0, 1], then converts and scales each component to an 8-bit unsigned integer in [0, 255],
//! returning a packed 32-bit unsigned integer, with vector components packed in ascending order from LSB to MSB
//! -> [comp-3][comp-2][comp-1][comp-0]
const_func uint32_t pack_unorm_4x8(const float4& vec) {
	return pack_unorm_4x8_clang(vec.to_clang_vector());
}

//! clamps the input vector to [-1, 1], then converts and scales each component to an 16-bit signed integer in [-32767, 32767],
//! returning a packed 32-bit unsigned integer, with vector components packed in ascending order from LSB to MSB
//! -> [comp-1][comp-0]
const_func uint32_t pack_snorm_2x16(const float2& vec) {
	return pack_snorm_2x16_clang(vec.to_clang_vector());
}

//! clamps the input vector to [0, 1], then converts and scales each component to an 16-bit unsigned integer in [0, 65535],
//! returning a packed 32-bit unsigned integer, with vector components packed in ascending order from LSB to MSB
//! -> [comp-1][comp-0]
const_func uint32_t pack_unorm_2x16(const float2& vec) {
	return pack_unorm_2x16_clang(vec.to_clang_vector());
}

//! converts the input 32-bit single-precision float vector to a 16-bit half-precision float vector,
//! returning a packed 32-bit unsigned integer, with vector components packed in ascending order from LSB to MSB
//! -> [comp-1][comp-0]
const_func uint32_t pack_half_2x16(const float2& vec) {
	return pack_half_2x16_clang(vec.to_clang_vector());
}

//! unpacks the input 32-bit unsigned integer into 4 8-bit signed integers, then converts these [-127, 127]-ranged integers
//! to normalized 32-bit single-precision float values in [-1, 1], returning them in a 4 component vector
const_func float4 unpack_snorm_4x8(const uint32_t& val) {
	return float4::from_clang_vector(unpack_snorm_4x8_clang(val));
}

//! unpacks the input 32-bit unsigned integer into 4 8-bit unsigned integers, then converts these [0, 255]-ranged integers
//! to normalized 32-bit single-precision float values in [0, 1], returning them in a 4 component vector
const_func float4 unpack_unorm_4x8(const uint32_t& val) {
	return float4::from_clang_vector(unpack_unorm_4x8_clang(val));
}

//! unpacks the input 32-bit unsigned integer into 2 16-bit signed integers, then converts these [-32767, 32767]-ranged integers
//! to normalized 32-bit single-precision float values in [-1, 1], returning them in a 2 component vector
const_func float2 unpack_snorm_2x16(const uint32_t& val) {
	return float2::from_clang_vector(unpack_snorm_2x16_clang(val));
}

//! unpacks the input 32-bit unsigned integer into 2 16-bit unsigned integers, then converts these [0, 65535]-ranged integers
//! to normalized 32-bit single-precision float values in [0, 1], returning them in a 2 component vector
const_func float2 unpack_unorm_2x16(const uint32_t& val) {
	return float2::from_clang_vector(unpack_unorm_2x16_clang(val));
}

//! unpacks the input 32-bit unsigned integer into 2 16-bit half-precision float values, then converts these values
//! to 32-bit single-precision float values, returning them in a 2 component vector
const_func float2 unpack_half_2x16(const uint32_t& val) {
	return float2::from_clang_vector(unpack_half_2x16_clang(val));
}

#if !defined(FLOOR_COMPUTE_NO_DOUBLE)
//! reinterprets the input 32-bit unsigned integer vector as a 64-bit double-precision float value,
//! with the first vector component representing the bottom/LSB part and the second component the top/MSB part
const_func double pack_double_2x32(const uint2& vec) {
	return pack_double_2x32_clang(vec.to_clang_vector());
}
//! unpacks the input 64-bit double-precision float value into 2 32-bit unsigned integers, returning them in a 2 component vector,
//! with the first vector component representing the bottom/LSB part and the second component the top/MSB part
const_func uint2 unpack_double_2x32(const double& val) {
	return uint2::from_clang_vector(unpack_double_2x32_clang(val));
}
#endif

//////////////////////////////////////////
// any shader
//! returns the view index inside a shader
const_func uint32_t get_view_index() asm("floor.builtin.view_index.i32");

//////////////////////////////////////////
// vertex shader
//! returns the vertex id inside a vertex shader
const_func uint32_t get_vertex_id() asm("floor.builtin.vertex_id.i32");
//! returns the instance id inside a vertex shader
const_func uint32_t get_instance_id() asm("floor.builtin.instance_id.i32");

//////////////////////////////////////////
// fragment shader
//! returns the normalized (in [0, 1]) point coordinate (clang_float2 version)
const_func clang_float2 get_point_coord_clang() asm("floor.builtin.point_coord.float2");
//! returns the normalized (in [0, 1]) point coordinate
floor_inline_always const_func float2 get_point_coord() { return float2::from_clang_vector(get_point_coord_clang()); }

//! NOTE/TODO: temporary
const_func clang_float4 get_frag_coord_clang() asm("floor.builtin.frag_coord.float4");
//! NOTE/TODO: temporary
floor_inline_always const_func float4 get_frag_coord() { return float4::from_clang_vector(get_frag_coord_clang()); }
//! NOTE/TODO: temporary
#define frag_coord get_frag_coord()

//! discards the current fragment
void discard_fragment() __attribute__((noreturn)) asm("floor.discard_fragment");

//! partial derivative of p with respect to the screen-space x coordinate
const_func float dfdx(float p) asm("floor.dfdx.f32");
//! partial derivative of p with respect to the screen-space y coordinate
const_func float dfdy(float p) asm("floor.dfdy.f32");
//! returns "abs(dfdx(p)) + abs(dfdy(p))"
const_func float fwidth(float p) asm("floor.fwidth.f32");
//! computes the partial deriviate of p with respect to the screen-space (x, y) coordinate
floor_inline_always const_func pair<float, float> dfdx_dfdy_gradient(const float& p) {
	return { { dfdx(p) }, { dfdy(p) } };
}
//! computes the partial deriviate of p with respect to the screen-space (x, y) coordinate
floor_inline_always const_func pair<float2, float2> dfdx_dfdy_gradient(const float2& p) {
	return { { dfdx(p.x), dfdx(p.y) }, { dfdy(p.x), dfdy(p.y) } };
}
//! computes the partial deriviate of p with respect to the screen-space (x, y) coordinate
floor_inline_always const_func pair<float3, float3> dfdx_dfdy_gradient(const float3& p) {
	return { { dfdx(p.x), dfdx(p.y), dfdx(p.z) }, { dfdy(p.x), dfdy(p.y), dfdy(p.z) } };
}

#endif

#endif
