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

#if defined(FLOOR_DEVICE_HOST_COMPUTE)

extern "C" {
#if !defined(FLOOR_DEVICE_HOST_COMPUTE_IS_DEVICE)
uint32_t floor_host_compute_simd_ballot(bool predicate) __attribute__((noduplicate)) FLOOR_HOST_COMPUTE_CC;
#else
uint32_t floor_host_compute_device_simd_ballot(bool predicate) __attribute__((noduplicate)) FLOOR_HOST_COMPUTE_CC;
#endif
}

namespace fl {

// native Host-Compute ballot: always returns a 32-bit uint mask
floor_inline_always static uint32_t simd_ballot_native(bool predicate) __attribute__((noduplicate, convergent)) {
#if !defined(FLOOR_DEVICE_HOST_COMPUTE_IS_DEVICE)
	return floor_host_compute_simd_ballot(predicate);
#else
	return floor_host_compute_device_simd_ballot(predicate);
#endif
}

floor_inline_always static uint32_t simd_ballot(bool predicate) __attribute__((noduplicate, convergent)) {
	return simd_ballot_native(predicate);
}

floor_inline_always static uint64_t simd_ballot_64(bool predicate) __attribute__((noduplicate, convergent)) {
	return simd_ballot_native(predicate);
}

// Host-Compute parallel group operation implementations / support
namespace algorithm::group {

// TODO

} // namespace algorithm::group

} // namespace fl

#endif

