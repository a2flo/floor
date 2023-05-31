/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2023 Florian Ziesche
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

#ifndef __FLOOR_COMPUTE_DEVICE_METAL_HPP__
#define __FLOOR_COMPUTE_DEVICE_METAL_HPP__

#if defined(FLOOR_COMPUTE_METAL)

// we always have reverse bits instructions
#define FLOOR_COMPUTE_INFO_HAS_REVERSE_BITS_32 1
#define FLOOR_COMPUTE_INFO_HAS_REVERSE_BITS_64 1

namespace std {
	// straightforward wrapping, use the fast_* version when possible
	const_func metal_func float sqrt(float) asm("air.fast_sqrt.f32");
	const_func metal_func float rsqrt(float) asm("air.fast_rsqrt.f32");
	const_func metal_func float fabs(float) asm("air.fast_fabs.f32");
	const_func metal_func float abs(float) asm("air.fast_fabs.f32");
	const_func metal_func float fmin(float, float) asm("air.fast_fmin.f32");
	const_func metal_func float fmax(float, float) asm("air.fast_fmax.f32");
	const_func metal_func float floor(float) asm("air.fast_floor.f32");
	const_func metal_func float ceil(float) asm("air.fast_ceil.f32");
	const_func metal_func float round(float) asm("air.fast_round.f32");
	const_func metal_func float trunc(float) asm("air.fast_trunc.f32");
	const_func metal_func float rint(float) asm("air.fast_rint.f32");
	const_func metal_func float sin(float) asm("air.fast_sin.f32");
	const_func metal_func float cos(float) asm("air.fast_cos.f32");
	const_func metal_func float tan(float) asm("air.fast_tan.f32");
	const_func metal_func float asin(float) asm("air.fast_asin.f32");
	const_func metal_func float acos(float) asm("air.fast_acos.f32");
	const_func metal_func float atan(float) asm("air.fast_atan.f32");
	const_func metal_func float atan2(float, float) asm("air.fast_atan2.f32");
	const_func metal_func float sinh(float) asm("air.fast_sinh.f32");
	const_func metal_func float cosh(float) asm("air.fast_cosh.f32");
	const_func metal_func float tanh(float) asm("air.fast_tanh.f32");
	const_func metal_func float asinh(float) asm("air.fast_asinh.f32");
	const_func metal_func float acosh(float) asm("air.fast_acosh.f32");
	const_func metal_func float atanh(float) asm("air.fast_atanh.f32");
	const_func metal_func float fma(float, float, float) asm("air.fma.f32");
	const_func metal_func float exp(float) asm("air.fast_exp.f32");
	const_func metal_func float exp2(float) asm("air.fast_exp2.f32");
	const_func metal_func float log(float) asm("air.fast_log.f32");
	const_func metal_func float log2(float) asm("air.fast_log2.f32");
	const_func metal_func float pow(float, float) asm("air.fast_pow.f32");
	const_func metal_func float fmod(float, float) asm("air.fast_fmod.f32");

	const_func metal_func half sqrt(half) asm("air.sqrt.f16");
	const_func metal_func half rsqrt(half) asm("air.rsqrt.f16");
	const_func metal_func half fabs(half) asm("air.fabs.f16");
	const_func metal_func half abs(half) asm("air.fabs.f16");
	const_func metal_func half fmin(half, half) asm("air.fmin.f16");
	const_func metal_func half fmax(half, half) asm("air.fmax.f16");
	const_func metal_func half floor(half) asm("air.floor.f16");
	const_func metal_func half ceil(half) asm("air.ceil.f16");
	const_func metal_func half round(half) asm("air.round.f16");
	const_func metal_func half trunc(half) asm("air.trunc.f16");
	const_func metal_func half rint(half) asm("air.rint.f16");
	const_func metal_func half sin(half) asm("air.sin.f16");
	const_func metal_func half cos(half) asm("air.cos.f16");
	const_func metal_func half tan(half) asm("air.tan.f16");
	const_func metal_func half asin(half) asm("air.asin.f16");
	const_func metal_func half acos(half) asm("air.acos.f16");
	const_func metal_func half atan(half) asm("air.atan.f16");
	const_func metal_func half atan2(half, half) asm("air.atan2.f16");
	const_func metal_func half sinh(half) asm("air.sinh.f16");
	const_func metal_func half cosh(half) asm("air.cosh.f16");
	const_func metal_func half tanh(half) asm("air.tanh.f16");
	const_func metal_func half asinh(half) asm("air.asinh.f16");
	const_func metal_func half acosh(half) asm("air.acosh.f16");
	const_func metal_func half atanh(half) asm("air.atanh.f16");
	const_func metal_func half fma(half, half, half) asm("air.fma.f16");
	const_func metal_func half exp(half) asm("air.exp.f16");
	const_func metal_func half exp2(half) asm("air.exp2.f16");
	const_func metal_func half log(half) asm("air.log.f16");
	const_func metal_func half log2(half) asm("air.log2.f16");
	const_func metal_func half pow(half, half) asm("air.pow.f16");
	const_func metal_func half fmod(half, half) asm("air.fmod.f16");
	
