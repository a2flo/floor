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

// NOTE: this is a (partial) counterpart to const_math for math functions that are non-standard and
//       faster at run-time when using run-time math functions instead of doing cumbersome constexpr math

// misc c++ headers
#include <type_traits>
#include <utility>
#include <limits>
#include <algorithm>
#if !defined(FLOOR_COMPUTE) || (defined(FLOOR_COMPUTE_HOST) && !defined(FLOOR_COMPUTE_HOST_DEVICE))
#include <cmath>
#include <cstdint>
#include <cstdlib>
#if !defined(_MSC_VER)
#include <unistd.h>
#endif
#endif
#include <floor/core/essentials.hpp>
#include <floor/constexpr/soft_f16.hpp>
#include <floor/math/constants.hpp>
using namespace std;

namespace rt_math {
	//! computes min(x, y), returning x if x <= y, else y
	template <typename rt_type>
	static floor_inline_always rt_type min(const rt_type& a, const rt_type& b) {
#if !defined(FLOOR_COMPUTE) || defined(FLOOR_COMPUTE_HOST)
		return std::min(a, b);
#else
		if constexpr(is_same_v<rt_type, int8_t> ||
					 is_same_v<rt_type, int16_t> ||
					 is_same_v<rt_type, int32_t> ||
					 is_same_v<rt_type, int64_t> ||
					 is_same_v<rt_type, uint8_t> ||
					 is_same_v<rt_type, uint16_t> ||
					 is_same_v<rt_type, uint32_t> ||
					 is_same_v<rt_type, uint64_t> ||
					 is_same_v<rt_type, float>
#if !defined(FLOOR_COMPUTE_NO_DOUBLE)
					 || is_same_v<rt_type, double>
#endif
#if defined(FLOOR_COMPUTE_METAL) || defined(FLOOR_COMPUTE_VULKAN) || defined(FLOOR_COMPUTE_CUDA) || defined(FLOOR_COMPUTE_OPENCL)
					 || is_same_v<rt_type, half>
#endif
					 ) {
			return floor_rt_min(a, b);
		}
		else {
			return std::min(a, b);
		}
#endif
	}
	
	//! computes max(x, y), returning x if x >= y, else y
	template <typename rt_type>
	static floor_inline_always rt_type max(const rt_type& a, const rt_type& b) {
#if !defined(FLOOR_COMPUTE) || defined(FLOOR_COMPUTE_HOST)
		return std::max(a, b);
#else
		if constexpr(is_same_v<rt_type, int8_t> ||
					 is_same_v<rt_type, int16_t> ||
					 is_same_v<rt_type, int32_t> ||
					 is_same_v<rt_type, int64_t> ||
					 is_same_v<rt_type, uint8_t> ||
					 is_same_v<rt_type, uint16_t> ||
					 is_same_v<rt_type, uint32_t> ||
					 is_same_v<rt_type, uint64_t> ||
					 is_same_v<rt_type, float>
#if !defined(FLOOR_COMPUTE_NO_DOUBLE)
					 || is_same_v<rt_type, double>
#endif
#if defined(FLOOR_COMPUTE_METAL) || defined(FLOOR_COMPUTE_VULKAN) || defined(FLOOR_COMPUTE_CUDA) || defined(FLOOR_COMPUTE_OPENCL)
					 || is_same_v<rt_type, half>
#endif
					 ) {
			return floor_rt_max(a, b);
		}
		else {
			return std::max(a, b);
		}
#endif
	}
	
	//! clamps val to the range [min, max]
	template <typename arithmetic_type> requires(ext::is_arithmetic_v<arithmetic_type>)
	static floor_inline_always constexpr arithmetic_type clamp(const arithmetic_type& val,
															   const arithmetic_type& min_,
															   const arithmetic_type& max_) {
		return min(max(val, min_), max_);
	}
	
	//! clamps val to the range [0, max]
	template <typename arithmetic_type> requires(ext::is_arithmetic_v<arithmetic_type>)
	static floor_inline_always constexpr arithmetic_type clamp(const arithmetic_type& val,
															   const arithmetic_type& max_) {
		return min(max(val, (arithmetic_type)0), max_);
	}
	
