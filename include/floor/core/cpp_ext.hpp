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

#pragma once

#include <bit>
#include <cstdint>

// NOTE: this header adds misc base C++ functionality that either will be part of a future C++ standard, or should be part of it

// string is not supported on compute
#if !defined(FLOOR_DEVICE) || (defined(FLOOR_DEVICE_HOST_COMPUTE) && !defined(FLOOR_DEVICE_HOST_COMPUTE_IS_DEVICE))
#include <string>
#include <limits>

namespace fl {

// for whatever reason there is no "string to 32-bit uint" conversion function in the standard
#if !defined(FLOOR_NO_STOU)
floor_inline_always static uint32_t stou(const std::string& str, std::size_t* pos = nullptr, int base = 10) {
	const auto ret = std::stoull(str, pos, base);
	if(ret > 0xFFFFFFFFull) {
		return std::numeric_limits<uint32_t>::max();
	}
	return (uint32_t)ret;
}
#endif
// same for size_t
#if !defined(FLOOR_NO_STOSIZE)
floor_inline_always static std::size_t stosize(const std::string& str, std::size_t* pos = nullptr, int base = 10) {
	return (std::size_t)std::stoull(str, pos, base);
}
#endif
// and for bool
#if !defined(FLOOR_NO_STOB)
floor_inline_always static bool stob(const std::string& str) {
	return (str == "1" || str == "true" || str == "TRUE" || str == "YES");
}
#endif

} // namespace fl

#endif // !FLOOR_DEVICE || FLOOR_DEVICE_HOST_COMPUTE

#if __clang_major__ < 17
//! static_assert that only triggers on instantiation, e.g. for use in "if constexpr" branches that may not be taken / should trigger an error
#define instantiation_trap(msg) static_assert([]() constexpr { return false; }(), msg)
//! similar to instantiation_trap, but in some cases we need it to be dependent so it doesn't always trigger
template <typename> constexpr bool instantiation_trap_dependent_type_helper = false;
#define instantiation_trap_dependent_type(dep_type, msg) static_assert(instantiation_trap_dependent_type_helper<dep_type>, msg)
template <auto> constexpr bool instantiation_trap_dependent_value_helper = false;
#define instantiation_trap_dependent_value(dep_value, msg) static_assert(instantiation_trap_dependent_value_helper<dep_value>, msg)
#else // as per CWG 2518 / P2593, it is now possible to directly fail the static_assert
#define instantiation_trap(msg) static_assert(false, msg)
#define instantiation_trap_dependent_type(dep_type, msg) static_assert(false, msg)
#define instantiation_trap_dependent_value(dep_value, msg) static_assert(false, msg)
#endif
