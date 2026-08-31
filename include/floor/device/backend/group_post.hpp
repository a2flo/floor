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

namespace fl {

//! common 8-/16-/32-bit simd_match_any support for all backends that don't have native support for them
//! NOTE: requires simd_ballot() support
//! NOTE: these assume SIMD32 support (but should technically work on less than that as well)
#if !FLOOR_DEVICE_INFO_HAS_SUB_GROUP_MATCH_ANY_NATIVE && FLOOR_DEVICE_INFO_HAS_SUB_GROUP_BALLOT

//! returns a mask of all lanes that have the same specified "value"
//! NOTE: if the calling lane is part of "valid_mask" it will be contained in the return mask, otherwise the returned mask is 0 for that lane
template <typename data_type>
static inline uint32_t simd_match_any_generic(const data_type value, const uint32_t valid_mask) {
	uint32_t match_mask = (valid_mask & (1u << sub_group_local_id) ? valid_mask : 0u);
#pragma unroll
	for (uint16_t i = 0; i < sizeof(data_type) * 8u; ++i) {
		const bool is_bit_set = (value & (1u << i));
		const auto lane_mask = simd_ballot(is_bit_set);
		match_mask &= (is_bit_set ? lane_mask : ~lane_mask);
	}
	return match_mask;
}
//! same as above, but with all lanes implicitly part of the mask
template <typename data_type>
static inline uint32_t simd_match_any_generic(const data_type value) {
	uint32_t match_mask = ~0u;
#pragma unroll
	for (uint16_t i = 0; i < sizeof(data_type) * 8u; ++i) {
		const bool is_bit_set = (value & (1u << i));
		const auto lane_mask = simd_ballot(is_bit_set);
		match_mask &= (is_bit_set ? lane_mask : ~lane_mask);
	}
	return match_mask;
}

static inline uint32_t simd_match_any(const uint8_t value, const uint32_t valid_mask) {
	return simd_match_any_generic(value, valid_mask);
}
static inline uint32_t simd_match_any(const uint8_t value) {
	return simd_match_any_generic(value);
}

static inline uint32_t simd_match_any(const uint16_t value, const uint32_t valid_mask) {
	return simd_match_any_generic(value, valid_mask);
}
static inline uint32_t simd_match_any(const uint16_t value) {
	return simd_match_any_generic(value);
}

static inline uint32_t simd_match_any(const uint32_t value, const uint32_t valid_mask) {
	return simd_match_any_generic(value, valid_mask);
}
static inline uint32_t simd_match_any(const uint32_t value) {
	return simd_match_any_generic(value);
}
#endif

} // namespace fl
