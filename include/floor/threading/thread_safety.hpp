/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2025 Florian Ziesche
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

#pragma once

// Enable thread safety attributes only with clang.
// The attributes can be safely erased when compiling with other compilers.
#if defined(__clang__)
#define THREAD_ANNOTATION_ATTRIBUTE__(x) __attribute__((x))
#else
// must use clang
#error "unsupported compiler"
#endif

#define CAPABILITY(x) THREAD_ANNOTATION_ATTRIBUTE__(capability(x))
#define REENTRANT_CAPABILITY THREAD_ANNOTATION_ATTRIBUTE__(reentrant_capability)
#define SCOPED_CAPABILITY THREAD_ANNOTATION_ATTRIBUTE__(scoped_lockable)
#define GUARDED_BY(x) THREAD_ANNOTATION_ATTRIBUTE__(guarded_by(x))
#define PT_GUARDED_BY(x) THREAD_ANNOTATION_ATTRIBUTE__(pt_guarded_by(x))
#define ACQUIRED_BEFORE(...) THREAD_ANNOTATION_ATTRIBUTE__(acquired_before(__VA_ARGS__))
#define ACQUIRED_AFTER(...) THREAD_ANNOTATION_ATTRIBUTE__(acquired_after(__VA_ARGS__))
#define REQUIRES(...) THREAD_ANNOTATION_ATTRIBUTE__(requires_capability(__VA_ARGS__))
#define REQUIRES_SHARED(...) THREAD_ANNOTATION_ATTRIBUTE__(requires_shared_capability(__VA_ARGS__))
#define ACQUIRE(...) THREAD_ANNOTATION_ATTRIBUTE__(acquire_capability(__VA_ARGS__))
#define ACQUIRE_SHARED(...) THREAD_ANNOTATION_ATTRIBUTE__(acquire_shared_capability(__VA_ARGS__))
#define RELEASE(...) THREAD_ANNOTATION_ATTRIBUTE__(release_capability(__VA_ARGS__))
#define RELEASE_SHARED(...) THREAD_ANNOTATION_ATTRIBUTE__(release_shared_capability(__VA_ARGS__))
#define RELEASE_GENERIC(...) THREAD_ANNOTATION_ATTRIBUTE__(release_generic_capability(__VA_ARGS__))
#define TRY_ACQUIRE(...) THREAD_ANNOTATION_ATTRIBUTE__(exclusive_trylock_function(__VA_ARGS__))
#define TRY_ACQUIRE_SHARED(...) THREAD_ANNOTATION_ATTRIBUTE__(try_acquire_shared_capability(__VA_ARGS__))
#define EXCLUDES(...) THREAD_ANNOTATION_ATTRIBUTE__(locks_excluded(__VA_ARGS__))
#define ASSERT_CAPABILITY(x) THREAD_ANNOTATION_ATTRIBUTE__(assert_capability(x))
#define ASSERT_SHARED_CAPABILITY(x) THREAD_ANNOTATION_ATTRIBUTE__(assert_shared_capability(x))
#define RETURN_CAPABILITY(x) THREAD_ANNOTATION_ATTRIBUTE__(lock_returned(x))
#define NO_THREAD_SAFETY_ANALYSIS THREAD_ANNOTATION_ATTRIBUTE__(no_thread_safety_analysis)

// attribute to disable thread sanitizer
#if __has_feature(thread_sanitizer)
#define ds_no_sanitize_thread __attribute__((no_sanitize_thread))
#else
#define ds_no_sanitize_thread
#endif

// wrappers / replacements
#include <mutex>
#include <shared_mutex>

namespace fl {

// wrapper around std::mutex
class CAPABILITY("mutex") safe_mutex {
protected:
	std::mutex mtx;
	
public:
#if !defined(__GLIBCXX__) && !defined(_MSVC_STL_UPDATE) // libstdc++/MSSTL are not standard compliant - don't make it constexpr there for now
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

// wrapper around std::recursive_mutex
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

// wrapper around std::shared_mutex
class CAPABILITY("shared_mutex") safe_shared_mutex {
protected:
	std::shared_mutex mtx;
	
public:
	safe_shared_mutex() noexcept = default;
	~safe_shared_mutex() {}
	
