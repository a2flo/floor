/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2015 Florian Ziesche
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

#ifndef __FLOOR_VECTOR_HELPER_HPP__
#define __FLOOR_VECTOR_HELPER_HPP__

#include <type_traits>
#include <limits>
#if !defined(FLOOR_COMPUTE)
#include <cstdint>
#endif
#include <floor/constexpr/const_math.hpp>
using namespace std;

//! the unsigned integer type equivalent/corresponding to the size of T
template <typename T, class disabled_default = void> struct sized_unsigned_eqv;
template <typename T> struct sized_unsigned_eqv<T, typename enable_if<sizeof(T) == 1, void>::type> {
	typedef uint8_t type;
};
template <typename T> struct sized_unsigned_eqv<T, typename enable_if<sizeof(T) == 2, void>::type> {
	typedef uint16_t type;
};
template <typename T> struct sized_unsigned_eqv<T, typename enable_if<sizeof(T) == 4, void>::type> {
	typedef uint32_t type;
};
template <typename T> struct sized_unsigned_eqv<T, typename enable_if<sizeof(T) == 8, void>::type> {
	typedef uint64_t type;
};
template <typename T> struct sized_unsigned_eqv<T, typename enable_if<(sizeof(T) > 8), void>::type> {
	typedef __uint128_t type;
};

//! the signed equivalent to T
template <typename T, class disabled_default = void> struct signed_eqv;
template <> struct signed_eqv<bool> {
	typedef bool type;
};
template <typename fp_type>
struct signed_eqv<fp_type, typename enable_if<is_floating_point<fp_type>::value, void>::type> {
	typedef fp_type type;
};
template <typename int_type>
struct signed_eqv<int_type, typename enable_if<(is_integral<int_type>::value &&
												is_signed<int_type>::value &&
												!is_same<int_type, __int128_t>::value), void>::type> {
	typedef int_type type;
};
template <typename uint_type>
struct signed_eqv<uint_type, typename enable_if<(is_integral<uint_type>::value &&
												 is_unsigned<uint_type>::value &&
												 sizeof(uint_type) <= 8), void>::type> {
	typedef make_signed_t<uint_type> type;
};
template <typename uint_type>
struct signed_eqv<uint_type, typename enable_if<(is_integral<uint_type>::value &&
												 is_unsigned<uint_type>::value &&
												 !is_same<uint_type, __uint128_t>::value &&
												 sizeof(uint_type) == 16), void>::type> {
	typedef __int128_t type;
};
// is_integral is not specialized for __int128_t and __uint128_t
template <typename int128_type>
struct signed_eqv<int128_type, typename enable_if<(is_same<int128_type, __int128_t>::value ||
												   is_same<int128_type, __uint128_t>::value), void>::type> {
	typedef __int128_t type;
};

//! the integral equivalent to T
template <typename T, class disabled_default = void> struct integral_eqv;
template <> struct integral_eqv<bool> {
	typedef bool type;
};
template <typename fp_type>
struct integral_eqv<fp_type, typename enable_if<is_floating_point<fp_type>::value, void>::type> {
	typedef typename sized_unsigned_eqv<fp_type>::type type;
};
template <typename int_type>
struct integral_eqv<int_type, typename enable_if<(is_integral<int_type>::value &&
												  !is_same<int_type, __int128_t>::value &&
												  !is_same<int_type, __uint128_t>::value), void>::type> {
	typedef int_type type;
};
// is_integral is not specialized for __int128_t and __uint128_t
template <typename int128_type>
struct integral_eqv<int128_type, typename enable_if<(is_same<int128_type, __int128_t>::value ||
													 is_same<int128_type, __uint128_t>::value), void>::type> {
	typedef int128_type type;
};

// nan and inf aren't constexpr yet in msvc's stl
#if defined(_MSC_VER)
template <typename T, typename = void> struct nan_helper {
	static constexpr constant const T scalar_nan { (T)0 };
};
template <typename T> struct nan_helper<T, enable_if_t<is_same<T, float>::value>> {
	static constexpr constant const T scalar_nan { __builtin_nanf("") };
};
template <typename T> struct nan_helper<T, enable_if_t<is_same<T, double>::value>> {
	static constexpr constant const T scalar_nan { __builtin_nan("") };
};
template <typename T> struct nan_helper<T, enable_if_t<is_same<T, long double>::value>> {
	static constexpr constant const T scalar_nan { __builtin_nanl("") };
};
template <typename T, typename = void> struct inf_helper {
	static constexpr constant const T scalar_inf { (T)0 };
};
template <typename T> struct inf_helper<T, enable_if_t<is_same<T, float>::value>> {
	static constexpr constant const T scalar_inf { __builtin_huge_valf() };
};
template <typename T> struct inf_helper<T, enable_if_t<is_same<T, double>::value>> {
	static constexpr constant const T scalar_inf { __builtin_huge_val() };
};
template <typename T> struct inf_helper<T, enable_if_t<is_same<T, long double>::value>> {
	static constexpr constant const T scalar_inf { __builtin_huge_vall() };
};
#else
template <typename T> struct nan_helper {
	static constexpr constant const T scalar_nan { numeric_limits<T>::quiet_NaN() };
};
template <typename T> struct inf_helper {
	static constexpr constant const T scalar_inf { numeric_limits<T>::infinity() };
};
#endif

