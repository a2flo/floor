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

#ifndef __FLOOR_CONST_MATH_HPP__
#define __FLOOR_CONST_MATH_HPP__

// misc c++ headers
#include <type_traits>
#include <utility>
#include <limits>
#include <algorithm>
#if !defined(FLOOR_COMPUTE) || defined(FLOOR_COMPUTE_HOST)
#include <cmath>
#include <cstdint>
#if !defined(_MSC_VER)
#include <unistd.h>
#endif
#endif
#include <floor/core/essentials.hpp>
#include <floor/constexpr/soft_i128.hpp>
#include <floor/math/constants.hpp>
using namespace std;

// disable "comparing floating point with == or != is unsafe" warnings,
// b/c the comparisons here are actually supposed to be bitwise comparisons
FLOOR_PUSH_WARNINGS()
FLOOR_IGNORE_WARNING(float-equal)

namespace const_math {
	//! converts the input radian value to degrees
	template <typename fp_type, typename enable_if<is_floating_point<fp_type>::value, int>::type = 0>
	constexpr fp_type rad_to_deg(const fp_type& val) {
		return _180_DIV_PI<fp_type> * val;
	}
	//! converts the input radian value to degrees (for non floating point types)
	template <typename any_type, typename enable_if<!is_floating_point<any_type>::value, int>::type = 0>
	constexpr any_type rad_to_deg(const any_type& val) {
		return any_type(_180_DIV_PI<> * (max_fp_type)val);
	}
	
	//! converts the input degrees value to radian
	template <typename fp_type, typename enable_if<is_floating_point<fp_type>::value, int>::type = 0>
	constexpr fp_type deg_to_rad(const fp_type& val) {
		return PI_DIV_180<fp_type> * val;
	}
	//! converts the input degrees value to radian (for non floating point types)
	template <typename any_type, typename enable_if<!is_floating_point<any_type>::value, int>::type = 0>
	constexpr any_type deg_to_rad(const any_type& val) {
		return any_type(PI_DIV_180<> * (max_fp_type)val);
	}
	
	//! tests if two values are equal +/- a fixed epsilon (0.00001 / const_math::EPSILON<>)
	template <typename arithmetic_type, typename enable_if<is_arithmetic<arithmetic_type>::value, int>::type = 0>
	constexpr bool is_equal(const arithmetic_type& lhs, const arithmetic_type& rhs) {
		return (lhs > (rhs - arithmetic_type(EPSILON<>)) && lhs < (rhs + arithmetic_type(EPSILON<>)));
	}
	
	//! tests if two values are unequal +/- a fixed epsilon (0.00001 / const_math::EPSILON<>)
	template <typename arithmetic_type, typename enable_if<is_arithmetic<arithmetic_type>::value, int>::type = 0>
	constexpr bool is_unequal(const arithmetic_type& lhs, const arithmetic_type& rhs) {
		return (lhs < (rhs - arithmetic_type(EPSILON<>)) || lhs > (rhs + arithmetic_type(EPSILON<>)));
	}
	
	//! tests if two values are equal +/- a specified epsilon
	template <typename arithmetic_type, typename enable_if<is_arithmetic<arithmetic_type>::value, int>::type = 0>
	constexpr bool is_equal(const arithmetic_type& lhs, const arithmetic_type& rhs, const arithmetic_type& epsilon) {
		return (lhs > (rhs - epsilon) && lhs < (rhs + epsilon));
	}
	
	//! tests if two values are unequal +/- a specified epsilon
	template <typename arithmetic_type, typename enable_if<is_arithmetic<arithmetic_type>::value, int>::type = 0>
	constexpr bool is_unequal(const arithmetic_type& lhs, const arithmetic_type& rhs, const arithmetic_type& epsilon) {
		return (lhs < (rhs - epsilon) || lhs > (rhs + epsilon));
	}
	
	//! tests if the first value is less than the second value +/- a specified epsilon
	template <typename arithmetic_type, typename enable_if<is_arithmetic<arithmetic_type>::value, int>::type = 0>
	constexpr bool is_less(const arithmetic_type& lhs, const arithmetic_type& rhs, const arithmetic_type& epsilon) {
		return (lhs < (rhs + epsilon));
	}
	
	//! tests if the first value is less than or equal to the second value +/- a specified epsilon
	template <typename arithmetic_type, typename enable_if<is_arithmetic<arithmetic_type>::value, int>::type = 0>
	constexpr bool is_less_or_equal(const arithmetic_type& lhs, const arithmetic_type& rhs, const arithmetic_type& epsilon) {
		return (is_less(lhs, rhs, epsilon) || is_equal(lhs, rhs, epsilon));
	}
	
	//! tests if the first value is greater than the second value +/- a specified epsilon
	template <typename arithmetic_type, typename enable_if<is_arithmetic<arithmetic_type>::value, int>::type = 0>
	constexpr bool is_greater(const arithmetic_type& lhs, const arithmetic_type& rhs, const arithmetic_type& epsilon) {
		return (lhs > (rhs - epsilon));
	}
	
	//! tests if the first value is greater than or equal to the second value +/- a specified epsilon
	template <typename arithmetic_type, typename enable_if<is_arithmetic<arithmetic_type>::value, int>::type = 0>
	constexpr bool is_greater_or_equal(const arithmetic_type& lhs, const arithmetic_type& rhs, const arithmetic_type& epsilon) {
		return (is_greater(lhs, rhs, epsilon) || is_equal(lhs, rhs, epsilon));
	}
	
	//! decomposes a floating point value into <fp_type in [1, 2), 2^exp>
	//! NOTE: this doesn't handle infinity, NaNs or denormals
	template <typename fp_type, class = typename enable_if<is_floating_point<fp_type>::value>::type>
	constexpr pair<fp_type, int32_t> decompose_fp(fp_type in_val) {
		// get the min/max exponent (2^exp) that is representable by this fp type
		// note that these functions are defined as "one more than the actual exponent" => sub 1
		constexpr const auto max_exp = numeric_limits<fp_type>::max_exponent - 1; // e.g. 127 for float
		constexpr const auto min_exp = numeric_limits<fp_type>::min_exponent - 1; // e.g. -126 for float
		constexpr const auto abs_min_exp = -min_exp; // will need a positive number in a loop later
		
		// retrieve the sign of the input value (will be multiplied again in the end)
		const fp_type sign = in_val < (fp_type)0 ? -(fp_type)1 : (fp_type)1;
		fp_type val = sign < (fp_type)0 ? -in_val : in_val; // abs
		
		// actual algorithm:
		// for a positive exponent, divide the value by 2^i until it is in [1, 2)
		// for a negative exponent, multiply the value by 2^i until it is in [1, 2)
		int32_t exp = 0; // start with 2^0
		fp_type fexp = (fp_type)1;
		if(val > (fp_type)2) { // positive exponent: float: 2^1 .. 2^127 (2^128 is infinity)
			++exp;
			fexp *= (fp_type)2;
			for(; exp <= max_exp; ++exp) {
				const fp_type div_by_exp = (val / fexp);
				if(div_by_exp >= (fp_type)1 && div_by_exp < (fp_type)2) {
					val = div_by_exp;
					break;
				}
				fexp *= (fp_type)2;
			}
		}
		else if(val < (fp_type)1) { // negative exponent: float: 2^-1 .. 2^-126 (2^-127 is denorm)
			++exp;
			fexp *= (fp_type)2;
			for(; exp <= abs_min_exp; ++exp) {
				const fp_type mul_by_exp = (val * fexp);
				if(mul_by_exp >= (fp_type)1 && mul_by_exp < (fp_type)2) {
					val = mul_by_exp;
					break;
				}
				fexp *= (fp_type)2;
			}
			exp = -exp; // negative exponent
		}
		// else: [1, 2) -> 2^0
		
		return { val * sign, exp };
	}
	
	//! computes |x|, the absolute value of x
	template <typename arithmetic_type, class = typename enable_if<(is_arithmetic<arithmetic_type>::value ||
																	is_same<arithmetic_type, __int128_t>::value ||
																	is_same<arithmetic_type, __uint128_t>::value)>::type>
	constexpr arithmetic_type abs(arithmetic_type val) {
		return (val < (arithmetic_type)0 ? -val : val);
	}
	
	//! computes min(x, y), returning x if x <= y, else y
	template <typename arithmetic_type, class = typename enable_if<(is_arithmetic<arithmetic_type>::value ||
																	is_same<arithmetic_type, __int128_t>::value ||
																	is_same<arithmetic_type, __uint128_t>::value)>::type>
	constexpr arithmetic_type min(arithmetic_type x, arithmetic_type y) {
		return (x <= y ? x : y);
	}
	
	//! computes max(x, y), returning x if x >= y, else y
	template <typename arithmetic_type, class = typename enable_if<(is_arithmetic<arithmetic_type>::value ||
																	is_same<arithmetic_type, __int128_t>::value ||
																	is_same<arithmetic_type, __uint128_t>::value)>::type>
	constexpr arithmetic_type max(arithmetic_type x, arithmetic_type y) {
		return (x >= y ? x : y);
	}
	
	//! computes round(val), the nearest integer value to val
	//! NOTE: not precise for huge values that don't fit into a 64-bit int!
	template <typename fp_type, class = typename enable_if<is_floating_point<fp_type>::value>::type>
	constexpr fp_type round(fp_type val) {
		// add 0.5 for positive values, substract 0.5 for negative values,
		// then cast to int for rounding and back to fp_type again
		// e.g. (int)(2.3 + 0.5 == 2.7) == 2; (int)(2.5 + 0.5 == 3.0) == 3
		return (fp_type)(int64_t)(val + (val >= (fp_type)0 ? (fp_type)0.5_fp : (fp_type)-0.5_fp));
	}
	
