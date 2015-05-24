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

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-macros"
#endif

// this is included by the compute (opencl/cuda/metal) compiler -> need a header guard just in case
#ifndef __FLOOR_CONST_MATH_CPP__
#define __FLOOR_CONST_MATH_CPP__

#include <floor/constexpr/const_math.hpp>

#if !defined(_MSC_VER) // name mangling issues
namespace const_select {
	// generic is_constexpr checks
#define FLOOR_IS_CONSTEXPR(type_name) \
	__attribute__((always_inline)) bool is_constexpr(type_name) { \
		return false; \
	}
	
	FLOOR_IS_CONSTEXPR(bool)
	FLOOR_IS_CONSTEXPR(int)
	FLOOR_IS_CONSTEXPR(size_t)
	FLOOR_IS_CONSTEXPR(float)
#if !defined(FLOOR_COMPUTE_NO_DOUBLE)
	FLOOR_IS_CONSTEXPR(double)
#endif
	
	// const math select functions
#define FLOOR_CONST_MATH_SELECT(func_name, rt_func, type_name, type_suffix) \
	__attribute__((always_inline)) type_name func_name (type_name val) { \
		return rt_func ; \
	}

#define FLOOR_CONST_MATH_SELECT_2(func_name, rt_func, type_name, type_suffix) \
	__attribute__((always_inline)) type_name func_name (type_name y, type_name x) { \
		return rt_func ; \
	}
	
#define FLOOR_CONST_MATH_SELECT_3(func_name, rt_func, type_name, type_suffix) \
	__attribute__((always_inline)) type_name func_name (type_name a, type_name b, type_name c) { \
		return rt_func ; \
	}
	
