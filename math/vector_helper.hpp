/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2014 Florian Ziesche
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
#include <cstdint>
#include "constexpr/const_math.hpp"
using namespace std;

//! base class
template <typename T> class vector_helper {
public:
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
	static constexpr T fma(const T& a, const T& b, const T& c);
	
	// TODO: inc, dec, bit ops (&, |, ^, ~, !), I/O?
	
protected:
	// static class
	vector_helper(const vector_helper&) = delete;
	~vector_helper() = delete;
	vector_helper& operator=(const vector_helper&) = delete;
	
};

// if bool is a macro for whatever insane reasons, undefine it, because it causes issues here
#if defined(bool)
#undef bool
#endif

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
//! macro that defines a static "func_constexpr" function, taking three arguments with the name "func_name"
#define FLOOR_VH_FUNC_IMPL_3(func_name, func_constexpr, func_impl) \
static func_constexpr scalar_type func_name (const scalar_type& a, const scalar_type& b, const scalar_type& c) { \
	return func_impl; \
}

//! implements the vector_helper class for a specific "vh_type" and it's function implementations
#define FLOOR_VH_IMPL(vh_type, vh_sign_type, func_impl) \
template <> class vector_helper<vh_type> { \
public: \
	typedef vh_type scalar_type; \
	typedef vh_sign_type signed_type; \
	static constexpr const scalar_type scalar_zero { (scalar_type)0 }; \
	static constexpr const scalar_type scalar_one { (scalar_type)1 }; \
	static constexpr const scalar_type scalar_nan { numeric_limits<scalar_type>::quiet_NaN() }; \
	static constexpr const scalar_type scalar_inf { numeric_limits<scalar_type>::infinity() }; \
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

#define FLOOR_VH_IMPL_DEF_FLOAT(F1, F2, F3) FLOOR_VH_IMPL(float, float, \
F2(modulo, constexpr, const_math_select::fmod(lhs, rhs)) \
F1(sqrt, constexpr, const_math_select::sqrt(val)) \
F1(inv_sqrt, constexpr, const_math_select::inv_sqrt(val)) \
F1(abs, constexpr, const_math_select::abs(val)) \
F1(floor, constexpr, const_math_select::floor(val)) \
F1(ceil, constexpr, const_math_select::ceil(val)) \
F1(round, constexpr, const_math_select::round(val)) \
F1(trunc, constexpr, const_math_select::trunc(val)) \
F1(rint, constexpr, const_math_select::rint(val)) \
F1(sin, constexpr, const_math_select::sin(val)) \
F1(cos, constexpr, const_math_select::cos(val)) \
F1(tan, constexpr, const_math_select::tan(val)) \
F1(asin, constexpr, const_math_select::asin(val)) \
F1(acos, constexpr, const_math_select::acos(val)) \
F1(atan, constexpr, const_math_select::atan(val)) \
F2(atan2, constexpr, const_math_select::atan2(lhs, rhs)) \
F3(fma, constexpr, const_math_select::fma(a, b, c)) \
)

#define FLOOR_VH_IMPL_DEF_DOUBLE(F1, F2, F3) FLOOR_VH_IMPL(double, double, \
F2(modulo, constexpr, const_math_select::fmod(lhs, rhs)) \
F1(sqrt, constexpr, const_math_select::sqrt(val)) \
F1(inv_sqrt, constexpr, const_math_select::inv_sqrt(val)) \
F1(abs, constexpr, const_math_select::abs(val)) \
F1(floor, constexpr, const_math_select::floor(val)) \
F1(ceil, constexpr, const_math_select::ceil(val)) \
F1(round, constexpr, const_math_select::round(val)) \
F1(trunc, constexpr, const_math_select::trunc(val)) \
F1(rint, constexpr, const_math_select::rint(val)) \
F1(sin, constexpr, const_math_select::sin(val)) \
F1(cos, constexpr, const_math_select::cos(val)) \
F1(tan, constexpr, const_math_select::tan(val)) \
F1(asin, constexpr, const_math_select::asin(val)) \
F1(acos, constexpr, const_math_select::acos(val)) \
F1(atan, constexpr, const_math_select::atan(val)) \
F2(atan2, constexpr, const_math_select::atan2(lhs, rhs)) \
F3(fma, constexpr, const_math_select::fma(a, b, c)) \
)

