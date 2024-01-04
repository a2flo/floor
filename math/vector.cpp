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

#include <floor/math/vector_lib.hpp>

#if !defined(FLOOR_COMPUTE) || (defined(FLOOR_COMPUTE_HOST) && !defined(FLOOR_COMPUTE_HOST_DEVICE))
namespace floor_vector_rand {
static random_device vec_rd {};
static mt19937 vec_gen { vec_rd() };

mt19937& get_vec_gen() {
	return vec_gen;
}
} // floor_vector_rand
#endif
