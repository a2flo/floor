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

#ifndef __FLOOR_DUAL_QUATERNION_HPP__
#define __FLOOR_DUAL_QUATERNION_HPP__

#include <floor/math/vector_lib.hpp>
#include <floor/math/quaternion.hpp>
#if !defined(FLOOR_NO_MATH_STR)
#include <ostream>
#include <sstream>
#include <string>
#endif

template <typename scalar_type> class dual_quaternion {
public:
	//! this scalar type
	typedef scalar_type this_scalar_type;
	//! "this" dual quaternion type
	typedef dual_quaternion<scalar_type> dual_quaternion_type;
	//! contained quaternion type
	typedef quaternion<scalar_type> quaternion_type;
	
	//! rotational part
	quaternion_type rq { scalar_type(0), scalar_type(0), scalar_type(0), scalar_type(1) };
	//! dual/translational part
	quaternion_type dq { scalar_type(0), scalar_type(0), scalar_type(0), scalar_type(0) };
	
	//////////////////////////////////////////
	// constructors and assignment operators
#pragma mark constructors and assignment operators
	
	//! constructs an identity dual quaternion
	constexpr dual_quaternion() noexcept = default;
	
	//! constructs a dual quaternion from its corresponding rotational and translational parts
	constexpr dual_quaternion(const quaternion<scalar_type>& rq_, const quaternion<scalar_type>& dq_) noexcept : rq(rq_), dq(dq_) {}
	
	//! constructs a dual quaternion from its corresponding vector4 representations
	constexpr dual_quaternion(const vector4<scalar_type>& rq_v4,
							  const vector4<scalar_type>& dq_v4) noexcept : rq(rq_v4), dq(dq_v4) {}
	
	//! constructs a dual quaternion with the rotational components set to (rx, ry, rz, r) and translational components to (dx, dy, dz)
	constexpr dual_quaternion(const scalar_type& rx, const scalar_type& ry, const scalar_type& rz, const scalar_type& r,
							  const scalar_type& dx, const scalar_type& dy, const scalar_type& dz) noexcept :
	rq(rx, ry, rz, r), dq(dx, dy, dz, scalar_type(0)) {}
	
	//! copy-construct from same dual quaternion type
	constexpr dual_quaternion(const dual_quaternion& q_) noexcept : rq(q_.rq), dq(q_.dq) {}
	
	//! copy-construct from same dual quaternion type (rvalue)
	constexpr dual_quaternion(dual_quaternion&& q_) noexcept : rq(q_.rq), dq(q_.dq) {}
	
	//! copy-construct from same dual quaternion type (pointer)
	constexpr dual_quaternion(const dual_quaternion* q_) noexcept : rq(q_->rq), dq(q_->dq) {}
	
	//! copy assignment
	constexpr dual_quaternion& operator=(const dual_quaternion& q_) {
		rq = q_.rq;
		dq = q_.dq;
		return *this;
	}
	
	//////////////////////////////////////////
	// I/O
#pragma mark I/O

#if !defined(FLOOR_NO_MATH_STR)
	//! ostream output of this dual quaternion
	friend ostream& operator<<(ostream& output, const dual_quaternion& q) {
		output << "(rq: " << q.rq << ", dq: " << q.dq << ")";
		return output;
	}
	
	//! returns a string representation of this dual quaternion
	//!  * if !as_readable: returns the plain rq and dq quaternions as a string (same as operator<<)
	//!  * if as_readable: returns a more human-readable (rotation-angle: 3D rotatation-axis, 3D position) string
	string to_string(const bool as_readable = false) const {
		stringstream sstr;
		if (!as_readable) {
			sstr << *this;
		} else {
			sstr << "(" << rq.rotation_angle_deg() << "°: " << rq.rotation_axis() << ", @" << to_position() << ")";
		}
		return sstr.str();
	}
#endif
	
	//////////////////////////////////////////
	// basic ops
#pragma mark basic ops
	
	//! adds this dual quaternion to 'q' and returns the result
	constexpr dual_quaternion operator+(const dual_quaternion& q) const {
		return { rq + q.rq, dq + q.dq };
	}
	
	//! adds this dual quaternion to 'q' and sets this dual quaternion to the result
	constexpr dual_quaternion& operator+=(const dual_quaternion& q) {
		rq += q.rq;
		dq += q.dq;
		return *this;
	}
	
	//! subtracts 'q' from this dual quaternion and returns the result
	constexpr dual_quaternion operator-(const dual_quaternion& q) const {
		return { rq - q.rq, dq - q.dq };
	}
	
	//! subtracts 'q' from this dual quaternion and sets this dual quaternion to the result
	constexpr dual_quaternion& operator-=(const dual_quaternion& q) {
		rq -= q.rq;
		dq -= q.dq;
		return *this;
	}
	
	//! multiplies this dual quaternion with 'q' and returns the result
	constexpr dual_quaternion operator*(const dual_quaternion& q) const {
		return {
			rq * q.rq,
			rq * q.dq + dq * q.rq,
		};
	}

	//! scales/multiplies each vector component and the dual quaternion real part with 'f' and returns the result
	constexpr dual_quaternion operator*(const scalar_type& f) const {
		return { rq * f, dq * f };
	}
	
	//! multiplies this dual quaternion with 'q' and sets this dual quaternion to the result
	constexpr dual_quaternion& operator*=(const dual_quaternion& q) {
		*this = *this * q;
		return *this;
	}
	
	//! scales/multiplies each vector component and the dual quaternion real part with 'f' and sets this dual quaternion to the result
	constexpr dual_quaternion& operator*=(const scalar_type& f) {
		*this = *this * f;
		return *this;
	}
	
	//! divides this dual quaternion by 'q' (multiplies with 'q's inverted form) and returns the result
	constexpr dual_quaternion operator/(const dual_quaternion& q) const {
		const auto qrq_sq = q.rq * q.rq;
		return {
			(rq * q.rq) / qrq_sq,
			(q.rq * dq - rq * q.dq) / qrq_sq
		};
	}
	
	//! divides ("scales") each vector component and the dual quaternion real part by 'f' and returns the result
	constexpr dual_quaternion operator/(const scalar_type& f) const {
		// rather perform 1 division instead of 4
		const auto one_div_f = scalar_type(1) / f;
		return { rq * one_div_f, dq * one_div_f };
	}
	
	//! divides this dual quaternion by 'q' (multiplies with 'q's inverted form) and sets this dual quaternion to the result
	constexpr dual_quaternion& operator/=(const dual_quaternion& q) {
		*this = *this / q;
		return *this;
	}
	
	//! divides ("scales") each vector component and the dual quaternion real part by 'f' and sets this dual quaternion to the result
	constexpr dual_quaternion& operator/=(const scalar_type& f) {
		*this = *this / f;
		return *this;
	}
	
	//! inverts this dual quaternion
	constexpr dual_quaternion& invert() {
		*this = inverted();
		return *this;
	}
	
	//! returns the inverted form of this dual quaternion
	constexpr dual_quaternion inverted() const {
		const auto rq_inv = rq.inverted();
		return { rq_inv, -rq_inv * dq * rq_inv };
	}
	
	//! sets this dual quaternion to its conjugate
	constexpr dual_quaternion& conjugate() {
		*this = conjugated();
		return *this;
	}

	//! returns the conjugated form of this dual quaternion
	constexpr dual_quaternion conjugated() const {
		return { rq.conjugated(), dq.conjugated() };
	}
	
	//! sets this dual quaternion to its dual form conjugate (second conjugate)
	constexpr dual_quaternion& dual_conjugate() {
		*this = dual_conjugated();
		return *this;
	}

	//! returns the dual form of the conjugate of this dual quaternion (second conjugate)
	constexpr dual_quaternion dual_conjugated() const {
		return { rq, { -dq.x, -dq.y, -dq.z, -dq.r } };
	}
	
	//! sets this dual quaternion to its combined form conjugate (third conjugate)
	constexpr dual_quaternion& combined_conjugate() {
		*this = combined_conjugated();
		return *this;
	}

	//! returns the combined form of the conjugate of this dual quaternion (third conjugate)
	constexpr dual_quaternion combined_conjugated() const {
		return { rq.conjugated(), { dq.x, dq.y, dq.z, -dq.r } };
	}
	
	//! canonicalizes this dual quaternion (r component in rq will be positive)
	constexpr dual_quaternion& canonicalize() {
		if (rq.r < scalar_type(0)) {
			rq = -rq;
			dq = -dq;
		}
		return *this;
	}
	
	//! returns a canonicalized copy of this dual quaternion (r component in rq will be positive)
	constexpr dual_quaternion canonicalized() const {
		if (rq.r < scalar_type(0)) {
			return { -rq, -dq };
		}
		return *this;
	}

	//! transform the specified vector 'vec' according to this dual quaternion and returns the result
	//! NOTE: dual quaternion must be a unit/normalized dual quaternion
	constexpr vector3<scalar_type> transform(const vector3<scalar_type>& vec) const {
		// TODO: optimize this
		return (*this * dual_quaternion<scalar_type>(quaternion<scalar_type>(), { vec, scalar_type(0) }) * combined_conjugated()).dq.to_vector3();
	}
	
	//! returns the translational part as a 3D position
	constexpr vector3<scalar_type> to_position() const {
		return ((dq * scalar_type(2)) * rq.conjugated()).to_vector3();
	}
	
	//! converts this dual quaterion to a 4x4 matrix
	constexpr matrix4<scalar_type> to_matrix4() const {
		return rq.to_matrix4().set_translation(to_position());
	}
	
	//////////////////////////////////////////
	// relational
#pragma mark relational
	
	//! equal comparison
	constexpr bool operator==(const dual_quaternion<scalar_type>& q) const {
		return (rq == q.rq && dq == q.dq);
	}
	//! unequal comparison
	constexpr bool operator!=(const dual_quaternion<scalar_type>& q) const {
		return (rq != q.rq || dq != q.dq);
	}
	
	//! three-way comparison
	constexpr std::partial_ordering operator<=>(const dual_quaternion<scalar_type>& q) const {
		if (*this == q) {
			return std::partial_ordering::equivalent;
		}
		return std::partial_ordering::unordered;
	}
	
	//////////////////////////////////////////
	// static dual quaternion creation functions
#pragma mark static dual quaternion creation functions
	
	//! computes the translational/dual part from "pos" and the rotational part "rq"
	static constexpr quaternion<scalar_type> translation_with_rotation(const quaternion<scalar_type>& rq, const vector3<scalar_type>& pos) {
		// dq = 0.5 * (quaternion(pos, 0) * rq)
		return {
			// {x, y, z} * rq.r + {x, y, z}.crossed({rq.x, rq.y, rq.z})
			scalar_type(0.5) * (pos.x * rq.r + pos.y * rq.z - pos.z * rq.y),
			scalar_type(0.5) * (pos.y * rq.r + pos.z * rq.x - pos.x * rq.z),
			scalar_type(0.5) * (pos.z * rq.r + pos.x * rq.y - pos.y * rq.x),
			// -{x, y, z}.dot({rq.x, rq.y, rq.z})
			scalar_type(-0.5) * (pos.x * rq.x + pos.y * rq.y + pos.z * rq.z),
		};
	}
	
	//! converts the specified 4x4 matrix to a dual quaternion (3x3 part is considered for rotation, 1x3 translational part for translation)
	//! NOTE: for accuracy and better/proper handling of singularities, the branched conversion should be used (default), the branchless variant is however faster
	template <bool branchless = false>
	static constexpr dual_quaternion<scalar_type> from_matrix4(const matrix4<scalar_type>& mat) {
		auto rq = quaternion<scalar_type>::template from_matrix4<branchless>(mat);
		return { rq, translation_with_rotation(rq, mat.get_translation()) };
	}
	
	//! converts the specified 4x4 matrix (only considering the 3x3 rotational part) and the specified position to a dual quaternion
	//! NOTE: for accuracy and better/proper handling of singularities, the branched conversion should be used (default), the branchless variant is however faster
	template <bool branchless = false>
	static constexpr dual_quaternion<scalar_type> from_matrix4_with_position(const matrix4<scalar_type>& mat, const vector3<scalar_type>& pos) {
		auto rq = quaternion<scalar_type>::template from_matrix4<branchless>(mat);
		return { rq, translation_with_rotation(rq, pos) };
	}
	
	//! creates a dual quaternion with the rotational part set from the specified degrees angle "deg_angle" and axis vector "axis",
	//! and the translational/dual part set from "pos"
	template <bool canonicalize = true>
	static constexpr dual_quaternion rotation_deg_and_translation(const scalar_type& deg_angle,
																  const vector3<scalar_type>& axis,
																  const vector3<scalar_type>& pos) {
		auto rq = quaternion<scalar_type>::template rotation_deg<canonicalize>(deg_angle, axis);
		return { rq, translation_with_rotation(rq, pos) };
	}
	
	//! creates a dual quaternion with the rotational part set from the specified degrees angle "rad_angle" and axis vector "axis",
	//! and the translational/dual part set from "pos"
	template <bool canonicalize = true>
	static constexpr dual_quaternion rotation_and_translation(const scalar_type& rad_angle,
															  const vector3<scalar_type>& axis,
															  const vector3<scalar_type>& pos) {
		auto rq = quaternion<scalar_type>::template rotation<canonicalize>(rad_angle, axis);
		return { rq, translation_with_rotation(rq, pos) };
	}
	
	//////////////////////////////////////////
	// type conversion
	
	//! returns a C++/std array with the elements of this dual quaternion in { x, y, z, r, dx, dy, dz, dq } order
	constexpr auto to_array() const {
		return std::array<scalar_type, 8u> { rq.x, rq.y, rq.z, rq.r, dq.x, dq.y, dq.z, dq.r };
	}
	
	//! explicitly casts this dual quaternion (its components) to "dst_scalar_type"
	template <typename dst_scalar_type>
	constexpr auto cast() const {
		return dual_quaternion<dst_scalar_type> { rq.template cast<dst_scalar_type>(), dq.template cast<dst_scalar_type>() };
	}
	
	//! explicitly reinterprets this dual quaternion (its components) as "dst_scalar_type"
	template <typename dst_scalar_type>
	constexpr auto reinterpret() const {
		static_assert(sizeof(dst_scalar_type) <= sizeof(scalar_type),
					  "reinterpret type size must <= the current type size");
		return dual_quaternion<dst_scalar_type> { rq.template reinterpret<dst_scalar_type>(), dq.template reinterpret<dst_scalar_type>() };
	}
	
};

typedef dual_quaternion<float> dual_quaternionf;
#if !defined(FLOOR_COMPUTE_NO_DOUBLE) // disable double + long double if specified
typedef dual_quaternion<double> dual_quaterniond;
// always disable long double on compute platforms
#if !defined(FLOOR_COMPUTE) || (defined(FLOOR_COMPUTE_HOST) && !defined(FLOOR_COMPUTE_HOST_DEVICE))
typedef dual_quaternion<long double> dual_quaternionl;
#endif
#endif

#if defined(FLOOR_EXPORT)
// only instantiate this in the dual_quaternion.cpp
extern template class dual_quaternion<float>;
#if !defined(FLOOR_COMPUTE_NO_DOUBLE)
extern template class dual_quaternion<double>;
extern template class dual_quaternion<long double>;
#endif
#endif

#endif