	//! wraps val to the range [0, max]
	template <typename fp_type> requires(ext::is_floating_point_v<fp_type>)
	static floor_inline_always fp_type wrap(const fp_type& val, const fp_type& max) {
		return (val < (fp_type)0 ? (max - fmod(abs(val), max)) : fmod(val, max));
	}
	
	//! wraps val to the range [0, max]
	template <typename int_type>
	requires(ext::is_integral_v<int_type> &&
			 ext::is_signed_v<int_type> &&
			 !is_same_v<int_type, __int128_t>)
	static floor_inline_always constexpr int_type wrap(const int_type& val, const int_type& max) {
#if !defined(FLOOR_COMPUTE) || defined(FLOOR_COMPUTE_HOST) // needed for libstdc++
		using cast_int_type = conditional_t<sizeof(int_type) < 4, int32_t, int_type>;
#else // still want native 8-bit/16-bit abs calls on compute platforms
		using cast_int_type = int_type;
#endif
		return (val < (int_type)0 ? (max - (std::abs(cast_int_type(val)) % max)) : (val % max));
	}
	
	//! wraps val to the range [0, max]
	template <typename int_type> requires(is_same_v<int_type, __int128_t>)
	static floor_inline_always constexpr int_type wrap(const int_type& val, const int_type& max) {
		// no std::abs or __builtin_abs for __int128_t
		return (val < (int_type)0 ? (max - ((val < (int_type)0 ? -val : val) % max)) : (val % max));
	}
	
	//! wraps val to the range [0, max]
	template <typename uint_type> requires(ext::is_integral_v<uint_type> && ext::is_unsigned_v<uint_type>)
	static floor_inline_always constexpr uint_type wrap(const uint_type& val, const uint_type& max) {
		return (val % max);
	}
	
	//! signed wrapping of val to the range [-max, max]
	template <typename fp_type> requires(ext::is_floating_point_v<fp_type>)
	static floor_inline_always fp_type swrap(const fp_type val, const fp_type max) {
		return rt_math::wrap(val + max, fp_type(2) * max) - max;
	}
	
	//! signed wrapping of val to the range [-max, max]
	template <typename int_type> requires (ext::is_integral_v<int_type> && ext::is_signed_v<int_type>)
	static floor_inline_always int_type swrap(const int_type val, const int_type max) {
		return int_type(rt_math::wrap(val + max, int_type(2) * max)) - max;
	}

	//! wraps val to the range [0, max]
	//! NOTE: only exists for completeness, no actual purposes
	template <typename uint_type> requires(ext::is_integral_v<uint_type> && ext::is_unsigned_v<uint_type>)
	static floor_inline_always uint_type swrap(const uint_type val, const uint_type max) {
		return (val % max);
	}
	
	//! mirrored/alternating wrapping of val to the range [0, max],
	//! i.e. creating a triangle/zigzag signal from a linear input
	template <typename fp_type> requires(ext::is_floating_point_v<fp_type>)
	static floor_inline_always fp_type mwrap(const fp_type val, const fp_type max) {
		return abs(rt_math::swrap(val, max));
	}
	
	//! mirrored/alternating wrapping of val to the range [0, max],
	//! i.e. creating a triangle/zigzag signal from a linear input
	template <typename int_type>
	requires(ext::is_integral_v<int_type> &&
			 ext::is_signed_v<int_type> &&
			 !is_same_v<int_type, __int128_t>)
	static floor_inline_always int_type mwrap(const int_type val, const int_type max) {
#if !defined(FLOOR_COMPUTE) || defined(FLOOR_COMPUTE_HOST) // needed for libstdc++
		using cast_int_type = conditional_t<sizeof(int_type) < 4, int32_t, int_type>;
#else // still want native 8-bit/16-bit abs calls on compute platforms
		using cast_int_type = int_type;
#endif
		return int_type(std::abs(cast_int_type(rt_math::swrap(val, max))));
	}
	
	//! wraps val to the range [0, max]
	template <typename int_type> requires(is_same_v<int_type, __int128_t>)
	static floor_inline_always constexpr int_type mwrap(const int_type& val, const int_type& max) {
		// no std::abs or __builtin_abs for __int128_t
		const auto swrap_val = rt_math::swrap(val, max);
		return (swrap_val < (int_type)0 ? -swrap_val : swrap_val);
	}

