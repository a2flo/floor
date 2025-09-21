/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2025 Florian Ziesche
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

#if defined(FLOOR_DEVICE_VULKAN)

namespace fl {

// all supported scalar data types in Vulkan SIMD/subgroup functions
#define FLOOR_VULKAN_SUB_GROUP_DATA_TYPES_SCALAR(F, P) \
F(P, int16_t, int16_t, "s16") \
F(P, uint16_t, uint16_t, "u16") \
F(P, half, half, "f16") \
F(P, int32_t, int32_t, "s32") \
F(P, uint32_t, uint32_t, "u32") \
F(P, float, float, "f32")

// all supported vector data types in Vulkan SIMD/subgroup functions
#define FLOOR_VULKAN_SUB_GROUP_DATA_TYPES_VECTOR(F, P) \
F(P, short2, clang_short2, "v2.s16") \
F(P, ushort2, clang_ushort2, "v2.u16") \
F(P, half2, clang_half2, "v2.f16") \
F(P, int2, clang_int2, "v2.s32") \
F(P, uint2, clang_uint2, "v2.u32") \
F(P, float2, clang_float2, "v2.f32") \
F(P, short3, clang_short3, "v3.s16") \
F(P, ushort3, clang_ushort3, "v3.u16") \
F(P, half3, clang_half3, "v3.f16") \
F(P, int3, clang_int3, "v3.s32") \
F(P, uint3, clang_uint3, "v3.u32") \
F(P, float3, clang_float3, "v3.f32") \
F(P, short4, clang_short4, "v4.s16") \
F(P, ushort4, clang_ushort4, "v4.u16") \
F(P, half4, clang_half4, "v4.f16") \
F(P, int4, clang_int4, "v4.s32") \
F(P, uint4, clang_uint4, "v4.u32") \
F(P, float4, clang_float4, "v4.f32")

// all supported data types in Vulkan SIMD/subgroup functions
#define FLOOR_VULKAN_SUB_GROUP_DATA_TYPES(F, P) \
FLOOR_VULKAN_SUB_GROUP_DATA_TYPES_SCALAR(F, P) \
FLOOR_VULKAN_SUB_GROUP_DATA_TYPES_VECTOR(F, P)

// scalar subgroup functions can simply be defined and used
#define SUB_GROUP_SCALAR_FUNC(func, floor_data_type, clang_data_type, type_suffix) \
clang_data_type func(clang_data_type, uint32_t lane_idx_delta_or_mask) \
__attribute__((noduplicate, convergent)) asm("floor.sub_group." #func "." type_suffix);
FLOOR_VULKAN_SUB_GROUP_DATA_TYPES_SCALAR(SUB_GROUP_SCALAR_FUNC, simd_shuffle)
FLOOR_VULKAN_SUB_GROUP_DATA_TYPES_SCALAR(SUB_GROUP_SCALAR_FUNC, simd_shuffle_down)
FLOOR_VULKAN_SUB_GROUP_DATA_TYPES_SCALAR(SUB_GROUP_SCALAR_FUNC, simd_shuffle_up)
FLOOR_VULKAN_SUB_GROUP_DATA_TYPES_SCALAR(SUB_GROUP_SCALAR_FUNC, simd_shuffle_xor)
#undef SUB_GROUP_SCALAR_FUNC

// vector subgroup functions must be defined for the native clang vector type first, ...
#define SUB_GROUP_CLANG_FUNC(func, floor_data_type, clang_data_type, type_suffix) \
clang_data_type func##_clang(clang_data_type, uint32_t lane_idx_delta_or_mask) \
__attribute__((noduplicate, convergent)) asm("floor.sub_group." #func "." type_suffix);
FLOOR_VULKAN_SUB_GROUP_DATA_TYPES_VECTOR(SUB_GROUP_CLANG_FUNC, simd_shuffle)
FLOOR_VULKAN_SUB_GROUP_DATA_TYPES_VECTOR(SUB_GROUP_CLANG_FUNC, simd_shuffle_down)
FLOOR_VULKAN_SUB_GROUP_DATA_TYPES_VECTOR(SUB_GROUP_CLANG_FUNC, simd_shuffle_up)
FLOOR_VULKAN_SUB_GROUP_DATA_TYPES_VECTOR(SUB_GROUP_CLANG_FUNC, simd_shuffle_xor)
#undef SUB_GROUP_CLANG_FUNC

// ... then we can define vector subgroup functions using our own vector type
#define SUB_GROUP_VECTOR_FUNC(func, floor_data_type, clang_data_type, type_suffix) \
floor_inline_always static floor_data_type func(floor_data_type value, uint32_t lane_idx_delta_or_mask) \
__attribute__((noduplicate, convergent)) { \
	return floor_data_type::from_clang_vector(func##_clang(value.to_clang_vector(), lane_idx_delta_or_mask)); \
}
FLOOR_VULKAN_SUB_GROUP_DATA_TYPES_VECTOR(SUB_GROUP_VECTOR_FUNC, simd_shuffle)
FLOOR_VULKAN_SUB_GROUP_DATA_TYPES_VECTOR(SUB_GROUP_VECTOR_FUNC, simd_shuffle_down)
FLOOR_VULKAN_SUB_GROUP_DATA_TYPES_VECTOR(SUB_GROUP_VECTOR_FUNC, simd_shuffle_up)
FLOOR_VULKAN_SUB_GROUP_DATA_TYPES_VECTOR(SUB_GROUP_VECTOR_FUNC, simd_shuffle_xor)
#undef SUB_GROUP_VECTOR_FUNC

// Vulkan parallel group operation implementations / support
namespace algorithm::group {

// compiler side functions
#define FLOOR_VULKAN_SUBGROUP_OPS(func, floor_data_type, clang_data_type, type_suffix) \
clang_data_type sub_group_reduce_add(clang_data_type value) __attribute__((noduplicate, convergent)) asm("floor.sub_group.reduce.add." type_suffix); \
clang_data_type sub_group_reduce_min(clang_data_type value) __attribute__((noduplicate, convergent)) asm("floor.sub_group.reduce.min." type_suffix); \
clang_data_type sub_group_reduce_max(clang_data_type value) __attribute__((noduplicate, convergent)) asm("floor.sub_group.reduce.max." type_suffix); \
clang_data_type sub_group_inclusive_scan_add(clang_data_type value) __attribute__((noduplicate, convergent)) asm("floor.sub_group.inclusive_scan.add." type_suffix); \
clang_data_type sub_group_inclusive_scan_min(clang_data_type value) __attribute__((noduplicate, convergent)) asm("floor.sub_group.inclusive_scan.min." type_suffix); \
clang_data_type sub_group_inclusive_scan_max(clang_data_type value) __attribute__((noduplicate, convergent)) asm("floor.sub_group.inclusive_scan.max." type_suffix); \
clang_data_type sub_group_exclusive_scan_add(clang_data_type value) __attribute__((noduplicate, convergent)) asm("floor.sub_group.exclusive_scan.add." type_suffix); \
clang_data_type sub_group_exclusive_scan_min(clang_data_type value) __attribute__((noduplicate, convergent)) asm("floor.sub_group.exclusive_scan.min." type_suffix); \
clang_data_type sub_group_exclusive_scan_max(clang_data_type value) __attribute__((noduplicate, convergent)) asm("floor.sub_group.exclusive_scan.max." type_suffix);

FLOOR_VULKAN_SUB_GROUP_DATA_TYPES(FLOOR_VULKAN_SUBGROUP_OPS, )
	
#undef FLOOR_VULKAN_SUBGROUP_OPS

// specialize for all supported operations
#define FLOOR_VULKAN_SUPPORT_SUBGROUP_OPS(func, floor_data_type, clang_data_type, type_suffix) \
template <> struct supports<ALGORITHM::SUB_GROUP_REDUCE, OP::ADD, floor_data_type> : public std::true_type {}; \
template <> struct supports<ALGORITHM::SUB_GROUP_REDUCE, OP::MIN, floor_data_type> : public std::true_type {}; \
template <> struct supports<ALGORITHM::SUB_GROUP_REDUCE, OP::MAX, floor_data_type> : public std::true_type {}; \
template <> struct supports<ALGORITHM::SUB_GROUP_INCLUSIVE_SCAN, OP::ADD, floor_data_type> : public std::true_type {}; \
template <> struct supports<ALGORITHM::SUB_GROUP_INCLUSIVE_SCAN, OP::MIN, floor_data_type> : public std::true_type {}; \
template <> struct supports<ALGORITHM::SUB_GROUP_INCLUSIVE_SCAN, OP::MAX, floor_data_type> : public std::true_type {}; \
template <> struct supports<ALGORITHM::SUB_GROUP_EXCLUSIVE_SCAN, OP::ADD, floor_data_type> : public std::true_type {}; \
template <> struct supports<ALGORITHM::SUB_GROUP_EXCLUSIVE_SCAN, OP::MIN, floor_data_type> : public std::true_type {}; \
template <> struct supports<ALGORITHM::SUB_GROUP_EXCLUSIVE_SCAN, OP::MAX, floor_data_type> : public std::true_type {};

FLOOR_VULKAN_SUB_GROUP_DATA_TYPES(FLOOR_VULKAN_SUPPORT_SUBGROUP_OPS, )

#undef FLOOR_VULKAN_SUPPORT_SUBGROUP_OPS

template <OP op, typename data_type>
requires (op == OP::ADD)
static auto sub_group_reduce(const data_type input_value) {
	if constexpr (is_floor_vector_v<data_type>) {
		return data_type::from_clang_vector(sub_group_reduce_add(input_value.to_clang_vector()));
	} else {
		return sub_group_reduce_add(input_value);
	}
}

template <OP op, typename data_type>
requires (op == OP::MIN)
static auto sub_group_reduce(const data_type input_value) {
	if constexpr (is_floor_vector_v<data_type>) {
		return data_type::from_clang_vector(sub_group_reduce_min(input_value.to_clang_vector()));
	} else {
		return sub_group_reduce_min(input_value);
	}
}

template <OP op, typename data_type>
requires (op == OP::MAX)
static auto sub_group_reduce(const data_type input_value) {
	if constexpr (is_floor_vector_v<data_type>) {
		return data_type::from_clang_vector(sub_group_reduce_max(input_value.to_clang_vector()));
	} else {
		return sub_group_reduce_max(input_value);
	}
}

template <OP op, typename data_type>
requires (op == OP::ADD)
static auto sub_group_inclusive_scan(const data_type input_value) {
	if constexpr (is_floor_vector_v<data_type>) {
		return data_type::from_clang_vector(sub_group_inclusive_scan_add(input_value.to_clang_vector()));
	} else {
		return sub_group_inclusive_scan_add(input_value);
	}
}

template <OP op, typename data_type>
requires (op == OP::MIN)
static auto sub_group_inclusive_scan(const data_type input_value) {
	if constexpr (is_floor_vector_v<data_type>) {
		return data_type::from_clang_vector(sub_group_inclusive_scan_min(input_value.to_clang_vector()));
	} else {
		return sub_group_inclusive_scan_min(input_value);
	}
}

template <OP op, typename data_type>
requires (op == OP::MAX)
static auto sub_group_inclusive_scan(const data_type input_value) {
	if constexpr (is_floor_vector_v<data_type>) {
		return data_type::from_clang_vector(sub_group_inclusive_scan_max(input_value.to_clang_vector()));
	} else {
		return sub_group_inclusive_scan_max(input_value);
	}
}

template <OP op, typename data_type>
requires (op == OP::ADD)
static auto sub_group_exclusive_scan(const data_type input_value) {
	if constexpr (is_floor_vector_v<data_type>) {
		return data_type::from_clang_vector(sub_group_exclusive_scan_add(input_value.to_clang_vector()));
	} else {
		return sub_group_exclusive_scan_add(input_value);
	}
}

template <OP op, typename data_type>
requires (op == OP::MIN)
static auto sub_group_exclusive_scan(const data_type input_value) {
	if constexpr (is_floor_vector_v<data_type>) {
		return data_type::from_clang_vector(sub_group_exclusive_scan_min(input_value.to_clang_vector()));
	} else {
		return sub_group_exclusive_scan_min(input_value);
	}
}

template <OP op, typename data_type>
requires (op == OP::MAX)
static auto sub_group_exclusive_scan(const data_type input_value) {
	if constexpr (is_floor_vector_v<data_type>) {
		return data_type::from_clang_vector(sub_group_exclusive_scan_max(input_value.to_clang_vector()));
	} else {
		return sub_group_exclusive_scan_max(input_value);
	}
}

} // namespace algorithm::group

#undef FLOOR_VULKAN_SUB_GROUP_DATA_TYPES_SCALAR
#undef FLOOR_VULKAN_SUB_GROUP_DATA_TYPES_VECTOR
#undef FLOOR_VULKAN_SUB_GROUP_DATA_TYPES

} // namespace fl

#endif
