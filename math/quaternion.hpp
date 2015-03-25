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

#ifndef __FLOOR_QUATERNION_HPP__
#define __FLOOR_QUATERNION_HPP__

#include <floor/math/vector_lib.hpp>
#if !defined(FLOOR_NO_MATH_STR)
#include <ostream>
#include <sstream>
#include <string>
#endif

template <typename T> class quaternion;
typedef quaternion<float> quaternionf;
typedef quaternion<double> quaterniond;

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wpacked"
#endif

template <typename T> class __attribute__((packed, aligned(4))) quaternion {
public:
	T x, y, z, r;
	constexpr quaternion() noexcept : x(T(0)), y(T(0)), z(T(0)), r(T(1)) {}
	constexpr quaternion(const T& x_, const T& y_, const T& z_, const T& r_) noexcept : x(x_), y(y_), z(z_), r(r_) {}
	constexpr quaternion(const quaternion& q) noexcept : x(q.x), y(q.y), z(q.z), r(q.r) {}
	constexpr quaternion(const T& a, const vector3<T>& v) noexcept { set_rotation(a, v); }
	
#if !defined(FLOOR_NO_MATH_STR)
	friend ostream& operator<<(ostream& output, const quaternion& q) {
		output << "(" << q.r << ": " << q.x << ", " << q.y << ", " << q.z << ")";
		return output;
	}
	string to_string() const {
		stringstream sstr;
		sstr << *this;
		return sstr.str();
	}
#endif
	
	constexpr quaternion operator+(const quaternion& q) const;
	constexpr quaternion operator-(const quaternion& q) const;
	constexpr quaternion operator*(const quaternion& q) const;
	constexpr quaternion operator*(const T& f) const;
	constexpr quaternion operator/(const quaternion& q) const;
	constexpr quaternion operator/(const T& f) const;
	constexpr quaternion& operator=(const quaternion& q);
	constexpr quaternion& operator+=(const quaternion& q);
	constexpr quaternion& operator-=(const quaternion& q);
	constexpr quaternion& operator*=(const quaternion& q);
	constexpr quaternion& operator*=(const T& f);
	constexpr quaternion& operator/=(const quaternion& q);
	constexpr quaternion& operator/=(const T& f);
	
	//
	constexpr T magnitude() const;
	
	constexpr quaternion& invert();
	constexpr quaternion inverted() const;
	
	constexpr quaternion& conjugate();
	constexpr quaternion conjugated() const;
	
	constexpr quaternion& normalize();
	constexpr quaternion normalized() const;
	
	constexpr quaternion& compute_r();
	
	constexpr vector3<T> rotate(const vector3<T>& v) const;
	constexpr quaternion& set_rotation(const T& a, const vector3<T>& v);
	constexpr quaternion& set_rotation(const T& a, const T& i, const T& j, const T& k);
	
	//
	constexpr vector3<T> to_euler() const;
	constexpr matrix4<T> to_matrix4() const;
	constexpr quaternion& from_euler(const vector3<T>& v);
};


template<typename T> constexpr quaternion<T> quaternion<T>::operator+(const quaternion& q) const {
	return { x + q.x, y + q.y, z + q.z, r + q.r };
}

template<typename T> constexpr quaternion<T> quaternion<T>::operator-(const quaternion& q) const {
	return { x - q.x, y - q.y, z - q.z, r - q.r };
}

template<typename T> constexpr quaternion<T> quaternion<T>::operator*(const quaternion& q) const {
	return {
		r * q.x + x * q.r + y * q.z - z * q.y,
		r * q.y - x * q.z + y * q.r + z * q.x,
		r * q.z + x * q.y - y * q.x + z * q.r,
		r * q.r - x * q.x - y * q.y - z * q.z
	};
}

template<typename T> constexpr quaternion<T> quaternion<T>::operator*(const T& f) const {
	return { x * f, y * f, z * f, r * f };
}

template<typename T> constexpr quaternion<T> quaternion<T>::operator/(const quaternion& q) const {
	return *this * q.inverted();
}

template<typename T> constexpr quaternion<T> quaternion<T>::operator/(const T& f) const {
	return { x / f, y / f, z / f, r / f };
}

template<typename T> constexpr quaternion<T>& quaternion<T>::operator=(const quaternion& q) {
	x = q.x;
	y = q.y;
	z = q.z;
	r = q.r;
	return *this;
}

template<typename T> constexpr quaternion<T>& quaternion<T>::operator+=(const quaternion& q) {
	x += q.x;
	y += q.y;
	z += q.z;
	r += q.r;
	return *this;
}

template<typename T> constexpr quaternion<T>& quaternion<T>::operator-=(const quaternion& q) {
	x -= q.x;
	y -= q.y;
	z -= q.z;
	r -= q.r;
	return *this;
}

template<typename T> constexpr quaternion<T>& quaternion<T>::operator*=(const quaternion& q) {
	*this = *this * q;
	return *this;
}

