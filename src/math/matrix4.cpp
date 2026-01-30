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

#include <floor/math/matrix4.hpp>
#include <floor/math/vector_lib.hpp>

namespace fl {

#if defined(FLOOR_EXPORT)
// instantiate
template class matrix4<float>;
template class matrix4<double>;
template class matrix4<long double>;
template class matrix4<int32_t>;
template class matrix4<uint32_t>;
#endif

static_assert(std::is_trivially_copyable_v<matrix4f>);
static_assert(std::is_standard_layout_v<matrix4f>);
static_assert(std::is_trivially_copyable_v<matrix4d>);
static_assert(std::is_standard_layout_v<matrix4d>);
static_assert(std::is_trivially_copyable_v<matrix4l>);
static_assert(std::is_standard_layout_v<matrix4l>);
static_assert(std::is_trivially_copyable_v<matrix4i>);
static_assert(std::is_standard_layout_v<matrix4i>);
static_assert(std::is_trivially_copyable_v<matrix4ui>);
static_assert(std::is_standard_layout_v<matrix4ui>);

} // namespace fl