//! base class
template <typename T> class vector_helper {
public:
	typedef typename integral_eqv<T>::type integral_type;
	
	// these have no implementation in here
	static constexpr T modulo(const T& lhs, const T& rhs);
	static constexpr T sqrt(const T& val);
	static constexpr T inv_sqrt(const T& val);
	static constexpr T abs(const T& val);
	static constexpr T floor(const T& val);
	static constexpr T ceil(const T& val);
	static constexpr T round(const T& val);
	static constexpr T trunc(const T& val);
	static constexpr T rint(const T& val);
	static constexpr T sin(const T& val);
	static constexpr T cos(const T& val);
	static constexpr T tan(const T& val);
	static constexpr T asin(const T& val);
	static constexpr T acos(const T& val);
	static constexpr T atan(const T& val);
	static constexpr T atan2(const T& lhs, const T& rhs);
	static constexpr T exp(const T& val);
	static constexpr T log(const T& val);
	static constexpr T fma(const T& a, const T& b, const T& c);
	static constexpr T bit_and(const T& lhs, const integral_type& rhs);
	static constexpr T bit_or(const T& lhs, const integral_type& rhs);
	static constexpr T bit_xor(const T& lhs, const integral_type& rhs);
	static constexpr T bit_left_shift(const T& lhs, const integral_type& rhs);
	static constexpr T bit_right_shift(const T& lhs, const integral_type& rhs);
	static constexpr T unary_not(const T& val);
	static constexpr T unary_complement(const T& val);
	
	// TODO: I/O?
	
protected:
	// static class
	vector_helper(const vector_helper&) = delete;
	~vector_helper() = delete;
	vector_helper& operator=(const vector_helper&) = delete;
	
};

//! macro that defines a static "func_constexpr" function, taking a single argument with the name "func_name"
#define FLOOR_VH_FUNC_IMPL_1(func_name, func_constexpr, func_impl) \
static func_constexpr scalar_type func_name (const scalar_type& val) { \
	return func_impl; \
}
//! macro that defines a static "func_constexpr" function, taking two arguments with the name "func_name"
#define FLOOR_VH_FUNC_IMPL_2(func_name, func_constexpr, func_impl) \
static func_constexpr scalar_type func_name (const scalar_type& lhs, const scalar_type& rhs) { \
	return func_impl; \
}
//! macro that defines a static "func_constexpr" function, taking two arguments (lhs = scalar_type and rhs = integral_type),
//! with the name "func_name"
#define FLOOR_VH_FUNC_IMPL_2_INT(func_name, func_constexpr, func_impl, ...) \
static func_constexpr scalar_type func_name (const scalar_type& lhs, const integral_type& rhs) { \
	__VA_ARGS__ ; \
	return func_impl; \
}
//! macro that defines a static "func_constexpr" function, taking three arguments with the name "func_name"
#define FLOOR_VH_FUNC_IMPL_3(func_name, func_constexpr, func_impl) \
static func_constexpr scalar_type func_name (const scalar_type& a, const scalar_type& b, const scalar_type& c) { \
	return func_impl; \
}

//! implements the vector_helper class for a specific "vh_type" and it's function implementations
#define FLOOR_VH_IMPL(vh_type, func_impl) \
template <> class vector_helper<vh_type> { \
public: \
	typedef vh_type scalar_type; \
	typedef typename signed_eqv<vh_type>::type signed_type; \
	typedef typename sized_unsigned_eqv<vh_type>::type sized_unsigned_type; \
	typedef typename integral_eqv<vh_type>::type integral_type; \
	static constexpr constant const scalar_type scalar_zero { (scalar_type)0 }; \
	static constexpr constant const scalar_type scalar_one { (scalar_type)1 }; \
	static constexpr constant const scalar_type scalar_nan { nan_helper<scalar_type>::scalar_nan }; \
	static constexpr constant const scalar_type scalar_inf { inf_helper<scalar_type>::scalar_inf }; \
	func_impl \
\
protected: \
	vector_helper(const vector_helper&) = delete; \
	~vector_helper() = delete; \
	vector_helper& operator=(const vector_helper&) = delete; \
};

// int base types: bool, char, short, int, long long int, unsigned char, unsigned short, unsigned int, unsigned long long int
//              -> bool, int8_t, int16_t, int32_t, int64_t, uint8_t, uint16_t, uint32_t, uint64_t
// float base types: float, double, long double

#define FLOOR_VH_IMPL_DEF_FLOAT(F1, F2, F2_INT, F3) FLOOR_VH_IMPL(float, \
F2(modulo, constexpr, const_select::fmod(lhs, rhs)) \
F1(sqrt, constexpr, const_select::sqrt(val)) \
F1(inv_sqrt, constexpr, const_select::inv_sqrt(val)) \
F1(abs, constexpr, const_select::abs(val)) \
F1(floor, constexpr, const_select::floor(val)) \
F1(ceil, constexpr, const_select::ceil(val)) \
F1(round, constexpr, const_select::round(val)) \
F1(trunc, constexpr, const_select::trunc(val)) \
F1(rint, constexpr, const_select::rint(val)) \
F1(sin, constexpr, const_select::sin(val)) \
F1(cos, constexpr, const_select::cos(val)) \
F1(tan, constexpr, const_select::tan(val)) \
F1(asin, constexpr, const_select::asin(val)) \
F1(acos, constexpr, const_select::acos(val)) \
F1(atan, constexpr, const_select::atan(val)) \
F2(atan2, constexpr, const_select::atan2(lhs, rhs)) \
F1(exp, constexpr, const_select::exp(val)) \
F1(log, constexpr, const_select::log(val)) \
F3(fma, constexpr, const_select::fma(a, b, c)) \
F2_INT(bit_and, , *(float*)&ret, const auto ret = *(uint32_t*)&lhs & rhs) \
F2_INT(bit_or, , *(float*)&ret, const auto ret = *(uint32_t*)&lhs | rhs) \
F2_INT(bit_xor, , *(float*)&ret, const auto ret = *(uint32_t*)&lhs ^ rhs) \
F2_INT(bit_left_shift, , *(float*)&ret, const auto ret = *(uint32_t*)&lhs << rhs) \
F2_INT(bit_right_shift, , *(float*)&ret, const auto ret = *(uint32_t*)&lhs >> rhs) \
F1(unary_not, constexpr, (val == 0.0f ? 0.0f : 1.0f)) \
F1(unary_complement, constexpr, (val < 0.0f ? 1.0f : -1.0f) * (__FLT_MAX__ - const_select::abs(val))) \
)

