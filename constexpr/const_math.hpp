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

#ifndef __FLOOR_CONST_MATH_HPP__
#define __FLOOR_CONST_MATH_HPP__

// misc c++ headers
#include <type_traits>
#include <utility>
#include <limits>
#if !defined(FLOOR_COMPUTE)
#include <cmath>
#include <cstdint>
#if !defined(_MSC_VER)
#include <unistd.h>
#endif
#endif
#include <floor/core/essentials.hpp>
#include <floor/constexpr/soft_i128.hpp>
using namespace std;

// disable "comparing floating point with == or != is unsafe" warnings,
// b/c the comparisons here are actually supposed to be bitwise comparisons
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wfloat-equal"
#endif

namespace const_math {
	//! largest supported floating point type
#if !defined(FLOOR_COMPUTE)
	typedef long double max_fp_type;
#elif !defined(FLOOR_COMPUTE_NO_DOUBLE)
	typedef double max_fp_type; // can only use double with spir/opencl/cuda
#else
	typedef float max_fp_type; // or even only float when there is no double support
#endif
}

//! udl to convert any floating-point value into the largest supported one
constexpr const_math::max_fp_type operator"" _fp(long double val) {
	return (const_math::max_fp_type)val;
}

namespace const_math {
	// misc math constants
	//! pi
	template <typename fp_type = max_fp_type, typename enable_if<is_floating_point<fp_type>::value, int>::type = 0>
	constexpr constant fp_type PI = fp_type(3.14159265358979323846264338327950288419716939937510L);
	//! pi/2
	template <typename fp_type = max_fp_type, typename enable_if<is_floating_point<fp_type>::value, int>::type = 0>
	constexpr constant fp_type PI_DIV_2 = fp_type(1.57079632679489661923132169163975144209858469968755L);
	//! pi/4
	template <typename fp_type = max_fp_type, typename enable_if<is_floating_point<fp_type>::value, int>::type = 0>
	constexpr constant fp_type PI_DIV_4 = fp_type(0.785398163397448309615660845819875721049292349843775L);
	//! pi/180
	template <typename fp_type = max_fp_type, typename enable_if<is_floating_point<fp_type>::value, int>::type = 0>
	constexpr constant fp_type PI_DIV_180 = fp_type(0.0174532925199432957692369076848861271344287188854172L);
	//! pi/360
	template <typename fp_type = max_fp_type, typename enable_if<is_floating_point<fp_type>::value, int>::type = 0>
	constexpr constant fp_type PI_DIV_360 = fp_type(0.00872664625997164788461845384244306356721435944270861L);
	//! 2*pi
	template <typename fp_type = max_fp_type, typename enable_if<is_floating_point<fp_type>::value, int>::type = 0>
	constexpr constant fp_type PI_MUL_2 = fp_type(6.2831853071795864769252867665590057683943387987502L);
	//! 4*pi
	template <typename fp_type = max_fp_type, typename enable_if<is_floating_point<fp_type>::value, int>::type = 0>
	constexpr constant fp_type PI_MUL_4 = fp_type(12.5663706143591729538505735331180115367886775975004L);
	//! 1/pi
	template <typename fp_type = max_fp_type, typename enable_if<is_floating_point<fp_type>::value, int>::type = 0>
	constexpr constant fp_type _1_DIV_PI = fp_type(0.318309886183790671537767526745028724068919291480913487283406L);
	//! 1/(2*pi)
	template <typename fp_type = max_fp_type, typename enable_if<is_floating_point<fp_type>::value, int>::type = 0>
	constexpr constant fp_type _1_DIV_2PI = fp_type(0.159154943091895335768883763372514362034459645740456743641703L);
	//! 2/pi
	template <typename fp_type = max_fp_type, typename enable_if<is_floating_point<fp_type>::value, int>::type = 0>
	constexpr constant fp_type _2_DIV_PI = fp_type(0.636619772367581343075535053490057448137838582961826974566812L);
	//! 180/pi
	template <typename fp_type = max_fp_type, typename enable_if<is_floating_point<fp_type>::value, int>::type = 0>
	constexpr constant fp_type _180_DIV_PI = fp_type(57.29577951308232087679815481410517033240547246656442771101308L);
	//! 360/pi
	template <typename fp_type = max_fp_type, typename enable_if<is_floating_point<fp_type>::value, int>::type = 0>
	constexpr constant fp_type _360_DIV_PI = fp_type(114.5915590261646417535963096282103406648109449331288554220261L);
	//! epsilon (for general use)
	template <typename fp_type = max_fp_type, typename enable_if<is_floating_point<fp_type>::value, int>::type = 0>
	constexpr constant fp_type EPSILON = fp_type(0.00001L);
	
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
		
