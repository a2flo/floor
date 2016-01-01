/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2016 Florian Ziesche
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

#ifndef __FLOOR_CONST_ARRAY_HPP__
#define __FLOOR_CONST_ARRAY_HPP__

#include <floor/core/essentials.hpp>
#include <iterator>

//! array<> for use with constexpr, this is necessary because array doesn't provide a constexpr modifiable "operator[]"
//! NOTE: array size of 0 is not allowed!
template <class data_type, size_t array_size>
struct const_array {
	static_assert(array_size > 0, "array size may not be 0!");
	data_type elems[array_size];
	
	constexpr size_t size() const { return array_size; }
	constexpr size_t max_size() const { return array_size; }
	constexpr bool empty() const { return false; }
	
#if !defined(_MSC_VER) // duplicate name mangling issues
	constexpr data_type& operator[](const size_t& index) __attribute__((enable_if(index < array_size, "index is const"))) {
		return elems[index];
	}
	constexpr data_type& operator[](const size_t& index) __attribute__((enable_if(index >= array_size, "index is invalid"), unavailable("index is invalid")));
#endif
	constexpr data_type& operator[](const size_t& index) /* run-time index */ {
		return elems[index];
	}
	
#if !defined(_MSC_VER) // duplicate name mangling issues
	constexpr const data_type& operator[](const size_t& index) const __attribute__((enable_if(index < array_size, "index is const"))) {
		return elems[index];
	}
	constexpr const data_type& operator[](const size_t& index) const __attribute__((enable_if(index >= array_size, "index is invalid"), unavailable("index is invalid")));
#endif
	constexpr const data_type& operator[](const size_t& index) const /* run-time index */ {
		return elems[index];
	}
	
#if !defined(_MSC_VER) // duplicate name mangling issues
	constexpr data_type& at(const size_t& index) __attribute__((enable_if(index < array_size, "index is const"))) {
		return elems[index];
	}
	constexpr data_type& at(const size_t& index) __attribute__((enable_if(index >= array_size, "index is invalid"), unavailable("index is invalid")));
#endif
	constexpr data_type& at(const size_t& index) /* run-time index */ {
		return elems[index];
	}
	
#if !defined(_MSC_VER) // duplicate name mangling issues
	constexpr const data_type& at(const size_t& index) const __attribute__((enable_if(index < array_size, "index is const"))) {
		return elems[index];
	}
	constexpr const data_type& at(const size_t& index) const __attribute__((enable_if(index >= array_size, "index is invalid"), unavailable("index is invalid")));
#endif
	constexpr const data_type& at(const size_t& index) const /* run-time index */ {
		return elems[index];
	}
	
	constexpr data_type& operator*() { return &elems[0]; }
	constexpr const data_type& operator*() const { return &elems[0]; }
	
	constexpr data_type* data() { return elems; }
	constexpr const data_type* data() const { return elems; }
	
	constexpr data_type& front() { return elems[0]; }
	constexpr const data_type& front() const { return elems[0]; }
	
	constexpr data_type& back() { return elems[array_size - 1]; }
	constexpr const data_type& back() const { return elems[array_size - 1]; }
	
	constexpr data_type* begin() { return elems; }
	constexpr const data_type* begin() const { return elems; }
	
	constexpr data_type* end() { return elems + array_size; }
	constexpr const data_type* end() const { return elems + array_size; }
	
	constexpr const data_type* cbegin() const { return begin(); }
	constexpr const data_type* cend() const { return end(); }
	
	constexpr auto rbegin() { return reverse_iterator<data_type*>(end()); }
	constexpr auto rbegin() const { return reverse_iterator<const data_type*>(end()); }
	
	constexpr auto rend() { return reverse_iterator<data_type*>(begin()); }
	constexpr auto rend() const { return reverse_iterator<const data_type*>(begin()); }
	
	constexpr auto crbegin() const { return rbegin(); }
	constexpr auto crend() const { return rend(); }
	
};

#endif
