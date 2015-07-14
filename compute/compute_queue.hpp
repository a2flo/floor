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

#ifndef __FLOOR_COMPUTE_QUEUE_HPP__
#define __FLOOR_COMPUTE_QUEUE_HPP__

#include <string>
#include <vector>
#include <floor/math/vector_lib.hpp>

#include <floor/compute/compute_kernel.hpp>

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wweak-vtables"
#endif

class compute_queue {
protected:
	//! argument type validity specializations, with pretty error messages
	template <typename T, typename = void> struct is_valid_arg { static constexpr bool valid() { return true; } };
	template <typename T> struct is_valid_arg<T, enable_if_t<is_same<T, size_t>::value>> {
		static constexpr bool valid()
		__attribute__((unavailable("size_t is not allowed due to a possible host/device size mismatch"))) { return false; }
	};
	template <typename T> struct is_valid_arg<T, enable_if_t<(is_floor_vector<T>::value &&
															  is_same<typename T::decayed_scalar_type, size_t>::value)>> {
		static constexpr bool valid()
		__attribute__((unavailable("size_t vector types are not allowed due to a possible host/device size mismatch"))) { return false; }
	};
	template <typename T> struct is_valid_arg<T, enable_if_t<is_pointer<T>::value>> {
		static constexpr bool valid()
		__attribute__((unavailable("raw pointers are not allowed"))) { return false; }
	};
	template <typename T> struct is_valid_arg<T, enable_if_t<is_null_pointer<T>::value>> {
		static constexpr bool valid()
		__attribute__((unavailable("nullptr is not allowed"))) { return false; }
	};
	
	//! checks if an individual argument type is valid
	template <typename T = void>
	static constexpr bool check_arg_types() {
		constexpr const bool is_valid { is_valid_arg<T>::valid() };
		static_assert(is_valid, "invalid argument type");
		return is_valid;
	}
	
	//! checks if all argument types are valid
	template <typename T, typename... Args, enable_if_t<sizeof...(Args) != 0, int> = 0>
	static constexpr bool check_arg_types() {
		return (check_arg_types<T>() && check_arg_types<Args...>());
	}
	
public:
	virtual ~compute_queue() = 0;
	
	//! blocks until all currently scheduled work in this queue has been executed
	virtual void finish() const = 0;
	
	//! flushes all scheduled work to the associated device
	virtual void flush() const = 0;
	
	//! implementation specific queue object ptr (cl_command_queue or CUStream, both "struct _ *")
	virtual const void* get_queue_ptr() const = 0;
	
	//! enqueues (and executes) the specified kernel into this queue
	template <typename... Args, class work_size_type_global, class work_size_type_local,
			  enable_if_t<((is_same<decay_t<work_size_type_global>, uint1>::value ||
							is_same<decay_t<work_size_type_global>, uint2>::value ||
							is_same<decay_t<work_size_type_global>, uint3>::value) &&
						   is_same<decay_t<work_size_type_global>, decay_t<work_size_type_local>>::value), int> = 0>
	void execute(shared_ptr<compute_kernel> kernel,
				 work_size_type_global&& global_work_size,
				 work_size_type_local&& local_work_size,
				 Args&&... args) __attribute__((enable_if(check_arg_types<Args...>(), "valid args"))) {
		kernel->execute(this, global_work_size, local_work_size, forward<Args>(args)...);
	}
	
	template <typename... Args, class work_size_type_global, class work_size_type_local,
			  enable_if_t<((is_same<decay_t<work_size_type_global>, uint1>::value ||
							is_same<decay_t<work_size_type_global>, uint2>::value ||
							is_same<decay_t<work_size_type_global>, uint3>::value) &&
						   is_same<decay_t<work_size_type_global>, decay_t<work_size_type_local>>::value), int> = 0>
	void execute(shared_ptr<compute_kernel>, work_size_type_global&&, work_size_type_local&&, Args&&...)
	__attribute__((enable_if(!check_arg_types<Args...>(), "invalid args"), unavailable("invalid kernel argument(s)!")));
	
protected:
	
};

#if defined(__clang__)
#pragma clang diagnostic pop
#endif

#endif