	const_func floor_inline_always metal_func float copysign(float a, float b) {
		// metal/air doesn't have a builtin function/intrinsic for this and does bit ops instead -> do the same
		return bit_cast<float>((bit_cast<uint32_t>(a) & 0x7FFF'FFFFu) | (bit_cast<uint32_t>(b) & 0x8000'0000u));
	}
	
	const_func metal_func int8_t abs(int8_t) asm("air.abs.s.i8");
	const_func metal_func int16_t abs(int16_t) asm("air.abs.s.i16");
	const_func metal_func int32_t abs(int32_t) asm("air.abs.s.i32");
	const_func metal_func int64_t abs(int64_t val) { return val < int64_t(0) ? -val : val; }
	const_func metal_func uint8_t abs(uint8_t) asm("air.abs.u.i8");
	const_func metal_func uint16_t abs(uint16_t) asm("air.abs.u.i16");
	const_func metal_func uint32_t abs(uint32_t) asm("air.abs.u.i32");
	const_func metal_func uint64_t abs(uint64_t val) { return val; }
	
	const_func metal_func int8_t floor_rt_min(int8_t, int8_t) asm("air.min.s.i8");
	const_func metal_func uint8_t floor_rt_min(uint8_t, uint8_t) asm("air.min.u.i8");
	const_func metal_func int16_t floor_rt_min(int16_t, int16_t) asm("air.min.s.i16");
	const_func metal_func uint16_t floor_rt_min(uint16_t, uint16_t) asm("air.min.u.i16");
	const_func metal_func int32_t floor_rt_min(int32_t, int32_t) asm("air.min.s.i32");
	const_func metal_func uint32_t floor_rt_min(uint32_t, uint32_t) asm("air.min.u.i32");
	const_func metal_func int64_t floor_rt_min(int64_t x, int64_t y) { return x <= y ? x : y; }
	const_func metal_func uint64_t floor_rt_min(uint64_t x, uint64_t y) { return x <= y ? x : y; }
	const_func metal_func half floor_rt_min(half, half) asm("air.fmin.f16");
	const_func metal_func float floor_rt_min(float, float) asm("air.fast_fmin.f32");
	const_func metal_func int8_t floor_rt_max(int8_t, int8_t) asm("air.max.s.i8");
	const_func metal_func uint8_t floor_rt_max(uint8_t, uint8_t) asm("air.max.u.i8");
	const_func metal_func int16_t floor_rt_max(int16_t, int16_t) asm("air.max.s.i16");
	const_func metal_func uint16_t floor_rt_max(uint16_t, uint16_t) asm("air.max.u.i16");
	const_func metal_func int32_t floor_rt_max(int32_t, int32_t) asm("air.max.s.i32");
	const_func metal_func uint32_t floor_rt_max(uint32_t, uint32_t) asm("air.max.u.i32");
	const_func metal_func int64_t floor_rt_max(int64_t x, int64_t y) { return x >= y ? x : y; }
	const_func metal_func uint64_t floor_rt_max(uint64_t x, uint64_t y) { return x >= y ? x : y; }
	const_func metal_func half floor_rt_max(half, half) asm("air.fmax.f16");
	const_func metal_func float floor_rt_max(float, float) asm("air.fast_fmax.f32");
	
