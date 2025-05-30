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

#include <floor/device/device_fence.hpp>

#if !defined(FLOOR_NO_METAL)
#include <Metal/MTLFence.h>

namespace fl {

class metal_fence final : public device_fence {
public:
	metal_fence(id <MTLFence> mtl_fence_);
	~metal_fence() override;
	
	//! returns the Metal specific fence object
	id <MTLFence> get_metal_fence() const {
		return mtl_fence;
	}
	
	void set_debug_label(const std::string& label) override;
	
protected:
	id <MTLFence> mtl_fence { nil };
	
};

} // namespace fl

#endif
