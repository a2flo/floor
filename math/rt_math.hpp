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
	static floor_inline_always constexpr fp_type wrap(const fp_type& val, const fp_type& max) {
		return (val < (fp_type)0 ? (max - fmod(abs(val), max)) : fmod(val, max));
	}
	
	//! wraps val to the range [0, max]
	template <typename int_type, enable_if_t<((is_integral<int_type>() &&
											   is_signed<int_type>()) &&
											  !is_same<int_type, __int128_t>())>* = nullptr>
	static floor_inline_always constexpr int_type wrap(const int_type& val, const int_type& max) {
		return (val < (int_type)0 ? (max - (std::abs(val) % max)) : (val % max));
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
	
}

#endif