	const_func metal_func int32_t mulhi(int32_t x, int32_t y) asm("air.mul_hi.i32");
	const_func metal_func uint32_t mulhi(uint32_t x, uint32_t y) asm("air.mul_hi.u.i32");
	
	const_func metal_func uint32_t madsat(uint32_t, uint32_t, uint32_t) asm("air.mad_sat.u.i32");
	
	// non-standard bit counting functions (don't use these directly, use math::func instead)
	const_func metal_func uint16_t air_rt_clz(uint16_t, bool undef = false) asm("air.clz.i16");
	const_func metal_func uint32_t air_rt_clz(uint32_t, bool undef = false) asm("air.clz.i32");
	const_func metal_func uint16_t air_rt_ctz(uint16_t, bool undef = false) asm("air.ctz.i16");
	const_func metal_func uint32_t air_rt_ctz(uint32_t, bool undef = false) asm("air.ctz.i32");
	
	const_func floor_inline_always metal_func uint16_t floor_rt_clz(uint16_t x) {
		return air_rt_clz(x);
	}
	const_func floor_inline_always metal_func uint32_t floor_rt_clz(uint32_t x) {
		return air_rt_clz(x);
	}
	const_func floor_inline_always metal_func uint64_t floor_rt_clz(uint64_t x) {
		const auto upper = uint32_t(x >> 32ull);
		const auto lower = uint32_t(x & 0xFFFFFFFFull);
		const auto clz_upper = floor_rt_clz(upper);
		const auto clz_lower = floor_rt_clz(lower);
		return (clz_upper < 32 ? clz_upper : (clz_upper + clz_lower));
	}
	const_func floor_inline_always metal_func uint16_t floor_rt_ctz(uint16_t x) {
		return air_rt_ctz(x);
	}
	const_func floor_inline_always metal_func uint32_t floor_rt_ctz(uint32_t x) {
		return air_rt_ctz(x);
	}
	const_func floor_inline_always metal_func uint64_t floor_rt_ctz(uint64_t x) {
		const auto upper = uint32_t(x >> 32ull);
		const auto lower = uint32_t(x & 0xFFFFFFFFull);
		const auto ctz_upper = floor_rt_ctz(upper);
		const auto ctz_lower = floor_rt_ctz(lower);
		return (ctz_lower < 32 ? ctz_lower : (ctz_upper + ctz_lower));
	}
	const_func metal_func uint16_t floor_rt_popcount(uint16_t) asm("air.popcount.i16");
	const_func metal_func uint32_t floor_rt_popcount(uint32_t) asm("air.popcount.i32");
	const_func floor_inline_always metal_func uint64_t floor_rt_popcount(uint64_t x) {
		const auto upper = uint32_t(x >> 32ull);
		const auto lower = uint32_t(x & 0xFFFFFFFFull);
		return floor_rt_popcount(upper) + floor_rt_popcount(lower);
	}
	const_func metal_func uint32_t floor_rt_reverse_bits(uint32_t) asm("air.reverse_bits.i32");
	const_func metal_func uint64_t floor_rt_reverse_bits(uint64_t value) {
		const auto low_rev = floor_rt_reverse_bits(uint32_t(value));
		const auto high_rev = floor_rt_reverse_bits(uint32_t(value >> 32ull));
		return (uint64_t(low_rev) << 32ull) | uint64_t(high_rev);
	}
	
}

