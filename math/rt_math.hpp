/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2016 Florian Ziesche
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

#ifndef __FLOOR_RT_MATH_HPP__
#define __FLOOR_RT_MATH_HPP__

// NOTE: this is a (partial) counterpart to const_math for math functions that are non-standard and
//       faster at run-time when using run-time math functions instead of doing cumbersome constexpr math

// misc c++ headers
#include <type_traits>
#include <utility>
#include <limits>
#include <algorithm>
#if !defined(FLOOR_COMPUTE) || defined(FLOOR_COMPUTE_HOST)
#include <cmath>
#include <cstdint>
#include <cstdlib>
#if !defined(_MSC_VER)
#include <unistd.h>
#endif
#endif
#include <floor/core/essentials.hpp>
#include <floor/constexpr/soft_i128.hpp>
#include <floor/math/constants.hpp>
using namespace std;

namespace rt_math {
	//! computes min(x, y), returning x if x <= y, else y
	template <typename rt_type>
	static floor_inline_always rt_type min(const rt_type& a, const rt_type& b) {
#if !defined(FLOOR_COMPUTE) || defined(FLOOR_COMPUTE_HOST)
		return std::min(a, b);
#else
#if defined(FLOOR_CXX17)
		if constexpr(is_same<rt_type, int8_t>() ||
					 is_same<rt_type, int16_t>() ||
					 is_same<rt_type, int32_t>() ||
					 is_same<rt_type, int64_t>() ||
					 is_same<rt_type, uint8_t>() ||
					 is_same<rt_type, uint16_t>() ||
					 is_same<rt_type, uint32_t>() ||
					 is_same<rt_type, uint64_t>() ||
					 is_same<rt_type, float>()
#if !defined(FLOOR_COMPUTE_NO_DOUBLE)
					 || is_same<rt_type, double>()
#endif
#if defined(FLOOR_COMPUTE_METAL)
					 || is_same<rt_type, half>()
#endif
					 ) {
			return floor_rt_min(a, b);
		}
		else {
			return std::min(a, b));;
		}
#else
		return __builtin_choose_expr((is_same<rt_type, int8_t>() ||
									  is_same<rt_type, int16_t>() ||
									  is_same<rt_type, int32_t>() ||
									  is_same<rt_type, int64_t>() ||
									  is_same<rt_type, uint8_t>() ||
									  is_same<rt_type, uint16_t>() ||
									  is_same<rt_type, uint32_t>() ||
									  is_same<rt_type, uint64_t>() ||
									  is_same<rt_type, float>()
#if !defined(FLOOR_COMPUTE_NO_DOUBLE)
									  || is_same<rt_type, double>()
#endif
#if defined(FLOOR_COMPUTE_METAL)
									  || is_same<rt_type, half>()
#endif
									  ),
									 floor_rt_min(a, b), std::min(a, b));
#endif
#endif
	}
	
	//! computes max(x, y), returning x if x >= y, else y
	template <typename rt_type>
	static floor_inline_always rt_type max(const rt_type& a, const rt_type& b) {
#if !defined(FLOOR_COMPUTE) || defined(FLOOR_COMPUTE_HOST)
		return std::max(a, b);
#else
#if defined(FLOOR_CXX17)
		if constexpr(is_same<rt_type, int8_t>() ||
					 is_same<rt_type, int16_t>() ||
					 is_same<rt_type, int32_t>() ||
					 is_same<rt_type, int64_t>() ||
					 is_same<rt_type, uint8_t>() ||
					 is_same<rt_type, uint16_t>() ||
					 is_same<rt_type, uint32_t>() ||
					 is_same<rt_type, uint64_t>() ||
					 is_same<rt_type, float>()
#if !defined(FLOOR_COMPUTE_NO_DOUBLE)
					 || is_same<rt_type, double>()
#endif
#if defined(FLOOR_COMPUTE_METAL)
					 || is_same<rt_type, half>()
#endif
					 ) {
			return floor_rt_max(a, b);
		}
		else {
			return std::max(a, b));;
		}
#else
		return __builtin_choose_expr((is_same<rt_type, int8_t>() ||
									  is_same<rt_type, int16_t>() ||
									  is_same<rt_type, int32_t>() ||
									  is_same<rt_type, int64_t>() ||
									  is_same<rt_type, uint8_t>() ||
									  is_same<rt_type, uint16_t>() ||
									  is_same<rt_type, uint32_t>() ||
									  is_same<rt_type, uint64_t>() ||
									  is_same<rt_type, float>()
#if !defined(FLOOR_COMPUTE_NO_DOUBLE)
									  || is_same<rt_type, double>()
#endif
#if defined(FLOOR_COMPUTE_METAL)
									  || is_same<rt_type, half>()
#endif
									  ),
									 floor_rt_max(a, b), std::max(a, b));
