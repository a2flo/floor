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

#ifndef __FLOOR_COMPUTE_SUPPORT_HPP__
#define __FLOOR_COMPUTE_SUPPORT_HPP__

// TODO: clean this up (srsly ...) + possible move into separate cuda/opencl headers

#if defined(__CUDA_CLANG__)

//
#define kernel extern "C" __attribute__((cuda_kernel))

// map address space keywords
#define global
#define local __attribute__((cuda_local))
#define constant __attribute__((cuda_constant))

// TODO: properly do this
#define get_global_id(dim) (size_t(bid_x * bdim_x + tid_x))

// have some special magic:
// NOTE: this should be compiled with at least -O1, otherwise dead code won't be eliminated
// NOTE: there is sadly no other way of doing this (short of pre-preprocessing the source code)
struct __special_reg { int x, y, z; };

#define tid_x __builtin_ptx_read_tid_x()
#define tid_y __builtin_ptx_read_tid_y()
#define tid_z __builtin_ptx_read_tid_z()
__attribute__((device, always_inline, flatten)) static inline __special_reg __get_thread_idx() {
	return __special_reg { tid_x, tid_y, tid_z };
}
#define threadIdx __get_thread_idx()

#define ctaid_x __builtin_ptx_read_ctaid_x()
#define ctaid_y __builtin_ptx_read_ctaid_y()
#define ctaid_z __builtin_ptx_read_ctaid_z()
__attribute__((device, always_inline, flatten)) static inline __special_reg __get_block_idx() {
	return __special_reg { ctaid_x, ctaid_y, ctaid_z };
}
#define blockIdx __get_block_idx()

#define ntid_x __builtin_ptx_read_ntid_x()
#define ntid_y __builtin_ptx_read_ntid_y()
#define ntid_z __builtin_ptx_read_ntid_z()
__attribute__((device, always_inline, flatten)) static inline __special_reg __get_block_dim() {
	return __special_reg { ntid_x, ntid_y, ntid_z };
}
#define blockDim __get_block_dim()

#define nctaid_x __builtin_ptx_read_nctaid_x()
#define nctaid_y __builtin_ptx_read_nctaid_y()
#define nctaid_z __builtin_ptx_read_nctaid_z()
__attribute__((device, always_inline, flatten)) static inline __special_reg __get_grid_dim() {
	return __special_reg { nctaid_x, nctaid_y, nctaid_z };
}
#define gridDim __get_grid_dim()

#define laneId __builtin_ptx_read_laneid()
#define warpId __builtin_ptx_read_warpid()
#define warpSize __builtin_ptx_read_nwarpid()

// misc (not directly defined by cuda?)
#define smId __builtin_ptx_read_smid()
#define smDim __builtin_ptx_read_nsmid()
#define gridId __builtin_ptx_read_gridid()

#define lanemask_eq __builtin_ptx_read_lanemask_eq()
#define lanemask_le __builtin_ptx_read_lanemask_le()
#define lanemask_lt __builtin_ptx_read_lanemask_lt()
#define lanemask_ge __builtin_ptx_read_lanemask_ge()
#define lanemask_gt __builtin_ptx_read_lanemask_gt()

#define ptx_clock __builtin_ptx_read_clock()
#define ptx_clock64 __builtin_ptx_read_clock64()

// some aliases for easier use:
// (tid_* already defined above)
#define bid_x ctaid_x
#define bid_y ctaid_y
#define bid_z ctaid_z
#define bdim_x ntid_x
#define bdim_y ntid_y
#define bdim_z ntid_z
#define gdim_x nctaid_x
#define gdim_y nctaid_y
#define gdim_z nctaid_z
#define lane_id laneId
#define warp_id warpId
#define warp_size warpSize
#define sm_id smId
#define sm_dim smDim
#define grid_id gridId

// provided by libcudart:
/*extern "C" {
	extern __device__ __device_builtin__ int printf(const char*, ...);
};*/

// misc types
typedef __signed char int8_t;
typedef short int int16_t;
typedef int int32_t;
typedef long long int int64_t;
typedef unsigned char uint8_t;
typedef unsigned short int uint16_t;
typedef unsigned int uint32_t;
typedef unsigned long long int uint64_t;

