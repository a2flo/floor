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

// clang vector types that are needed in some places (opencl/metal)
typedef int8_t clang_char1 __attribute__((ext_vector_type(1)));
typedef int8_t clang_char2 __attribute__((ext_vector_type(2)));
typedef int8_t clang_char3 __attribute__((ext_vector_type(3)));
typedef int8_t clang_char4 __attribute__((ext_vector_type(4)));
typedef uint8_t clang_uchar1 __attribute__((ext_vector_type(1)));
typedef uint8_t clang_uchar2 __attribute__((ext_vector_type(2)));
typedef uint8_t clang_uchar3 __attribute__((ext_vector_type(3)));
typedef uint8_t clang_uchar4 __attribute__((ext_vector_type(4)));
typedef int16_t clang_short1 __attribute__((ext_vector_type(1)));
typedef int16_t clang_short2 __attribute__((ext_vector_type(2)));
typedef int16_t clang_short3 __attribute__((ext_vector_type(3)));
typedef int16_t clang_short4 __attribute__((ext_vector_type(4)));
typedef uint16_t clang_ushort1 __attribute__((ext_vector_type(1)));
typedef uint16_t clang_ushort2 __attribute__((ext_vector_type(2)));
typedef uint16_t clang_ushort3 __attribute__((ext_vector_type(3)));
typedef uint16_t clang_ushort4 __attribute__((ext_vector_type(4)));
typedef int32_t clang_int1 __attribute__((ext_vector_type(1)));
typedef int32_t clang_int2 __attribute__((ext_vector_type(2)));
typedef int32_t clang_int3 __attribute__((ext_vector_type(3)));
typedef int32_t clang_int4 __attribute__((ext_vector_type(4)));
typedef uint32_t clang_uint1 __attribute__((ext_vector_type(1)));
typedef uint32_t clang_uint2 __attribute__((ext_vector_type(2)));
typedef uint32_t clang_uint3 __attribute__((ext_vector_type(3)));
typedef uint32_t clang_uint4 __attribute__((ext_vector_type(4)));
typedef int64_t clang_long1 __attribute__((ext_vector_type(1)));
typedef int64_t clang_long2 __attribute__((ext_vector_type(2)));
typedef int64_t clang_long3 __attribute__((ext_vector_type(3)));
typedef int64_t clang_long4 __attribute__((ext_vector_type(4)));
typedef uint64_t clang_ulong1 __attribute__((ext_vector_type(1)));
typedef uint64_t clang_ulong2 __attribute__((ext_vector_type(2)));
typedef uint64_t clang_ulong3 __attribute__((ext_vector_type(3)));
typedef uint64_t clang_ulong4 __attribute__((ext_vector_type(4)));
typedef float clang_float1 __attribute__((ext_vector_type(1)));
typedef float clang_float2 __attribute__((ext_vector_type(2)));
typedef float clang_float3 __attribute__((ext_vector_type(3)));
typedef float clang_float4 __attribute__((ext_vector_type(4)));
#if defined(FLOOR_COMPUTE_OPENCL) || defined(FLOOR_COMPUTE_METAL)
typedef half clang_half1 __attribute__((ext_vector_type(1)));
typedef half clang_half2 __attribute__((ext_vector_type(2)));
typedef half clang_half3 __attribute__((ext_vector_type(3)));
typedef half clang_half4 __attribute__((ext_vector_type(4)));
#endif
typedef double clang_double1 __attribute__((ext_vector_type(1)));
typedef double clang_double2 __attribute__((ext_vector_type(2)));
typedef double clang_double3 __attribute__((ext_vector_type(3)));
typedef double clang_double4 __attribute__((ext_vector_type(4)));

// const attribute allows external functions to be optimized away when not needed
// (used for asm label functions, functions containing non-side-effect asm code, other external functions)
#define const_func __attribute__((const))

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
#if !defined(FLOOR_COMPUTE_CUDA)
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
//#if !defined(__WINDOWS__)
#if !defined(FLOOR_COMPUTE_HOST)
#define local_buffer local compute_local_buffer
//#else // mt-group: local buffer must use thread local storage
//#define local_buffer alignas(128) static _Thread_local compute_local_buffer
//#endif

template <typename T, size_t count_x> using compute_local_buffer_1d = T[count_x];
template <typename T, size_t count_y, size_t count_x> using compute_local_buffer_2d = T[count_y][count_x];
template <typename T, size_t count_z, size_t count_y, size_t count_x> using compute_local_buffer_3d = T[count_z][count_y][count_x];
template <typename T, size_t count_x, size_t count_y = 0, size_t count_z = 0> using compute_local_buffer =
	conditional_t<count_y == 0, compute_local_buffer_1d<T, count_x>,
				  conditional_t<count_z == 0, compute_local_buffer_2d<T, count_y, count_x>,
											  compute_local_buffer_3d<T, count_z, count_y, count_x>>>;

#else // -> windows

// NOTE: since this is "static", it should only ever be called (allocated + initialized) by a single thread once
#define local_buffer static compute_local_buffer

// defined and set in host_kernel.cpp, must be extern so to avoid an opaque function call (which would kill vectorization)
extern _Thread_local uint32_t floor_thread_idx;
extern _Thread_local uint32_t floor_thread_local_memory_offset;

template <typename T, size_t count_x, size_t count_y = 0, size_t count_z = 0>
class compute_local_buffer {
protected:
	constexpr uint32_t dim() { return (count_y == 0 ? 1 : (count_z == 0 ? 2 : 3)); }
	constexpr size_t data_size() {
		constexpr auto type_size = sizeof(T);
		switch(dim()) {
			case 1: return count_x * type_size;
			case 2: return count_x * count_y * type_size;
			default: return count_x * count_y * count_z * type_size;
		}
	}
	
	T* __attribute__((aligned(128))) data;
	uint32_t offset { 0 };
	
public:
	// TODO: 2d/3d access
	
	T& operator[](const size_t& index) {
		return ((T*)__builtin_assume_aligned((uint8_t*)data + floor_thread_local_memory_offset + offset, 16))[index];
	}
	const T& operator[](const size_t& index) const {
		return ((T*)__builtin_assume_aligned((uint8_t*)data + floor_thread_local_memory_offset + offset, 16))[index];
	}
	
	compute_local_buffer() : data((T*)__builtin_assume_aligned(floor_requisition_local_memory(data_size(), offset), 16)) {}
	
};

#endif

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