	//! computes ⌊val⌋, the largest integer value not greater than val
	//! NOTE: not precise for huge values that don't fit into a 64-bit int!
	template <typename fp_type, class = typename enable_if<is_floating_point<fp_type>::value>::type>
	constexpr fp_type floor(fp_type val) {
		// casting to int truncates the value, which is floor(val) for positive values,
		// but we have to substract 1 for negative values (unless val is already floored == recasted int val)
		const auto val_int = (int64_t)val;
		const fp_type fval_int = (fp_type)val_int;
		return (val >= (fp_type)0 ? fval_int : (val == fval_int ? val : fval_int - (fp_type)1));
	}
	
	//! computes ⌈val⌉, the smallest integer value not less than val
	//! NOTE: not precise for huge values that don't fit into a 64-bit int!
	template <typename fp_type, class = typename enable_if<is_floating_point<fp_type>::value>::type>
	constexpr fp_type ceil(fp_type val) {
		// casting to int truncates the value, which is ceil(val) for negative values,
		// but we have to add 1 for positive values (unless val is already ceiled == recasted int val)
		const auto val_int = (int64_t)val;
		const fp_type fval_int = (fp_type)val_int;
		return (val < (fp_type)0 ? fval_int : (val == fval_int ? val : fval_int + (fp_type)1));
	}
	
	//! computes trunc(val), val rounded towards 0, or "drop the fractional part"
	//! NOTE: not precise for huge values that don't fit into a 64-bit int!
	template <typename fp_type, class = typename enable_if<is_floating_point<fp_type>::value>::type>
	constexpr fp_type trunc(fp_type val) {
		// this is basically the standard int cast
		return (fp_type)(int64_t)val;
	}
	
	//! this function only exists for completeness reasons and will always compute floor(x)
	//! NOTE: not precise for huge values that don't fit into a 64-bit int!
	template <typename fp_type, class = typename enable_if<is_floating_point<fp_type>::value>::type>
	constexpr fp_type rint(fp_type val) {
		return const_math::floor(val);
	}
	
	//! computes x % y, the remainder of the division x / y (aka modulo)
	//! NOTE: not precise for huge values that don't fit into a 64-bit int!
	template <typename fp_type, class = typename enable_if<is_floating_point<fp_type>::value>::type>
	constexpr fp_type fmod(fp_type x, fp_type y) {
		return x - y * const_math::trunc(x / y);
	}
	
	//! decomposes val into its integral and fractional part, fractional is returned, integral is stored in "dst_integral"
	//! NOTE: not precise for huge values that don't fit into a 64-bit int!
	template <typename fp_type, enable_if_t<is_floating_point<fp_type>::value>* = nullptr>
	constexpr fp_type modf(fp_type val, fp_type* dst_integral) {
		const auto truncated = const_math::trunc(val);
		*dst_integral = truncated;
		return val - truncated;
	}
	
	//! returns the fractional part of val
	//! NOTE: not precise for huge values that don't fit into a 64-bit int!
	template <typename fp_type, enable_if_t<is_floating_point<fp_type>::value>* = nullptr>
	constexpr fp_type fractional(fp_type val) {
		return val - const_math::trunc(val);
	}
	
	//! computes n!, the factorial of n
	//! NOTE: be aware that this uses 64-bit precision only, thus 20! is the largest correct result
	template <uint64_t n> constexpr uint64_t factorial() {
		static_assert(n <= 20ull, "input value too large");
		
		uint64_t fac = 1; // return 1 for n = 0 and n = 1
		for(uint64_t i = 2; i <= n; ++i) {
			fac *= i;
		}
		return fac;
	};
	
	//! computes n!, the factorial of n (usable with a runtime parameter)
	//! NOTE: be aware that this uses 64-bit precision only, thus 20! is the largest correct result
	constexpr uint64_t factorial(uint64_t n) {
		uint64_t fac = 1; // return 1 for n = 0 and n = 1
		for(uint64_t i = 2; i <= n; ++i) {
			fac *= i;
		}
		return fac;
	};
	
	//! computes (n choose k), the binomial coefficient
	//! NOTE: only safe to call up to n = 67, after this results may no longer fit into 64-bit
	__attribute__((pure, const)) constexpr uint64_t binomial(uint64_t n, uint64_t k)
	__attribute__((enable_if(!__builtin_constant_p(n) || (__builtin_constant_p(n) && n <= 67), "64-bit range"))) {
		if(k > n) return 0u;
		if(k == 0u || k == n) return 1u;
		
		// if n is small enough, long doubles are safe to use
		if(n <= 63) {
			k = const_math::min(k, n - k);
			long double ret = 1.0L;
			for(uint64_t i = 1u; i <= k; ++i) {
				// sadly have to use fp math because of this
				ret *= ((long double)((n + 1u) - i)) / ((long double)(i));
			}
			return (uint64_t)const_math::round(ret);
		}
		// else: compute it recursively
		else {
			// here: n > 63, k > 0
			return binomial(n - 1u, k - 1u) + binomial(n - 1u, k);
		}
	}
	
#if (!defined(FLOOR_COMPUTE) || defined(FLOOR_COMPUTE_HOST)) && !defined(PLATFORM_X32) // no 128-bit types
	//! computes (n choose k), the binomial coefficient
	//! NOTE: this allows for larger n than binomial(n, k), but recursiveness gets ugly for n > 80
	__attribute__((pure, const)) constexpr __uint128_t binomial_128(__uint128_t n, __uint128_t k) {
		if(k > n) return 0u;
		if(k == 0u || k == n) return 1u;
		
		// if n is small enough, long doubles are safe to use
		if(n <= 63) {
			k = const_math::min(k, n - k);
			long double ret = 1.0L;
			for(uint64_t i = 1u; i <= k; ++i) {
				// sadly have to use fp math because of this
				ret *= ((long double)((n + 1u) - i)) / ((long double)(i));
			}
			return (__uint128_t)const_math::round(ret);
		}
		// else: compute it recursively
		else {
			// here: n > 63, k > 0
			return binomial_128(n - 1u, k - 1u) + binomial_128(n - 1u, k);
		}
	}
#endif
	
	//! computes base^exponent, base to the power of exponent (with an integer exponent)
	template <typename arithmetic_type, typename enable_if<is_arithmetic<arithmetic_type>::value, int>::type = 0>
	constexpr arithmetic_type pow(const arithmetic_type base, const int32_t exponent) {
		arithmetic_type ret = (arithmetic_type)1;
		for(int32_t i = 0; i < exponent; ++i) {
			ret *= base;
		}
		return ret;
	}
	
	//! computes e^val, the exponential function value of val
	template <typename fp_type, class = typename enable_if<is_floating_point<fp_type>::value>::type>
	constexpr fp_type exp(fp_type val) {
		// convert to largest float type + div with ln(2) so that we can compute 2^x instead: e^x == 2^(x / ln(2))
		const auto abs_val = const_math::abs(val);
		const auto exponent = const_math::_1_DIV_LN2<> * (max_fp_type)abs_val;
		auto pot_factors = 1.0_fp;
		
		// "decompose" x of 2^x into integer power of two values that we can easily compute and a remainder in [0, 1)
		// -> check all pot exponents from 2^14 == 16384 to 2^0 == 1
		auto rem = exponent;
		int32_t pot_bits = 0;
		for(int32_t pot = 14; pot >= 0; --pot) {
			const auto ldbl_pot = (max_fp_type)(1 << pot);
			if(rem >= ldbl_pot) {
				pot_bits |= (1 << pot);
				rem -= ldbl_pot;
				pot_factors *= const_math::pow(2.0_fp, 1 << pot);
			}
			
			// no need to go on if this is the case (no more 2^x left to substract)
			if(rem < 1.0_fp) break;
		}
		
		// approximate e^x with x in [0, ln(2)) (== 2^x with x in [0, 1))
		// NOTE: slightly better accuracy if we don't reconvert rem again, but substract the converted pots instead
		const auto exp_val = ((max_fp_type)abs_val) - (const_math::LN2<> * (max_fp_type)pot_bits);
		constexpr const uint32_t pade_deg = 10 + 1; // NOTE: there is no benefit of using a higher degree than this
		constexpr const max_fp_type pade[pade_deg] {
			1.0_fp,
			0.5_fp,
			9.0_fp / 76.0_fp,
			1.0_fp / 57.0_fp,
			7.0_fp / 3876.0_fp,
			7.0_fp / 51680.0_fp,
			7.0_fp / 930240.0_fp,
			1.0_fp / 3255840.0_fp,
			1.0_fp / 112869120.0_fp,
			1.0_fp / 6094932480.0_fp,
			1.0_fp / 670442572800.0_fp,
		};
		auto exp_num = 0.0_fp;
		auto exp_denom = 0.0_fp;
		auto exp_pow = 1.0_fp;
		for(uint32_t i = 0; i < pade_deg; ++i) {
			exp_num += pade[i] * exp_pow;
			exp_denom += pade[i] * exp_pow * (i % 2 == 1 ? -1.0_fp : 1.0_fp);
			exp_pow *= exp_val;
		}
		const auto exp_approx = exp_num / exp_denom;
		
		// put it all together
		auto ret = pot_factors * exp_approx;
		if(val < fp_type(0.0)) {
			ret = 1.0_fp / ret;
		}
		return fp_type(ret);
	}
	
