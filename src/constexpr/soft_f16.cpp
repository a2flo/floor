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

#include <floor/constexpr/soft_f16.hpp>

// just to make sure this is correct
static_assert(sizeof(fl::soft_f16) == 2, "invalid soft 16-bit half floating point type size");

// this must be ensured
static_assert(std::is_trivially_copyable_v<fl::soft_f16>, "soft 16-bit half floating point type must be trivially copyable");
static_assert(std::is_standard_layout_v<fl::soft_f16>);
