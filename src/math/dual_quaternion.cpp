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

#include <floor/math/dual_quaternion.hpp>

namespace fl {

#if defined(FLOOR_EXPORT)
// instantiate
template class dual_quaternion<float>;
template class dual_quaternion<double>;
template class dual_quaternion<long double>;
#endif

static_assert(std::is_trivially_copyable_v<dual_quaternionf>);
static_assert(std::is_standard_layout_v<dual_quaternionf>);
static_assert(std::is_trivially_copyable_v<dual_quaterniond>);
static_assert(std::is_standard_layout_v<dual_quaterniond>);
static_assert(std::is_trivially_copyable_v<dual_quaternionl>);
static_assert(std::is_standard_layout_v<dual_quaternionl>);

} // namespace fl
