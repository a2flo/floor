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

#pragma once

// NOTE: need to be defined after all backend specific atomic_* functions

template <typename T> T floor_atomic_fetch_add(global T* addr, const T& val, memory_order) { return atomic_add(addr, val); }
template <typename T> T floor_atomic_fetch_sub(global T* addr, const T& val, memory_order) { return atomic_sub(addr, val); }
template <typename T> T floor_atomic_fetch_inc(global T* addr, memory_order) { return atomic_inc(addr); }
template <typename T> T floor_atomic_fetch_dec(global T* addr, memory_order) { return atomic_dec(addr); }
template <typename T> T floor_atomic_fetch_and(global T* addr, const T& val, memory_order) { return atomic_and(addr, val); }
template <typename T> T floor_atomic_fetch_or(global T* addr, const T& val, memory_order) { return atomic_or(addr, val); }
template <typename T> T floor_atomic_fetch_xor(global T* addr, const T& val, memory_order) { return atomic_xor(addr, val); }
template <typename T> T floor_atomic_exchange(global T* addr, const T& val, memory_order) { return atomic_xchg(addr, val); }
template <typename T> void floor_atomic_store(global T* addr, const T& val, memory_order) { atomic_store(addr, val); }
template <typename T> void floor_atomic_init(global T* addr, const T& val, memory_order) { atomic_store(addr, val); }
template <typename T> T floor_atomic_load(global T* addr, memory_order) { return atomic_load(addr); }
template <typename T> bool floor_atomic_compare_exchange_strong(global T* addr, const T* expected, const T& desired,
																memory_order, memory_order) {
	const auto old = *expected;
	return (atomic_cmpxchg(addr, *expected, desired) == old);
}

#if !defined(FLOOR_COMPUTE_CUDA) && !defined(FLOOR_COMPUTE_HOST) // cuda and host don't require address space specialization
template <typename T> T floor_atomic_fetch_add(local T* addr, const T& val, memory_order) { return atomic_add(addr, val); }
template <typename T> T floor_atomic_fetch_sub(local T* addr, const T& val, memory_order) { return atomic_sub(addr, val); }
template <typename T> T floor_atomic_fetch_inc(local T* addr, memory_order) { return atomic_inc(addr); }
template <typename T> T floor_atomic_fetch_dec(local T* addr, memory_order) { return atomic_dec(addr); }
template <typename T> T floor_atomic_fetch_and(local T* addr, const T& val, memory_order) { return atomic_and(addr, val); }
template <typename T> T floor_atomic_fetch_or(local T* addr, const T& val, memory_order) { return atomic_or(addr, val); }
template <typename T> T floor_atomic_fetch_xor(local T* addr, const T& val, memory_order) { return atomic_xor(addr, val); }
template <typename T> T floor_atomic_exchange(local T* addr, const T& val, memory_order) { return atomic_xchg(addr, val); }
template <typename T> void floor_atomic_store(local T* addr, const T& val, memory_order) { atomic_store(addr, val); }
template <typename T> void floor_atomic_init(local T* addr, const T& val, memory_order) { atomic_store(addr, val); }
template <typename T> T floor_atomic_load(local T* addr, memory_order) { return atomic_load(addr); }
template <typename T> bool floor_atomic_compare_exchange_strong(local T* addr, const T* expected, const T& desired,
																memory_order, memory_order) {
	const auto old = *expected;
	return (atomic_cmpxchg(addr, *expected, desired) == old);
}
#endif