#define FLOOR_VH_IMPL_DEF_DOUBLE(F1, F2, F2_INT, F3) FLOOR_VH_IMPL(double, \
F2(modulo, constexpr, const_select::fmod(lhs, rhs)) \
F1(sqrt, constexpr, const_select::sqrt(val)) \
F1(inv_sqrt, constexpr, const_select::inv_sqrt(val)) \
F1(abs, constexpr, const_select::abs(val)) \
F1(floor, constexpr, const_select::floor(val)) \
F1(ceil, constexpr, const_select::ceil(val)) \
F1(round, constexpr, const_select::round(val)) \
F1(trunc, constexpr, const_select::trunc(val)) \
F1(rint, constexpr, const_select::rint(val)) \
F1(sin, constexpr, const_select::sin(val)) \
F1(cos, constexpr, const_select::cos(val)) \
F1(tan, constexpr, const_select::tan(val)) \
F1(asin, constexpr, const_select::asin(val)) \
F1(acos, constexpr, const_select::acos(val)) \
F1(atan, constexpr, const_select::atan(val)) \
F2(atan2, constexpr, const_select::atan2(lhs, rhs)) \
F1(exp, constexpr, const_select::exp(val)) \
F1(log, constexpr, const_select::log(val)) \
F3(fma, constexpr, const_select::fma(a, b, c)) \
F2_INT(bit_and, , *(double*)&ret, const auto ret = *(uint64_t*)&lhs & rhs) \
F2_INT(bit_or, , *(double*)&ret, const auto ret = *(uint64_t*)&lhs | rhs) \
F2_INT(bit_xor, , *(double*)&ret, const auto ret = *(uint64_t*)&lhs ^ rhs) \
F2_INT(bit_left_shift, , *(double*)&ret, const auto ret = *(uint64_t*)&lhs << rhs) \
F2_INT(bit_right_shift, , *(double*)&ret, const auto ret = *(uint64_t*)&lhs >> rhs) \
F1(unary_not, constexpr, (val == 0.0 ? 0.0 : 1.0)) \
F1(unary_complement, constexpr, (val < 0.0 ? 1.0 : -1.0) * (__DBL_MAX__ - const_select::abs(val))) \
)

#define FLOOR_VH_IMPL_DEF_LDOUBLE(F1, F2, F2_INT, F3) FLOOR_VH_IMPL(long double, \
F2(modulo, constexpr, const_select::fmod(lhs, rhs)) \
F1(sqrt, constexpr, const_select::sqrt(val)) \
F1(inv_sqrt, constexpr, const_select::inv_sqrt(val)) \
F1(abs, constexpr, const_select::abs(val)) \
F1(floor, constexpr, const_select::floor(val)) \
F1(ceil, constexpr, const_select::ceil(val)) \
F1(round, constexpr, const_select::round(val)) \
F1(trunc, constexpr, const_select::trunc(val)) \
F1(rint, constexpr, const_select::rint(val)) \
F1(sin, constexpr, const_select::sin(val)) \
F1(cos, constexpr, const_select::cos(val)) \
F1(tan, constexpr, const_select::tan(val)) \
F1(asin, constexpr, const_select::asin(val)) \
F1(acos, constexpr, const_select::acos(val)) \
F1(atan, constexpr, const_select::atan(val)) \
F2(atan2, constexpr, const_select::atan2(lhs, rhs)) \
F1(exp, constexpr, const_select::exp(val)) \
F1(log, constexpr, const_select::log(val)) \
F3(fma, constexpr, const_select::fma(a, b, c)) \
F2_INT(bit_and, , *(long double*)&ret, const auto ret = *(integral_type*)&lhs & rhs) \
F2_INT(bit_or, , *(long double*)&ret, const auto ret = *(integral_type*)&lhs | rhs) \
F2_INT(bit_xor, , *(long double*)&ret, const auto ret = *(integral_type*)&lhs ^ rhs) \
F2_INT(bit_left_shift, , *(long double*)&ret, const auto ret = *(integral_type*)&lhs << rhs) \
F2_INT(bit_right_shift, , *(long double*)&ret, const auto ret = *(integral_type*)&lhs >> rhs) \
F1(unary_not, constexpr, (val == 0.0L ? 0.0L : 1.0L)) \
F1(unary_complement, constexpr, (val < 0.0L ? 1.0L : -1.0L) * (__LDBL_MAX__ - const_select::abs(val))) \
)

#define FLOOR_VH_IMPL_DEF_INT32(F1, F2, F2_INT, F3) FLOOR_VH_IMPL(int32_t, \
F2(modulo, constexpr, lhs % rhs) \
F1(sqrt, constexpr, (scalar_type)const_select::sqrt((const_math::max_fp_type)val)) \
F1(inv_sqrt, constexpr, val) \
F1(abs, constexpr, const_math::abs(val)) \
F1(floor, constexpr, val) \
F1(ceil, constexpr, val) \
F1(round, constexpr, val) \
F1(trunc, constexpr, val) \
F1(rint, constexpr, val) \
F1(sin, constexpr, (scalar_type)const_select::sin((const_math::max_fp_type)val)) \
F1(cos, constexpr, (scalar_type)const_select::cos((const_math::max_fp_type)val)) \
F1(tan, constexpr, (scalar_type)const_select::tan((const_math::max_fp_type)val)) \
F1(asin, constexpr, (scalar_type)const_select::asin((const_math::max_fp_type)val)) \
F1(acos, constexpr, (scalar_type)const_select::acos((const_math::max_fp_type)val)) \
F1(atan, constexpr, (scalar_type)const_select::atan((const_math::max_fp_type)val)) \
F2(atan2, constexpr, (scalar_type)const_select::atan2((const_math::max_fp_type)lhs, (const_math::max_fp_type)rhs)) \
F1(exp, constexpr, (scalar_type)const_select::exp((const_math::max_fp_type)val)) \
F1(log, constexpr, (scalar_type)const_select::log((const_math::max_fp_type)val)) \
F3(fma, constexpr, ((a * b) + c)) \
F2_INT(bit_and, constexpr, (lhs & rhs)) \
F2_INT(bit_or, constexpr, (lhs | rhs)) \
F2_INT(bit_xor, constexpr, (lhs ^ rhs)) \
F2_INT(bit_left_shift, constexpr, (lhs << rhs)) \
F2_INT(bit_right_shift, constexpr, (lhs >> rhs)) \
F1(unary_not, constexpr, (!val)) \
F1(unary_complement, constexpr, (~val)) \
)

