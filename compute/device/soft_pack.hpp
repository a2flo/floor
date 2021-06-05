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

#ifndef __FLOOR_COMPUTE_DEVICE_SOFT_PACK_HPP__
#define __FLOOR_COMPUTE_DEVICE_SOFT_PACK_HPP__

#if defined(FLOOR_COMPUTE_CUDA) || defined(FLOOR_COMPUTE_HOST) || defined(FLOOR_COMPUTE_OPENCL)

//! clamps the input vector to [-1, 1], then converts and scales each component to an 8-bit signed integer in [-127, 127],
//! returning a packed 32-bit unsigned integer, with vector components packed in ascending order from LSB to MSB
//! -> [comp-3][comp-2][comp-1][comp-0]
floor_inline_always __attribute__((const)) uint32_t pack_snorm_4x8(const float4& vec) {
	const auto uivec = ((vec.clamped(-1.0f, 1.0f) * 127.0f).cast<int32_t>() & 0xFF).reinterpret<uint32_t>();
	return uivec.x | (uivec.y << 8u) | (uivec.z << 16u) | (uivec.w << 24u);
}

//! clamps the input vector to [0, 1], then converts and scales each component to an 8-bit unsigned integer in [0, 255],
//! returning a packed 32-bit unsigned integer, with vector components packed in ascending order from LSB to MSB
//! -> [comp-3][comp-2][comp-1][comp-0]
floor_inline_always __attribute__((const)) uint32_t pack_unorm_4x8(const float4& vec) {
	const auto uivec = (vec.clamped(0.0f, 1.0f) * 255.0f).cast<uint32_t>();
	return uivec.x | (uivec.y << 8u) | (uivec.z << 16u) | (uivec.w << 24u);
}

//! clamps the input vector to [-1, 1], then converts and scales each component to an 16-bit signed integer in [-32767, 32767],
//! returning a packed 32-bit unsigned integer, with vector components packed in ascending order from LSB to MSB
//! -> [comp-1][comp-0]
floor_inline_always __attribute__((const)) uint32_t pack_snorm_2x16(const float2& vec) {
	const auto uivec = ((vec.clamped(-1.0f, 1.0f) * 32767.0f).cast<int32_t>() & 0xFFFF).reinterpret<uint32_t>();
	return uivec.x | (uivec.y << 16u);
}

//! clamps the input vector to [0, 1], then converts and scales each component to an 16-bit unsigned integer in [0, 65535],
//! returning a packed 32-bit unsigned integer, with vector components packed in ascending order from LSB to MSB
//! -> [comp-1][comp-0]
floor_inline_always __attribute__((const)) uint32_t pack_unorm_2x16(const float2& vec) {
	const auto uivec = (vec.clamped(0.0f, 1.0f) * 65535.0f).cast<uint32_t>();
	return uivec.x | (uivec.y << 16u);
}

//! converts the input 32-bit single-precision float vector to a 16-bit half-precision float vector,
//! returning a packed 32-bit unsigned integer, with vector components packed in ascending order from LSB to MSB
//! -> [comp-1][comp-0]
floor_inline_always __attribute__((const)) uint32_t pack_half_2x16(const float2& vec) {
	const auto uivec = vec.cast<half>().reinterpret<uint16_t>();
	return uint32_t(uivec.x) | uint32_t(uivec.y << 16u);
}

//! reinterprets the input 32-bit unsigned integer vector as a 64-bit double-precision float value,
//! with the first vector component representing the bottom/LSB part and the second component the top/MSB part
floor_inline_always __attribute__((const)) double pack_double_2x32(const uint2& vec) {
	struct {
		const uint64_t ui64_val;
		const double f64_val;
	} conversion { .ui64_val = (uint64_t(vec.y) << uint64_t(32)) | vec.x };
	return conversion.f64_val;
}

//! unpacks the input 32-bit unsigned integer into 4 8-bit signed integers, then converts these [-127, 127]-ranged integers
//! to normalized 32-bit single-precision float values in [-1, 1], returning them in a 4 component vector
floor_inline_always __attribute__((const)) float4 unpack_snorm_4x8(const uint32_t& val) {
	return uchar4(val & 0xFFu,
				  (val >> 8u) & 0xFFu,
				  (val >> 16u) & 0xFFu,
				  (val >> 24u) & 0xFFu).reinterpret<int8_t>().cast<float>() * (1.0f / 127.0f);
}

//! unpacks the input 32-bit unsigned integer into 4 8-bit unsigned integers, then converts these [0, 255]-ranged integers
//! to normalized 32-bit single-precision float values in [0, 1], returning them in a 4 component vector
floor_inline_always __attribute__((const)) float4 unpack_unorm_4x8(const uint32_t& val) {
	return uchar4(val & 0xFFu,
				  (val >> 8u) & 0xFFu,
				  (val >> 16u) & 0xFFu,
				  (val >> 24u) & 0xFFu).cast<float>() * (1.0f / 255.0f);
}

//! unpacks the input 32-bit unsigned integer into 2 16-bit signed integers, then converts these [-32767, 32767]-ranged integers
//! to normalized 32-bit single-precision float values in [-1, 1], returning them in a 2 component vector
floor_inline_always __attribute__((const)) float2 unpack_snorm_2x16(const uint32_t& val) {
	return ushort2(val & 0xFFFFu, (val >> 16u) & 0xFFFFu).reinterpret<int16_t>().cast<float>() * (1.0f / 32767.0f);
}

//! unpacks the input 32-bit unsigned integer into 2 16-bit unsigned integers, then converts these [0, 65535]-ranged integers
//! to normalized 32-bit single-precision float values in [0, 1], returning them in a 2 component vector
floor_inline_always __attribute__((const)) float2 unpack_unorm_2x16(const uint32_t& val) {
	return ushort2(val & 0xFFFFu, (val >> 16u) & 0xFFFFu).cast<float>() * (1.0f / 65535.0f);
}

//! unpacks the input 32-bit unsigned integer into 2 16-bit half-precision float values, then converts these values
//! to 32-bit single-precision float values, returning them in a 2 component vector
floor_inline_always __attribute__((const)) float2 unpack_half_2x16(const uint32_t& val) {
	return ushort2(val & 0xFFFFu, (val >> 16u) & 0xFFFFu).reinterpret<half>().cast<float>();
}

//! unpacks the input 64-bit double-precision float value into 2 32-bit unsigned integers, returning them in a 2 component vector,
//! with the first vector component representing the bottom/LSB part and the second component the top/MSB part
floor_inline_always __attribute__((const)) uint2 unpack_double_2x32(const double& val) {
	struct {
		const uint64_t ui64_val;
		const double f64_val;
	} conversion { .f64_val = val };
	return uint2 { uint32_t(conversion.ui64_val) & 0xFFFFFFFFu, uint32_t(conversion.ui64_val >> uint64_t(32)) & 0xFFFFFFFFu };
}

#endif

#endif