	//! computes exp2(val) == 2^val == exp(val * ln(2))
	template <typename fp_type, class = typename enable_if<is_floating_point<fp_type>::value>::type>
	constexpr fp_type exp2(fp_type val) {
		return fp_type(exp(max_fp_type(val) * const_math::LN2<>));
	}
	
	//! makes use of log(x * y) = log(x) + log(y) by decomposing val into a value in [1, 2) and its 2^x exponent,
	//! then easily computing log(val in [1, 2)) which converges quickly, and log2(2^x) == x for its exponent
	//! NOTE: returns { false, error ret value, ... } if val is an invalid value, { true, ..., log(val in [1, 2)), exponent } if valid
	template <typename fp_type, class = typename enable_if<is_floating_point<fp_type>::value>::type>
	constexpr auto partial_ln_and_log2(fp_type val) {
		struct ln_ret {
			bool valid;
			fp_type invalid_ret;
			max_fp_type decomp_base;
			max_fp_type decomp_exp;
		};
		if(val == (fp_type)0 || val == -(fp_type)0) return ln_ret { false, -numeric_limits<fp_type>::infinity(), 0.0_fp, 0.0_fp };
		if(val == (fp_type)1) return ln_ret { false, (fp_type)0, 0.0_fp, 0.0_fp };
		if(val < (fp_type)0) return ln_ret { false, numeric_limits<fp_type>::quiet_NaN(), 0.0_fp, 0.0_fp };
		if(__builtin_isinf(val)) return ln_ret { false, numeric_limits<fp_type>::infinity(), 0.0_fp, 0.0_fp };
		if(__builtin_isnan(val)) return ln_ret { false, numeric_limits<fp_type>::quiet_NaN(), 0.0_fp, 0.0_fp };
		
		// decompose into [1, 2) part and 2^x part
		const auto decomp = const_math::decompose_fp(val);
		
		// this converges quickly for [1, 2)
		// ref: https://en.wikipedia.org/wiki/Logarithm#Power_series (more efficient series)
		typedef long double max_fp_type;
		const auto ldbl_val = (max_fp_type)decomp.first;
		const auto frac = (ldbl_val - 1.0_fp) / (ldbl_val + 1.0_fp);
		const auto frac_sq = frac * frac;
		auto frac_exp = frac;
		auto res = frac;
		for(uint32_t i = 1; i < 32; ++i) {
			frac_exp *= frac_sq;
			res += frac_exp / max_fp_type(i * 2 + 1);
		}
		const auto decomp_res = res * 2.0_fp;
		
		// compute log2(decomp exponent), which is == exponent in this case
		const auto exp_log2_val = (max_fp_type)decomp.second;
		
		return ln_ret { true, (fp_type)0, decomp_res, exp_log2_val };
	}
	
	//! computes ln(val), the natural logarithm of val
	template <typename fp_type, class = typename enable_if<is_floating_point<fp_type>::value>::type>
	constexpr fp_type log(fp_type val) {
		const auto ret = partial_ln_and_log2(val);
		if(!ret.valid) return ret.invalid_ret;
		// "log_e(x) = log_2(x) / log_2(e)" for the exponent value, log(val in [1, 2)) is already correct
		return fp_type(ret.decomp_base + (ret.decomp_exp * const_math::_1_DIV_LD2_E<max_fp_type>));
	}
	
	//! computes lb(val) / ld(val) / log2(val), the base-2/binary logarithm of val
	template <typename fp_type, class = typename enable_if<is_floating_point<fp_type>::value>::type>
	constexpr fp_type log2(fp_type val) {
		const auto ret = partial_ln_and_log2(val);
		if(!ret.valid) return ret.invalid_ret;
		// "log_2(x) = ln(x) / ln(2)" for the decomposed value, log2(2^x) == x is already correct
		return fp_type((ret.decomp_base * const_math::_1_DIV_LN2<max_fp_type>) + ret.decomp_exp);
	}
	
	//! computes base^exponent, base to the power of exponent
	template <typename fp_type, class = typename enable_if<is_floating_point<fp_type>::value>::type>
	constexpr fp_type pow(const fp_type base, const fp_type exponent) {
		return const_math::exp(exponent * const_math::log(base));
	}
	
	//! returns the amount of halley iterations needed for a certain precision (type)
	//! for float: needs 3 iterations of halley to compute 1 / sqrt(x),
	//! although 2 iterations are enough for ~99.999% of all cases,
	//! but this should be identical to std::sqrt
	//! for double and long double: use 4 and 5 iterations, but this will never be
	//! fully identical to std::sqrt due to precision problems (but usually only
	//! off by 1 mantissa bit)
	template <typename T> constexpr int select_halley_iters() {
		switch(sizeof(T)) {
			// float
			case 4: return 3;
			// double
			case 8: return 4;
			// long double
			case 10:
			case 12:
			case 16:
			default:
				return 5;
		}
	}
	
	//! computes the square root and inverse/reciprocal square root of val
	//! return pair: <square root, inverse/reciprocal square root>
	template <typename fp_type, class = typename enable_if<is_floating_point<fp_type>::value>::type>
	constexpr pair<fp_type, fp_type> sqrt_and_rsqrt(fp_type val) {
		// make sure this is IEC559/IEEE-754 compliant
		static_assert(numeric_limits<fp_type>::is_iec559, "compiler or target is not IEC559/IEEE-754 compliant!");
		
		// handle special cases (need to resort to built-ins, because the c functions aren't constexpr):
		// * return unmodified +/- 0 (and NaN for "1 / 0")
		if(val == (fp_type)0 || val == -(fp_type)0) {
			return { val, numeric_limits<fp_type>::quiet_NaN() };
		}
		
		// * return unmodified +inf (note: support for __builtin_isinf_sign is problematic)
		if(__builtin_isinf(val) && val > (fp_type)0) {
			return { val, (fp_type)0 };
		}
		// * return NaN if val is NaN, -infinity or negative
		if(__builtin_isnan(val) || __builtin_isinf(val) || val < -(fp_type)0) {
			return { numeric_limits<fp_type>::quiet_NaN(), numeric_limits<fp_type>::quiet_NaN() };
		}
		// * return 0 if val is a denormal
		if(!__builtin_isnormal(val)) {
			return { (fp_type)0, (fp_type)0 };
		}
		
		// if constexpr would support reinterpret_casts, it would be as simple as:
		//    first_estimate = 0x5F375A86u - (*(uint32_t*)&val >> 1u)
		// ref: https://en.wikipedia.org/wiki/Fast_inverse_square_root
		//
		// sadly, it doesn't and we have to use a more complex method to compute the first estimate:
		//  * method #1: we know that: sqrt(x) = e^(0.5 * ln(x))
		//               -> approximate with ln and exp
		//               -> needs a lot of iterations to get right (~1536 exp iterations)
		//  * method #2: decompose float value into a 2^exp part and a value in [1, 2)
		//               -> can compute sqrt(2^exp) quite easily
		//                  (note: will make even exponent by dividing by 2 and multiplying float value by 2)
		//               -> can use simple estimate for value in [1, 2) (note: with an odd exponent this is in [1, 4))
		//               -> in the end: multiply both results
		//
		// after we've computed the first estimate, we could either use newton or halley to compute
		// the exact value of 1/sqrt(x). both methods should converge quickly with a good estimate.
		//  * newton (with rcp sqrt): 4 MULs, 1 SUB per iteration -- converges quadratically
		//  * halley (with rcp sqrt): 6 MULs, 2 SUBs per iteration -- converges cubically
		// -> will use halley
		// ref: https://en.wikipedia.org/wiki/Methods_of_computing_square_roots#Iterative_methods_for_reciprocal_square_roots
		
		// compute first estimate
		const auto decomp = decompose_fp(val);
		auto ldbl_val = (max_fp_type)decomp.first; // cast to more precise type, b/c we need the precision
		const bool is_neg_exp = (decomp.second < 0);
		int32_t abs_exp = (is_neg_exp ? -decomp.second : decomp.second);
		if(abs_exp % 2 == 1) {
			// if exponent is an odd number, divide exponent by 2 (-> substract 1 if positive, add 1 if negative) ...
			if(!is_neg_exp) --abs_exp;
			else ++abs_exp;
			// ... and multiply float by 2
			ldbl_val *= 2.0_fp;
		}
		
		// thanks wolfram: LinearModelFit[Table[{1+(t/1000), sqrt(1+(t/1000))}, {t, 0, 3000}], x^Range[0, 2], x]
		const auto estimate = 0.546702_fp + 0.502315_fp * ldbl_val - 0.0352763_fp * ldbl_val * ldbl_val;
		
		// halley iteration for sqrt(1 / x)
		constexpr const int halley_iters = select_halley_iters<fp_type>();
		auto x = 1.0_fp / estimate;
		for(int i = 0; i < halley_iters; ++i) {
			const auto y = ldbl_val * x * x;
			x = (x * 0.125_fp) * (15.0_fp - y * (10.0_fp - 3.0_fp * y));
		}
		auto rcp_x = x;
		
		// sqrt(1 / x) * x = sqrt(x)
		x *= ldbl_val;
		
		// compute sqrt of 2^exp (-> divide exponent by 2 => right shift by 1)
		// and multiply with/divide by sqrt(2^exp) resp.
		const max_fp_type sqrt_two_exp = const_math::pow(2.0_fp, (abs_exp >> 1));
		if(!is_neg_exp) {
			x *= sqrt_two_exp;
			rcp_x *= 1.0_fp / sqrt_two_exp;
		}
		else {
			x *= 1.0_fp / sqrt_two_exp;
			rcp_x *= sqrt_two_exp;
		}
		
		// finally done (cast down again to target type)
		return { fp_type(x), fp_type(rcp_x) };
	}
	