#define FLOOR_VH_IMPL_DEF_UINT32(F1, F2, F2_INT, F3) FLOOR_VH_IMPL(uint32_t, \
F2(modulo, constexpr, lhs % rhs) \
F1(sqrt, constexpr, (scalar_type)const_select::sqrt((const_math::max_fp_type)val)) \
F1(inv_sqrt, constexpr, val) \
F1(abs, constexpr, val) \
F1(floor, constexpr, val) \
F1(ceil, constexpr, val) \
F1(round, constexpr, val) \
F1(trunc, constexpr, val) \
F1(rint, constexpr, val) \
F1(sin, constexpr, (scalar_type)const_select::sin((const_math::max_fp_type)val)) \
F1(cos, constexpr, (scalar_type)const_select::cos((const_math::max_fp_type)val)) \
F1(tan, constexpr, (scalar_type)const_select::tan((const_math::max_fp_type)val)) \
F1(asin, constexpr, (scalar_type)const_select::asin((const_math::max_fp_type)val)) \
F1(acos, constexpr, (scalar_type)const_select::acos((const_math::max_fp_type)val)) \
F1(atan, constexpr, (scalar_type)const_select::atan((const_math::max_fp_type)val)) \
F2(atan2, constexpr, (scalar_type)const_select::atan2((const_math::max_fp_type)lhs, (const_math::max_fp_type)rhs)) \
F1(exp, constexpr, (scalar_type)const_select::exp((const_math::max_fp_type)val)) \
F1(log, constexpr, (scalar_type)const_select::log((const_math::max_fp_type)val)) \
F3(fma, constexpr, ((a * b) + c)) \
F2_INT(bit_and, constexpr, (lhs & rhs)) \
F2_INT(bit_or, constexpr, (lhs | rhs)) \
F2_INT(bit_xor, constexpr, (lhs ^ rhs)) \
F2_INT(bit_left_shift, constexpr, (lhs << rhs)) \
F2_INT(bit_right_shift, constexpr, (lhs >> rhs)) \
F1(unary_not, constexpr, (!val)) \
F1(unary_complement, constexpr, (~val)) \
)

#define FLOOR_VH_IMPL_DEF_INT8(F1, F2, F2_INT, F3) FLOOR_VH_IMPL(int8_t, \
F2(modulo, constexpr, lhs % rhs) \
F1(sqrt, constexpr, (scalar_type)const_select::sqrt((const_math::max_fp_type)val)) \
F1(inv_sqrt, constexpr, val) \
F1(abs, constexpr, const_math::abs(val)) \
F1(floor, constexpr, val) \
F1(ceil, constexpr, val) \
F1(round, constexpr, val) \
F1(trunc, constexpr, val) \
F1(rint, constexpr, val) \
F1(sin, constexpr, (scalar_type)const_select::sin((const_math::max_fp_type)val)) \
F1(cos, constexpr, (scalar_type)const_select::cos((const_math::max_fp_type)val)) \
F1(tan, constexpr, (scalar_type)const_select::tan((const_math::max_fp_type)val)) \
F1(asin, constexpr, (scalar_type)const_select::asin((const_math::max_fp_type)val)) \
F1(acos, constexpr, (scalar_type)const_select::acos((const_math::max_fp_type)val)) \
F1(atan, constexpr, (scalar_type)const_select::atan((const_math::max_fp_type)val)) \
F2(atan2, constexpr, (scalar_type)const_select::atan2((const_math::max_fp_type)lhs, (const_math::max_fp_type)rhs)) \
F1(exp, constexpr, (scalar_type)const_select::exp((const_math::max_fp_type)val)) \
F1(log, constexpr, (scalar_type)const_select::log((const_math::max_fp_type)val)) \
F3(fma, constexpr, ((a * b) + c)) \
F2_INT(bit_and, constexpr, (lhs & rhs)) \
F2_INT(bit_or, constexpr, (lhs | rhs)) \
F2_INT(bit_xor, constexpr, (lhs ^ rhs)) \
F2_INT(bit_left_shift, constexpr, (int8_t)(lhs << rhs)) \
F2_INT(bit_right_shift, constexpr, (int8_t)(lhs >> rhs)) \
F1(unary_not, constexpr, (!val)) \
F1(unary_complement, constexpr, (~val)) \
)

#define FLOOR_VH_IMPL_DEF_UINT8(F1, F2, F2_INT, F3) FLOOR_VH_IMPL(uint8_t, \
F2(modulo, constexpr, lhs % rhs) \
F1(sqrt, constexpr, (scalar_type)const_select::sqrt((const_math::max_fp_type)val)) \
F1(inv_sqrt, constexpr, val) \
F1(abs, constexpr, val) \
F1(floor, constexpr, val) \
F1(ceil, constexpr, val) \
F1(round, constexpr, val) \
F1(trunc, constexpr, val) \
F1(rint, constexpr, val) \
F1(sin, constexpr, (scalar_type)const_select::sin((const_math::max_fp_type)val)) \
F1(cos, constexpr, (scalar_type)const_select::cos((const_math::max_fp_type)val)) \
F1(tan, constexpr, (scalar_type)const_select::tan((const_math::max_fp_type)val)) \
F1(asin, constexpr, (scalar_type)const_select::asin((const_math::max_fp_type)val)) \
F1(acos, constexpr, (scalar_type)const_select::acos((const_math::max_fp_type)val)) \
F1(atan, constexpr, (scalar_type)const_select::atan((const_math::max_fp_type)val)) \
F2(atan2, constexpr, (scalar_type)const_select::atan2((const_math::max_fp_type)lhs, (const_math::max_fp_type)rhs)) \
F1(exp, constexpr, (scalar_type)const_select::exp((const_math::max_fp_type)val)) \
F1(log, constexpr, (scalar_type)const_select::log((const_math::max_fp_type)val)) \
F3(fma, constexpr, ((a * b) + c)) \
F2_INT(bit_and, constexpr, (lhs & rhs)) \
F2_INT(bit_or, constexpr, (lhs | rhs)) \
F2_INT(bit_xor, constexpr, (lhs ^ rhs)) \
F2_INT(bit_left_shift, constexpr, (uint8_t)(lhs << rhs)) \
F2_INT(bit_right_shift, constexpr, (uint8_t)(lhs >> rhs)) \
F1(unary_not, constexpr, (!val)) \
F1(unary_complement, constexpr, (~val)) \
)