// metal itself does not provide get_*_id/get_*_size functions, but rather handles this by using additional kernel arguments
// that must be tagged with a specific attribute. personally, I think this is a bad design decision that adds unnecessary work
// to both the user and the backend/frontend developer (me), and makes everything more complex than it has to be. furthermore,
// the way of doing it like this makes it incompatible to the way opencl and cuda handle this.
// note that "air.get_*_id.i32" intrinsics do exist, but are only partially available or not available at all to all backends,
// and for those that are supported, the return type is sometimes a 32-bit and sometimes a 64-bit integer -> unusable.
// solution: add compiler voodoo that automatically adds the special kernel arguments and loads these arguments in places
// where the following "intrinsics" are used. thus, compatible to the opencl/cuda way of doing it (and sane ...).
FLOOR_GLOBAL_ID_RANGE_ATTR const_func uint32_t get_global_id(uint32_t dim) asm("floor.get_global_id.i32");
FLOOR_GLOBAL_SIZE_RANGE_ATTR const_func uint32_t get_global_size(uint32_t dim) asm("floor.get_global_size.i32");
FLOOR_LOCAL_ID_RANGE_ATTR const_func uint32_t get_local_id(uint32_t dim) asm("floor.get_local_id.i32");
FLOOR_LOCAL_SIZE_RANGE_ATTR const_func uint32_t get_local_size(uint32_t dim) asm("floor.get_local_size.i32");
FLOOR_GROUP_ID_RANGE_ATTR const_func uint32_t get_group_id(uint32_t dim) asm("floor.get_group_id.i32");
FLOOR_GROUP_SIZE_RANGE_ATTR const_func uint32_t get_group_size(uint32_t dim) asm("floor.get_group_size.i32");
[[range(1u, 3u)]] const_func uint32_t get_work_dim() asm("floor.get_work_dim.i32");

#if FLOOR_COMPUTE_INFO_HAS_SUB_GROUPS != 0
FLOOR_SUB_GROUP_ID_RANGE_ATTR const_func uint32_t get_sub_group_id() asm("floor.get_sub_group_id.i32");
FLOOR_SUB_GROUP_LOCAL_ID_RANGE_ATTR const_func uint32_t get_sub_group_local_id() asm("floor.get_sub_group_local_id.i32");
FLOOR_SUB_GROUP_SIZE_RANGE_ATTR const_func uint32_t get_sub_group_size() asm("floor.get_sub_group_size.i32");
FLOOR_NUM_SUB_GROUPS_RANGE_ATTR const_func uint32_t get_num_sub_groups() asm("floor.get_num_sub_groups.i32");

#define SUB_GROUP_TYPES(F, P) F(int32_t, "s.i32", P) F(uint32_t, "u.i32", P) F(float, "f32", P)
#define SUB_GROUP_FUNC(type, type_str, func) type func(type, uint16_t lane_idx_delta_or_mask) __attribute__((noduplicate, convergent)) asm("air." #func "." type_str);
SUB_GROUP_TYPES(SUB_GROUP_FUNC, simd_shuffle)
SUB_GROUP_TYPES(SUB_GROUP_FUNC, simd_shuffle_down)
SUB_GROUP_TYPES(SUB_GROUP_FUNC, simd_shuffle_up)
SUB_GROUP_TYPES(SUB_GROUP_FUNC, simd_shuffle_xor)
#undef SUB_GROUP_TYPES
#undef SUB_GROUP_FUNC

#endif

// barrier and mem_fence functionality
// (note that there is also a air.mem_barrier function, but it seems non-functional/broken and isn't used by apples code)
void air_wg_barrier(uint32_t mem_scope, int32_t sync_scope) __attribute__((noduplicate, convergent)) asm("air.wg.barrier");

floor_inline_always static void global_barrier() __attribute__((noduplicate, convergent)) {
	air_wg_barrier(FLOOR_METAL_MEM_SCOPE_GLOBAL, FLOOR_METAL_SYNC_SCOPE_LOCAL);
}
floor_inline_always static void global_mem_fence() __attribute__((noduplicate, convergent)) {
	air_wg_barrier(FLOOR_METAL_MEM_SCOPE_GLOBAL, FLOOR_METAL_SYNC_SCOPE_LOCAL);
}
floor_inline_always static void global_read_mem_fence() __attribute__((noduplicate, convergent)) {
	air_wg_barrier(FLOOR_METAL_MEM_SCOPE_GLOBAL, FLOOR_METAL_SYNC_SCOPE_LOCAL);
}
floor_inline_always static void global_write_mem_fence() __attribute__((noduplicate, convergent)) {
	air_wg_barrier(FLOOR_METAL_MEM_SCOPE_GLOBAL, FLOOR_METAL_SYNC_SCOPE_LOCAL);
}

