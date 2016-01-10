/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2016 Florian Ziesche
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

#ifndef __FLOOR_COMPUTE_DEVICE_METAL_POST_HPP__
#define __FLOOR_COMPUTE_DEVICE_METAL_POST_HPP__

#if defined(FLOOR_COMPUTE_METAL)

// NOTE: very wip
// TODO: frag coord/position?

//////////////////////////////////////////
// vertex shader
// returns the vertex id inside a vertex shader
const_func uint32_t get_vertex_id() asm("floor.get_vertex_id.i32");
// returns the instance id inside a vertex shader
const_func uint32_t get_instance_id() asm("floor.get_instance_id.i32");

#define vertex_id get_vertex_id()
#define instance_id get_instance_id()

//////////////////////////////////////////
// fragment shader
//! returns the normalized (in [0, 1]) point coordinate (clang_float2 version)
const_func clang_float2 get_point_coord_cf2() asm("floor.get_point_coord.float2");
//! returns the normalized (in [0, 1]) point coordinate
const_func float2 get_point_coord() { return float2::from_clang_vector(get_point_coord_cf2()); }
//! discards the current fragment
void discard_fragment() asm("air.discard_fragment");

#endif

#endif
