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

#if defined(FLOOR_COMPUTE_CUDA) || defined(FLOOR_COMPUTE_OPENCL) || defined(FLOOR_COMPUTE_METAL)

// compute implementation specific headers (pre-std headers)
#if defined(FLOOR_COMPUTE_CUDA)
#include <floor/compute/device/cuda_pre.hpp>
#elif defined(FLOOR_COMPUTE_OPENCL)
#include <floor/compute/device/opencl_pre.hpp>
#elif defined(FLOOR_COMPUTE_METAL)
#include <floor/compute/device/metal_pre.hpp>
#endif

// libc++ stl functionality without (most of) the baggage
#include <utility>
#include <type_traits>
#include <initializer_list>

_LIBCPP_BEGIN_NAMESPACE_STD

// <array> replacement
template <class data_type, size_t array_size>
struct _LIBCPP_TYPE_VIS_ONLY __attribute__((packed, aligned(sizeof(data_type) > 4 ? sizeof(data_type) : 4))) array {
	static_assert(array_size > 0, "array size may not be 0!");
	alignas(sizeof(data_type) > 4 ? sizeof(data_type) : 4) data_type elems[array_size];
	
	constexpr size_t size() const { return array_size; }
	
	constexpr data_type& operator[](const size_t& index) { return elems[index]; }
	constexpr const data_type& operator[](const size_t& index) const { return elems[index]; }
	
	constexpr data_type& operator*() { return &elems[0]; }
	constexpr const data_type& operator*() const { return &elems[0]; }
	