	//! wraps val to the range [0, max]
	//! NOTE: only exists for completeness, no actual purposes
	template <typename uint_type> requires(ext::is_integral_v<uint_type> && ext::is_unsigned_v<uint_type>)
	static floor_inline_always uint_type mwrap(const uint_type val, const uint_type max) {
		return (val % max);
	}
	
	//! signed mirrored/alternating wrapping of val to the range [-max, max],
	//! i.e. creating a triangle/zigzag signal from a linear input
	template <typename fp_type> requires(ext::is_floating_point_v<fp_type>)
	static floor_inline_always fp_type mswrap(const fp_type val, const fp_type max) {
		const auto val_sign = (val < fp_type(0) ? fp_type(-1) : fp_type(1));
		const auto sign = fp_type(2) * floor(fmod((val_sign * val + fp_type(3) * max) / (fp_type(2) * max), fp_type(2))) - fp_type(1);
		return val_sign * sign * (fmod(val_sign * val + max, fp_type(2) * max) - max);
	}
	
	//! signed mirrored/alternating wrapping of val to the range [-max, max],
	//! i.e. creating a triangle/zigzag signal from a linear input
	template <typename int_type> requires (ext::is_integral_v<int_type> && ext::is_signed_v<int_type>)
	static floor_inline_always int_type mswrap(const int_type val, const int_type max) {
		const auto val_sign = (val < int_type(0) ? int_type(-1) : int_type(1));
		const auto sign = int_type(2) * (((val_sign * val + int_type(3) * max) / (int_type(2) * max)) % int_type(2)) - int_type(1);
		return val_sign * int_type(sign * (((val_sign * val + max) % (int_type(2) * max)) - max));
	}

	//! wraps val to the range [0, max]
	//! NOTE: only exists for completeness, no actual purposes
	template <typename uint_type> requires(ext::is_integral_v<uint_type> && ext::is_unsigned_v<uint_type>)
	static floor_inline_always uint_type mswrap(const uint_type val, const uint_type max) {
		return (val % max);
	}
	
	//! returns the fractional part of val
	template <typename fp_type> requires(ext::is_floating_point_v<fp_type>)
	static floor_inline_always fp_type fractional(const fp_type& val) {
		return (val - trunc(val));
	}
	
	//! count leading zeros
	template <typename uint_type> requires(is_same_v<uint_type, uint16_t>)
	static floor_inline_always int32_t clz(const uint_type& val) {
#if !defined(FLOOR_COMPUTE) || defined(FLOOR_COMPUTE_HOST)
		return (val != 0u ? __builtin_clzs(val) : 16u);
#else
		return floor_rt_clz(val);
#endif
	}
	template <typename uint_type> requires(is_same_v<uint_type, uint32_t>)
	static floor_inline_always int32_t clz(const uint_type& val) {
#if !defined(FLOOR_COMPUTE) || defined(FLOOR_COMPUTE_HOST)
		return (val != 0u ? __builtin_clz(val) : 32u);
#else
		return floor_rt_clz(val);
#endif
	}
	template <typename uint_type> requires(is_same_v<uint_type, uint64_t>)
	static floor_inline_always int32_t clz(const uint_type& val) {
#if !defined(FLOOR_COMPUTE) || defined(FLOOR_COMPUTE_HOST)
		return (val != 0u ? __builtin_clzll(val) : 64u);
#else
		return floor_rt_clz(val);
#endif
	}
	template <typename any_type> requires(is_same_v<any_type, bool>)
	static floor_inline_always int32_t clz(const any_type& val) {
		return val ? 0 : 1;
	}
	template <typename any_type> requires(!is_same_v<any_type, bool> && sizeof(any_type) == 1)
	static floor_inline_always int32_t clz(const any_type& val) {
		const uint16_t widened_val = *(const uint8_t*)&val;
		return rt_math::clz(widened_val) - 8 /* upper 8 bits */;
	}
	template <typename any_type> requires(!is_same_v<any_type, uint16_t> && sizeof(any_type) == 2)
	static floor_inline_always int32_t clz(const any_type& val) {
		return rt_math::clz(*(const uint16_t*)&val);
	}
	template <typename any_type> requires(!is_same_v<any_type, uint32_t> && sizeof(any_type) == 4)
	static floor_inline_always int32_t clz(const any_type& val) {
		return rt_math::clz(*(const uint32_t*)&val);
	}
	template <typename any_type> requires(!is_same_v<any_type, uint64_t> && sizeof(any_type) == 8)
	static floor_inline_always int32_t clz(const any_type& val) {
		return rt_math::clz(*(const uint64_t*)&val);
	}
#if !defined(FLOOR_COMPUTE) || defined(FLOOR_COMPUTE_HOST)
	template <typename any_type> requires(is_same_v<any_type, __uint128_t> || is_same_v<any_type, __int128_t> || sizeof(any_type) == 16)
	static floor_inline_always int32_t clz(const any_type& val) {
		union {
			__uint128_t ui128;
			any_type val;
		} conv { .val = val };
		const auto upper = uint64_t(conv.ui128 >> __uint128_t(64));
		const auto lower = uint64_t(conv.ui128 & __uint128_t(~0ull));
		const auto clz_upper = clz(upper);
		const auto clz_lower = clz(lower);
		return (clz_upper < 64 ? clz_upper : (clz_upper + clz_lower));
	}
#endif
	
