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

// NOTE: can't use #pragma once here, because we're using it for pch and CLI include purposes
#ifndef LIBFLOOR_DEVICE_BACKEND_COMMON_HPP
#define LIBFLOOR_DEVICE_BACKEND_COMMON_HPP

// basic floor macros + misc
#include <floor/core/essentials.hpp>

#if defined(FLOOR_DEVICE_CUDA) || defined(FLOOR_DEVICE_OPENCL) || defined(FLOOR_DEVICE_METAL) || defined(FLOOR_DEVICE_HOST_COMPUTE) || defined(FLOOR_DEVICE_VULKAN)

// libc++ STL functionality without (most of) the baggage
#if !defined(FLOOR_DEVICE_HOST_COMPUTE) || defined(FLOOR_DEVICE_HOST_COMPUTE_IS_DEVICE)
#define _LIBCPP_NO_RTTI 1
#define _LIBCPP_BUILD_STATIC 1
#define _LIBCPP_NO_EXCEPTIONS 1
#define _LIBCPP_HAS_NO_THREADS 1
#define _LIBCPP_HAS_NO_VENDOR_AVAILABILITY_ANNOTATIONS 1
#define _LIBCPP_HAS_NO_ALIGNED_ALLOCATION 1
#define _LIBCPP_HAS_NO_PLATFORM_WAIT 1
#define assert(expr) __builtin_expect(!(expr), 0)
#endif

// compute implementation specific headers (pre-std headers)
#if defined(FLOOR_DEVICE_CUDA)
#include <floor/device/backend/cuda_pre.hpp>
#elif defined(FLOOR_DEVICE_OPENCL)
#include <floor/device/backend/opencl_pre.hpp>
#elif defined(FLOOR_DEVICE_VULKAN)
#include <floor/device/backend/vulkan_pre.hpp>
#elif defined(FLOOR_DEVICE_METAL)
#include <floor/device/backend/metal_pre.hpp>
#elif defined(FLOOR_DEVICE_HOST_COMPUTE)
#include <floor/device/backend/host_pre.hpp>
#endif

// more integer types
#if !defined(FLOOR_DEVICE_HOST_COMPUTE)
using int_least8_t = int8_t;
using int_least16_t = int16_t;
using int_least32_t = int32_t;
using int_least64_t = int64_t;
using uint_least8_t = uint8_t;
using uint_least16_t = uint16_t;
using uint_least32_t = uint32_t;
using uint_least64_t = uint64_t;

using int_fast8_t = int8_t;
using int_fast16_t = int16_t;
using int_fast32_t = int32_t;
using int_fast64_t = int64_t;
using uint_fast8_t = uint8_t;
using uint_fast16_t = uint16_t;
using uint_fast32_t = uint32_t;
using uint_fast64_t = uint64_t;

using intptr_t = __INTPTR_TYPE__;
using uintptr_t = __UINTPTR_TYPE__;
using intmax_t = int64_t;
using uintmax_t = uint64_t;
#endif

// misc device information
#include <floor/device/backend/device_info.hpp>