	safe_shared_mutex(const safe_shared_mutex&) = delete;
	safe_shared_mutex& operator=(const safe_shared_mutex&) = delete;
	
	void lock() ACQUIRE() {
		mtx.lock();
	}
	bool try_lock() TRY_ACQUIRE(true) {
		return mtx.try_lock();
	}
	void unlock() RELEASE() {
		mtx.unlock();
	}
	
	void lock_shared() ACQUIRE_SHARED() {
		mtx.lock_shared();
	}
	bool try_lock_shared() TRY_ACQUIRE_SHARED(true) {
		return mtx.try_lock_shared();
	}
	void unlock_shared() RELEASE_SHARED() {
		mtx.unlock_shared();
	}
	
	// for negative capabilities
	const safe_shared_mutex& operator!() const { return *this; }
	
};

// replacement for std::lock_guard
template <class Mutex>
class SCOPED_CAPABILITY safe_guard {
public:
	using mutex_type = Mutex;
	
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

// replacement for std::shared_lock
template <class Mutex>
class SCOPED_CAPABILITY safe_shared_guard {
public:
	using mutex_type = Mutex;
	
protected:
	mutex_type& mtx_ref;
	
public:
	explicit safe_shared_guard(mutex_type& m_ref) ACQUIRE_SHARED(m_ref) : mtx_ref(m_ref) {
		mtx_ref.lock_shared();
	}
	safe_shared_guard(mutex_type& m_ref, std::adopt_lock_t) REQUIRES_SHARED(m_ref) : mtx_ref(m_ref) {}
	~safe_shared_guard() RELEASE_GENERIC() /* NOTE: RELEASE_SHARED() does not work ... */ {
		mtx_ref.unlock_shared();
	}
	
	safe_shared_guard(safe_shared_guard const&) = delete;
	safe_shared_guard& operator=(safe_shared_guard const&) = delete;
};

namespace floor_multi_guard {

//! contains all the mutexes of a multi-guard and performs the intial locking and final unlocking (used for the MULTI_GUARD(...))
//! NOTE: this is currently limited to a maximum of 9 mutexes (sadly can't use variadic templates here, because thread safety analysis can't handle them)
template <typename mtxs_tuple_type>
class SCOPED_CAPABILITY safe_multi_guard {
protected:
	//! contained mutexes
	mtxs_tuple_type mtxs;
	
	//! number of mutexes in this multi-guard
	static constexpr const auto mutex_count = std::tuple_size<mtxs_tuple_type>::value;
	
	//! unlocks all contained mutexes
	//! NOTE: disabled thread-safety analysis here, because it can't be properly handled here and RELEASE() in destructor already signals it
	template <size_t... indices>
	void unlock_all(std::index_sequence<indices...>) NO_THREAD_SAFETY_ANALYSIS {
		((void)std::get<indices>(mtxs).unlock(), ...);
	}
	
public:
	template <typename mtx_type_0>
	requires(mutex_count == 1)
	explicit safe_multi_guard(mtx_type_0& mtx_0)
	ACQUIRE(mtx_0) :
	mtxs(mtx_0) {
		std::lock(mtx_0);
	}
	
	template <typename mtx_type_0, typename mtx_type_1>
	requires(mutex_count == 2)
	explicit safe_multi_guard(mtx_type_0& mtx_0, mtx_type_1& mtx_1)
	ACQUIRE(mtx_0, mtx_1) :
	mtxs(mtx_0, mtx_1) {
		std::lock(mtx_0, mtx_1);
	}
	
	template <typename mtx_type_0, typename mtx_type_1, typename mtx_type_2>
	requires(mutex_count == 3)
	explicit safe_multi_guard(mtx_type_0& mtx_0, mtx_type_1& mtx_1, mtx_type_2& mtx_2)
	ACQUIRE(mtx_0, mtx_1, mtx_2) :
	mtxs(mtx_0, mtx_1, mtx_2) {
		std::lock(mtx_0, mtx_1, mtx_2);
	}
	