#define FLOOR_VH_IMPL_DEF_LDOUBLE(F1, F2, F3) FLOOR_VH_IMPL(long double, long double, \
F2(modulo, constexpr, const_math_select::fmod(lhs, rhs)) \
F1(sqrt, constexpr, const_math_select::sqrt(val)) \
F1(inv_sqrt, constexpr, const_math_select::inv_sqrt(val)) \
F1(abs, constexpr, const_math_select::abs(val)) \
F1(floor, constexpr, const_math_select::floor(val)) \
F1(ceil, constexpr, const_math_select::ceil(val)) \
F1(round, constexpr, const_math_select::round(val)) \
F1(trunc, constexpr, const_math_select::trunc(val)) \
F1(rint, constexpr, const_math_select::rint(val)) \
F1(sin, constexpr, const_math_select::sin(val)) \
F1(cos, constexpr, const_math_select::cos(val)) \
F1(tan, constexpr, const_math_select::tan(val)) \
F1(asin, constexpr, const_math_select::asin(val)) \
F1(acos, constexpr, const_math_select::acos(val)) \
F1(atan, constexpr, const_math_select::atan(val)) \
F2(atan2, constexpr, const_math_select::atan2(lhs, rhs)) \
F3(fma, constexpr, const_math_select::fma(a, b, c)) \
)

#define FLOOR_VH_IMPL_DEF_INT32(F1, F2, F3) FLOOR_VH_IMPL(int32_t, int32_t, \
F2(modulo, constexpr, lhs % rhs) \
F1(sqrt, constexpr, (scalar_type)const_math_select::sqrt((long double)val)) \
F1(inv_sqrt, constexpr, val) \
F1(abs, constexpr, const_math::abs(val)) \
F1(floor, constexpr, val) \
F1(ceil, constexpr, val) \
F1(round, constexpr, val) \
F1(trunc, constexpr, val) \
F1(rint, constexpr, val) \
F1(sin, constexpr, (scalar_type)const_math_select::sin((long double)val)) \
F1(cos, constexpr, (scalar_type)const_math_select::cos((long double)val)) \
F1(tan, constexpr, (scalar_type)const_math_select::tan((long double)val)) \
F1(asin, constexpr, (scalar_type)const_math_select::asin((long double)val)) \
F1(acos, constexpr, (scalar_type)const_math_select::acos((long double)val)) \
F1(atan, constexpr, (scalar_type)const_math_select::atan((long double)val)) \
F2(atan2, constexpr, (scalar_type)const_math_select::atan2((long double)lhs, (long double)rhs)) \
F3(fma, constexpr, ((a * b) + c)) \
)

#define FLOOR_VH_IMPL_DEF_UINT32(F1, F2, F3) FLOOR_VH_IMPL(uint32_t, int32_t, \
F2(modulo, constexpr, lhs % rhs) \
F1(sqrt, constexpr, (scalar_type)const_math_select::sqrt((long double)val)) \
F1(inv_sqrt, constexpr, val) \
F1(abs, constexpr, val) \
F1(floor, constexpr, val) \
F1(ceil, constexpr, val) \
F1(round, constexpr, val) \
F1(trunc, constexpr, val) \
F1(rint, constexpr, val) \
F1(sin, constexpr, (scalar_type)const_math_select::sin((long double)val)) \
F1(cos, constexpr, (scalar_type)const_math_select::cos((long double)val)) \
F1(tan, constexpr, (scalar_type)const_math_select::tan((long double)val)) \
F1(asin, constexpr, (scalar_type)const_math_select::asin((long double)val)) \
F1(acos, constexpr, (scalar_type)const_math_select::acos((long double)val)) \
F1(atan, constexpr, (scalar_type)const_math_select::atan((long double)val)) \
F2(atan2, constexpr, (scalar_type)const_math_select::atan2((long double)lhs, (long double)rhs)) \
F3(fma, constexpr, ((a * b) + c)) \
)

#define FLOOR_VH_IMPL_DEF_INT8(F1, F2, F3) FLOOR_VH_IMPL(int8_t, int8_t, \
F2(modulo, constexpr, lhs % rhs) \
F1(sqrt, constexpr, (scalar_type)const_math_select::sqrt((long double)val)) \
F1(inv_sqrt, constexpr, val) \
F1(abs, constexpr, const_math::abs(val)) \
F1(floor, constexpr, val) \
F1(ceil, constexpr, val) \
F1(round, constexpr, val) \
F1(trunc, constexpr, val) \
F1(rint, constexpr, val) \
F1(sin, constexpr, (scalar_type)const_math_select::sin((long double)val)) \
F1(cos, constexpr, (scalar_type)const_math_select::cos((long double)val)) \
F1(tan, constexpr, (scalar_type)const_math_select::tan((long double)val)) \
F1(asin, constexpr, (scalar_type)const_math_select::asin((long double)val)) \
F1(acos, constexpr, (scalar_type)const_math_select::acos((long double)val)) \
F1(atan, constexpr, (scalar_type)const_math_select::atan((long double)val)) \
F2(atan2, constexpr, (scalar_type)const_math_select::atan2((long double)lhs, (long double)rhs)) \
F3(fma, constexpr, ((a * b) + c)) \
)