	FLOOR_CONST_MATH_SELECT_2(fmod, std::fmod(y, x), float, "f")
	FLOOR_CONST_MATH_SELECT(sqrt, std::sqrt(val), float, "f")
	FLOOR_CONST_MATH_SELECT(inv_sqrt, const_math::native_rsqrt(val), float, "f")
	FLOOR_CONST_MATH_SELECT(abs, std::fabs(val), float, "f")
	FLOOR_CONST_MATH_SELECT(floor, std::floor(val), float, "f")
	FLOOR_CONST_MATH_SELECT(ceil, std::ceil(val), float, "f")
	FLOOR_CONST_MATH_SELECT(round, std::round(val), float, "f")
	FLOOR_CONST_MATH_SELECT(trunc, std::trunc(val), float, "f")
	FLOOR_CONST_MATH_SELECT(rint, std::rint(val), float, "f")
	FLOOR_CONST_MATH_SELECT(sin, std::sin(val), float, "f")
	FLOOR_CONST_MATH_SELECT(cos, std::cos(val), float, "f")
	FLOOR_CONST_MATH_SELECT(tan, std::tan(val), float, "f")
	FLOOR_CONST_MATH_SELECT(asin, std::asin(val), float, "f")
	FLOOR_CONST_MATH_SELECT(acos, std::acos(val), float, "f")
	FLOOR_CONST_MATH_SELECT(atan, std::atan(val), float, "f")
	FLOOR_CONST_MATH_SELECT_2(atan2, std::atan2(y, x), float, "f")
	FLOOR_CONST_MATH_SELECT_3(fma, const_math::native_fma(a, b, c), float, "f")
	FLOOR_CONST_MATH_SELECT(exp, std::exp(val), float, "f")
	FLOOR_CONST_MATH_SELECT(exp2, std::exp2(val), float, "f")
	FLOOR_CONST_MATH_SELECT(log, std::log(val), float, "f")
	FLOOR_CONST_MATH_SELECT(log2, std::log2(val), float, "f")
	
#if !defined(FLOOR_COMPUTE_NO_DOUBLE)
	FLOOR_CONST_MATH_SELECT_2(fmod, std::fmod(y, x), double, "d")
	FLOOR_CONST_MATH_SELECT(sqrt, std::sqrt(val), double, "d")
	FLOOR_CONST_MATH_SELECT(inv_sqrt, const_math::native_rsqrt(val), double, "d")
	FLOOR_CONST_MATH_SELECT(abs, std::fabs(val), double, "d")
	FLOOR_CONST_MATH_SELECT(floor, std::floor(val), double, "d")
	FLOOR_CONST_MATH_SELECT(ceil, std::ceil(val), double, "d")
	FLOOR_CONST_MATH_SELECT(round, std::round(val), double, "d")
	FLOOR_CONST_MATH_SELECT(trunc, std::trunc(val), double, "d")
	FLOOR_CONST_MATH_SELECT(rint, std::rint(val), double, "d")
	FLOOR_CONST_MATH_SELECT(sin, std::sin(val), double, "d")
	FLOOR_CONST_MATH_SELECT(cos, std::cos(val), double, "d")
	FLOOR_CONST_MATH_SELECT(tan, std::tan(val), double, "d")
	FLOOR_CONST_MATH_SELECT(asin, std::asin(val), double, "d")
	FLOOR_CONST_MATH_SELECT(acos, std::acos(val), double, "d")
	FLOOR_CONST_MATH_SELECT(atan, std::atan(val), double, "d")
	FLOOR_CONST_MATH_SELECT_2(atan2, std::atan2(y, x), double, "d")
	FLOOR_CONST_MATH_SELECT_3(fma, const_math::native_fma(a, b, c), double, "d")
	FLOOR_CONST_MATH_SELECT(exp, std::exp(val), double, "d")
	FLOOR_CONST_MATH_SELECT(exp2, std::exp2(val), double, "d")
	FLOOR_CONST_MATH_SELECT(log, std::log(val), double, "d")
	FLOOR_CONST_MATH_SELECT(log2, std::log2(val), double, "d")
#endif
	
#if !defined(FLOOR_COMPUTE)
	FLOOR_CONST_MATH_SELECT_2(fmod, std::fmodl(y, x), long double, "l")
	FLOOR_CONST_MATH_SELECT(sqrt, std::sqrtl(val), long double, "l")
	FLOOR_CONST_MATH_SELECT(inv_sqrt, const_math::native_rsqrt(val), long double, "l")
	FLOOR_CONST_MATH_SELECT(abs, std::fabsl(val), long double, "l")
	FLOOR_CONST_MATH_SELECT(floor, std::floorl(val), long double, "l")
	FLOOR_CONST_MATH_SELECT(ceil, std::ceill(val), long double, "l")
	FLOOR_CONST_MATH_SELECT(round, std::roundl(val), long double, "l")
	FLOOR_CONST_MATH_SELECT(trunc, std::truncl(val), long double, "l")
	FLOOR_CONST_MATH_SELECT(rint, std::rintl(val), long double, "l")
	FLOOR_CONST_MATH_SELECT(sin, std::sinl(val), long double, "l")
	FLOOR_CONST_MATH_SELECT(cos, std::cosl(val), long double, "l")
	FLOOR_CONST_MATH_SELECT(tan, std::tanl(val), long double, "l")
	FLOOR_CONST_MATH_SELECT(asin, std::asinl(val), long double, "l")
	FLOOR_CONST_MATH_SELECT(acos, std::acosl(val), long double, "l")
	FLOOR_CONST_MATH_SELECT(atan, std::atanl(val), long double, "l")
	FLOOR_CONST_MATH_SELECT_2(atan2, std::atan2l(y, x), long double, "l")
	FLOOR_CONST_MATH_SELECT_3(fma, const_math::native_fma(a, b, c), long double, "l")
	FLOOR_CONST_MATH_SELECT(exp, std::expl(val), long double, "l")
	FLOOR_CONST_MATH_SELECT(exp2, std::exp2(val), long double, "l")
	FLOOR_CONST_MATH_SELECT(log, std::logl(val), long double, "l")
	FLOOR_CONST_MATH_SELECT(log2, std::log2(val), long double, "l")
#endif

#undef FLOOR_IS_CONSTEXPR
#undef FLOOR_CONST_MATH_SELECT
#undef FLOOR_CONST_MATH_SELECT_2
#undef FLOOR_CONST_MATH_SELECT_3
	
}
#endif

#endif