// TODO: arch size support
#if defined(PLATFORM_X32)
typedef uint32_t size_t;
typedef int32_t ssize_t;
#elif defined(PLATFORM_X64)
typedef uint64_t size_t;
typedef int64_t ssize_t;
#endif

//#include <floor/compute/cuda/cuda_device_functions.hpp>
// TODO: add proper cuda math support
float pow(float a, float b) { return __nvvm_ex2_approx_ftz_f(b * __nvvm_lg2_approx_ftz_f(a)); }
float sqrt(float a) { return __nvvm_sqrt_rn_ftz_f(a); }
float sin(float a) { return __nvvm_sin_approx_ftz_f(a); }
float cos(float a) { return __nvvm_cos_approx_ftz_f(a); }
float tan(float a) { return sin(a) / cos(a); }

#elif defined(__SPIR_CLANG__)
// TODO: should really use this header somehow!
//#include "opencl_spir.h"
#if !defined(FLOOR_COMPUTE_NO_DOUBLE)
#pragma OPENCL EXTENSION cl_khr_fp64 : enable
#endif

// misc types
typedef char int8_t;
typedef short int int16_t;
typedef int int32_t;
typedef long int int64_t;
typedef unsigned char uint8_t;
typedef unsigned short int uint16_t;
typedef unsigned int uint32_t;
typedef unsigned long int uint64_t;

typedef unsigned short int ushort;
typedef unsigned int uint;
typedef unsigned long int ulong;

#if defined(__SPIR32__)
typedef uint size_t;
typedef int ssize_t;
#elif defined (__SPIR64__)
typedef unsigned long int size_t;
typedef long int ssize_t;
#endif

#define const_func __attribute__((const))
size_t const_func get_global_id(uint dimindx);

float const_func __attribute__((overloadable)) fmod(float, float);
float const_func __attribute__((overloadable)) sqrt(float);
float const_func __attribute__((overloadable)) rsqrt(float);
float const_func __attribute__((overloadable)) fabs(float);
float const_func __attribute__((overloadable)) floor(float);
float const_func __attribute__((overloadable)) ceil(float);
float const_func __attribute__((overloadable)) round(float);
float const_func __attribute__((overloadable)) trunc(float);
float const_func __attribute__((overloadable)) rint(float);
float const_func __attribute__((overloadable)) sin(float);
float const_func __attribute__((overloadable)) cos(float);
float const_func __attribute__((overloadable)) tan(float);
float const_func __attribute__((overloadable)) asin(float);
float const_func __attribute__((overloadable)) acos(float);
float const_func __attribute__((overloadable)) atan(float);
float const_func __attribute__((overloadable)) atan2(float, float);
float const_func __attribute__((overloadable)) fma(float, float, float);
float const_func __attribute__((overloadable)) exp(float x);
float const_func __attribute__((overloadable)) log(float x);
float const_func __attribute__((overloadable)) pow(float x, float y);

#if !defined(FLOOR_COMPUTE_NO_DOUBLE)
double const_func __attribute__((overloadable)) fmod(double, double);
double const_func __attribute__((overloadable)) sqrt(double);
double const_func __attribute__((overloadable)) rsqrt(double);
double const_func __attribute__((overloadable)) fabs(double);
double const_func __attribute__((overloadable)) floor(double);
double const_func __attribute__((overloadable)) ceil(double);
double const_func __attribute__((overloadable)) round(double);
double const_func __attribute__((overloadable)) trunc(double);
double const_func __attribute__((overloadable)) rint(double);
double const_func __attribute__((overloadable)) sin(double);
double const_func __attribute__((overloadable)) cos(double);
double const_func __attribute__((overloadable)) tan(double);
double const_func __attribute__((overloadable)) asin(double);
double const_func __attribute__((overloadable)) acos(double);
double const_func __attribute__((overloadable)) atan(double);
double const_func __attribute__((overloadable)) atan2(double, double);
double const_func __attribute__((overloadable)) fma(double, double, double);
double const_func __attribute__((overloadable)) exp(double x);
double const_func __attribute__((overloadable)) log(double x);
double const_func __attribute__((overloadable)) pow(double x, double y);
#endif