#define FLOOR_VH_IMPL_DEF_UINT8(F1, F2, F3) FLOOR_VH_IMPL(uint8_t, int8_t, \
F2(modulo, constexpr, lhs % rhs) \
F1(sqrt, constexpr, (scalar_type)const_math_select::sqrt((long double)val)) \
F1(inv_sqrt, constexpr, val) \
F1(abs, constexpr, val) \
F1(floor, constexpr, val) \
F1(ceil, constexpr, val) \
F1(round, constexpr, val) \
F1(trunc, constexpr, val) \
F1(rint, constexpr, val) \
F1(sin, constexpr, (scalar_type)const_math_select::sin((long double)val)) \
F1(cos, constexpr, (scalar_type)const_math_select::cos((long double)val)) \
F1(tan, constexpr, (scalar_type)const_math_select::tan((long double)val)) \
F1(asin, constexpr, (scalar_type)const_math_select::asin((long double)val)) \
F1(acos, constexpr, (scalar_type)const_math_select::acos((long double)val)) \
F1(atan, constexpr, (scalar_type)const_math_select::atan((long double)val)) \
F2(atan2, constexpr, (scalar_type)const_math_select::atan2((long double)lhs, (long double)rhs)) \
F3(fma, constexpr, ((a * b) + c)) \
)

#define FLOOR_VH_IMPL_DEF_INT16(F1, F2, F3) FLOOR_VH_IMPL(int16_t, int16_t, \
F2(modulo, constexpr, lhs % rhs) \
F1(sqrt, constexpr, (scalar_type)const_math_select::sqrt((long double)val)) \
F1(inv_sqrt, constexpr, val) \
F1(abs, constexpr, const_math::abs(val)) \
F1(floor, constexpr, val) \
F1(ceil, constexpr, val) \
F1(round, constexpr, val) \
F1(trunc, constexpr, val) \
F1(rint, constexpr, val) \
F1(sin, constexpr, (scalar_type)const_math_select::sin((long double)val)) \
F1(cos, constexpr, (scalar_type)const_math_select::cos((long double)val)) \
F1(tan, constexpr, (scalar_type)const_math_select::tan((long double)val)) \
F1(asin, constexpr, (scalar_type)const_math_select::asin((long double)val)) \
F1(acos, constexpr, (scalar_type)const_math_select::acos((long double)val)) \
F1(atan, constexpr, (scalar_type)const_math_select::atan((long double)val)) \
F2(atan2, constexpr, (scalar_type)const_math_select::atan2((long double)lhs, (long double)rhs)) \
F3(fma, constexpr, ((a * b) + c)) \
)

#define FLOOR_VH_IMPL_DEF_UINT16(F1, F2, F3) FLOOR_VH_IMPL(uint16_t, int16_t, \
F2(modulo, constexpr, lhs % rhs) \
F1(sqrt, constexpr, (scalar_type)const_math_select::sqrt((long double)val)) \
F1(inv_sqrt, constexpr, val) \
F1(abs, constexpr, val) \
F1(floor, constexpr, val) \
F1(ceil, constexpr, val) \
F1(round, constexpr, val) \
F1(trunc, constexpr, val) \
F1(rint, constexpr, val) \
F1(sin, constexpr, (scalar_type)const_math_select::sin((long double)val)) \
F1(cos, constexpr, (scalar_type)const_math_select::cos((long double)val)) \
F1(tan, constexpr, (scalar_type)const_math_select::tan((long double)val)) \
F1(asin, constexpr, (scalar_type)const_math_select::asin((long double)val)) \
F1(acos, constexpr, (scalar_type)const_math_select::acos((long double)val)) \
F1(atan, constexpr, (scalar_type)const_math_select::atan((long double)val)) \
F2(atan2, constexpr, (scalar_type)const_math_select::atan2((long double)lhs, (long double)rhs)) \
F3(fma, constexpr, ((a * b) + c)) \
)