	template <typename mtx_type_0, typename mtx_type_1, typename mtx_type_2, typename mtx_type_3>
	requires(mutex_count == 4)
	explicit safe_multi_guard(mtx_type_0& mtx_0, mtx_type_1& mtx_1, mtx_type_2& mtx_2, mtx_type_3& mtx_3)
	ACQUIRE(mtx_0, mtx_1, mtx_2, mtx_3) :
	mtxs(mtx_0, mtx_1, mtx_2, mtx_3) {
		std::lock(mtx_0, mtx_1, mtx_2, mtx_3);
	}
	
	template <typename mtx_type_0, typename mtx_type_1, typename mtx_type_2, typename mtx_type_3, typename mtx_type_4>
	requires(mutex_count == 5)
	explicit safe_multi_guard(mtx_type_0& mtx_0, mtx_type_1& mtx_1, mtx_type_2& mtx_2, mtx_type_3& mtx_3, mtx_type_4& mtx_4)
	ACQUIRE(mtx_0, mtx_1, mtx_2, mtx_3, mtx_4) :
	mtxs(mtx_0, mtx_1, mtx_2, mtx_3, mtx_4) {
		std::lock(mtx_0, mtx_1, mtx_2, mtx_3, mtx_4);
	}
	
	template <typename mtx_type_0, typename mtx_type_1, typename mtx_type_2, typename mtx_type_3, typename mtx_type_4, typename mtx_type_5>
	requires(mutex_count == 6)
	explicit safe_multi_guard(mtx_type_0& mtx_0, mtx_type_1& mtx_1, mtx_type_2& mtx_2, mtx_type_3& mtx_3, mtx_type_4& mtx_4, mtx_type_5& mtx_5)
	ACQUIRE(mtx_0, mtx_1, mtx_2, mtx_3, mtx_4, mtx_5) :
	mtxs(mtx_0, mtx_1, mtx_2, mtx_3, mtx_4, mtx_5) {
		std::lock(mtx_0, mtx_1, mtx_2, mtx_3, mtx_4, mtx_5);
	}
	
	template <typename mtx_type_0, typename mtx_type_1, typename mtx_type_2, typename mtx_type_3, typename mtx_type_4, typename mtx_type_5,
			  typename mtx_type_6>
	requires(mutex_count == 7)
	explicit safe_multi_guard(mtx_type_0& mtx_0, mtx_type_1& mtx_1, mtx_type_2& mtx_2, mtx_type_3& mtx_3, mtx_type_4& mtx_4,
							  mtx_type_5& mtx_5, mtx_type_6& mtx_6)
	ACQUIRE(mtx_0, mtx_1, mtx_2, mtx_3, mtx_4, mtx_5, mtx_6) :
	mtxs(mtx_0, mtx_1, mtx_2, mtx_3, mtx_4, mtx_5, mtx_6) {
		std::lock(mtx_0, mtx_1, mtx_2, mtx_3, mtx_4, mtx_5, mtx_6);
	}
	
	template <typename mtx_type_0, typename mtx_type_1, typename mtx_type_2, typename mtx_type_3, typename mtx_type_4, typename mtx_type_5,
			  typename mtx_type_6, typename mtx_type_7>
	requires(mutex_count == 8)
	explicit safe_multi_guard(mtx_type_0& mtx_0, mtx_type_1& mtx_1, mtx_type_2& mtx_2, mtx_type_3& mtx_3, mtx_type_4& mtx_4,
							  mtx_type_5& mtx_5, mtx_type_6& mtx_6, mtx_type_7& mtx_7)
	ACQUIRE(mtx_0, mtx_1, mtx_2, mtx_3, mtx_4, mtx_5, mtx_6, mtx_7) :
	mtxs(mtx_0, mtx_1, mtx_2, mtx_3, mtx_4, mtx_5, mtx_6, mtx_7) {
		std::lock(mtx_0, mtx_1, mtx_2, mtx_3, mtx_4, mtx_5, mtx_6, mtx_7);
	}
	