	//! computes the square root of val
	template <typename fp_type, class = typename enable_if<is_floating_point<fp_type>::value>::type>
	constexpr fp_type sqrt(fp_type val) {
		return sqrt_and_rsqrt(val).first;
	}
	
	//! computes the inverse/reciprocal square root of val
	template <typename fp_type, class = typename enable_if<is_floating_point<fp_type>::value>::type>
	constexpr fp_type rsqrt(fp_type val) {
		return sqrt_and_rsqrt(val).second;
	}
	
	//! computes cos(x), the cosine of the radian angle x
	template <typename fp_type, class = typename enable_if<is_floating_point<fp_type>::value>::type>
	constexpr fp_type cos(fp_type rad_angle) {
		// ref: https://en.wikipedia.org/wiki/Trigonometric_functions#Series_definitions
		// sum(k = 0 to inf): (-1)^k * x^(2*k) / (2*k)!
		// here: 10 iterations seem enough for now (this will go up to 20!, which is max factorial that fits into 64-bit)
		
		// always cast to long double precision for better accuracy + wrap to appropriate range [-pi, pi]
		const max_fp_type lrad_angle = const_math::fmod((max_fp_type)rad_angle, PI_MUL_2<>);
		const max_fp_type ldbl_val = lrad_angle + (lrad_angle > PI<> ? -PI_MUL_2<> : (lrad_angle < -PI<> ? PI_MUL_2<> : 0.0_fp));
		
		max_fp_type cos_x = 1.0_fp;
		uint64_t factorial_2k = 1ull; // (2*k)! in the iteration
		max_fp_type pow_x_2k = 1.0_fp; // x^(2*k) in the iteration
		const max_fp_type x_2 = ldbl_val * ldbl_val; // x^2 (needed in the iteration)
		for(int k = 1; k <= 10; ++k) {
			pow_x_2k *= x_2; // x^(2*k)
			factorial_2k *= (uint64_t)(k * 2 - 1) * (uint64_t)(k * 2); // (2*k)!
			cos_x += ((k % 2 == 1 ? -1.0_fp : 1.0_fp) * pow_x_2k) / (max_fp_type)(factorial_2k);
		}
		return (fp_type)cos_x;
	}
	
	//! computes sin(x), the sine of the radian angle x
	template <typename fp_type, class = typename enable_if<is_floating_point<fp_type>::value>::type>
	constexpr fp_type sin(fp_type rad_angle) {
		return (fp_type)const_math::cos(PI_DIV_2<> - (max_fp_type)rad_angle);
	}
	
	//! computes tan(x), the tangent of the radian angle x
	template <typename fp_type, class = typename enable_if<is_floating_point<fp_type>::value>::type>
	constexpr fp_type tan(fp_type rad_angle) {
		return (fp_type)(const_math::sin((max_fp_type)rad_angle) / const_math::cos((max_fp_type)rad_angle));
	}
	
	//! computes asin(x), the inverse sine / arcsine of x
	template <typename fp_type, class = typename enable_if<is_floating_point<fp_type>::value>::type>
	constexpr fp_type asin(fp_type val) {
		// ref: https://en.wikipedia.org/wiki/Inverse_trigonometric_functions#Infinite_series
		// sum(k = 0 to inf): ((2*k over k) * x^(1 + 2*k)) / (4^k * (1 + 2*k))
		// note: (2*k over k) = (2*k)! / (k! * k!)
		//  => each iteration: multiply with:
		//     (2*(k-1) * 2*k) / (k * k)
		//   = (4*k^2 - 2*k) / k^2
		//   = 4 - 2/k
		// this will work well enough up to |x| ~ 0.5 (errors are noticable from +/- 0.4 on)
		// -> from https://en.wikipedia.org/wiki/List_of_trigonometric_identities
		// we get: asin(x) = pi/2 - 2 * asin(sqrt((1 - x) / 2))
		// which can be used from 0.5 on (sqrt((1 - 0.5) / 2) = 0.5 ==> sqrt((1 - 1) / 2) = 0) using this same function
		
		// handle invalid input
		if(val < (fp_type)-1 || val > (fp_type)1) {
			return numeric_limits<fp_type>::quiet_NaN();
		}
		if(__builtin_isnan(val)) {
			return val;
		}
		
		// always cast to long double precision for better accuracy
		const max_fp_type ldbl_val = (max_fp_type)val;
		
		// handle |val| > 0.5
		if(const_math::abs(ldbl_val) > 0.5_fp) {
			return (fp_type)(PI_DIV_2<> - 2.0_fp * const_math::asin(const_math::sqrt((1.0_fp - ldbl_val) * 0.5_fp)));
		}
		
		// infinite series iteration
		max_fp_type asin_x = ldbl_val; // the approximated asin(x)
		max_fp_type binom_2k_k = 1.0_fp; // (2*k over k) in the iteration
		max_fp_type pow_x_1_2k = ldbl_val; // x^(1 + 2*k) in the iteration
		const max_fp_type x_2 = ldbl_val * ldbl_val; // x^2 (needed in the iteration)
		max_fp_type pow_4_k = 1.0_fp; // 4^k in the iteration
		for(int k = 1; k <= 9; ++k) {
			const auto fp_k = max_fp_type(k);
			binom_2k_k *= 4.0_fp - 2.0_fp / fp_k; // (2*k over k)
			pow_x_1_2k *= x_2; // x^(1 + 2*k)
			pow_4_k *= 4.0_fp; // 4^k
			asin_x += (binom_2k_k * pow_x_1_2k) / (pow_4_k * (1.0_fp + 2.0_fp * fp_k));
		}
		
		return (fp_type)asin_x;
	}
	
	//! computes acos(x), the inverse cosine / arccosine of x
	template <typename fp_type, class = typename enable_if<is_floating_point<fp_type>::value>::type>
	constexpr fp_type acos(fp_type val) {
		return (fp_type)(PI_DIV_2<> - const_math::asin((max_fp_type)val));
	}
	
	//! computes atan(x), the inverse tangent / arctangent of x
	template <typename fp_type, class = typename enable_if<is_floating_point<fp_type>::value>::type>
	constexpr fp_type atan(fp_type val) {
		const max_fp_type ldbl_val = (max_fp_type)val;
		return (fp_type)const_math::asin(ldbl_val / const_math::sqrt(ldbl_val * ldbl_val + 1.0_fp));
	}
	
	//! computes atan2(y, x), the arctangent with two arguments
	template <typename fp_type, class = typename enable_if<is_floating_point<fp_type>::value>::type>
	constexpr fp_type atan2(fp_type y, fp_type x) {
		// ref: https://en.wikipedia.org/wiki/Atan2
		const max_fp_type ldbl_x = (max_fp_type)x;
		const max_fp_type ldbl_y = (max_fp_type)y;
		if(x > (fp_type)0) {
			return (fp_type)const_math::atan(ldbl_y / ldbl_x);
		}
		else if(x < (fp_type)0) {
			if(y >= (fp_type)0) {
				return (fp_type)(const_math::atan(ldbl_y / ldbl_x) + PI<>);
			}
			else { // y < 0
				return (fp_type)(const_math::atan(ldbl_y / ldbl_x) - PI<>);
			}
		}
		else { // x == 0
			if(y > (fp_type)0) {
				return (fp_type)PI_DIV_2<>;
			}
			else if(y < (fp_type)0) {
				return (fp_type)-PI_DIV_2<>;
			}
			else { // y == 0
				return numeric_limits<fp_type>::quiet_NaN();
			}
		}
	}
	
	//! computes sinh(x), the hyperbolic sine of the radian angle x
	template <typename fp_type, class = typename enable_if<is_floating_point<fp_type>::value>::type>
	constexpr fp_type sinh(fp_type rad_angle) {
		const auto ldbl_val = (max_fp_type)rad_angle;
		return 0.5_fp * (const_math::exp(ldbl_val) - const_math::exp(-ldbl_val));
	}
	
	//! computes cosh(x), the hyperbolic cosine of the radian angle x
	template <typename fp_type, class = typename enable_if<is_floating_point<fp_type>::value>::type>
	constexpr fp_type cosh(fp_type rad_angle) {
		const auto ldbl_val = (max_fp_type)rad_angle;
		return 0.5_fp * (const_math::exp(ldbl_val) + const_math::exp(-ldbl_val));
	}
	
	//! computes tanh(x), the hyperbolic tangent of the radian angle x
	template <typename fp_type, class = typename enable_if<is_floating_point<fp_type>::value>::type>
	constexpr fp_type tanh(fp_type rad_angle) {
		const auto ldbl_val = (max_fp_type)rad_angle;
		const auto exp_pos = const_math::exp(ldbl_val);
		const auto exp_neg = const_math::exp(-ldbl_val);
		return fp_type((exp_pos - exp_neg) / (exp_pos + exp_neg));
	}
	