#define FLOOR_VH_IMPL_DEF_INT64(F1, F2, F3) FLOOR_VH_IMPL(int64_t, int64_t, \
F2(modulo, constexpr, lhs % rhs) \
F1(sqrt, constexpr, (scalar_type)const_math_select::sqrt((long double)val)) \
F1(inv_sqrt, constexpr, val) \
F1(abs, constexpr, const_math::abs(val)) \
F1(floor, constexpr, val) \
F1(ceil, constexpr, val) \
F1(round, constexpr, val) \
F1(trunc, constexpr, val) \
F1(rint, constexpr, val) \
F1(sin, constexpr, (scalar_type)const_math_select::sin((long double)val)) \
F1(cos, constexpr, (scalar_type)const_math_select::cos((long double)val)) \
F1(tan, constexpr, (scalar_type)const_math_select::tan((long double)val)) \
F1(asin, constexpr, (scalar_type)const_math_select::asin((long double)val)) \
F1(acos, constexpr, (scalar_type)const_math_select::acos((long double)val)) \
F1(atan, constexpr, (scalar_type)const_math_select::atan((long double)val)) \
F2(atan2, constexpr, (scalar_type)const_math_select::atan2((long double)lhs, (long double)rhs)) \
F3(fma, constexpr, ((a * b) + c)) \
)

#define FLOOR_VH_IMPL_DEF_UINT64(F1, F2, F3) FLOOR_VH_IMPL(uint64_t, int64_t, \
F2(modulo, constexpr, lhs % rhs) \
F1(sqrt, constexpr, (scalar_type)const_math_select::sqrt((long double)val)) \
F1(inv_sqrt, constexpr, val) \
F1(abs, constexpr, val) \
F1(floor, constexpr, val) \
F1(ceil, constexpr, val) \
F1(round, constexpr, val) \
F1(trunc, constexpr, val) \
F1(rint, constexpr, val) \
F1(sin, constexpr, (scalar_type)const_math_select::sin((long double)val)) \
F1(cos, constexpr, (scalar_type)const_math_select::cos((long double)val)) \
F1(tan, constexpr, (scalar_type)const_math_select::tan((long double)val)) \
F1(asin, constexpr, (scalar_type)const_math_select::asin((long double)val)) \
F1(acos, constexpr, (scalar_type)const_math_select::acos((long double)val)) \
F1(atan, constexpr, (scalar_type)const_math_select::atan((long double)val)) \
F2(atan2, constexpr, (scalar_type)const_math_select::atan2((long double)lhs, (long double)rhs)) \
F3(fma, constexpr, ((a * b) + c)) \
)

#define FLOOR_VH_IMPL_DEF_SSIZE_T(F1, F2, F3) FLOOR_VH_IMPL(ssize_t, ssize_t, \
F2(modulo, constexpr, lhs % rhs) \
F1(sqrt, constexpr, (scalar_type)const_math_select::sqrt((long double)val)) \
F1(inv_sqrt, constexpr, val) \
F1(abs, constexpr, const_math::abs(val)) \
F1(floor, constexpr, val) \
F1(ceil, constexpr, val) \
F1(round, constexpr, val) \
F1(trunc, constexpr, val) \
F1(rint, constexpr, val) \
F1(sin, constexpr, (scalar_type)const_math_select::sin((long double)val)) \
F1(cos, constexpr, (scalar_type)const_math_select::cos((long double)val)) \
F1(tan, constexpr, (scalar_type)const_math_select::tan((long double)val)) \
F1(asin, constexpr, (scalar_type)const_math_select::asin((long double)val)) \
F1(acos, constexpr, (scalar_type)const_math_select::acos((long double)val)) \
F1(atan, constexpr, (scalar_type)const_math_select::atan((long double)val)) \
F2(atan2, constexpr, (scalar_type)const_math_select::atan2((long double)lhs, (long double)rhs)) \
F3(fma, constexpr, ((a * b) + c)) \
)

#define FLOOR_VH_IMPL_DEF_SIZE_T(F1, F2, F3) FLOOR_VH_IMPL(size_t, ssize_t, \
F2(modulo, constexpr, lhs % rhs) \
F1(sqrt, constexpr, (scalar_type)const_math_select::sqrt((long double)val)) \
F1(inv_sqrt, constexpr, val) \
F1(abs, constexpr, val) \
F1(floor, constexpr, val) \
F1(ceil, constexpr, val) \
F1(round, constexpr, val) \
F1(trunc, constexpr, val) \
F1(rint, constexpr, val) \
F1(sin, constexpr, (scalar_type)const_math_select::sin((long double)val)) \
F1(cos, constexpr, (scalar_type)const_math_select::cos((long double)val)) \
F1(tan, constexpr, (scalar_type)const_math_select::tan((long double)val)) \
F1(asin, constexpr, (scalar_type)const_math_select::asin((long double)val)) \
F1(acos, constexpr, (scalar_type)const_math_select::acos((long double)val)) \
F1(atan, constexpr, (scalar_type)const_math_select::atan((long double)val)) \
F2(atan2, constexpr, (scalar_type)const_math_select::atan2((long double)lhs, (long double)rhs)) \
F3(fma, constexpr, ((a * b) + c)) \
)