// clang vector types that are needed in some places (image functions)
using clang_char1 = __attribute__((ext_vector_type(1))) int8_t;
using clang_char2 = __attribute__((ext_vector_type(2))) int8_t;
using clang_char3 = __attribute__((ext_vector_type(3))) int8_t;
using clang_char4 = __attribute__((ext_vector_type(4))) int8_t;
using clang_uchar1 = __attribute__((ext_vector_type(1))) uint8_t;
using clang_uchar2 = __attribute__((ext_vector_type(2))) uint8_t;
using clang_uchar3 = __attribute__((ext_vector_type(3))) uint8_t;
using clang_uchar4 = __attribute__((ext_vector_type(4))) uint8_t;
using clang_short1 = __attribute__((ext_vector_type(1))) int16_t;
using clang_short2 = __attribute__((ext_vector_type(2))) int16_t;
using clang_short3 = __attribute__((ext_vector_type(3))) int16_t;
using clang_short4 = __attribute__((ext_vector_type(4))) int16_t;
using clang_ushort1 = __attribute__((ext_vector_type(1))) uint16_t;
using clang_ushort2 = __attribute__((ext_vector_type(2))) uint16_t;
using clang_ushort3 = __attribute__((ext_vector_type(3))) uint16_t;
using clang_ushort4 = __attribute__((ext_vector_type(4))) uint16_t;
using clang_int1 = __attribute__((ext_vector_type(1))) int32_t;
using clang_int2 = __attribute__((ext_vector_type(2))) int32_t;
using clang_int3 = __attribute__((ext_vector_type(3))) int32_t;
using clang_int4 = __attribute__((ext_vector_type(4))) int32_t;
using clang_uint1 = __attribute__((ext_vector_type(1))) uint32_t;
using clang_uint2 = __attribute__((ext_vector_type(2))) uint32_t;
using clang_uint3 = __attribute__((ext_vector_type(3))) uint32_t;
using clang_uint4 = __attribute__((ext_vector_type(4))) uint32_t;
using clang_long1 = __attribute__((ext_vector_type(1))) int64_t;
using clang_long2 = __attribute__((ext_vector_type(2))) int64_t;
using clang_long3 = __attribute__((ext_vector_type(3))) int64_t;
using clang_long4 = __attribute__((ext_vector_type(4))) int64_t;
using clang_ulong1 = __attribute__((ext_vector_type(1))) uint64_t;
using clang_ulong2 = __attribute__((ext_vector_type(2))) uint64_t;
using clang_ulong3 = __attribute__((ext_vector_type(3))) uint64_t;
using clang_ulong4 = __attribute__((ext_vector_type(4))) uint64_t;
using clang_float1 = __attribute__((ext_vector_type(1))) float;
using clang_float2 = __attribute__((ext_vector_type(2))) float;
using clang_float3 = __attribute__((ext_vector_type(3))) float;
using clang_float4 = __attribute__((ext_vector_type(4))) float;
#if defined(FLOOR_DEVICE_OPENCL) || defined(FLOOR_DEVICE_METAL) || defined(FLOOR_DEVICE_VULKAN) || defined(FLOOR_DEVICE_CUDA)
using clang_half1 = __attribute__((ext_vector_type(1))) half;
using clang_half2 = __attribute__((ext_vector_type(2))) half;
using clang_half3 = __attribute__((ext_vector_type(3))) half;
using clang_half4 = __attribute__((ext_vector_type(4))) half;
#endif
using clang_double1 = __attribute__((ext_vector_type(1))) double;
using clang_double2 = __attribute__((ext_vector_type(2))) double;
using clang_double3 = __attribute__((ext_vector_type(3))) double;
using clang_double4 = __attribute__((ext_vector_type(4))) double;

// const attribute allows external functions to be optimized away when not needed
// (used for asm label functions, functions containing non-side-effect asm code, other external functions)
#if !defined(const_func)
#define const_func __attribute__((const))
#endif

// STL headers that can be included before atomic functions
#include <type_traits>

// atomics (needed before C++ headers)
#include <floor/device/backend/atomic_fallback.hpp>
// ring dependencies ftw
#define min(x, y) (x <= y ? x : y)
#define max(x, y) (x >= y ? x : y)
// add atomic compat/forwarder/alias functions
#include <floor/device/backend/atomic_compat.hpp>
#if defined(FLOOR_DEVICE_CUDA)
#include <floor/device/backend/cuda_atomic.hpp>
#elif defined(FLOOR_DEVICE_OPENCL) || defined(FLOOR_DEVICE_VULKAN)
#include <floor/device/backend/opencl_atomic.hpp>
#elif defined(FLOOR_DEVICE_METAL)
#include <floor/device/backend/metal_atomic.hpp>
#elif defined(FLOOR_DEVICE_HOST_COMPUTE)
#include <floor/device/backend/host_atomic.hpp>
#endif
#include <floor/device/backend/atomic_compat_post.hpp>
// undef above min/max, we want the std::min/max
#undef min
#undef max

