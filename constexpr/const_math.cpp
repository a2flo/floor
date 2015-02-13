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
#elif defined(__CUDA_CLANG__)
	// builtin functions defined by cuda/ptx
	FLOOR_CONST_MATH_SELECT_2(fmod, (y - x * __nvvm_trunc_ftz_f(y / x)), float, "f")
	FLOOR_CONST_MATH_SELECT(sqrt, __nvvm_sqrt_rz_ftz_f(val), float, "f")
	FLOOR_CONST_MATH_SELECT(inv_sqrt, __nvvm_rsqrt_approx_ftz_f(val), float, "f")
	FLOOR_CONST_MATH_SELECT(abs, __nvvm_fabs_ftz_f(val), float, "f")
	FLOOR_CONST_MATH_SELECT(floor, __nvvm_floor_ftz_f(val), float, "f")
	FLOOR_CONST_MATH_SELECT(ceil, __nvvm_ceil_ftz_f(val), float, "f")
	FLOOR_CONST_MATH_SELECT(round, __nvvm_round_ftz_f(val), float, "f")
	FLOOR_CONST_MATH_SELECT(trunc, __nvvm_trunc_ftz_f(val), float, "f")
	FLOOR_CONST_MATH_SELECT(rint, __nvvm_trunc_ftz_f(val), float, "f")
	FLOOR_CONST_MATH_SELECT(sin, __nvvm_sin_approx_ftz_f(val), float, "f")
	FLOOR_CONST_MATH_SELECT(cos, __nvvm_cos_approx_ftz_f(val), float, "f")
	FLOOR_CONST_MATH_SELECT(tan, (__nvvm_sin_approx_ftz_f(val) / __nvvm_cos_approx_ftz_f(val)), float, "f")
	FLOOR_CONST_MATH_SELECT(asin, (val), float, "f") // TODO: not supported in h/w, write proper rt computation
	FLOOR_CONST_MATH_SELECT(acos, (val), float, "f") // TODO: see above
	FLOOR_CONST_MATH_SELECT(atan, (val), float, "f") // TODO: see above
	FLOOR_CONST_MATH_SELECT_2(atan2, (y + x), float, "f") // TODO: see above
	FLOOR_CONST_MATH_SELECT_3(fma, __nvvm_fma_rz_ftz_f(a, b, c), float, "f")
	FLOOR_CONST_MATH_SELECT(exp, __nvvm_ex2_approx_ftz_f(val * 1.442695041f), float, "f") // 2^(x / ln(2))
	FLOOR_CONST_MATH_SELECT(log, (__nvvm_lg2_approx_ftz_f(val) * 1.442695041f), float, "f") // log_e = log_2(x) / log_2(e)
	
	FLOOR_CONST_MATH_SELECT_2(fmod, (y - x * __nvvm_trunc_d(y / x)), double, "d")
	FLOOR_CONST_MATH_SELECT(sqrt, __nvvm_sqrt_rz_d(val), double, "d")
	FLOOR_CONST_MATH_SELECT(inv_sqrt, __nvvm_rsqrt_approx_d(val), double, "d")
	FLOOR_CONST_MATH_SELECT(abs, __nvvm_fabs_d(val), double, "d")
	FLOOR_CONST_MATH_SELECT(floor, __nvvm_floor_d(val), double, "d")
	FLOOR_CONST_MATH_SELECT(ceil, __nvvm_ceil_d(val), double, "d")
	FLOOR_CONST_MATH_SELECT(round, __nvvm_round_d(val), double, "d")
	FLOOR_CONST_MATH_SELECT(trunc, __nvvm_trunc_d(val), double, "d")
	FLOOR_CONST_MATH_SELECT(rint, __nvvm_trunc_d(val), double, "d")
	FLOOR_CONST_MATH_SELECT(sin, (double)__nvvm_sin_approx_ftz_f(float(val)), double, "d")
	FLOOR_CONST_MATH_SELECT(cos, (double)__nvvm_cos_approx_ftz_f(float(val)), double, "d")
	FLOOR_CONST_MATH_SELECT(tan, (double)(__nvvm_sin_approx_ftz_f(float(val)) / __nvvm_cos_approx_ftz_f(float(val))), double, "d")
	FLOOR_CONST_MATH_SELECT(asin, (val), double, "d") // TODO: not supported in h/w, write proper rt computation
	FLOOR_CONST_MATH_SELECT(acos, (val), double, "d") // TODO: see above
	FLOOR_CONST_MATH_SELECT(atan, atan(val), double, "d") // TODO: see above
	FLOOR_CONST_MATH_SELECT_2(atan2, (y + x), double, "d") // TODO: see above
	FLOOR_CONST_MATH_SELECT_3(fma, __nvvm_fma_rz_d(a, b, c), double, "d")
	// TODO: even though there are intrinsics for this, there are no double/f64 versions supported in h/w
	FLOOR_CONST_MATH_SELECT(exp, __nvvm_ex2_approx_ftz_f(float(val) * 1.442695041f), double, "d") // 2^(x / ln(2))
	FLOOR_CONST_MATH_SELECT(log, (double(__nvvm_lg2_approx_ftz_f(val)) * 1.442695041), double, "d") // log_e = log_2(x) / log_2(e)
