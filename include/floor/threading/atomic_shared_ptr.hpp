/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2026 Florian Ziesche
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

#pragma once

#include <floor/threading/atomic_spin_lock.hpp>
#include <floor/threading/thread_safety.hpp>
#include <floor/core/essentials.hpp>

namespace fl {

// interface partially based on std shared_ptr and N4162/N4260 proposals, with additional functionality:
//  * this is a completely thread-safe shared_ptr, not just a wrapper around atomic_* functions like N4162/N4260
//  * locked/thread-safe function calls / access via get/*/->
// ref shared_ptr: https://github.com/cplusplus/draft/blob/master/source/utilities.tex#L5568
// ref thread-safety of shared_ptr: http://www.boost.org/doc/libs/1_53_0/libs/smart_ptr/shared_ptr.htm#ThreadSafety
// ref atomic_shared_ptr: http://isocpp.org/files/papers/N4162.pdf
//                        http://isocpp.org/files/papers/N4260.pdf
template <class T> class alignas(64u) atomic_shared_ptr {
protected:
	mutable atomic_spin_lock_unaligned mtx;
	mutable std::shared_ptr<T> ptr GUARDED_BY(mtx);
	
public:
	// aka locking proxy
	template <typename U>
	class locked_ptr {
	protected:
		atomic_spin_lock_unaligned& mtx_ref;
		std::shared_ptr<U>& ptr;
		
	public:
		explicit locked_ptr(atomic_spin_lock_unaligned& mtx_, std::shared_ptr<U>& ptr_) noexcept : mtx_ref(mtx_), ptr(ptr_) {
			mtx_ref.lock();
		}
		~locked_ptr() {
			mtx_ref.unlock();
		}
		locked_ptr& operator=(const locked_ptr&) = delete;
		floor_inline_always U* get() const noexcept {
			return ptr.get();
		}
		floor_inline_always U& operator*() const noexcept {
			return *ptr;
		}
		floor_inline_always U* operator->() const noexcept {
			return ptr.get();
		}
		floor_inline_always explicit operator bool() const noexcept {
			return (ptr.get() != nullptr);
		}
	};
	
	// constructors:
	constexpr atomic_shared_ptr() noexcept = default;
	constexpr atomic_shared_ptr(nullptr_t) noexcept {}
	constexpr atomic_shared_ptr(std::shared_ptr<T> sptr) noexcept : ptr(sptr) {}
	atomic_shared_ptr(const atomic_shared_ptr&) = delete;
	
	// destructor:
	~atomic_shared_ptr() {
		mtx.lock();
		// note: shared_ptr is cleaned up / destructed automatically
		ptr.reset();
		mtx.unlock();
	}
	
	// assignment:
	floor_inline_always atomic_shared_ptr& operator=(std::shared_ptr<T> sptr) REQUIRES(!mtx) {
		safe_guard<atomic_spin_lock_unaligned> guard(mtx);
		ptr.swap(sptr);
		return *this;
	}
	floor_inline_always atomic_shared_ptr& operator=(nullptr_t) REQUIRES(!mtx) {
		reset();
		return *this;
	}
	atomic_shared_ptr& operator=(const atomic_shared_ptr&) = delete;
	
	floor_inline_always void store(std::shared_ptr<T> sptr) noexcept REQUIRES(!mtx) {
		safe_guard<atomic_spin_lock_unaligned> guard(mtx);
		ptr.swap(sptr);
	}
	
	floor_inline_always std::shared_ptr<T> exchange(std::shared_ptr<T> sptr) noexcept REQUIRES(!mtx) {
		safe_guard<atomic_spin_lock_unaligned> guard(mtx);
		ptr.swap(sptr);
		return sptr;
	}
	
	floor_inline_always bool compare_exchange_strong(std::shared_ptr<T>& expected, const std::shared_ptr<T>& desired) noexcept REQUIRES(!mtx) {
		safe_guard<atomic_spin_lock_unaligned> guard(mtx);
		// check if ptr and expected share the same control block (are the same)
		if(!ptr.owner_before(expected) && !expected.owner_before(ptr)) {
			ptr = desired;
			return true;
		}
		expected = ptr;
		return false;
	}
	floor_inline_always bool compare_exchange_strong(std::shared_ptr<T>& expected, std::shared_ptr<T>&& desired) noexcept REQUIRES(!mtx) {
		safe_guard<atomic_spin_lock_unaligned> guard(mtx);
		// check if ptr and expected share the same control block (are the same)
		if(!ptr.owner_before(expected) && !expected.owner_before(ptr)) {
			ptr = desired;
			return true;
		}
		expected = ptr;
		return false;
	}
	floor_inline_always bool compare_exchange_weak(std::shared_ptr<T>& expected, const std::shared_ptr<T>& desired) noexcept REQUIRES(!mtx) {
		return compare_exchange_strong(expected, desired);
	}
	floor_inline_always bool compare_exchange_weak(std::shared_ptr<T>& expected, std::shared_ptr<T>&& desired) noexcept REQUIRES(!mtx) {
		return compare_exchange_strong(expected, std::forward(desired));
	}
	
	// modifiers:
	floor_inline_always void reset() noexcept REQUIRES(!mtx) {
		safe_guard<atomic_spin_lock_unaligned> guard(mtx);
		ptr.reset();
	}
	template<class Y> floor_inline_always void reset(Y* p) REQUIRES(!mtx) {
		safe_guard<atomic_spin_lock_unaligned> guard(mtx);
		ptr.reset(p);
	}
	template<class Y, class D> floor_inline_always void reset(Y* p, D d) REQUIRES(!mtx) {
		safe_guard<atomic_spin_lock_unaligned> guard(mtx);
		ptr.reset(p, d);
	}
	template<class Y, class D, class A> floor_inline_always void reset(Y* p, D d, A a) REQUIRES(!mtx) {
		safe_guard<atomic_spin_lock_unaligned> guard(mtx);
		ptr.reset(p, d, a);
	}
	
	// observers:
	floor_inline_always locked_ptr<T> get() const noexcept NO_THREAD_SAFETY_ANALYSIS {
		return locked_ptr<T> { mtx, ptr };
	}
	floor_inline_always T* unsafe_get() const noexcept {
		return ptr.get();
	}
	floor_inline_always locked_ptr<T> operator*() const noexcept NO_THREAD_SAFETY_ANALYSIS {
		return locked_ptr<T> { mtx, ptr };
	}
	floor_inline_always locked_ptr<T> operator->() const noexcept NO_THREAD_SAFETY_ANALYSIS {
		return locked_ptr<T> { mtx, ptr };
	}
	floor_inline_always long use_count() const noexcept REQUIRES(!mtx) {
		safe_guard<atomic_spin_lock_unaligned> guard(mtx);
		return ptr.use_count();
	}
	floor_inline_always bool unique() const noexcept REQUIRES(!mtx) {
		safe_guard<atomic_spin_lock_unaligned> guard(mtx);
		return ptr.unique();
	}
	floor_inline_always explicit operator bool() const noexcept REQUIRES(!mtx) {
		safe_guard<atomic_spin_lock_unaligned> guard(mtx);
		return ptr();
	}
	template<class U> floor_inline_always bool owner_before(std::shared_ptr<U> const& b) const REQUIRES(!mtx) {
		safe_guard<atomic_spin_lock_unaligned> guard(mtx);
		return ptr.owner_before(b);
	}
	
	constexpr bool is_lock_free() const noexcept { return false; }
	
	floor_inline_always std::shared_ptr<T> load() const noexcept REQUIRES(!mtx) {
		safe_guard<atomic_spin_lock_unaligned> guard(mtx);
		std::shared_ptr<T> ret = ptr;
		return ret;
	}
	
	floor_inline_always operator std::shared_ptr<T>() const noexcept {
		return load();
	}
	
};

static_assert(sizeof(atomic_shared_ptr<int>) == 64u);

} // namespace fl