	template <typename mtx_type_0, typename mtx_type_1, typename mtx_type_2, typename mtx_type_3, typename mtx_type_4, typename mtx_type_5,
			  typename mtx_type_6, typename mtx_type_7, typename mtx_type_8>
	requires(mutex_count == 9)
	explicit safe_multi_guard(mtx_type_0& mtx_0, mtx_type_1& mtx_1, mtx_type_2& mtx_2, mtx_type_3& mtx_3, mtx_type_4& mtx_4,
							  mtx_type_5& mtx_5, mtx_type_6& mtx_6, mtx_type_7& mtx_7, mtx_type_8& mtx_8)
	ACQUIRE(mtx_0, mtx_1, mtx_2, mtx_3, mtx_4, mtx_5, mtx_6, mtx_7, mtx_8) :
	mtxs(mtx_0, mtx_1, mtx_2, mtx_3, mtx_4, mtx_5, mtx_6, mtx_7, mtx_8) {
		std::lock(mtx_0, mtx_1, mtx_2, mtx_3, mtx_4, mtx_5, mtx_6, mtx_7, mtx_8);
	}
	
	~safe_multi_guard() RELEASE() {
		unlock_all(std::make_index_sequence<mutex_count> {});
	}
	
	// copy and move are not allowed
	safe_multi_guard(const safe_multi_guard&) = delete;
	safe_multi_guard(safe_multi_guard&&) = delete;
	safe_multi_guard& operator=(const safe_multi_guard&) = delete;
	safe_multi_guard& operator=(safe_multi_guard&&) = delete;
	
};

//! contains all the mutexes of a shared-multi-guard and performs the intial locking and final unlocking (used for the SHARED_MULTI_GUARD(...))
//! NOTE: this is currently limited to a maximum of 9 mutexes
template <typename mtxs_tuple_type>
class SCOPED_CAPABILITY safe_shared_multi_guard {
protected:
	//! contained mutexes
	mtxs_tuple_type mtxs;
	
	//! number of mutexes in this multi-guard
	static constexpr const auto mutex_count = std::tuple_size<mtxs_tuple_type>::value;
	
	//! unlocks all contained mutexes
	//! NOTE: disabled thread-safety analysis here, because it can't be properly handled here and RELEASE() in destructor already signals it
	template <size_t... indices>
	void unlock_all(std::index_sequence<indices...>) NO_THREAD_SAFETY_ANALYSIS {
		((void)std::get<indices>(mtxs).unlock_shared(), ...);
	}
	
public:
	template <typename mtx_type_0>
	requires(mutex_count == 1)
	explicit safe_shared_multi_guard(mtx_type_0& mtx_0)
	ACQUIRE_SHARED(mtx_0) :
	mtxs(mtx_0) {
		mtx_0.lock_shared();
	}
	
	template <typename mtx_type_0, typename mtx_type_1>
	requires(mutex_count == 2)
	explicit safe_shared_multi_guard(mtx_type_0& mtx_0, mtx_type_1& mtx_1)
	ACQUIRE_SHARED(mtx_0, mtx_1) :
	mtxs(mtx_0, mtx_1) {
		mtx_0.lock_shared();
		mtx_1.lock_shared();
	}
	
	template <typename mtx_type_0, typename mtx_type_1, typename mtx_type_2>
	requires(mutex_count == 3)
	explicit safe_shared_multi_guard(mtx_type_0& mtx_0, mtx_type_1& mtx_1, mtx_type_2& mtx_2)
	ACQUIRE_SHARED(mtx_0, mtx_1, mtx_2) :
	mtxs(mtx_0, mtx_1, mtx_2) {
		mtx_0.lock_shared();
		mtx_1.lock_shared();
		mtx_2.lock_shared();
	}
	
	template <typename mtx_type_0, typename mtx_type_1, typename mtx_type_2, typename mtx_type_3>
	requires(mutex_count == 4)
	explicit safe_shared_multi_guard(mtx_type_0& mtx_0, mtx_type_1& mtx_1, mtx_type_2& mtx_2, mtx_type_3& mtx_3)
	ACQUIRE_SHARED(mtx_0, mtx_1, mtx_2, mtx_3) :
	mtxs(mtx_0, mtx_1, mtx_2, mtx_3) {
		mtx_0.lock_shared();
		mtx_1.lock_shared();
		mtx_2.lock_shared();
		mtx_3.lock_shared();
	}
	
