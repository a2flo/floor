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

namespace fl {

//! underlying type of indices in an index buffer
enum class INDEX_TYPE : uint32_t {
	//! 32-bit unsinged integer
	UINT = 0u,
	//! 16-bit unsinged short integer
	USHORT = 1u,
};

//! returns the size of the specified index type in bytes
floor_inline_always static constexpr uint32_t index_type_size(const INDEX_TYPE& index_type) {
	return (index_type == INDEX_TYPE::UINT ? 4u : 2u);
}

} // namespace fl
