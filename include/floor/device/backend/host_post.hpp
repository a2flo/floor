/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2026 Florian Ziesche
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

#if defined(FLOOR_GRAPHICS_HOST_COMPUTE)

#include <cassert>

namespace fl {

// NOTE: not supported
floor_inline_always __attribute__((const)) static uint32_t get_vertex_id() {
	return 0;
}
// NOTE: not supported
floor_inline_always __attribute__((const)) static uint32_t get_base_vertex_id() {
	return 0;
}
// NOTE: not supported
floor_inline_always __attribute__((const)) static uint32_t get_instance_id() {
	return 0;
}
// NOTE: not supported
floor_inline_always __attribute__((const)) static uint32_t get_base_instance_id() {
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
floor_inline_always __attribute__((const)) std::pair<float, float> dfdx_dfdy_gradient(const float& p) {
	return { { dfdx(p) }, { dfdy(p) } };
}
// NOTE: not supported
floor_inline_always __attribute__((const)) std::pair<float2, float2> dfdx_dfdy_gradient(const float2& p) {
	return { { dfdx(p.x), dfdx(p.y) }, { dfdy(p.x), dfdy(p.y) } };
}
// NOTE: not supported
floor_inline_always __attribute__((const)) std::pair<float3, float3> dfdx_dfdy_gradient(const float3& p) {
	return { { dfdx(p.x), dfdx(p.y), dfdx(p.z) }, { dfdy(p.x), dfdy(p.y), dfdy(p.z) } };
}

// NOTE: not supported
floor_inline_always __attribute__((const)) uint32_t get_patch_id() { return 0u; }
// NOTE: not supported
floor_inline_always __attribute__((const)) float3 get_position_in_patch() { return {}; }

//////////////////////////////////////////
// task/mesh shader

#if FLOOR_DEVICE_INFO_MESH_SHADING_SUPPORT

template <typename vertex_type, typename primitive_type,
		  uint32_t max_vertex_count_, uint32_t max_primitive_count_,
		  MESH_TOPOLOGY topology = MESH_TOPOLOGY::TRIANGLE>
#if defined(FLOOR_DEVICE_HOST_COMPUTE_IS_DEVICE)
requires (__libfloor_is_valid_mesh_vertex_type(vertex_type) && __libfloor_is_valid_mesh_primitive_type(primitive_type))
#endif
struct mesh {
	static constexpr const bool is_void_primitive = std::is_same_v<primitive_type, void>;
	static constexpr const uint32_t max_vertex_count { max_vertex_count_ };
	static constexpr const uint32_t max_primitive_count { max_primitive_count_ };
	static constexpr const uint32_t indices_per_primitive {
		topology == MESH_TOPOLOGY::TRIANGLE ? 3u : (topology == MESH_TOPOLOGY::LINE ? 2u : 1u)
	};
	static constexpr const uint32_t max_indices { max_primitive_count * indices_per_primitive };
	
	//! mesh output type / fragment stage input type
	using output_type = mesh_output_type<vertex_type, primitive_type>;
	
	//! sets the actual output sizes (primitive count and vertex count),
	//! the vertex count defaults to the max specified vertex count of the mesh (max_vertex_count)
	void set_output_size([[maybe_unused]] const uint32_t primitive_count) const {
		assert(primitive_count <= max_primitive_count);
	}
	void set_output_size([[maybe_unused]] const uint32_t primitive_count, [[maybe_unused]] const uint32_t vertex_count) const {
		assert(primitive_count <= max_primitive_count);
		assert(vertex_count == ~0u || vertex_count <= max_primitive_count);
	}
	
	//! sets the single "index" of a point for the primitive at index "primitive_idx"
	void set_index([[maybe_unused]] const uint32_t primitive_idx,
				   [[maybe_unused]] const uint8_t index) const requires(topology == MESH_TOPOLOGY::POINT) {
		// nop
	}
	
	//! sets the two "indices" of a line for the primitive at index "primitive_idx"
	void set_indices([[maybe_unused]] const uint32_t primitive_idx,
					 [[maybe_unused]] const uchar2 indices) const requires(topology == MESH_TOPOLOGY::LINE) {
		// nop
	}
	
	//! sets the three "indices" of a triangle for the primitive at index "primitive_idx"
	void set_indices([[maybe_unused]] const uint32_t primitive_idx,
					 [[maybe_unused]] const uchar3 indices) const requires(topology == MESH_TOPOLOGY::TRIANGLE) {
		// nop
	}
	
	void set_vertex([[maybe_unused]] const uint32_t output_position,
					[[maybe_unused]] vertex_type vert) const {
		// nop
	}
	
	void set_primitive([[maybe_unused]] const uint32_t output_position,
					   [[maybe_unused]] std::conditional_t<!is_void_primitive, primitive_type, int> prim) const requires(!is_void_primitive) {
		// nop
	}
	
protected:
#if defined(FLOOR_DEVICE_HOST_COMPUTE_IS_DEVICE)
	__mesh_t mesh;
#else
	void* mesh { nullptr };
#endif
	
};

struct mesh_grid_properties {
	void emit_tasks([[maybe_unused]] const uint3 work_groups) const {
		// nop
	}
	
protected:
#if defined(FLOOR_DEVICE_HOST_COMPUTE_IS_DEVICE)
	__mesh_grid_properties_t props;
#else
	void* props { nullptr };
#endif
};

#endif

} // namespace fl

#endif
