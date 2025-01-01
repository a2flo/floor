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

#if defined(FLOOR_COMPUTE_VULKAN)

namespace vulkan_image {
	//////////////////////////////////////////
	// vulkan immutable/fixed sampler type
	// NOTE: this matches the immutable samplers created in vulkan_compute.cpp
	struct sampler {
		enum FILTER_MODE : uint32_t {
			__FILTER_MODE_MASK			= (0x00000001u),
			__FILTER_MODE_SHIFT			= (0u),
			NEAREST						= (0u << __FILTER_MODE_SHIFT),
			LINEAR						= (1u << __FILTER_MODE_SHIFT),
		};
		enum COMPARE_FUNCTION : uint32_t {
			__COMPARE_FUNCTION_MASK		= (0x0000000Eu),
			__COMPARE_FUNCTION_SHIFT	= (1u),
			NEVER						= (0u << __COMPARE_FUNCTION_SHIFT),
			LESS						= (1u << __COMPARE_FUNCTION_SHIFT),
			EQUAL						= (2u << __COMPARE_FUNCTION_SHIFT),
			LESS_OR_EQUAL				= (3u << __COMPARE_FUNCTION_SHIFT),
			GREATER						= (4u << __COMPARE_FUNCTION_SHIFT),
			NOT_EQUAL					= (5u << __COMPARE_FUNCTION_SHIFT),
			GREATER_OR_EQUAL			= (6u << __COMPARE_FUNCTION_SHIFT),
			ALWAYS						= (7u << __COMPARE_FUNCTION_SHIFT),
		};
		enum ADDRESS_MODE : uint32_t {
			__ADDRESS_MODE_MASK			= (0x00000030u),
			__ADDRESS_MODE_SHIFT		= (4u),
			CLAMP_TO_EDGE				= (0u << __ADDRESS_MODE_SHIFT),
			REPEAT						= (1u << __ADDRESS_MODE_SHIFT),
			REPEAT_MIRRORED				= (2u << __ADDRESS_MODE_SHIFT),
		};
		// NOTE: this should be the MSB, because we won't actually be creating samplers for pixel addressing
		enum COORD_MODE : uint32_t {
			__COORD_MODE_MASK			= (0x00000040u),
			__COORD_MODE_SHIFT			= (6u),
			NORMALIZED					= (0u << __COORD_MODE_SHIFT),
			PIXEL						= (1u << __COORD_MODE_SHIFT),
		};
		
		uint32_t value;
		
		constexpr sampler() : value(0) {}
		constexpr sampler(const sampler& s) : value(s.value) {}
		constexpr sampler(const FILTER_MODE& filter_mode,
						  const ADDRESS_MODE& address_mode,
						  const COORD_MODE& coord_mode,
						  const COMPARE_FUNCTION& compare_func)
		: value(filter_mode | address_mode | coord_mode | compare_func) {}
		
		// unavailable
		void operator=(const sampler&) = delete;
		void operator&() = delete;
	};
	static_assert(sizeof(sampler) == 4, "invalid sampler size");
	
}

#endif
