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

#if FLOOR_DEVICE_INFO_MESH_SHADING_SUPPORT

// when compiling Host-Compute code with a vanilla toolchain, libfloor attributes are of course not available
FLOOR_PUSH_AND_IGNORE_WARNING(unknown-attributes)

namespace fl {

//! mesh shading primitive topology
enum class MESH_TOPOLOGY : uint32_t {
	POINT = 0u,
	LINE = 1u,
	TRIANGLE = 2u,
};

//! mesh output type / fragment stage input type, used within mesh<> class implementations
template <typename vertex_type, typename primitive_type> struct mesh_output_type;
template <typename vertex_type, typename primitive_type> requires (!std::is_same_v<primitive_type, void>)
struct mesh_output_type<vertex_type, primitive_type> {
	//! per-vertex data
	vertex_type vtx;
	//! per-primitive data
	//! NOTE: per_primitive attribute is required for Vulkan only, signaling the wanted per-primitive interpolation
	[[per_primitive]] primitive_type prim;
};
template <typename vertex_type, typename primitive_type> requires (std::is_same_v<primitive_type, void>)
struct mesh_output_type<vertex_type, primitive_type> {
	//! per-vertex data
	vertex_type vtx;
};

} // namespace fl

FLOOR_POP_WARNINGS()

#endif
