/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2024 Florian Ziesche
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

#ifndef __FLOOR_FENCE_HPP__
#define __FLOOR_FENCE_HPP__

#include <floor/core/essentials.hpp>
#include <string>

//! a lightweight synchronization primitive
//! NOTE: this only supports synchronization within the same compute_queue
class compute_fence {
public:
	compute_fence() = default;
	virtual ~compute_fence() = default;
	
	//! synchronization stage for fences
	enum class SYNC_STAGE {
		NONE = 0u,
		VERTEX = 1u,
		TESSELLATION = 2u,
		FRAGMENT = 3u,
		
		//! mostly Vulkan-specific sync stage (on Metal this aliases FRAGMENT)
		COLOR_ATTACHMENT_OUTPUT = 10u,
	};
	
	//! sets the debug label for this fence object (e.g. for display in a debugger)
	virtual void set_debug_label(const std::string& label);
	//! returns the current debug label
	virtual const std::string& get_debug_label() const;

protected:
	std::string debug_label;
	
};

#endif