#define FLOOR_VH_IMPL_DEF_BOOL(F1, F2, F3) FLOOR_VH_IMPL(bool, bool, \
F2(modulo, constexpr, lhs % rhs) \
F1(sqrt, constexpr, val) \
F1(inv_sqrt, constexpr, val) \
F1(abs, constexpr, val) \
F1(floor, constexpr, val) \
F1(ceil, constexpr, val) \
F1(round, constexpr, val) \
F1(trunc, constexpr, val) \
F1(rint, constexpr, val) \
F1(sin, constexpr, (scalar_type)const_math_select::sin((long double)val)) \
F1(cos, constexpr, (scalar_type)const_math_select::cos((long double)val)) \
F1(tan, constexpr, (scalar_type)const_math_select::tan((long double)val)) \
F1(asin, constexpr, (scalar_type)const_math_select::asin((long double)val)) \
F1(acos, constexpr, (scalar_type)const_math_select::acos((long double)val)) \
F1(atan, constexpr, (scalar_type)const_math_select::atan((long double)val)) \
F2(atan2, constexpr, (scalar_type)const_math_select::atan2((long double)lhs, (long double)rhs)) \
F3(fma, constexpr, ((a * b) + c)) \
)

FLOOR_VH_IMPL_DEF_FLOAT(FLOOR_VH_FUNC_IMPL_1, FLOOR_VH_FUNC_IMPL_2, FLOOR_VH_FUNC_IMPL_3)
FLOOR_VH_IMPL_DEF_DOUBLE(FLOOR_VH_FUNC_IMPL_1, FLOOR_VH_FUNC_IMPL_2, FLOOR_VH_FUNC_IMPL_3)
FLOOR_VH_IMPL_DEF_LDOUBLE(FLOOR_VH_FUNC_IMPL_1, FLOOR_VH_FUNC_IMPL_2, FLOOR_VH_FUNC_IMPL_3)
FLOOR_VH_IMPL_DEF_INT32(FLOOR_VH_FUNC_IMPL_1, FLOOR_VH_FUNC_IMPL_2, FLOOR_VH_FUNC_IMPL_3)
FLOOR_VH_IMPL_DEF_UINT32(FLOOR_VH_FUNC_IMPL_1, FLOOR_VH_FUNC_IMPL_2, FLOOR_VH_FUNC_IMPL_3)
FLOOR_VH_IMPL_DEF_INT8(FLOOR_VH_FUNC_IMPL_1, FLOOR_VH_FUNC_IMPL_2, FLOOR_VH_FUNC_IMPL_3)
FLOOR_VH_IMPL_DEF_UINT8(FLOOR_VH_FUNC_IMPL_1, FLOOR_VH_FUNC_IMPL_2, FLOOR_VH_FUNC_IMPL_3)
FLOOR_VH_IMPL_DEF_INT16(FLOOR_VH_FUNC_IMPL_1, FLOOR_VH_FUNC_IMPL_2, FLOOR_VH_FUNC_IMPL_3)
FLOOR_VH_IMPL_DEF_UINT16(FLOOR_VH_FUNC_IMPL_1, FLOOR_VH_FUNC_IMPL_2, FLOOR_VH_FUNC_IMPL_3)
FLOOR_VH_IMPL_DEF_INT64(FLOOR_VH_FUNC_IMPL_1, FLOOR_VH_FUNC_IMPL_2, FLOOR_VH_FUNC_IMPL_3)
FLOOR_VH_IMPL_DEF_UINT64(FLOOR_VH_FUNC_IMPL_1, FLOOR_VH_FUNC_IMPL_2, FLOOR_VH_FUNC_IMPL_3)
FLOOR_VH_IMPL_DEF_BOOL(FLOOR_VH_FUNC_IMPL_1, FLOOR_VH_FUNC_IMPL_2, FLOOR_VH_FUNC_IMPL_3)

#if !defined(__GNU_LIBRARY__)
FLOOR_VH_IMPL_DEF_SSIZE_T(FLOOR_VH_FUNC_IMPL_1, FLOOR_VH_FUNC_IMPL_2, FLOOR_VH_FUNC_IMPL_3)
FLOOR_VH_IMPL_DEF_SIZE_T(FLOOR_VH_FUNC_IMPL_1, FLOOR_VH_FUNC_IMPL_2, FLOOR_VH_FUNC_IMPL_3)
#endif

#endif
