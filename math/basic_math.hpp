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

#ifndef __FLOOR_BASIC_MATH_HPP__
#define __FLOOR_BASIC_MATH_HPP__

#if !defined(PI)
#define PI          3.14159265358979323846264338327950288419716939937510L
#endif
#if !defined(PI_DIV_2)
#define PI_DIV_2    1.57079632679489661923132169163975144209858469968755L
#endif
#if !defined(PI_DIV_4)
#define PI_DIV_4    0.785398163397448309615660845819875721049292349843775L
#endif
#if !defined(PI_DIV_180)
#define PI_DIV_180  0.0174532925199432957692369076848861271344287188854172L
#endif
#if !defined(PI_DIV_360)
#define PI_DIV_360  0.00872664625997164788461845384244306356721435944270861L
#endif
#if !defined(_2_MUL_PI)
#define _2_MUL_PI   6.2831853071795864769252867665590057683943387987502L
#endif
#if !defined(_4_MUL_PI)
#define _4_MUL_PI   12.5663706143591729538505735331180115367886775975004L
#endif
#if !defined(_1_DIV_PI)
#define _1_DIV_PI   0.318309886183790671537767526745028724068919291480913487283406L
#endif
#if !defined(_1_DIV_2PI)
#define _1_DIV_2PI  0.159154943091895335768883763372514362034459645740456743641703L
#endif
#if !defined(_2_DIV_PI)
#define _2_DIV_PI   0.636619772367581343075535053490057448137838582961826974566812L
#endif
#if !defined(_180_DIV_PI)
#define _180_DIV_PI 57.29577951308232087679815481410517033240547246656442771101308L
#endif
#if !defined(_360_DIV_PI)
#define _360_DIV_PI 114.5915590261646417535963096282103406648109449331288554220261L
#endif

#define RAD2DEG(rad) (rad * _180_DIV_PI)
#define DEG2RAD(deg) (deg * PI_DIV_180)

#define FLOAT_EPSILON 0.00001f
#define FLOAT_EQ(x, v) ((((v) - FLOAT_EPSILON) < (x)) && ((x) < ((v) + FLOAT_EPSILON)))
#define FLOAT_EQ_EPS(x, v, eps) ((((x) > (v) - eps)) && ((x) < ((v) + eps)))
#define FLOAT_NE_EPS(x, v, eps) ((((x) < (v) - eps)) || ((x) > ((v) + eps)))
#define FLOAT_LT_EPS(x, v, eps) ((((x) < (v) - eps)) && ((x) < ((v) + eps)))
#define FLOAT_LE_EPS(x, v, eps) ((((x) <= (v) - eps)) && ((x) <= ((v) + eps)))
#define FLOAT_GT_EPS(x, v, eps) ((((x) > (v) - eps)) && ((x) > ((v) + eps)))
#define FLOAT_GE_EPS(x, v, eps) ((((x) >= (v) - eps)) && ((x) >= ((v) + eps)))

#endif
