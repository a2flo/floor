/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2024 Florian Ziesche
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

enum class VERTEX_FORMAT : uint32_t {
	//! invalid/uninitialized
	NONE					= (0u),
	
	//! bits 24-31: type flags
	__FLAG_MASK				= (0xFF00'0000u),
	__FLAG_SHIFT			= (24u),
	//! base type: signals that (un)signed integer formats are read as normalized float,
	//!            unsigned: [0, 1], signed [-1, 1]
	FLAG_NORMALIZED			= (1u << (__FLAG_SHIFT + 0u)),
	//! optional type: flips the layout from RGBA to BGRA
	FLAG_BGRA				= (1u << (__FLAG_SHIFT + 1u)),
	__UNUSED_FLAG_2			= (1u << (__FLAG_SHIFT + 2u)),
	__UNUSED_FLAG_3			= (1u << (__FLAG_SHIFT + 3u)),
	__UNUSED_FLAG_4			= (1u << (__FLAG_SHIFT + 4u)),
	__UNUSED_FLAG_5			= (1u << (__FLAG_SHIFT + 5u)),
	__UNUSED_FLAG_6			= (1u << (__FLAG_SHIFT + 6u)),
	__UNUSED_FLAG_7			= (1u << (__FLAG_SHIFT + 7u)),
	
	//! bits 22-23: storage data type
	__DATA_TYPE_MASK		= (0x00C0'0000u),
	__DATA_TYPE_SHIFT		= (22u),
	INT						= (1u << __DATA_TYPE_SHIFT),
	UINT					= (2u << __DATA_TYPE_SHIFT),
	FLOAT					= (3u << __DATA_TYPE_SHIFT),
	
	//! bits 20-21: dimensionality
	__DIM_MASK				= (0x0030'0000u),
	__DIM_SHIFT				= (20u),
	DIM_1D					= (0u << __DIM_SHIFT),
	DIM_2D					= (1u << __DIM_SHIFT),
	DIM_3D					= (2u << __DIM_SHIFT),
	DIM_4D					= (3u << __DIM_SHIFT),
	
	//! bits 6-19: unused
	
	//! bits 0-5: base formats
	__FORMAT_MASK			= (0x0000'003Fu),
	__FORMAT_SHIFT			= (0u),
	//! 8 bits per component
	FORMAT_8				= (1u),
	//! 16 bits per component
	FORMAT_16				= (2u),
	//! 32 bits per component
	FORMAT_32				= (3u),
	//! 64 bits per component
	FORMAT_64				= (4u),
	//! 4 component format: 10-bit/10-bit/10-bit XYZ, 2-bit W
	//! WWZZ ZZZZ ZZZZ YYYY YYYY YYXX XXXX XXXX
	FORMAT_10_10_10_ALPHA_2	= (5u),
	
	//////////////////////////////////////////
	// -> convenience aliases
	
	HALF1					= FLOAT | DIM_1D | FORMAT_16,
	HALF2					= FLOAT | DIM_2D | FORMAT_16,
	HALF3					= FLOAT | DIM_3D | FORMAT_16,
	HALF4					= FLOAT | DIM_4D | FORMAT_16,
	FLOAT1					= FLOAT | DIM_1D | FORMAT_32,
	FLOAT2					= FLOAT | DIM_2D | FORMAT_32,
	FLOAT3					= FLOAT | DIM_3D | FORMAT_32,
	FLOAT4					= FLOAT | DIM_4D | FORMAT_32,
	
	UCHAR1					= UINT | DIM_1D | FORMAT_8,
	UCHAR2					= UINT | DIM_2D | FORMAT_8,
	UCHAR3					= UINT | DIM_3D | FORMAT_8,
	UCHAR4					= UINT | DIM_4D | FORMAT_8,
	USHORT1					= UINT | DIM_1D | FORMAT_16,
	USHORT2					= UINT | DIM_2D | FORMAT_16,
	USHORT3					= UINT | DIM_3D | FORMAT_16,
	USHORT4					= UINT | DIM_4D | FORMAT_16,
	UINT1					= UINT | DIM_1D | FORMAT_32,
	UINT2					= UINT | DIM_2D | FORMAT_32,
	UINT3					= UINT | DIM_3D | FORMAT_32,
	UINT4					= UINT | DIM_4D | FORMAT_32,
	
	CHAR1					= INT | DIM_1D | FORMAT_8,
	CHAR2					= INT | DIM_2D | FORMAT_8,
	CHAR3					= INT | DIM_3D | FORMAT_8,
	CHAR4					= INT | DIM_4D | FORMAT_8,
	SHORT1					= INT | DIM_1D | FORMAT_16,
	SHORT2					= INT | DIM_2D | FORMAT_16,
	SHORT3					= INT | DIM_3D | FORMAT_16,
	SHORT4					= INT | DIM_4D | FORMAT_16,
	INT1					= INT | DIM_1D | FORMAT_32,
	INT2					= INT | DIM_2D | FORMAT_32,
	INT3					= INT | DIM_3D | FORMAT_32,
	INT4					= INT | DIM_4D | FORMAT_32,
	
	UCHAR1_NORM				= UINT | DIM_1D | FORMAT_8 | FLAG_NORMALIZED,
	UCHAR2_NORM				= UINT | DIM_2D | FORMAT_8 | FLAG_NORMALIZED,
	UCHAR3_NORM				= UINT | DIM_3D | FORMAT_8 | FLAG_NORMALIZED,
	UCHAR4_NORM				= UINT | DIM_4D | FORMAT_8 | FLAG_NORMALIZED,
	USHORT1_NORM			= UINT | DIM_1D | FORMAT_16 | FLAG_NORMALIZED,
	USHORT2_NORM			= UINT | DIM_2D | FORMAT_16 | FLAG_NORMALIZED,
	USHORT3_NORM			= UINT | DIM_3D | FORMAT_16 | FLAG_NORMALIZED,
	USHORT4_NORM			= UINT | DIM_4D | FORMAT_16 | FLAG_NORMALIZED,
	USHORT4_NORM_BGRA			= UINT | DIM_4D | FORMAT_16 | FLAG_NORMALIZED | FLAG_BGRA,
	
	CHAR1_NORM				= INT | DIM_1D | FORMAT_8 | FLAG_NORMALIZED,
	CHAR2_NORM				= INT | DIM_2D | FORMAT_8 | FLAG_NORMALIZED,
	CHAR3_NORM				= INT | DIM_3D | FORMAT_8 | FLAG_NORMALIZED,
	CHAR4_NORM				= INT | DIM_4D | FORMAT_8 | FLAG_NORMALIZED,
	SHORT1_NORM				= INT | DIM_1D | FORMAT_16 | FLAG_NORMALIZED,
	SHORT2_NORM				= INT | DIM_2D | FORMAT_16 | FLAG_NORMALIZED,
	SHORT3_NORM				= INT | DIM_3D | FORMAT_16 | FLAG_NORMALIZED,
	SHORT4_NORM				= INT | DIM_4D | FORMAT_16 | FLAG_NORMALIZED,
	
	U1010102_NORM			= UINT | DIM_4D | FORMAT_10_10_10_ALPHA_2 | FLAG_NORMALIZED,
	I1010102_NORM			= INT | DIM_4D | FORMAT_10_10_10_ALPHA_2 | FLAG_NORMALIZED,

};
floor_global_enum_ext(VERTEX_FORMAT)

//! returns the dimensionality of the specified vertex format
floor_inline_always static constexpr uint32_t vertex_dim_count(const VERTEX_FORMAT& vertex_format) {
	return (uint32_t(vertex_format & VERTEX_FORMAT::__DIM_MASK) >> uint32_t(VERTEX_FORMAT::__DIM_SHIFT)) + 1u;
}

//! return the base format of the specified vertex format
floor_inline_always static constexpr uint32_t vertex_base_format(const VERTEX_FORMAT& vertex_format) {
	return uint32_t(vertex_format & VERTEX_FORMAT::__FORMAT_MASK);
}

//! returns the amount of bits needed to store one vertex
static constexpr uint32_t vertex_bits(const VERTEX_FORMAT& vertex_format) {
	const auto format = vertex_format & VERTEX_FORMAT::__FORMAT_MASK;
	const auto dim_count = vertex_dim_count(vertex_format);
	switch (format) {
		// arbitrary channel formats
		case VERTEX_FORMAT::FORMAT_8: return 8u * dim_count;
		case VERTEX_FORMAT::FORMAT_16: return 16u * dim_count;
		case VERTEX_FORMAT::FORMAT_32: return 32u * dim_count;
		case VERTEX_FORMAT::FORMAT_64: return 64u * dim_count;

		// special channel specific formats
		case VERTEX_FORMAT::FORMAT_10_10_10_ALPHA_2: return 32u;
		default: return dim_count;
	}
}

//! returns the amount of bytes needed to store one vertex
//! NOTE: rounded up if "bits per vertex" is not divisible by 8
static constexpr uint32_t vertex_bytes(const VERTEX_FORMAT& vertex_format) {
	const auto bits = vertex_bits(vertex_format);
	return ((bits + 7u) / 8u);
}

//! vertex data size -> data type mapping
//! NOTE: if size is 0, this will default to a 32-bit type
template <VERTEX_FORMAT vertex_format, size_t size> struct vertex_sized_data_type {};
template <VERTEX_FORMAT vertex_format, size_t size>
requires((vertex_format & VERTEX_FORMAT::__DATA_TYPE_MASK) == VERTEX_FORMAT::UINT && size <= 64u)
struct vertex_sized_data_type<vertex_format, size> {
	typedef conditional_t<(size == 0u), uint32_t, conditional_t<
						  (size > 0u && size <= 8u), uint8_t, conditional_t<
						  (size > 8u && size <= 16u), uint16_t, conditional_t<
						  (size > 16u && size <= 32u), uint32_t, conditional_t<
						  (size > 32u && size <= 64u), uint64_t, void>>>>> type;
};
template <VERTEX_FORMAT vertex_format, size_t size>
requires((vertex_format & VERTEX_FORMAT::__DATA_TYPE_MASK) == VERTEX_FORMAT::INT && size <= 64u)
struct vertex_sized_data_type<vertex_format, size> {
	typedef conditional_t<(size == 0u), int32_t, conditional_t<
						  (size > 0u && size <= 8u), int8_t, conditional_t<
						  (size > 8u && size <= 16u), int16_t, conditional_t<
						  (size > 16u && size <= 32u), int32_t, conditional_t<
						  (size > 32u && size <= 64u), int64_t, void>>>>> type;
};
template <VERTEX_FORMAT vertex_format, size_t size>
requires((vertex_format & VERTEX_FORMAT::__DATA_TYPE_MASK) == VERTEX_FORMAT::FLOAT && size <= 64u)
struct vertex_sized_data_type<vertex_format, size> {
	typedef conditional_t<(size == 0u), float, conditional_t<
						  (size > 0u && size <= 16u), float /* no half type, load/stores via float */, conditional_t<
						  (size > 16u && size <= 32u), float, conditional_t<
						  (size > 32u && size <= 64u), double, void>>>> type;
};
