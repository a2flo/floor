/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2020 Florian Ziesche
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

#ifndef __FLOOR_ALIGNED_PTR_HPP__
#define __FLOOR_ALIGNED_PTR_HPP__

#include <floor/core/essentials.hpp>
#include <memory>
#include <type_traits>
#include <cstdlib>
#include <cstdio>
#include <stdexcept>
#if defined(__WINDOWS__)
#include <malloc.h>
#endif

//! unique_ptr-like smart pointer with allocations being aligned to 4096 bytes
template <typename T>
class aligned_ptr {
public:
	using pointer = T*;
	using element_type = T;
	
	constexpr aligned_ptr() noexcept = default;
	explicit aligned_ptr(nullptr_t) noexcept : ptr(nullptr) {}
	explicit aligned_ptr(pointer ptr_) noexcept : ptr(ptr_) {}
	explicit aligned_ptr(aligned_ptr&& aligned_ptr_) noexcept : ptr(aligned_ptr_.release()) {}
	~aligned_ptr() { reset(); }
	
	aligned_ptr& operator=(aligned_ptr&& aligned_ptr_) noexcept {
		reset(aligned_ptr_.release());
		return *this;
	}
	
	aligned_ptr& operator=(nullptr_t) noexcept {
		reset();
		return *this;
	}

	pointer release() noexcept {
		pointer ret = ptr;
		ptr = pointer {};
		return ret;
	}
	
	void reset(pointer ptr_ = pointer {}) noexcept {
		if (ptr != nullptr) {
#if !defined(__WINDOWS__)
			free(ptr);
#else
			_aligned_free(ptr);
#endif
		}
		ptr = ptr_;
	}
	
	void swap(aligned_ptr& rhs) noexcept {
		swap(ptr, rhs.ptr);
	}
	
	pointer get() noexcept {
		return ptr;
	}
	
	add_lvalue_reference_t<T> operator*() const {
		return *ptr;
	}
	
	pointer operator->() const noexcept {
		return ptr;
	}
	
protected:
	pointer ptr { nullptr };
	
};

//! creates an aligned_ptr of the specified type, with the specified element count
template <typename T>
aligned_ptr<T> make_aligned_ptr(const size_t count = 1u) {
	const auto size = count * sizeof(T);
	T* ptr = nullptr;
#if !defined(__WINDOWS__)
	if (posix_memalign((void**)&ptr, 4096u, size) != 0 || ptr == nullptr) {
		throw std::runtime_error("failed to allocated aligned_ptr");
	}
#else
	ptr = _aligned_malloc(size, 4096u);
	if (ptr == nullptr) {
		throw std::runtime_error("failed to allocated aligned_ptr");
	}
#endif
	return aligned_ptr<T> { ptr };
}

#endif