template<typename T> constexpr quaternion<T>& quaternion<T>::operator*=(const T& f) {
	*this = *this * f;
	return *this;
}

template<typename T> constexpr quaternion<T>& quaternion<T>::operator/=(const quaternion& q) {
	*this = *this * q.inverted();
  	return *this;
}

template<typename T> constexpr quaternion<T>& quaternion<T>::operator/=(const T& f) {
	*this = *this / f;
	return *this;
}

template<typename T> constexpr T quaternion<T>::magnitude() const {
	return const_select::sqrt(r * r + x * x + y * y + z * z);
}

template<typename T> constexpr quaternion<T> quaternion<T>::inverted() const {
	return conjugated() / (r * r + x * x + y * y + z * z);
}

template<typename T> constexpr quaternion<T> quaternion<T>::conjugated() const {
	return { -x, -y, -z, r };
}

template<typename T> constexpr quaternion<T> quaternion<T>::normalized() const {
	return *this / magnitude();
}

template<typename T> constexpr quaternion<T>& quaternion<T>::invert() {
	*this = conjugated() / (r * r + x * x + y * y + z * z);
	return *this;
}

template<typename T> constexpr quaternion<T>& quaternion<T>::conjugate() {
	x = -x;
	y = -y;
	z = -z;
	return *this;
}

template<typename T> constexpr quaternion<T>& quaternion<T>::normalize() {
	*this /= magnitude();
	return *this;
}

template<typename T> constexpr quaternion<T>& quaternion<T>::compute_r() {
	const T val = ((T)1) - (x * x) - (y * y) - (z * z);
	r = (val < ((T)0) ? ((T)0) : -const_select::sqrt(val));
	return *this;
}

template<typename T> constexpr vector3<T> quaternion<T>::rotate(const vector3<T>& v) const {
	const auto qvec2 = *this * quaternion<T> { v.x, v.y, v.z, (T)0 } * conjugated();
	return { qvec2.x, qvec2.y, qvec2.z };
}

template<typename T> constexpr quaternion<T>& quaternion<T>::set_rotation(const T& a, const vector3<T>& v) {
	const auto angle = a * const_math::PI_DIV_360<T>;
	const auto sin_a = const_select::sin(angle);
	const auto cos_a = const_select::cos(angle);
	const auto nv_sin = v.normalized() * sin_a;
	x = nv_sin.x;
	y = nv_sin.y;
	z = nv_sin.z;
	r = cos_a;
	return *this;
}

template<typename T> constexpr quaternion<T>& quaternion<T>::set_rotation(const T& a, const T& i, const T& j, const T& k) {
	return set_rotation(a, vector3<T> { i, j, k });
}

template<typename T> constexpr vector3<T> quaternion<T>::to_euler() const {
	// http://en.wikipedia.org/wiki/Conversion_between_quaternions_and_Euler_angles#Conversion
	return {
		const_select::atan2(((T)2) * (r * x + y * z), ((T)1) - ((T)2) * (x * x + y * y)),
		const_select::asin(((T)2) * (r * y - z * x)),
		const_select::atan2(((T)2) * (r * z + x * y), ((T)1) - ((T)2) * (y * y + z * z))
	};
}

template<typename T> constexpr matrix4<T> quaternion<T>::to_matrix4() const {
	// http://www.euclideanspace.com/maths/geometry/rotations/conversions/quaternionToMatrix/index.htm
	const auto xx = x * x;
	const auto yy = y * y;
	const auto zz = z * z;
	
	return {
		((T)1) - ((T)2) * yy - ((T)2) * zz, ((T)2) * (x * y - z * r), ((T)2) * (x * z + y * r), ((T)0),
		((T)2) * (x * y + z * r), ((T)1) - ((T)2) * xx - ((T)2) * zz, ((T)2) * (y * z - x * r), ((T)0),
		((T)2) * (x * z - y * r), ((T)2) * (y * z + x * r), ((T)1) - ((T)2) * xx - ((T)2) * yy, ((T)0),
		((T)0), ((T)0), ((T)0), ((T)1)
	};
}

template<typename T> constexpr quaternion<T>& quaternion<T>::from_euler(const vector3<T>& v) {
	x = const_select::sin((v.x - v.z) / ((T)2)) / const_select::sin(v.y / ((T)2));
	y = -const_select::sin((v.x + v.z) / ((T)2)) / const_select::cos(v.y / ((T)2));
	z = const_select::cos((v.x + v.z) / ((T)2)) / const_select::cos(v.y / ((T)2));
	r = -const_select::cos((v.x - v.z) / ((T)2)) / const_select::sin(v.y / ((T)2));
	return *this;
}

#if defined(FLOOR_EXPORT)
// only instantiate this in the quaternion.cpp
extern template class quaternion<float>;
extern template class quaternion<double>;
#endif
		
#if defined(__clang__)
#pragma clang diagnostic pop
#endif

#endif
