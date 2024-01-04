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

#ifndef __FLOOR_COMPUTE_DEVICE_HOST_POST_HPP__
#define __FLOOR_COMPUTE_DEVICE_HOST_POST_HPP__

#if defined(FLOOR_GRAPHICS_HOST)

// NOTE: not supported
floor_inline_always __attribute__((const)) static uint32_t get_vertex_id() {
	return 0;
}
// NOTE: not supported
floor_inline_always __attribute__((const)) static uint32_t get_instance_id() {
	return 0;
}
// NOTE: not supported
floor_inline_always __attribute__((const)) float2 get_point_coord() {
	return {};
}
// NOTE: not supported
floor_inline_always __attribute__((const)) static uint32_t get_view_index() {
	return 0;
}
// NOTE: not supported
floor_inline_always __attribute__((const)) static uint32_t get_primitive_id() {
	return 0;
}
// NOTE: not supported
floor_inline_always __attribute__((const)) float3 get_barycentric_coord() {
	return {};
}
// NOTE: not supported
floor_inline_always void discard_fragment() /*__attribute__((noreturn))*/ {
	// TODO: exit current fiber instead of returning here
	return;
}
// NOTE: not supported
floor_inline_always __attribute__((const)) float dfdx(float) { return 0.0f; }
// NOTE: not supported
floor_inline_always __attribute__((const)) float dfdy(float) { return 0.0f; }
// NOTE: not supported
floor_inline_always __attribute__((const)) float fwidth(float) { return 0.0f; }
// NOTE: not supported
floor_inline_always __attribute__((const)) pair<float, float> dfdx_dfdy_gradient(const float& p) {
	return { { dfdx(p) }, { dfdy(p) } };
}
// NOTE: not supported
floor_inline_always __attribute__((const)) pair<float2, float2> dfdx_dfdy_gradient(const float2& p) {
	return { { dfdx(p.x), dfdx(p.y) }, { dfdy(p.x), dfdy(p.y) } };
}
// NOTE: not supported
floor_inline_always __attribute__((const)) pair<float3, float3> dfdx_dfdy_gradient(const float3& p) {
	return { { dfdx(p.x), dfdx(p.y), dfdx(p.z) }, { dfdy(p.x), dfdy(p.y), dfdy(p.z) } };
}

// NOTE: not supported
floor_inline_always __attribute__((const)) uint32_t get_patch_id() { return 0u; }
// NOTE: not supported
floor_inline_always __attribute__((const)) float3 get_position_in_patch() { return {}; }

#endif

#endif
