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

#include <floor/math/bbox.hpp>

namespace fl {

#if defined(FLOOR_EXPORT)
// instantiate
template class bbox<half3>;
template class bbox<float3>;
template class bbox<double3>;
template class bbox<ldouble3>;
#endif

static_assert(sizeof(bboxh) == sizeof(half3) * 2u);
static_assert(std::is_trivially_copyable_v<bboxh>);
static_assert(std::is_standard_layout_v<bboxh>);
static_assert(sizeof(bboxf) == sizeof(float3) * 2u);
static_assert(std::is_trivially_copyable_v<bboxf>);
static_assert(std::is_standard_layout_v<bboxf>);
static_assert(sizeof(bboxd) == sizeof(double3) * 2u);
static_assert(std::is_trivially_copyable_v<bboxd>);
static_assert(std::is_standard_layout_v<bboxd>);
static_assert(sizeof(bboxl) == sizeof(ldouble3) * 2u);
static_assert(std::is_trivially_copyable_v<bboxl>);
static_assert(std::is_standard_layout_v<bboxl>);

} // namespace fl