	template <typename mtx_type_0, typename mtx_type_1, typename mtx_type_2, typename mtx_type_3, typename mtx_type_4>
	requires(mutex_count == 5)
	explicit safe_shared_multi_guard(mtx_type_0& mtx_0, mtx_type_1& mtx_1, mtx_type_2& mtx_2, mtx_type_3& mtx_3, mtx_type_4& mtx_4)
	ACQUIRE_SHARED(mtx_0, mtx_1, mtx_2, mtx_3, mtx_4) :
	mtxs(mtx_0, mtx_1, mtx_2, mtx_3, mtx_4) {
		mtx_0.lock_shared();
		mtx_1.lock_shared();
		mtx_2.lock_shared();
		mtx_3.lock_shared();
		mtx_4.lock_shared();
	}
	
	template <typename mtx_type_0, typename mtx_type_1, typename mtx_type_2, typename mtx_type_3, typename mtx_type_4, typename mtx_type_5>
	requires(mutex_count == 6)
	explicit safe_shared_multi_guard(mtx_type_0& mtx_0, mtx_type_1& mtx_1, mtx_type_2& mtx_2, mtx_type_3& mtx_3, mtx_type_4& mtx_4,
									 mtx_type_5& mtx_5)
	ACQUIRE_SHARED(mtx_0, mtx_1, mtx_2, mtx_3, mtx_4, mtx_5) :
	mtxs(mtx_0, mtx_1, mtx_2, mtx_3, mtx_4, mtx_5) {
		mtx_0.lock_shared();
		mtx_1.lock_shared();
		mtx_2.lock_shared();
		mtx_3.lock_shared();
		mtx_4.lock_shared();
		mtx_5.lock_shared();
	}
	
	template <typename mtx_type_0, typename mtx_type_1, typename mtx_type_2, typename mtx_type_3, typename mtx_type_4, typename mtx_type_5,
			  typename mtx_type_6>
	requires(mutex_count == 7)
	explicit safe_shared_multi_guard(mtx_type_0& mtx_0, mtx_type_1& mtx_1, mtx_type_2& mtx_2, mtx_type_3& mtx_3, mtx_type_4& mtx_4,
									 mtx_type_5& mtx_5, mtx_type_6& mtx_6)
	ACQUIRE_SHARED(mtx_0, mtx_1, mtx_2, mtx_3, mtx_4, mtx_5, mtx_6) :
	mtxs(mtx_0, mtx_1, mtx_2, mtx_3, mtx_4, mtx_5, mtx_6) {
		mtx_0.lock_shared();
		mtx_1.lock_shared();
		mtx_2.lock_shared();
		mtx_3.lock_shared();
		mtx_4.lock_shared();
		mtx_5.lock_shared();
		mtx_6.lock_shared();
	}
	
	template <typename mtx_type_0, typename mtx_type_1, typename mtx_type_2, typename mtx_type_3, typename mtx_type_4, typename mtx_type_5,
			  typename mtx_type_6, typename mtx_type_7>
	requires(mutex_count == 8)
	explicit safe_shared_multi_guard(mtx_type_0& mtx_0, mtx_type_1& mtx_1, mtx_type_2& mtx_2, mtx_type_3& mtx_3, mtx_type_4& mtx_4,
									 mtx_type_5& mtx_5, mtx_type_6& mtx_6, mtx_type_7& mtx_7)
	ACQUIRE_SHARED(mtx_0, mtx_1, mtx_2, mtx_3, mtx_4, mtx_5, mtx_6, mtx_7) :
	mtxs(mtx_0, mtx_1, mtx_2, mtx_3, mtx_4, mtx_5, mtx_6, mtx_7) {
		mtx_0.lock_shared();
		mtx_1.lock_shared();
		mtx_2.lock_shared();
		mtx_3.lock_shared();
		mtx_4.lock_shared();
		mtx_5.lock_shared();
		mtx_6.lock_shared();
		mtx_7.lock_shared();
	}
	
