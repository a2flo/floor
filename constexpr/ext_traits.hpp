/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2018 Florian Ziesche
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

#ifndef __FLOOR_EXT_TRAITS_HPP__
#define __FLOOR_EXT_TRAITS_HPP__

#include <type_traits>
#include <experimental/type_traits>
#include <limits>

// vector is not supported on compute/graphics backends
#if !defined(FLOOR_COMPUTE) || defined(FLOOR_COMPUTE_HOST)
#include <vector>
#endif

#include <floor/core/essentials.hpp>
#include <floor/constexpr/soft_i128.hpp>
#include <floor/constexpr/soft_f16.hpp>
using namespace std;

namespace ext {
	//! is_floating_point with half, float, double and long double support
	template <typename T> struct is_floating_point :
	public conditional_t<(is_same<remove_cv_t<T>, float>::value ||
						  is_same<remove_cv_t<T>, double>::value ||
						  is_same<remove_cv_t<T>, long double>::value
#if defined(FLOOR_COMPUTE_METAL) || defined(FLOOR_COMPUTE_VULKAN) || defined(FLOOR_GRAPHICS_HOST)
						  || is_same<remove_cv_t<T>, half>::value
#endif
						  ), true_type, false_type> {};
	
	template <typename T> constexpr bool is_floating_point_v = ext::is_floating_point<T>::value;
	
	//! is_integral with std types and 128-bit int/uint
	template <typename T> struct is_integral :
	public conditional_t<(std::is_integral<T>::value ||
						  is_same<remove_cv_t<T>, __int128_t>::value ||
						  is_same<remove_cv_t<T>, __uint128_t>::value), true_type, false_type> {};
	
	template <typename T> constexpr bool is_integral_v = ext::is_integral<T>::value;
	
	//! is_signed with std types and 128-bit int
	template <typename T> struct is_signed :
	public conditional_t<(std::is_signed<T>::value ||
						  is_same<remove_cv_t<T>, __int128_t>::value), true_type, false_type> {};
	
	template <typename T> constexpr bool is_signed_v = ext::is_signed<T>::value;
	
	//! is_unsigned with std types and 128-bit uint
	template <typename T> struct is_unsigned :
	public conditional_t<(std::is_unsigned<T>::value ||
						  is_same<remove_cv_t<T>, __uint128_t>::value), true_type, false_type> {};
	
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
	
#if !defined(FLOOR_COMPUTE) || defined(FLOOR_COMPUTE_HOST)
	//! is_vector to detect vector<T>
	template <typename any_type> struct is_vector : public false_type {};
	template <typename... vec_params> struct is_vector<vector<vec_params...>> : public true_type {};
#endif
	
	//! the equivalent/corresponding unsigned integer type to sizeof(T)
	template <typename T, typename = void> struct sized_unsigned_int_eqv;
	template <typename T> struct sized_unsigned_int_eqv<T, enable_if_t<sizeof(T) == 1>> {
		typedef uint8_t type;
	};
	template <typename T> struct sized_unsigned_int_eqv<T, enable_if_t<sizeof(T) == 2>> {
		typedef uint16_t type;
	};
	template <typename T> struct sized_unsigned_int_eqv<T, enable_if_t<sizeof(T) == 4>> {
		typedef uint32_t type;
	};
	template <typename T> struct sized_unsigned_int_eqv<T, enable_if_t<sizeof(T) == 8>> {
		typedef uint64_t type;
	};
	template <typename T> struct sized_unsigned_int_eqv<T, enable_if_t<(sizeof(T) > 8)>> {
		typedef __uint128_t type;
	};
	
	template <typename T> using sized_unsigned_int_eqv_t = typename sized_unsigned_int_eqv<T>::type;
	
	//! the signed equivalent to T
	template <typename T, typename = void> struct signed_eqv;
	template <> struct signed_eqv<bool> {
		typedef bool type;
	};
	template <typename fp_type>
	struct signed_eqv<fp_type, enable_if_t<ext::is_floating_point_v<fp_type>>> {
		typedef fp_type type;
	};
	template <typename int_type>
	struct signed_eqv<int_type, enable_if_t<(ext::is_integral_v<int_type> &&
											 ext::is_signed_v<int_type>)>> {
		typedef int_type type;
	};
	template <typename uint_type>
	struct signed_eqv<uint_type, enable_if_t<(ext::is_integral_v<uint_type> &&
											  ext::is_unsigned_v<uint_type> &&
											  !is_same<uint_type, __uint128_t>())>> {
		typedef std::make_signed_t<uint_type> type;
	};
	template <typename uint_type>
	struct signed_eqv<uint_type, enable_if_t<is_same<uint_type, __uint128_t>::value>> {
		typedef __int128_t type;
	};
	
	template <typename T> using signed_eqv_t = typename signed_eqv<T>::type;
	
	//! the integral equivalent to T
	template <typename T, typename = void> struct integral_eqv;
	template <> struct integral_eqv<bool> {
		typedef bool type;
	};
	template <typename fp_type>
	struct integral_eqv<fp_type, enable_if_t<ext::is_floating_point_v<fp_type>>> {
		typedef ext::sized_unsigned_int_eqv_t<fp_type> type;
	};
	template <typename int_type>
	struct integral_eqv<int_type, enable_if_t<ext::is_integral_v<int_type>>> {
		typedef int_type type;
	};
	
	template <typename T> using integral_eqv_t = typename integral_eqv<T>::type;

}

#endif
