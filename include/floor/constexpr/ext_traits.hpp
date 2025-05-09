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

#include <type_traits>
#include <limits>
#include <memory>

// vector is not supported on compute/graphics backends
#if !defined(FLOOR_DEVICE) || (defined(FLOOR_DEVICE_HOST_COMPUTE) && !defined(FLOOR_DEVICE_HOST_COMPUTE_IS_DEVICE))
#include <vector>
#include <span>
#endif

#include <floor/core/essentials.hpp>
#include <floor/constexpr/soft_f16.hpp>

namespace fl::ext {
	//! true if "T" is std::is_same_v as any type in "any_types"
	template <typename T, typename... any_types>
	requires(sizeof...(any_types) > 0)
	struct is_same_as_any : public std::conditional_t<(std::is_same_v<T, any_types> || ...), std::true_type, std::false_type> {};
	
	template <typename T, typename... any_types> constexpr bool is_same_as_any_v = ext::is_same_as_any<T, any_types...>::value;
	
	//! std::is_floating_point with half, float, double and long double support
	template <typename T> struct is_floating_point :
	public std::conditional_t<(std::is_same_v<std::remove_cv_t<T>, float> ||
						  std::is_same_v<std::remove_cv_t<T>, double> ||
						  std::is_same_v<std::remove_cv_t<T>, long double> ||
						  std::is_same_v<std::remove_cv_t<T>, half>), std::true_type, std::false_type> {};
	
	template <typename T> constexpr bool is_floating_point_v = ext::is_floating_point<T>::value;
	
	//! std::is_integral with std types and 128-bit int/uint
	template <typename T> struct is_integral :
	public std::conditional_t<(std::is_integral<T>::value ||
						  std::is_same_v<std::remove_cv_t<T>, __int128_t> ||
						  std::is_same_v<std::remove_cv_t<T>, __uint128_t>), std::true_type, std::false_type> {};
	
	template <typename T> constexpr bool is_integral_v = ext::is_integral<T>::value;
	
	//! is_signed with std types and 128-bit int
	template <typename T> struct is_signed :
	public std::conditional_t<(std::is_signed<T>::value ||
						  std::is_same_v<std::remove_cv_t<T>, __int128_t>), std::true_type, std::false_type> {};
	
	template <typename T> constexpr bool is_signed_v = ext::is_signed<T>::value;
	
	//! is_unsigned with std types and 128-bit uint
	template <typename T> struct is_unsigned :
	public std::conditional_t<(std::is_unsigned<T>::value ||
						  std::is_same_v<std::remove_cv_t<T>, __uint128_t>), std::true_type, std::false_type> {};
	
	template <typename T> constexpr bool is_unsigned_v = ext::is_unsigned<T>::value;
	
	//! is_arithmetic with std types and 128-bit int/uint and 16-bit half (if supported)
	template <typename T> struct is_arithmetic :
	public std::conditional_t<(ext::is_floating_point_v<T> || ext::is_integral_v<T>), std::true_type, std::false_type> {};
	
	template <typename T> constexpr bool is_arithmetic_v = ext::is_arithmetic<T>::value;
	
	// is_fundamental with std types and 128-bit int/uint and 16-bit half (if supported)
	template <typename T> struct is_fundamental :
	public std::conditional_t<(std::is_fundamental<T>::value || ext::is_arithmetic_v<T>), std::true_type, std::false_type> {};
	
	template <typename T> constexpr bool is_fundamental_v = ext::is_fundamental<T>::value;
	
	// is_scalar with std types and 128-bit int/uint and 16-bit half (if supported)
	template <typename T> struct is_scalar :
	public std::conditional_t<(std::is_scalar<T>::value || ext::is_arithmetic_v<T>), std::true_type, std::false_type> {};
	
