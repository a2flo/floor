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

// must use clang
#if !defined(__clang__)
#error "unsupported compiler"
#endif

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

//! holder class (+actual locking and unlocking) for the MULTI_GUARD/multi_guard_*
template <class mtxs_tuple_type>
class safe_multi_guard_holder {
protected:
	mtxs_tuple_type mtxs;
	
	//! unlocks all contained mutexes
	//! NOTE: disabled thread-safety analysis here, because it can't be properly handled here and RELEASE() in multi_guard_* destructor already signals it
	template <size_t... Indices>
	inline void unlock_all(std::index_sequence<Indices...>) NO_THREAD_SAFETY_ANALYSIS {
		((void)std::get<Indices>(mtxs).unlock(), ...);
	}
	
public:
	template <typename... mutex_types>
	explicit safe_multi_guard_holder(mutex_types&... mtx_refs) : mtxs(mtx_refs...) {
		std::lock(mtx_refs...);
	}
	safe_multi_guard_holder(safe_multi_guard_holder&& holder) : mtxs(std::move(holder.mtxs)) {}
	~safe_multi_guard_holder() {
		unlock_all(std::make_index_sequence<std::tuple_size<mtxs_tuple_type>::value> {});
	}
	
};

//! creates a tuple of references to the specified arguments
template <typename... Args>
static constexpr inline auto make_refs_tuple(Args&&... args) {
	return std::tuple<std::decay_t<Args>&...> { args... };
}

#define GUARD_ID_CONCAT(num) guard_ ## num
#define GUARD_ID_EVAL(num) GUARD_ID_CONCAT(num)
#define GUARD(mtx) safe_guard<std::decay_t<decltype(mtx)>> GUARD_ID_EVAL(__LINE__) (mtx)

#define MULTI_GUARD_NAME_CONCAT(prefix, num, suffix) prefix ## num ## suffix
#define MULTI_GUARD_NAME(prefix, num, suffix) MULTI_GUARD_NAME_CONCAT(prefix, num, suffix)
//! variadic all-or-nothing locking of multiple locks
//! NOTE: we need to construct a local temporary class, so that the ACQUIRE() attribute can actually be used with variadic parameters
//! NOTE: this acts as a replacement for std::scoped_lock
#define MULTI_GUARD(...) \
using MULTI_GUARD_NAME(multi_guard_, __LINE__, _tuple_t) = decltype(make_refs_tuple(__VA_ARGS__)); \
struct SCOPED_CAPABILITY MULTI_GUARD_NAME(multi_guard_, __LINE__, _t) { \
	safe_multi_guard_holder<MULTI_GUARD_NAME(multi_guard_, __LINE__, _tuple_t)> holder; \
	explicit MULTI_GUARD_NAME(multi_guard_, __LINE__, _t) (decltype(holder)&& holder_) ACQUIRE(__VA_ARGS__) : holder(std::move(holder_)) {} \
	~ MULTI_GUARD_NAME(multi_guard_, __LINE__, _t) () RELEASE() {} \
} MULTI_GUARD_NAME(multi_guard_, __LINE__, _object) { safe_multi_guard_holder<MULTI_GUARD_NAME(multi_guard_, __LINE__, _tuple_t)> { __VA_ARGS__ } }

#endif
