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

#if defined(FLOOR_DEVICE_HOST_COMPUTE)

#if !defined(FLOOR_DEVICE_HOST_COMPUTE_IS_DEVICE)
// loop unrolling in any of the functions below doesn't work on non-device Host-Compute, stop complaining about it
FLOOR_PUSH_AND_IGNORE_WARNING(pass-failed)
#endif

extern "C" {
#if !defined(FLOOR_DEVICE_HOST_COMPUTE_IS_DEVICE)
uint32_t floor_host_compute_simd_ballot(bool predicate) __attribute__((noduplicate)) FLOOR_HOST_COMPUTE_CC;
#endif
uint32_t floor_host_compute_device_simd_ballot(bool predicate) __attribute__((noduplicate)) FLOOR_HOST_COMPUTE_CC;
}

#if !defined(FLOOR_DEVICE_HOST_COMPUTE_IS_DEVICE)
using fl::half;
#endif

// all supported scalar data types in Host-Compute SIMD/subgroup functions
#define FLOOR_HOST_COMPUTE_SUB_GROUP_DATA_TYPES_SCALAR(F, P, D) \
F(P, D, int16_t, s16) \
F(P, D, uint16_t, u16) \
F(P, D, half, f16) \
F(P, D, int32_t, s32) \
F(P, D, uint32_t, u32) \
F(P, D, float, f32)

// all supported vector data types in Host-Compute SIMD/subgroup functions
// NOTE: we don't need or want to use clang vector types here
#define FLOOR_HOST_COMPUTE_SUB_GROUP_DATA_TYPES_VECTOR(F, P, D) \
F(P, D, fl::short2, v2s16) \
F(P, D, fl::ushort2, v2u16) \
F(P, D, fl::half2, v2f16) \
F(P, D, fl::int2, v2s32) \
F(P, D, fl::uint2, v2u32) \
F(P, D, fl::float2, v2f32) \
F(P, D, fl::short3, v3s16) \
F(P, D, fl::ushort3, v3u16) \
F(P, D, fl::half3, v3f16) \
F(P, D, fl::int3, v3s32) \
F(P, D, fl::uint3, v3u32) \
F(P, D, fl::float3, v3f32) \
F(P, D, fl::short4, v4s16) \
F(P, D, fl::ushort4, v4u16) \
F(P, D, fl::half4, v4f16) \
F(P, D, fl::int4, v4s32) \
F(P, D, fl::uint4, v4u32) \
F(P, D, fl::float4, v4f32)

// all supported data types in Host-Compute SIMD/subgroup functions
#define FLOOR_HOST_COMPUTE_SUB_GROUP_DATA_TYPES(F, D, P) \
FLOOR_HOST_COMPUTE_SUB_GROUP_DATA_TYPES_SCALAR(F, D, P) \
FLOOR_HOST_COMPUTE_SUB_GROUP_DATA_TYPES_VECTOR(F, D, P)

// scalar and vector subgroup functions can simply be defined and used

#if !defined(FLOOR_DEVICE_HOST_COMPUTE_IS_DEVICE)

#define SUB_GROUP_HOST_FUNC(func, device_suffix, floor_data_type, type_suffix) \
extern void floor_host_compute ## device_suffix ## func ## _ ## type_suffix(floor_data_type& ret, floor_data_type value, \
																			uint32_t lane_idx_delta_or_mask) \
__attribute__((noduplicate, convergent)) FLOOR_HOST_COMPUTE_CC;
FLOOR_HOST_COMPUTE_SUB_GROUP_DATA_TYPES(SUB_GROUP_HOST_FUNC, simd_shuffle, _)
FLOOR_HOST_COMPUTE_SUB_GROUP_DATA_TYPES(SUB_GROUP_HOST_FUNC, simd_shuffle_down, _)
FLOOR_HOST_COMPUTE_SUB_GROUP_DATA_TYPES(SUB_GROUP_HOST_FUNC, simd_shuffle_up, _)
FLOOR_HOST_COMPUTE_SUB_GROUP_DATA_TYPES(SUB_GROUP_HOST_FUNC, simd_shuffle_xor, _)
#undef SUB_GROUP_HOST_FUNC

#endif

#define SUB_GROUP_DEVICE_FUNC(func, device_suffix, floor_data_type, type_suffix) \
extern "C" void floor_host_compute ## device_suffix ## func ## _ ## type_suffix(floor_data_type& ret, floor_data_type value, \
																				uint32_t lane_idx_delta_or_mask) \