	//! clamps val to the range [min, max]
	template <typename arithmetic_type, enable_if_t<(is_arithmetic<arithmetic_type>() ||
													 is_same<arithmetic_type, __int128_t>() ||
													 is_same<arithmetic_type, __uint128_t>())>* = nullptr>
	constexpr arithmetic_type clamp(const arithmetic_type& val, const arithmetic_type& min, const arithmetic_type& max) {
		return (val > max ? max : (val < min ? min : val));
	}
	
	//! clamps val to the range [0, max]
	template <typename arithmetic_type, enable_if_t<(is_arithmetic<arithmetic_type>() ||
													 is_same<arithmetic_type, __int128_t>() ||
													 is_same<arithmetic_type, __uint128_t>())>* = nullptr>
	constexpr arithmetic_type clamp(const arithmetic_type& val, const arithmetic_type& max) {
		return (val > max ? max : (val < (arithmetic_type)0 ? (arithmetic_type)0 : val));
	}
	
	//! wraps val to the range [0, max]
	template <typename fp_type, enable_if_t<(is_floating_point<fp_type>())>* = nullptr>
	constexpr fp_type wrap(const fp_type& val, const fp_type& max) {
		return (val < (fp_type)0 ?
				(max - const_math::fmod(const_math::abs(val), max)) :
				const_math::fmod(val, max));
	}
	
	//! wraps val to the range [0, max]
	template <typename int_type, enable_if_t<((is_integral<int_type>() &&
											   is_signed<int_type>()) ||
											  is_same<int_type, __int128_t>())>* = nullptr>
	constexpr int_type wrap(const int_type& val, const int_type& max) {
		return (val < (int_type)0 ? (max - (const_math::abs(val) % max)) : (val % max));
	}
	
	//! wraps val to the range [0, max]
	template <typename uint_type, enable_if_t<((is_integral<uint_type>() &&
												is_unsigned<uint_type>()) ||
											   is_same<uint_type, __uint128_t>())>* = nullptr>
	constexpr uint_type wrap(const uint_type& val, const uint_type& max) {
		return (val % max);
	}
	
	//! computes the linear interpolation between a and b (with t = 0 -> a, t = 1 -> b)
	template <typename fp_type, enable_if_t<is_floating_point<fp_type>::value>* = nullptr>
	constexpr fp_type interpolate(const fp_type& a, const fp_type& b, const fp_type& t) {
#if !defined(FLOOR_COMPUTE) || defined(FLOOR_COMPUTE_HOST) || defined(FLOOR_COMPUTE_INFO_HAS_FMA_0)
		return ((b - a) * t + a);
#else
		return ::fma(t, b, ::fma(-t, a, a));
#endif
	}
	
	//! computes the linear interpolation between a and b (with t = 0 -> a, t = 1 -> b)
	//! NOTE: to be used with non-floating-point types, the interpolator must be a floating point type still
	template <typename any_type, typename fp_type, enable_if_t<(!is_floating_point<any_type>::value &&
																is_floating_point<fp_type>::value)>* = nullptr>
	constexpr any_type interpolate(const any_type& a, const any_type& b, const fp_type& t) {
		return any_type(fp_type(b - a) * t) + a;
	}
	
	//! computes the cubic interpolation between a and b, requiring the "point" prior to a and the "point" after b
	template <typename fp_type, enable_if_t<is_floating_point<fp_type>::value>* = nullptr>
	constexpr fp_type cubic_interpolate(const fp_type& a_prev,
										const fp_type& a,
										const fp_type& b,
										const fp_type& b_next,
										const fp_type& t) {
		// simple cubic interpolation
		// ref: http://paulbourke.net/miscellaneous/interpolation/
		//                        |  0   1   0   0 |   | a3 |
		//                        | -1   0   1   0 |   | a2 |
		// c(t) = (1 t t^2 t^3) * |  2  -2   1  -1 | * | a1 |
		//                        | -1   1  -1   1 |   | a0 |
		const auto t_2 = t * t;
		const auto a_diff = (a_prev - a);
		const auto b_diff = (b_next - b);
		const auto A = b_diff - a_diff;
		return {
			(A * t * t_2) +
			((a_diff - A) * t_2) +
			((b - a_prev) * t) +
			a
		};
	}
	
	//! computes the cubic interpolation between a and b, requiring the "point" prior to a and the "point" after b
	//! NOTE: to be used with non-floating-point types, the interpolator must be a floating point type still
	template <typename any_type, typename fp_type, enable_if_t<(!is_floating_point<any_type>::value &&
																is_floating_point<fp_type>::value)>* = nullptr>
	constexpr any_type cubic_interpolate(const any_type& a_prev,
										 const any_type& a,
										 const any_type& b,
										 const any_type& b_next,
										 const fp_type& t) {
		// -> explanation above
		const auto t_2 = t * t;
		const auto a_diff = (a_prev - a);
		const auto b_diff = (b_next - b);
		const auto A = b_diff - a_diff;
		return {
			any_type((fp_type(A) * t * t_2) +
					 (fp_type(a_diff - A) * t_2) +
					 (fp_type(b - a_prev) * t) +
					 fp_type(a))
		};
	}
	
	//! computes the cubic catmull-rom interpolation between a and b, requiring the "point" prior to a and the "point" after b
	template <typename fp_type, enable_if_t<is_floating_point<fp_type>::value>* = nullptr>
	constexpr fp_type catmull_rom_interpolate(const fp_type& a_prev,
											  const fp_type& a,
											  const fp_type& b,
											  const fp_type& b_next,
											  const fp_type& t) {
		// cubic catmull-rom interpolation
		// ref: http://en.wikipedia.org/wiki/Cubic_Hermite_spline
		//                              |  0   2   0   0 |   | a3 |
		//                              | -1   0   1   0 |   | a2 |
		// c(t) = 0.5 * (1 t t^2 t^3) * |  2  -5   4  -1 | * | a1 |
		//                              | -1   3  -3   1 |   | a0 |
		const auto t_2 = t * t;
		return {
			(((fp_type(3.0_fp) * (a - b) - a_prev + b_next) * t * t_2) +
			 ((fp_type(2.0_fp) * a_prev - fp_type(5.0_fp) * a + fp_type(4.0_fp) * b - b_next) * t_2) +
			 ((b - a_prev) * t))
			* fp_type(0.5_fp) + a
		};
	}
	
	//! computes the cubic catmull-rom interpolation between a and b, requiring the "point" prior to a and the "point" after b
	//! NOTE: to be used with non-floating-point types, the interpolator must be a floating point type still
	template <typename any_type, typename fp_type, enable_if_t<(!is_floating_point<any_type>::value &&
																is_floating_point<fp_type>::value)>* = nullptr>
	constexpr any_type catmull_rom_interpolate(const any_type& a_prev,
											   const any_type& a,
											   const any_type& b,
											   const any_type& b_next,
											   const fp_type& t) {
		// -> explanation above
		const auto t_2 = t * t;
		return {
			any_type((((fp_type(3.0_fp) * fp_type(a - b) + fp_type(b_next - a_prev)) * t * t_2) +
					  ((fp_type(2.0_fp) * fp_type(a_prev) - fp_type(5.0_fp) * fp_type(a) + fp_type(4.0_fp) * fp_type(b) - fp_type(b_next)) * t_2) +
					  (fp_type(b - a_prev) * t))
					 * fp_type(0.5_fp) + fp_type(a))
		};
	}
	
	//! computes the least common multiple of v1 and v2
	template <typename int_type, typename enable_if<is_integral<int_type>::value && is_signed<int_type>::value, int>::type = 0>
	constexpr int_type lcm(int_type v1, int_type v2) {
		int_type lcm_ = 1, div = 2;
		while(v1 != 1 || v2 != 1) {
			if((v1 % div) == 0 || (v2 % div) == 0) {
				if((v1 % div) == 0) v1 /= div;
				if((v2 % div) == 0) v2 /= div;
				lcm_ *= div;
			}
			else ++div;
		}
		return lcm_;
	}
	
	//! computes the greatest common divisor of v1 and v2
	template <typename int_type, typename enable_if<is_integral<int_type>::value, int>::type = 0>
	constexpr int_type gcd(const int_type& v1, const int_type& v2) {
		return ((v1 * v2) / const_math::lcm(v1, v2));
	}
	
	//! returns the nearest power of two value of num (only numerical upwards)
	template <typename int_type, typename enable_if<is_integral<int_type>::value, int>::type = 0>
	constexpr int_type next_pot(const int_type& num) {
		int_type tmp = 2;
		for(size_t i = 0; i < ((sizeof(int_type) * 8) - 1); ++i) {
			if(tmp >= num) return tmp;
			tmp <<= 1;
		}
		return 0;
	}
	
	//! computes the width of an integer value (e.g. 7 = 1, 42 = 2, 987654 = 6)
	template <typename int_type, typename enable_if<is_integral<int_type>::value, int>::type = 0>
	constexpr uint32_t int_width(const int_type& num) {
		uint32_t width = 1;
		auto val = const_math::abs(num);
		while(val > (int_type)10) {
			++width;
			val /= (int_type)10;
		}
		return width;
	}
	
	//! returns 'a' with the sign of 'b', essentially "sign(b) * abs(a)"
	template <typename fp_type, enable_if_t<is_floating_point<fp_type>::value>* = nullptr>
	constexpr fp_type copysign(const fp_type& a, const fp_type& b) {
		return (b < fp_type(0) ? fp_type(-1) : fp_type(1)) * const_math::abs(a);
	}
	
