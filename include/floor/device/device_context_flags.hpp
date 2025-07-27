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

#include <floor/core/essentials.hpp>
#include <floor/core/enum_helpers.hpp>

namespace fl {

//! global context flags that can be specified during context creation
enum class DEVICE_CONTEXT_FLAGS : uint32_t {
	NONE = 0u,
	
	//! Metal-only (right now): disables any automatic resource tracking on the allocated Metal object
	//! NOTE: this is achieved by automatically adding MEMORY_FLAG::NO_RESOURCE_TRACKING for all buffers/images that are created
	NO_RESOURCE_TRACKING = (1u << 0u),
	
	//! Vulkan-only: flag that disables blocking queue submission
	VULKAN_NO_BLOCKING = (1u << 1u),
	
	//! Metal/Vulkan-only: enables explicit heap memory management
	//! by default, all supported allocations will be made from internal memory heaps rather than dedicated allocations,
	//! enabling this flag disables that behavior and all allocations are dedicated unless MEMORY_FLAG::HEAP_ALLOCATION is manually specified
	//! NOTE: mutually exclusive with DISABLE_HEAP
	EXPLICIT_HEAP = (1u << 2u),
	
	//! Metal/Vulkan-only: disbles heap memory management
	//! by default, all supported allocations will be made from internal memory heaps rather than dedicated allocations,
	//! enabling this flag disables that behavior and all allocations are dedicated
	//! NOTE: mutually exclusive with EXPLICIT_HEAP
	DISABLE_HEAP = (1u << 3u),
};
floor_global_enum_ext(DEVICE_CONTEXT_FLAGS)

} // namespace fl
