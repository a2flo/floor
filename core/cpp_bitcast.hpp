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

#ifndef __FLOOR_CPP_BITCAST_HPP__
#define __FLOOR_CPP_BITCAST_HPP__

#include <bit>
#include <version>

// provide a simplistic std::bit_cast until a proper one is available everywhere with C++20
#if !defined(__cpp_lib_bit_cast) && __has_builtin(__builtin_bit_cast) && (!defined(_GLIBCXX_RELEASE) || _GLIBCXX_RELEASE < 11)
template <typename to_type, typename from_type>
requires(sizeof(to_type) == sizeof(from_type) &&
		 is_trivially_copyable_v<to_type> &&
		 is_trivially_copyable_v<from_type>)
static constexpr inline auto bit_cast(const from_type& from) noexcept {
	return __builtin_bit_cast(to_type, from);
}
#endif

#endif