		// if n is small enough, doubles are safe to use
		if(n <= 53) {
			k = const_math::min(k, n - k);
			double ret = 1.0;
			for(uint64_t i = 1u; i <= k; ++i) {
				// sadly have to use fp math because of this
				ret *= double((n + 1u) - i) / double(i);
			}
			return (uint64_t)const_math::round(ret);
		}
		// else: compute it recursively
		else {
			// here: n > 53, k > 0
			return binomial(n - 1u, k - 1u) + binomial(n - 1u, k);
		}
	}
	
#if !defined(FLOOR_COMPUTE) && !defined(PLATFORM_X32) // no 128-bit types
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
	
	//! computes e^val, the exponential function value of val
	//! NOTE: not precise, especially for huge values
	template <typename fp_type, class = typename enable_if<is_floating_point<fp_type>::value>::type>
	constexpr fp_type exp(fp_type val) {
		// ref: https://en.wikipedia.org/wiki/Characterizations_of_the_exponential_function#Characterizations
		const max_fp_type ldbl_val = (max_fp_type)val;
		max_fp_type x_pow = 1.0_fp;
		max_fp_type x = 1.0_fp;
		max_fp_type dfac = 1.0_fp;
		for(int i = 1; i < 48; ++i) {
			x_pow *= ldbl_val;
			dfac *= (max_fp_type)i;
			x += x_pow / dfac;
		}
		return (fp_type)x;
	}
	
	//! computes ln(val), the natural logarithm of val
	//! NOTE: not precise, especially for huge values
	template <typename fp_type, class = typename enable_if<is_floating_point<fp_type>::value>::type>
	constexpr fp_type log(fp_type val) {
		// ref: https://en.wikipedia.org/wiki/Natural_logarithm#High_precision
		// compute a first estimate
		const max_fp_type ldbl_val = (max_fp_type)val;
		max_fp_type estimate = 0.0_fp;
		const max_fp_type x_inv = (ldbl_val - 1.0_fp) / ldbl_val;
		max_fp_type x_pow = x_inv;
		estimate = x_inv;
		for(int i = 2; i < 16; ++i) {
			x_pow *= x_inv;
			estimate += (1.0_fp / (max_fp_type)i) * x_pow;
		}
		
		// compute a more accurate ln(x) with newton
		max_fp_type x = estimate;
		for(int i = 0; i < 32; ++i) {
			const max_fp_type exp_x = const_math::exp(x);
			x = x + 2.0_fp * ((ldbl_val - exp_x) / (ldbl_val + exp_x));
		}
		return (fp_type)x;
	}
	
	//! computes base^exponent, base to the power of exponent
	template <typename arithmetic_type, typename enable_if<is_arithmetic<arithmetic_type>::value, int>::type = 0>
	constexpr arithmetic_type pow(const arithmetic_type base, const int32_t exponent) {
		arithmetic_type ret = (arithmetic_type)1;
		for(int32_t i = 0; i < exponent; ++i) {
			ret *= base;
		}
		return ret;
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
			case 16:
			default:
				return 5;
		}
	}
	
	//! computes the square root and inverse/reciprocal square root of val
	//! return pair: <square root, inverse/reciprocal square root>
	template <typename fp_type, class = typename enable_if<is_floating_point<fp_type>::value>::type>
	constexpr pair<fp_type, fp_type> sqrt_and_inv_sqrt(fp_type val) {
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
		max_fp_type ldbl_val = decomp.first; // cast to more precise type, b/c we need the precision
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
		return sqrt_and_inv_sqrt(val).first;
	}
	
	//! computes the inverse/reciprocal square root of val
	template <typename fp_type, class = typename enable_if<is_floating_point<fp_type>::value>::type>
	constexpr fp_type inv_sqrt(fp_type val) {
		return sqrt_and_inv_sqrt(val).second;
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
			const auto dbl_k = double(k);
			binom_2k_k *= 4.0_fp - 2.0_fp / dbl_k; // (2*k over k)
			pow_x_1_2k *= x_2; // x^(1 + 2*k)
			pow_4_k *= 4.0_fp; // 4^k
			asin_x += (binom_2k_k * pow_x_1_2k) / (pow_4_k * (1.0_fp + 2.0_fp * dbl_k));
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
	
	//! clamps val to the range [min, max]
	template <typename arithmetic_type, class = typename enable_if<is_arithmetic<arithmetic_type>::value>::type>
	constexpr arithmetic_type clamp(const arithmetic_type& val, const arithmetic_type& min, const arithmetic_type& max) {
		return (val > max ? max : (val < min ? min : val));
	}
	
	//! clamps val to the range [0, max]
	template <typename arithmetic_type, class = typename enable_if<is_arithmetic<arithmetic_type>::value>::type>
	constexpr arithmetic_type clamp(const arithmetic_type& val, const arithmetic_type& max) {
		return (val > max ? max : (val < (arithmetic_type)0 ? (arithmetic_type)0 : val));
	}
	
	//! wraps val to the range [0, max]
	template <typename fp_type, typename enable_if<is_floating_point<fp_type>::value, int>::type = 0>
	constexpr fp_type wrap(const fp_type& val, const fp_type& max) {
		return (val < (fp_type)0 ?
				(max - const_math::fmod(const_math::abs(val), max)) :
				const_math::fmod(val, max));
	}
	
	//! wraps val to the range [0, max]
	template <typename int_type, typename enable_if<is_integral<int_type>::value && is_signed<int_type>::value, int>::type = 0>
	constexpr int_type wrap(const int_type& val, const int_type& max) {
		return (val < (int_type)0 ? (max - (const_math::abs(val) % max)) : (val % max));
	}
	
	//! wraps val to the range [0, max]
	template <typename uint_type, typename enable_if<is_integral<uint_type>::value && is_unsigned<uint_type>::value, int>::type = 0>
	constexpr uint_type wrap(const uint_type& val, const uint_type& max) {
		return (val % max);
	}
	
	//! computes the linear interpolation between a and b
	template <typename fp_type, typename enable_if<is_floating_point<fp_type>::value, int>::type = 0>
	constexpr fp_type interpolate(const fp_type& a, const fp_type& b, const fp_type& t) {
		return ((b - a) * t + a);
	}
	
	//! computes the cubic interpolation between a and b, requiring the "point" prior to a and the "point" after b
	template <typename fp_type, typename enable_if<is_floating_point<fp_type>::value, int>::type = 0>
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
	
	//! computes the cubic catmull-rom interpolation between a and b, requiring the "point" prior to a and the "point" after b
	template <typename fp_type, typename enable_if<is_floating_point<fp_type>::value, int>::type = 0>
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
	
	//! computes the fused-multiply-add (a * b) + c, "as if to infinite precision and rounded only once to fit the result type"
	//! note: all arguments are cast to long double, then used to do the computation and then cast back to the return type
	template <typename fp_type, typename enable_if<is_floating_point<fp_type>::value, int>::type = 0>
	constexpr fp_type fma(fp_type mul_a, fp_type mul_b, fp_type add_c) {
		max_fp_type ldbl_a = (max_fp_type)mul_a;
		max_fp_type ldbl_b = (max_fp_type)mul_b;
		max_fp_type ldbl_c = (max_fp_type)add_c;
		return fp_type((ldbl_a * ldbl_b) + ldbl_c);
	}
	
#if !defined(FLOOR_COMPUTE)
	//! not actually constexpr, but necessary to properly wrap native/builtin fma intrinsics
	floor_inline_always floor_used static float native_fma(float a, float b, float c) {
		return __builtin_fmaf(a, b, c);
	}
	//! not actually constexpr, but necessary to properly wrap native/builtin fma intrinsics
	floor_inline_always floor_used static double native_fma(double a, double b, double c) {
		return __builtin_fma(a, b, c);
	}
	//! not actually constexpr, but necessary to properly wrap native/builtin fma intrinsics
	floor_inline_always floor_used static long double native_fma(long double a, long double b, long double c) {
		return __builtin_fmal(a, b, c);
	}
	//! not actually constexpr, but necessary to properly wrap native/builtin rsqrt intrinsics
	template <typename fp_type, typename enable_if<is_floating_point<fp_type>::value, int>::type = 0>
	floor_inline_always static fp_type native_rsqrt(fp_type a) {
		return fp_type(1.0L) / std::sqrt(a);
	}
#elif defined(FLOOR_COMPUTE_SPIR) || defined(FLOOR_COMPUTE_CUDA) || defined(FLOOR_COMPUTE_METAL)
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
	
}

