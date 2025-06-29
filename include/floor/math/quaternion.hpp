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
#if !defined(FLOOR_NO_MATH_STR)
#include <ostream>
#include <sstream>
#include <string>
#endif

// for compute and graphics: signal that this quaternion type can be converted to the corresponding clang/LLVM vector type
#if defined(FLOOR_DEVICE) && (!defined(FLOOR_DEVICE_HOST_COMPUTE) || defined(FLOOR_DEVICE_HOST_COMPUTE_IS_DEVICE))
#define FLOOR_CLANG_VECTOR_COMPAT __attribute__((vector_compat))
#else
#define FLOOR_CLANG_VECTOR_COMPAT
#endif

namespace fl {

template <typename scalar_type> class FLOOR_CLANG_VECTOR_COMPAT quaternion {
public:
	//! "this" quaternion type
	using quaternion_type = quaternion<scalar_type>;
	//! this scalar type
	using this_scalar_type = scalar_type;
	
	//! { xyz = 3D vector component, r = real component }
	scalar_type x;
	scalar_type y;
	scalar_type z;
	scalar_type r;
	
	//////////////////////////////////////////
	// constructors and assignment operators
#pragma mark constructors and assignment operators
	
	//! constructs an identity quaternion
	constexpr quaternion() noexcept : x(scalar_type(0)), y(scalar_type(0)), z(scalar_type(0)), r(scalar_type(1)) {}
	
	//! constructs a quaternion with the vector component set to 'vec' and the real component set to 'r_'
	constexpr quaternion(const vector3<scalar_type>& vec, const scalar_type& r_) noexcept : x(vec.x), y(vec.y), z(vec.z), r(r_) {}
	
	//! constructs a quaternion from its corresponding vector4 representation
	constexpr quaternion(const vector4<scalar_type>& vec) noexcept : x(vec.x), y(vec.y), z(vec.z), r(vec.w) {}
	
	//! constructs a quaternion with the vector component set to (x_, y_, z_) and the real component set to 'r_'
	constexpr quaternion(const scalar_type& x_, const scalar_type& y_, const scalar_type& z_, const scalar_type& r_) noexcept :
	x(x_), y(y_), z(z_), r(r_) {}
	
	//! default copy-construct from same quaternion type
	constexpr quaternion(const quaternion& q) noexcept = default;
	
	//! default copy-construct from same quaternion type (rvalue)
	constexpr quaternion(quaternion&& q) noexcept = default;
	
	//! copy-construct from same quaternion type (pointer)
	constexpr quaternion(const quaternion* q) noexcept : x(q->x), y(q->y), z(q->z), r(q->r) {}
	
	//! default copy assignment
	constexpr quaternion& operator=(const quaternion& q) noexcept = default;
	
	//! default move assignment
	constexpr quaternion& operator=(quaternion&& q) noexcept = default;
	
	//////////////////////////////////////////
	// I/O
#pragma mark I/O
	
#if !defined(FLOOR_NO_MATH_STR)
	//! ostream output of this quaternion
	friend std::ostream& operator<<(std::ostream& output, const quaternion& q) {
		output << "(" << q.r << ": " << q.x << ", " << q.y << ", " << q.z << ")";
		return output;
	}
	
	//! returns a string representation of this quaternion:
	//!  * if !as_readable: returns the plain (x, y, z, r) values as a string (same as operator<<)
	//!  * if as_readable: returns a more human-readable (rotation-angle: 3D rotatation-axis) string
	std::string to_string(const bool as_readable = false) const {
		std::stringstream sstr;
		if (!as_readable) {
			sstr << *this;
		} else {
			sstr << "(" << rotation_angle_deg() << "°: " << rotation_axis() << ")";
		}
		return sstr.str();
	}
#endif
	
	//////////////////////////////////////////
	// basic ops
#pragma mark basic ops
	
	//! adds this quaternion to 'q' and returns the result
	constexpr quaternion operator+(const quaternion& q) const {
		return { x + q.x, y + q.y, z + q.z, r + q.r };
	}
	
	//! adds this quaternion to 'q' and sets this quaternion to the result
	constexpr quaternion& operator+=(const quaternion& q) {
		x += q.x;
		y += q.y;
		z += q.z;
		r += q.r;
		return *this;
	}
	
	//! subtracts 'q' from this quaternion and returns the result
	constexpr quaternion operator-(const quaternion& q) const {
		return { x - q.x, y - q.y, z - q.z, r - q.r };
	}
	
	//! subtracts 'q' from this quaternion and sets this quaternion to the result
	constexpr quaternion& operator-=(const quaternion& q) {
		x -= q.x;
		y -= q.y;
		z -= q.z;
		r -= q.r;
		return *this;
	}
	
	//! component-wise unary - (flips the sign of all components)
	constexpr quaternion operator-() const {
		return { -x, -y, -z, -r };
	}
	
	//! interprets this quaternion as 4D vector and computes its dot product with itself
	constexpr scalar_type dot() const {
		return (x * x + y * y + z * z + r * r);
	}
	
	//! interprets this quaternion and the specified quaternion "q" as 4D vectors and computes their dot product
	constexpr scalar_type dot(const quaternion& q) const {
		return (x * q.x + y * q.y + z * q.z + r * q.r);
	}
	
	//! interprets the vector component of this quaternion as a 3D vector and computes the cross product of it with the specified vector
	constexpr vector3<scalar_type> crossed(const vector3<scalar_type>& vec) const {
		return {
			y * vec.z - z * vec.y,
			z * vec.x - x * vec.z,
			x * vec.y - y * vec.x
		};
	}
	
	//! multiplies this quaternion with 'q' and returns the result
	constexpr quaternion operator*(const quaternion& q) const {
		return {
			r * q.to_vector3() + q.r * to_vector3() + crossed(q.to_vector3()),
			r * q.r - to_vector3().dot(q.to_vector3())
		};
	}
	
	//! scales/multiplies each vector component and the quaternion real part with 'f' and returns the result
	constexpr quaternion operator*(const scalar_type& f) const {
		return { x * f, y * f, z * f, r * f };
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
		return { x * one_div_f, y * one_div_f, z * one_div_f, r * one_div_f };
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
		return vector_helper<scalar_type>::sqrt(dot());
	}
	
	//! computes the "1 / magnitude" of this quaternion
	constexpr scalar_type inv_magnitude() const {
		return vector_helper<scalar_type>::rsqrt(dot());
	}
	
	//! inverts this quaternion
	constexpr quaternion& invert() {
		*this = inverted();
		return *this;
	}
	
	//! returns the inverted form of this quaternion
	constexpr quaternion inverted() const {
		return conjugated() / dot();
	}
	
	//! conjugates this quaternion (flip v)
	constexpr quaternion& conjugate() {
		x = -x;
		y = -y;
		z = -z;
		return *this;
	}
	
	//! returns the conjugated form of this quaternion
	constexpr quaternion conjugated() const {
		return { -x, -y, -z, r };
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
	
	//! canonicalizes this quaternion (r will be positive)
	constexpr quaternion& canonicalize() {
		if (r < scalar_type(0)) {
			x = -x;
			y = -y;
			z = -z;
			r = -r;
		}
		return *this;
	}
	
	//! returns a canonicalized copy of this quaternion (r will be positive)
	constexpr quaternion canonicalized() const {
		return (r >= scalar_type(0) ? *this : quaternion { -x, -y, -z, -r });
	}
	
	//! computes the r component for this quaternion when only the vector component has been set (used for compression)
	constexpr quaternion& compute_r() {
		const scalar_type val = scalar_type(1) - (x * x + y * y + z * z);
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
	//! NOTE: quaternion must be a unit/normalized quaternion
	constexpr vector3<scalar_type> rotate_vector(const vector3<scalar_type>& vec) const {
		// original: (*this * quaternion { vec, scalar_type(0) } * conjugated()).v;
		// simplified: https://gamedev.stackexchange.com/a/50545 + comments
		return { scalar_type(2) * (to_vector3().dot(vec) * to_vector3() + r * crossed(vec)) + (scalar_type(2) * r * r - scalar_type(1)) * vec };
	}
	
	//! returns the rotation axis of this quaternion
	constexpr vector3<scalar_type> rotation_axis() const {
		return vector3<scalar_type>(x, y, z).normalize();
	}
	
	//! returns the rotation angle of this quaternion in radian
	constexpr scalar_type rotation_angle() const {
		return scalar_type(2) * vector_helper<scalar_type>::acos(r);
	}
	
	//! returns the rotation angle of this quaternion in degrees
	constexpr scalar_type rotation_angle_deg() const {
		return const_math::rad_to_deg(rotation_angle());
	}
	
	//! linearly interpolates this quaternion with another quaternion according to interp
	constexpr quaternion& interpolate(const quaternion& q, const scalar_type& interp) {
		*this = ((q - *this) * interp + *this).normalize();
		return *this;
	}
	
	//! returns the linear interpolation between this quaternion and another quaternion according to interp
	constexpr quaternion interpolated(const quaternion& q, const scalar_type& interp) const {
		return ((q - *this) * interp + *this).normalize();
	}
	
	//! converts the rotation of this quaternion to euler angles
	constexpr vector3<scalar_type> to_euler() const {
		// http://en.wikipedia.org/wiki/Conversion_between_quaternions_and_Euler_angles#Conversion
		return {
			vector_helper<scalar_type>::atan2(scalar_type(2) * (r * x + y * z),
											  scalar_type(1) - scalar_type(2) * (x * x + y * y)),
			vector_helper<scalar_type>::asin(scalar_type(2) * (r * y - z * x)),
			vector_helper<scalar_type>::atan2(scalar_type(2) * (r * z + x * y),
											  scalar_type(1) - scalar_type(2) * (y * y + z * z))
		};
	}
	
	//! converts the rotation of this quaternion to a 4x4 matrix
	constexpr matrix4<scalar_type> to_matrix4() const {
		// http://www.euclideanspace.com/maths/geometry/rotations/conversions/quaternionToMatrix/index.htm
		const auto xx = x * x;
		const auto yy = y * y;
		const auto zz = z * z;
		
		return {
			scalar_type(1) - scalar_type(2) * yy - scalar_type(2) * zz,
			scalar_type(2) * (x * y + z * r),
			scalar_type(2) * (x * z - y * r),
			scalar_type(0),
			
			scalar_type(2) * (x * y - z * r),
			scalar_type(1) - scalar_type(2) * xx - scalar_type(2) * zz,
			scalar_type(2) * (y * z + x * r),
			scalar_type(0),
			
			scalar_type(2) * (x * z + y * r),
			scalar_type(2) * (y * z - x * r),
			scalar_type(1) - scalar_type(2) * xx - scalar_type(2) * yy,
			scalar_type(0),
			
			scalar_type(0),
			scalar_type(0),
			scalar_type(0),
			scalar_type(1)
		};
	}
	
	//////////////////////////////////////////
	// relational
#pragma mark relational
	
FLOOR_PUSH_WARNINGS()
FLOOR_IGNORE_WARNING(float-equal)
	
	//! equal comparison
	constexpr bool operator==(const quaternion<scalar_type>& q) const {
		return (r == q.r && x == q.x && y == q.y && z == q.z);
	}
	//! unequal comparison
	constexpr bool operator!=(const quaternion<scalar_type>& q) const {
		return (r != q.r || x != q.x || y != q.y || z != q.z);
	}
	
	//! equal comparison with an epsilon
	constexpr bool is_equal(const quaternion<scalar_type>& q, const scalar_type& epsilon) const {
		return (const_math::is_equal(r, q.r, epsilon) &&
				const_math::is_equal(x, q.x, epsilon) &&
				const_math::is_equal(y, q.y, epsilon) &&
				const_math::is_equal(z, q.z, epsilon));
	}
	
	//! three-way comparison
	constexpr std::partial_ordering operator<=>(const quaternion<scalar_type>& q) const {
		if (*this == q) {
			return std::partial_ordering::equivalent;
		}
		return std::partial_ordering::unordered;
	}
	
FLOOR_POP_WARNINGS()
	
	//////////////////////////////////////////
	// static quaternion creation functions
#pragma mark static quaternion creation functions
	
	//! converts the specified euler angle rotations to a quaternion
	//! NOTE: assumes the angles are in radian
	//! ref: https://en.wikipedia.org/wiki/Conversion_between_quaternions_and_Euler_angles#Euler_angles_(in_3-2-1_sequence)_to_quaternion_conversion
	static constexpr quaternion<scalar_type> from_euler(const scalar_type x, const scalar_type y, const scalar_type z) {
		const auto cr = math::cos(x * scalar_type(0.5));
		const auto sr = math::sin(x * scalar_type(0.5));
		const auto cp = math::cos(y * scalar_type(0.5));
		const auto sp = math::sin(y * scalar_type(0.5));
		const auto cy = math::cos(z * scalar_type(0.5));
		const auto sy = math::sin(z * scalar_type(0.5));
		return {
			sr * cp * cy - cr * sp * sy,
			cr * sp * cy + sr * cp * sy,
			cr * cp * sy - sr * sp * cy,
			cr * cp * cy + sr * sp * sy,
		};
	}
	
	//! converts the specified euler angle rotations to a quaternion
	//! NOTE: assumes the angles are in radian
	static constexpr quaternion<scalar_type> from_euler(const vector3<scalar_type>& xyz) {
		return from_euler(xyz.x, xyz.y, xyz.z);
	}
	
	//! converts the specified 4x4 matrix (only considering 3x3) to a quaternion
	//! NOTE: for accuracy and better/proper handling of singularities, the branched conversion should be used (default), the branchless variant is however faster
	template <bool branchless = false>
	static constexpr quaternion<scalar_type> from_matrix4(const matrix4<scalar_type>& mat) {
		if constexpr (branchless) {
			// http://www.thetenthplanet.de/archives/1994
			using st = scalar_type;
			quaternion<scalar_type> q {
				vector_helper<st>::sqrt(vector_helper<st>::max(st(1) + mat.data[0] - mat.data[5] - mat.data[10], st(0))) * st(0.5),
				vector_helper<st>::sqrt(vector_helper<st>::max(st(1) - mat.data[0] + mat.data[5] - mat.data[10], st(0))) * st(0.5),
				vector_helper<st>::sqrt(vector_helper<st>::max(st(1) - mat.data[0] - mat.data[5] + mat.data[10], st(0))) * st(0.5),
				vector_helper<st>::sqrt(vector_helper<st>::max(st(1) + mat.data[0] + mat.data[5] + mat.data[10], st(0))) * st(0.5),
			};
			q.x = vector_helper<st>::copysign(q.x, mat.data[6] - mat.data[9]);
			q.y = vector_helper<st>::copysign(q.y, mat.data[8] - mat.data[2]);
			q.z = vector_helper<st>::copysign(q.z, mat.data[1] - mat.data[4]);
			return q;
		} else {
			// https://math.stackexchange.com/questions/893984/conversion-of-rotation-matrix-to-quaternion/3183435#3183435
			// https://d3cw3dd2w32x2b.cloudfront.net/wp-content/uploads/2015/01/matrix-to-quat.pdf
			// NOTE: matrix is transposed here
			quaternion<scalar_type> q;
			scalar_type t {};
			if (mat.data[10] < scalar_type(0)) {
				if (mat.data[0] > mat.data[5]) {
					t = scalar_type(1) + mat.data[0] - mat.data[5] - mat.data[10];
					q = { t, mat.data[1] + mat.data[4], mat.data[8] + mat.data[2], mat.data[6] - mat.data[9] };
				} else {
					t = scalar_type(1) - mat.data[0] + mat.data[5] - mat.data[10];
					q = { mat.data[1] + mat.data[4], t, mat.data[6] + mat.data[9], mat.data[8] - mat.data[2] };
				}
			} else {
				if (mat.data[0] < -mat.data[5]) {
					t = scalar_type(1) - mat.data[0] - mat.data[5] + mat.data[10];
					q = { mat.data[8] + mat.data[2], mat.data[6] + mat.data[9], t, mat.data[1] - mat.data[4] };
				} else {
					t = scalar_type(1) + mat.data[0] + mat.data[5] + mat.data[10];
					q = { mat.data[6] - mat.data[9], mat.data[8] - mat.data[2], mat.data[1] - mat.data[4], t };
				}
			}
			q *= scalar_type(0.5) / vector_helper<scalar_type>::sqrt(t);
			return q;
		}
	}
	
	//! creates a quaternion from the specified radian angle 'rad_angle' and axis vector 'vec'
	template <bool canonicalize = true>
	static constexpr quaternion rotation(const scalar_type& rad_angle, const vector3<scalar_type>& vec) {
		const auto rad_angle_2 = rad_angle * scalar_type(0.5);
		const auto r = vector_helper<scalar_type>::cos(rad_angle_2);
		if constexpr (canonicalize) {
			return {
				(r < scalar_type(0) ? -scalar_type(1) : scalar_type(1)) * vec.normalized() * vector_helper<scalar_type>::sin(rad_angle_2),
				r < scalar_type(0) ? -r : r,
			};
		} else {
			return { vec.normalized() * vector_helper<scalar_type>::sin(rad_angle_2), r };
		}
	}
	
	//! creates a quaternion from the specified degrees angle 'deg_angle' and axis vector 'vec'
	template <bool canonicalize = true>
	static constexpr quaternion rotation_deg(const scalar_type& deg_angle, const vector3<scalar_type>& vec) {
		// note that this already performs the later division by 2
		const auto rad_angle = const_math::PI_DIV_360<scalar_type> * deg_angle;
		const auto r = vector_helper<scalar_type>::cos(rad_angle);
		if constexpr (canonicalize) {
			return {
				(r < scalar_type(0) ? -scalar_type(1) : scalar_type(1)) * vec.normalized() * vector_helper<scalar_type>::sin(rad_angle),
				r < scalar_type(0) ? -r : r,
			};
		} else {
			return { vec.normalized() * vector_helper<scalar_type>::sin(rad_angle), r };
		}
	}
	
	//! creates a quaternion according to the necessary rotation to get from vector "from" to vector "to"
	static constexpr quaternion rotation_from_to_vector(const vector3<scalar_type>& from, const vector3<scalar_type>& to) {
		return rotation(from.angle_kahan(to), from.crossed(to).normalize());
	}
	
	//! creates a quaternion according to the necessary rotation to get from quaternion "from" to quaternion "to",
	//! i.e. result * from == to
	static constexpr quaternion rotation_from_to(const quaternion<scalar_type>& from, const quaternion<scalar_type>& to) {
		return to * from.inverted();
	}
	
	//////////////////////////////////////////
	// type conversion
	
	//! returns a C++/std array with the elements of this quaternion in { x, y, z, r } order
	constexpr auto to_array() const {
		return std::array<scalar_type, 4u> { x, y, z, r };
	}
	
	//! returns a vector4<scalar_type> with the elements of this quaternion in { x, y, z, r } order
	constexpr vector4<scalar_type> to_vector4() const {
		return { x, y, z, r };
	}
	
	//! returns a vector3<scalar_type> with the vector elements of this quaternion in { x, y, z } order
	constexpr vector3<scalar_type> to_vector3() const {
		return { x, y, z };
	}
	
	//! explicitly casts this quaternion (its components) to "dst_scalar_type"
	template <typename dst_scalar_type>
	constexpr auto cast() const {
		return quaternion<dst_scalar_type> {
			dst_scalar_type(x),
			dst_scalar_type(y),
			dst_scalar_type(z),
			dst_scalar_type(r)
		};
	}
	
	//! explicitly reinterprets this quaternion (its components) as "dst_scalar_type"
	template <typename dst_scalar_type>
	constexpr auto reinterpret() const {
		static_assert(sizeof(dst_scalar_type) <= sizeof(scalar_type),
					  "reinterpret type size must <= the current type size");
		return quaternion<dst_scalar_type> {
			*((const dst_scalar_type*)&x),
			*((const dst_scalar_type*)&y),
			*((const dst_scalar_type*)&z),
			*((const dst_scalar_type*)&r),
		};
	}
	
};

#undef FLOOR_CLANG_VECTOR_COMPAT

using quaternionf = quaternion<float>;
#if !defined(FLOOR_DEVICE_NO_DOUBLE) // disable double + long double if specified
using quaterniond = quaternion<double>;
// always disable long double on device platforms
#if !defined(FLOOR_DEVICE) || (defined(FLOOR_DEVICE_HOST_COMPUTE) && !defined(FLOOR_DEVICE_HOST_COMPUTE_IS_DEVICE))
using quaternionl = quaternion<long double>;
#endif
#endif

#if defined(FLOOR_EXPORT)
// only instantiate this in the quaternion.cpp
extern template class quaternion<float>;
#if !defined(FLOOR_DEVICE_NO_DOUBLE)
extern template class quaternion<double>;
extern template class quaternion<long double>;
#endif
#endif

} // namespace fl
