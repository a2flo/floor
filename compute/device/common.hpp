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

#if defined(FLOOR_COMPUTE_CUDA) || defined(FLOOR_COMPUTE_SPIR) || defined(FLOOR_COMPUTE_METAL)

// compute implementation specific headers (pre-std headers)
#if defined(FLOOR_COMPUTE_CUDA)
#include <floor/compute/device/cuda_pre.hpp>
#elif defined(FLOOR_COMPUTE_SPIR)
#include <floor/compute/device/spir_pre.hpp>
#elif defined(FLOOR_COMPUTE_METAL)
#include <floor/compute/device/metal_pre.hpp>
#endif

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

// compute implementation specific headers
#if defined(FLOOR_COMPUTE_CUDA)
#include <floor/compute/device/cuda.hpp>
#elif defined(FLOOR_COMPUTE_SPIR)
#include <floor/compute/device/spir.hpp>
#elif defined(FLOOR_COMPUTE_METAL)
#include <floor/compute/device/metal.hpp>
#endif

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

#if defined(FLOOR_COMPUTE_SPIR)
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

#elif defined(FLOOR_COMPUTE_CUDA)
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

#elif defined(FLOOR_COMPUTE_METAL)
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
