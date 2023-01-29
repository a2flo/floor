/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2023 Florian Ziesche
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

#if defined(FLOOR_COMPUTE_CUDA) || defined(FLOOR_COMPUTE_OPENCL) || defined(FLOOR_COMPUTE_METAL) || defined(FLOOR_COMPUTE_HOST) || defined(FLOOR_COMPUTE_VULKAN)

// libc++ STL functionality without (most of) the baggage
#if !defined(FLOOR_COMPUTE_HOST) || defined(FLOOR_COMPUTE_HOST_DEVICE)
#define _LIBCPP_NO_RTTI 1
#define _LIBCPP_BUILD_STATIC 1
#define _LIBCPP_NO_EXCEPTIONS 1
#define _LIBCPP_HAS_NO_THREADS 1
#define _LIBCPP_HAS_NO_VENDOR_AVAILABILITY_ANNOTATIONS 1
#define _LIBCPP_HAS_NO_ALIGNED_ALLOCATION 1
#define _LIBCPP_HAS_NO_PLATFORM_WAIT 1
#define assert(...)
#endif

// compute implementation specific headers (pre-std headers)
#if defined(FLOOR_COMPUTE_CUDA)
#include <floor/compute/device/cuda_pre.hpp>
#elif defined(FLOOR_COMPUTE_OPENCL)
#include <floor/compute/device/opencl_pre.hpp>
#elif defined(FLOOR_COMPUTE_VULKAN)
#include <floor/compute/device/vulkan_pre.hpp>
#elif defined(FLOOR_COMPUTE_METAL)
#include <floor/compute/device/metal_pre.hpp>
#elif defined(FLOOR_COMPUTE_HOST)
#include <floor/compute/device/host_pre.hpp>
#endif

// more integer types
#if !defined(FLOOR_COMPUTE_HOST)
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

// sets a required local size/dim
#if defined(FLOOR_COMPUTE) && (!defined(FLOOR_COMPUTE_HOST) || defined(FLOOR_COMPUTE_HOST_DEVICE))
#define kernel_local_size(x, y, z) __attribute__((reqd_work_group_size(x, y, z)))
#else // can't set this on the host
#define kernel_local_size(x, y, z)
#endif

// misc device information
#include <floor/compute/device/device_info.hpp>

// clang vector types that are needed in some places (image functions)
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
#if defined(FLOOR_COMPUTE_OPENCL) || defined(FLOOR_COMPUTE_METAL) || defined(FLOOR_COMPUTE_VULKAN) || defined(FLOOR_COMPUTE_CUDA)
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
#if !defined(const_func)
#define const_func __attribute__((const))
#endif

// stl headers that can be included before atomic functions
#include <type_traits>

using namespace std;

// atomics (needed before c++ headers)
#include <floor/compute/device/atomic_fallback.hpp>
// ring dependencies ftw
#define min(x, y) (x <= y ? x : y)
#define max(x, y) (x >= y ? x : y)
// add atomic compat/forwarder/alias functions
#include <floor/compute/device/atomic_compat.hpp>
#if defined(FLOOR_COMPUTE_CUDA)
#include <floor/compute/device/cuda_atomic.hpp>
#elif defined(FLOOR_COMPUTE_OPENCL) || defined(FLOOR_COMPUTE_VULKAN)
#include <floor/compute/device/opencl_atomic.hpp>
#elif defined(FLOOR_COMPUTE_METAL)
#include <floor/compute/device/metal_atomic.hpp>
#elif defined(FLOOR_COMPUTE_HOST)
#include <floor/compute/device/host_atomic.hpp>
#endif
#include <floor/compute/device/atomic_compat_post.hpp>
// undef above min/max, we want the std::min/max
#undef min
#undef max

// stl headers that must be included after atomic functions
#include <utility>
#include <initializer_list>
#include <functional>
#include <algorithm>
#include <atomic>
#include <array>

// decay_as, same as decay, but also removes the address space
template <typename T> struct decay_as { typedef decay_t<T> type; };
#if !defined(FLOOR_COMPUTE_HOST)
template <typename T> struct decay_as<__attribute__((global_as)) T> { typedef decay_t<T> type; };
template <typename T> struct decay_as<__attribute__((local_as)) T> { typedef decay_t<T> type; };
template <typename T> struct decay_as<__attribute__((constant_as)) T> { typedef decay_t<T> type; };
template <typename T> struct decay_as<__attribute__((generic_as)) T> { typedef decay_t<T> type; };
#endif
template <typename T> using decay_as_t = typename decay_as<T>::type;