#define FLOOR_VH_IMPL_DEF_INT16(F1, F2, F2_INT, F3) FLOOR_VH_IMPL(int16_t, \
F2(modulo, constexpr, lhs % rhs) \
F1(sqrt, constexpr, (scalar_type)const_select::sqrt((const_math::max_fp_type)val)) \
F1(inv_sqrt, constexpr, val) \
F1(abs, constexpr, const_math::abs(val)) \
F1(floor, constexpr, val) \
F1(ceil, constexpr, val) \
F1(round, constexpr, val) \
F1(trunc, constexpr, val) \
F1(rint, constexpr, val) \
F1(sin, constexpr, (scalar_type)const_select::sin((const_math::max_fp_type)val)) \
F1(cos, constexpr, (scalar_type)const_select::cos((const_math::max_fp_type)val)) \
F1(tan, constexpr, (scalar_type)const_select::tan((const_math::max_fp_type)val)) \
F1(asin, constexpr, (scalar_type)const_select::asin((const_math::max_fp_type)val)) \
F1(acos, constexpr, (scalar_type)const_select::acos((const_math::max_fp_type)val)) \
F1(atan, constexpr, (scalar_type)const_select::atan((const_math::max_fp_type)val)) \
F2(atan2, constexpr, (scalar_type)const_select::atan2((const_math::max_fp_type)lhs, (const_math::max_fp_type)rhs)) \
F1(exp, constexpr, (scalar_type)const_select::exp((const_math::max_fp_type)val)) \
F1(log, constexpr, (scalar_type)const_select::log((const_math::max_fp_type)val)) \
F3(fma, constexpr, ((a * b) + c)) \
F2_INT(bit_and, constexpr, (lhs & rhs)) \
F2_INT(bit_or, constexpr, (lhs | rhs)) \
F2_INT(bit_xor, constexpr, (lhs ^ rhs)) \
F2_INT(bit_left_shift, constexpr, (int16_t)(lhs << rhs)) \
F2_INT(bit_right_shift, constexpr, (int16_t)(lhs >> rhs)) \
F1(unary_not, constexpr, (!val)) \
F1(unary_complement, constexpr, (~val)) \
)

#define FLOOR_VH_IMPL_DEF_UINT16(F1, F2, F2_INT, F3) FLOOR_VH_IMPL(uint16_t, \
F2(modulo, constexpr, lhs % rhs) \
F1(sqrt, constexpr, (scalar_type)const_select::sqrt((const_math::max_fp_type)val)) \
F1(inv_sqrt, constexpr, val) \
F1(abs, constexpr, val) \
F1(floor, constexpr, val) \
F1(ceil, constexpr, val) \
F1(round, constexpr, val) \
F1(trunc, constexpr, val) \
F1(rint, constexpr, val) \
F1(sin, constexpr, (scalar_type)const_select::sin((const_math::max_fp_type)val)) \
F1(cos, constexpr, (scalar_type)const_select::cos((const_math::max_fp_type)val)) \
F1(tan, constexpr, (scalar_type)const_select::tan((const_math::max_fp_type)val)) \
F1(asin, constexpr, (scalar_type)const_select::asin((const_math::max_fp_type)val)) \
F1(acos, constexpr, (scalar_type)const_select::acos((const_math::max_fp_type)val)) \
F1(atan, constexpr, (scalar_type)const_select::atan((const_math::max_fp_type)val)) \
F2(atan2, constexpr, (scalar_type)const_select::atan2((const_math::max_fp_type)lhs, (const_math::max_fp_type)rhs)) \
F1(exp, constexpr, (scalar_type)const_select::exp((const_math::max_fp_type)val)) \
F1(log, constexpr, (scalar_type)const_select::log((const_math::max_fp_type)val)) \
F3(fma, constexpr, ((a * b) + c)) \
F2_INT(bit_and, constexpr, (lhs & rhs)) \
F2_INT(bit_or, constexpr, (lhs | rhs)) \
F2_INT(bit_xor, constexpr, (lhs ^ rhs)) \
F2_INT(bit_left_shift, constexpr, (uint16_t)(lhs << rhs)) \
F2_INT(bit_right_shift, constexpr, (uint16_t)(lhs >> rhs)) \
F1(unary_not, constexpr, (!val)) \
F1(unary_complement, constexpr, (~val)) \
)

