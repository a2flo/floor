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

#include <floor/math/vector_lib.hpp>

namespace fl {

#if !defined(FLOOR_DEVICE) || (defined(FLOOR_DEVICE_HOST_COMPUTE) && !defined(FLOOR_DEVICE_HOST_COMPUTE_IS_DEVICE))
namespace vector_rng {
static std::random_device vec_rd {};
static std::mt19937 vec_gen { vec_rd() };

std::mt19937& get_vec_gen() {
	return vec_gen;
}
} // vector_rng
#endif

} // namespace fl