#endif
#endif
	}
	
	//! clamps val to the range [min, max]
	template <typename arithmetic_type, enable_if_t<(is_arithmetic<arithmetic_type>() ||
													 is_same<arithmetic_type, __int128_t>() ||
													 is_same<arithmetic_type, __uint128_t>())>* = nullptr>
	static floor_inline_always constexpr arithmetic_type clamp(const arithmetic_type& val,
															   const arithmetic_type& min_,
															   const arithmetic_type& max_) {
		return min(max(val, min_), max_);
	}
	
	//! clamps val to the range [0, max]
	template <typename arithmetic_type, enable_if_t<(is_arithmetic<arithmetic_type>() ||
													 is_same<arithmetic_type, __int128_t>() ||
													 is_same<arithmetic_type, __uint128_t>())>* = nullptr>
	static floor_inline_always constexpr arithmetic_type clamp(const arithmetic_type& val,
															   const arithmetic_type& max_) {
		return min(max(val, (arithmetic_type)0), max_);
	}
	
	//! wraps val to the range [0, max]
	template <typename fp_type, enable_if_t<(is_floating_point<fp_type>())>* = nullptr>
	static floor_inline_always fp_type wrap(const fp_type& val, const fp_type& max) {
		return (val < (fp_type)0 ? (max - fmod(abs(val), max)) : fmod(val, max));
	}
	
	//! wraps val to the range [0, max]
	template <typename int_type, enable_if_t<((is_integral<int_type>() &&
											   is_signed<int_type>()) &&
											  !is_same<int_type, __int128_t>())>* = nullptr>
	static floor_inline_always constexpr int_type wrap(const int_type& val, const int_type& max) {
#if !defined(FLOOR_COMPUTE) || defined(FLOOR_COMPUTE_HOST) // needed for libstdc++
		typedef conditional_t<sizeof(int_type) < 4, int32_t, int_type> cast_int_type;
#else // still want native 8-bit/16-bit abs calls on compute platforms
		typedef int_type cast_int_type;
#endif
		return (val < (int_type)0 ? (max - (std::abs(cast_int_type(val)) % max)) : (val % max));
	}
	
	//! wraps val to the range [0, max]
	template <typename uint_type, enable_if_t<((is_integral<uint_type>() &&
												is_unsigned<uint_type>()) ||
											   is_same<uint_type, __uint128_t>())>* = nullptr>
	static floor_inline_always constexpr uint_type wrap(const uint_type& val, const uint_type& max) {
		return (val % max);
	}
	
	//! wraps val to the range [0, max]
	template <typename int_type, enable_if_t<(is_same<int_type, __int128_t>())>* = nullptr>
	static floor_inline_always constexpr int_type wrap(const int_type& val, const int_type& max) {
		// no std::abs or __builtin_abs for __int128_t
		return (val < (int_type)0 ? (max - ((val < (int_type)0 ? -val : val) % max)) : (val % max));
	}
	
	//! returns the fractional part of val
	template <typename fp_type, enable_if_t<(is_floating_point<fp_type>())>* = nullptr>
	static floor_inline_always fp_type fractional(const fp_type& val) {
		return (val - trunc(val));
	}
	
	//! count leading zeros
	template <typename uint_type, enable_if_t<(is_same<uint_type, uint16_t>())>* = nullptr>
	static floor_inline_always int32_t clz(const uint_type& val) {
#if !defined(FLOOR_COMPUTE) || defined(FLOOR_COMPUTE_HOST)
		return __builtin_clzs(val);
#else
		return floor_rt_clz(val);
#endif
	}
	template <typename uint_type, enable_if_t<(is_same<uint_type, uint32_t>())>* = nullptr>
	static floor_inline_always int32_t clz(const uint_type& val) {
#if !defined(FLOOR_COMPUTE) || defined(FLOOR_COMPUTE_HOST)
		return __builtin_clz(val);
#else
		return floor_rt_clz(val);
#endif
	}
	template <typename uint_type, enable_if_t<(is_same<uint_type, uint64_t>())>* = nullptr>
	static floor_inline_always int32_t clz(const uint_type& val) {
#if !defined(FLOOR_COMPUTE) || defined(FLOOR_COMPUTE_HOST)
		return __builtin_clzll(val);
#else
		return floor_rt_clz(val);
#endif
	}
	template <typename any_type, enable_if_t<(is_same<any_type, bool>())>* = nullptr>
	static floor_inline_always int32_t clz(const any_type& val) {
		return val ? 0 : 1;
	}
	template <typename any_type, enable_if_t<(!is_same<any_type, bool>() && sizeof(any_type) == 1)>* = nullptr>
	static floor_inline_always int32_t clz(const any_type& val) {
		const uint16_t widened_val = *(uint8_t*)&val;
		return rt_math::clz(widened_val) - 8 /* upper 8 bits */;
	}
	template <typename any_type, enable_if_t<(!is_same<any_type, uint16_t>() && sizeof(any_type) == 2)>* = nullptr>
	static floor_inline_always int32_t clz(const any_type& val) {
		return rt_math::clz(*(uint16_t*)&val);
	}
	template <typename any_type, enable_if_t<(!is_same<any_type, uint32_t>() && sizeof(any_type) == 4)>* = nullptr>
	static floor_inline_always int32_t clz(const any_type& val) {
		return rt_math::clz(*(uint32_t*)&val);
	}
	template <typename any_type, enable_if_t<(!is_same<any_type, uint64_t>() && sizeof(any_type) == 8)>* = nullptr>
	static floor_inline_always int32_t clz(const any_type& val) {
		return rt_math::clz(*(uint64_t*)&val);
	}