#define FLOOR_VH_IMPL_DEF_INT64(F1, F2, F2_INT, F3) FLOOR_VH_IMPL(int64_t, \
F2(modulo, constexpr, lhs % rhs) \
F1(sqrt, constexpr, (scalar_type)const_select::sqrt((const_math::max_fp_type)val)) \
F1(inv_sqrt, constexpr, val) \
F1(abs, constexpr, const_math::abs(val)) \
F1(floor, constexpr, val) \
F1(ceil, constexpr, val) \
F1(round, constexpr, val) \
F1(trunc, constexpr, val) \
F1(rint, constexpr, val) \
F1(sin, constexpr, (scalar_type)const_select::sin((const_math::max_fp_type)val)) \
F1(cos, constexpr, (scalar_type)const_select::cos((const_math::max_fp_type)val)) \
F1(tan, constexpr, (scalar_type)const_select::tan((const_math::max_fp_type)val)) \
F1(asin, constexpr, (scalar_type)const_select::asin((const_math::max_fp_type)val)) \
F1(acos, constexpr, (scalar_type)const_select::acos((const_math::max_fp_type)val)) \
F1(atan, constexpr, (scalar_type)const_select::atan((const_math::max_fp_type)val)) \
F2(atan2, constexpr, (scalar_type)const_select::atan2((const_math::max_fp_type)lhs, (const_math::max_fp_type)rhs)) \
F1(exp, constexpr, (scalar_type)const_select::exp((const_math::max_fp_type)val)) \
F1(log, constexpr, (scalar_type)const_select::log((const_math::max_fp_type)val)) \
F3(fma, constexpr, ((a * b) + c)) \
F2_INT(bit_and, constexpr, (lhs & rhs)) \
F2_INT(bit_or, constexpr, (lhs | rhs)) \
F2_INT(bit_xor, constexpr, (lhs ^ rhs)) \
F2_INT(bit_left_shift, constexpr, (lhs << rhs)) \
F2_INT(bit_right_shift, constexpr, (lhs >> rhs)) \
F1(unary_not, constexpr, (!val)) \
F1(unary_complement, constexpr, (~val)) \
)

#define FLOOR_VH_IMPL_DEF_UINT64(F1, F2, F2_INT, F3) FLOOR_VH_IMPL(uint64_t, \
F2(modulo, constexpr, lhs % rhs) \
F1(sqrt, constexpr, (scalar_type)const_select::sqrt((const_math::max_fp_type)val)) \
F1(inv_sqrt, constexpr, val) \
F1(abs, constexpr, val) \
F1(floor, constexpr, val) \
F1(ceil, constexpr, val) \
F1(round, constexpr, val) \
F1(trunc, constexpr, val) \
F1(rint, constexpr, val) \
F1(sin, constexpr, (scalar_type)const_select::sin((const_math::max_fp_type)val)) \
F1(cos, constexpr, (scalar_type)const_select::cos((const_math::max_fp_type)val)) \
F1(tan, constexpr, (scalar_type)const_select::tan((const_math::max_fp_type)val)) \
F1(asin, constexpr, (scalar_type)const_select::asin((const_math::max_fp_type)val)) \
F1(acos, constexpr, (scalar_type)const_select::acos((const_math::max_fp_type)val)) \
F1(atan, constexpr, (scalar_type)const_select::atan((const_math::max_fp_type)val)) \
F2(atan2, constexpr, (scalar_type)const_select::atan2((const_math::max_fp_type)lhs, (const_math::max_fp_type)rhs)) \
F1(exp, constexpr, (scalar_type)const_select::exp((const_math::max_fp_type)val)) \
F1(log, constexpr, (scalar_type)const_select::log((const_math::max_fp_type)val)) \
F3(fma, constexpr, ((a * b) + c)) \
F2_INT(bit_and, constexpr, (lhs & rhs)) \
F2_INT(bit_or, constexpr, (lhs | rhs)) \
F2_INT(bit_xor, constexpr, (lhs ^ rhs)) \
F2_INT(bit_left_shift, constexpr, (lhs << rhs)) \
F2_INT(bit_right_shift, constexpr, (lhs >> rhs)) \
F1(unary_not, constexpr, (!val)) \
F1(unary_complement, constexpr, (~val)) \
)

#define FLOOR_VH_IMPL_DEF_SSIZE_T(F1, F2, F2_INT, F3) FLOOR_VH_IMPL(ssize_t, \
F2(modulo, constexpr, lhs % rhs) \
F1(sqrt, constexpr, (scalar_type)const_select::sqrt((const_math::max_fp_type)val)) \
F1(inv_sqrt, constexpr, val) \
F1(abs, constexpr, const_math::abs(val)) \
F1(floor, constexpr, val) \
F1(ceil, constexpr, val) \
F1(round, constexpr, val) \
F1(trunc, constexpr, val) \
F1(rint, constexpr, val) \
F1(sin, constexpr, (scalar_type)const_select::sin((const_math::max_fp_type)val)) \
F1(cos, constexpr, (scalar_type)const_select::cos((const_math::max_fp_type)val)) \
F1(tan, constexpr, (scalar_type)const_select::tan((const_math::max_fp_type)val)) \
F1(asin, constexpr, (scalar_type)const_select::asin((const_math::max_fp_type)val)) \
F1(acos, constexpr, (scalar_type)const_select::acos((const_math::max_fp_type)val)) \
F1(atan, constexpr, (scalar_type)const_select::atan((const_math::max_fp_type)val)) \
F2(atan2, constexpr, (scalar_type)const_select::atan2((const_math::max_fp_type)lhs, (const_math::max_fp_type)rhs)) \
F1(exp, constexpr, (scalar_type)const_select::exp((const_math::max_fp_type)val)) \
F1(log, constexpr, (scalar_type)const_select::log((const_math::max_fp_type)val)) \
F3(fma, constexpr, ((a * b) + c)) \
F2_INT(bit_and, constexpr, (lhs & rhs)) \
F2_INT(bit_or, constexpr, (lhs | rhs)) \
F2_INT(bit_xor, constexpr, (lhs ^ rhs)) \
F2_INT(bit_left_shift, constexpr, (lhs << rhs)) \
F2_INT(bit_right_shift, constexpr, (lhs >> rhs)) \
F1(unary_not, constexpr, (!val)) \
F1(unary_complement, constexpr, (~val)) \
)

