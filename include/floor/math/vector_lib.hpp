/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2025 Florian Ziesche
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

#include <floor/core/essentials.hpp>

// disable "comparing floating point with == or != is unsafe" warnings,
// b/c the comparisons here are actually supposed to be bitwise comparisons
FLOOR_PUSH_WARNINGS()
FLOOR_IGNORE_WARNING(float-equal)

// forwarder for many math functions for all necessary base types
#include <floor/math/vector_helper.hpp>

namespace fl {

// forward declare all vector types because of inter-dependencies
template <typename scalar_type> class vector1;
template <typename scalar_type> class vector2;
template <typename scalar_type> class vector3;
template <typename scalar_type> class vector4;

// pod type -> typedef name prefix
#if defined(__APPLE__) && (!defined(FLOOR_DEVICE) || (defined(FLOOR_DEVICE_HOST_COMPUTE) && !defined(FLOOR_DEVICE_HOST_COMPUTE_IS_DEVICE)))
// all types when compiling on macOS/iOS
#define FLOOR_VECTOR_TYPES_F(F, vec_width) \
F(float, float, vec_width) \
F(double, double, vec_width) \
F(long double, ldouble, vec_width) \
F(int8_t, char, vec_width) \
F(uint8_t, uchar, vec_width) \
F(int16_t, short, vec_width) \
F(uint16_t, ushort, vec_width) \
F(int32_t, int, vec_width) \
F(uint32_t, uint, vec_width) \
F(ssize_t, ssize, vec_width) \
F(size_t, size, vec_width) \
F(int64_t, long, vec_width) \
F(uint64_t, ulong, vec_width) \
F(bool, bool, vec_width)
#elif defined(FLOOR_DEVICE) && (!defined(FLOOR_DEVICE_HOST_COMPUTE) || defined(FLOOR_DEVICE_HOST_COMPUTE_IS_DEVICE))
// remove long double / double when compiling for device platforms
#if !defined(FLOOR_DEVICE_NO_DOUBLE)
#define FLOOR_VECTOR_TYPES_F(F, vec_width) \
F(float, float, vec_width) \
F(double, double, vec_width) \
F(int8_t, char, vec_width) \
F(uint8_t, uchar, vec_width) \
F(int16_t, short, vec_width) \
F(uint16_t, ushort, vec_width) \
F(int32_t, int, vec_width) \
F(uint32_t, uint, vec_width) \
F(int64_t, long, vec_width) \
F(uint64_t, ulong, vec_width) \
F(bool, bool, vec_width)
#else // disable double support as well
#define FLOOR_VECTOR_TYPES_F(F, vec_width) \
F(float, float, vec_width) \
F(int8_t, char, vec_width) \
F(uint8_t, uchar, vec_width) \
F(int16_t, short, vec_width) \
F(uint16_t, ushort, vec_width) \
F(int32_t, int, vec_width) \
F(uint32_t, uint, vec_width) \
F(int64_t, long, vec_width) \
F(uint64_t, ulong, vec_width) \
F(bool, bool, vec_width)
#endif
#else
// remove size_t and ssize_t when not compiling on macOS/iOS
#define FLOOR_VECTOR_TYPES_F(F, vec_width) \
F(float, float, vec_width) \
F(double, double, vec_width) \
F(long double, ldouble, vec_width) \
F(int8_t, char, vec_width) \
F(uint8_t, uchar, vec_width) \
F(int16_t, short, vec_width) \
F(uint16_t, ushort, vec_width) \
F(int32_t, int, vec_width) \
F(uint32_t, uint, vec_width) \
F(int64_t, long, vec_width) \
F(uint64_t, ulong, vec_width) \
F(bool, bool, vec_width)
#endif

// typedefs for all types and widths
#define FLOOR_VECTOR_TYPEDEF(pod_type, prefix, vec_width) \
using prefix##vec_width = vector##vec_width<pod_type>;

FLOOR_VECTOR_TYPES_F(FLOOR_VECTOR_TYPEDEF, 1)
FLOOR_VECTOR_TYPES_F(FLOOR_VECTOR_TYPEDEF, 2)
FLOOR_VECTOR_TYPES_F(FLOOR_VECTOR_TYPEDEF, 3)
FLOOR_VECTOR_TYPES_F(FLOOR_VECTOR_TYPEDEF, 4)

#if defined(FLOOR_DEVICE_METAL) || defined(FLOOR_DEVICE_VULKAN) || defined(FLOOR_DEVICE_HOST_COMPUTE) || defined(FLOOR_DEVICE_CUDA) || defined(FLOOR_DEVICE_OPENCL)
#define FLOOR_HALF_VECTOR_TYPE_F(F, vec_width) F(half, half, vec_width)
FLOOR_HALF_VECTOR_TYPE_F(FLOOR_VECTOR_TYPEDEF, 1)
FLOOR_HALF_VECTOR_TYPE_F(FLOOR_VECTOR_TYPEDEF, 2)
FLOOR_HALF_VECTOR_TYPE_F(FLOOR_VECTOR_TYPEDEF, 3)
FLOOR_HALF_VECTOR_TYPE_F(FLOOR_VECTOR_TYPEDEF, 4)
#endif

// necessary non-macOS/iOS aliases
#if !defined(__APPLE__) || (defined(FLOOR_DEVICE) && !defined(FLOOR_DEVICE_HOST_COMPUTE))
using size1 = ulong1;
using size2 = ulong2;
using size3 = ulong3;
using size4 = ulong4;
using ssize1 = long1;
using ssize2 = long2;
using ssize3 = long3;
using ssize4 = long4;
#endif

} // namespace fl