	//! computes the fused-multiply-add (a * b) + c, "as if to infinite precision and rounded only once to fit the result type"
	//! note: all arguments are cast to long double, then used to do the computation and then cast back to the return type
	template <typename fp_type, typename enable_if<is_floating_point<fp_type>::value, int>::type = 0>
	constexpr fp_type fma(fp_type mul_a, fp_type mul_b, fp_type add_c) {
		max_fp_type ldbl_a = (max_fp_type)mul_a;
		max_fp_type ldbl_b = (max_fp_type)mul_b;
		max_fp_type ldbl_c = (max_fp_type)add_c;
		return fp_type((ldbl_a * ldbl_b) + ldbl_c);
	}
	
#if !defined(FLOOR_COMPUTE) || defined(FLOOR_COMPUTE_HOST)
	//! not actually constexpr, but necessary to properly wrap native/builtin fma intrinsics
	floor_inline_always floor_used static float native_fma(float a, float b, float c) {
#if !defined(__c2__) // "Intrinsic not yet implemented"
		return __builtin_fmaf(a, b, c);
#else
		return (a * b) + c;
#endif
	}
	//! not actually constexpr, but necessary to properly wrap native/builtin fma intrinsics
	floor_inline_always floor_used static double native_fma(double a, double b, double c) {
#if !defined(__c2__) // "Intrinsic not yet implemented"
		return __builtin_fma(a, b, c);
#else
		return (a * b) + c;
#endif
	}
	//! not actually constexpr, but necessary to properly wrap native/builtin fma intrinsics
	floor_inline_always floor_used static long double native_fma(long double a, long double b, long double c) {
#if !defined(__c2__) // "Intrinsic not yet implemented"
		return __builtin_fmal(a, b, c);
#else
		return (a * b) + c;
#endif
	}
	//! not actually constexpr, but necessary to properly wrap native/builtin rsqrt intrinsics
	template <typename fp_type, typename enable_if<is_floating_point<fp_type>::value, int>::type = 0>
	floor_inline_always static fp_type native_rsqrt(fp_type a) {
		return fp_type(1.0_fp) / std::sqrt(a);
	}
#elif defined(FLOOR_COMPUTE_OPENCL) || defined(FLOOR_COMPUTE_CUDA) || defined(FLOOR_COMPUTE_METAL) || defined(FLOOR_COMPUTE_VULKAN)
	//! not actually constexpr, but necessary to properly wrap native/builtin fma intrinsics
	template <typename fp_type, typename enable_if<is_floating_point<fp_type>::value, int>::type = 0>
	floor_inline_always static fp_type native_fma(fp_type a, fp_type b, fp_type c) {
		return ::fma(a, b, c);
	}
	//! not actually constexpr, but necessary to properly wrap native/builtin rsqrt intrinsics
	template <typename fp_type, typename enable_if<is_floating_point<fp_type>::value, int>::type = 0>
	floor_inline_always static fp_type native_rsqrt(fp_type a) {
		return ::rsqrt(a);
	}
#else
#error "unsupported target"
#endif
	
	//! creates a "(1 << val) - 1" / "2^N - 1" bit mask (for 0 < N <= 64)
	template <typename uint_type, typename enable_if<is_integral<uint_type>::value && is_unsigned<uint_type>::value, int>::type = 0>
	constexpr uint_type bit_mask(const uint_type& val) {
		return (uint_type)(~(((~0ull) << ((unsigned long long int)val - 1ull)) << 1ull));
	}
	
}

// runtime equivalents for certain non-standard math functions
#include <floor/math/rt_math.hpp>

// reenable -Wfloat-equal warnings, disable -Wunused-function
FLOOR_POP_WARNINGS()
FLOOR_PUSH_WARNINGS()
FLOOR_IGNORE_WARNING(unused-function)

namespace math {
	// Dear reader of this code,
	// the following compiler extension and macroo voodoo insanity is sadly necessary
	// to obtain the ability to select a function depending on if it's truly constexpr
	// or an actual runtime call. this is not possible in plain iso c++14 (a constexpr
	// function must be callable at runtime and will be chosen over a non-constexpr
	// counterpart that is potentially/immensely faster at runtime). there are obviously
	// good reasons for that ("a function should return the same output with the same
	// input at compile-time and runtime"), but this is c++ land and efficiency always
	// trumps syntax or sane language design:
	//  -- "People will suffer atrocious syntax to get valuable functionality"
	//
	// Now, onto the gory details (note that this only works with clang 3.5+):
	// as mentioned, constexpr functions must be callable at runtime and compile-time,
	// thus they don't know (1) whether they're being called at runtime or compile-time
	// and (2) if their function parameters are actually constexpr or not; all "by design".
	// so the goal is to break either.
	//
	// clang recently added the ability to use enable_if attributes on functions (not to
	// be confused with the c++ stl enable_if) that enables or disables a function based
	// on a constant expression result, potentially using the function parameters.
	// this lets us determine if a function parameter is constexpr or not - but only
	// for direct function calls.
	// ref: http://clang.llvm.org/docs/AttributeReference.html#enable-if
	//
	// the gnu extension __builtin_constant_p(expr) is also present in clang (returns true
	// if expr is a compile-time constant), but won't work with constexpr function parameters
	// (will always return false). however, the evaluation and result of __builtin_constant_p
	// is always considered to be constexpr.
	//
	// asm("label") on a function changes the linkage name of the function to "label".
	//
	//
	// There are now two different ways of how and where this can be exploited:
	//  * for direct calls of functions in here, it is only necessary to provide a run-time
	//    and a constexpr variant of a function, the latter only being callable if all
	//    arguments are constexpr. this is done through the use of enable_if(val == val),
	//    where the comparison will only succeed and return true if "val" is constexpr.
	//    Note also that with the appropriate optimization setting even calls that aren't
	//    expressly forced to be constexpr evaluated _can_ still be constexpr evaluated
	//    by the compiler (clang).
	// The downside here is that constexpr function chaining with this will not work
	// (and luckily produce a hard error if constexpr evaluation is forced).
	//
	// For this, the second way has to be used, which is however incompatible to the first
	// one and should thus only be used inside other constexpr functions that need to call
	// this with parameters that aren't expressly always constexpr.
	//  * for forwarded/chained calls (another constexpr function calling a function in here),
	//    a combination of enable_if, __builtin_constant_p and asm labels is used to make
	//    chained constexpr calls still constexpr evaluable if forced.
	//    a run-time and a constexpr variant is still provided, but this time, the enable_if
	//    on the constexpr function makes it only callable if "!__builtin_constant_p(val)"
	//    evaluates to true. as mentioned before, __builtin_constant_p(val) will always
	//    evaluate to false if called with a constexpr function parameter (hence the negate).
	//    as-is, clang would now always call/select the constexpr function, even for run-time
	//    calls. therefore, an asm label with the exact same function name is applied to
	//    both the run-time and the constexpr function. with the run-time function defined
	//    after the constexpr function, clang will only emit the run-time function and just
	//    drop/ignore the constexpr one. however, for forced constexpr evaluation (which
	//    happens prior to codegen / function emission), the constexpr function is still
	//    the "active" one that will be considered for constexpr evaluation. thus, we
	//    achieved what we wanted.
	// The downside to the second approach is that it won't work for things that are
	// potentially constexpr, but aren't forced to be evaluated as such. This is why both
	// ways are implemented here and why the first one should be used for direct calls.
	// an additional drawback here is that this won't work with templates (or at least I
	// haven't figured out a workaround yet), thus support for different types has to be
	// handled/added manually.
	
#define FLOOR_CONST_SELECT_ARG_EXP_1(prefix, sep) prefix arg_0
#define FLOOR_CONST_SELECT_ARG_EXP_2(prefix, sep) prefix arg_0 sep prefix arg_1
#define FLOOR_CONST_SELECT_ARG_EXP_3(prefix, sep) prefix arg_0 sep prefix arg_1 sep prefix arg_2
#define FLOOR_CONST_SELECT_EIF_EXP_1() arg_0 == arg_0
#define FLOOR_CONST_SELECT_EIF_EXP_2() arg_0 == arg_0 && arg_1 == arg_1
#define FLOOR_CONST_SELECT_EIF_EXP_3() arg_0 == arg_0 && arg_1 == arg_1 && arg_2 == arg_2
#define FLOOR_PAREN_LEFT (
#define FLOOR_PAREN_RIGHT )
#define FLOOR_COMMA ,
	
#define FLOOR_CONST_SELECT(ARG_EXPANDER, ENABLE_IF_EXPANDER, func_name, ce_func, rt_func, type, overload_suffix) \
	/* direct call - run-time */ \
	static __attribute__((always_inline, flatten)) type func_name (ARG_EXPANDER(type, FLOOR_COMMA)) { \
		return rt_func (ARG_EXPANDER(, FLOOR_COMMA)); \
	} \
	/* direct call - constexpr */ \
	static __attribute__((always_inline, flatten)) constexpr type func_name (ARG_EXPANDER(type, FLOOR_COMMA)) \
	__attribute__((enable_if(ENABLE_IF_EXPANDER(), ""))) { \
		return ce_func (ARG_EXPANDER(, FLOOR_COMMA)); \
	} \
	\
	/* forwarded call prototype - run-time */ \
	static __attribute__((always_inline, flatten)) type __ ## func_name (ARG_EXPANDER(type, FLOOR_COMMA)) \
	asm("floor_const_select_" #func_name "_" #type overload_suffix ); \
	/* forwarded call prototype - constexpr */ \
	static __attribute__((always_inline, flatten)) constexpr type __ ## func_name (ARG_EXPANDER(type, FLOOR_COMMA)) \
	__attribute__((enable_if(ARG_EXPANDER(!__builtin_constant_p FLOOR_PAREN_LEFT, FLOOR_PAREN_RIGHT &&)), ""))) \
	asm("floor_const_select_" #func_name "_" #type overload_suffix ); \
	\
	/* forwarded call - constexpr */ \
	static __attribute__((always_inline, flatten)) constexpr type __ ## func_name (ARG_EXPANDER(type, FLOOR_COMMA)) \
	__attribute__((enable_if(ARG_EXPANDER(!__builtin_constant_p FLOOR_PAREN_LEFT, FLOOR_PAREN_RIGHT &&)), ""))) { \
		return ce_func (ARG_EXPANDER(, FLOOR_COMMA)); \
	} \
	/* forwarded call - run-time */ \
	static __attribute__((always_inline, flatten)) type __ ## func_name (ARG_EXPANDER(type, FLOOR_COMMA)) { \
		return rt_func (ARG_EXPANDER(, FLOOR_COMMA)); \
	}
	
