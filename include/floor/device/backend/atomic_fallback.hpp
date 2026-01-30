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

// 32-bit: <operation, address space, pointer, value>, use with binary op
#define FLOOR_ATOMIC_FALLBACK_OP_32(op, as, ptr, val) for (;;) { \
	using u32_ptr_type = std::conditional_t<std::is_volatile_v<std::remove_pointer_t<decltype(ptr)>>, as volatile uint32_t*, as uint32_t*>; \
	const auto expected = *ptr; \
	const auto wanted = expected op val; \
	if (atomic_cmpxchg((u32_ptr_type)ptr, *(const uint32_t*)&expected, *(const uint32_t*)&wanted) == *(const uint32_t*)&expected) { \
		return expected; \
	} \
}

// 64-bit: <operation, address space, pointer, value>, use with binary op
#define FLOOR_ATOMIC_FALLBACK_OP_64(op, as, ptr, val) for (;;) { \
	using u64_ptr_type = std::conditional_t<std::is_volatile_v<std::remove_pointer_t<decltype(ptr)>>, as volatile uint64_t*, as uint64_t*>; \
	const auto expected = *ptr; \
	const auto wanted = expected op val; \
	if (atomic_cmpxchg((u64_ptr_type)ptr, *(const uint64_t*)&expected, *(const uint64_t*)&wanted) == *(const uint64_t*)&expected) { \
		return expected; \
	} \
}

// 32-bit: <function, address space, pointer, value>, use with a binary function
#define FLOOR_ATOMIC_FALLBACK_FUNC_OP_32(func, as, ptr, val) for (;;) { \
	using u32_ptr_type = std::conditional_t<std::is_volatile_v<std::remove_pointer_t<decltype(ptr)>>, as volatile uint32_t*, as uint32_t*>; \
	const auto expected = *ptr; \
	const auto wanted = func(expected, val); \
	if (atomic_cmpxchg((u32_ptr_type)ptr, *(const uint32_t*)&expected, *(const uint32_t*)&wanted) == *(const uint32_t*)&expected) { \
		return expected; \
	} \
}

// 64-bit: <function, address space, pointer, value>, use with a binary function
#define FLOOR_ATOMIC_FALLBACK_FUNC_OP_64(func, as, ptr, val) for (;;) { \
	using u64_ptr_type = std::conditional_t<std::is_volatile_v<std::remove_pointer_t<decltype(ptr)>>, as volatile uint64_t*, as uint64_t*>; \
	const auto expected = *ptr; \
	const auto wanted = func(expected, val); \
	if (atomic_cmpxchg((u64_ptr_type)ptr, *(const uint64_t*)&expected, *(const uint64_t*)&wanted) == *(const uint64_t*)&expected) { \
		return expected; \
	} \
}

// 32-bit uint atomic inc/dec + cmp fallback functions
#define FLOOR_ATOMIC_FALLBACK_INC(expected, cmp_val) (expected >= cmp_val ? 0u : expected + 1u)
#define FLOOR_ATOMIC_FALLBACK_DEC(expected, cmp_val) (expected == 0u || expected > cmp_val ? cmp_val : expected - 1u)
