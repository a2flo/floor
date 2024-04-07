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

#include <type_traits>
#include <limits>
#include <memory>

// vector is not supported on compute/graphics backends
#if !defined(FLOOR_COMPUTE) || (defined(FLOOR_COMPUTE_HOST) && !defined(FLOOR_COMPUTE_HOST_DEVICE))
#include <vector>
#include <span>
#endif

#include <floor/core/essentials.hpp>
#include <floor/constexpr/soft_f16.hpp>
using namespace std;

namespace ext {
	//! is_floating_point with half, float, double and long double support
	template <typename T> struct is_floating_point :
	public conditional_t<(is_same_v<remove_cv_t<T>, float> ||
						  is_same_v<remove_cv_t<T>, double> ||
						  is_same_v<remove_cv_t<T>, long double> ||
						  is_same_v<remove_cv_t<T>, half>), true_type, false_type> {};
	
	template <typename T> constexpr bool is_floating_point_v = ext::is_floating_point<T>::value;
	
	//! is_integral with std types and 128-bit int/uint
	template <typename T> struct is_integral :
	public conditional_t<(std::is_integral<T>::value ||
						  is_same_v<remove_cv_t<T>, __int128_t> ||
						  is_same_v<remove_cv_t<T>, __uint128_t>), true_type, false_type> {};
	
	template <typename T> constexpr bool is_integral_v = ext::is_integral<T>::value;
	
	//! is_signed with std types and 128-bit int
	template <typename T> struct is_signed :
	public conditional_t<(std::is_signed<T>::value ||
						  is_same_v<remove_cv_t<T>, __int128_t>), true_type, false_type> {};
	
	template <typename T> constexpr bool is_signed_v = ext::is_signed<T>::value;
	
	//! is_unsigned with std types and 128-bit uint
	template <typename T> struct is_unsigned :
	public conditional_t<(std::is_unsigned<T>::value ||
						  is_same_v<remove_cv_t<T>, __uint128_t>), true_type, false_type> {};
	
	template <typename T> constexpr bool is_unsigned_v = ext::is_unsigned<T>::value;
	
	//! is_arithmetic with std types and 128-bit int/uint and 16-bit half (if supported)
	template <typename T> struct is_arithmetic :
	public conditional_t<(ext::is_floating_point_v<T> || ext::is_integral_v<T>), true_type, false_type> {};
	
	template <typename T> constexpr bool is_arithmetic_v = ext::is_arithmetic<T>::value;
	
	// is_fundamental with std types and 128-bit int/uint and 16-bit half (if supported)
	template <typename T> struct is_fundamental :
	public conditional_t<(std::is_fundamental<T>::value || ext::is_arithmetic_v<T>), true_type, false_type> {};
	
	template <typename T> constexpr bool is_fundamental_v = ext::is_fundamental<T>::value;
	
	// is_scalar with std types and 128-bit int/uint and 16-bit half (if supported)
	template <typename T> struct is_scalar :
	public conditional_t<(std::is_scalar<T>::value || ext::is_arithmetic_v<T>), true_type, false_type> {};
	
	template <typename T> constexpr bool is_scalar_v = ext::is_scalar<T>::value;
	
#if !defined(FLOOR_COMPUTE) || (defined(FLOOR_COMPUTE_HOST) && !defined(FLOOR_COMPUTE_HOST_DEVICE))
	//! is_vector to detect vector<T>
	template <typename any_type> struct is_vector : public false_type {};
	template <typename... vec_params> struct is_vector<vector<vec_params...>> : public true_type {};

	//! span<T[, extent]> detection
	template <typename any_type> struct is_span_helper : public false_type {};
	template <typename elem_type, size_t extent> struct is_span_helper<span<elem_type, extent>> : public true_type {};
	template <typename any_type> static constexpr bool is_span_v = is_span_helper<decay_t<any_type>>::value;

	//! shared_ptr<T> detection
	template <typename any_type> struct is_shared_ptr : public false_type {};
	template <typename any_type> struct is_shared_ptr<shared_ptr<any_type>> : public true_type {};
	template <typename T> constexpr bool is_shared_ptr_v = ext::is_shared_ptr<T>::value;

	//! unique_ptr<T> detection
	template <typename any_type> struct is_unique_ptr : public false_type {};
	template <typename any_type> struct is_unique_ptr<unique_ptr<any_type>> : public true_type {};
	template <typename T> constexpr bool is_unique_ptr_v = ext::is_unique_ptr<T>::value;
#endif
	
	//! the equivalent/corresponding unsigned integer type to sizeof(T)
	template <typename T> struct sized_unsigned_int_eqv;
	template <typename T> requires(sizeof(T) == 1) struct sized_unsigned_int_eqv<T> {
		typedef uint8_t type;
	};
	template <typename T> requires(sizeof(T) == 2) struct sized_unsigned_int_eqv<T> {
		typedef uint16_t type;
	};
	template <typename T> requires(sizeof(T) == 4) struct sized_unsigned_int_eqv<T> {
		typedef uint32_t type;
	};
	template <typename T> requires(sizeof(T) == 8) struct sized_unsigned_int_eqv<T> {
		typedef uint64_t type;
	};
	template <typename T> requires(sizeof(T) > 8) struct sized_unsigned_int_eqv<T> {
		typedef __uint128_t type;
	};
	
	template <typename T> using sized_unsigned_int_eqv_t = typename sized_unsigned_int_eqv<T>::type;
	
	//! the signed equivalent to T
	template <typename T> struct signed_eqv;
	template <> struct signed_eqv<bool> {
		typedef bool type;
	};
	template <typename fp_type>
	requires(ext::is_floating_point_v<fp_type>)
	struct signed_eqv<fp_type> {
		typedef fp_type type;
	};
	template <typename int_type>
	requires(ext::is_integral_v<int_type> && ext::is_signed_v<int_type>)
	struct signed_eqv<int_type> {
		typedef int_type type;
	};
	template <typename uint_type>
	requires(ext::is_integral_v<uint_type> && ext::is_unsigned_v<uint_type> && !is_same_v<uint_type, __uint128_t>)
	struct signed_eqv<uint_type> {
		typedef std::make_signed_t<uint_type> type;
	};
	template <typename uint_type>
	requires(is_same_v<uint_type, __uint128_t>)
	struct signed_eqv<uint_type> {
		typedef __int128_t type;
	};
	
	template <typename T> using signed_eqv_t = typename signed_eqv<T>::type;
	
	//! the integral equivalent to T
	template <typename T> struct integral_eqv;
	template <> struct integral_eqv<bool> {
		typedef bool type;
	};
	template <typename fp_type>
	requires(ext::is_floating_point_v<fp_type>)
	struct integral_eqv<fp_type> {
		typedef ext::sized_unsigned_int_eqv_t<fp_type> type;
	};
	template <typename int_type>
	requires(ext::is_integral_v<int_type>)
	struct integral_eqv<int_type> {
		typedef int_type type;
	};
	
	template <typename T> using integral_eqv_t = typename integral_eqv<T>::type;

}
