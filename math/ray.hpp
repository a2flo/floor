/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2015 Florian Ziesche
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

#ifndef __FLOOR_RAY_HPP__
#define __FLOOR_RAY_HPP__

#include <floor/math/vector_lib.hpp>

class ray {
public:
	float3 origin;
	float3 direction;
	
	constexpr ray() noexcept : origin(), direction() {}
	constexpr ray(const ray& r) noexcept : origin(r.origin), direction(r.direction) {}
	constexpr ray(const float3& rorigin, const float3& rdirection) noexcept : origin(rorigin), direction(rdirection) {}
	
	float3 get_point(const float& distance) {
		return origin + distance * direction;
	}
	
};

#endif