	//! count trailing zeros
	template <typename uint_type> requires(is_same_v<uint_type, uint16_t>)
	static floor_inline_always int32_t ctz(const uint_type& val) {
#if !defined(FLOOR_COMPUTE) || defined(FLOOR_COMPUTE_HOST)
		return (val != 0u ? __builtin_ctzs(val) : 16u);
#else
		return floor_rt_ctz(val);
#endif
	}
	template <typename uint_type> requires(is_same_v<uint_type, uint32_t>)
	static floor_inline_always int32_t ctz(const uint_type& val) {
#if !defined(FLOOR_COMPUTE) || defined(FLOOR_COMPUTE_HOST)
		return (val != 0u ? __builtin_ctz(val) : 32u);
#else
		return floor_rt_ctz(val);
#endif
	}
	template <typename uint_type> requires(is_same_v<uint_type, uint64_t>)
	static floor_inline_always int32_t ctz(const uint_type& val) {
#if !defined(FLOOR_COMPUTE) || defined(FLOOR_COMPUTE_HOST)
		return (val != 0u ? __builtin_ctzll(val) : 64u);
#else
		return floor_rt_ctz(val);
#endif
	}
	template <typename any_type> requires(is_same_v<any_type, bool>)
	static floor_inline_always int32_t ctz(const any_type& val) {
		return val ? 0 : 1;
	}
	template <typename any_type> requires(!is_same_v<any_type, bool> && sizeof(any_type) == 1)
	static floor_inline_always int32_t ctz(const any_type& val) {
		const uint16_t widened_val = 0xFFu | *(const uint8_t*)&val;
		return rt_math::ctz(widened_val);
	}
	template <typename any_type> requires(!is_same_v<any_type, uint16_t> && sizeof(any_type) == 2)
	static floor_inline_always int32_t ctz(const any_type& val) {
		return rt_math::ctz(*(const uint16_t*)&val);
	}
	template <typename any_type> requires(!is_same_v<any_type, uint32_t> && sizeof(any_type) == 4)
	static floor_inline_always int32_t ctz(const any_type& val) {
		return rt_math::ctz(*(const uint32_t*)&val);
	}
	template <typename any_type> requires(!is_same_v<any_type, uint64_t> && sizeof(any_type) == 8)
	static floor_inline_always int32_t ctz(const any_type& val) {
		return rt_math::ctz(*(const uint64_t*)&val);
	}
#if !defined(FLOOR_COMPUTE) || defined(FLOOR_COMPUTE_HOST)
	template <typename any_type> requires(is_same_v<any_type, __uint128_t> || is_same_v<any_type, __int128_t> || sizeof(any_type) == 16)
	static floor_inline_always int32_t ctz(const any_type& val) {
		union {
			__uint128_t ui128;
			any_type val;
		} conv { .val = val };
		const auto upper = uint64_t(conv.ui128 >> __uint128_t(64));
		const auto lower = uint64_t(conv.ui128 & __uint128_t(~0ull));
		const auto ctz_upper = ctz(upper);
		const auto ctz_lower = ctz(lower);
		return (ctz_lower < 64 ? ctz_lower : (ctz_upper + ctz_lower));
	}
#endif
	