#define FLOOR_CONST_SELECT_1(func_name, ce_func, rt_func, type, ...) \
	FLOOR_CONST_SELECT(FLOOR_CONST_SELECT_ARG_EXP_1, FLOOR_CONST_SELECT_EIF_EXP_1, func_name, ce_func, rt_func, type, "_1" __VA_ARGS__)
#define FLOOR_CONST_SELECT_2(func_name, ce_func, rt_func, type, ...) \
	FLOOR_CONST_SELECT(FLOOR_CONST_SELECT_ARG_EXP_2, FLOOR_CONST_SELECT_EIF_EXP_2, func_name, ce_func, rt_func, type, "_2" __VA_ARGS__)
#define FLOOR_CONST_SELECT_3(func_name, ce_func, rt_func, type, ...) \
	FLOOR_CONST_SELECT(FLOOR_CONST_SELECT_ARG_EXP_3, FLOOR_CONST_SELECT_EIF_EXP_3, func_name, ce_func, rt_func, type, "_3" __VA_ARGS__)
	
	// standard functions:
	FLOOR_CONST_SELECT_2(fmod, const_math::fmod, std::fmod, float)
	FLOOR_CONST_SELECT_1(sqrt, const_math::sqrt, std::sqrt, float)
	FLOOR_CONST_SELECT_1(abs, const_math::abs, std::fabs, float)
	FLOOR_CONST_SELECT_1(floor, const_math::floor, std::floor, float)
	FLOOR_CONST_SELECT_1(ceil, const_math::ceil, std::ceil, float)
	FLOOR_CONST_SELECT_1(round, const_math::round, std::round, float)
	FLOOR_CONST_SELECT_1(trunc, const_math::trunc, std::trunc, float)
	FLOOR_CONST_SELECT_1(rint, const_math::rint, std::rint, float)
	FLOOR_CONST_SELECT_1(sin, const_math::sin, std::sin, float)
	FLOOR_CONST_SELECT_1(cos, const_math::cos, std::cos, float)
	FLOOR_CONST_SELECT_1(tan, const_math::tan, std::tan, float)
	FLOOR_CONST_SELECT_1(asin, const_math::asin, std::asin, float)
	FLOOR_CONST_SELECT_1(acos, const_math::acos, std::acos, float)
	FLOOR_CONST_SELECT_1(atan, const_math::atan, std::atan, float)
	FLOOR_CONST_SELECT_2(atan2, const_math::atan2, std::atan2, float)
	FLOOR_CONST_SELECT_3(fma, const_math::fma, const_math::native_fma, float)
	FLOOR_CONST_SELECT_1(exp, const_math::exp, std::exp, float)
	FLOOR_CONST_SELECT_1(exp2, const_math::exp2, std::exp2, float)
	FLOOR_CONST_SELECT_1(log, const_math::log, std::log, float)
	FLOOR_CONST_SELECT_1(log2, const_math::log2, std::log2, float)
	FLOOR_CONST_SELECT_2(pow, const_math::pow, std::pow, float)
	FLOOR_CONST_SELECT_2(copysign, const_math::copysign, std::copysign, float)
	
#if !defined(FLOOR_COMPUTE_NO_DOUBLE)
	FLOOR_CONST_SELECT_2(fmod, const_math::fmod, std::fmod, double)
	FLOOR_CONST_SELECT_1(sqrt, const_math::sqrt, std::sqrt, double)
	FLOOR_CONST_SELECT_1(abs, const_math::abs, std::fabs, double)
	FLOOR_CONST_SELECT_1(floor, const_math::floor, std::floor, double)
	FLOOR_CONST_SELECT_1(ceil, const_math::ceil, std::ceil, double)
	FLOOR_CONST_SELECT_1(round, const_math::round, std::round, double)
	FLOOR_CONST_SELECT_1(trunc, const_math::trunc, std::trunc, double)
	FLOOR_CONST_SELECT_1(rint, const_math::rint, std::rint, double)
	FLOOR_CONST_SELECT_1(sin, const_math::sin, std::sin, double)
	FLOOR_CONST_SELECT_1(cos, const_math::cos, std::cos, double)
	FLOOR_CONST_SELECT_1(tan, const_math::tan, std::tan, double)
	FLOOR_CONST_SELECT_1(asin, const_math::asin, std::asin, double)
	FLOOR_CONST_SELECT_1(acos, const_math::acos, std::acos, double)
	FLOOR_CONST_SELECT_1(atan, const_math::atan, std::atan, double)
	FLOOR_CONST_SELECT_2(atan2, const_math::atan2, std::atan2, double)
	FLOOR_CONST_SELECT_3(fma, const_math::fma, const_math::native_fma, double)
	FLOOR_CONST_SELECT_1(exp, const_math::exp, std::exp, double)
	FLOOR_CONST_SELECT_1(exp2, const_math::exp2, std::exp2, double)
	FLOOR_CONST_SELECT_1(log, const_math::log, std::log, double)
	FLOOR_CONST_SELECT_1(log2, const_math::log2, std::log2, double)
	FLOOR_CONST_SELECT_2(pow, const_math::pow, std::pow, double)
	FLOOR_CONST_SELECT_2(copysign, const_math::copysign, std::copysign, double)
#endif
	
#if !defined(FLOOR_COMPUTE) || defined(FLOOR_COMPUTE_HOST)
	FLOOR_CONST_SELECT_2(fmod, const_math::fmod, ::fmodl, long double)
	FLOOR_CONST_SELECT_1(sqrt, const_math::sqrt, ::sqrtl, long double)
	FLOOR_CONST_SELECT_1(abs, const_math::abs, ::fabsl, long double)
	FLOOR_CONST_SELECT_1(floor, const_math::floor, ::floorl, long double)
	FLOOR_CONST_SELECT_1(ceil, const_math::ceil, ::ceill, long double)
	FLOOR_CONST_SELECT_1(round, const_math::round, ::roundl, long double)
	FLOOR_CONST_SELECT_1(trunc, const_math::trunc, ::truncl, long double)
	FLOOR_CONST_SELECT_1(rint, const_math::rint, ::rintl, long double)
	FLOOR_CONST_SELECT_1(sin, const_math::sin, ::sinl, long double)
	FLOOR_CONST_SELECT_1(cos, const_math::cos, ::cosl, long double)
	FLOOR_CONST_SELECT_1(tan, const_math::tan, ::tanl, long double)
	FLOOR_CONST_SELECT_1(asin, const_math::asin, ::asinl, long double)
	FLOOR_CONST_SELECT_1(acos, const_math::acos, ::acosl, long double)
	FLOOR_CONST_SELECT_1(atan, const_math::atan, ::atanl, long double)
	FLOOR_CONST_SELECT_2(atan2, const_math::atan2, ::atan2l, long double)
	FLOOR_CONST_SELECT_3(fma, const_math::fma, const_math::native_fma, long double)
	FLOOR_CONST_SELECT_1(exp, const_math::exp, ::expl, long double)
	FLOOR_CONST_SELECT_1(exp2, const_math::exp2, std::exp2, long double)
	FLOOR_CONST_SELECT_1(log, const_math::log, ::logl, long double)
	FLOOR_CONST_SELECT_1(log2, const_math::log2, std::log2, long double)
	FLOOR_CONST_SELECT_2(pow, const_math::pow, ::powl, long double)
	FLOOR_CONST_SELECT_2(copysign, const_math::copysign, ::copysignl, long double)