	constexpr data_type* data() { return elems; }
	constexpr const data_type* data() const { return elems; }
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

// non-member size (N4280, also provided by libc++ 3.6+ in c++1z mode)
#if ((__cplusplus <= 201402L) || (__clang_major__ == 3 && __clang_minor__ <= 5)) && \
	!defined(FLOOR_COMPUTE_NO_NON_MEMBER_SIZE)
template <class C> constexpr auto size(const C& c) noexcept -> decltype(c.size()) {
	return c.size();
}
template <class T, size_t N> constexpr size_t size(const T (&)[N]) noexcept {
	return N;
}
#endif

_LIBCPP_END_NAMESPACE_STD
using namespace std;

// compute implementation specific headers
#if defined(FLOOR_COMPUTE_CUDA)
#include <floor/compute/device/cuda.hpp>
#elif defined(FLOOR_COMPUTE_OPENCL)
#include <floor/compute/device/opencl.hpp>
#elif defined(FLOOR_COMPUTE_METAL)
#include <floor/compute/device/metal.hpp>
#endif

// always include const_math (and const_select) functionality
#include <floor/constexpr/const_math.hpp>

// always include vector lib/types
#include <floor/math/vector_lib.hpp>

// image support headers + common attributes
#define read_only __attribute__((image_read_only))
#define write_only __attribute__((image_write_only))
#define read_write __attribute__((image_read_write))
#include <floor/core/enum_helpers.hpp>
#include <floor/compute/device/image_types.hpp>

// buffer / local_buffer / const_buffer / param target-specific specialization/implementation
namespace floor_compute {
	template <typename T> struct indirect_type_wrapper {
		T elem;
		
		indirect_type_wrapper() {}
		indirect_type_wrapper(const T& obj) : elem(obj) {}
		indirect_type_wrapper& operator=(const T& obj) {
			elem = obj;
			return *this;
		}
		const T& operator*() const { return elem; }
		T& operator*() { return elem; }
		const T* const operator->() const { return &elem; }
		T* operator->() { return &elem; }
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

#if defined(FLOOR_COMPUTE_CUDA)
//! global memory buffer
template <typename T> using buffer = floor_compute::indirect_type_wrapper<T>*;
//! local memory buffer
// NOTE: need to workaround the issue that "local" is not part of the type in cuda
#define local_buffer local cuda_local_buffer
template <typename T, size_t count_x> using cuda_local_buffer_1d = T[count_x];
template <typename T, size_t count_y, size_t count_x> using cuda_local_buffer_2d = T[count_y][count_x];
template <typename T, size_t count_1, size_t count_2 = 0> using cuda_local_buffer =
	conditional_t<count_2 == 0, cuda_local_buffer_1d<T, count_1>, cuda_local_buffer_2d<T, count_1, count_2>>;
//! constant memory buffer
// NOTE: again: need to workaround the issue that "constant" is not part of the type in cuda
#define const_buffer constant const_buffer_cuda
template <typename T> using const_buffer_cuda = const T* const;
//! generic parameter object/buffer
template <typename T,
		  typename param_wrapper = const conditional_t<
			  is_fundamental<T>::value,
			  floor_compute::indirect_type_wrapper<T>,
			  floor_compute::direct_type_wrapper<T>>>
using param = const param_wrapper;
//! array for use with static constant memory
#define constant_array constant constant_array_cuda
template <class data_type, size_t array_size> using constant_array_cuda = data_type[array_size];

#elif defined(FLOOR_COMPUTE_METAL) || defined(FLOOR_COMPUTE_OPENCL)
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
#if 0 // not needed for now, and not really functional
	template <typename T> struct address_space_store {
		static void store(global T* to, const T& data) {
			for(size_t i = 0; i < (sizeof(T)); ++i) {
				*(((global uint8_t*)to) + i) = *(((uint8_t*)&data) + i);
			}
		}
		static void store(local T* to, const T& data) {
			for(size_t i = 0; i < (sizeof(T)); ++i) {
				*(((global uint8_t*)to) + i) = *(((uint8_t*)&data) + i);
			}
		}
	};
#endif
	
	//! adaptor to access memory in a global/local/constant address space, with support for explicit and implicit stores and loads.
	//! NOTE: this is necessary for any class that uses the "this" pointer to access memory in a global/local/constant address space.
	template <typename contained_type, typename as_ptr_type, bool can_read = true, bool can_write = true>
	struct address_space_adaptor {
		contained_type elem;
		
		constexpr address_space_adaptor() {}
		constexpr address_space_adaptor(const contained_type& elem_) : elem(elem_) {}
		
		//! explicit load
		template <bool can_read_ = can_read, enable_if_t<can_read_>* = nullptr>
		contained_type load() {
			return contained_type::load(((const as_ptr_type)this));
		}
		template <bool can_read_ = can_read, enable_if_t<!can_read_>* = nullptr>
		contained_type load() {
			static_assert(can_write, "load not supported in this address space!");
			return contained_type {};
		}
		
		//! implicit load (convert to contained_type in private/default address space)
		template <bool can_read_ = can_read, enable_if_t<can_read_>* = nullptr>
		operator contained_type() {
			return load();
		}
		template <bool can_read_ = can_read, enable_if_t<!can_read_>* = nullptr>
		operator contained_type() {
			static_assert(can_write, "load not supported in this address space!");
			return contained_type {};
		}
		
		//! explicit store
		template <bool can_write_ = can_write, enable_if_t<can_write_>* = nullptr>
		void store(const contained_type& vec) {
			contained_type::store(((as_ptr_type)this), vec);
		}
		template <bool can_write_ = can_write, enable_if_t<!can_write_>* = nullptr>
		void store(const contained_type&) {
			static_assert(can_write, "store not supported in this address space!");
		}
		
		//! implicit store (assign)
		template <typename T, bool can_write_ = can_write, enable_if_t<can_write_>* = nullptr>
		address_space_adaptor& operator=(const T& obj) {
			store(obj);
			return *this;
		}
		template <typename T, bool can_write_ = can_write, enable_if_t<!can_write_>* = nullptr>
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
		template <bool can_write_ = can_write, enable_if_t<can_write_>* = nullptr>
		proxy<contained_type> operator->() {
			return { load() };
		}
		//! implicit load via "pointer access" (returns a const/read-only proxy object)
		template <bool can_write_ = can_write, enable_if_t<!can_write_>* = nullptr>
		const_proxy<contained_type> operator->() {
			return { load() };
		}
		
		//! explicit load via deref (provided for completeness sake, implicit loads should usually suffice,
		//! with other explicit loads being the preferred way of clarifying "this is an explicit load")
		contained_type operator*() {
			return { load() };
		}
		
	};
	
	//! template voodoo, false if T has no load function for global/local/constant memory
	template <typename T, typename = void> struct has_load_function : false_type {};
	//! template voodoo, true if T has a load function for global/local/constant memory
	template <typename T> struct has_load_function<T, decltype(T::load((global const T*)nullptr),
															   T::load((local const T*)nullptr),
															   T::load((constant const T*)nullptr),
															   void())> : true_type {};
	
	//! for internal use (wraps a struct or class and provides an automatic load function)
	template <typename T> struct buffer_container : T, address_space_loader<buffer_container<T>> {};
	
	//! for internal use (wraps a fundamental / non-struct or -class type and provides an automatic load function)
	template <typename T>
	struct generic_type_adaptor : address_space_loader<generic_type_adaptor<T>> {
		T elem;
		constexpr operator T() const noexcept { return elem; }
	};
}

//! depending on if a type has address space load functions either aliases the type directly
//! or wraps the type in a class that provides load functionality for all address spaces
//! NOTE: store functions are currently implied if a type has load functions!
template <typename T> using type_load_wrapper = conditional_t<floor_compute::has_load_function<T>::value,
															  T,
															  floor_compute::buffer_container<T>>;

#define FLOOR_OLD_AS_HANDLING 1
//! global memory buffer
#if defined(FLOOR_OLD_AS_HANDLING)
template <typename T, typename wrapper = type_load_wrapper<T>>
using buffer = global floor_compute::address_space_adaptor<wrapper, global wrapper*, true, !is_const<T>()>*;
#else
template <typename T>
using buffer = global T*;
#endif

//! local memory buffer
#if defined(FLOOR_OLD_AS_HANDLING)
template <typename T, size_t count, typename wrapper = type_load_wrapper<T>>
using local_buffer = local floor_compute::address_space_adaptor<wrapper, local wrapper*, true, !is_const<T>()>[count];
#else
template <typename T, size_t count> using local_buffer = local T[count];
#endif

//! constant memory buffer
template <typename T, typename wrapper = type_load_wrapper<T>>
using const_buffer = constant floor_compute::address_space_adaptor<const wrapper, constant const wrapper*, true, false>*;

#if defined(FLOOR_COMPUTE_OPENCL)
//! generic parameter object/buffer (provided via launch parameter)
template <typename T,
		  typename param_wrapper = const conditional_t<
			  is_fundamental<T>::value,
			  floor_compute::indirect_type_wrapper<T>,
			  floor_compute::direct_type_wrapper<T>>>
using param = const param_wrapper;
#elif defined(FLOOR_COMPUTE_METAL)
//! generic parameter object/buffer (stored in constant memory)
template <typename T,
		  typename param_wrapper = const conditional_t<
			  is_fundamental<T>::value,
			  floor_compute::generic_type_adaptor<T>,
			  conditional_t<
				  floor_compute::has_load_function<T>::value,
				  T,
				  floor_compute::buffer_container<T>>>>
using param = constant floor_compute::address_space_adaptor<const param_wrapper, constant const param_wrapper*, true, false>&;
#endif

//! array<> for use with static constant memory
template <class data_type, size_t array_size>
class __attribute__((packed, aligned(4))) constant_array {
public:
	static_assert(array_size > 0, "array size may not be 0!");
	alignas(4) const floor_compute::address_space_adaptor<data_type, constant const data_type*, true, false> elems[array_size];
	
	constexpr constant_array() noexcept = default;
	
	template <typename... Args>
	constexpr constant_array(const Args&... args) noexcept : elems { args... } {}
	
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

// implementation specific image headers
#if defined(FLOOR_COMPUTE_CUDA)
#include <floor/compute/device/cuda_image.hpp>
#elif defined(FLOOR_COMPUTE_OPENCL)
#include <floor/compute/device/opencl_image.hpp>
#elif defined(FLOOR_COMPUTE_METAL)
#include <floor/compute/device/metal_image.hpp>
#endif

// yeah, it's kinda ugly to include a .cpp file, but this is never (and should never be) included by user code,
// this is all correctly handled in llvm_compute which includes this header as a prefix header.
// the .cpp is needed, because it provides the implementation and redirects of the functions defined in const_math.hpp
// (would otherwise need to compile and link this separately which is obviously overkill and unnecessary)
#include <floor/constexpr/const_math.cpp>

// device logging functions
#include <floor/compute/device/logger.hpp>

#endif

#endif
