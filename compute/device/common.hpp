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

#ifndef __FLOOR_COMPUTE_DEVICE_COMMON_HPP__
#define __FLOOR_COMPUTE_DEVICE_COMMON_HPP__

// basic floor macros + misc
#include <floor/core/essentials.hpp>

#if defined(FLOOR_COMPUTE_CUDA) || defined(FLOOR_COMPUTE_OPENCL) || defined(FLOOR_COMPUTE_METAL) || defined(FLOOR_COMPUTE_HOST)

// compute implementation specific headers (pre-std headers)
#if defined(FLOOR_COMPUTE_CUDA)
#include <floor/compute/device/cuda_pre.hpp>
#elif defined(FLOOR_COMPUTE_OPENCL)
#include <floor/compute/device/opencl_pre.hpp>
#elif defined(FLOOR_COMPUTE_METAL)
#include <floor/compute/device/metal_pre.hpp>
#elif defined(FLOOR_COMPUTE_HOST)
#include <floor/compute/device/host_pre.hpp>
#endif

// misc device information
#include <floor/compute/device/device_info.hpp>

#if !defined(FLOOR_COMPUTE_HOST)
// more integer types
typedef int8_t int_least8_t;
typedef int16_t int_least16_t;
typedef int32_t int_least32_t;
typedef int64_t int_least64_t;
typedef uint8_t uint_least8_t;
typedef uint16_t uint_least16_t;
typedef uint32_t uint_least32_t;
typedef uint64_t uint_least64_t;

typedef int8_t int_fast8_t;
typedef int16_t int_fast16_t;
typedef int32_t int_fast32_t;
typedef int64_t int_fast64_t;
typedef uint8_t uint_fast8_t;
typedef uint16_t uint_fast16_t;
typedef uint32_t uint_fast32_t;
typedef uint64_t uint_fast64_t;

typedef __INTPTR_TYPE__ intptr_t;
typedef __UINTPTR_TYPE__ uintptr_t;
typedef int64_t intmax_t;
typedef uint64_t uintmax_t;
#endif

using namespace std;

// atomics (needed before c++ headers)
#include <floor/compute/device/atomic_fallback.hpp>
// ring dependencies ftw
#define min(x, y) (x <= y ? x : y)
#define max(x, y) (x >= y ? x : y)
#if defined(FLOOR_COMPUTE_CUDA)
#include <floor/compute/device/cuda_atomic.hpp>
#elif defined(FLOOR_COMPUTE_OPENCL)
#include <floor/compute/device/opencl_atomic.hpp>
#elif defined(FLOOR_COMPUTE_METAL)
#include <floor/compute/device/metal_atomic.hpp>
#elif defined(FLOOR_COMPUTE_HOST)
#include <floor/compute/device/host_atomic.hpp>
#endif
// undef above min/max, we want the std::min/max
#undef min
#undef max

// *_atomic.hpp headers included above, now add compat/forwarder/alias functions
#include <floor/compute/device/atomic_compat.hpp>

// libc++ stl functionality without (most of) the baggage
#if !defined(FLOOR_COMPUTE_HOST)
#define _LIBCPP_NO_RTTI 1
#define _LIBCPP_BUILD_STATIC 1
#define _LIBCPP_NO_EXCEPTIONS 1
#define assert(...)
#endif
#include <utility>
#include <type_traits>
#include <initializer_list>
#include <functional>
#include <algorithm>
#include <atomic>

// will be using const_array instead of stl array
#include <floor/constexpr/const_array.hpp>

#if !defined(FLOOR_COMPUTE_HOST)
_LIBCPP_BEGIN_NAMESPACE_STD

// <array> replacement
template <class data_type, size_t array_size> _LIBCPP_TYPE_VIS_ONLY struct array : const_array<data_type, array_size> {};

_LIBCPP_END_NAMESPACE_STD
#else
#include <array>
#endif

// c++ stl "extensions"
#include <floor/core/cpp_ext.hpp>

// compute implementation specific headers
#if defined(FLOOR_COMPUTE_CUDA)
#include <floor/compute/device/cuda.hpp>
#elif defined(FLOOR_COMPUTE_OPENCL)
#include <floor/compute/device/opencl.hpp>
#elif defined(FLOOR_COMPUTE_METAL)
#include <floor/compute/device/metal.hpp>
#elif defined(FLOOR_COMPUTE_HOST)
#include <floor/compute/device/host.hpp>
#endif

