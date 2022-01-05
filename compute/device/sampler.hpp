/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2022 Florian Ziesche
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

#ifndef __FLOOR_COMPUTE_DEVICE_SAMPLER_HPP__
#define __FLOOR_COMPUTE_DEVICE_SAMPLER_HPP__

//! depth compare functions
//! NOTE: this is the order used by CUDA, host-side Metal and Vulkan
enum class COMPARE_FUNCTION : uint32_t {
	NEVER				= 0u,
	LESS				= 1u,
	EQUAL				= 2u,
	LESS_OR_EQUAL		= 3u,
	GREATER				= 4u,
	NOT_EQUAL			= 5u,
	GREATER_OR_EQUAL	= 6u,
	ALWAYS				= 7u,
};

//! preliminary/wip sampler type
struct sampler {
	COMPARE_FUNCTION compare_function;
};

#endif
