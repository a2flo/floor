/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2019 Florian Ziesche
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

// thread macros source: http://clang.llvm.org/docs/ThreadSafetyAnalysis.html#mutexheader

#ifndef __FLOOR_THREAD_SAFETY_HPP__
#define __FLOOR_THREAD_SAFETY_HPP__

// Enable thread safety attributes only with clang.
// The attributes can be safely erased when compiling with other compilers.
#if defined(__clang__) && !defined(_MSC_VER)
#define THREAD_ANNOTATION_ATTRIBUTE__(x) __attribute__((x))
#else
#define THREAD_ANNOTATION_ATTRIBUTE__(x) // no-op
#endif

#define CAPABILITY(x) \
	THREAD_ANNOTATION_ATTRIBUTE__(capability(x))

#define SCOPED_CAPABILITY \
	THREAD_ANNOTATION_ATTRIBUTE__(scoped_lockable)

#define GUARDED_BY(x) \
	THREAD_ANNOTATION_ATTRIBUTE__(guarded_by(x))

#define PT_GUARDED_BY(x) \
	THREAD_ANNOTATION_ATTRIBUTE__(pt_guarded_by(x))

#define ACQUIRED_BEFORE(...) \
	THREAD_ANNOTATION_ATTRIBUTE__(acquired_before(__VA_ARGS__))

#define ACQUIRED_AFTER(...) \
	THREAD_ANNOTATION_ATTRIBUTE__(acquired_after(__VA_ARGS__))

#define REQUIRES(...) \
	THREAD_ANNOTATION_ATTRIBUTE__(requires_capability(__VA_ARGS__))

#define REQUIRES_SHARED(...) \
	THREAD_ANNOTATION_ATTRIBUTE__(requires_shared_capability(__VA_ARGS__))

#define ACQUIRE(...) \
	THREAD_ANNOTATION_ATTRIBUTE__(acquire_capability(__VA_ARGS__))

#define ACQUIRE_SHARED(...) \
	THREAD_ANNOTATION_ATTRIBUTE__(acquire_shared_capability(__VA_ARGS__))

#define RELEASE(...) \
	THREAD_ANNOTATION_ATTRIBUTE__(release_capability(__VA_ARGS__))

#define RELEASE_SHARED(...) \
	THREAD_ANNOTATION_ATTRIBUTE__(release_shared_capability(__VA_ARGS__))

#define TRY_ACQUIRE(...) \
	THREAD_ANNOTATION_ATTRIBUTE__(exclusive_trylock_function(__VA_ARGS__))

#define TRY_ACQUIRE_SHARED(...) \
	THREAD_ANNOTATION_ATTRIBUTE__(try_acquire_shared_capability(__VA_ARGS__))

#define EXCLUDES(...) \
	THREAD_ANNOTATION_ATTRIBUTE__(locks_excluded(__VA_ARGS__))

#define ASSERT_CAPABILITY(x) \
	THREAD_ANNOTATION_ATTRIBUTE__(assert_capability(x))

#define ASSERT_SHARED_CAPABILITY(x) \
	THREAD_ANNOTATION_ATTRIBUTE__(assert_shared_capability(x))

#define RETURN_CAPABILITY(x) \
	THREAD_ANNOTATION_ATTRIBUTE__(lock_returned(x))

#define NO_THREAD_SAFETY_ANALYSIS \
	THREAD_ANNOTATION_ATTRIBUTE__(no_thread_safety_analysis)

//
#if defined(__clang__)
#if __has_feature(thread_sanitizer)
#define ds_no_sanitize_thread __attribute__((no_sanitize_thread))
#else
#define ds_no_sanitize_thread
#endif
#else
#define ds_no_sanitize_thread
#endif

// wrappers / replacements
#include <mutex>

#if defined(__clang__) && !defined(_MSC_VER)
// wrapper around std::mutex, based on libc++
class CAPABILITY("mutex") safe_mutex {
protected:
	std::mutex mtx;
	
public:
#if !defined(__GLIBCXX__) // libstdc++ is not standard compliant - don't make it constexpr there for now
	constexpr
#endif
	safe_mutex() noexcept = default;
	~safe_mutex() {}
	
	safe_mutex(const safe_mutex&) = delete;
	safe_mutex& operator=(const safe_mutex&) = delete;
	
	void lock() ACQUIRE() {
		mtx.lock();
	}
	bool try_lock() TRY_ACQUIRE(true) {
		return mtx.try_lock();
	}
	void unlock() RELEASE() {
		mtx.unlock();
	}
	
	// for negative capabilities
	const safe_mutex& operator!() const { return *this; }
	
};

// wrapper around std::recursive_mutex, based on libc++
class CAPABILITY("mutex") safe_recursive_mutex {
protected:
	std::recursive_mutex mtx;
	
public:
	safe_recursive_mutex() = default;
	~safe_recursive_mutex() {}
	
	safe_recursive_mutex(const safe_recursive_mutex&) = delete;
	safe_recursive_mutex& operator=(const safe_recursive_mutex&) = delete;
	
	void lock() ACQUIRE() {
		mtx.lock();
	}
	bool try_lock() TRY_ACQUIRE(true) {
		return mtx.try_lock();
	}
	void unlock() RELEASE() {
		mtx.unlock();
	}
	
	// for negative capabilities
	const safe_recursive_mutex& operator!() const { return *this; }
	
};

// replacement for std::lock_guard, based on libc++
template <class Mutex>
class SCOPED_CAPABILITY safe_guard {
public:
	typedef Mutex mutex_type;
	
protected:
	mutex_type& mtx_ref;
	
public:
	explicit safe_guard(mutex_type& m_ref) ACQUIRE(m_ref) : mtx_ref(m_ref) {
		mtx_ref.lock();
	}
	safe_guard(mutex_type& m_ref, std::adopt_lock_t) REQUIRES(m_ref) : mtx_ref(m_ref) {}
	~safe_guard() RELEASE() {
		mtx_ref.unlock();
	}
	
	safe_guard(safe_guard const&) = delete;
	safe_guard& operator=(safe_guard const&) = delete;
};

#else
// for gcc and older clang: simply use std mutex/recursive_mutex/lock_guard
#define safe_mutex mutex
#define safe_recursive_mutex recursive_mutex
#define safe_guard lock_guard
#endif

#define GUARD_ID_CONCAT(num) guard_ ## num
#define GUARD_ID_EVAL(num) GUARD_ID_CONCAT(num)
#define GUARD(mtx) safe_guard<decay<decltype(mtx)>::type> GUARD_ID_EVAL(__LINE__) (mtx)

#endif
