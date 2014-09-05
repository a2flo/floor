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

#ifndef __FLOOR_VECTOR_LIB_HPP__
#define __FLOOR_VECTOR_LIB_HPP__

// disable "comparing floating point with == or != is unsafe" warnings,
// b/c the comparisons here are actually supposed to be bitwise comparisons
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wfloat-equal"
#elif defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wfloat-equal"
#endif

// forwarder for many math functions for all necessary base types
#include "math/vector_helper.hpp"

// old vector classes (TODO: remove this)
#include "core/vector2.hpp"
#include "core/vector3.hpp"
#include "core/vector4.hpp"

// forward declare all vector types because of inter-dependencies
template <typename scalar_type> class vector1_test;
template <typename scalar_type> class vector2_test;
template <typename scalar_type> class vector3_test;
template <typename scalar_type> class vector4_test;

//
#define FLOOR_VECTOR_WIDTH 1
#include "math/vector.hpp"
#undef FLOOR_VECTOR_WIDTH
#define FLOOR_VECTOR_WIDTH 2
#include "math/vector.hpp"
#undef FLOOR_VECTOR_WIDTH
#define FLOOR_VECTOR_WIDTH 3
#include "math/vector.hpp"
#undef FLOOR_VECTOR_WIDTH
#define FLOOR_VECTOR_WIDTH 4
#include "math/vector.hpp"
#undef FLOOR_VECTOR_WIDTH

// TODO: instantiate all types (extern template + create new .cpp file)
// TODO: sizeof checks for all types

// reenable warnings
#if defined(__clang__)
#pragma clang diagnostic pop
#elif defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

#endif