#define FLOOR_VH_IMPL_DEF_SIZE_T(F1, F2, F2_INT, F3) FLOOR_VH_IMPL(size_t, \
F2(modulo, constexpr, lhs % rhs) \
F1(sqrt, constexpr, (scalar_type)const_select::sqrt((const_math::max_fp_type)val)) \
F1(inv_sqrt, constexpr, val) \
F1(abs, constexpr, val) \
F1(floor, constexpr, val) \
F1(ceil, constexpr, val) \
F1(round, constexpr, val) \
F1(trunc, constexpr, val) \
F1(rint, constexpr, val) \
F1(sin, constexpr, (scalar_type)const_select::sin((const_math::max_fp_type)val)) \
F1(cos, constexpr, (scalar_type)const_select::cos((const_math::max_fp_type)val)) \
F1(tan, constexpr, (scalar_type)const_select::tan((const_math::max_fp_type)val)) \
F1(asin, constexpr, (scalar_type)const_select::asin((const_math::max_fp_type)val)) \
F1(acos, constexpr, (scalar_type)const_select::acos((const_math::max_fp_type)val)) \
F1(atan, constexpr, (scalar_type)const_select::atan((const_math::max_fp_type)val)) \
F2(atan2, constexpr, (scalar_type)const_select::atan2((const_math::max_fp_type)lhs, (const_math::max_fp_type)rhs)) \
F1(exp, constexpr, (scalar_type)const_select::exp((const_math::max_fp_type)val)) \
F1(log, constexpr, (scalar_type)const_select::log((const_math::max_fp_type)val)) \
F3(fma, constexpr, ((a * b) + c)) \
F2_INT(bit_and, constexpr, (lhs & rhs)) \
F2_INT(bit_or, constexpr, (lhs | rhs)) \
F2_INT(bit_xor, constexpr, (lhs ^ rhs)) \
F2_INT(bit_left_shift, constexpr, (lhs << rhs)) \
F2_INT(bit_right_shift, constexpr, (lhs >> rhs)) \
F1(unary_not, constexpr, (!val)) \
F1(unary_complement, constexpr, (~val)) \
)

#define FLOOR_VH_IMPL_DEF_INT128(F1, F2, F2_INT, F3) FLOOR_VH_IMPL(__int128_t, \
F2(modulo, constexpr, lhs % rhs) \
F1(sqrt, constexpr, (scalar_type)const_select::sqrt((const_math::max_fp_type)val)) \
F1(inv_sqrt, constexpr, val) \
F1(abs, constexpr, const_math::abs(val)) \
F1(floor, constexpr, val) \
F1(ceil, constexpr, val) \
F1(round, constexpr, val) \
F1(trunc, constexpr, val) \
F1(rint, constexpr, val) \
F1(sin, constexpr, (scalar_type)const_select::sin((const_math::max_fp_type)val)) \
F1(cos, constexpr, (scalar_type)const_select::cos((const_math::max_fp_type)val)) \
F1(tan, constexpr, (scalar_type)const_select::tan((const_math::max_fp_type)val)) \
F1(asin, constexpr, (scalar_type)const_select::asin((const_math::max_fp_type)val)) \
F1(acos, constexpr, (scalar_type)const_select::acos((const_math::max_fp_type)val)) \
F1(atan, constexpr, (scalar_type)const_select::atan((const_math::max_fp_type)val)) \
F2(atan2, constexpr, (scalar_type)const_select::atan2((const_math::max_fp_type)lhs, (const_math::max_fp_type)rhs)) \
F1(exp, constexpr, (scalar_type)const_select::exp((const_math::max_fp_type)val)) \
F1(log, constexpr, (scalar_type)const_select::log((const_math::max_fp_type)val)) \
F3(fma, constexpr, ((a * b) + c)) \
F2_INT(bit_and, constexpr, (lhs & rhs)) \
F2_INT(bit_or, constexpr, (lhs | rhs)) \
F2_INT(bit_xor, constexpr, (lhs ^ rhs)) \
F2_INT(bit_left_shift, constexpr, (lhs << rhs)) \
F2_INT(bit_right_shift, constexpr, (lhs >> rhs)) \
F1(unary_not, constexpr, (!val)) \
F1(unary_complement, constexpr, (~val)) \
)

#define FLOOR_VH_IMPL_DEF_UINT128(F1, F2, F2_INT, F3) FLOOR_VH_IMPL(__uint128_t, \
F2(modulo, constexpr, lhs % rhs) \
F1(sqrt, constexpr, (scalar_type)const_select::sqrt((const_math::max_fp_type)val)) \
F1(inv_sqrt, constexpr, val) \
F1(abs, constexpr, val) \
F1(floor, constexpr, val) \
F1(ceil, constexpr, val) \
F1(round, constexpr, val) \
F1(trunc, constexpr, val) \
F1(rint, constexpr, val) \
F1(sin, constexpr, (scalar_type)const_select::sin((const_math::max_fp_type)val)) \
F1(cos, constexpr, (scalar_type)const_select::cos((const_math::max_fp_type)val)) \
F1(tan, constexpr, (scalar_type)const_select::tan((const_math::max_fp_type)val)) \
F1(asin, constexpr, (scalar_type)const_select::asin((const_math::max_fp_type)val)) \
F1(acos, constexpr, (scalar_type)const_select::acos((const_math::max_fp_type)val)) \
F1(atan, constexpr, (scalar_type)const_select::atan((const_math::max_fp_type)val)) \
F2(atan2, constexpr, (scalar_type)const_select::atan2((const_math::max_fp_type)lhs, (const_math::max_fp_type)rhs)) \
F1(exp, constexpr, (scalar_type)const_select::exp((const_math::max_fp_type)val)) \
F1(log, constexpr, (scalar_type)const_select::log((const_math::max_fp_type)val)) \
F3(fma, constexpr, ((a * b) + c)) \
F2_INT(bit_and, constexpr, (lhs & rhs)) \
F2_INT(bit_or, constexpr, (lhs | rhs)) \
F2_INT(bit_xor, constexpr, (lhs ^ rhs)) \
F2_INT(bit_left_shift, constexpr, (lhs << rhs)) \
F2_INT(bit_right_shift, constexpr, (lhs >> rhs)) \
F1(unary_not, constexpr, (!val)) \
F1(unary_complement, constexpr, (~val)) \
)

