/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2022 Florian Ziesche
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

#ifndef __FLOOR_ATOMIC_SHARED_PTR_HPP__
#define __FLOOR_ATOMIC_SHARED_PTR_HPP__

#include <floor/threading/atomic_spin_lock.hpp>
#include <floor/threading/thread_safety.hpp>
#include <floor/core/essentials.hpp>

// interface partially based on std shared_ptr and N4162/N4260 proposals, with additional functionality:
//  * this is a completely thread-safe shared_ptr, not just a wrapper around atomic_* functions like N4162/N4260
//  * locked/thread-safe function calls / access via get/*/->
// ref shared_ptr: https://github.com/cplusplus/draft/blob/master/source/utilities.tex#L5568
// ref thread-safety of shared_ptr: http://www.boost.org/doc/libs/1_53_0/libs/smart_ptr/shared_ptr.htm#ThreadSafety
// ref atomic_shared_ptr: http://isocpp.org/files/papers/N4162.pdf
//                        http://isocpp.org/files/papers/N4260.pdf
template <class T> class atomic_shared_ptr {
protected:
	shared_ptr<T> ptr GUARDED_BY(mtx);
	mutable atomic_spin_lock mtx;
	
public:
	// aka locking proxy
	template <typename U>
	class locked_ptr {
	protected:
		atomic_spin_lock& mtx_ref;
		shared_ptr<U>& ptr;
		
	public:
		explicit locked_ptr(atomic_spin_lock& mtx_, shared_ptr<U>& ptr_) noexcept : mtx_ref(mtx_), ptr(ptr_) {
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
	};
	
	// constructors:
	constexpr atomic_shared_ptr() noexcept = default;
	constexpr atomic_shared_ptr(nullptr_t) noexcept {}
	constexpr atomic_shared_ptr(shared_ptr<T> sptr) noexcept : ptr(sptr) {}
	atomic_shared_ptr(const atomic_shared_ptr&) = delete;
	
	// destructor:
	~atomic_shared_ptr() {
		mtx.lock();
		// note: shared_ptr is cleaned up / destructed automatically
		ptr.reset();
		mtx.unlock();
	}
	
	// assignment:
	floor_inline_always atomic_shared_ptr& operator=(shared_ptr<T> sptr) {
		safe_guard<atomic_spin_lock> guard(mtx);
		ptr.swap(sptr);
		return *this;
	}
	atomic_shared_ptr& operator=(const atomic_shared_ptr&) = delete;
	
	floor_inline_always void store(shared_ptr<T> sptr) noexcept {
		safe_guard<atomic_spin_lock> guard(mtx);
		ptr.swap(sptr);
	}
	
	floor_inline_always shared_ptr<T> exchange(shared_ptr<T> sptr) noexcept {
		safe_guard<atomic_spin_lock> guard(mtx);
		ptr.swap(sptr);
		return sptr;
	}
	
	floor_inline_always bool compare_exchange_strong(shared_ptr<T>& expected, const shared_ptr<T>& desired) noexcept {
		safe_guard<atomic_spin_lock> guard(mtx);
		// check if ptr and expected share the same control block (are the same)
		if(!ptr.owner_before(expected) && !expected.owner_before(ptr)) {
			ptr = desired;
			return true;
		}
		expected = ptr;
		return false;
	}
	floor_inline_always bool compare_exchange_strong(shared_ptr<T>& expected, shared_ptr<T>&& desired) noexcept {
		safe_guard<atomic_spin_lock> guard(mtx);
		// check if ptr and expected share the same control block (are the same)
		if(!ptr.owner_before(expected) && !expected.owner_before(ptr)) {
			ptr = desired;
			return true;
		}
		expected = ptr;
		return false;
	}
	floor_inline_always bool compare_exchange_weak(shared_ptr<T>& expected, const shared_ptr<T>& desired) noexcept {
		return compare_exchange_strong(expected, desired);
	}
	floor_inline_always bool compare_exchange_weak(shared_ptr<T>& expected, shared_ptr<T>&& desired) noexcept {
		return compare_exchange_strong(expected, forward(desired));
	}
	
	// modifiers:
	floor_inline_always void reset() noexcept {
		safe_guard<atomic_spin_lock> guard(mtx);
		ptr.reset();
	}
	template<class Y> floor_inline_always void reset(Y* p) {
		safe_guard<atomic_spin_lock> guard(mtx);
		ptr.reset(p);
	}
	template<class Y, class D> floor_inline_always void reset(Y* p, D d) {
		safe_guard<atomic_spin_lock> guard(mtx);
		ptr.reset(p, d);
	}
	template<class Y, class D, class A> floor_inline_always void reset(Y* p, D d, A a) {
		safe_guard<atomic_spin_lock> guard(mtx);
		ptr.reset(p, d, a);
	}
	
	// observers:
	floor_inline_always locked_ptr<T> get() const noexcept {
		return locked_ptr<T> { mtx, ptr };
	}
	floor_inline_always T* unsafe_get() const noexcept {
		return ptr.get();
	}
	floor_inline_always locked_ptr<T> operator*() const noexcept {
		return locked_ptr<T> { mtx, ptr };
	}
	floor_inline_always locked_ptr<T> operator->() const noexcept {
		return locked_ptr<T> { mtx, ptr };
	}
	floor_inline_always long use_count() const noexcept {
		safe_guard<atomic_spin_lock> guard(mtx);
		return ptr.use_count();
	}
	floor_inline_always bool unique() const noexcept {
		safe_guard<atomic_spin_lock> guard(mtx);
		return ptr.unique();
	}
	floor_inline_always explicit operator bool() const noexcept {
		safe_guard<atomic_spin_lock> guard(mtx);
		return ptr();
	}
	template<class U> floor_inline_always bool owner_before(shared_ptr<U> const& b) const {
		safe_guard<atomic_spin_lock> guard(mtx);
		return ptr.owner_before(b);
	}
	
	constexpr bool is_lock_free() const noexcept { return false; }
	
	floor_inline_always shared_ptr<T> load() const noexcept {
		safe_guard<atomic_spin_lock> guard(mtx);
		shared_ptr<T> ret = ptr;
		return ret;
	}
	
	floor_inline_always operator shared_ptr<T>() const noexcept {
		return load();
	}
	
};

#endif
