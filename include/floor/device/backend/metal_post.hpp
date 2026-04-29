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

#if defined(FLOOR_DEVICE_METAL)

namespace fl {

//////////////////////////////////////////
// general

const_func uint32_t pack_snorm_4x8_clang(clang_float4) asm("air.pack.snorm4x8.v4f32");
const_func uint32_t pack_unorm_4x8_clang(clang_float4) asm("air.pack.unorm4x8.v4f32");
const_func uint32_t pack_snorm_2x16_clang(clang_float2) asm("air.pack.snorm2x16.v2f32");
const_func uint32_t pack_unorm_2x16_clang(clang_float2) asm("air.pack.unorm2x16.v2f32");
const_func uint32_t pack_half_2x16_clang(clang_half2) asm("air.pack.snorm2x16.v2f16");
const_func clang_float4 unpack_snorm_4x8_clang(uint32_t) asm("air.unpack.snorm4x8.v4f32");
const_func clang_float4 unpack_unorm_4x8_clang(uint32_t) asm("air.unpack.unorm4x8.v4f32");
const_func clang_float2 unpack_snorm_2x16_clang(uint32_t) asm("air.unpack.snorm2x16.v2f32");
const_func clang_float2 unpack_unorm_2x16_clang(uint32_t) asm("air.unpack.unorm2x16.v2f32");
const_func clang_half2 unpack_half_2x16_clang(uint32_t) asm("air.unpack.snorm2x16.v2f16");
// unavailable: pack_double_2x32, unpack_double_2x32

//! clamps the input vector to [-1, 1], then converts and scales each component to an 8-bit signed integer in [-127, 127],
//! returning a packed 32-bit unsigned integer, with vector components packed in ascending order from LSB to MSB
//! -> [comp-3][comp-2][comp-1][comp-0]
const_func uint32_t pack_snorm_4x8(const float4& vec) {
	return pack_snorm_4x8_clang(vec.to_clang_vector());
}

//! clamps the input vector to [0, 1], then converts and scales each component to an 8-bit unsigned integer in [0, 255],
//! returning a packed 32-bit unsigned integer, with vector components packed in ascending order from LSB to MSB
//! -> [comp-3][comp-2][comp-1][comp-0]
const_func uint32_t pack_unorm_4x8(const float4& vec) {
	return pack_unorm_4x8_clang(vec.to_clang_vector());
}

//! clamps the input vector to [-1, 1], then converts and scales each component to an 16-bit signed integer in [-32767, 32767],
//! returning a packed 32-bit unsigned integer, with vector components packed in ascending order from LSB to MSB
//! -> [comp-1][comp-0]
const_func uint32_t pack_snorm_2x16(const float2& vec) {
	return pack_snorm_2x16_clang(vec.to_clang_vector());
}

//! clamps the input vector to [0, 1], then converts and scales each component to an 16-bit unsigned integer in [0, 65535],
//! returning a packed 32-bit unsigned integer, with vector components packed in ascending order from LSB to MSB
//! -> [comp-1][comp-0]
const_func uint32_t pack_unorm_2x16(const float2& vec) {
	return pack_unorm_2x16_clang(vec.to_clang_vector());
}

//! converts the input 32-bit single-precision float vector to a 16-bit half-precision float vector,
//! returning a packed 32-bit unsigned integer, with vector components packed in ascending order from LSB to MSB
//! -> [comp-1][comp-0]
const_func uint32_t pack_half_2x16(const float2& vec) {
	return pack_half_2x16_clang(half2(vec.x, vec.y).to_clang_vector());
}

//! unpacks the input 32-bit unsigned integer into 4 8-bit signed integers, then converts these [-127, 127]-ranged integers
//! to normalized 32-bit single-precision float values in [-1, 1], returning them in a 4 component vector
const_func float4 unpack_snorm_4x8(const uint32_t& val) {
	return float4::from_clang_vector(unpack_snorm_4x8_clang(val));
}

//! unpacks the input 32-bit unsigned integer into 4 8-bit unsigned integers, then converts these [0, 255]-ranged integers
//! to normalized 32-bit single-precision float values in [0, 1], returning them in a 4 component vector
const_func float4 unpack_unorm_4x8(const uint32_t& val) {
	return float4::from_clang_vector(unpack_unorm_4x8_clang(val));
}

//! unpacks the input 32-bit unsigned integer into 2 16-bit signed integers, then converts these [-32767, 32767]-ranged integers
//! to normalized 32-bit single-precision float values in [-1, 1], returning them in a 2 component vector
const_func float2 unpack_snorm_2x16(const uint32_t& val) {
	return float2::from_clang_vector(unpack_snorm_2x16_clang(val));
}

//! unpacks the input 32-bit unsigned integer into 2 16-bit unsigned integers, then converts these [0, 65535]-ranged integers
//! to normalized 32-bit single-precision float values in [0, 1], returning them in a 2 component vector
const_func float2 unpack_unorm_2x16(const uint32_t& val) {
	return float2::from_clang_vector(unpack_unorm_2x16_clang(val));
}

//! unpacks the input 32-bit unsigned integer into 2 16-bit half-precision float values, then converts these values
//! to 32-bit single-precision float values, returning them in a 2 component vector
const_func float2 unpack_half_2x16(const uint32_t& val) {
	return (float2)half2::from_clang_vector(unpack_half_2x16_clang(val));
}

//////////////////////////////////////////
// any shader
//! returns the view index inside a shader
//! TODO: implement this
const_func uint32_t get_view_index() {
	//asm("floor.get_view_index.i32");
	return 0;
}

//////////////////////////////////////////
// vertex shader
//! returns the vertex id inside a vertex shader
const_func uint32_t get_vertex_id() asm("floor.get_vertex_id.i32");
//! returns the base/first vertex id inside a vertex shader
const_func uint32_t get_base_vertex_id() asm("floor.get_base_vertex_id.i32");
//! returns the instance id inside a vertex shader or tessellation evaluation shader
const_func uint32_t get_instance_id() asm("floor.get_instance_id.i32");
//! returns the base/first instance id inside a vertex shader or tessellation evaluation shader
const_func uint32_t get_base_instance_id() asm("floor.get_base_instance_id.i32");

//////////////////////////////////////////
// fragment shader
//! returns the normalized (in [0, 1]) point coordinate (clang_float2 version)
const_func clang_float2 get_point_coord_cf2() asm("floor.get_point_coord.float2");
//! returns the normalized (in [0, 1]) point coordinate
floor_inline_always const_func float2 get_point_coord() { return float2::from_clang_vector(get_point_coord_cf2()); }

//! returns the primitive id inside a fragment shader
const_func uint32_t get_primitive_id() asm("floor.get_primitive_id.i32");

#if defined(FLOOR_DEVICE_INFO_HAS_BARYCENTRIC_COORD_1)
const_func clang_float3 get_barycentric_coord_cf3() asm("floor.get_barycentric_coord.float3");
//! returns the barycentric coordinate inside a fragment shader
floor_inline_always const_func float3 get_barycentric_coord() { return float3::from_clang_vector(get_barycentric_coord_cf3()); }
#else
const_func clang_float3 get_barycentric_coord_cf3() __attribute__((unavailable("not supported on this target")));
floor_inline_always const_func float3 get_barycentric_coord() __attribute__((unavailable("not supported on this target")));
#endif

//! discards the current fragment
void discard_fragment() __attribute__((noreturn)) asm("air.discard_fragment");

//! partial derivative of p with respect to the screen-space x coordinate
const_func float dfdx(float p) asm("air.dfdx.f32");
//! partial derivative of p with respect to the screen-space y coordinate
const_func float dfdy(float p) asm("air.dfdy.f32");
//! returns "abs(dfdx(p)) + abs(dfdy(p))"
const_func float fwidth(float p) asm("air.fwidth.f32");
//! computes the partial deriviate of p with respect to the screen-space (x, y) coordinate
floor_inline_always const_func std::pair<float, float> dfdx_dfdy_gradient(const float& p) {
	return { { dfdx(p) }, { dfdy(p) } };
}
//! computes the partial deriviate of p with respect to the screen-space (x, y) coordinate
floor_inline_always const_func std::pair<float2, float2> dfdx_dfdy_gradient(const float2& p) {
	return { { dfdx(p.x), dfdx(p.y) }, { dfdy(p.x), dfdy(p.y) } };
}
//! computes the partial deriviate of p with respect to the screen-space (x, y) coordinate
floor_inline_always const_func std::pair<float3, float3> dfdx_dfdy_gradient(const float3& p) {
	return { { dfdx(p.x), dfdx(p.y), dfdx(p.z) }, { dfdy(p.x), dfdy(p.y), dfdy(p.z) } };
}

//////////////////////////////////////////
// tessellation evaluation shader

//! returns the patch id inside a tessellation evaluation shader
const_func uint32_t get_patch_id() asm("floor.get_patch_id.i32");

//! returns the position within a patch inside a tessellation evaluation shader (clang vector variant)
const_func clang_float3 get_position_in_patch_cf3() asm("floor.get_position_in_patch.float3");
//! returns the position within a patch inside a tessellation evaluation shader
floor_inline_always const_func float3 get_position_in_patch() { return float3::from_clang_vector(get_position_in_patch_cf3()); }

// NOTE: for tessellation evaluation shader instance_id, see vertex shader

//////////////////////////////////////////
// task/mesh shader

#if FLOOR_DEVICE_INFO_MESH_SHADING_SUPPORT

metal_func void metal_mesh_set_primitive_count(const __mesh_t, uint32_t)
__attribute__((noduplicate)) asm("air.set_primitive_count_mesh");

metal_func void metal_mesh_set_index(const __mesh_t, uint32_t, uint8_t) asm("air.set_index_mesh");
metal_func void metal_mesh_set_indices(const __mesh_t, uint32_t, clang_uchar2) asm("air.set_indices_mesh.v2i8");
metal_func void metal_mesh_set_indices(const __mesh_t, uint32_t, clang_uchar4) asm("air.set_indices_mesh.v4i8");

metal_func void metal_mesh_grid_properties_set_threadgroups(const __mesh_grid_properties_t, clang_uint3)
__attribute__((noduplicate)) asm("air.set_threadgroups_per_grid_mesh_properties");

template <typename vertex_type, typename primitive_type,
		  uint32_t max_vertex_count, uint32_t max_primitive_count,
		  MESH_TOPOLOGY topology = MESH_TOPOLOGY::TRIANGLE>
requires (__libfloor_is_valid_mesh_vertex_type(vertex_type) && __libfloor_is_valid_mesh_primitive_type(primitive_type))
struct mesh {
	static constexpr const bool is_void_primitive = std::is_same_v<primitive_type, void>;
	