// STL headers that must be included after atomic functions
#include <utility>
#include <initializer_list>
#include <functional>
#include <algorithm>
#include <atomic>
#include <array>

// decay_as, same as decay, but also removes the address space
template <typename T> struct decay_as { using type = std::decay_t<T>; };
#if !defined(FLOOR_DEVICE_HOST_COMPUTE)
template <typename T> struct decay_as<__attribute__((global_as)) T> { using type = std::decay_t<T>; };
template <typename T> struct decay_as<__attribute__((local_as)) T> { using type = std::decay_t<T>; };
template <typename T> struct decay_as<__attribute__((constant_as)) T> { using type = std::decay_t<T>; };
template <typename T> struct decay_as<__attribute__((generic_as)) T> { using type = std::decay_t<T>; };
#endif
template <typename T> using decay_as_t = typename decay_as<T>::type;

// C++ STL "extensions"
#include <floor/core/cpp_ext.hpp>
#include <floor/constexpr/ext_traits.hpp>

// implementation specific headers
#if defined(FLOOR_DEVICE_CUDA)
#include <floor/device/backend/cuda.hpp>
#elif defined(FLOOR_DEVICE_OPENCL)
#include <floor/device/backend/opencl.hpp>
#elif defined(FLOOR_DEVICE_VULKAN)
#include <floor/device/backend/vulkan.hpp>
#elif defined(FLOOR_DEVICE_METAL)
#include <floor/device/backend/metal.hpp>
#elif defined(FLOOR_DEVICE_HOST_COMPUTE)
#include <floor/device/backend/host.hpp>
#endif

