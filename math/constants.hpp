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

#ifndef __FLOOR_CONSTANTS_HPP__
#define __FLOOR_CONSTANTS_HPP__

#include <type_traits>
#include <floor/core/essentials.hpp>
using namespace std;

namespace const_math {
	//! largest supported floating point type at compile-time
#if !defined(FLOOR_COMPUTE) || defined(FLOOR_COMPUTE_HOST)
	typedef long double max_fp_type;
#else
	typedef double max_fp_type; // can only use double with opencl/cuda/metal
#endif
	
	//! largest supported floating point type at run-time
#if !defined(FLOOR_COMPUTE) || defined(FLOOR_COMPUTE_HOST)
	typedef long double max_rt_fp_type;
#elif !defined(FLOOR_COMPUTE_NO_DOUBLE)
	typedef double max_rt_fp_type; // can only use double with opencl/cuda/metal
#else
	typedef float max_rt_fp_type; // or even only float when there is no double support
#endif
	
	// misc math constants
	//! pi
	template <typename fp_type = max_fp_type, typename enable_if<is_floating_point<fp_type>::value, int>::type = 0>
	constexpr fp_type PI = fp_type(3.14159265358979323846264338327950288419716939937510L);
	//! pi/2
	template <typename fp_type = max_fp_type, typename enable_if<is_floating_point<fp_type>::value, int>::type = 0>
	constexpr fp_type PI_DIV_2 = fp_type(1.57079632679489661923132169163975144209858469968755L);
	//! pi/4
	template <typename fp_type = max_fp_type, typename enable_if<is_floating_point<fp_type>::value, int>::type = 0>
	constexpr fp_type PI_DIV_4 = fp_type(0.785398163397448309615660845819875721049292349843775L);
	//! pi/180
	template <typename fp_type = max_fp_type, typename enable_if<is_floating_point<fp_type>::value, int>::type = 0>
	constexpr fp_type PI_DIV_180 = fp_type(0.0174532925199432957692369076848861271344287188854172L);
	//! pi/360
	template <typename fp_type = max_fp_type, typename enable_if<is_floating_point<fp_type>::value, int>::type = 0>
	constexpr fp_type PI_DIV_360 = fp_type(0.00872664625997164788461845384244306356721435944270861L);
	//! 2*pi
	template <typename fp_type = max_fp_type, typename enable_if<is_floating_point<fp_type>::value, int>::type = 0>
	constexpr fp_type PI_MUL_2 = fp_type(6.2831853071795864769252867665590057683943387987502L);
	//! 4*pi
	template <typename fp_type = max_fp_type, typename enable_if<is_floating_point<fp_type>::value, int>::type = 0>
	constexpr fp_type PI_MUL_4 = fp_type(12.5663706143591729538505735331180115367886775975004L);
	//! 1/pi
	template <typename fp_type = max_fp_type, typename enable_if<is_floating_point<fp_type>::value, int>::type = 0>
	constexpr fp_type _1_DIV_PI = fp_type(0.318309886183790671537767526745028724068919291480913487283406L);
	//! 1/(2*pi)
	template <typename fp_type = max_fp_type, typename enable_if<is_floating_point<fp_type>::value, int>::type = 0>
	constexpr fp_type _1_DIV_2PI = fp_type(0.159154943091895335768883763372514362034459645740456743641703L);
	//! 2/pi
	template <typename fp_type = max_fp_type, typename enable_if<is_floating_point<fp_type>::value, int>::type = 0>
	constexpr fp_type _2_DIV_PI = fp_type(0.636619772367581343075535053490057448137838582961826974566812L);
	//! 180/pi
	template <typename fp_type = max_fp_type, typename enable_if<is_floating_point<fp_type>::value, int>::type = 0>
	constexpr fp_type _180_DIV_PI = fp_type(57.29577951308232087679815481410517033240547246656442771101308L);
	//! 360/pi
	template <typename fp_type = max_fp_type, typename enable_if<is_floating_point<fp_type>::value, int>::type = 0>
	constexpr fp_type _360_DIV_PI = fp_type(114.5915590261646417535963096282103406648109449331288554220261L);
	//! ln(2)
	template <typename fp_type = max_fp_type, typename enable_if<is_floating_point<fp_type>::value, int>::type = 0>
	constexpr fp_type LN2 = fp_type(0.693147180559945309417232121458176568075500134360255L);
	//! 1/ln(2)
	template <typename fp_type = max_fp_type, typename enable_if<is_floating_point<fp_type>::value, int>::type = 0>
	constexpr fp_type _1_DIV_LN2 = fp_type(1.4426950408889634073599246810018921374266459541529859341354494L);
	//! 1/ld(e)
	template <typename fp_type = max_fp_type, typename enable_if<is_floating_point<fp_type>::value, int>::type = 0>
	constexpr fp_type _1_DIV_LD2_E = fp_type(0.6931471805599453094172321214581765680755001343602552L);
	//! epsilon (for general use)
	template <typename fp_type = max_fp_type, typename enable_if<is_floating_point<fp_type>::value, int>::type = 0>
	constexpr fp_type EPSILON = fp_type(0.00001L);
}

//! udl to convert any floating-point value into the largest supported one at compile-time
constexpr const_math::max_fp_type operator"" _fp(long double val) {
	return (const_math::max_fp_type)val;
}

//! udl to convert any floating-point value into the largest supported one at run-time
constexpr const_math::max_rt_fp_type operator"" _rtfp(long double val) {
	return (const_math::max_rt_fp_type)val;
}

#endif