	//! mesh output type / fragment stage input type
	using output_type = mesh_output_type<vertex_type, primitive_type>;
	
	//! sets the actual output sizes (primitive count and vertex count),
	//! the vertex count defaults to the max specified vertex count of the mesh (max_vertex_count)
	void set_output_size(const uint32_t primitive_count, [[maybe_unused]] const uint32_t vertex_count = ~0u) const {
		metal_mesh_set_primitive_count(mesh, primitive_count);
	}
	
	//! sets the single "index" of a point for the primitive at index "primitive_idx"
	void set_index(const uint32_t primitive_idx, const uint8_t index) const requires(topology == MESH_TOPOLOGY::POINT) {
		metal_mesh_set_index(mesh, primitive_idx, index);
	}
	
	//! sets the two "indices" of a line for the primitive at index "primitive_idx"
	void set_indices(const uint32_t primitive_idx, const uchar2 indices) const requires(topology == MESH_TOPOLOGY::LINE) {
		metal_mesh_set_indices(mesh, primitive_idx * 2u, indices.to_clang_vector());
	}
	
	//! sets the three "indices" of a triangle for the primitive at index "primitive_idx"
	void set_indices(const uint32_t primitive_idx, const uchar3 indices) const requires(topology == MESH_TOPOLOGY::TRIANGLE) {
		metal_mesh_set_index(mesh, primitive_idx * 3u, indices.x);
		metal_mesh_set_index(mesh, primitive_idx * 3u + 1u, indices.y);
		metal_mesh_set_index(mesh, primitive_idx * 3u + 2u, indices.z);
	}
	