// id handling through "variables"/objects (cuda handles this slightly differently -> defined in cuda.hpp)
#if !defined(FLOOR_COMPUTE_CUDA) && !(defined(FLOOR_COMPUTE_METAL) && defined(FLOOR_COMPUTE_INFO_VENDOR_NVIDIA))
#define global_id uint3 { get_global_id(0), get_global_id(1), get_global_id(2) }
#define global_size uint3 { get_global_size(0), get_global_size(1), get_global_size(2) }
#define local_id uint3 { get_local_id(0), get_local_id(1), get_local_id(2) }
#define local_size uint3 { get_local_size(0), get_local_size(1), get_local_size(2) }
#define group_id uint3 { get_group_id(0), get_group_id(1), get_group_id(2) }
#define group_size uint3 { get_num_groups(0), get_num_groups(1), get_num_groups(2) }
#endif

// always include const_math (and const_select) functionality
#include <floor/constexpr/const_math.hpp>

// always include vector lib/types
#include <floor/math/vector_lib.hpp>

// image support headers + common attributes
#if !defined(FLOOR_COMPUTE_HOST)
#define read_only __attribute__((image_read_only))
#define write_only __attribute__((image_write_only))
#define read_write __attribute__((image_read_write))
#else
#define read_only
#define write_only
#define read_write
#endif
#include <floor/core/enum_helpers.hpp>
#include <floor/compute/device/image_types.hpp>

//! global memory buffer
#define buffer __attribute__((align_value(128))) compute_global_buffer
template <typename T> using compute_global_buffer = global T*;

//! local memory buffer:
//! local_buffer<T, 42> => T[42]
//! local_buffer<T, 42, 23> => T[23][42]
//! local_buffer<T, 42, 23, 21> => T[21][23][42]
// NOTE: need to workaround the issue that "local" is not part of the type in cuda
#if !defined(FLOOR_COMPUTE_HOST) || 0
#define local_buffer local compute_local_buffer
#else // mt-group: local buffer must use thread local storage
#define local_buffer alignas(128) static _Thread_local compute_local_buffer
#endif
template <typename T, size_t count_x> using compute_local_buffer_1d = T[count_x];
template <typename T, size_t count_y, size_t count_x> using compute_local_buffer_2d = T[count_y][count_x];
template <typename T, size_t count_z, size_t count_y, size_t count_x> using compute_local_buffer_3d = T[count_z][count_y][count_x];
template <typename T, size_t count_x, size_t count_y = 0, size_t count_z = 0> using compute_local_buffer =
	conditional_t<count_y == 0, compute_local_buffer_1d<T, count_x>,
				  conditional_t<count_z == 0, compute_local_buffer_2d<T, count_y, count_x>,
											  compute_local_buffer_3d<T, count_z, count_y, count_x>>>;

//! constant memory buffer
// NOTE: again: need to workaround the issue that "constant" is not part of the type in cuda
#define constant_buffer __attribute__((align_value(128))) constant compute_constant_buffer
template <typename T> using compute_constant_buffer = const T* const;

//! array for use with static constant memory
#define constant_array constant compute_constant_array
template <class data_type, size_t array_size> using compute_constant_array = data_type[array_size];

//! generic parameter object/buffer
#if defined(FLOOR_COMPUTE_CUDA) || defined(FLOOR_COMPUTE_OPENCL)
template <typename T> using param = const T;
#elif defined(FLOOR_COMPUTE_METAL)
template <typename T> using param = const constant T&;
#elif defined(FLOOR_COMPUTE_HOST)
template <typename T> using param = const T&;
#endif

// implementation specific image headers
#if defined(FLOOR_COMPUTE_CUDA)
#include <floor/compute/device/cuda_image.hpp>
#elif defined(FLOOR_COMPUTE_OPENCL)
#include <floor/compute/device/opencl_image.hpp>
#elif defined(FLOOR_COMPUTE_METAL)
#include <floor/compute/device/metal_image.hpp>
#elif defined(FLOOR_COMPUTE_HOST)
#include <floor/compute/device/host_image.hpp>
#endif

// device logging functions
#include <floor/compute/device/logger.hpp>

#endif

#endif