// reenable warnings
#if defined(__clang__)
#pragma clang diagnostic pop
#endif

namespace const_select {
	// dear reader of this code,
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
	// now, onto the gory details (note that this only works with clang 3.5+):
	// as mentioned, constexpr functions must be callable at runtime and compile-time,
	// thus they don't know (1) whether they're being called at runtime or compile-time
	// and (2) if their function parameters are actually constexpr or not; all "by design".
	// so the goal is to break (1). (2) won't matter in that case.
	// clang recently added the ability to use enable_if attributes on functions (not to
	// be confused with the c++ stl enable_if) that enables or disables a function based
	// on a constant expression result.
	// ref: http://clang.llvm.org/docs/AttributeReference.html#enable-if
	// using this, in combination with __builtin_constant_p(expr) (a gnu extension that
	// returns true if expr is a compile-time constant) as the enable_if test expression
	// allows to enable a function only if a function parameter is a compile-time constant.
	// this works to some degree with constexpr functions, but only if the function is
	// _directly_ called with a constexpr variable. this is obviously not the case for
	// member variables of an otherwise constexpr-class (var life-time began outside the
	// function). this also won't work with nested constexpr functions as their function
	// parameters are not constexpr.
	// however, clang has a weird quirk/feature when using constexpr + enable_if in that
	// it will select a non-constexpr function over a constexpr function with the same
	// name	for non-constexpr calls and vice versa for constexpr calls.
	// => exactly what we wanted for (1)
	// note that this also slightly abuses __builtin_constant_p, which always returns
	// false for constexpr function parameters, !-ing the result thus enables the
	// constexpr function (this is of course also true for non-constexpr vars/calls).
	// the last remaining problem: using enable_if on a function will usually result in a
	// different mangled name, making the previous trick not work at all. to solve this,
	// asm labels are carefully applied so that both functions have the same symbol name.
	// note that the constexpr one won't be emitted (ignored by the linker) in that case.
	//
	// => create two forwarder functions with the same (symbol) name,
	//    one non-constexpr that calls a runtime function and
	//    one constexpr that calls a constexpr/compile-time function.
	//
	// there is one additional drawback to this: it doesn't work with templates (or at
	// least I haven't figured out a workaround yet), thus support for different types
	// has to be handled/added manually.
	
