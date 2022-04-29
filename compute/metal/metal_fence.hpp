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

#ifndef __FLOOR_METAL_FENCE_HPP__
#define __FLOOR_METAL_FENCE_HPP__

#include <floor/compute/compute_fence.hpp>

#if !defined(FLOOR_NO_METAL)
#include <Metal/MTLFence.h>

FLOOR_PUSH_WARNINGS()
FLOOR_IGNORE_WARNING(weak-vtables)

class metal_fence final : public compute_fence {
public:
	metal_fence(id <MTLFence> mtl_fence_);
	~metal_fence() override = default;
	
	//! returns the metal specific fence object
	id <MTLFence> get_metal_fence() const {
		return mtl_fence;
	}
	
protected:
	id <MTLFence> mtl_fence { nil };
	
};

FLOOR_POP_WARNINGS()

#endif

#endif
