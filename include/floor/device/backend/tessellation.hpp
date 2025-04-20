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

#if defined(FLOOR_DEVICE_HOST_COMPUTE) || defined(FLOOR_DEVICE_METAL) || defined(FLOOR_DEVICE_VULKAN)

namespace fl {

//! tessellation levels/factors for a triangle
struct triangle_tessellation_levels_t {
	//! outer/edge levels
	half outer[3];
	//! inner/inside levels
	half inner;
};
//! tessellation levels/factors for a quad
struct quad_tessellation_levels_t {
	//! outer/edge levels
	half outer[4];
	//! inner/inside levels
	half inner[2];
};

//! patch control point wrapper
//! NOTE: this is backend-specific
template <typename T> using patch_control_point =
#if defined(FLOOR_DEVICE_METAL)
metal_patch_control_point<T>
#elif defined(FLOOR_DEVICE_VULKAN)
vulkan_patch_control_point<T>
#elif defined(FLOOR_DEVICE_HOST_COMPUTE)
host_patch_control_point<T>
#else
#error "unhandled backend"
#endif
;

} // namespace fl

#endif