	// generic is_constexpr checks
#define FLOOR_IS_CONSTEXPR(type_name) \
	extern __attribute__((always_inline)) bool is_constexpr(type_name val) asm("floor__is_constexpr_" #type_name); \
	extern __attribute__((always_inline)) constexpr bool is_constexpr(type_name val) \
	__attribute__((enable_if(!__builtin_constant_p(val), ""))) asm("floor__is_constexpr_" #type_name); \
	\
	__attribute__((always_inline)) constexpr bool is_constexpr(type_name val) \
	__attribute__((enable_if(!__builtin_constant_p(val), ""))) { \
		return true; \
	}
	
	FLOOR_IS_CONSTEXPR(bool)
	FLOOR_IS_CONSTEXPR(int)
	FLOOR_IS_CONSTEXPR(size_t)
	FLOOR_IS_CONSTEXPR(float)
#if !defined(FLOOR_COMPUTE_NO_DOUBLE)
	FLOOR_IS_CONSTEXPR(double)
#endif
	
	// decl here, constexpr impl here, non-constexpr impl in const_math.cpp
#define FLOOR_CONST_MATH_SELECT(func_name, rt_func, type_name, type_suffix) \
	extern __attribute__((always_inline)) type_name func_name (type_name val) asm("floor__const_math_" #func_name type_suffix ); \
	extern __attribute__((always_inline)) constexpr type_name func_name (type_name val) \
	__attribute__((enable_if(!__builtin_constant_p(val), ""))) asm("floor__const_math_" #func_name type_suffix ); \
	\
	__attribute__((always_inline)) constexpr type_name func_name (type_name val) \
	__attribute__((enable_if(!__builtin_constant_p(val), ""))) { \
		return const_math:: func_name (val); \
	}

#define FLOOR_CONST_MATH_SELECT_2(func_name, rt_func, type_name, type_suffix) \
	extern __attribute__((always_inline)) type_name func_name (type_name y, type_name x) asm("floor__const_math_" #func_name type_suffix ); \
	extern __attribute__((always_inline)) constexpr type_name func_name (type_name y, type_name x) \
	__attribute__((enable_if(!__builtin_constant_p(y), ""))) \
	__attribute__((enable_if(!__builtin_constant_p(x), ""))) asm("floor__const_math_" #func_name type_suffix ); \
	\
	__attribute__((always_inline)) constexpr type_name func_name (type_name y, type_name x) \
	__attribute__((enable_if(!__builtin_constant_p(y), ""))) \
	__attribute__((enable_if(!__builtin_constant_p(x), ""))) { \
		return const_math:: func_name (y, x); \
	}

#define FLOOR_CONST_MATH_SELECT_3(func_name, rt_func, type_name, type_suffix) \
	extern __attribute__((always_inline)) type_name func_name (type_name a, type_name b, type_name c) \
	asm("floor__const_math_" #func_name type_suffix ); \
	extern __attribute__((always_inline)) constexpr type_name func_name (type_name a, type_name b, type_name c) \
	__attribute__((enable_if(!__builtin_constant_p(a), ""))) \
	__attribute__((enable_if(!__builtin_constant_p(b), ""))) \
	__attribute__((enable_if(!__builtin_constant_p(c), ""))) asm("floor__const_math_" #func_name type_suffix ); \
	\
	__attribute__((always_inline)) constexpr type_name func_name (type_name a, type_name b, type_name c) \
	__attribute__((enable_if(!__builtin_constant_p(a), ""))) \
	__attribute__((enable_if(!__builtin_constant_p(b), ""))) \
	__attribute__((enable_if(!__builtin_constant_p(c), ""))) { \
		return const_math:: func_name (a, b, c); \
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
	FLOOR_CONST_MATH_SELECT(log, std::log(val), float, "f")
	
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
	FLOOR_CONST_MATH_SELECT(log, std::log(val), double, "d")
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
	FLOOR_CONST_MATH_SELECT(log, std::logl(val), long double, "l")
#endif

#undef FLOOR_IS_CONSTEXPR
#undef FLOOR_CONST_MATH_SELECT
#undef FLOOR_CONST_MATH_SELECT_2
#undef FLOOR_CONST_MATH_SELECT_3
	
}

#endif
