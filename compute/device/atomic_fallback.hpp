/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2015 Florian Ziesche
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
	const auto expected = *ptr; \
	const auto wanted = expected op val; \
	if(atomic_cmpxchg((as uint32_t*)ptr, *(uint32_t*)&expected, *(uint32_t*)&wanted) == *(uint32_t*)&expected) { \
		return expected; \
	} \
}

// 64-bit: <operation, address space, pointer, value>, use with binary op
#define FLOOR_ATOMIC_FALLBACK_OP_64(op, as, ptr, val) for(;;) { \
	const auto expected = *ptr; \
	const auto wanted = expected op val; \
	if(atomic_cmpxchg((as uint64_t*)ptr, *(uint64_t*)&expected, *(uint64_t*)&wanted) == *(uint64_t*)&expected) { \
		return expected; \
	} \
}

// 32-bit: <function, address space, pointer, value>, use with a binary function
#define FLOOR_ATOMIC_FALLBACK_FUNC_OP_32(func, as, ptr, val) for(;;) { \
	const auto expected = *ptr; \
	const auto wanted = func(expected, val); \
	if(atomic_cmpxchg((as uint32_t*)ptr, *(uint32_t*)&expected, *(uint32_t*)&wanted) == *(uint32_t*)&expected) { \
		return expected; \
	} \
}

// 64-bit: <function, address space, pointer, value>, use with a binary function
#define FLOOR_ATOMIC_FALLBACK_FUNC_OP_64(func, as, ptr, val) for(;;) { \
	const auto expected = *ptr; \
	const auto wanted = func(expected, val); \
	if(atomic_cmpxchg((as uint64_t*)ptr, *(uint64_t*)&expected, *(uint64_t*)&wanted) == *(uint64_t*)&expected) { \
		return expected; \
	} \
}

#endif
