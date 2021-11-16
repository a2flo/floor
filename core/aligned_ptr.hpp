/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2021 Florian Ziesche
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
#include <tuple>
#if defined(__WINDOWS__)
#include <malloc.h>
#else
#include <sys/mman.h>
#endif

//! unique_ptr-like smart pointer with allocations being aligned to 4096 bytes (x86-64) or 16384 (aarch64) (== page size),
//! with the additional ability to page-lock/pin the memory and set page protection
template <typename T>
class aligned_ptr {
public:
	using pointer = T*;
	using element_type = T;
	
	//! the assumed page size is 4KiB (x86-64) or 16KiB (aarch64)
	//! NOTE: if this is not the case, floor init will already fail
	static constexpr const uint64_t page_size {
#if defined(__x86_64__)
		4096u
#elif defined(__aarch64__)
		16384u
#else
#error "unhandled arch"
#endif
	};
	
	constexpr aligned_ptr() noexcept = default;
	explicit aligned_ptr(std::nullptr_t) noexcept : ptr(nullptr) {}
	explicit aligned_ptr(pointer ptr_, const size_t size_) noexcept : ptr(ptr_), size(size_) {}
	aligned_ptr(aligned_ptr&& aligned_ptr_) noexcept {
		reset(aligned_ptr_.release());
	}
	~aligned_ptr() { reset(); }
	
	aligned_ptr& operator=(aligned_ptr&& aligned_ptr_) noexcept {
		reset(aligned_ptr_.release());
		return *this;
	}
	
	aligned_ptr& operator=(std::nullptr_t) noexcept {
		reset();
		return *this;
	}
	
	explicit operator bool() const noexcept {
		return (ptr != nullptr);
	}
	
	bool operator==(const aligned_ptr<T>& ptr_) const {
		return (ptr == ptr_.ptr);
	}
	bool operator==(const pointer ptr_) const {
		return (ptr == ptr_);
	}
	bool operator!=(const aligned_ptr<T>& ptr_) const {
		return (ptr != ptr_.ptr);
	}
	bool operator!=(const pointer ptr_) const {
		return (ptr != ptr_);
	}
	bool operator<(const aligned_ptr<T>& ptr_) const {
		return (ptr < ptr_.ptr);
	}
	bool operator<(const pointer ptr_) const {
		return (ptr < ptr_);
	}

	std::tuple<pointer, size_t, bool> release() noexcept {
		pointer ret = ptr;
		size_t ret_size = size;
		bool ret_pinned = pinned;
		ptr = pointer {};
		size = 0u;
		pinned = false;
		return { ret, ret_size, ret_pinned };
	}
	
	//! NOTE: reset also clears all page-locks and protection
	void reset(std::tuple<pointer, size_t, bool> ptr_size_pinned_info = { pointer {}, 0u, false }) noexcept {
		if (ptr != nullptr) {
			// must unpin before freeing
			if (pinned) {
				(void)unpin();
			}
#if !defined(__WINDOWS__)
			// must be writable and readable before freeing
			set_protection(PAGE_PROTECTION::READ_WRITE);
			free(ptr);
#else
			_aligned_free(ptr);
#endif
			size = 0;
			pinned = false;
		}
		std::tie(ptr, size, pinned) = ptr_size_pinned_info;
	}
	
	void swap(aligned_ptr& rhs) noexcept {
		std::swap(ptr, rhs.ptr);
		std::swap(size, rhs.size);
		std::swap(pinned, rhs.pinned);
	}
	
	pointer __attribute__((aligned(page_size))) get() noexcept {
		return ptr;
	}
	
	const pointer __attribute__((aligned(page_size))) get() const noexcept {
		return ptr;
	}

	std::add_lvalue_reference_t<T> operator*() const {
		return *ptr;
	}
	
	pointer __attribute__((aligned(page_size))) operator->() const noexcept {
		return ptr;
	}
	
	//! returns the size of this allocation
	size_t allocation_size() const noexcept {
		return size;
	}
	
	//! page-locks/pins the memory
	bool pin() {
#if !defined(__WINDOWS__)
		if (mlock(ptr, size) == 0) {
			pinned = true;
			return true;
		}
#else
		// TODO: Windows implementation
		return false;
#endif
		return false;
	}
	
	//! unlocks/unpins the memory again
	bool unpin() {
#if !defined(__WINDOWS__)
		if (munlock(ptr, size) == 0) {
			pinned = false;
			return true;
		}
#else
		// TODO: Windows implementation
#endif
		return false;
	}
	
	//! the allowed memory protection types
	enum class PAGE_PROTECTION {
		READ_ONLY,
		READ_WRITE,
		READ_EXEC,
	};
	
	//! sets the memory protection of all contained pages,
	//! returns true on success
	bool set_protection(const PAGE_PROTECTION& protection) {
#if !defined(__WINDOWS__)
		int prot_flags = 0;
		switch (protection) {
			case PAGE_PROTECTION::READ_ONLY:
				prot_flags = PROT_READ;
				break;
			case PAGE_PROTECTION::READ_WRITE:
				prot_flags = PROT_READ | PROT_WRITE;
				break;
			case PAGE_PROTECTION::READ_EXEC:
				prot_flags = PROT_READ | PROT_EXEC;
				break;
		}
		if (mprotect(ptr, size, prot_flags) != 0) {
			return false;
		}
		return true;
#else
		// TODO: Windows implementation
		(void)protection;
		return false;
#endif
	}
	
protected:
	pointer ptr { nullptr };
	size_t size { 0u };
	bool pinned { false };
	
};

//! creates an aligned_ptr of the specified type, with the specified element count
template <typename T>
aligned_ptr<T> make_aligned_ptr(const size_t count = 1u) {
	auto size = count * sizeof(T);
	if (size % aligned_ptr<T>::page_size != 0u) {
		size += aligned_ptr<T>::page_size - (size % aligned_ptr<T>::page_size);
	}
	T* ptr = nullptr;
#if !defined(__WINDOWS__)
	if (posix_memalign((void**)&ptr, aligned_ptr<T>::page_size, size) != 0 || ptr == nullptr) {
		throw std::runtime_error("failed to allocated aligned_ptr");
	}
#else
	ptr = (T*)_aligned_malloc(size, aligned_ptr<T>::page_size);
	if (ptr == nullptr) {
		throw std::runtime_error("failed to allocated aligned_ptr");
	}
#endif
	return aligned_ptr<T> { ptr, size };
}

#endif