	template <typename mtx_type_0, typename mtx_type_1, typename mtx_type_2, typename mtx_type_3, typename mtx_type_4, typename mtx_type_5,
			  typename mtx_type_6, typename mtx_type_7, typename mtx_type_8>
	requires(mutex_count == 9)
	explicit safe_shared_multi_guard(mtx_type_0& mtx_0, mtx_type_1& mtx_1, mtx_type_2& mtx_2, mtx_type_3& mtx_3, mtx_type_4& mtx_4,
									 mtx_type_5& mtx_5, mtx_type_6& mtx_6, mtx_type_7& mtx_7, mtx_type_8& mtx_8)
	ACQUIRE_SHARED(mtx_0, mtx_1, mtx_2, mtx_3, mtx_4, mtx_5, mtx_6, mtx_7, mtx_8) :
	mtxs(mtx_0, mtx_1, mtx_2, mtx_3, mtx_4, mtx_5, mtx_6, mtx_7, mtx_8) {
		mtx_0.lock_shared();
		mtx_1.lock_shared();
		mtx_2.lock_shared();
		mtx_3.lock_shared();
		mtx_4.lock_shared();
		mtx_5.lock_shared();
		mtx_6.lock_shared();
		mtx_7.lock_shared();
		mtx_8.lock_shared();
	}
	
	~safe_shared_multi_guard() RELEASE_GENERIC() {
		unlock_all(std::make_index_sequence<mutex_count> {});
	}
	
	// copy and move are not allowed
	safe_shared_multi_guard(const safe_shared_multi_guard&) = delete;
	safe_shared_multi_guard(safe_shared_multi_guard&&) = delete;
	safe_shared_multi_guard& operator=(const safe_shared_multi_guard&) = delete;
	safe_shared_multi_guard& operator=(safe_shared_multi_guard&&) = delete;
	
};

//! creates a tuple of references to the specified arguments
template <typename... Args>
static constexpr inline auto make_refs_tuple(Args&&... args) {
	return std::tuple<std::decay_t<Args>&...> { args... };
}

} // namespace floor_multi_guard
} // namespace fl

#define GUARD_ID_CONCAT(num) guard_ ## num
#define GUARD_ID_EVAL(num) GUARD_ID_CONCAT(num)
#define GUARD(mtx) const fl::safe_guard<std::decay_t<decltype(mtx)>> GUARD_ID_EVAL(__LINE__) (mtx)

#define SHARED_GUARD_ID_CONCAT(num) shared_guard_ ## num
#define SHARED_GUARD_ID_EVAL(num) SHARED_GUARD_ID_CONCAT(num)
#define SHARED_GUARD(mtx) const fl::safe_shared_guard<std::decay_t<decltype(mtx)>> SHARED_GUARD_ID_EVAL(__LINE__) (mtx)

//! all-or-nothing locking of multiple locks
//! NOTE: this acts as a replacement for std::scoped_lock
#define MULTI_GUARD_NAME_CONCAT(prefix, num, suffix) prefix ## num ## suffix
#define MULTI_GUARD_NAME(prefix, num, suffix) MULTI_GUARD_NAME_CONCAT(prefix, num, suffix)
#define MULTI_GUARD(...) const fl::floor_multi_guard::safe_multi_guard<decltype(fl::floor_multi_guard::make_refs_tuple(__VA_ARGS__))> MULTI_GUARD_NAME(multi_guard_, __LINE__, _object) { __VA_ARGS__ }

//! all-or-nothing shared locking of multiple shared locks
#define SHARED_MULTI_GUARD_NAME_CONCAT(prefix, num, suffix) prefix ## num ## suffix
#define SHARED_MULTI_GUARD_NAME(prefix, num, suffix) SHARED_MULTI_GUARD_NAME_CONCAT(prefix, num, suffix)
#define SHARED_MULTI_GUARD(...) const fl::floor_multi_guard::safe_shared_multi_guard<decltype(fl::floor_multi_guard::make_refs_tuple(__VA_ARGS__))> SHARED_MULTI_GUARD_NAME(shared_multi_guard_, __LINE__, _object) { __VA_ARGS__ }
