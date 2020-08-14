/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2020 Florian Ziesche
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

#ifndef __FLOOR_QUATERNION_HPP__
#define __FLOOR_QUATERNION_HPP__

#include <floor/math/vector_lib.hpp>
#if !defined(FLOOR_NO_MATH_STR)
#include <ostream>
#include <sstream>
#include <string>
#endif

template <typename scalar_type> class quaternion {
public:
	//! "this" quaternion type
	typedef quaternion<scalar_type> quaternion_type;
	//! this scalar type
	typedef scalar_type this_scalar_type;
	//! decayed scalar type (removes refs/etc.)
	typedef decay_t<scalar_type> decayed_scalar_type;
	//! this vector type
	typedef vector3<scalar_type> this_vector_type;
	//! decayed vector type (with decayed_scalar_type)
	typedef vector3<decayed_scalar_type> decayed_vector_type;
	
	//! quaternion 3D vector component
	vector3<scalar_type> v;
	//! quaternion real component
	scalar_type r;
	
	//////////////////////////////////////////
	// constructors and assignment operators
#pragma mark constructors and assignment operators
	
	//! constructs an identity quaternion
	constexpr quaternion() noexcept : v(scalar_type(0)), r(scalar_type(1)) {}
	
	//! constructs a quaternion with the vector component set to 'vec' and the real component set to 'r_'
	constexpr quaternion(const vector3<scalar_type>& vec, const scalar_type& r_) noexcept : v(vec), r(r_) {}
	
	//! constructs a quaternion with the vector component set to (x, y, z) and the real component set to 'r_'
	constexpr quaternion(const scalar_type& x, const scalar_type& y, const scalar_type& z, const scalar_type& r_) noexcept :
	v(x, y, z), r(r_) {}
	
	//! copy-construct from same quaternion type
	constexpr quaternion(const quaternion& q) noexcept : v(q.v), r(q.r) {}
	
	//! copy-construct from same quaternion type (rvalue)
	constexpr quaternion(quaternion&& q) noexcept : v(q.v), r(q.r) {}
	
	//! copy-construct from same quaternion type (pointer)
	constexpr quaternion(const quaternion* q) noexcept : v(q->v), r(q->r) {}
	
	//! copy assignment
	constexpr quaternion& operator=(const quaternion& q) {
		v = q.v;
		r = q.r;
		return *this;
	}
	
	//////////////////////////////////////////
	// I/O
#pragma mark I/O

#if !defined(FLOOR_NO_MATH_STR)
	//! ostream output of this quaternion
	friend ostream& operator<<(ostream& output, const quaternion& q) {
		output << "(" << q.r << ": " << q.v << ")";
		return output;
	}
	
	//! returns a string representation of this quaternion
	string to_string() const {
		stringstream sstr;
		sstr << *this;
		return sstr.str();
	}
#endif
	
	//////////////////////////////////////////
	// basic ops
#pragma mark basic ops
	
	//! adds this quaternion to 'q' and returns the result
	constexpr quaternion operator+(const quaternion& q) const {
		return { v + q.v, r + q.r };
	}
	
	//! adds this quaternion to 'q' and sets this quaternion to the result
	constexpr quaternion& operator+=(const quaternion& q) {
		v += q.v;
		r += q.r;
		return *this;
	}
	
	//! subtracts 'q' from this quaternion and returns the result
	constexpr quaternion operator-(const quaternion& q) const {
		return { v - q.v, r - q.r };
	}
	
	//! subtracts 'q' from this quaternion and sets this quaternion to the result
	constexpr quaternion& operator-=(const quaternion& q) {
		v -= q.v;
		r -= q.r;
		return *this;
	}
	
	//! multiplies this quaternion with 'q' and returns the result
	constexpr quaternion operator*(const quaternion& q) const {
		return {
			r * q.v + q.r * v + v.crossed(q.v),
			r * q.r - v.dot(q.v)
		};
	}

	//! scales/multiplies each vector component and the quaternion real part with 'f' and returns the result
	constexpr quaternion operator*(const scalar_type& f) const {
		return { v * f, r * f };
	}
	
	//! multiplies this quaternion with 'q' and sets this quaternion to the result
	constexpr quaternion& operator*=(const quaternion& q) {
		*this = *this * q;
		return *this;
	}
	
	//! scales/multiplies each vector component and the quaternion real part with 'f' and sets this quaternion to the result
	constexpr quaternion& operator*=(const scalar_type& f) {
		*this = *this * f;
		return *this;
	}
	
	//! divides this quaternion by 'q' (multiplies with 'q's inverted form) and returns the result
	constexpr quaternion operator/(const quaternion& q) const {
		return *this * q.inverted();
	}
	
	//! divides ("scales") each vector component and the quaternion real part by 'f' and returns the result
	constexpr quaternion operator/(const scalar_type& f) const {
		// rather perform 1 division instead of 4
		const auto one_div_f = scalar_type(1) / f;
		return { v * one_div_f, r * one_div_f };
	}
	
	//! divides this quaternion by 'q' (multiplies with 'q's inverted form) and sets this quaternion to the result
	constexpr quaternion& operator/=(const quaternion& q) {
		*this = *this / q;
		return *this;
	}
	
	//! divides ("scales") each vector component and the quaternion real part by 'f' and sets this quaternion to the result
	constexpr quaternion& operator/=(const scalar_type& f) {
		*this = *this / f;
		return *this;
	}
	
	//! computes the magnitude of this quaternion
	constexpr scalar_type magnitude() const {
		return vector_helper<scalar_type>::sqrt(r * r + v.dot());
	}
	
	//! computes the "1 / magnitude" of this quaternion
	constexpr scalar_type inv_magnitude() const {
		return vector_helper<scalar_type>::rsqrt(r * r + v.dot());
	}
	
	//! inverts this quaternion
	constexpr quaternion& invert() {
		*this = inverted();
		return *this;
	}
	
	//! returns the inverted form of this quaternion
	constexpr quaternion inverted() const {
		return conjugated() / (r * r + v.dot());
	}
	
	//! conjugates this quaternion (flip v)
	constexpr quaternion& conjugate() {
		v = -v;
		return *this;
	}

	//! returns the conjugated form of this quaternion
	constexpr quaternion conjugated() const {
		return { -v, r };
	}
	
	//! normalizes this quaternion
	constexpr quaternion& normalize() {
		*this *= inv_magnitude();
		return *this;
	}
	
	//! returns the normalized form of this quaternion
	constexpr quaternion normalized() const {
		return *this * inv_magnitude();
	}
	
	//! computes the r component for this quaternion when only the vector component has been set (used for compression)
	constexpr quaternion& compute_r() {
		const scalar_type val = scalar_type(1) - v.dot();
		r = (val < scalar_type(0) ? scalar_type(0) : -vector_helper<scalar_type>::sqrt(val));
		return *this;
	}
	
	//! rotates to the specified vector 'vec' and sets this quaternion to the resulting quaternion
	constexpr quaternion& rotate(const vector3<scalar_type>& vec) {
		*this = rotated(vec);
		return *this;
	}

	//! rotates to the specified vector 'vec' and returns the resulting quaternion
	constexpr quaternion rotated(const vector3<scalar_type>& vec) const {
		return (*this * quaternion { vec, scalar_type(0) } * conjugated());
	}

	//! rotates the specified vector 'vec' according to this quaternion and returns the result
	constexpr vector3<scalar_type> rotate_vector(const vector3<scalar_type>& vec) const {
		return (*this * quaternion { vec, scalar_type(0) } * conjugated()).v;
	}
	
	//! converts the rotation of this quaternion to euler angles
	constexpr vector3<scalar_type> to_euler() const {
		// http://en.wikipedia.org/wiki/Conversion_between_quaternions_and_Euler_angles#Conversion
		return {
			vector_helper<scalar_type>::atan2(scalar_type(2) * (r * v.x + v.y * v.z),
											  scalar_type(1) - scalar_type(2) * (v.x * v.x + v.y * v.y)),
			vector_helper<scalar_type>::asin(scalar_type(2) * (r * v.y - v.z * v.x)),
			vector_helper<scalar_type>::atan2(scalar_type(2) * (r * v.z + v.x * v.y),
											  scalar_type(1) - scalar_type(2) * (v.y * v.y + v.z * v.z))
		};
	}
	
	//! converts the rotation of this quaternion to a 4x4 matrix
	constexpr matrix4<scalar_type> to_matrix4() const {
		// http://www.euclideanspace.com/maths/geometry/rotations/conversions/quaternionToMatrix/index.htm
		const auto xx = v.x * v.x;
		const auto yy = v.y * v.y;
		const auto zz = v.z * v.z;
		
		return {
			scalar_type(1) - scalar_type(2) * yy - scalar_type(2) * zz,
			scalar_type(2) * (v.x * v.y - v.z * r),
			scalar_type(2) * (v.x * v.z + v.y * r),
			scalar_type(0),
			
			scalar_type(2) * (v.x * v.y + v.z * r),
			scalar_type(1) - scalar_type(2) * xx - scalar_type(2) * zz,
			scalar_type(2) * (v.y * v.z - v.x * r),
			scalar_type(0),
			
			scalar_type(2) * (v.x * v.z - v.y * r),
			scalar_type(2) * (v.y * v.z + v.x * r),
			scalar_type(1) - scalar_type(2) * xx - scalar_type(2) * yy,
			scalar_type(0),
			
			scalar_type(0),
			scalar_type(0),
			scalar_type(0),
			scalar_type(1)
		};
	}
	
	//////////////////////////////////////////
	// static quaternion creation functions
#pragma mark static quaternion creation functions
	
	//! creates a quaternion from the specified radian angle 'rad_angle' and axis vector 'vec'
	static constexpr quaternion rotation(const scalar_type& rad_angle, const vector3<scalar_type>& vec) {
		const auto rad_angle_2 = rad_angle * scalar_type(0.5);
		return {
			vec.normalized() * vector_helper<scalar_type>::sin(rad_angle_2),
			vector_helper<scalar_type>::cos(rad_angle_2)
		};
	}
	
	//! creates a quaternion from the specified degrees angle 'deg_angle' and axis vector 'vec'
	static constexpr quaternion rotation_deg(const scalar_type& deg_angle, const vector3<scalar_type>& vec) {
		// note that this already performs the later division by 2
		const auto rad_angle = const_math::PI_DIV_360<scalar_type> * deg_angle;
		return {
			vec.normalized() * vector_helper<scalar_type>::sin(rad_angle),
			vector_helper<scalar_type>::cos(rad_angle)
		};
	}
	
};

typedef quaternion<float> quaternionf;
#if !defined(FLOOR_COMPUTE_NO_DOUBLE) // disable double + long double if specified
typedef quaternion<double> quaterniond;
// always disable long double on compute platforms
#if !defined(FLOOR_COMPUTE) || (defined(FLOOR_COMPUTE_HOST) && !defined(FLOOR_COMPUTE_HOST_DEVICE))
typedef quaternion<long double> quaternionl;
#endif
#endif

#if defined(FLOOR_EXPORT)
// only instantiate this in the quaternion.cpp
extern template class quaternion<float>;
extern template class quaternion<double>;
extern template class quaternion<long double>;
#endif

#endif
