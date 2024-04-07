/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2024 Florian Ziesche
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
#include <floor/core/essentials.hpp>
using namespace std;

namespace const_math {
	//! largest supported floating point type at compile-time
#if !defined(FLOOR_COMPUTE) || (defined(FLOOR_COMPUTE_HOST) && !defined(FLOOR_COMPUTE_HOST_DEVICE))
	typedef long double max_fp_type;
#else
	typedef double max_fp_type; // can only use double with opencl/cuda/metal
#endif
	
	//! largest supported floating point type at run-time
#if !defined(FLOOR_COMPUTE) || (defined(FLOOR_COMPUTE_HOST) && !defined(FLOOR_COMPUTE_HOST_DEVICE))
	typedef long double max_rt_fp_type;
#elif !defined(FLOOR_COMPUTE_NO_DOUBLE)
	typedef double max_rt_fp_type; // can only use double with opencl/cuda/metal
#else
	typedef float max_rt_fp_type; // or even only float when there is no double support
#endif
	
	// misc math constants
	//! pi
	template <typename fp_type = max_fp_type>
	constexpr fp_type PI = fp_type(3.141592653589793238462643383279502884197169399375105820974944592307816406286208998628034825L);
	//! pi/2
	template <typename fp_type = max_fp_type>
	constexpr fp_type PI_DIV_2 = fp_type(1.570796326794896619231321691639751442098584699687552910487472296153908203143104499314L);
	//! pi/4
	template <typename fp_type = max_fp_type>
	constexpr fp_type PI_DIV_4 = fp_type(0.785398163397448309615660845819875721049292349843776455243736148076954101571552249657L);
	//! pi/180
	template <typename fp_type = max_fp_type>
	constexpr fp_type PI_DIV_180 = fp_type(0.0174532925199432957692369076848861271344287188854172545609719144017100911460344944L);
	//! pi/360
	template <typename fp_type = max_fp_type>
	constexpr fp_type PI_DIV_360 = fp_type(0.0087266462599716478846184538424430635672143594427086272804859572008550455730172472L);
	//! 2*pi
	template <typename fp_type = max_fp_type>
	constexpr fp_type PI_MUL_2 = fp_type(6.28318530717958647692528676655900576839433879875021164194988918461563281257241799725607L);
	//! 4*pi
	template <typename fp_type = max_fp_type>
	constexpr fp_type PI_MUL_4 = fp_type(12.5663706143591729538505735331180115367886775975004232838997783692312656251448359945121L);
	//! 1/pi
	template <typename fp_type = max_fp_type>
	constexpr fp_type _1_DIV_PI = fp_type(0.31830988618379067153776752674502872406891929148091289749533468811779359526845307018L);
	//! 1/(2*pi)
	template <typename fp_type = max_fp_type>
	constexpr fp_type _1_DIV_2PI = fp_type(0.1591549430918953357688837633725143620344596457404564487476673440588967976342265351L);
	//! 2/pi
	template <typename fp_type = max_fp_type>
	constexpr fp_type _2_DIV_PI = fp_type(0.63661977236758134307553505349005744813783858296182579499066937623558719053690614036L);
	//! 180/pi
	template <typename fp_type = max_fp_type>
	constexpr fp_type _180_DIV_PI = fp_type(57.29577951308232087679815481410517033240547246656432154916024386120284714832155263L);
	//! 360/pi
	template <typename fp_type = max_fp_type>
	constexpr fp_type _360_DIV_PI = fp_type(114.5915590261646417535963096282103406648109449331286430983204877224056942966431053L);
	//! 2/sqrt(pi)
	template <typename fp_type = max_fp_type>
	constexpr fp_type _2_DIV_SQRT_PI = fp_type(1.1283791670955125738961589031215451716881012586579977136881714434212849368829868L);
	//! sqrt(2)
	template <typename fp_type = max_fp_type>
	constexpr fp_type SQRT_2 = fp_type(1.4142135623730950488016887242096980785696718753769480731766797379907324784621070388503875L);
	//! 1/sqrt(2)
	template <typename fp_type = max_fp_type>
	constexpr fp_type _1_DIV_SQRT_2 = fp_type(0.707106781186547524400844362104849039284835937688474036588339868995366239231053519L);
	//! ln(2)
	template <typename fp_type = max_fp_type>
	constexpr fp_type LN_2 = fp_type(0.693147180559945309417232121458176568075500134360255254120680009493393621969694715605863327L);
	//! ln(10)
	template <typename fp_type = max_fp_type>
	constexpr fp_type LN_10 = fp_type(2.30258509299404568401799145468436420760110148862877297603332790096757260967735248023599721L);
	//! 1/ln(2)
	template <typename fp_type = max_fp_type>
	constexpr fp_type _1_DIV_LN_2 = fp_type(1.44269504088896340735992468100189213742664595415298593413544940693110921918118507989L);
	//! e
	template <typename fp_type = max_fp_type>
	constexpr fp_type E = fp_type(2.71828182845904523536028747135266249775724709369995957496696762772407663035354759457138217853L);
	//! ld(e)
	template <typename fp_type = max_fp_type>
	constexpr fp_type LD_E = _1_DIV_LN_2<fp_type>;
	//! log(e)
	template <typename fp_type = max_fp_type>
	constexpr fp_type LOG_E = fp_type(0.4342944819032518276511289189166050822943970058036665661144537831658646492088707747292249L);
	//! 1/ld(e)
	template <typename fp_type = max_fp_type>
	constexpr fp_type _1_DIV_LD_E = fp_type(0.693147180559945309417232121458176568075500134360255254120680009493393621969694716L);
	//! epsilon (for general use)
	template <typename fp_type = max_fp_type>
	constexpr fp_type EPSILON = fp_type(0.00001L);
}

//! udl to convert any floating-point value into the largest supported one at compile-time
constexpr const_math::max_fp_type operator""_fp(long double val) {
	return (const_math::max_fp_type)val;
}

//! udl to convert any floating-point value into the largest supported one at run-time
constexpr const_math::max_rt_fp_type operator""_rtfp(long double val) {
	return (const_math::max_rt_fp_type)val;
}