floor_inline_always static void local_barrier() __attribute__((noduplicate, convergent)) {
	air_wg_barrier(FLOOR_METAL_MEM_SCOPE_LOCAL, FLOOR_METAL_SYNC_SCOPE_LOCAL);
}
floor_inline_always static void local_mem_fence() __attribute__((noduplicate, convergent)) {
	air_wg_barrier(FLOOR_METAL_MEM_SCOPE_LOCAL, FLOOR_METAL_SYNC_SCOPE_LOCAL);
}
floor_inline_always static void local_read_mem_fence() __attribute__((noduplicate, convergent)) {
	air_wg_barrier(FLOOR_METAL_MEM_SCOPE_LOCAL, FLOOR_METAL_SYNC_SCOPE_LOCAL);
}
floor_inline_always static void local_write_mem_fence() __attribute__((noduplicate, convergent)) {
	air_wg_barrier(FLOOR_METAL_MEM_SCOPE_LOCAL, FLOOR_METAL_SYNC_SCOPE_LOCAL);
}

floor_inline_always static void barrier() __attribute__((noduplicate, convergent)) {
	air_wg_barrier(FLOOR_METAL_MEM_SCOPE_ALL, FLOOR_METAL_SYNC_SCOPE_LOCAL);
}

floor_inline_always static void image_barrier() __attribute__((noduplicate, convergent)) {
	air_wg_barrier(FLOOR_METAL_MEM_SCOPE_TEXTURE, FLOOR_METAL_SYNC_SCOPE_LOCAL);
}
floor_inline_always static void image_mem_fence() __attribute__((noduplicate, convergent)) {
	air_wg_barrier(FLOOR_METAL_MEM_SCOPE_TEXTURE, FLOOR_METAL_SYNC_SCOPE_LOCAL);
}
floor_inline_always static void image_read_mem_fence() __attribute__((noduplicate, convergent)) {
	air_wg_barrier(FLOOR_METAL_MEM_SCOPE_TEXTURE, FLOOR_METAL_SYNC_SCOPE_LOCAL);
}
floor_inline_always static void image_write_mem_fence() __attribute__((noduplicate, convergent)) {
	air_wg_barrier(FLOOR_METAL_MEM_SCOPE_TEXTURE, FLOOR_METAL_SYNC_SCOPE_LOCAL);
}

#if !defined(FLOOR_COMPUTE_HAS_SOFT_PRINTF) || (FLOOR_COMPUTE_HAS_SOFT_PRINTF == 0)
// not supported (neither __printf_cl nor __builtin_printf work)
#define printf(...)
#else // software printf implementation

global uint32_t* floor_get_printf_buffer() asm("floor.builtin.get_printf_buffer");
#include <floor/compute/device/soft_printf.hpp>

template <size_t format_N, typename... Args>
static void printf(constant const char (&format)[format_N], const Args&... args) {
	soft_printf::as::printf_impl(format, args...);
}

#endif

// tessellation
const_func metal_func uint16_t metal_get_num_patch_control_points() asm("air.get_num_patch_control_points");

template <typename point_data_t>
class metal_patch_control_point {
public:
	size_t size() const {
		return metal_get_num_patch_control_points();
	}
	
	auto operator[](const size_t idx) const {
		return __libfloor_access_patch_control_point(uint32_t(idx), p, point_data_t {});
	}
	
protected:
	//! compiler-internal opaque type to deal with generic control point types
	__patch_control_point_t p;
	
};

