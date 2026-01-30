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

#include <floor/math/vector_lib.hpp>

namespace fl {

class ray {
public:
	float3 origin;
	float3 direction;
	
	constexpr ray() noexcept = default;
	constexpr ray(const ray& r) noexcept = default;
	constexpr ray(const float3& rorigin, const float3& rdirection) noexcept : origin(rorigin), direction(rdirection) {}
	
	constexpr ray& operator=(const ray& r) noexcept {
		origin = r.origin;
		direction = r.direction;
		return *this;
	}
	
	constexpr float3 get_point(const float& distance) const {
		return origin + distance * direction;
	}
	
#if !defined(FLOOR_NO_MATH_STR)
	friend std::ostream& operator<<(std::ostream& output, const ray& r) {
		output << "(Origin: " << r.origin << ", Dir: " << r.direction << ")";
		return output;
	}
	std::string to_string() const {
		std::stringstream sstr;
		sstr << *this;
		return sstr.str();
	}
#endif
	
};

} // namespace fl