	//! count 1-bits
	template <typename uint_type> requires(is_same_v<uint_type, uint16_t>)
	static floor_inline_always int32_t popcount(const uint_type& val) {
#if !defined(FLOOR_COMPUTE) || defined(FLOOR_COMPUTE_HOST)
		return __builtin_popcount((uint32_t)val);
#else
		return floor_rt_popcount(val);
#endif
	}
	template <typename uint_type> requires(is_same_v<uint_type, uint32_t>)
	static floor_inline_always int32_t popcount(const uint_type& val) {
#if !defined(FLOOR_COMPUTE) || defined(FLOOR_COMPUTE_HOST)
		return __builtin_popcount(val);
#else
		return floor_rt_popcount(val);
#endif
	}
	template <typename uint_type> requires(is_same_v<uint_type, uint64_t>)
	static floor_inline_always int32_t popcount(const uint_type& val) {
#if !defined(FLOOR_COMPUTE) || defined(FLOOR_COMPUTE_HOST)
		return __builtin_popcountll(val);
#else
		return floor_rt_popcount(val);
#endif
	}
	template <typename any_type> requires(is_same_v<any_type, bool>)
	static floor_inline_always int32_t popcount(const any_type& val) {
		return val ? 1 : 0;
	}
	template <typename any_type> requires(!is_same_v<any_type, bool> && sizeof(any_type) == 1)
	static floor_inline_always int32_t popcount(const any_type& val) {
		const uint16_t widened_val = *(const uint8_t*)&val;
		return rt_math::popcount(widened_val);
	}
	template <typename any_type> requires(!is_same_v<any_type, uint16_t> && sizeof(any_type) == 2)
	static floor_inline_always int32_t popcount(const any_type& val) {
		return rt_math::popcount(*(const uint16_t*)&val);
	}
	template <typename any_type> requires(!is_same_v<any_type, uint32_t> && sizeof(any_type) == 4)
	static floor_inline_always int32_t popcount(const any_type& val) {
		return rt_math::popcount(*(const uint32_t*)&val);
	}
	template <typename any_type> requires(!is_same_v<any_type, uint64_t> && sizeof(any_type) == 8)
	static floor_inline_always int32_t popcount(const any_type& val) {
		return rt_math::popcount(*(const uint64_t*)&val);
	}
#if !defined(FLOOR_COMPUTE) || defined(FLOOR_COMPUTE_HOST)
	template <typename any_type> requires(is_same_v<any_type, __uint128_t> || is_same_v<any_type, __int128_t> || sizeof(any_type) == 16)
	static floor_inline_always int32_t popcount(const any_type& val) {
		union {
			__uint128_t ui128;
			any_type val;
		} conv { .val = val };
		const auto upper = uint64_t(conv.ui128 >> __uint128_t(64));
		const auto lower = uint64_t(conv.ui128 & __uint128_t(~0ull));
		return popcount(upper) + popcount(lower);
	}
#endif
	
	//! find first set/one: ctz(x) + 1 if x != 0, 0 if x == 0
	template <typename any_type>
	static floor_inline_always int32_t ffs(const any_type& val) {
		return val != any_type(0) ? ctz(val) + 1 : 0;
	}
	
	//! parity: 1 if odd number of 1-bits set, 0 else
	template <typename any_type>
	static floor_inline_always int32_t parity(const any_type& val) {
		return popcount(val) & 1;
	}