#if FLOOR_COMPUTE_METAL_HAS_SIMD_REDUCTION != 0 || FLOOR_COMPUTE_INFO_HAS_SUB_GROUPS != 0
//! Metal parallel group operation implementations / support
namespace compute_algorithm::group {

#if FLOOR_COMPUTE_METAL_HAS_SIMD_REDUCTION != 0 // -> AIR backend functions
int32_t sub_group_reduce_add(int32_t value) __attribute__((noduplicate, convergent)) asm("air.simd_sum.s.i32");
uint32_t sub_group_reduce_add(uint32_t value) __attribute__((noduplicate, convergent)) asm("air.simd_sum.u.i32");
float sub_group_reduce_add(float value) __attribute__((noduplicate, convergent)) asm("air.simd_sum.f32");

int32_t sub_group_reduce_min(int32_t value) __attribute__((noduplicate, convergent)) asm("air.simd_min.s.i32");
uint32_t sub_group_reduce_min(uint32_t value) __attribute__((noduplicate, convergent)) asm("air.simd_min.u.i32");
float sub_group_reduce_min(float value) __attribute__((noduplicate, convergent)) asm("air.simd_min.f32");

int32_t sub_group_reduce_max(int32_t value) __attribute__((noduplicate, convergent)) asm("air.simd_max.s.i32");
uint32_t sub_group_reduce_max(uint32_t value) __attribute__((noduplicate, convergent)) asm("air.simd_max.u.i32");
float sub_group_reduce_max(float value) __attribute__((noduplicate, convergent)) asm("air.simd_max.f32");

int32_t sub_group_inclusive_scan_add(int32_t value) __attribute__((noduplicate, convergent)) asm("air.simd_prefix_inclusive_sum.s.i32");
uint32_t sub_group_inclusive_scan_add(uint32_t value) __attribute__((noduplicate, convergent)) asm("air.simd_prefix_inclusive_sum.u.i32");
float sub_group_inclusive_scan_add(float value) __attribute__((noduplicate, convergent)) asm("air.simd_prefix_inclusive_sum.f32");

int32_t sub_group_exclusive_scan_add(int32_t value) __attribute__((noduplicate, convergent)) asm("air.simd_prefix_exclusive_sum.s.i32");
uint32_t sub_group_exclusive_scan_add(uint32_t value) __attribute__((noduplicate, convergent)) asm("air.simd_prefix_exclusive_sum.u.i32");
float sub_group_exclusive_scan_add(float value) __attribute__((noduplicate, convergent)) asm("air.simd_prefix_exclusive_sum.f32");

#elif FLOOR_COMPUTE_INFO_HAS_SUB_GROUPS != 0 // -> fallback to manual sub-group implementation
//! performs a butterfly reduction inside the sub-group using the specific operation/function
template <typename T, typename F> requires(is_same_v<T, int32_t> || is_same_v<T, uint32_t> || is_same_v<T, float>)
floor_inline_always static T metal_sub_group_reduce(T lane_var, F&& op) {
	// on Metal we only have a fixed+known SIMD-width at compile-time when we're specifically compiling for a device
	if constexpr (device_info::has_fixed_known_simd_width()) {
		T shfled_var;
#pragma unroll
		for (uint32_t lane = device_info::simd_width() / 2; lane > 0; lane >>= 1) {
			shfled_var = simd_shuffle_xor(lane_var, lane);
			lane_var = op(lane_var, shfled_var);
		}
		return lane_var;
	} else {
		// dynamic version
		T shfled_var;
		for (uint32_t lane = get_sub_group_size() / 2; lane > 0; lane >>= 1) {
			shfled_var = simd_shuffle_xor(lane_var, lane);
			lane_var = op(lane_var, shfled_var);
		}
		return lane_var;
	}
}

template <typename T> floor_inline_always static T sub_group_reduce_add(T lane_var) {
	return metal_sub_group_reduce(lane_var, plus<T> {});
}
template <typename T> floor_inline_always static T sub_group_reduce_min(T lane_var) {
	return metal_sub_group_reduce(lane_var, [](const auto& lhs, const auto& rhs) { return ::min(lhs, rhs); });
}
template <typename T> floor_inline_always static T sub_group_reduce_max(T lane_var) {
	return metal_sub_group_reduce(lane_var, [](const auto& lhs, const auto& rhs) { return ::max(lhs, rhs); });
}

template <bool is_exclusive, typename T, typename F> requires (is_same_v<T, int32_t> || is_same_v<T, uint32_t> || is_same_v<T, float>)
floor_inline_always static T metal_sub_group_scan(T lane_var, F&& op) {
	const auto lane_idx = get_sub_group_local_id();
	
	T shfled_var;
#pragma unroll
	for (uint32_t delta = 1u; delta <= (device_info::simd_width() / 2u); delta <<= 1u) {
		shfled_var = simd_shuffle_up(lane_var, delta);
		if (lane_idx >= delta) {
			lane_var = op(lane_var, shfled_var);
		}
	}
	
	if constexpr (is_exclusive) {
		// if this is an exclusive scan: shift one up
		const auto incl_result = lane_var;
		lane_var = simd_shuffle_up(incl_result, 1u);
		return (lane_idx == 0 ? T(0) : lane_var);
	} else {
		return lane_var;
	}
}

template <typename T> floor_inline_always static T sub_group_inclusive_scan_add(T lane_var) {
	return metal_sub_group_scan<false>(lane_var, plus<T> {});
}

template <typename T> floor_inline_always static T sub_group_exclusive_scan_add(T lane_var) {
	return metal_sub_group_scan<true>(lane_var, plus<T> {});
}
#endif

// specialize for all supported operations
template <> struct supports<ALGORITHM::SUB_GROUP_REDUCE, OP::ADD, uint32_t> : public std::true_type {};
template <> struct supports<ALGORITHM::SUB_GROUP_REDUCE, OP::ADD, int32_t> : public std::true_type {};
template <> struct supports<ALGORITHM::SUB_GROUP_REDUCE, OP::ADD, float> : public std::true_type {};
template <OP op, typename data_type>
requires(op == OP::ADD)
static auto sub_group_reduce(const data_type& input_value) {
	return sub_group_reduce_add(input_value);
}

template <> struct supports<ALGORITHM::SUB_GROUP_REDUCE, OP::MIN, uint32_t> : public std::true_type {};
template <> struct supports<ALGORITHM::SUB_GROUP_REDUCE, OP::MIN, int32_t> : public std::true_type {};
template <> struct supports<ALGORITHM::SUB_GROUP_REDUCE, OP::MIN, float> : public std::true_type {};
template <OP op, typename data_type>
requires(op == OP::MIN)
static auto sub_group_reduce(const data_type& input_value) {
	return sub_group_reduce_min(input_value);
}

template <> struct supports<ALGORITHM::SUB_GROUP_REDUCE, OP::MAX, uint32_t> : public std::true_type {};
template <> struct supports<ALGORITHM::SUB_GROUP_REDUCE, OP::MAX, int32_t> : public std::true_type {};
template <> struct supports<ALGORITHM::SUB_GROUP_REDUCE, OP::MAX, float> : public std::true_type {};
template <OP op, typename data_type>
requires(op == OP::MAX)
static auto sub_group_reduce(const data_type& input_value) {
	return sub_group_reduce_max(input_value);
}

template <> struct supports<ALGORITHM::SUB_GROUP_INCLUSIVE_SCAN, OP::ADD, uint32_t> : public std::true_type {};
template <> struct supports<ALGORITHM::SUB_GROUP_INCLUSIVE_SCAN, OP::ADD, int32_t> : public std::true_type {};
template <> struct supports<ALGORITHM::SUB_GROUP_INCLUSIVE_SCAN, OP::ADD, float> : public std::true_type {};
template <OP op, typename data_type>
requires(op == OP::ADD)
static auto sub_group_inclusive_scan(const data_type& input_value) {
	return sub_group_inclusive_scan_add(input_value);
}

template <> struct supports<ALGORITHM::SUB_GROUP_EXCLUSIVE_SCAN, OP::ADD, uint32_t> : public std::true_type {};
template <> struct supports<ALGORITHM::SUB_GROUP_EXCLUSIVE_SCAN, OP::ADD, int32_t> : public std::true_type {};
template <> struct supports<ALGORITHM::SUB_GROUP_EXCLUSIVE_SCAN, OP::ADD, float> : public std::true_type {};
template <OP op, typename data_type>
requires(op == OP::ADD)
static auto sub_group_exclusive_scan(const data_type& input_value) {
	return sub_group_exclusive_scan_add(input_value);
}

} // namespace compute_algorithm::group
#endif

#endif

#endif