__attribute__((noduplicate, convergent)) FLOOR_HOST_COMPUTE_CC;
FLOOR_HOST_COMPUTE_SUB_GROUP_DATA_TYPES(SUB_GROUP_DEVICE_FUNC, simd_shuffle, _device_)
FLOOR_HOST_COMPUTE_SUB_GROUP_DATA_TYPES(SUB_GROUP_DEVICE_FUNC, simd_shuffle_down, _device_)
FLOOR_HOST_COMPUTE_SUB_GROUP_DATA_TYPES(SUB_GROUP_DEVICE_FUNC, simd_shuffle_up, _device_)
FLOOR_HOST_COMPUTE_SUB_GROUP_DATA_TYPES(SUB_GROUP_DEVICE_FUNC, simd_shuffle_xor, _device_)
#undef SUB_GROUP_FUNC

namespace fl {

// define user-facing simd_* functions
#define SUB_GROUP_FUNC(func, device_suffix, floor_data_type, type_suffix) \
floor_inline_always static floor_data_type func(floor_data_type value, uint32_t lane_idx_delta_or_mask) \
__attribute__((noduplicate, convergent)) FLOOR_HOST_COMPUTE_CC { \
	floor_data_type ret {}; \
	floor_host_compute ## device_suffix ## func ## _ ## type_suffix(ret, value, lane_idx_delta_or_mask); \
	return ret; \
}
#if !defined(FLOOR_DEVICE_HOST_COMPUTE_IS_DEVICE)
FLOOR_HOST_COMPUTE_SUB_GROUP_DATA_TYPES(SUB_GROUP_FUNC, simd_shuffle, _)
FLOOR_HOST_COMPUTE_SUB_GROUP_DATA_TYPES(SUB_GROUP_FUNC, simd_shuffle_down, _)
FLOOR_HOST_COMPUTE_SUB_GROUP_DATA_TYPES(SUB_GROUP_FUNC, simd_shuffle_up, _)
FLOOR_HOST_COMPUTE_SUB_GROUP_DATA_TYPES(SUB_GROUP_FUNC, simd_shuffle_xor, _)
#else
FLOOR_HOST_COMPUTE_SUB_GROUP_DATA_TYPES(SUB_GROUP_FUNC, simd_shuffle, _device_)
FLOOR_HOST_COMPUTE_SUB_GROUP_DATA_TYPES(SUB_GROUP_FUNC, simd_shuffle_down, _device_)
FLOOR_HOST_COMPUTE_SUB_GROUP_DATA_TYPES(SUB_GROUP_FUNC, simd_shuffle_up, _device_)
FLOOR_HOST_COMPUTE_SUB_GROUP_DATA_TYPES(SUB_GROUP_FUNC, simd_shuffle_xor, _device_)
#endif
#undef SUB_GROUP_FUNC

// native Host-Compute ballot: always returns a 32-bit uint mask
floor_inline_always static uint32_t simd_ballot_native(bool predicate) __attribute__((noduplicate, convergent)) {
#if !defined(FLOOR_DEVICE_HOST_COMPUTE_IS_DEVICE)
	return floor_host_compute_simd_ballot(predicate);
#else
	return floor_host_compute_device_simd_ballot(predicate);
#endif
}

floor_inline_always static uint32_t simd_ballot(bool predicate) __attribute__((noduplicate, convergent)) {
	return simd_ballot_native(predicate);
}

floor_inline_always static uint64_t simd_ballot_64(bool predicate) __attribute__((noduplicate, convergent)) {
	return simd_ballot_native(predicate);
}

// Host-Compute parallel group operation implementations / support
namespace algorithm::group {

// defines a list of all supported subgroup data types as: ", type1, type2, type3"
#define FLOOR_HOST_COMPUTE_SUB_GROUP_DATA_TYPES_LIST(func, device_suffix, floor_data_type, type_suffix) , floor_data_type

//! performs a butterfly reduction inside the sub-group using the specific operation/function
template <typename T, typename F>
requires (ext::is_same_as_any_v<T FLOOR_HOST_COMPUTE_SUB_GROUP_DATA_TYPES(FLOOR_HOST_COMPUTE_SUB_GROUP_DATA_TYPES_LIST, , )>)
floor_inline_always static T host_compute_sub_group_reduce(T lane_var, F&& op) {
	T shfled_var;
#pragma unroll
	for (uint32_t lane = fl::host_limits::simd_width / 2; lane > 0; lane >>= 1) {
		shfled_var = simd_shuffle_xor(lane_var, lane);
		lane_var = op(lane_var, T(shfled_var));
	}
	return lane_var;
}

//! performs an inclusive or exclusive scan inside the sub-group using the specific operation/function
template <bool is_exclusive, typename T, typename F>
requires (ext::is_same_as_any_v<T FLOOR_HOST_COMPUTE_SUB_GROUP_DATA_TYPES(FLOOR_HOST_COMPUTE_SUB_GROUP_DATA_TYPES_LIST, , )>)
floor_inline_always static T host_compute_sub_group_scan(T lane_var, F&& op) {
	const auto lane_idx = get_sub_group_local_id();
	T shfled_var;
#pragma unroll
	for (uint32_t delta = 1u; delta <= (fl::host_limits::simd_width / 2u); delta <<= 1u) {
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

#undef FLOOR_HOST_COMPUTE_SUB_GROUP_DATA_TYPES_LIST

// specialize for all supported operations
#define FLOOR_HOST_COMPUTE_SUPPORT_SUBGROUP_OPS(func, device_suffix, floor_data_type, type_suffix) \
template <> struct supports<ALGORITHM::SUB_GROUP_REDUCE, OP::ADD, floor_data_type> : public std::true_type {}; \
template <> struct supports<ALGORITHM::SUB_GROUP_REDUCE, OP::MIN, floor_data_type> : public std::true_type {}; \
template <> struct supports<ALGORITHM::SUB_GROUP_REDUCE, OP::MAX, floor_data_type> : public std::true_type {}; \
template <> struct supports<ALGORITHM::SUB_GROUP_INCLUSIVE_SCAN, OP::ADD, floor_data_type> : public std::true_type {}; \
template <> struct supports<ALGORITHM::SUB_GROUP_INCLUSIVE_SCAN, OP::MIN, floor_data_type> : public std::true_type {}; \
template <> struct supports<ALGORITHM::SUB_GROUP_INCLUSIVE_SCAN, OP::MAX, floor_data_type> : public std::true_type {}; \
template <> struct supports<ALGORITHM::SUB_GROUP_EXCLUSIVE_SCAN, OP::ADD, floor_data_type> : public std::true_type {}; \
template <> struct supports<ALGORITHM::SUB_GROUP_EXCLUSIVE_SCAN, OP::MIN, floor_data_type> : public std::true_type {}; \
template <> struct supports<ALGORITHM::SUB_GROUP_EXCLUSIVE_SCAN, OP::MAX, floor_data_type> : public std::true_type {};

FLOOR_HOST_COMPUTE_SUB_GROUP_DATA_TYPES(FLOOR_HOST_COMPUTE_SUPPORT_SUBGROUP_OPS, , )

#undef FLOOR_HOST_COMPUTE_SUPPORT_SUBGROUP_OPS

template <OP op, typename data_type>
requires (op == OP::ADD)
static inline auto sub_group_reduce(const data_type input_value) {
	return host_compute_sub_group_reduce(input_value, std::plus<data_type> {});
}

template <OP op, typename data_type>
requires (op == OP::MIN)
static inline auto sub_group_reduce(const data_type input_value) {
	return host_compute_sub_group_reduce(input_value, min_op<data_type> {});
}

template <OP op, typename data_type>
requires (op == OP::MAX)
static inline auto sub_group_reduce(const data_type input_value) {
	return host_compute_sub_group_reduce(input_value, max_op<data_type> {});
}

template <OP op, typename data_type>
requires (op == OP::ADD)
static inline auto sub_group_inclusive_scan(const data_type input_value) {
	return host_compute_sub_group_scan<false>(input_value, std::plus<data_type> {});
}

template <OP op, typename data_type>
requires (op == OP::MIN)
static inline auto sub_group_inclusive_scan(const data_type input_value) {
	return host_compute_sub_group_scan<false>(input_value, min_op<data_type> {});
}

template <OP op, typename data_type>
requires (op == OP::MAX)
static inline auto sub_group_inclusive_scan(const data_type input_value) {
	return host_compute_sub_group_scan<false>(input_value, max_op<data_type> {});
}

template <OP op, typename data_type>
requires (op == OP::ADD)
static inline auto sub_group_exclusive_scan(const data_type input_value) {
	return host_compute_sub_group_scan<true>(input_value, std::plus<data_type> {});
}

template <OP op, typename data_type>
requires (op == OP::MIN)
static inline auto sub_group_exclusive_scan(const data_type input_value) {
	return host_compute_sub_group_scan<true>(input_value, min_op<data_type> {});
}

template <OP op, typename data_type>
requires (op == OP::MAX)
static inline auto sub_group_exclusive_scan(const data_type input_value) {
	return host_compute_sub_group_scan<true>(input_value, max_op<data_type> {});
}

} // namespace algorithm::group

// NOTE: don't undef FLOOR_HOST_COMPUTE_SUB_GROUP_DATA_TYPES* here as we still need them

} // namespace fl

#if !defined(FLOOR_DEVICE_HOST_COMPUTE_IS_DEVICE)
FLOOR_POP_WARNINGS()
#endif

#endif
