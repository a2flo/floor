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

#ifndef __FLOOR_COMPUTE_DEVICE_ATOMIC_FALLBACK_HPP__
#define __FLOOR_COMPUTE_DEVICE_ATOMIC_FALLBACK_HPP__

// 32-bit: <operation, address space, pointer, value>, use with binary op
#define FLOOR_ATOMIC_FALLBACK_OP_32(op, as, ptr, val) for(;;) { \
	typedef conditional_t<is_volatile_v<remove_pointer_t<decltype(ptr)>>, as volatile uint32_t*, as uint32_t*> u32_ptr_type; \
	const auto expected = *ptr; \
	const auto wanted = expected op val; \
	if(atomic_cmpxchg((u32_ptr_type)ptr, *(const uint32_t*)&expected, *(const uint32_t*)&wanted) == *(const uint32_t*)&expected) { \
		return expected; \
	} \
}

// 64-bit: <operation, address space, pointer, value>, use with binary op
#define FLOOR_ATOMIC_FALLBACK_OP_64(op, as, ptr, val) for(;;) { \
	typedef conditional_t<is_volatile_v<remove_pointer_t<decltype(ptr)>>, as volatile uint64_t*, as uint64_t*> u64_ptr_type; \
	const auto expected = *ptr; \
	const auto wanted = expected op val; \
	if(atomic_cmpxchg((u64_ptr_type)ptr, *(const uint64_t*)&expected, *(const uint64_t*)&wanted) == *(const uint64_t*)&expected) { \
		return expected; \
	} \
}

// 32-bit: <function, address space, pointer, value>, use with a binary function
#define FLOOR_ATOMIC_FALLBACK_FUNC_OP_32(func, as, ptr, val) for(;;) { \
	typedef conditional_t<is_volatile_v<remove_pointer_t<decltype(ptr)>>, as volatile uint32_t*, as uint32_t*> u32_ptr_type; \
	const auto expected = *ptr; \
	const auto wanted = func(expected, val); \
	if(atomic_cmpxchg((u32_ptr_type)ptr, *(const uint32_t*)&expected, *(const uint32_t*)&wanted) == *(const uint32_t*)&expected) { \
		return expected; \
	} \
}

// 64-bit: <function, address space, pointer, value>, use with a binary function
#define FLOOR_ATOMIC_FALLBACK_FUNC_OP_64(func, as, ptr, val) for(;;) { \
	typedef conditional_t<is_volatile_v<remove_pointer_t<decltype(ptr)>>, as volatile uint64_t*, as uint64_t*> u64_ptr_type; \
	const auto expected = *ptr; \
	const auto wanted = func(expected, val); \
	if(atomic_cmpxchg((u64_ptr_type)ptr, *(const uint64_t*)&expected, *(const uint64_t*)&wanted) == *(const uint64_t*)&expected) { \
		return expected; \
	} \
}

#endif