// min/max functions are problematic with most backends, C++14 requires/defines them to be constexpr,
// but backends define these as normal non-constexpr runtime functions, which will be chosen over
// the existing templated min/max functions (which are constexpr)
// -> use compiler/enable_if voodoo to fix it (see const_math.hpp for how this works)
#if !defined(FLOOR_DEVICE_HOST_COMPUTE)
_LIBCPP_BEGIN_NAMESPACE_STD
#define FLOOR_CONST_SELECT_2(func_name, ce_func, rt_func, type_name, type_suffix) \
static __attribute__((always_inline)) type_name func_name (type_name x, type_name y) asm("floor__fl_" #func_name type_suffix ); \
static __attribute__((always_inline)) constexpr type_name func_name (type_name x, type_name y) \
__attribute__((enable_if(!__builtin_constant_p(&x), ""))) \
__attribute__((enable_if(!__builtin_constant_p(&y), ""))) asm("floor__fl_" #func_name type_suffix ); \
\
[[maybe_unused]] static __attribute__((always_inline)) constexpr type_name func_name (type_name x, type_name y) \
__attribute__((enable_if(!__builtin_constant_p(&x), ""))) \
__attribute__((enable_if(!__builtin_constant_p(&y), ""))) { \
	return ce_func (x, y); \
} \
[[maybe_unused]] static __attribute__((always_inline)) type_name func_name (type_name x, type_name y) { \
	return fl:: rt_func (x, y); \
}

template <typename T> constexpr const_func floor_inline_always auto floor_ce_min(T x, T y) { return x <= y ? x : y; }
template <typename T> constexpr const_func floor_inline_always auto floor_ce_max(T x, T y) { return x >= y ? x : y; }

FLOOR_CONST_SELECT_2(min, floor_ce_min, floor_rt_min, int8_t, "c")
FLOOR_CONST_SELECT_2(min, floor_ce_min, floor_rt_min, int16_t, "s")
FLOOR_CONST_SELECT_2(min, floor_ce_min, floor_rt_min, int32_t, "i")
FLOOR_CONST_SELECT_2(min, floor_ce_min, floor_rt_min, int64_t, "l")
FLOOR_CONST_SELECT_2(min, floor_ce_min, floor_rt_min, uint8_t, "h")
FLOOR_CONST_SELECT_2(min, floor_ce_min, floor_rt_min, uint16_t, "t")
FLOOR_CONST_SELECT_2(min, floor_ce_min, floor_rt_min, uint32_t, "j")
FLOOR_CONST_SELECT_2(min, floor_ce_min, floor_rt_min, uint64_t, "m")
FLOOR_CONST_SELECT_2(min, floor_ce_min, floor_rt_min, half, "Dh")
FLOOR_CONST_SELECT_2(min, floor_ce_min, floor_rt_min, float, "f")
#if !defined(FLOOR_DEVICE_NO_DOUBLE)
FLOOR_CONST_SELECT_2(min, floor_ce_min, floor_rt_min, double, "d")
#endif

FLOOR_CONST_SELECT_2(max, floor_ce_max, floor_rt_max, int8_t, "c")
FLOOR_CONST_SELECT_2(max, floor_ce_max, floor_rt_max, int16_t, "s")
FLOOR_CONST_SELECT_2(max, floor_ce_max, floor_rt_max, int32_t, "i")
FLOOR_CONST_SELECT_2(max, floor_ce_max, floor_rt_max, int64_t, "l")
FLOOR_CONST_SELECT_2(max, floor_ce_max, floor_rt_max, uint8_t, "h")
FLOOR_CONST_SELECT_2(max, floor_ce_max, floor_rt_max, uint16_t, "t")
FLOOR_CONST_SELECT_2(max, floor_ce_max, floor_rt_max, uint32_t, "j")
FLOOR_CONST_SELECT_2(max, floor_ce_max, floor_rt_max, uint64_t, "m")
FLOOR_CONST_SELECT_2(max, floor_ce_max, floor_rt_max, half, "Dh")
FLOOR_CONST_SELECT_2(max, floor_ce_max, floor_rt_max, float, "f")
#if !defined(FLOOR_DEVICE_NO_DOUBLE)
FLOOR_CONST_SELECT_2(max, floor_ce_max, floor_rt_max, double, "d")
#endif

#undef FLOOR_CONST_SELECT_2
_LIBCPP_END_NAMESPACE_STD
#endif

// id handling through "variables"/objects (CUDA handles this slightly differently -> defined in cuda.hpp)
#if !defined(FLOOR_DEVICE_CUDA)
#define global_id fl::uint3 { get_global_id(0), get_global_id(1), get_global_id(2) }
#define global_size fl::uint3 { get_global_size(0), get_global_size(1), get_global_size(2) }
#define local_id fl::uint3 { get_local_id(0), get_local_id(1), get_local_id(2) }
#define local_size fl::uint3 { get_local_size(0), get_local_size(1), get_local_size(2) }
#define group_id fl::uint3 { get_group_id(0), get_group_id(1), get_group_id(2) }
#define group_size fl::uint3 { get_group_size(0), get_group_size(1), get_group_size(2) }
#define sub_group_id get_sub_group_id()
#define sub_group_local_id get_sub_group_local_id()
#define sub_group_size get_sub_group_size()
#define sub_group_count get_num_sub_groups()
#endif
// signal that these functions are unavailable
#if FLOOR_DEVICE_INFO_HAS_SUB_GROUPS == 0
floor_inline_always static uint32_t get_sub_group_id() __attribute__((unavailable("sub-group functionality not available")));
floor_inline_always static uint32_t get_sub_group_local_id() __attribute__((unavailable("sub-group functionality not available")));
floor_inline_always static uint32_t get_sub_group_size() __attribute__((unavailable("sub-group functionality not available")));
floor_inline_always static uint32_t get_num_sub_groups() __attribute__((unavailable("sub-group functionality not available")));
#endif

// always include const_math, rt_math and math constexpr-select functionality
#include <floor/constexpr/const_math.hpp>

// we can't use the 'h' suffix everywhere (e.g. Host-Compute with a vanilla clang)
// -> add custom '_h' UDL which can be used everywhere
#if defined(FLOOR_DEVICE_HOST_COMPUTE) && !defined(FLOOR_DEVICE_HOST_COMPUTE_IS_DEVICE)
static constexpr inline fl::half operator""_h (long double val) {
	return fl::half(val);
}
#else
static constexpr inline half operator""_h (long double val) {
	return half(val);
}
#endif

// always include vector lib/types
#if !defined(FLOOR_DEBUG)
#include <floor/math/vector_lib.hpp>
#else // vector_lib.hpp + a few more checks that might be useful in debug mode
#include <floor/math/vector_lib_checks.hpp>
#endif

// image types / enum (+enum helpers as this depends on it)
#include <floor/core/enum_helpers.hpp>
#include <floor/device/backend/image_types.hpp>

// parallel group ops interface
#include <floor/device/backend/group.hpp>
// implementation specific group op headers
#if defined(FLOOR_DEVICE_CUDA)
#include <floor/device/backend/cuda_group.hpp>
#elif defined(FLOOR_DEVICE_VULKAN)
#include <floor/device/backend/vulkan_group.hpp>
#elif defined(FLOOR_DEVICE_METAL)
#include <floor/device/backend/metal_group.hpp>
#elif defined(FLOOR_DEVICE_HOST_COMPUTE)
#include <floor/device/backend/host_group.hpp>
#endif

// device logging functions
#include <floor/device/backend/logger.hpp>

//! global memory buffer
//! TODO: set proper base alignment on the compiler side
#if !defined(FLOOR_DEVICE_METAL)
#define buffer __restrict device_global_buffer
#else // align is not supported with metal
// all global buffers are noalias/restrict with Metal 2.1+
#define buffer __restrict device_global_buffer
#endif
template <typename T> using device_global_buffer = global T*;

//! local memory buffer:
#if !defined(FLOOR_DEVICE_HOST_COMPUTE)

// NOTE: need to workaround the issue that "local" is not part of the type in cuda
#define local_buffer local device_local_buffer

template <typename T, size_t count_1> using device_local_buffer_1d = T[count_1];
template <typename T, size_t count_1, size_t count_2> using device_local_buffer_2d = T[count_1][count_2];
template <typename T, size_t count_1, size_t count_2, size_t count_3> using device_local_buffer_3d = T[count_1][count_2][count_3];
template <typename T, size_t count_1, size_t count_2 = 0, size_t count_3 = 0> using device_local_buffer =
	std::conditional_t<count_2 == 0, device_local_buffer_1d<T, count_1>,
				  std::conditional_t<count_3 == 0, device_local_buffer_2d<T, count_1, count_2>,
											  device_local_buffer_3d<T, count_1, count_2, count_3>>>;

#else // -> Host-Compute

#if !defined(FLOOR_DEVICE_HOST_COMPUTE_IS_DEVICE) // -> host
// NOTE: since this is "static", it should only ever be called (allocated + initialized) by a single thread once
#define local_buffer static device_local_buffer

template <typename T, size_t count_1, size_t count_2 = 0, size_t count_3 = 0>
class device_local_buffer {
protected:
	static constexpr uint32_t dim() { return (count_2 == 0 ? 1 : (count_3 == 0 ? 2 : 3)); }
	static constexpr size_t data_size() {
		constexpr const auto type_size = sizeof(T);
		switch(dim()) {
			case 1: return count_1 * type_size;
			case 2: return count_1 * count_2 * type_size;
			default: return count_1 * count_2 * count_3 * type_size;
		}
	}
	
	T* __attribute__((aligned(1024))) data;
	uint32_t offset;
	
	using type_1d = T;
	using type_2d = T[count_2];
	using type_3d = T[count_2][count_3];
	
public:
	T& operator[](const size_t& index) requires(dim() == 1) {
		return ((type_1d*)__builtin_assume_aligned((uint8_t*)data + floor_thread_local_memory_offset + offset, 128))[index];
	}
	const T& operator[](const size_t& index) const requires(dim() == 1) {
		return ((const type_1d*)__builtin_assume_aligned((const uint8_t*)data + floor_thread_local_memory_offset + offset, 128))[index];
	}
	
	T (&operator[](const size_t& index)) [count_2] requires(dim() == 2) {
		return ((type_2d*)__builtin_assume_aligned((uint8_t*)data + floor_thread_local_memory_offset + offset, 128))[index];
	}
	const T (&operator[](const size_t& index) const) [count_2] requires(dim() == 2) {
		return ((const type_2d*)__builtin_assume_aligned((const uint8_t*)data + floor_thread_local_memory_offset + offset, 128))[index];
	}
	
	T (&operator[](const size_t& index)) [count_2][count_3] requires(dim() == 3) {
		return ((type_3d*)__builtin_assume_aligned((uint8_t*)data + floor_thread_local_memory_offset + offset, 128))[index];
	}
	const T (&operator[](const size_t& index) const) [count_2][count_3] requires(dim() == 3) {
		return ((const type_3d*)__builtin_assume_aligned((const uint8_t*)data + floor_thread_local_memory_offset + offset, 128))[index];
	}
	
	floor_inline_always T (&as_array()) [count_1] {
		using array_1d_type = T[count_1];
		return *(array_1d_type*)__builtin_assume_aligned((uint8_t*)data + floor_thread_local_memory_offset + offset, 128);
	}
	floor_inline_always const T (&as_array() const) [count_1] {
		using array_1d_type = T[count_1];
		return *(const array_1d_type*)__builtin_assume_aligned((const uint8_t*)data + floor_thread_local_memory_offset + offset, 128);
	}
	
	device_local_buffer() noexcept : data((T*)__builtin_assume_aligned(floor_requisition_local_memory(data_size(), offset), 128)) {}
	
};
#else // -> host-device
// NOTE: since this is "static", it should only ever be called (allocated + initialized) by a single thread once
// NOTE: for host-device execution this can be a simple array (-> part of the per-instance BSS)
#define local_buffer alignas(1024) static __attribute__((used)) device_local_buffer
template <typename T, size_t count_1, size_t count_2 = 1, size_t count_3 = 1>
using device_local_buffer = T[count_1 * count_2 * count_3];
#endif

#endif

//! constant memory buffer
//! TODO: set proper base alignment on the compiler side
//! NOTE: again: need to workaround the issue that "constant" is not part of the type
#define constant_buffer __restrict constant device_constant_buffer
template <typename T> using device_constant_buffer = const T* const;

//! array for use with static constant memory
#define constant_array constant device_constant_array
template <class data_type, size_t array_size> using device_constant_array = data_type[array_size];

//! argument buffer
#if defined(FLOOR_DEVICE_OPENCL) || (defined(FLOOR_DEVICE_HOST_COMPUTE) && !defined(FLOOR_DEVICE_HOST_COMPUTE_IS_DEVICE))
template <typename T> using arg_buffer = const T;
#elif defined(FLOOR_DEVICE_CUDA) || defined(FLOOR_DEVICE_HOST_COMPUTE_IS_DEVICE)
template <typename T> using device_arg_buffer = const T&;
#define arg_buffer __attribute__((floor_arg_buffer)) __restrict device_arg_buffer
#elif defined(FLOOR_DEVICE_METAL)
template <typename T> using arg_buffer = const constant T& __restrict;
#elif defined(FLOOR_DEVICE_VULKAN)
template <typename T> using device_arg_buffer = const T;
#define arg_buffer __attribute__((floor_arg_buffer)) device_arg_buffer
#endif

//! generic parameter object/buffer
#if (defined(FLOOR_DEVICE_CUDA) || defined(FLOOR_DEVICE_OPENCL)) && !defined(FLOOR_DEVICE_PARAM_WORKAROUND)
template <typename T> using param = const T;
#elif defined(FLOOR_DEVICE_METAL) || defined(FLOOR_DEVICE_VULKAN) || defined(FLOOR_DEVICE_PARAM_WORKAROUND)
// the __restrict prevents the parameter unnecessarily being loaded multiple times
#define param __restrict device_param
template <typename T> using device_param = const constant T&;
#elif defined(FLOOR_DEVICE_HOST_COMPUTE)
template <typename T> using param = const T&;
#endif

//! image or buffer array parameter
#if defined(FLOOR_DEVICE_METAL) || defined(FLOOR_DEVICE_VULKAN) || defined(FLOOR_DEVICE_CUDA)
template <typename data_type, size_t array_size> using array_param = std::array<data_type, array_size>;
#elif defined(FLOOR_DEVICE_HOST_COMPUTE) || defined(FLOOR_DEVICE_OPENCL)
template <typename data_type, size_t array_size> using array_param = const global std::array<data_type, array_size>&;
#endif

// implementation specific image headers
#include <floor/device/backend/sampler.hpp>
#if defined(FLOOR_DEVICE_CUDA)
#include <floor/device/backend/cuda_image.hpp>
#elif defined(FLOOR_DEVICE_OPENCL)
#include <floor/device/backend/opencl_image.hpp>
#elif defined(FLOOR_DEVICE_VULKAN)
#include <floor/device/backend/vulkan_image.hpp>
#elif defined(FLOOR_DEVICE_METAL)
#include <floor/device/backend/metal_image.hpp>
#elif defined(FLOOR_DEVICE_HOST_COMPUTE)
#include <floor/device/backend/host_image.hpp>
#endif
#include <floor/device/backend/image.hpp>

// compute algorithms
#include <floor/device/backend/algorithm.hpp>

// software pack/unpack functions
#include <floor/device/backend/soft_pack.hpp>

// tessellation support
#include <floor/device/backend/tessellation.hpp>

// late function declarations that require any of the prior functionality
#if defined(FLOOR_DEVICE_METAL)
#include <floor/device/backend/metal_post.hpp>
#elif defined(FLOOR_DEVICE_VULKAN)
#include <floor/device/backend/vulkan_post.hpp>
#elif defined(FLOOR_DEVICE_HOST_COMPUTE)
#include <floor/device/backend/host_post.hpp>
#endif

// graphics builtin/id handling
#if defined(FLOOR_DEVICE_METAL) || defined(FLOOR_DEVICE_VULKAN) || defined(FLOOR_GRAPHICS_HOST_COMPUTE)
#define vertex_id get_vertex_id()
#define base_vertex_id get_base_vertex_id()
#define instance_id get_instance_id()
#define base_instance_id get_base_instance_id()
#define point_coord get_point_coord()
#define view_index get_view_index()
#define primitive_id get_primitive_id()
#define barycentric_coord get_barycentric_coord()
#define patch_id get_patch_id()
#define position_in_patch get_position_in_patch()
#endif

// enable assert support at the end of everything
// NOTE: we don't want this to be enabled for any STL or other libfloor code
#if defined(FLOOR_ASSERT) && FLOOR_ASSERT
#if defined(FLOOR_DEVICE_CUDA)
// can just call PTX exit
void floor_exit() __attribute__((noreturn)) {
	asm volatile("exit;");
}
#elif defined(FLOOR_DEVICE_METAL) || defined(FLOOR_DEVICE_VULKAN) || defined(FLOOR_DEVICE_OPENCL)
// must be handled on the compiler side
void floor_exit() __attribute__((noreturn)) asm("floor.exit");
#elif defined(FLOOR_DEVICE_HOST_COMPUTE)
// make this debuggable
#define floor_exit() __builtin_debugtrap()
#endif

#undef assert // undef previous
#if defined(FLOOR_DEVICE_METAL) || defined(FLOOR_DEVICE_VULKAN) || defined(FLOOR_DEVICE_HOST_COMPUTE_IS_DEVICE)
// we have no printf string argument support with soft-printf yet (TODO: update this once implemented)
#define assert(expr) \
	do { \
		if (__builtin_expect(!(expr), 0)) { \
			print("assert in " __FILE_NAME__ ":$", __LINE__); \
			floor_exit(); \
		} \
	} while (false)
#else // CUDA/OpenCL
#define assert(expr) \
	do { \
		if (__builtin_expect(!(expr), 0)) { \
			print("assert in " __FILE_NAME__ ":$: $", __LINE__, #expr); \
			floor_exit(); \
		} \
	} while (false)
#endif

#endif

#endif

#endif