// can't match/produce _Z6printfPrU3AS2cz with clang/llvm 3.5, because a proper "restrict" is missing in c++ mode,
// but apparently extern c printf is working fine with intels and amds implementation, so just use that ...
extern "C" int printf(const char __constant* st, ...);

// NOTE: I purposefully didn't enable these as aliases in clang,
// so that they can be properly redirected on any other target (cuda/metal/host)
// -> need to add simple macro aliases here
#define global __attribute__((opencl_global))
#define constant __attribute__((opencl_constant))
#define local __attribute__((opencl_local))
// abuse the section attribute for now, because clang/llvm won't emit kernel functions with "spir_kernel" calling convention
#define kernel __kernel __attribute__((section("spir_kernel")))

#elif defined(__METAL_CLANG__)

#pragma OPENCL EXTENSION cl_khr_fp64 : enable
#define global __attribute__((opencl_global))
#define constant __attribute__((opencl_constant))
#define local __attribute__((opencl_local))
#define kernel extern "C" __kernel

// misc types
typedef char int8_t;
typedef short int int16_t;
typedef int int32_t;
typedef long int int64_t;
typedef unsigned char uint8_t;
typedef unsigned short int uint16_t;
typedef unsigned int uint32_t;
typedef unsigned long int uint64_t;

typedef __SIZE_TYPE__ size_t;
typedef __PTRDIFF_TYPE__ ssize_t;
typedef __SIZE_TYPE__ uintptr_t;
typedef __PTRDIFF_TYPE__ intptr_t;

// straightforward wrapping, use the fast_* version when possible
#define metal_func inline __attribute__((always_inline))
metal_func float sqrt(float) asm("air.fast_sqrt.f32");
metal_func float rsqrt(float) asm("air.fast_rsqrt.f32");
metal_func float fabs(float) asm("air.fast_fabs.f32");
metal_func float fmin(float, float) asm("air.fast_fmin.f32");
metal_func float fmax(float, float) asm("air.fast_fmax.f32");
metal_func float floor(float) asm("air.fast_floor.f32");
metal_func float ceil(float) asm("air.fast_ceil.f32");
metal_func float round(float) asm("air.fast_round.f32");
metal_func float trunc(float) asm("air.fast_trunc.f32");
metal_func float rint(float) asm("air.fast_rint.f32");
metal_func float sin(float) asm("air.fast_sin.f32");
metal_func float cos(float) asm("air.fast_cos.f32");
metal_func float tan(float) asm("air.fast_tan.f32");
metal_func float asin(float) asm("air.fast_asin.f32");
metal_func float acos(float) asm("air.fast_acos.f32");
metal_func float atan(float) asm("air.fast_atan.f32");
metal_func float atan2(float, float) asm("air.fast_atan2.f32");
metal_func float fma(float, float, float) asm("air.fma.f32");;
metal_func float exp(float) asm("air.fast_exp.f32");
metal_func float log(float) asm("air.fast_log.f32");
metal_func float pow(float, float) asm("air.fast_pow.f32");
metal_func float fmod(float, float) asm("air.fast_fmod.f32");

metal_func int32_t mulhi(int32_t x, int32_t y) asm("air.mul_hi.i32");
metal_func uint32_t mulhi(uint32_t x, uint32_t y) asm("air.mul_hi.u.i32");
metal_func int64_t mulhi(int64_t x, int64_t y) asm("air.mul_hi.i64");
metal_func uint64_t mulhi(uint64_t x, uint64_t y) asm("air.mul_hi.u.i64");

metal_func uint32_t madsat(uint32_t, uint32_t, uint32_t) asm("air.mad_sat.u.i32");

