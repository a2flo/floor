/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2021 Florian Ziesche
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

#include <floor/math/vector_lib.hpp>

// sizeof check
#define FLOOR_VECTOR_SIZEOF_CHECK(pod_type, prefix, vec_width) \
static_assert(sizeof(prefix##vec_width) == sizeof(pod_type) * vec_width, "invalid vector size");

FLOOR_VECTOR_TYPES_F(FLOOR_VECTOR_SIZEOF_CHECK, 1)
FLOOR_VECTOR_TYPES_F(FLOOR_VECTOR_SIZEOF_CHECK, 2)
FLOOR_VECTOR_TYPES_F(FLOOR_VECTOR_SIZEOF_CHECK, 3)
FLOOR_VECTOR_TYPES_F(FLOOR_VECTOR_SIZEOF_CHECK, 4)