#endif
	
	// non-standard functions:
	FLOOR_CONST_SELECT_1(rsqrt, const_math::rsqrt, const_math::native_rsqrt, float)
	FLOOR_CONST_SELECT_3(clamp, const_math::clamp, rt_math::clamp, float)
	FLOOR_CONST_SELECT_2(clamp, const_math::clamp, rt_math::clamp, float)
	FLOOR_CONST_SELECT_2(wrap, const_math::wrap, rt_math::wrap, float)
	FLOOR_CONST_SELECT_1(fractional, const_math::fractional, rt_math::fractional, float)
	
	FLOOR_CONST_SELECT_3(clamp, const_math::clamp, rt_math::clamp, int8_t)
	FLOOR_CONST_SELECT_2(clamp, const_math::clamp, rt_math::clamp, int8_t)
	FLOOR_CONST_SELECT_2(wrap, const_math::wrap, rt_math::wrap, int8_t)
	
	FLOOR_CONST_SELECT_3(clamp, const_math::clamp, rt_math::clamp, int16_t)
	FLOOR_CONST_SELECT_2(clamp, const_math::clamp, rt_math::clamp, int16_t)
	FLOOR_CONST_SELECT_2(wrap, const_math::wrap, rt_math::wrap, int16_t)
	
	FLOOR_CONST_SELECT_3(clamp, const_math::clamp, rt_math::clamp, int32_t)
	FLOOR_CONST_SELECT_2(clamp, const_math::clamp, rt_math::clamp, int32_t)
	FLOOR_CONST_SELECT_2(wrap, const_math::wrap, rt_math::wrap, int32_t)
	
	FLOOR_CONST_SELECT_3(clamp, const_math::clamp, rt_math::clamp, int64_t)
	FLOOR_CONST_SELECT_2(clamp, const_math::clamp, rt_math::clamp, int64_t)
	FLOOR_CONST_SELECT_2(wrap, const_math::wrap, rt_math::wrap, int64_t)
	
	FLOOR_CONST_SELECT_3(clamp, const_math::clamp, rt_math::clamp, uint8_t)
	FLOOR_CONST_SELECT_2(clamp, const_math::clamp, rt_math::clamp, uint8_t)
	FLOOR_CONST_SELECT_2(wrap, const_math::wrap, rt_math::wrap, uint8_t)
	
	FLOOR_CONST_SELECT_3(clamp, const_math::clamp, rt_math::clamp, uint16_t)
	FLOOR_CONST_SELECT_2(clamp, const_math::clamp, rt_math::clamp, uint16_t)
	FLOOR_CONST_SELECT_2(wrap, const_math::wrap, rt_math::wrap, uint16_t)
	
	FLOOR_CONST_SELECT_3(clamp, const_math::clamp, rt_math::clamp, uint32_t)
	FLOOR_CONST_SELECT_2(clamp, const_math::clamp, rt_math::clamp, uint32_t)
	FLOOR_CONST_SELECT_2(wrap, const_math::wrap, rt_math::wrap, uint32_t)
	
	FLOOR_CONST_SELECT_3(clamp, const_math::clamp, rt_math::clamp, uint64_t)
	FLOOR_CONST_SELECT_2(clamp, const_math::clamp, rt_math::clamp, uint64_t)
	FLOOR_CONST_SELECT_2(wrap, const_math::wrap, rt_math::wrap, uint64_t)
	
	FLOOR_CONST_SELECT_3(clamp, const_math::clamp, rt_math::clamp, bool)
	FLOOR_CONST_SELECT_2(clamp, const_math::clamp, rt_math::clamp, bool)
	FLOOR_CONST_SELECT_2(wrap, const_math::wrap, rt_math::wrap, bool)
	
#if (!defined(FLOOR_COMPUTE) || defined(FLOOR_COMPUTE_HOST)) && !defined(PLATFORM_X32)
	FLOOR_CONST_SELECT_3(clamp, const_math::clamp, rt_math::clamp, __int128_t)
	FLOOR_CONST_SELECT_2(clamp, const_math::clamp, rt_math::clamp, __int128_t)
	FLOOR_CONST_SELECT_2(wrap, const_math::wrap, rt_math::wrap, __int128_t)
	
	FLOOR_CONST_SELECT_3(clamp, const_math::clamp, rt_math::clamp, __uint128_t)
	FLOOR_CONST_SELECT_2(clamp, const_math::clamp, rt_math::clamp, __uint128_t)
	FLOOR_CONST_SELECT_2(wrap, const_math::wrap, rt_math::wrap, __uint128_t)
#endif
	
#if defined(__APPLE__)
	FLOOR_CONST_SELECT_3(clamp, const_math::clamp, rt_math::clamp, ssize_t)
	FLOOR_CONST_SELECT_2(clamp, const_math::clamp, rt_math::clamp, ssize_t)
	FLOOR_CONST_SELECT_2(wrap, const_math::wrap, rt_math::wrap, ssize_t)
#endif
#if defined(__APPLE__) || (defined(FLOOR_COMPUTE_CUDA) && \
						   defined(PLATFORM_X64) && \
						   !(__clang_major__ == 3 && __clang_minor__ < 7))
	FLOOR_CONST_SELECT_3(clamp, const_math::clamp, rt_math::clamp, size_t)
	FLOOR_CONST_SELECT_2(clamp, const_math::clamp, rt_math::clamp, size_t)
	FLOOR_CONST_SELECT_2(wrap, const_math::wrap, rt_math::wrap, size_t)
#endif
	
#if !defined(FLOOR_COMPUTE_NO_DOUBLE)
	FLOOR_CONST_SELECT_1(rsqrt, const_math::rsqrt, const_math::native_rsqrt, double)
	FLOOR_CONST_SELECT_3(clamp, const_math::clamp, rt_math::clamp, double)
	FLOOR_CONST_SELECT_2(clamp, const_math::clamp, rt_math::clamp, double)
	FLOOR_CONST_SELECT_2(wrap, const_math::wrap, rt_math::wrap, double)
	FLOOR_CONST_SELECT_1(fractional, const_math::fractional, rt_math::fractional, double)
#endif
	
#if !defined(FLOOR_COMPUTE) || defined(FLOOR_COMPUTE_HOST)
	FLOOR_CONST_SELECT_1(rsqrt, const_math::rsqrt, const_math::native_rsqrt, long double)
	FLOOR_CONST_SELECT_3(clamp, const_math::clamp, rt_math::clamp, long double)
	FLOOR_CONST_SELECT_2(clamp, const_math::clamp, rt_math::clamp, long double)
	FLOOR_CONST_SELECT_2(wrap, const_math::wrap, rt_math::wrap, long double)
	FLOOR_CONST_SELECT_1(fractional, const_math::fractional, rt_math::fractional, long double)
#endif
	
	// non-standard and metal-only for now
#if defined(FLOOR_COMPUTE_METAL)
	FLOOR_CONST_SELECT_2(fmod, const_math::fmod, std::fmod, half)
	FLOOR_CONST_SELECT_1(sqrt, const_math::sqrt, std::sqrt, half)
	FLOOR_CONST_SELECT_1(rsqrt, const_math::rsqrt, const_math::native_rsqrt, half)
	FLOOR_CONST_SELECT_1(abs, const_math::abs, std::fabs, half)
	FLOOR_CONST_SELECT_1(floor, const_math::floor, std::floor, half)
	FLOOR_CONST_SELECT_1(ceil, const_math::ceil, std::ceil, half)
	FLOOR_CONST_SELECT_1(round, const_math::round, std::round, half)
	FLOOR_CONST_SELECT_1(trunc, const_math::trunc, std::trunc, half)
	FLOOR_CONST_SELECT_1(rint, const_math::rint, std::rint, half)
	FLOOR_CONST_SELECT_1(sin, const_math::sin, std::sin, half)
	FLOOR_CONST_SELECT_1(cos, const_math::cos, std::cos, half)
	FLOOR_CONST_SELECT_1(tan, const_math::tan, std::tan, half)
	FLOOR_CONST_SELECT_1(asin, const_math::asin, std::asin, half)
	FLOOR_CONST_SELECT_1(acos, const_math::acos, std::acos, half)
	FLOOR_CONST_SELECT_1(atan, const_math::atan, std::atan, half)
	FLOOR_CONST_SELECT_2(atan2, const_math::atan2, std::atan2, half)
	FLOOR_CONST_SELECT_3(fma, const_math::fma, const_math::native_fma, half)
	FLOOR_CONST_SELECT_1(exp, const_math::exp, std::exp, half)
	FLOOR_CONST_SELECT_1(exp2, const_math::exp2, std::exp2, half)
	FLOOR_CONST_SELECT_1(log, const_math::log, std::log, half)
	FLOOR_CONST_SELECT_1(log2, const_math::log2, std::log2, half)
	FLOOR_CONST_SELECT_2(pow, const_math::pow, std::pow, half)
	FLOOR_CONST_SELECT_2(copysign, const_math::copysign, std::copysign, half)
	
	FLOOR_CONST_SELECT_3(clamp, const_math::clamp, rt_math::clamp, half)
	FLOOR_CONST_SELECT_2(clamp, const_math::clamp, rt_math::clamp, half)
	FLOOR_CONST_SELECT_2(wrap, const_math::wrap, rt_math::wrap, half)
	FLOOR_CONST_SELECT_1(fractional, const_math::fractional, rt_math::fractional, half)
#endif

	// cleanup
#undef FLOOR_CONST_SELECT_ARG_EXP_1
#undef FLOOR_CONST_SELECT_ARG_EXP_2
#undef FLOOR_CONST_SELECT_ARG_EXP_3
#undef FLOOR_CONST_SELECT_EIF_EXP_1
#undef FLOOR_CONST_SELECT_EIF_EXP_2
#undef FLOOR_CONST_SELECT_EIF_EXP_3
#undef FLOOR_PAREN_LEFT
#undef FLOOR_PAREN_RIGHT
#undef FLOOR_COMMA

#undef FLOOR_CONST_SELECT
#undef FLOOR_CONST_SELECT_1
#undef FLOOR_CONST_SELECT_2
#undef FLOOR_CONST_SELECT_3
	
}

// reenable -Wunused-function
FLOOR_POP_WARNINGS()

#endif