// would usually have to provide these as kernel arguments in metal, but this works as well
// (thx for providing these apple, interesting cl_kernel_air64.h and cl_kernel.h you have there ;))
// NOTE: these all do and have to return 32-bit values, otherwise bad things(tm) will happen
uint32_t get_global_id(uint32_t dimindx) asm("air.get_global_id.i32");
uint32_t get_local_id (uint32_t dimindx) asm("air.get_local_id.i32");
uint32_t get_group_id(uint32_t dimindx) asm("air.get_group_id.i32");
uint32_t get_work_dim() asm("air.get_work_dim.i32");
uint32_t get_global_size(uint32_t dimindx) asm("air.get_global_size.i32");
uint32_t get_global_offset(uint32_t dimindx) asm("air.get_global_offset.i32");
uint32_t get_local_size(uint32_t dimindx) asm("air.get_local_size.i32");
uint32_t get_num_groups(uint32_t dimindx) asm("air.get_num_groups.i32");
uint32_t get_global_linear_id() asm("air.get_global_linear_id.i32");
uint32_t get_local_linear_id() asm("air.get_local_linear_id.i32");

#endif

#if defined(__CUDA_CLANG__) || defined(__SPIR_CLANG__) || defined(__METAL_CLANG__)
// libc++ stl functionality without (most of) the baggage
#include <utility>
#include <type_traits>

_LIBCPP_BEGIN_NAMESPACE_STD

// <array> replacement
template <class data_type, size_t array_size>
struct _LIBCPP_TYPE_VIS_ONLY array {
	data_type elems[array_size > 0 ? array_size : 1];

	constexpr size_t size() const {
		return array_size;
	}

	constexpr data_type& operator[](const size_t& index) { return elems[index]; }
	constexpr const data_type& operator[](const size_t& index) const { return elems[index]; }
};

// std::min / std::max replacements
template <class data_type>
constexpr data_type min(const data_type& lhs, const data_type& rhs) {
	return (lhs < rhs ? lhs : rhs);
}
template <class data_type>
constexpr data_type max(const data_type& lhs, const data_type& rhs) {
	return (lhs > rhs ? lhs : rhs);
}

_LIBCPP_END_NAMESPACE_STD
using namespace std;

// always include const_math (and const_select) functionality
#include <floor/constexpr/const_math.hpp>

// buffer / local_buffer / const_buffer / param target-specific specialization/implementation
namespace floor_compute {
	template <typename T> struct indirect_type_wrapper {
		T elem;
		
		indirect_type_wrapper(const T& obj) : elem(obj) {}
		indirect_type_wrapper& operator=(const T& obj) {
			elem = obj;
			return *this;
		}
		const T& operator*() const { return elem; }
		T& operator*() { return elem; }
		const T* const operator->() const { return &elem; }
		T* operator->() { return &elem; }
		operator T() const { return elem; }
	};
	template <typename T> struct direct_type_wrapper : T {
		using T::T;
		direct_type_wrapper& operator=(const T& obj) {
			*((T*)this) = obj;
			return *this;
		}
		const T& operator*() const { return *this; }
		T& operator*() { return *this; }
		const T* const operator->() const { return this; }
		T* operator->() { return this; }
	};
}

#if defined(__SPIR_CLANG__)
//! global memory buffer
template <typename T> using buffer = global floor_compute::indirect_type_wrapper<T>*;
//! local memory buffer
template <typename T> using local_buffer = local floor_compute::indirect_type_wrapper<T>*;
//! constant memory buffer
template <typename T> using const_buffer = constant const floor_compute::indirect_type_wrapper<T>* const;
//! generic parameter object/buffer
template <typename T,
		  typename param_wrapper = const conditional_t<
			  is_fundamental<T>::value,
			  floor_compute::indirect_type_wrapper<T>,
			  floor_compute::direct_type_wrapper<T>>>
using param = const param_wrapper;
//! array<> for use with static constant memory
template <class data_type, size_t array_size> using const_array = std::array<data_type, array_size>;

#elif defined(__CUDA_CLANG__)
//! global memory buffer
template <typename T> using buffer = floor_compute::indirect_type_wrapper<T>*;
//! local memory buffer
template <typename T> using local_buffer = local floor_compute::indirect_type_wrapper<T>*;
//! constant memory buffer
template <typename T> using const_buffer = constant const floor_compute::indirect_type_wrapper<T>* const;
//! generic parameter object/buffer
template <typename T,
		  typename param_wrapper = const conditional_t<
			  is_fundamental<T>::value,
			  floor_compute::indirect_type_wrapper<T>,
			  floor_compute::direct_type_wrapper<T>>>