	void set_vertex(const uint32_t output_position, vertex_type vert) const {
		__libfloor_mesh_set_vertex(mesh, output_position, vert);
	}
	
	void set_primitive(const uint32_t output_position,
					   std::conditional_t<!is_void_primitive, primitive_type, int> prim) const requires(!is_void_primitive) {
		__libfloor_mesh_set_primitive(mesh, output_position, prim);
	}
	
protected:
	__mesh_t mesh;
	
	//! sets a single "index" at the specified "output_position"
	void set_index_internal(const uint32_t output_position, const uint8_t index) const {
		metal_mesh_set_index(mesh, output_position, index);
	}
	
	//! sets two consecutive "indices" at the specified "output_position"
	void set_indices_internal(const uint32_t output_position, const uchar2 indices) const {
		metal_mesh_set_indices(mesh, output_position, indices.to_clang_vector());
	}
	
	//! sets four consecutive "indices" at the specified "output_position"
	void set_indices_internal(const uint32_t output_position, const uchar4 indices) const {
		metal_mesh_set_indices(mesh, output_position, indices.to_clang_vector());
	}
};

struct mesh_grid_properties {
	void emit_tasks(const uint3 work_groups) const {
		metal_mesh_grid_properties_set_threadgroups(props, work_groups.to_clang_vector());
	}
	
protected:
	__mesh_grid_properties_t props;
};

#endif

} // namespace fl

#endif