#if !defined(FLOOR_COMPUTE) || defined(FLOOR_COMPUTE_HOST)
	template <typename any_type, enable_if_t<(is_same<any_type, __uint128_t>() ||
											  is_same<any_type, __int128_t>() ||
											  sizeof(any_type) == 16)>* = nullptr>
	static floor_inline_always int32_t clz(const any_type& val) {
		const auto upper = uint64_t((*(__uint128_t*)&val) >> __uint128_t(64));
		const auto lower = uint64_t((*(__uint128_t*)&val) & __uint128_t(0xFFFF'FFFF'FFFF'FFFFull));
		const auto clz_upper = clz(upper);
		const auto clz_lower = clz(lower);
		return (clz_upper < 64 ? clz_upper : (clz_upper + clz_lower));
	}
#endif
	
	//! count trailing zeros
	template <typename uint_type, enable_if_t<(is_same<uint_type, uint16_t>())>* = nullptr>
	static floor_inline_always int32_t ctz(const uint_type& val) {
#if !defined(FLOOR_COMPUTE) || defined(FLOOR_COMPUTE_HOST)
		return __builtin_ctzs(val);
#else
		return floor_rt_ctz(val);
#endif
	}
	template <typename uint_type, enable_if_t<(is_same<uint_type, uint32_t>())>* = nullptr>
	static floor_inline_always int32_t ctz(const uint_type& val) {
#if !defined(FLOOR_COMPUTE) || defined(FLOOR_COMPUTE_HOST)
		return __builtin_ctz(val);
#else
		return floor_rt_ctz(val);
#endif
	}
	template <typename uint_type, enable_if_t<(is_same<uint_type, uint64_t>())>* = nullptr>
	static floor_inline_always int32_t ctz(const uint_type& val) {
#if !defined(FLOOR_COMPUTE) || defined(FLOOR_COMPUTE_HOST)
		return __builtin_ctzll(val);
#else
		return floor_rt_ctz(val);
#endif
	}
	template <typename any_type, enable_if_t<(is_same<any_type, bool>())>* = nullptr>
	static floor_inline_always int32_t ctz(const any_type& val) {
		return val ? 0 : 1;
	}
	template <typename any_type, enable_if_t<(!is_same<any_type, bool>() && sizeof(any_type) == 1)>* = nullptr>
	static floor_inline_always int32_t ctz(const any_type& val) {
		const uint16_t widened_val = 0xFFu | *(uint8_t*)&val;
		return rt_math::ctz(widened_val);
	}
	template <typename any_type, enable_if_t<(!is_same<any_type, uint16_t>() && sizeof(any_type) == 2)>* = nullptr>
	static floor_inline_always int32_t ctz(const any_type& val) {
		return rt_math::ctz(*(uint16_t*)&val);
	}
	template <typename any_type, enable_if_t<(!is_same<any_type, uint32_t>() && sizeof(any_type) == 4)>* = nullptr>
	static floor_inline_always int32_t ctz(const any_type& val) {
		return rt_math::ctz(*(uint32_t*)&val);
	}
	template <typename any_type, enable_if_t<(!is_same<any_type, uint64_t>() && sizeof(any_type) == 8)>* = nullptr>
	static floor_inline_always int32_t ctz(const any_type& val) {
		return rt_math::ctz(*(uint64_t*)&val);
	}
#if !defined(FLOOR_COMPUTE) || defined(FLOOR_COMPUTE_HOST)
	template <typename any_type, enable_if_t<(is_same<any_type, __uint128_t>() ||
											  is_same<any_type, __int128_t>() ||
											  sizeof(any_type) == 16)>* = nullptr>
	static floor_inline_always int32_t ctz(const any_type& val) {
		const auto upper = uint64_t((*(__uint128_t*)&val) >> __uint128_t(64));
		const auto lower = uint64_t((*(__uint128_t*)&val) & __uint128_t(0xFFFF'FFFF'FFFF'FFFFull));
		const auto ctz_upper = ctz(upper);
		const auto ctz_lower = ctz(lower);
		return (ctz_lower < 64 ? ctz_lower : (ctz_upper + ctz_lower));
	}
#endif
	
	//! count 1-bits
	template <typename uint_type, enable_if_t<(is_same<uint_type, uint16_t>())>* = nullptr>
	static floor_inline_always int32_t popcount(const uint_type& val) {
#if !defined(FLOOR_COMPUTE) || defined(FLOOR_COMPUTE_HOST)
		return __builtin_popcount((uint32_t)val);
#else
		return floor_rt_popcount(val);
#endif
	}
	template <typename uint_type, enable_if_t<(is_same<uint_type, uint32_t>())>* = nullptr>
	static floor_inline_always int32_t popcount(const uint_type& val) {
#if !defined(FLOOR_COMPUTE) || defined(FLOOR_COMPUTE_HOST)
		return __builtin_popcount(val);
#else
		return floor_rt_popcount(val);
#endif
	}
	template <typename uint_type, enable_if_t<(is_same<uint_type, uint64_t>())>* = nullptr>
	static floor_inline_always int32_t popcount(const uint_type& val) {
#if !defined(FLOOR_COMPUTE) || defined(FLOOR_COMPUTE_HOST)
		return __builtin_popcountll(val);
#else
		return floor_rt_popcount(val);
#endif
	}
	template <typename any_type, enable_if_t<(is_same<any_type, bool>())>* = nullptr>
	static floor_inline_always int32_t popcount(const any_type& val) {
		return val ? 1 : 0;
	}
	template <typename any_type, enable_if_t<(!is_same<any_type, bool>() && sizeof(any_type) == 1)>* = nullptr>
	static floor_inline_always int32_t popcount(const any_type& val) {
		const uint16_t widened_val = *(uint8_t*)&val;
		return rt_math::popcount(widened_val);
	}
	template <typename any_type, enable_if_t<(!is_same<any_type, uint16_t>() && sizeof(any_type) == 2)>* = nullptr>
	static floor_inline_always int32_t popcount(const any_type& val) {
		return rt_math::popcount(*(uint16_t*)&val);
	}
	template <typename any_type, enable_if_t<(!is_same<any_type, uint32_t>() && sizeof(any_type) == 4)>* = nullptr>
	static floor_inline_always int32_t popcount(const any_type& val) {
		return rt_math::popcount(*(uint32_t*)&val);
	}
	template <typename any_type, enable_if_t<(!is_same<any_type, uint64_t>() && sizeof(any_type) == 8)>* = nullptr>
	static floor_inline_always int32_t popcount(const any_type& val) {
		return rt_math::popcount(*(uint64_t*)&val);
	}
#if !defined(FLOOR_COMPUTE) || defined(FLOOR_COMPUTE_HOST)
	template <typename any_type, enable_if_t<(is_same<any_type, __uint128_t>() ||
											  is_same<any_type, __int128_t>() ||
											  sizeof(any_type) == 16)>* = nullptr>
	static floor_inline_always int32_t popcount(const any_type& val) {
		const auto upper = uint64_t((*(__uint128_t*)&val) >> __uint128_t(64));
		const auto lower = uint64_t((*(__uint128_t*)&val) & __uint128_t(0xFFFF'FFFF'FFFF'FFFFull));
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
	
}

#endif
