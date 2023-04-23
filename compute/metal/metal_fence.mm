/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2023 Florian Ziesche
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

#include <floor/core/essentials.hpp>

#if !defined(FLOOR_NO_METAL)
#include <floor/compute/compute_queue.hpp>
#include <floor/compute/metal/metal_fence.hpp>
#include <floor/compute/metal/metal_device.hpp>

metal_fence::metal_fence(id <MTLFence> mtl_fence_) : compute_fence(), mtl_fence(mtl_fence_) {
}

void metal_fence::set_debug_label(const string& label) {
	compute_fence::set_debug_label(label);
	if (mtl_fence) {
		mtl_fence.label = [NSString stringWithUTF8String:debug_label.c_str()];
	}
}

#endif
