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

#include <floor/math/quaternion.hpp>

namespace fl {

#if defined(FLOOR_EXPORT)
// instantiate
template class quaternion<float>;
template class quaternion<double>;
template class quaternion<long double>;
#endif

static_assert(std::is_trivially_copyable_v<quaternionf>);
static_assert(std::is_standard_layout_v<quaternionf>);
static_assert(std::is_trivially_copyable_v<quaterniond>);
static_assert(std::is_standard_layout_v<quaterniond>);
static_assert(std::is_trivially_copyable_v<quaternionl>);
static_assert(std::is_standard_layout_v<quaternionl>);

} // namespace fl