	template <typename T> constexpr bool is_scalar_v = ext::is_scalar<T>::value;
	
#if !defined(FLOOR_DEVICE) || (defined(FLOOR_DEVICE_HOST_COMPUTE) && !defined(FLOOR_DEVICE_HOST_COMPUTE_IS_DEVICE))
	//! is_vector to detect std::vector<T>
	template <typename any_type> struct is_vector : public std::false_type {};
	template <typename... vec_params> struct is_vector<std::vector<vec_params...>> : public std::true_type {};
	template <typename T> constexpr bool is_vector_v = ext::is_vector<T>::value;

	//! std::span<T[, extent]> detection
	template <typename any_type> struct is_span_helper : public std::false_type {};
	template <typename elem_type, size_t extent> struct is_span_helper<std::span<elem_type, extent>> : public std::true_type {};
	template <typename any_type> static constexpr bool is_span_v = is_span_helper<std::decay_t<any_type>>::value;

	//! std::shared_ptr<T> detection
	template <typename any_type> struct is_shared_ptr : public std::false_type {};
	template <typename any_type> struct is_shared_ptr<std::shared_ptr<any_type>> : public std::true_type {};
	template <typename T> constexpr bool is_shared_ptr_v = ext::is_shared_ptr<T>::value;

	//! std::unique_ptr<T> detection
	template <typename any_type> struct is_unique_ptr : public std::false_type {};
	template <typename any_type> struct is_unique_ptr<std::unique_ptr<any_type>> : public std::true_type {};
	template <typename T> constexpr bool is_unique_ptr_v = ext::is_unique_ptr<T>::value;
#endif
	
	//! the equivalent/corresponding unsigned integer type to sizeof(T)
	template <typename T> struct sized_unsigned_int_eqv;
	template <typename T> requires(sizeof(T) == 1) struct sized_unsigned_int_eqv<T> {
		using type = uint8_t;
	};
	template <typename T> requires(sizeof(T) == 2) struct sized_unsigned_int_eqv<T> {
		using type = uint16_t;
	};
	template <typename T> requires(sizeof(T) == 4) struct sized_unsigned_int_eqv<T> {
		using type = uint32_t;
	};
	template <typename T> requires(sizeof(T) == 8) struct sized_unsigned_int_eqv<T> {
		using type = uint64_t;
	};
	template <typename T> requires(sizeof(T) > 8) struct sized_unsigned_int_eqv<T> {
		using type = __uint128_t;
	};
	
	template <typename T> using sized_unsigned_int_eqv_t = typename sized_unsigned_int_eqv<T>::type;
	
	//! the signed equivalent to T
	template <typename T> struct signed_eqv;
	template <> struct signed_eqv<bool> {
		using type = bool;
	};
	template <typename fp_type>
	requires(ext::is_floating_point_v<fp_type>)
	struct signed_eqv<fp_type> {
		using type = fp_type;
	};
	template <typename int_type>
	requires(ext::is_integral_v<int_type> && ext::is_signed_v<int_type>)
	struct signed_eqv<int_type> {
		using type = int_type;
	};
	template <typename uint_type>
	requires(ext::is_integral_v<uint_type> && ext::is_unsigned_v<uint_type> && !std::is_same_v<uint_type, __uint128_t>)
	struct signed_eqv<uint_type> {
		using type = std::make_signed_t<uint_type>;
	};
	template <typename uint_type>
	requires(std::is_same_v<uint_type, __uint128_t>)
	struct signed_eqv<uint_type> {
		using type = __int128_t;
	};
	
	template <typename T> using signed_eqv_t = typename signed_eqv<T>::type;
	
	//! the integral equivalent to T
	template <typename T> struct integral_eqv;
	template <> struct integral_eqv<bool> {
		using type = bool;
	};
	template <typename fp_type>
	requires(ext::is_floating_point_v<fp_type>)
	struct integral_eqv<fp_type> {
		using type = ext::sized_unsigned_int_eqv_t<fp_type>;
	};
	template <typename int_type>
	requires(ext::is_integral_v<int_type>)
	struct integral_eqv<int_type> {
		using type = int_type;
	};
	
	template <typename T> using integral_eqv_t = typename integral_eqv<T>::type;

} // namespace fl::ext
