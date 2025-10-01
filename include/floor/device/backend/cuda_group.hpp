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

#if defined(FLOOR_DEVICE_CUDA)

namespace fl {

// all supported 16-bit and 32-bit scalar data types in CUDA SIMD/subgroup functions
#define FLOOR_CUDA_SUB_GROUP_DATA_TYPES_SCALAR_16_32(F, P) \
F(P, int16_t, int16_t, "s16") \
F(P, uint16_t, uint16_t, "u16") \
F(P, half, half, "f16") \
F(P, int32_t, int32_t, "s32") \
F(P, uint32_t, uint32_t, "u32") \
F(P, float, float, "f32")

// all supported 64-bit scalar data types in CUDA SIMD/subgroup functions
#define FLOOR_CUDA_SUB_GROUP_DATA_TYPES_SCALAR_64(F, P) \
F(P, int64_t, int64_t, "s64") \
F(P, uint64_t, uint64_t, "u64") \
F(P, double, double, "f64")

// all supported scalar data types in CUDA SIMD/subgroup functions
#define FLOOR_CUDA_SUB_GROUP_DATA_TYPES_SCALAR(F, P) \
FLOOR_CUDA_SUB_GROUP_DATA_TYPES_SCALAR_16_32(F, P) \
FLOOR_CUDA_SUB_GROUP_DATA_TYPES_SCALAR_64(F, P)

// all supported 16-bit and 32-bit vector data types in CUDA SIMD/subgroup functions
// NOTE: we emulate these
#define FLOOR_CUDA_SUB_GROUP_DATA_TYPES_VECTOR_16_32(F, P) \
F(P, short2, short2, "v2.s16") \
F(P, ushort2, ushort2, "v2.u16") \
F(P, half2, half2, "v2.f16") \
F(P, int2, int2, "v2.s32") \
F(P, uint2, uint2, "v2.u32") \
F(P, float2, float2, "v2.f32") \
F(P, short3, short3, "v3.s16") \
F(P, ushort3, ushort3, "v3.u16") \
F(P, half3, half3, "v3.f16") \
F(P, int3, int3, "v3.s32") \
F(P, uint3, uint3, "v3.u32") \
F(P, float3, float3, "v3.f32") \
F(P, short4, short4, "v4.s16") \
F(P, ushort4, ushort4, "v4.u16") \
F(P, half4, half4, "v4.f16") \
F(P, int4, int4, "v4.s32") \
F(P, uint4, uint4, "v4.u32") \
F(P, float4, float4, "v4.f32")

// all supported 64-bit vector data types in CUDA SIMD/subgroup functions
// NOTE: we emulate these
#define FLOOR_CUDA_SUB_GROUP_DATA_TYPES_VECTOR_64(F, P) \
F(P, long2, long2, "v2.s64") \
F(P, ulong2, ulong2, "v2.u64") \
F(P, double2, double2, "v2.f64") \
F(P, long3, long3, "v3.s64") \
F(P, ulong3, ulong3, "v3.u64") \
F(P, double3, double3, "v3.f64") \
F(P, long4, long4, "v4.s64") \
F(P, ulong4, ulong4, "v4.u64") \
F(P, double4, double4, "v4.f64")

// all supported vector data types in CUDA SIMD/subgroup functions
// NOTE: we emulate these
#define FLOOR_CUDA_SUB_GROUP_DATA_TYPES_VECTOR(F, P) \
FLOOR_CUDA_SUB_GROUP_DATA_TYPES_VECTOR_16_32(F, P) \
FLOOR_CUDA_SUB_GROUP_DATA_TYPES_VECTOR_64(F, P)

// all supported data types in CUDA SIMD/subgroup functions
#define FLOOR_CUDA_SUB_GROUP_DATA_TYPES(F, P) \
FLOOR_CUDA_SUB_GROUP_DATA_TYPES_SCALAR(F, P) \
FLOOR_CUDA_SUB_GROUP_DATA_TYPES_VECTOR(F, P)

// shuffle functionality
floor_inline_always static float simd_shuffle(const float lane_var, const uint32_t lane_id) {
	float ret;
	asm volatile("shfl.sync.idx.b32 %0, %1, %2, %3, 0xFFFFFFFF;"
				 : "=f"(ret) : "f"(lane_var), "r"(lane_id), "i"(fl::device_info::simd_width() - 1));
	return ret;
}
floor_inline_always static uint32_t simd_shuffle(const uint32_t lane_var, const uint32_t lane_id) {
	uint32_t ret;
	asm volatile("shfl.sync.idx.b32 %0, %1, %2, %3, 0xFFFFFFFF;"
				 : "=r"(ret) : "r"(lane_var), "r"(lane_id), "i"(fl::device_info::simd_width() - 1));
	return ret;
}
floor_inline_always static int32_t simd_shuffle(const int32_t lane_var, const uint32_t lane_id) {
	int32_t ret;
	asm volatile("shfl.sync.idx.b32 %0, %1, %2, %3, 0xFFFFFFFF;"
				 : "=r"(ret) : "r"(lane_var), "r"(lane_id), "i"(fl::device_info::simd_width() - 1));
	return ret;
}
template <typename type_16b> requires (fl::ext::is_same_as_any_v<type_16b, uint16_t, int16_t, half>)
floor_inline_always static type_16b simd_shuffle(const type_16b lane_var, const uint32_t lane_id) {
	using type_32b = std::conditional_t<fl::ext::is_floating_point_v<type_16b>, float,
										std::conditional_t<!std::is_signed_v<type_16b>, uint32_t, int32_t>>;
	return type_16b(simd_shuffle(type_32b(lane_var), lane_id));
}

floor_inline_always static float simd_shuffle_down(const float lane_var, const uint32_t delta) {
	float ret;
	asm volatile("shfl.sync.down.b32 %0, %1, %2, %3, 0xFFFFFFFF;"
				 : "=f"(ret) : "f"(lane_var), "r"(delta), "i"(fl::device_info::simd_width() - 1));
	return ret;
}
floor_inline_always static uint32_t simd_shuffle_down(const uint32_t lane_var, const uint32_t delta) {
	uint32_t ret;
	asm volatile("shfl.sync.down.b32 %0, %1, %2, %3, 0xFFFFFFFF;"
				 : "=r"(ret) : "r"(lane_var), "r"(delta), "i"(fl::device_info::simd_width() - 1));
	return ret;
}
floor_inline_always static int32_t simd_shuffle_down(const int32_t lane_var, const uint32_t delta) {
	int32_t ret;
	asm volatile("shfl.sync.down.b32 %0, %1, %2, %3, 0xFFFFFFFF;"
				 : "=r"(ret) : "r"(lane_var), "r"(delta), "i"(fl::device_info::simd_width() - 1));
	return ret;
}
template <typename type_16b> requires (fl::ext::is_same_as_any_v<type_16b, uint16_t, int16_t, half>)
floor_inline_always static type_16b simd_shuffle_down(const type_16b lane_var, const uint32_t delta) {
	using type_32b = std::conditional_t<fl::ext::is_floating_point_v<type_16b>, float,
										std::conditional_t<!std::is_signed_v<type_16b>, uint32_t, int32_t>>;
	return type_16b(simd_shuffle_down(type_32b(lane_var), delta));
}

floor_inline_always static float simd_shuffle_up(const float lane_var, const uint32_t delta) {
	float ret;
	asm volatile("shfl.sync.up.b32 %0, %1, %2, 0, 0xFFFFFFFF;"
				 : "=f"(ret) : "f"(lane_var), "r"(delta));
	return ret;
}
floor_inline_always static uint32_t simd_shuffle_up(const uint32_t lane_var, const uint32_t delta) {
	uint32_t ret;
	asm volatile("shfl.sync.up.b32 %0, %1, %2, 0, 0xFFFFFFFF;"
				 : "=r"(ret) : "r"(lane_var), "r"(delta));
	return ret;
}
floor_inline_always static int32_t simd_shuffle_up(const int32_t lane_var, const uint32_t delta) {
	int32_t ret;
	asm volatile("shfl.sync.up.b32 %0, %1, %2, 0, 0xFFFFFFFF;"
				 : "=r"(ret) : "r"(lane_var), "r"(delta));
	return ret;
}
template <typename type_16b> requires (fl::ext::is_same_as_any_v<type_16b, uint16_t, int16_t, half>)
floor_inline_always static type_16b simd_shuffle_up(const type_16b lane_var, const uint32_t delta) {
	using type_32b = std::conditional_t<fl::ext::is_floating_point_v<type_16b>, float,
										std::conditional_t<!std::is_signed_v<type_16b>, uint32_t, int32_t>>;
	return type_16b(simd_shuffle_up(type_32b(lane_var), delta));
}

floor_inline_always static float simd_shuffle_xor(const float lane_var, const uint32_t mask) {
	float ret;
	asm volatile("shfl.sync.bfly.b32 %0, %1, %2, %3, 0xFFFFFFFF;"
				 : "=f"(ret) : "f"(lane_var), "r"(mask), "i"(fl::device_info::simd_width() - 1));
	return ret;
}
floor_inline_always static uint32_t simd_shuffle_xor(const uint32_t lane_var, const uint32_t mask) {
	uint32_t ret;
	asm volatile("shfl.sync.bfly.b32 %0, %1, %2, %3, 0xFFFFFFFF;"
				 : "=r"(ret) : "r"(lane_var), "r"(mask), "i"(fl::device_info::simd_width() - 1));
	return ret;
}
floor_inline_always static int32_t simd_shuffle_xor(const int32_t lane_var, const uint32_t mask) {
	int32_t ret;
	asm volatile("shfl.sync.bfly.b32 %0, %1, %2, %3, 0xFFFFFFFF;"
				 : "=r"(ret) : "r"(lane_var), "r"(mask), "i"(fl::device_info::simd_width() - 1));
	return ret;
}
template <typename type_16b> requires (fl::ext::is_same_as_any_v<type_16b, uint16_t, int16_t, half>)
floor_inline_always static type_16b simd_shuffle_xor(const type_16b lane_var, const uint32_t mask) {
	using type_32b = std::conditional_t<fl::ext::is_floating_point_v<type_16b>, float,
										std::conditional_t<!std::is_signed_v<type_16b>, uint32_t, int32_t>>;
	return type_16b(simd_shuffle_xor(type_32b(lane_var), mask));
}

// emulate vector shuffle functions
#define SUB_GROUP_VECTOR_FUNC(func, floor_data_type, clang_data_type, type_suffix) \
floor_inline_always static floor_data_type func(floor_data_type vec, const uint32_t mask) { \
	return vec.apply([mask](floor_data_type::decayed_scalar_type value) { return func(value, mask); }); \
}
FLOOR_CUDA_SUB_GROUP_DATA_TYPES_VECTOR_16_32(SUB_GROUP_VECTOR_FUNC, simd_shuffle)
FLOOR_CUDA_SUB_GROUP_DATA_TYPES_VECTOR_16_32(SUB_GROUP_VECTOR_FUNC, simd_shuffle_down)
FLOOR_CUDA_SUB_GROUP_DATA_TYPES_VECTOR_16_32(SUB_GROUP_VECTOR_FUNC, simd_shuffle_up)
FLOOR_CUDA_SUB_GROUP_DATA_TYPES_VECTOR_16_32(SUB_GROUP_VECTOR_FUNC, simd_shuffle_xor)
#undef SUB_GROUP_CLANG_FUNC

// native CUDA ballot: always returns a 32-bit uint mask
floor_inline_always static uint32_t simd_ballot_native(bool predicate) {
	return __nvvm_vote_ballot_sync(0xFFFF'FFFFu, predicate);
}

floor_inline_always static uint32_t simd_ballot(bool predicate) {
	return simd_ballot_native(predicate);
}

floor_inline_always static uint64_t simd_ballot_64(bool predicate) {
	return simd_ballot_native(predicate);
}

// CUDA parallel group operation implementations / support
namespace algorithm::group {

//! performs a butterfly reduction inside the sub-group using the specific operation/function
template <typename T, typename F> requires (sizeof(T) <= 4)
floor_inline_always static T cuda_sub_group_reduce(T lane_var, F&& op) {
	using type_32b = std::conditional_t<fl::ext::is_floating_point_v<T>, float,
										std::conditional_t<!std::is_signed_v<T>, uint32_t, int32_t>>;
	auto lane_var_32b = type_32b(lane_var);
	type_32b shfled_var;
#pragma unroll
	for (uint32_t lane = fl::device_info::simd_width() / 2; lane > 0; lane >>= 1) {
		if constexpr (fl::ext::is_floating_point_v<T>) {
			asm volatile("shfl.sync.bfly.b32 %0, %1, %2, %3, 0xFFFFFFFF;"
						 : "=f"(shfled_var) : "f"(lane_var_32b), "i"(lane), "i"(fl::device_info::simd_width() - 1));
		} else {
			asm volatile("shfl.sync.bfly.b32 %0, %1, %2, %3, 0xFFFFFFFF;"
						 : "=r"(shfled_var) : "r"(lane_var_32b), "i"(lane), "i"(fl::device_info::simd_width() - 1));
		}
		lane_var_32b = op(lane_var_32b, shfled_var);
	}
	return T(lane_var_32b);
}

template <typename T, typename F> requires(sizeof(T) == 8)
floor_inline_always static T cuda_sub_group_reduce(T lane_var, F&& op) {
	T shfled_var;
#pragma unroll
	for (uint32_t lane = fl::device_info::simd_width() / 2; lane > 0; lane >>= 1) {
		uint32_t shfled_hi, shfled_lo, hi, lo;
		if constexpr(std::is_floating_point_v<T>) {
			asm volatile("mov.b64 { %0, %1 }, %2;" : "=r"(lo), "=r"(hi) : "d"(lane_var));
		} else {
			asm volatile("mov.b64 { %0, %1 }, %2;" : "=r"(lo), "=r"(hi) : "l"(lane_var));
		}
		asm volatile("shfl.sync.bfly.b32 %0, %2, %4, %5, 0xFFFFFFFF;\n"
					 "\tshfl.sync.bfly.b32 %1, %3, %4, %5, 0xFFFFFFFF;"
					 : "=r"(shfled_lo), "=r"(shfled_hi)
					 : "r"(lo), "r"(hi), "i"(lane), "i"(fl::device_info::simd_width() - 1));
		if constexpr(std::is_floating_point_v<T>) {
			asm volatile("mov.b64 %0, { %1, %2 };" : "=d"(shfled_var) : "r"(shfled_lo), "r"(shfled_hi));
		} else {
			asm volatile("mov.b64 %0, { %1, %2 };" : "=l"(shfled_var) : "r"(shfled_lo), "r"(shfled_hi));
		}
		lane_var = op(lane_var, shfled_var);
	}
	return lane_var;
}

template <bool is_exclusive, typename T, typename F> requires (sizeof(T) <= 4)
floor_inline_always static T cuda_sub_group_scan(T lane_var, F&& op) {
	const auto lane_idx = __nvvm_read_ptx_sreg_laneid();
	
	using type_32b = std::conditional_t<fl::ext::is_floating_point_v<T>, float,
										std::conditional_t<!std::is_signed_v<T>, uint32_t, int32_t>>;
	auto lane_var_32b = type_32b(lane_var);
	type_32b shfled_var;
#pragma unroll
	for (uint32_t delta = 1u; delta <= (fl::device_info::simd_width() / 2u); delta <<= 1u) {
		if constexpr(std::is_floating_point_v<T>) {
			asm volatile("shfl.sync.up.b32 %0, %1, %2, 0, 0xFFFFFFFF;"
						 : "=f"(shfled_var) : "f"(lane_var_32b), "i"(delta));
		} else {
			asm volatile("shfl.sync.up.b32 %0, %1, %2, 0, 0xFFFFFFFF;"
						 : "=r"(shfled_var) : "r"(lane_var_32b), "i"(delta));
		}
		if (lane_idx >= delta) {
			lane_var_32b = op(lane_var_32b, shfled_var);
		}
	}
	
	if constexpr (is_exclusive) {
		// if this is an exclusive scan: shift one up
		const auto incl_result = lane_var_32b;
		asm volatile("shfl.sync.up.b32 %0, %1, 1, 0, 0xFFFFFFFF;"
					 : "=r"(lane_var_32b) : "r"(incl_result));
		return T(lane_idx == 0 ? T(0) : lane_var_32b);
	} else {
		return T(lane_var_32b);
	}
}

#if FLOOR_DEVICE_INFO_CUDA_SM >= 80
//! use redux.sync with sm_80+
template <OP op, typename T>
requires (fl::ext::is_same_as_any_v<T, uint16_t, int16_t, uint32_t, int32_t> &&
		  (op == OP::ADD || op == OP::MIN || op == OP::MAX))
floor_inline_always static T cuda_sub_group_redux(T lane_var) {
	using int_type_32b = std::conditional_t<!std::is_signed_v<T>, uint32_t, int32_t>;
	const auto lane_var_32b = int_type_32b(lane_var);
	int_type_32b ret;
	if constexpr (op == OP::ADD) {
		if constexpr (!std::is_signed_v<T>) {
			asm volatile("redux.sync.add.u32 %0, %1, 0xFFFFFFFF;" : "=r"(ret) : "r"(lane_var_32b));
		} else {
			asm volatile("redux.sync.add.s32 %0, %1, 0xFFFFFFFF;" : "=r"(ret) : "r"(lane_var_32b));
		}
	} else if constexpr (op == OP::MIN) {
		if constexpr (!std::is_signed_v<T>) {
			asm volatile("redux.sync.min.u32 %0, %1, 0xFFFFFFFF;" : "=r"(ret) : "r"(lane_var_32b));
		} else {
			asm volatile("redux.sync.min.s32 %0, %1, 0xFFFFFFFF;" : "=r"(ret) : "r"(lane_var_32b));
		}
	} else if constexpr (op == OP::MAX) {
		if constexpr (!std::is_signed_v<T>) {
			asm volatile("redux.sync.max.u32 %0, %1, 0xFFFFFFFF;" : "=r"(ret) : "r"(lane_var_32b));
		} else {
			asm volatile("redux.sync.max.s32 %0, %1, 0xFFFFFFFF;" : "=r"(ret) : "r"(lane_var_32b));
		}
	}
	return T(ret);
}
#endif

// specialize for all supported operations
#define FLOOR_CUDA_SUPPORT_REDUCE_SUBGROUP_OPS(func, floor_data_type, clang_data_type, type_suffix) \
template <> struct supports<ALGORITHM::SUB_GROUP_REDUCE, OP::ADD, floor_data_type> : public std::true_type {}; \
template <> struct supports<ALGORITHM::SUB_GROUP_REDUCE, OP::MIN, floor_data_type> : public std::true_type {}; \
template <> struct supports<ALGORITHM::SUB_GROUP_REDUCE, OP::MAX, floor_data_type> : public std::true_type {};

#define FLOOR_CUDA_SUPPORT_SCAN_SUBGROUP_OPS(func, floor_data_type, clang_data_type, type_suffix) \
template <> struct supports<ALGORITHM::SUB_GROUP_INCLUSIVE_SCAN, OP::ADD, floor_data_type> : public std::true_type {}; \
template <> struct supports<ALGORITHM::SUB_GROUP_INCLUSIVE_SCAN, OP::MIN, floor_data_type> : public std::true_type {}; \
template <> struct supports<ALGORITHM::SUB_GROUP_INCLUSIVE_SCAN, OP::MAX, floor_data_type> : public std::true_type {}; \
template <> struct supports<ALGORITHM::SUB_GROUP_EXCLUSIVE_SCAN, OP::ADD, floor_data_type> : public std::true_type {}; \
template <> struct supports<ALGORITHM::SUB_GROUP_EXCLUSIVE_SCAN, OP::MIN, floor_data_type> : public std::true_type {}; \
template <> struct supports<ALGORITHM::SUB_GROUP_EXCLUSIVE_SCAN, OP::MAX, floor_data_type> : public std::true_type {};

// supported for all scalar data types (16/32/64-bit)
FLOOR_CUDA_SUB_GROUP_DATA_TYPES_SCALAR(FLOOR_CUDA_SUPPORT_REDUCE_SUBGROUP_OPS, )
// only supported for scalar 16/32-bit data types
FLOOR_CUDA_SUB_GROUP_DATA_TYPES_SCALAR_16_32(FLOOR_CUDA_SUPPORT_SCAN_SUBGROUP_OPS, )

#undef FLOOR_CUDA_SUPPORT_REDUCE_SUBGROUP_OPS
#undef FLOOR_CUDA_SUPPORT_SCAN_SUBGROUP_OPS

template <OP op, typename data_type>
requires (op == OP::ADD)
static auto sub_group_reduce(const data_type input_value) {
#if FLOOR_DEVICE_INFO_CUDA_SM >= 80
	if constexpr (fl::ext::is_same_as_any_v<data_type, uint16_t, int16_t, uint32_t, int32_t>) {
		return cuda_sub_group_redux<OP::ADD>(input_value);
	}
#endif
	return cuda_sub_group_reduce(input_value, std::plus<data_type> {});
}

template <OP op, typename data_type>
requires (op == OP::MIN)
static auto sub_group_reduce(const data_type input_value) {
#if FLOOR_DEVICE_INFO_CUDA_SM >= 80
	if constexpr (fl::ext::is_same_as_any_v<data_type, uint16_t, int16_t, uint32_t, int32_t>) {
		return cuda_sub_group_redux<OP::MIN>(input_value);
	}
#endif
	return cuda_sub_group_reduce(input_value, min_op<data_type> {});
}

template <OP op, typename data_type>
requires (op == OP::MAX)
static auto sub_group_reduce(const data_type input_value) {
#if FLOOR_DEVICE_INFO_CUDA_SM >= 80
	if constexpr (fl::ext::is_same_as_any_v<data_type, uint16_t, int16_t, uint32_t, int32_t>) {
		return cuda_sub_group_redux<OP::MAX>(input_value);
	}
#endif
	return cuda_sub_group_reduce(input_value, max_op<data_type> {});
}

template <OP op, typename data_type>
requires (op == OP::ADD)
static auto sub_group_inclusive_scan(const data_type input_value) {
	return cuda_sub_group_scan<false>(input_value, std::plus<data_type> {});
}

template <OP op, typename data_type>
requires (op == OP::MIN)
static auto sub_group_inclusive_scan(const data_type input_value) {
	return cuda_sub_group_scan<false>(input_value, min_op<data_type> {});
}

template <OP op, typename data_type>
requires (op == OP::MAX)
static auto sub_group_inclusive_scan(const data_type input_value) {
	return cuda_sub_group_scan<false>(input_value, max_op<data_type> {});
}

template <OP op, typename data_type>
requires (op == OP::ADD)
static auto sub_group_exclusive_scan(const data_type input_value) {
	return cuda_sub_group_scan<true>(input_value, std::plus<data_type> {});
}

template <OP op, typename data_type>
requires (op == OP::MIN)
static auto sub_group_exclusive_scan(const data_type input_value) {
	return cuda_sub_group_scan<true>(input_value, min_op<data_type> {});
}

template <OP op, typename data_type>
requires (op == OP::MAX)
static auto sub_group_exclusive_scan(const data_type input_value) {
	return cuda_sub_group_scan<true>(input_value, max_op<data_type> {});
}

} // namespace algorithm::group

#undef FLOOR_CUDA_SUB_GROUP_DATA_TYPES

#undef FLOOR_CUDA_SUB_GROUP_DATA_TYPES_VECTOR_16_32
#undef FLOOR_CUDA_SUB_GROUP_DATA_TYPES_VECTOR_64
#undef FLOOR_CUDA_SUB_GROUP_DATA_TYPES_VECTOR

#undef FLOOR_CUDA_SUB_GROUP_DATA_TYPES_SCALAR_16_32
#undef FLOOR_CUDA_SUB_GROUP_DATA_TYPES_SCALAR_64
#undef FLOOR_CUDA_SUB_GROUP_DATA_TYPES_SCALAR

} // namespace fl

#endif
