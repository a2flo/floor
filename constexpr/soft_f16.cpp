/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2016 Florian Ziesche
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
static_assert(sizeof(soft_f16) == 2, "invalid soft 16-bit half floating point type size");

// need linkage for constexpr tables (if < c++17)
#if !defined(FLOOR_CXX17) && FLOOR_HAS_NATIVE_FP16 == 0
alignas(128) const uint32_t soft_f16::htof_mantissa_table[1024];
alignas(128) const uint32_t soft_f16::htof_exponent_table[64];
alignas(128) const uint16_t soft_f16::ftoh_base_table[512];
alignas(128) const uint8_t soft_f16::ftoh_shift_table[512];
#endif
