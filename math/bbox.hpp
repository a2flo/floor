/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2023 Florian Ziesche
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

#ifndef __FLOOR_BBOX_HPP__
#define __FLOOR_BBOX_HPP__

#include <floor/math/vector_lib.hpp>
#include <floor/math/ray.hpp>
#if !defined(FLOOR_NO_MATH_STR)
#include <ostream>
#include <sstream>
#include <string>
#endif

FLOOR_PUSH_WARNINGS()
FLOOR_IGNORE_WARNING(non-virtual-dtor)

class __attribute__((packed, aligned(4))) bbox {
public:
	float3 min { numeric_limits<float>::max() };
	float3 max { -numeric_limits<float>::max() };
	
	constexpr bbox() noexcept = default;
	constexpr bbox(const bbox& box) noexcept = default;
	constexpr bbox(const float3& bmin, const float3& bmax) noexcept : min(bmin), max(bmax) {}
	
	constexpr void extend(const float3& v) {
		min.min(v);
		max.max(v);
	}
	
	constexpr void extend(const bbox& box) {
		min.min(box.min);
		max.max(box.max);
	}
	
	constexpr float3 diagonal() const {
		return max - min;
	}
	
	constexpr float3 center() const {
		return (min + max) * 0.5f;
	}
	
	constexpr bbox& operator=(const bbox& box) {
		min = box.min;
		max = box.max;
		return *this;
	}
	
#if !defined(FLOOR_NO_MATH_STR)
	friend ostream& operator<<(ostream& output, const bbox& box) {
		output << "(Min: " << box.min << ", Max: " << box.max << ")";
		return output;
	}
	string to_string() const {
		stringstream sstr;
		sstr << *this;
		return sstr.str();
	}
#endif
	
	static constexpr const float2 invalid_intersection { numeric_limits<float>::max(), -numeric_limits<float>::max() };
	
	//! intersects the specified ray with this bbox, returning the { min, max } intersection distances
	//! how to interpret return values:
	//!  * no intersection if min >= max
	//!  * proper intersection if min < max && min >= 0
	//!  * self-intersection if min < max && min < 0 && this->contains(r.origin)
	constexpr float2 intersect(const ray& r) const {
		// http://www.cs.utah.edu/~awilliam/box/box.pdf
		// https://tavianator.com/fast-branchless-raybounding-box-intersections-part-2-nans
		const auto inv_dir = 1.0f / r.direction;
		const auto t1 = (min - r.origin) * inv_dir;
		const auto t2 = (max - r.origin) * inv_dir;
		
		auto tmin = ::min(t1.x, t2.x);
		auto tmax = ::max(t1.x, t2.x);
		
		tmin = ::max(tmin, ::min(::min(t1.y, t2.y), tmax));
		tmax = ::min(tmax, ::max(::max(t1.y, t2.y), tmin));
		
		tmin = ::max(tmin, ::min(::min(t1.z, t2.z), tmax));
		tmax = ::min(tmax, ::max(::max(t1.z, t2.z), tmin));
		
		return { tmin, tmax };
	}
	
	//! returns true if the ray properly intersects this bbox
	//! NOTE: self-intersection return false
	constexpr bool is_intersection(const ray& r) const {
		const auto ret = intersect(r);
		return (ret.x < ret.y && ret.x >= 0.0f);
	}
	
	constexpr bool contains(const float3& p) const {
		if(((p.x >= min.x && p.x <= max.x) || (p.x <= min.x && p.x >= max.x)) &&
		   ((p.y >= min.y && p.y <= max.y) || (p.y <= min.y && p.y >= max.y)) &&
		   ((p.z >= min.z && p.z <= max.z) || (p.z <= min.z && p.z >= max.z))) {
			return true;
		}
		return false;
	}

};

//! extended bound box (including position and model view matrix)
class __attribute__((packed)) extbbox : public bbox {
public:
	float3 pos;
	matrix4f mview;
	
	constexpr extbbox() noexcept = default;
	constexpr extbbox(const extbbox& ebox) noexcept = default;
	constexpr extbbox(const float3& bmin, const float3& bmax, const float3& bpos, const matrix4f& bmview) noexcept :
		bbox(bmin, bmax), pos(bpos), mview(bmview) {}
	
	constexpr extbbox& operator=(const extbbox& box) {
		min = box.min;
		max = box.max;
		pos = box.pos;
		mview = box.mview;
		return *this;
	}
	
	constexpr bool contains(const float3& p) const {
		float3 tp(p);
		tp -= pos;
		tp *= mview;
		return bbox::contains(tp);
	}
	
	constexpr void intersect(float2& ret, const ray& r) const {
		ray tr(r);
		tr.origin -= pos;
		tr.origin *= mview;
		tr.origin += pos;
		tr.direction *= mview;
		tr.direction.normalize();
		
		ret = bbox::intersect(tr);
	}
	
	constexpr bool is_intersection(const extbbox& box floor_unused) const {
		// TODO: implement this at some point:
		// http://en.wikipedia.org/wiki/Separating_axis_theorem
		// http://www.gamasutra.com/view/feature/3383/simple_intersection_tests_for_games.php?page=5
		return false;
	}
	
	constexpr bool is_intersection(const ray& r) const {
		ray tr(r);
		tr.origin -= pos;
		tr.origin *= mview;
		tr.origin += pos;
		tr.direction *= mview;
		tr.direction.normalize();
		
		return bbox::is_intersection(tr);
	}
	
#if !defined(FLOOR_NO_MATH_STR)
	friend ostream& operator<<(ostream& output, const extbbox& box) {
		output << "(Min: " << box.min << ", Max: " << box.max;
		output << ", Pos: " << box.pos << ")" << endl;
		output << box.mview << endl;
		return output;
	}
	string to_string() const {
		stringstream sstr;
		sstr << *this;
		return sstr.str();
	}
#endif
	
};

FLOOR_POP_WARNINGS()

#endif

