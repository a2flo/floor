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

//! internal cuda sampler type
struct cuda_sampler {
	enum COORD_MODE : uint32_t {
		PIXEL					= 0u,
		NORMALIZED				= 1u,
		
		COORD_MODE_MAX			= NORMALIZED,
		COORD_MODE_SHIFT		= 0u,
		COORD_MODE_MASK			= (1u << COORD_MODE_SHIFT),
	};
	enum FILTER_MODE : uint32_t {
		NEAREST					= 0u,
		LINEAR					= 1u,
		
		FILTER_MODE_MAX			= LINEAR,
		FILTER_MODE_SHIFT		= 1u,
		FILTER_MODE_MASK		= (1u << FILTER_MODE_SHIFT),
	};
	enum COMPARE_FUNCTION : uint32_t {
		NONE					= 0u,
		//! NOTE: this is handled on the compiler side
		NEVER					= NONE,
		//! NOTE: this is handled on the compiler side
		ALWAYS					= NONE,
		
		LESS_OR_EQUAL			= 1u,
		GREATER_OR_EQUAL		= 2u,
		LESS					= 3u,
		GREATER					= 4u,
		EQUAL					= 5u,
		NOT_EQUAL				= 6u,
		
		COMPARE_FUNCTION_MAX	= NOT_EQUAL,
		COMPARE_FUNCTION_SHIFT	= 2u,
		COMPARE_FUNCTION_MASK	= (7u << COMPARE_FUNCTION_SHIFT),
	};
	enum ADDRESS_MODE : uint32_t {
		CLAMP_TO_EDGE			= 0u,
		REPEAT					= 1u,
		REPEAT_MIRRORED			= 2u,
		
		ADDRESS_MODE_MAX		= REPEAT_MIRRORED,
		ADDRESS_MODE_SHIFT		= 5u,
		ADDRESS_MODE_MASK		= (1u << ADDRESS_MODE_SHIFT),
	};
	
	static constexpr COORD_MODE get_coord_mode(const uint32_t& index) {
		return (COORD_MODE)((index & COORD_MODE_MASK) >> COORD_MODE_SHIFT);
	}
	
	static constexpr FILTER_MODE get_filter_mode(const uint32_t& index) {
		return (FILTER_MODE)((index & FILTER_MODE_MASK) >> FILTER_MODE_SHIFT);
	}

	static constexpr ADDRESS_MODE get_address_mode(const uint32_t& index) {
		return (ADDRESS_MODE)((index & ADDRESS_MODE_MASK) >> ADDRESS_MODE_SHIFT);
	}

	static constexpr COMPARE_FUNCTION get_compare_function(const uint32_t& index) {
		return (COMPARE_FUNCTION)((index & COMPARE_FUNCTION_MASK) >> COMPARE_FUNCTION_SHIFT);
	}
	
	static constexpr uint32_t sampler_index(const COORD_MODE& coord_mode,
											const FILTER_MODE& filter_mode,
											const ADDRESS_MODE& address_mode,
											const COMPARE_FUNCTION& compare_function) {
		uint32_t ret = 0;
		ret |= (coord_mode << COORD_MODE_SHIFT);
		ret |= (filter_mode << FILTER_MODE_SHIFT);
		ret |= (address_mode << ADDRESS_MODE_SHIFT);
		ret |= (compare_function << COMPARE_FUNCTION_SHIFT);
		return ret;
	}
	
	static constexpr const uint32_t max_sampler_count { 2u /* coord */ * 2u /* filter */ * 8u /* compare+ */ * 3u /* address */ };
	static_assert(max_sampler_count == 96u, "invalid max sampler count");
	
};