// c++ stl "extensions"
#include <floor/core/cpp_ext.hpp>
#include <floor/constexpr/ext_traits.hpp>

// compute implementation specific headers
#if defined(FLOOR_COMPUTE_CUDA)
#include <floor/compute/device/cuda.hpp>
#elif defined(FLOOR_COMPUTE_OPENCL) || defined(FLOOR_COMPUTE_VULKAN)
#include <floor/compute/device/opencl.hpp>
#elif defined(FLOOR_COMPUTE_METAL)
#include <floor/compute/device/metal.hpp>
#elif defined(FLOOR_COMPUTE_HOST)
#include <floor/compute/device/host.hpp>
#endif

// min/max functions are problematic with most backends, c++14 requires/defines them to be constexpr,
// but backends define these as normal non-constexpr runtime functions, which will be taken over
// the existing templated min/max functions (which are constexpr)
// -> use compiler/enable_if voodoo to fix it (see const_math.hpp for how this works)
#if !defined(FLOOR_COMPUTE_HOST)
_LIBCPP_BEGIN_NAMESPACE_STD
#define FLOOR_CONST_SELECT_2(func_name, ce_func, rt_func, type_name, type_suffix) \
	static __attribute__((always_inline)) type_name func_name (type_name x, type_name y) asm("floor__std_" #func_name type_suffix ); \
	static __attribute__((always_inline)) constexpr type_name func_name (type_name x, type_name y) \
	__attribute__((enable_if(!__builtin_constant_p(&x), ""))) \
	__attribute__((enable_if(!__builtin_constant_p(&y), ""))) asm("floor__std_" #func_name type_suffix ); \
	\
	static __attribute__((always_inline)) constexpr type_name func_name (type_name x, type_name y) \
	__attribute__((enable_if(!__builtin_constant_p(&x), ""))) \
	__attribute__((enable_if(!__builtin_constant_p(&y), ""))) { \
		return std:: ce_func (x, y); \
	} \
	static __attribute__((always_inline)) type_name func_name (type_name x, type_name y) { \
		return rt_func (x, y) ; \
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
	FLOOR_CONST_SELECT_2(min, floor_ce_min, fmin, half, "h")
	FLOOR_CONST_SELECT_2(min, floor_ce_min, fmin, float, "f")
#if !defined(FLOOR_COMPUTE_NO_DOUBLE)
	FLOOR_CONST_SELECT_2(min, floor_ce_min, fmin, double, "d")
#endif
	
	FLOOR_CONST_SELECT_2(max, floor_ce_max, floor_rt_max, int8_t, "c")
	FLOOR_CONST_SELECT_2(max, floor_ce_max, floor_rt_max, int16_t, "s")
	FLOOR_CONST_SELECT_2(max, floor_ce_max, floor_rt_max, int32_t, "i")
	FLOOR_CONST_SELECT_2(max, floor_ce_max, floor_rt_max, int64_t, "l")
	FLOOR_CONST_SELECT_2(max, floor_ce_max, floor_rt_max, uint8_t, "h")
	FLOOR_CONST_SELECT_2(max, floor_ce_max, floor_rt_max, uint16_t, "t")
	FLOOR_CONST_SELECT_2(max, floor_ce_max, floor_rt_max, uint32_t, "j")
	FLOOR_CONST_SELECT_2(max, floor_ce_max, floor_rt_max, uint64_t, "m")
	FLOOR_CONST_SELECT_2(max, floor_ce_max, fmax, half, "h")
	FLOOR_CONST_SELECT_2(max, floor_ce_max, fmax, float, "f")
#if !defined(FLOOR_COMPUTE_NO_DOUBLE)
	FLOOR_CONST_SELECT_2(max, floor_ce_max, fmax, double, "d")
#endif
	
#undef FLOOR_CONST_SELECT_2
_LIBCPP_END_NAMESPACE_STD
#endif

// id handling through "variables"/objects (cuda handles this slightly differently -> defined in cuda.hpp)
#if !defined(FLOOR_COMPUTE_CUDA)
#define global_id uint3 { get_global_id(0), get_global_id(1), get_global_id(2) }
#define global_size uint3 { get_global_size(0), get_global_size(1), get_global_size(2) }
#define local_id uint3 { get_local_id(0), get_local_id(1), get_local_id(2) }
#define local_size uint3 { get_local_size(0), get_local_size(1), get_local_size(2) }
#define group_id uint3 { get_group_id(0), get_group_id(1), get_group_id(2) }
#define group_size uint3 { get_group_size(0), get_group_size(1), get_group_size(2) }
#define sub_group_id get_sub_group_id()
#define sub_group_id_1d sub_group_id
#define sub_group_id_2d sub_group_id
#define sub_group_id_3d sub_group_id
#define sub_group_local_id get_sub_group_local_id()
#define sub_group_size get_sub_group_size()
#define sub_group_count get_num_sub_groups()
#endif
// signal that these functions are unavailable
#if FLOOR_COMPUTE_INFO_HAS_SUB_GROUPS == 0
floor_inline_always static uint32_t get_sub_group_id() __attribute__((unavailable("sub-group functionality not available")));
floor_inline_always static uint32_t get_sub_group_local_id() __attribute__((unavailable("sub-group functionality not available")));
floor_inline_always static uint32_t get_sub_group_size() __attribute__((unavailable("sub-group functionality not available")));
floor_inline_always static uint32_t get_num_sub_groups() __attribute__((unavailable("sub-group functionality not available")));
#endif

// always include const_math, rt_math and math constexpr-select functionality
#include <floor/constexpr/const_math.hpp>

// we can't use the 'h' suffix everywhere (e.g. Host-Compute with a vanilla clang)
// -> add custom '_h' UDL which can be used everywhere
static constexpr inline half operator "" _h (long double val) {
	return half(val);
}

// always include vector lib/types
#if !defined(FLOOR_DEBUG)
#include <floor/math/vector_lib.hpp>
#else // vector_lib.hpp + a few more checks that might be useful in debug mode
#include <floor/math/vector_lib_checks.hpp>
#endif

// image types / enum (+enum helpers as this depends on it)
#include <floor/core/enum_helpers.hpp>
#include <floor/compute/device/image_types.hpp>

// device logging functions
#include <floor/compute/device/logger.hpp>

//! global memory buffer
//! TODO: set proper base alignment on the compiler side
#if !defined(FLOOR_COMPUTE_METAL)
#define buffer __restrict compute_global_buffer
#else // align is not supported with metal
#if FLOOR_COMPUTE_METAL_MAJOR > 2 || (FLOOR_COMPUTE_METAL_MAJOR == 2 && FLOOR_COMPUTE_METAL_MINOR >= 1) // all global buffers are noalias/restrict with Metal 2.1+
#define buffer __restrict compute_global_buffer
#else
#define buffer compute_global_buffer
#endif
#endif
template <typename T> using compute_global_buffer = global T*;

//! local memory buffer:
#if !defined(FLOOR_COMPUTE_HOST)

// NOTE: need to workaround the issue that "local" is not part of the type in cuda
#define local_buffer local compute_local_buffer

template <typename T, size_t count_1> using compute_local_buffer_1d = T[count_1];
template <typename T, size_t count_1, size_t count_2> using compute_local_buffer_2d = T[count_1][count_2];
template <typename T, size_t count_1, size_t count_2, size_t count_3> using compute_local_buffer_3d = T[count_1][count_2][count_3];
template <typename T, size_t count_1, size_t count_2 = 0, size_t count_3 = 0> using compute_local_buffer =
	conditional_t<count_2 == 0, compute_local_buffer_1d<T, count_1>,
				  conditional_t<count_3 == 0, compute_local_buffer_2d<T, count_1, count_2>,
											  compute_local_buffer_3d<T, count_1, count_2, count_3>>>;

#else // -> host-compute

#if !defined(FLOOR_COMPUTE_HOST_DEVICE) // -> host
// NOTE: since this is "static", it should only ever be called (allocated + initialized) by a single thread once
#define local_buffer static compute_local_buffer

template <typename T, size_t count_1, size_t count_2 = 0, size_t count_3 = 0>
class compute_local_buffer {
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
	
	typedef T type_1d;
	typedef T type_2d[count_2];
	typedef T type_3d[count_2][count_3];
	
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
		typedef T array_1d_type[count_1];
		return *(array_1d_type*)__builtin_assume_aligned((uint8_t*)data + floor_thread_local_memory_offset + offset, 128);
	}
	floor_inline_always const T (&as_array() const) [count_1] {
		typedef T array_1d_type[count_1];
		return *(const array_1d_type*)__builtin_assume_aligned((const uint8_t*)data + floor_thread_local_memory_offset + offset, 128);
	}
	
	compute_local_buffer() noexcept : data((T*)__builtin_assume_aligned(floor_requisition_local_memory(data_size(), offset), 128)) {}
	
};
#else // -> host-device
// NOTE: since this is "static", it should only ever be called (allocated + initialized) by a single thread once
// NOTE: for host-device execution this can be a simple array (-> part of the per-instance BSS)
#define local_buffer static __attribute__((aligned(1024))) compute_local_buffer
template <typename T, size_t count_1, size_t count_2 = 1, size_t count_3 = 1>
using compute_local_buffer = T[count_1 * count_2 * count_3];
#endif

#endif

//! constant memory buffer
//! TODO: set proper base alignment on the compiler side
//! NOTE: again: need to workaround the issue that "constant" is not part of the type
#define constant_buffer __restrict constant compute_constant_buffer
template <typename T> using compute_constant_buffer = const T* const;

//! array for use with static constant memory
#define constant_array constant compute_constant_array
template <class data_type, size_t array_size> using compute_constant_array = data_type[array_size];

//! argument buffer
#if defined(FLOOR_COMPUTE_OPENCL) || (defined(FLOOR_COMPUTE_HOST) && !defined(FLOOR_COMPUTE_HOST_DEVICE))
template <typename T> using arg_buffer = const T;
#elif defined(FLOOR_COMPUTE_CUDA) || defined(FLOOR_COMPUTE_HOST_DEVICE)
template <typename T> using compute_arg_buffer = const T&;
#define arg_buffer __attribute__((floor_arg_buffer)) __restrict compute_arg_buffer
#elif defined(FLOOR_COMPUTE_METAL)
template <typename T> using arg_buffer = const constant T& __restrict;
#elif defined(FLOOR_COMPUTE_VULKAN)
template <typename T> using compute_arg_buffer = const T;
#define arg_buffer __attribute__((floor_arg_buffer)) compute_arg_buffer
#endif

//! generic parameter object/buffer
#if (defined(FLOOR_COMPUTE_CUDA) || defined(FLOOR_COMPUTE_OPENCL)) && !defined(FLOOR_COMPUTE_PARAM_WORKAROUND)
template <typename T> using param = const T;
#elif defined(FLOOR_COMPUTE_METAL) || defined(FLOOR_COMPUTE_VULKAN) || defined(FLOOR_COMPUTE_PARAM_WORKAROUND)
// the __restrict prevents the parameter unnecessarily being loaded multiple times
#define param __restrict compute_param
template <typename T> using compute_param = const constant T&;
#elif defined(FLOOR_COMPUTE_HOST)
template <typename T> using param = const T&;
#endif

// implementation specific image headers
#include <floor/compute/device/sampler.hpp>
#if defined(FLOOR_COMPUTE_CUDA)
#include <floor/compute/device/cuda_image.hpp>
#elif defined(FLOOR_COMPUTE_OPENCL)
#include <floor/compute/device/opencl_image.hpp>
#elif  defined(FLOOR_COMPUTE_VULKAN)
#include <floor/compute/device/vulkan_image.hpp>
#elif defined(FLOOR_COMPUTE_METAL)
#include <floor/compute/device/metal_image.hpp>
#elif defined(FLOOR_COMPUTE_HOST)
#include <floor/compute/device/host_image.hpp>
#endif
#include <floor/compute/device/image.hpp>

// compute algorithms
#include <floor/compute/device/compute_algorithm.hpp>

// software pack/unpack functions
#include <floor/compute/device/soft_pack.hpp>

// tessellation support
#include <floor/compute/device/tessellation.hpp>

// late function declarations that require any of the prior functionality
#if defined(FLOOR_COMPUTE_METAL)
#include <floor/compute/device/metal_post.hpp>
#elif defined(FLOOR_COMPUTE_VULKAN)
#include <floor/compute/device/vulkan_post.hpp>
#elif defined(FLOOR_COMPUTE_HOST)
#include <floor/compute/device/host_post.hpp>
#endif

// graphics builtin/id handling
#if defined(FLOOR_COMPUTE_METAL) || defined(FLOOR_COMPUTE_VULKAN) || defined(FLOOR_GRAPHICS_HOST)
#define vertex_id get_vertex_id()
#define instance_id get_instance_id()
#define point_coord get_point_coord()
#define view_index get_view_index()
#define primitive_id get_primitive_id()
#define barycentric_coord get_barycentric_coord()
#define patch_id get_patch_id()
#define position_in_patch get_position_in_patch()
#endif

#endif

#endif