	//! combines "low" and "high" to a 64-bit source uint (consisting of 8x 8-bit values), then uses the 4x 4-bit values
	//! of the lower 16-bit of "select" to select/recombine a 32-bit uint from these 8x 8-bit source values.
	//! the lower 3-bit of each 4-bit select value decide which source byte should be used, the upper 1-bit/msb decides
	//! whether the sign of the selected byte should be used instead (filling the full 8-bit, i.e. sign-extended)
	static floor_inline_always uint32_t permute(const uint32_t low, const uint32_t high, const uint32_t select) {
#if defined(FLOOR_COMPUTE_INFO_HAS_PERMUTE) && FLOOR_COMPUTE_INFO_HAS_PERMUTE == 1
		return floor_rt_permute(low, high, select);
#else
		// 64-bit source value (consisting of 8x 8-bit values we'll select from)
		const uint64_t src = (uint64_t(high) << 32ull) | uint64_t(low);
		
		// 4x 4-bit control nibbles
		uint32_t ctrl[] {
			select & 0xFu,
			(select >> 4u) & 0xFu,
			(select >> 8u) & 0xFu,
			(select >> 12u) & 0xFu
		};
		
		// if msb of control is set, selected byte is sign-extended
		const bool sext[] {
			ctrl[0] > 0x7u,
			ctrl[1] > 0x7u,
			ctrl[2] > 0x7u,
			ctrl[3] > 0x7u,
		};
		
		// mask msb
		ctrl[0] &= 0x7u;
		ctrl[1] &= 0x7u;
		ctrl[2] &= 0x7u;
		ctrl[3] &= 0x7u;
		
		// extract bytes
		uint32_t selected_bytes[] {
			uint32_t(src >> (ctrl[0] * 8u)) & 0xFFu,
			uint32_t(src >> (ctrl[1] * 8u)) & 0xFFu,
			uint32_t(src >> (ctrl[2] * 8u)) & 0xFFu,
			uint32_t(src >> (ctrl[3] * 8u)) & 0xFFu,
		};
		
		// replicate or sign-extend?
		selected_bytes[0] = (!sext[0] ? selected_bytes[0] : (selected_bytes[0] & 0x8u ? 0xFFu : 0u));
		selected_bytes[1] = (!sext[1] ? selected_bytes[1] : (selected_bytes[1] & 0x8u ? 0xFFu : 0u));
		selected_bytes[2] = (!sext[2] ? selected_bytes[2] : (selected_bytes[2] & 0x8u ? 0xFFu : 0u));
		selected_bytes[3] = (!sext[3] ? selected_bytes[3] : (selected_bytes[3] & 0x8u ? 0xFFu : 0u));
		
		// finally: combine processed bytes
		return selected_bytes[0] | (selected_bytes[1] << 8u) | (selected_bytes[2] << 16u) | (selected_bytes[3] << 24u);
#endif
	}
	
	//! combines "low" and "high" to a 64-bit uint, then shifts it left by "shift" amount of bits (modulo 32)
	static floor_inline_always uint32_t funnel_shift_left(const uint32_t low, const uint32_t high, const uint32_t shift) {
#if defined(FLOOR_COMPUTE_INFO_HAS_FUNNEL_SHIFT) && FLOOR_COMPUTE_INFO_HAS_FUNNEL_SHIFT == 1
		return floor_rt_funnel_shift_left(low, high, shift);
#else
		const uint32_t shift_amount = shift & 0x1Fu;
		return (high << shift_amount) | (low >> (32u - shift_amount));
#endif
	}
	
	//! combines "low" and "high" to a 64-bit uint, then shifts it right by "shift" amount of bits (modulo 32)
	static floor_inline_always uint32_t funnel_shift_right(const uint32_t low, const uint32_t high, const uint32_t shift) {
#if defined(FLOOR_COMPUTE_INFO_HAS_FUNNEL_SHIFT) && FLOOR_COMPUTE_INFO_HAS_FUNNEL_SHIFT == 1
		return floor_rt_funnel_shift_right(low, high, shift);
#else
		const uint32_t shift_amount = shift & 0x1Fu;
		return (high << (32u - shift_amount)) | (low >> shift_amount);
#endif
	}
	
	//! combines "low" and "high" to a 64-bit uint, then shifts it left by "shift" amount of bits (clamped by 32)
	static floor_inline_always uint32_t funnel_shift_clamp_left(const uint32_t low, const uint32_t high, const uint32_t shift) {
#if defined(FLOOR_COMPUTE_INFO_HAS_FUNNEL_SHIFT) && FLOOR_COMPUTE_INFO_HAS_FUNNEL_SHIFT == 1
		return floor_rt_funnel_shift_clamp_left(low, high, shift);
#else
		const uint32_t shift_amount = rt_math::min(shift, 32u);
		return (high << shift_amount) | (low >> (32u - shift_amount));
#endif
	}
	
