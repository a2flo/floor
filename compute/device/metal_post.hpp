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

#ifndef __FLOOR_COMPUTE_DEVICE_METAL_POST_HPP__
#define __FLOOR_COMPUTE_DEVICE_METAL_POST_HPP__

#if defined(FLOOR_COMPUTE_METAL)

// NOTE: very wip

//////////////////////////////////////////
// general

const_func uint32_t pack_snorm_4x8_clang(clang_float4) asm("air.pack.snorm4x8.v4f32");
const_func uint32_t pack_unorm_4x8_clang(clang_float4) asm("air.pack.unorm4x8.v4f32");
const_func uint32_t pack_snorm_2x16_clang(clang_float2) asm("air.pack.snorm2x16.v2f32");
const_func uint32_t pack_unorm_2x16_clang(clang_float2) asm("air.pack.unorm2x16.v2f32");
const_func uint32_t pack_half_2x16_clang(clang_half2) asm("air.pack.snorm2x16.v2f16");
const_func clang_float4 unpack_snorm_4x8_clang(uint32_t) asm("air.unpack.snorm4x8.v4f32");
const_func clang_float4 unpack_unorm_4x8_clang(uint32_t) asm("air.unpack.unorm4x8.v4f32");
const_func clang_float2 unpack_snorm_2x16_clang(uint32_t) asm("air.unpack.snorm2x16.v2f32");
const_func clang_float2 unpack_unorm_2x16_clang(uint32_t) asm("air.unpack.unorm2x16.v2f32");
const_func clang_half2 unpack_half_2x16_clang(uint32_t) asm("air.unpack.snorm2x16.v2f16");
// unavailable: pack_double_2x32, unpack_double_2x32

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
	return pack_half_2x16_clang(half2(vec.x, vec.y).to_clang_vector());
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
	return (float2)half2::from_clang_vector(unpack_half_2x16_clang(val));
}

//////////////////////////////////////////
// any shader
//! returns the view index inside a shader
//! TODO: implement this
const_func uint32_t get_view_index() {
	//asm("floor.get_view_index.i32");
	return 0;
}

//////////////////////////////////////////
// vertex shader
//! returns the vertex id inside a vertex shader
const_func uint32_t get_vertex_id() asm("floor.get_vertex_id.i32");
//! returns the instance id inside a vertex shader
const_func uint32_t get_instance_id() asm("floor.get_instance_id.i32");

//////////////////////////////////////////
// fragment shader
//! returns the normalized (in [0, 1]) point coordinate (clang_float2 version)
const_func clang_float2 get_point_coord_cf2() asm("floor.get_point_coord.float2");
//! returns the normalized (in [0, 1]) point coordinate
floor_inline_always const_func float2 get_point_coord() { return float2::from_clang_vector(get_point_coord_cf2()); }

#if defined(FLOOR_COMPUTE_INFO_HAS_PRIMITIVE_ID_1)
//! returns the primitive id inside a fragment shader
const_func uint32_t get_primitive_id() asm("floor.get_primitive_id.i32");
#else
const_func uint32_t get_primitive_id() __attribute__((unavailable("not supported on this target")));
#endif

#if defined(FLOOR_COMPUTE_INFO_HAS_BARYCENTRIC_COORD_1)
const_func clang_float3 get_barycentric_coord_cf3() asm("floor.get_barycentric_coord.float3");
//! returns the barycentric coordinate inside a fragment shader
floor_inline_always const_func float3 get_barycentric_coord() { return float3::from_clang_vector(get_barycentric_coord_cf3()); }
#else
const_func clang_float3 get_barycentric_coord_cf3() __attribute__((unavailable("not supported on this target")));
floor_inline_always const_func float3 get_barycentric_coord() __attribute__((unavailable("not supported on this target")));
#endif

//! discards the current fragment
void discard_fragment() __attribute__((noreturn)) asm("air.discard_fragment");

//! partial derivative of p with respect to the screen-space x coordinate
const_func float dfdx(float p) asm("air.dfdx.f32");
//! partial derivative of p with respect to the screen-space y coordinate
const_func float dfdy(float p) asm("air.dfdy.f32");
//! returns "abs(dfdx(p)) + abs(dfdy(p))"
const_func float fwidth(float p) asm("air.fwidth.f32");
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