using param = const param_wrapper;
//! array<> for use with static constant memory
template <class data_type, size_t array_size> using const_array = std::array<data_type, array_size>;

#elif defined(__METAL_CLANG__)
namespace floor_compute {
	//! generic loader from global/local/constant memory to private memory
	//! NOTE on efficiency: llvm is amazingly proficient at optimizing this to loads of any overlayed type(s),
	//!                     e.g. a load of a "struct { float2 a, b; }" will result in a <4 x float> load.
	template <typename T> struct address_space_loader {
		static T load(global const T* from) {
			uint8_t ints[sizeof(T)];
			for(size_t i = 0; i < (sizeof(T)); ++i) {
				ints[i] = *(((global const uint8_t*)from) + i);
			}
			return *(T*)&ints[0];
		}
		static T load(local const T* from) {
			uint8_t ints[sizeof(T)];
			for(size_t i = 0; i < (sizeof(T)); ++i) {
				ints[i] = *(((local const uint8_t*)from) + i);
			}
			return *(T*)&ints[0];
		}
		static T load(constant const T* from) {
			uint8_t ints[sizeof(T)];
			for(size_t i = 0; i < (sizeof(T)); ++i) {
				ints[i] = *(((constant const uint8_t*)from) + i);
			}
			return *(T*)&ints[0];
		}
		static T load(constant const T* from, const size_t& index) {
			uint8_t ints[sizeof(T)];
			for(size_t i = index * sizeof(T), count = i + sizeof(T); i < count; ++i) {
				ints[i] = *(((constant const uint8_t*)from) + i);
			}
			return *(T*)&ints[0];
		}
	};
	
	//! adaptor to access memory in a global/local/constant address space, with support for explicit and implicit stores and loads.
	//! NOTE: this is necessary for any class that uses the "this" pointer to access memory in a global/local/constant address space.
	template <typename contained_type, typename as_ptr_type, bool can_read = true, bool can_write = true>
	struct address_space_adaptor {
		contained_type elem;
		
		constexpr address_space_adaptor(const contained_type& elem_) : elem(elem_) {}
		
		//! explicit load
		template <bool can_read_ = can_read, std::enable_if_t<can_read_>* = nullptr>
		contained_type load() {
			return contained_type::load(((const as_ptr_type)this));
		}
		template <bool can_read_ = can_read, std::enable_if_t<!can_read_>* = nullptr>
		contained_type load() {
			static_assert(can_write, "load not supported in this address space!");
			return contained_type {};
		}
		
		//! implicit load (convert to contained_type in private/default address space)
		template <bool can_read_ = can_read, std::enable_if_t<can_read_>* = nullptr>
		operator contained_type() {
			return load();
		}
		template <bool can_read_ = can_read, std::enable_if_t<!can_read_>* = nullptr>
		operator contained_type() {
			static_assert(can_write, "load not supported in this address space!");
			return contained_type {};
		}
		
		//! explicit store
		template <bool can_write_ = can_write, std::enable_if_t<can_write_>* = nullptr>
		void store(const contained_type& vec) {
			contained_type::store(((as_ptr_type)this), vec);
		}
		template <bool can_write_ = can_write, std::enable_if_t<!can_write_>* = nullptr>
		void store(const contained_type&) {
			static_assert(can_write, "store not supported in this address space!");
		}
		
		//! implicit store (assign)
		template <typename T, bool can_write_ = can_write, std::enable_if_t<can_write_>* = nullptr>
		address_space_adaptor& operator=(const T& obj) {
			store(obj);
			return *this;
		}
		template <typename T, bool can_write_ = can_write, std::enable_if_t<!can_write_>* = nullptr>
		address_space_adaptor& operator=(const T&) {
			static_assert(can_write, "store not supported in this address space!");
			return *this;
		}
		
		//! r/w proxy (yo dawg, i herd you like wrapping, so I put a wrapper in your wrapper ...)
		template <typename U> class proxy {
		protected:
			U obj;
			
		public:
			proxy(const U& obj_) noexcept : obj(obj_) {}
			proxy& operator=(const proxy&) = delete;
			
			inline U& operator*() noexcept {
				return obj;
			}
			inline U* operator->() noexcept {
				return &obj;
			}
		};
		