#define FLOOR_VH_IMPL_DEF_BOOL(F1, F2, F2_INT, F3) FLOOR_VH_IMPL(bool, \
F2(modulo, constexpr, lhs % rhs) \
F1(sqrt, constexpr, val) \
F1(inv_sqrt, constexpr, val) \
F1(abs, constexpr, val) \
F1(floor, constexpr, val) \
F1(ceil, constexpr, val) \
F1(round, constexpr, val) \
F1(trunc, constexpr, val) \
F1(rint, constexpr, val) \
F1(sin, constexpr, val) \
F1(cos, constexpr, val) \
F1(tan, constexpr, val) \
F1(asin, constexpr, val) \
F1(acos, constexpr, val) \
F1(atan, constexpr, val) \
F2(atan2, constexpr, lhs & rhs) \
F1(exp, constexpr, val) \
F1(log, constexpr, val) \
F3(fma, constexpr, ((a * b) + c)) \
F2_INT(bit_and, constexpr, (lhs & rhs)) \
F2_INT(bit_or, constexpr, (lhs | rhs)) \
F2_INT(bit_xor, constexpr, (lhs ^ rhs)) \
F2_INT(bit_left_shift, constexpr, (lhs << rhs)) \
F2_INT(bit_right_shift, constexpr, (lhs >> rhs)) \
F1(unary_not, constexpr, (!val)) \
F1(unary_complement, constexpr, (~val)) \
)

FLOOR_VH_IMPL_DEF_FLOAT(FLOOR_VH_FUNC_IMPL_1, FLOOR_VH_FUNC_IMPL_2, FLOOR_VH_FUNC_IMPL_2_INT, FLOOR_VH_FUNC_IMPL_3)
#if !defined(FLOOR_COMPUTE_NO_DOUBLE)
FLOOR_VH_IMPL_DEF_DOUBLE(FLOOR_VH_FUNC_IMPL_1, FLOOR_VH_FUNC_IMPL_2, FLOOR_VH_FUNC_IMPL_2_INT, FLOOR_VH_FUNC_IMPL_3)
#endif
#if !defined(FLOOR_COMPUTE)
FLOOR_VH_IMPL_DEF_LDOUBLE(FLOOR_VH_FUNC_IMPL_1, FLOOR_VH_FUNC_IMPL_2, FLOOR_VH_FUNC_IMPL_2_INT, FLOOR_VH_FUNC_IMPL_3)
#endif
FLOOR_VH_IMPL_DEF_INT32(FLOOR_VH_FUNC_IMPL_1, FLOOR_VH_FUNC_IMPL_2, FLOOR_VH_FUNC_IMPL_2_INT, FLOOR_VH_FUNC_IMPL_3)
FLOOR_VH_IMPL_DEF_UINT32(FLOOR_VH_FUNC_IMPL_1, FLOOR_VH_FUNC_IMPL_2, FLOOR_VH_FUNC_IMPL_2_INT, FLOOR_VH_FUNC_IMPL_3)
FLOOR_VH_IMPL_DEF_INT8(FLOOR_VH_FUNC_IMPL_1, FLOOR_VH_FUNC_IMPL_2, FLOOR_VH_FUNC_IMPL_2_INT, FLOOR_VH_FUNC_IMPL_3)
FLOOR_VH_IMPL_DEF_UINT8(FLOOR_VH_FUNC_IMPL_1, FLOOR_VH_FUNC_IMPL_2, FLOOR_VH_FUNC_IMPL_2_INT, FLOOR_VH_FUNC_IMPL_3)
FLOOR_VH_IMPL_DEF_INT16(FLOOR_VH_FUNC_IMPL_1, FLOOR_VH_FUNC_IMPL_2, FLOOR_VH_FUNC_IMPL_2_INT, FLOOR_VH_FUNC_IMPL_3)
FLOOR_VH_IMPL_DEF_UINT16(FLOOR_VH_FUNC_IMPL_1, FLOOR_VH_FUNC_IMPL_2, FLOOR_VH_FUNC_IMPL_2_INT, FLOOR_VH_FUNC_IMPL_3)
FLOOR_VH_IMPL_DEF_INT64(FLOOR_VH_FUNC_IMPL_1, FLOOR_VH_FUNC_IMPL_2, FLOOR_VH_FUNC_IMPL_2_INT, FLOOR_VH_FUNC_IMPL_3)
FLOOR_VH_IMPL_DEF_UINT64(FLOOR_VH_FUNC_IMPL_1, FLOOR_VH_FUNC_IMPL_2, FLOOR_VH_FUNC_IMPL_2_INT, FLOOR_VH_FUNC_IMPL_3)
#if !defined(FLOOR_COMPUTE)
FLOOR_VH_IMPL_DEF_INT128(FLOOR_VH_FUNC_IMPL_1, FLOOR_VH_FUNC_IMPL_2, FLOOR_VH_FUNC_IMPL_2_INT, FLOOR_VH_FUNC_IMPL_3)
FLOOR_VH_IMPL_DEF_UINT128(FLOOR_VH_FUNC_IMPL_1, FLOOR_VH_FUNC_IMPL_2, FLOOR_VH_FUNC_IMPL_2_INT, FLOOR_VH_FUNC_IMPL_3)
#endif
FLOOR_VH_IMPL_DEF_BOOL(FLOOR_VH_FUNC_IMPL_1, FLOOR_VH_FUNC_IMPL_2, FLOOR_VH_FUNC_IMPL_2_INT, FLOOR_VH_FUNC_IMPL_3)

#if defined(__APPLE__)
FLOOR_VH_IMPL_DEF_SSIZE_T(FLOOR_VH_FUNC_IMPL_1, FLOOR_VH_FUNC_IMPL_2, FLOOR_VH_FUNC_IMPL_2_INT, FLOOR_VH_FUNC_IMPL_3)
#endif
#if defined(__APPLE__) || (defined(FLOOR_COMPUTE_CUDA) && !(__clang_major__ == 3 && __clang_minor__ < 7))
FLOOR_VH_IMPL_DEF_SIZE_T(FLOOR_VH_FUNC_IMPL_1, FLOOR_VH_FUNC_IMPL_2, FLOOR_VH_FUNC_IMPL_2_INT, FLOOR_VH_FUNC_IMPL_3)
#endif

#endif
