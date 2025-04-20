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
#include <string>
#include <cstdint>

namespace fl {

//! synchronization stages (e.g. for fences)
enum class SYNC_STAGE : uint32_t {
	NONE						= 0u,
	VERTEX						= (1u << 0u),
	TESSELLATION				= (1u << 1u),
	FRAGMENT					= (1u << 2u),
	
	//! mostly Vulkan-specific sync stage (on Metal this aliases FRAGMENT)
	COLOR_ATTACHMENT_OUTPUT		= (1u << 3u),
	//! Vulkan-specific sync stage
	TOP_OF_PIPE					= (1u << 4u),
	//! Vulkan-specific sync stage
	BOTTOM_OF_PIPE				= (1u << 5u),
};
floor_global_enum_ext(SYNC_STAGE)

//! a lightweight synchronization primitive
//! NOTE: this only supports synchronization within the same device_queue
class device_fence {
public:
	device_fence() = default;
	virtual ~device_fence() = default;
	
	//! sets the debug label for this fence object (e.g. for display in a debugger)
	virtual void set_debug_label(const std::string& label);
	//! returns the current debug label
	virtual const std::string& get_debug_label() const;
	
protected:
	std::string debug_label;
	
};

} // namespace fl
