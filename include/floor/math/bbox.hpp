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

#include <floor/math/vector_lib.hpp>
#include <floor/math/ray.hpp>
#if !defined(FLOOR_NO_MATH_STR)
#include <ostream>
#include <sstream>
#include <string>
#endif

namespace fl {

//! axis-aligned bounding box
template <typename bbox_vector_type = float3>
class bbox {
public:
	using vector_type = bbox_vector_type;
	using scalar_type = typename vector_type::decayed_scalar_type;
	using intersection_type = vector_n<scalar_type, 2>;
	
	vector_type min { ext::limits<scalar_type>::max };
	vector_type max { ext::limits<scalar_type>::lowest };
	
	//! default construct with invalid extent
	constexpr bbox() noexcept = default;
	//! default copy-construct from same bbox
	constexpr bbox(const bbox& box) noexcept = default;
	//! default move-construct from same bbox
	constexpr bbox(bbox&& box) noexcept = default;
	constexpr bbox(const vector_type bmin, const vector_type bmax) noexcept : min(bmin), max(bmax) {}
	
	//! default copy assignment
	constexpr bbox& operator=(const bbox& vec) noexcept = default;
	
	//! default move assignment
	constexpr bbox& operator=(bbox&& vec) noexcept = default;
	
	constexpr bbox& extend(const vector_type v) {
		min.min(v);
		max.max(v);
		return *this;
	}
	constexpr bbox extended(const vector_type v) const {
		return bbox(*this).extend(v);
	}
	
	constexpr bbox& extend(const bbox& box) {
		min.min(box.min);
		max.max(box.max);
		return *this;
	}
	constexpr bbox extended(const bbox& box) const {
		return bbox(*this).extend(box);
	}
	
	constexpr vector_type diagonal() const {
		return max - min;
	}
	
	constexpr vector_type center() const {
		return (min + max) * scalar_type(0.5);
	}
	
#if !defined(FLOOR_NO_MATH_STR)
	friend std::ostream& operator<<(std::ostream& output, const bbox& box) {
		output << "(min: " << box.min << ", max: " << box.max << ")";
		return output;
	}
	std::string to_string() const {
		std::stringstream sstr;
		sstr << *this;
		return sstr.str();
	}
#endif
	
	static constexpr const intersection_type invalid_intersection {
		std::numeric_limits<scalar_type>::max(), -std::numeric_limits<scalar_type>::max()
	};
	
	//! intersects the specified ray with this bbox, returning the { min, max } intersection distances
	//! how to interpret return values:
	//!  * no intersection if min >= max
	//!  * proper intersection if min < max && min >= 0
	//!  * self-intersection if min < max && min < 0 && this->contains(r.origin)
	constexpr intersection_type intersect(const ray r) const {
		// http://www.cs.utah.edu/~awilliam/box/box.pdf
		// https://tavianator.com/fast-branchless-raybounding-box-intersections-part-2-nans
		const auto inv_dir = (decltype(ray::direction)::decayed_scalar_type(1.0) / r.direction).cast<scalar_type>();
		const auto t1 = (min - r.origin) * inv_dir;
		const auto t2 = (max - r.origin) * inv_dir;
		
		auto tmin = math::min(t1.x, t2.x);
		auto tmax = math::max(t1.x, t2.x);
		
		tmin = math::max(tmin, math::min(std::min(t1.y, t2.y), tmax));
		tmax = math::min(tmax, math::max(std::max(t1.y, t2.y), tmin));
		
		tmin = math::max(tmin, math::min(std::min(t1.z, t2.z), tmax));
		tmax = math::min(tmax, math::max(std::max(t1.z, t2.z), tmin));
		
		return { tmin, tmax };
	}
	
	//! returns true if the ray properly intersects this bbox
	//! NOTE: self-intersection return false
	constexpr bool is_intersection(const ray r) const {
		const auto ret = intersect(r);
		return (ret.x < ret.y && ret.x >= scalar_type(0.0));
	}
	
	constexpr bool contains(const vector_type p) const {
		if (((p.x >= min.x && p.x <= max.x) || (p.x <= min.x && p.x >= max.x)) &&
			((p.y >= min.y && p.y <= max.y) || (p.y <= min.y && p.y >= max.y)) &&
			((p.z >= min.z && p.z <= max.z) || (p.z <= min.z && p.z >= max.z))) {
			return true;
		}
		return false;
	}
	
};

using bboxh = bbox<half3>;
using bboxf = bbox<float3>;
#if !defined(FLOOR_DEVICE_NO_DOUBLE) // disable double + long double if specified
using bboxd = bbox<double3>;
// always disable long double on device platforms
#if !defined(FLOOR_DEVICE) || (defined(FLOOR_DEVICE_HOST_COMPUTE) && !defined(FLOOR_DEVICE_HOST_COMPUTE_IS_DEVICE))
using bboxl = bbox<ldouble3>;
#endif
#endif

#if defined(FLOOR_EXPORT)
// only instantiate this in the bbox.cpp
extern template class bbox<half3>;
extern template class bbox<float3>;
#if !defined(FLOOR_DEVICE_NO_DOUBLE)
extern template class bbox<double3>;
extern template class bbox<ldouble3>;
#endif
#endif

//! extended bounding box (including position and model view matrix)
class __attribute__((packed)) extbbox : public bbox<float3> {
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
	friend std::ostream& operator<<(std::ostream& output, const extbbox& box) {
		output << "(Min: " << box.min << ", Max: " << box.max;
		output << ", Pos: " << box.pos << ")" << std::endl;
		output << box.mview << std::endl;
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
