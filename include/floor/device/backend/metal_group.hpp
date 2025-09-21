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

#if defined(FLOOR_DEVICE_METAL)

namespace fl {

// all supported scalar data types in Metal SIMD/subgroup functions
#define FLOOR_METAL_SUB_GROUP_DATA_TYPES_SCALAR(F, P) \
F(P, int16_t, int16_t, "s.i16") \
F(P, uint16_t, uint16_t, "u.i16") \
F(P, half, half, "f16") \
F(P, int32_t, int32_t, "s.i32") \
F(P, uint32_t, uint32_t, "u.i32") \
F(P, float, float, "f32")

// all supported vector data types in Metal SIMD/subgroup functions
#define FLOOR_METAL_SUB_GROUP_DATA_TYPES_VECTOR(F, P) \
F(P, short2, clang_short2, "s.v2i16") \
F(P, ushort2, clang_ushort2, "u.v2i16") \
F(P, half2, clang_half2, "v2f16") \
F(P, int2, clang_int2, "s.v2i32") \
F(P, uint2, clang_uint2, "u.v2i32") \
F(P, float2, clang_float2, "v2f32") \
F(P, short3, clang_short3, "s.v3i16") \
F(P, ushort3, clang_ushort3, "u.v3i16") \
F(P, half3, clang_half3, "v3f16") \
F(P, int3, clang_int3, "s.v3i32") \
F(P, uint3, clang_uint3, "u.v3i32") \
F(P, float3, clang_float3, "v3f32") \
F(P, short4, clang_short4, "s.v4i16") \
F(P, ushort4, clang_ushort4, "u.v4i16") \
F(P, half4, clang_half4, "v4f16") \
F(P, int4, clang_int4, "s.v4i32") \
F(P, uint4, clang_uint4, "u.v4i32") \
F(P, float4, clang_float4, "v4f32")

// all supported data types in Metal SIMD/subgroup functions
#define FLOOR_METAL_SUB_GROUP_DATA_TYPES(F, P) \
FLOOR_METAL_SUB_GROUP_DATA_TYPES_SCALAR(F, P) \
FLOOR_METAL_SUB_GROUP_DATA_TYPES_VECTOR(F, P)

// scalar subgroup functions can simply be defined and used
#define SUB_GROUP_SCALAR_FUNC(func, floor_data_type, clang_data_type, air_type_suffix) \
clang_data_type func(clang_data_type, uint16_t lane_idx_delta_or_mask) \
__attribute__((noduplicate, convergent)) asm("air." #func "." air_type_suffix);
FLOOR_METAL_SUB_GROUP_DATA_TYPES_SCALAR(SUB_GROUP_SCALAR_FUNC, simd_shuffle)
FLOOR_METAL_SUB_GROUP_DATA_TYPES_SCALAR(SUB_GROUP_SCALAR_FUNC, simd_shuffle_down)
FLOOR_METAL_SUB_GROUP_DATA_TYPES_SCALAR(SUB_GROUP_SCALAR_FUNC, simd_shuffle_up)
FLOOR_METAL_SUB_GROUP_DATA_TYPES_SCALAR(SUB_GROUP_SCALAR_FUNC, simd_shuffle_xor)
#undef SUB_GROUP_SCALAR_FUNC

// vector subgroup functions must be defined for the native clang vector type first, ...
#define SUB_GROUP_CLANG_FUNC(func, floor_data_type, clang_data_type, air_type_suffix) \
clang_data_type func##_clang(clang_data_type, uint16_t lane_idx_delta_or_mask) \
__attribute__((noduplicate, convergent)) asm("air." #func "." air_type_suffix);
FLOOR_METAL_SUB_GROUP_DATA_TYPES_VECTOR(SUB_GROUP_CLANG_FUNC, simd_shuffle)
FLOOR_METAL_SUB_GROUP_DATA_TYPES_VECTOR(SUB_GROUP_CLANG_FUNC, simd_shuffle_down)
FLOOR_METAL_SUB_GROUP_DATA_TYPES_VECTOR(SUB_GROUP_CLANG_FUNC, simd_shuffle_up)
FLOOR_METAL_SUB_GROUP_DATA_TYPES_VECTOR(SUB_GROUP_CLANG_FUNC, simd_shuffle_xor)
#undef SUB_GROUP_CLANG_FUNC

// ... then we can define vector subgroup functions using our own vector type
#define SUB_GROUP_VECTOR_FUNC(func, floor_data_type, clang_data_type, air_type_suffix) \
floor_inline_always static floor_data_type func(floor_data_type value, uint16_t lane_idx_delta_or_mask) \
__attribute__((noduplicate, convergent)) { \
	return floor_data_type::from_clang_vector(func##_clang(value.to_clang_vector(), lane_idx_delta_or_mask)); \
}
FLOOR_METAL_SUB_GROUP_DATA_TYPES_VECTOR(SUB_GROUP_VECTOR_FUNC, simd_shuffle)
FLOOR_METAL_SUB_GROUP_DATA_TYPES_VECTOR(SUB_GROUP_VECTOR_FUNC, simd_shuffle_down)
FLOOR_METAL_SUB_GROUP_DATA_TYPES_VECTOR(SUB_GROUP_VECTOR_FUNC, simd_shuffle_up)
FLOOR_METAL_SUB_GROUP_DATA_TYPES_VECTOR(SUB_GROUP_VECTOR_FUNC, simd_shuffle_xor)
#undef SUB_GROUP_VECTOR_FUNC

// Metal parallel group operation implementations / support
namespace algorithm::group {

// AIR backend functions
#define FLOOR_METAL_AIR_SUBGROUP_OPS(func, floor_data_type, clang_data_type, air_type_suffix) \
clang_data_type sub_group_reduce_add(clang_data_type value) __attribute__((noduplicate, convergent)) asm("air.simd_sum." air_type_suffix); \
clang_data_type sub_group_reduce_min(clang_data_type value) __attribute__((noduplicate, convergent)) asm("air.simd_min." air_type_suffix); \
clang_data_type sub_group_reduce_max(clang_data_type value) __attribute__((noduplicate, convergent)) asm("air.simd_max." air_type_suffix); \
clang_data_type sub_group_inclusive_scan_add(clang_data_type value) __attribute__((noduplicate, convergent)) asm("air.simd_prefix_inclusive_sum." air_type_suffix); \
clang_data_type sub_group_exclusive_scan_add(clang_data_type value) __attribute__((noduplicate, convergent)) asm("air.simd_prefix_exclusive_sum." air_type_suffix);

FLOOR_METAL_SUB_GROUP_DATA_TYPES(FLOOR_METAL_AIR_SUBGROUP_OPS, )

#undef FLOOR_METAL_AIR_SUBGROUP_OPS

// defines a list of all supported subgroup data types as: ", type1, type2, type3"
#define FLOOR_METAL_SUB_GROUP_DATA_TYPES_LIST(func, floor_data_type, clang_data_type, air_type_suffix) , floor_data_type

// emulation for nonexistent simd_prefix_inclusive_min/max, simd_prefix_exclusive_min/max
template <bool is_exclusive, typename T, typename F>
requires (fl::device_info::has_fixed_known_simd_width() &&
		  ext::is_same_as_any_v<T FLOOR_METAL_SUB_GROUP_DATA_TYPES(FLOOR_METAL_SUB_GROUP_DATA_TYPES_LIST, )>)
floor_inline_always static T metal_sub_group_scan(T lane_var, F&& op) {
	const auto lane_idx = get_sub_group_local_id();
	T shfled_var;
#pragma unroll
	for (uint32_t delta = 1u; delta <= (fl::device_info::simd_width() / 2u); delta <<= 1u) {
		shfled_var = simd_shuffle_up(lane_var, delta);
		if (lane_idx >= delta) {
			lane_var = op(lane_var, shfled_var);
		}
	}
	
	if constexpr (is_exclusive) {
		// if this is an exclusive scan: shift one up
		const auto incl_result = lane_var;
		lane_var = simd_shuffle_up(incl_result, 1);
		return T(lane_idx == 0 ? T(0) : lane_var);
	} else {
		return lane_var;
	}
}
	
#undef FLOOR_METAL_SUB_GROUP_DATA_TYPES_LIST

// specialize for all supported operations
#define FLOOR_METAL_SUPPORT_SUBGROUP_OPS(func, floor_data_type, clang_data_type, air_type_suffix) \
template <> struct supports<ALGORITHM::SUB_GROUP_REDUCE, OP::ADD, floor_data_type> : public std::true_type {}; \
template <> struct supports<ALGORITHM::SUB_GROUP_REDUCE, OP::MIN, floor_data_type> : public std::true_type {}; \
template <> struct supports<ALGORITHM::SUB_GROUP_REDUCE, OP::MAX, floor_data_type> : public std::true_type {}; \
template <> struct supports<ALGORITHM::SUB_GROUP_INCLUSIVE_SCAN, OP::ADD, floor_data_type> : public std::true_type {}; \
template <> struct supports<ALGORITHM::SUB_GROUP_INCLUSIVE_SCAN, OP::MIN, floor_data_type> : public std::true_type {}; \
template <> struct supports<ALGORITHM::SUB_GROUP_INCLUSIVE_SCAN, OP::MAX, floor_data_type> : public std::true_type {}; \
template <> struct supports<ALGORITHM::SUB_GROUP_EXCLUSIVE_SCAN, OP::ADD, floor_data_type> : public std::true_type {}; \
template <> struct supports<ALGORITHM::SUB_GROUP_EXCLUSIVE_SCAN, OP::MIN, floor_data_type> : public std::true_type {}; \
template <> struct supports<ALGORITHM::SUB_GROUP_EXCLUSIVE_SCAN, OP::MAX, floor_data_type> : public std::true_type {};

FLOOR_METAL_SUB_GROUP_DATA_TYPES(FLOOR_METAL_SUPPORT_SUBGROUP_OPS, )

#undef FLOOR_METAL_SUPPORT_SUBGROUP_OPS

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
	return metal_sub_group_scan<false>(input_value, [](const auto& lhs, const auto& rhs) { return fl::floor_rt_min(lhs, rhs); });
}

template <OP op, typename data_type>
requires (op == OP::MAX)
static auto sub_group_inclusive_scan(const data_type input_value) {
	return metal_sub_group_scan<false>(input_value, [](const auto& lhs, const auto& rhs) { return fl::floor_rt_max(lhs, rhs); });
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
	return metal_sub_group_scan<true>(input_value, [](const auto& lhs, const auto& rhs) { return fl::floor_rt_min(lhs, rhs); });
}

template <OP op, typename data_type>
requires (op == OP::MAX)
static auto sub_group_exclusive_scan(const data_type input_value) {
	return metal_sub_group_scan<true>(input_value, [](const auto& lhs, const auto& rhs) { return fl::floor_rt_max(lhs, rhs); });
}

} // namespace algorithm::group

#undef FLOOR_METAL_SUB_GROUP_DATA_TYPES_SCALAR
#undef FLOOR_METAL_SUB_GROUP_DATA_TYPES_VECTOR
#undef FLOOR_METAL_SUB_GROUP_DATA_TYPES

} // namespace fl

#endif