		//! r/o proxy
		template <typename U> class const_proxy {
		protected:
			const U obj;
			
		public:
			const_proxy(const U& obj_) noexcept : obj(obj_) {}
			const_proxy& operator=(const const_proxy&) = delete;
			
			inline const U& operator*() const noexcept {
				return obj;
			}
			inline const U* const operator->() const noexcept {
				return &obj;
			}
		};
		
		//! implicit load via "pointer access" (returns a read/write proxy object)
		template <bool can_write_ = can_write, std::enable_if_t<can_write_>* = nullptr>
		proxy<contained_type> operator->() {
			return { load() };
		}
		//! implicit load via "pointer access" (returns a const/read-only proxy object)
		template <bool can_write_ = can_write, std::enable_if_t<!can_write_>* = nullptr>
		const_proxy<contained_type> operator->() {
			return { load() };
		}
		
		//! explicit load via deref (provided for completeness sake, implicit loads should usually suffice,
		//! with other explicit loads being the preferred way of clarifying "this is an explicit load")
		contained_type operator*() {
			return { load() };
		}
		
	};
	
	//! template voodoo, false if T has no load function for constant memory
	template <typename T, typename = void> struct has_load_function : false_type {};
	//! template voodoo, true if T has a load function for constant memory
	template <typename T> struct has_load_function<T, decltype(T::load((constant const T*)nullptr), void())> : true_type {};
	
	//! for internal use (wraps a struct or class and provides an automatic load function)
	template <typename T> struct param_buffer_container : T, address_space_loader<param_buffer_container<T>> {};
	
	//! for internal use (wraps a fundamental / non-struct or -class type and provides an automatic load function)
	template <typename T>
	struct param_adaptor : address_space_loader<param_adaptor<T>> {
		T elem;
		constexpr operator T() const noexcept { return elem; }
	};
}

//! global memory buffer
template <typename T> using buffer = global floor_compute::address_space_adaptor<T, global T*, true, !is_const<T>()>*;
//! local memory buffer
template <typename T> using local_buffer = local floor_compute::address_space_adaptor<T, local T*, true, !is_const<T>()>*;
//! constant memory buffer
template <typename T> using const_buffer = constant floor_compute::address_space_adaptor<const T, constant const T*, true, false>*;
//! generic parameter object/buffer (stored in constant memory)
template <typename T,
		  typename param_wrapper = const conditional_t<
			  is_fundamental<T>::value,
			  floor_compute::param_adaptor<T>,
			  conditional_t<
				  floor_compute::has_load_function<T>::value,
				  T,
				  floor_compute::param_buffer_container<T>>>>
using param = constant floor_compute::address_space_adaptor<const param_wrapper, constant const param_wrapper*, true, false>&;
//! array<> for use with static constant memory
template <class data_type, size_t array_size>
class const_array {
public:
	const floor_compute::address_space_adaptor<data_type, constant const data_type*, true, false> elems[array_size > 0 ? array_size : 1];
	
	constexpr data_type operator[](const size_t& index) const {
		return const_select::is_constexpr(index) ? cexpr_get(index) : rt_get<data_type>(index);
	}
	
	constexpr size_t size() const { return array_size; }
	constexpr constant const data_type* data() const { return (constant const data_type*)&elems[0]; }
	
protected:
	constexpr data_type cexpr_get(const size_t& index) const {
		return elems[index].elem;
	}
	
	template <typename LT, enable_if_t<!floor_compute::has_load_function<LT>::value>* = nullptr>
	constexpr data_type rt_get(const size_t& index) const {
		return floor_compute::address_space_loader<data_type>::load((constant const data_type*)&elems[0] + index);
	}
	
	template <typename LT, enable_if_t<floor_compute::has_load_function<LT>::value>* = nullptr>
	constexpr data_type rt_get(const size_t& index) const {
		return data_type::load((constant const data_type*)&elems[0] + index);
	}

};
#endif

#endif

#endif
