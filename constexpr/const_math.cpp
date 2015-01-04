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

// this is included by the compute (spir/opencl/cuda) compiler -> need a header guard just in case
#ifndef __FLOOR_CONST_MATH_CPP__
#define __FLOOR_CONST_MATH_CPP__

#include <floor/constexpr/const_math.hpp>

namespace const_math_select {
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
	
#if !defined(FLOOR_LLVM_COMPUTE)
	FLOOR_CONST_MATH_SELECT_2(fmod, std::fmodf(y, x), float, "f")
	FLOOR_CONST_MATH_SELECT(sqrt, std::sqrtf(val), float, "f")
	FLOOR_CONST_MATH_SELECT(inv_sqrt, (1.0f / std::sqrtf(val)), float, "f")
	FLOOR_CONST_MATH_SELECT(abs, std::fabsf(val), float, "f")
	FLOOR_CONST_MATH_SELECT(floor, std::floorf(val), float, "f")
	FLOOR_CONST_MATH_SELECT(ceil, std::ceilf(val), float, "f")
	FLOOR_CONST_MATH_SELECT(round, std::roundf(val), float, "f")
	FLOOR_CONST_MATH_SELECT(trunc, std::truncf(val), float, "f")
	FLOOR_CONST_MATH_SELECT(rint, std::rintf(val), float, "f")
	FLOOR_CONST_MATH_SELECT(sin, std::sinf(val), float, "f")
	FLOOR_CONST_MATH_SELECT(cos, std::cosf(val), float, "f")
	FLOOR_CONST_MATH_SELECT(tan, std::tanf(val), float, "f")
	FLOOR_CONST_MATH_SELECT(asin, std::asinf(val), float, "f")
	FLOOR_CONST_MATH_SELECT(acos, std::acosf(val), float, "f")
	FLOOR_CONST_MATH_SELECT(atan, std::atanf(val), float, "f")
	FLOOR_CONST_MATH_SELECT_2(atan2, std::atan2f(y, x), float, "f")
	FLOOR_CONST_MATH_SELECT_3(fma, __builtin_fmaf(a, b, c), float, "f")
	FLOOR_CONST_MATH_SELECT(exp, std::expf(val), float, "f")
	FLOOR_CONST_MATH_SELECT(log, std::logf(val), float, "f")
	
	FLOOR_CONST_MATH_SELECT_2(fmod, std::fmod(y, x), double, "d")
	FLOOR_CONST_MATH_SELECT(sqrt, std::sqrt(val), double, "d")
	FLOOR_CONST_MATH_SELECT(inv_sqrt, (1.0 / std::sqrt(val)), double, "d")
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
	FLOOR_CONST_MATH_SELECT_3(fma, __builtin_fma(a, b, c), double, "d")
	FLOOR_CONST_MATH_SELECT(exp, std::exp(val), double, "d")
	FLOOR_CONST_MATH_SELECT(log, std::log(val), double, "d")
	
	FLOOR_CONST_MATH_SELECT_2(fmod, std::fmodl(y, x), long double, "l")
	FLOOR_CONST_MATH_SELECT(sqrt, std::sqrtl(val), long double, "l")
	FLOOR_CONST_MATH_SELECT(inv_sqrt, (1.0L / std::sqrtl(val)), long double, "l")
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
	FLOOR_CONST_MATH_SELECT_3(fma, __builtin_fmal(a, b, c), long double, "l")
	FLOOR_CONST_MATH_SELECT(exp, std::expl(val), long double, "l")
	FLOOR_CONST_MATH_SELECT(log, std::logl(val), long double, "l")
#else
	// builtin functions defined by opencl/spir
	FLOOR_CONST_MATH_SELECT_2(fmod, fmod(y, x), float, "f")
	FLOOR_CONST_MATH_SELECT(sqrt, sqrt(val), float, "f")
	FLOOR_CONST_MATH_SELECT(inv_sqrt, rsqrt(val), float, "f")
	FLOOR_CONST_MATH_SELECT(abs, fabs(val), float, "f")
	FLOOR_CONST_MATH_SELECT(floor, floor(val), float, "f")
	FLOOR_CONST_MATH_SELECT(ceil, ceil(val), float, "f")
	FLOOR_CONST_MATH_SELECT(round, round(val), float, "f")
	FLOOR_CONST_MATH_SELECT(trunc, trunc(val), float, "f")
	FLOOR_CONST_MATH_SELECT(rint, rint(val), float, "f")
	FLOOR_CONST_MATH_SELECT(sin, sin(val), float, "f")
	FLOOR_CONST_MATH_SELECT(cos, cos(val), float, "f")
	FLOOR_CONST_MATH_SELECT(tan, tan(val), float, "f")
	FLOOR_CONST_MATH_SELECT(asin, asin(val), float, "f")
	FLOOR_CONST_MATH_SELECT(acos, acos(val), float, "f")
	FLOOR_CONST_MATH_SELECT(atan, atan(val), float, "f")
	FLOOR_CONST_MATH_SELECT_2(atan2, atan2(y, x), float, "f")
	FLOOR_CONST_MATH_SELECT_3(fma, fma(a, b, c), float, "f")
	FLOOR_CONST_MATH_SELECT(exp, exp(val), float, "f")
	FLOOR_CONST_MATH_SELECT(log, log(val), float, "f")
	
	FLOOR_CONST_MATH_SELECT_2(fmod, fmod(y, x), double, "d")
	FLOOR_CONST_MATH_SELECT(sqrt, sqrt(val), double, "d")
	FLOOR_CONST_MATH_SELECT(inv_sqrt, rsqrt(val), double, "d")
	FLOOR_CONST_MATH_SELECT(abs, fabs(val), double, "d")
	FLOOR_CONST_MATH_SELECT(floor, floor(val), double, "d")
	FLOOR_CONST_MATH_SELECT(ceil, ceil(val), double, "d")
	FLOOR_CONST_MATH_SELECT(round, round(val), double, "d")
	FLOOR_CONST_MATH_SELECT(trunc, trunc(val), double, "d")
	FLOOR_CONST_MATH_SELECT(rint, rint(val), double, "d")
	FLOOR_CONST_MATH_SELECT(sin, sin(val), double, "d")
	FLOOR_CONST_MATH_SELECT(cos, cos(val), double, "d")
	FLOOR_CONST_MATH_SELECT(tan, tan(val), double, "d")
	FLOOR_CONST_MATH_SELECT(asin, asin(val), double, "d")
	FLOOR_CONST_MATH_SELECT(acos, acos(val), double, "d")
	FLOOR_CONST_MATH_SELECT(atan, atan(val), double, "d")
	FLOOR_CONST_MATH_SELECT_2(atan2, atan2(y, x), double, "d")
	FLOOR_CONST_MATH_SELECT_3(fma, fma(a, b, c), double, "d")
	FLOOR_CONST_MATH_SELECT(exp, exp(val), double, "d")
	FLOOR_CONST_MATH_SELECT(log, log(val), double, "d")
#endif
	
#undef FLOOR_CONST_MATH_SELECT
#undef FLOOR_CONST_MATH_SELECT_2
#undef FLOOR_CONST_MATH_SELECT_3
	
}

#endif

#if defined(__clang__)
#pragma clang diagnostic pop
#endif