#elif defined(__SPIR_CLANG__)
	// builtin functions defined by opencl/spir
	// builtin functions defined by opencl/spir
	FLOOR_CONST_MATH_SELECT_2(fmod, ::fmod(y, x), float, "f")
	FLOOR_CONST_MATH_SELECT(sqrt, ::sqrt(val), float, "f")
	FLOOR_CONST_MATH_SELECT(inv_sqrt, ::rsqrt(val), float, "f")
	FLOOR_CONST_MATH_SELECT(abs, ::fabs(val), float, "f")
	FLOOR_CONST_MATH_SELECT(floor, ::floor(val), float, "f")
	FLOOR_CONST_MATH_SELECT(ceil, ::ceil(val), float, "f")
	FLOOR_CONST_MATH_SELECT(round, ::round(val), float, "f")
	FLOOR_CONST_MATH_SELECT(trunc, ::trunc(val), float, "f")
	FLOOR_CONST_MATH_SELECT(rint, ::rint(val), float, "f")
	FLOOR_CONST_MATH_SELECT(sin, ::sin(val), float, "f")
	FLOOR_CONST_MATH_SELECT(cos, ::cos(val), float, "f")
	FLOOR_CONST_MATH_SELECT(tan, ::tan(val), float, "f")
	FLOOR_CONST_MATH_SELECT(asin, ::asin(val), float, "f")
	FLOOR_CONST_MATH_SELECT(acos, ::acos(val), float, "f")
	FLOOR_CONST_MATH_SELECT(atan, ::atan(val), float, "f")
	FLOOR_CONST_MATH_SELECT_2(atan2, ::atan2(y, x), float, "f")
	FLOOR_CONST_MATH_SELECT_3(fma, ::fma(a, b, c), float, "f")
	FLOOR_CONST_MATH_SELECT(exp, ::exp(val), float, "f")
	FLOOR_CONST_MATH_SELECT(log, ::log(val), float, "f")
	
#if !defined(FLOOR_COMPUTE_NO_DOUBLE)
	FLOOR_CONST_MATH_SELECT_2(fmod, ::fmod(y, x), double, "d")
	FLOOR_CONST_MATH_SELECT(sqrt, ::sqrt(val), double, "d")
	FLOOR_CONST_MATH_SELECT(inv_sqrt, ::rsqrt(val), double, "d")
	FLOOR_CONST_MATH_SELECT(abs, ::fabs(val), double, "d")
	FLOOR_CONST_MATH_SELECT(floor, ::floor(val), double, "d")
	FLOOR_CONST_MATH_SELECT(ceil, ::ceil(val), double, "d")
	FLOOR_CONST_MATH_SELECT(round, ::round(val), double, "d")
	FLOOR_CONST_MATH_SELECT(trunc, ::trunc(val), double, "d")
	FLOOR_CONST_MATH_SELECT(rint, ::rint(val), double, "d")
	FLOOR_CONST_MATH_SELECT(sin, ::sin(val), double, "d")
	FLOOR_CONST_MATH_SELECT(cos, ::cos(val), double, "d")
	FLOOR_CONST_MATH_SELECT(tan, ::tan(val), double, "d")
	FLOOR_CONST_MATH_SELECT(asin, ::asin(val), double, "d")
	FLOOR_CONST_MATH_SELECT(acos, ::acos(val), double, "d")
	FLOOR_CONST_MATH_SELECT(atan, ::atan(val), double, "d")
	FLOOR_CONST_MATH_SELECT_2(atan2, ::atan2(y, x), double, "d")
	FLOOR_CONST_MATH_SELECT_3(fma, ::fma(a, b, c), double, "d")
	FLOOR_CONST_MATH_SELECT(exp, ::exp(val), double, "d")
	FLOOR_CONST_MATH_SELECT(log, ::log(val), double, "d")
#endif
#endif
	
#undef FLOOR_CONST_MATH_SELECT
#undef FLOOR_CONST_MATH_SELECT_2
#undef FLOOR_CONST_MATH_SELECT_3
	
}

#endif

#if defined(__clang__)
#pragma clang diagnostic pop
#endif