// implementation for each vector width
#define FLOOR_VECTOR_WIDTH 1
#include <floor/math/vector.hpp>
#undef FLOOR_VECTOR_WIDTH
#define FLOOR_VECTOR_WIDTH 2
#include <floor/math/vector.hpp>
#undef FLOOR_VECTOR_WIDTH
#define FLOOR_VECTOR_WIDTH 3
#include <floor/math/vector.hpp>
#undef FLOOR_VECTOR_WIDTH
#define FLOOR_VECTOR_WIDTH 4
#include <floor/math/vector.hpp>
#undef FLOOR_VECTOR_WIDTH

namespace fl {

// convenience alias, with variable scalar type and vector width
template <typename scalar_type, size_t N>
using vector_n = std::conditional_t<N == 1u, vector1<scalar_type>, std::conditional_t<
							   N == 2u, vector2<scalar_type>, std::conditional_t<
							   N == 3u, vector3<scalar_type>, std::conditional_t<
							   N == 4u, vector4<scalar_type>, void>>>>;

// extern template instantiation
#if defined(FLOOR_EXPORT)
#define FLOOR_VECTOR_EXTERN_TMPL(pod_type, prefix, vec_width) \
extern template class vector##vec_width<pod_type>;

FLOOR_VECTOR_TYPES_F(FLOOR_VECTOR_EXTERN_TMPL, 1)
FLOOR_VECTOR_TYPES_F(FLOOR_VECTOR_EXTERN_TMPL, 2)
FLOOR_VECTOR_TYPES_F(FLOOR_VECTOR_EXTERN_TMPL, 3)
FLOOR_VECTOR_TYPES_F(FLOOR_VECTOR_EXTERN_TMPL, 4)
#endif

// type trait function to determine if a type is a floor vector*
template <typename any_type> struct is_floor_vector : public std::false_type {};
template <typename vec_type>
requires (std::decay_t<vec_type>::dim() >= 1 &&
		  std::decay_t<vec_type>::dim() <= 4 &&
		  std::is_same_v<std::decay_t<vec_type>, typename std::decay_t<vec_type>::vector_type>)
struct is_floor_vector<vec_type> : public std::true_type {};
template <typename vec_type> constexpr bool is_floor_vector_v = is_floor_vector<vec_type>::value;

// reenable warnings
FLOOR_POP_WARNINGS()

} // namespace fl