	//! combines "low" and "high" to a 64-bit uint, then shifts it right by "shift" amount of bits (clamped by 32)
	static floor_inline_always uint32_t funnel_shift_clamp_right(const uint32_t low, const uint32_t high, const uint32_t shift) {
#if defined(FLOOR_COMPUTE_INFO_HAS_FUNNEL_SHIFT) && FLOOR_COMPUTE_INFO_HAS_FUNNEL_SHIFT == 1
		return floor_rt_funnel_shift_clamp_right(low, high, shift);
#else
		const uint32_t shift_amount = rt_math::min(shift, 32u);
		return (high << (32u - shift_amount)) | (low >> shift_amount);
#endif
	}
	
	//! finds the n-th set bit (specified by "offset") in "value", starting at bit #"base",
	//! returns ~0u if the n-th set bit is not found
	//! NOTE: base must be >= 0 and < 32
	static floor_inline_always uint32_t find_nth_set(const uint32_t value,
													 const uint32_t base,
													 const int32_t offset_) {
#if defined(FLOOR_COMPUTE_INFO_HAS_FIND_NTH_SET) && FLOOR_COMPUTE_INFO_HAS_FIND_NTH_SET == 1
		return floor_rt_find_nth_set(value, base, offset_);
#else
		static constexpr const uint32_t failure_ret { ~0u };
		if(offset_ == 0) {
			return ((value & (1u << base)) != 0u ? base : failure_ret);
		}
		else if(offset_ > 0) {
			auto offset = uint32_t(offset_);
			for(uint32_t cur_pos = base; cur_pos < 32; ++cur_pos) {
				if((value & (1u << cur_pos)) != 0u) {
					if(offset == 0u) {
						return cur_pos;
					}
					--offset;
				}
			}
		}
		else if(offset_ < 0) {
			auto offset = uint32_t(-offset_);
			for(int32_t cur_pos = int32_t(base); cur_pos >= 0; --cur_pos) {
				if((value & (1u << uint32_t(cur_pos))) != 0u) {
					if(offset == 0u) {
						return uint32_t(cur_pos);
					}
					--offset;
				}
			}
		}
		return failure_ret;
#endif
	}
	
	//! reverses the bits of the specified 32-bit or 64-bit input value
	template <typename any_type> requires(sizeof(any_type) == 4 || sizeof(any_type) == 8)
	static floor_inline_always any_type reverse_bits(const any_type& value) {
		if constexpr(sizeof(any_type) == 4) {
#if defined(FLOOR_COMPUTE_INFO_HAS_REVERSE_BITS_32) && FLOOR_COMPUTE_INFO_HAS_REVERSE_BITS_32 == 1
			return (any_type)floor_rt_reverse_bits(*(const uint32_t*)&value);
#else
			uint32_t u32_val = *(const uint32_t*)&value;
			u32_val = ((u32_val >> 1u) & 0x55555555u) | ((u32_val & 0x55555555u) << 1u);
			u32_val = ((u32_val >> 2u) & 0x33333333u) | ((u32_val & 0x33333333u) << 2u);
			u32_val = ((u32_val >> 4u) & 0x0F0F0F0Fu) | ((u32_val & 0x0F0F0F0Fu) << 4u);
			u32_val = ((u32_val >> 8u) & 0x00FF00FFu) | ((u32_val & 0x00FF00FFu) << 8u);
			u32_val = (u32_val >> 16u) | (u32_val << 16u);
			return *(any_type*)&u32_val;
#endif
		}
		else {
#if defined(FLOOR_COMPUTE_INFO_HAS_REVERSE_BITS_64) && FLOOR_COMPUTE_INFO_HAS_REVERSE_BITS_64 == 1
			return (any_type)floor_rt_reverse_bits(*(const uint64_t*)&value);
#else
			uint64_t u64_val = *(const uint64_t*)&value;
			u64_val = ((u64_val >> 1ull) & 0x5555555555555555ull) | ((u64_val & 0x5555555555555555ull) << 1ull);
			u64_val = ((u64_val >> 2ull) & 0x3333333333333333ull) | ((u64_val & 0x3333333333333333ull) << 2ull);
			u64_val = ((u64_val >> 4ull) & 0x0F0F0F0F0F0F0F0Full) | ((u64_val & 0x0F0F0F0F0F0F0F0Full) << 4ull);
			u64_val = ((u64_val >> 8ull) & 0x00FF00FF00FF00FFull) | ((u64_val & 0x00FF00FF00FF00FFull) << 8ull);
			u64_val = ((u64_val >> 16ull) & 0x0000FFFF0000FFFFull) | ((u64_val & 0x0000FFFF0000FFFFull) << 16ull);
			u64_val = (u64_val >> 32ull) | (u64_val << 32ull);
			return *(any_type*)&u64_val;
#endif
		}
	}

	//! returns 'a' with the sign of 'b', essentially "sign(b) * abs(a)"
	//! NOTE: fallback for integral types, floating point types are handled via std::copysign
	template <typename int_type> requires(ext::is_integral_v<int_type>)
	static floor_inline_always int_type copysign(const int_type a, const int_type b) {
		if constexpr (ext::is_signed_v<int_type>) {
			if constexpr (sizeof(int_type) <= 8) {
				const auto abs_a = int_type(std::abs(a));
				return (b < int_type(0) ? -abs_a : abs_a);
			} else { // we don't have std::abs for integers > 64-bit
				const auto abs_a = int_type(const_math::abs(a));
				return (b < int_type(0) ? -abs_a : abs_a);
			}
		} else {
			return a;
		}
	}

}

// -> "std::" s/w half/fp16 math functions (simply forward to float functions)
// (don't define for backends that have h/w support)
#if !defined(FLOOR_COMPUTE_OPENCL) && !defined(FLOOR_COMPUTE_METAL) && !defined(FLOOR_COMPUTE_VULKAN) && !defined(FLOOR_COMPUTE_HOST_DEVICE)
#define FLOOR_HALF_SW_FUNC_1(func) static floor_inline_always auto func(half x) { return (half)std::func(float(x)); }
#define FLOOR_HALF_SW_FUNC_2(func) static floor_inline_always auto func(half x, half y) { return (half)std::func(float(x), float(y)); }
#define FLOOR_HALF_SW_FUNC_3(func) static floor_inline_always auto func(half x, half y, half z) { return (half)std::func(float(x), float(y), float(z)); }
FLOOR_HALF_SW_FUNC_2(fmod)
FLOOR_HALF_SW_FUNC_1(sqrt)
FLOOR_HALF_SW_FUNC_1(abs)
FLOOR_HALF_SW_FUNC_1(floor)
FLOOR_HALF_SW_FUNC_1(ceil)
FLOOR_HALF_SW_FUNC_1(round)
FLOOR_HALF_SW_FUNC_1(trunc)
FLOOR_HALF_SW_FUNC_1(rint)
FLOOR_HALF_SW_FUNC_1(sin)
FLOOR_HALF_SW_FUNC_1(cos)
FLOOR_HALF_SW_FUNC_1(tan)
FLOOR_HALF_SW_FUNC_1(asin)
FLOOR_HALF_SW_FUNC_1(acos)
FLOOR_HALF_SW_FUNC_1(atan)
FLOOR_HALF_SW_FUNC_2(atan2)
FLOOR_HALF_SW_FUNC_1(sinh)
FLOOR_HALF_SW_FUNC_1(cosh)
FLOOR_HALF_SW_FUNC_1(tanh)
FLOOR_HALF_SW_FUNC_1(asinh)
FLOOR_HALF_SW_FUNC_1(acosh)
FLOOR_HALF_SW_FUNC_1(atanh)
FLOOR_HALF_SW_FUNC_1(exp)
FLOOR_HALF_SW_FUNC_1(exp2)
FLOOR_HALF_SW_FUNC_1(log)
FLOOR_HALF_SW_FUNC_1(log2)
FLOOR_HALF_SW_FUNC_2(pow)
FLOOR_HALF_SW_FUNC_2(copysign)
#undef FLOOR_HALF_SW_FUNC_1
#undef FLOOR_HALF_SW_FUNC_2
#undef FLOOR_HALF_SW_FUNC_3
#endif
