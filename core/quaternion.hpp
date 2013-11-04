/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2013 Florian Ziesche
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

#include "core/cpp_headers.hpp"
#include "core/vector3.hpp"

template <typename T> class quaternion;
typedef quaternion<float> quaternionf;
typedef quaternion<double> quaterniond;

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wpacked"
#endif

template <typename T> class FLOOR_API __attribute__((packed, aligned(4))) quaternion {
public:
	T x, y, z, r;
	constexpr quaternion() noexcept : x(0.0f), y(0.0f), z(0.0f), r(1.0f) {}
	constexpr quaternion(const T& x_, const T& y_, const T& z_, const T& r_) noexcept : x(x_), y(y_), z(z_), r(r_) {}
	constexpr quaternion(const quaternion& q) noexcept : x(q.x), y(q.y), z(q.z), r(q.r) {}
	quaternion(const T& a, const vector3<T>& v) noexcept { set_rotation(a, v); }
	
	//
	friend ostream& operator<<(ostream& output, const quaternion& q) {
		output << "(" << q.r << ": " << q.x << ", " << q.y << ", " << q.z << ")";
		return output;
	}
	string to_string() const {
		stringstream sstr;
		sstr << *this;
		return sstr.str();
	}
	
	quaternion operator+(const quaternion& q) const;
	quaternion operator-(const quaternion& q) const;
	quaternion operator*(const quaternion& q) const;
	quaternion operator*(const T& f) const;
	quaternion operator/(const quaternion& q) const;
	quaternion operator/(const T& f) const;
	quaternion& operator=(const quaternion& q);
	quaternion& operator+=(const quaternion& q);
	quaternion& operator-=(const quaternion& q);
	quaternion& operator*=(const quaternion& q);
	quaternion& operator*=(const T& f);
	quaternion& operator/=(const quaternion& q);
	quaternion& operator/=(const T& f);
	
	//
	T magnitude() const;
	quaternion inverted() const;
	quaternion conjugated() const;
	quaternion normalized() const;
	
	void invert();
	void conjugate();
	void normalize();
	void compute_r();
	
	vector3<T> rotate(const vector3<T>& v) const;
	void set_rotation(const T& a, const vector3<T>& v);
	void set_rotation(const T& a, const T& i, const T& j, const T& k);
	
	//
	vector3<T> to_euler() const;
	matrix4<T> to_matrix4() const;
	void from_euler(const vector3<T>& v);
};


template<typename T> quaternion<T> quaternion<T>::operator+(const quaternion& q) const {
	return quaternion<T>(x + q.x, y + q.y, z + q.z, r + q.r);
}

template<typename T> quaternion<T> quaternion<T>::operator-(const quaternion& q) const {
	return quaternion<T>(x - q.x, y - q.y, z - q.z, r - q.r);
}

template<typename T> quaternion<T> quaternion<T>::operator*(const quaternion& q) const {
	return quaternion<T>(r * q.x + x * q.r + y * q.z - z * q.y,
						 r * q.y - x * q.z + y * q.r + z * q.x,
						 r * q.z + x * q.y - y * q.x + z * q.r,
						 r * q.r - x * q.x - y * q.y - z * q.z);
}

template<typename T> quaternion<T> quaternion<T>::operator*(const T& f) const {
	return quaternion<T>(x * f, y * f, z * f, r * f);
}

template<typename T> quaternion<T> quaternion<T>::operator/(const quaternion& q) const {
	return *this * q.inverted();
}

template<typename T> quaternion<T> quaternion<T>::operator/(const T& f) const {
	return quaternion<T>(x / f, y / f, z / f, r / f);
}

template<typename T> quaternion<T>& quaternion<T>::operator=(const quaternion& q) {
	x = q.x;
	y = q.y;
	z = q.z;
	r = q.r;
	return *this;
}

template<typename T> quaternion<T>& quaternion<T>::operator+=(const quaternion& q) {
	x += q.x;
	y += q.y;
	z += q.z;
	r += q.r;
	return *this;
}

template<typename T> quaternion<T>& quaternion<T>::operator-=(const quaternion& q) {
	x -= q.x;
	y -= q.y;
	z -= q.z;
	r -= q.r;
	return *this;
}

template<typename T> quaternion<T>& quaternion<T>::operator*=(const quaternion& q) {
	*this = *this * q;
	return *this;
}

template<typename T> quaternion<T>& quaternion<T>::operator*=(const T& f) {
	*this = *this * f;
	return *this;
}

template<typename T> quaternion<T>& quaternion<T>::operator/=(const quaternion& q) {
	*this = *this * q.inverted();
  	return *this;
}

template<typename T> quaternion<T>& quaternion<T>::operator/=(const T& f) {
	*this = *this / f;
	return *this;
}

template<typename T> T quaternion<T>::magnitude() const {
	return (T)sqrt(r * r + x * x + y * y + z * z);
}

template<typename T> quaternion<T> quaternion<T>::inverted() const {
	return conjugated() / (r * r + x * x + y * y + z * z);
}

template<typename T> quaternion<T> quaternion<T>::conjugated() const {
	return quaternion(-x, -y, -z, r);
}

template<typename T> quaternion<T> quaternion<T>::normalized() const {
	return *this / magnitude();
}

template<typename T> void quaternion<T>::invert() {
	*this = conjugated() / (r * r + x * x + y * y + z * z);
}

template<typename T> void quaternion<T>::conjugate() {
	x = -x;
	y = -y;
	z = -z;
}

template<typename T> void quaternion<T>::normalize() {
	*this /= magnitude();
}

template<typename T> void quaternion<T>::compute_r() {
	const T val = ((T)1) - (x * x) - (y * y) - (z * z);
	r = (val < ((T)0) ? ((T)0) : (T)-sqrt(val));
}


template<typename T> vector3<T> quaternion<T>::rotate(const vector3<T>& v) const {
	const quaternion<T> qvec(v.x, v.y, v.z, (T)0);
	const quaternion<T> qinv(this->conjugated());
	const quaternion qvec2(*this * qvec * qinv);
	return vector3<T>(qvec2.x, qvec2.y, qvec2.z);
}

template<typename T> void quaternion<T>::set_rotation(const T& a, const vector3<T>& v) {
	const T angle = a * ((T)PI) / ((T)360);
	const T sin_a = (T)sin(angle);
	const T cos_a = (T)cos(angle);
	const vector3<T> nv(v.normalized());
	r = cos_a;
	x = nv.x * sin_a;
	y = nv.y * sin_a;
	z = nv.z * sin_a;
}

template<typename T> void quaternion<T>::set_rotation(const T& a, const T& i, const T& j, const T& k) {
	set_rotation(a, vector3<T>(i, j, k));
}

template<typename T> vector3<T> quaternion<T>::to_euler() const {
	// http://en.wikipedia.org/wiki/Conversion_between_quaternions_and_Euler_angles#Conversion
	return vector3<T>(atan2(((T)2) * (r * x + y * z), ((T)1) - ((T)2) * (x * x + y * y)),
					  asin(((T)2) * (r * y - z * x)),
					  atan2(((T)2) * (r * z + x * y), ((T)1) - ((T)2) * (y * y + z * z)));
}

template<typename T> matrix4<T> quaternion<T>::to_matrix4() const {
	// http://www.euclideanspace.com/maths/geometry/rotations/conversions/quaternionToMatrix/index.htm
	const T xx = x * x;
	const T yy = y * y;
	const T zz = z * z;
	
	return matrix4<T>(((T)1) - ((T)2) * yy - ((T)2) * zz, ((T)2) * (x * y - z * r), ((T)2) * (x * z + y * r), ((T)0),
					  ((T)2) * (x * y + z * r), ((T)1) - ((T)2) * xx - ((T)2) * zz, ((T)2) * (y * z - x * r), ((T)0),
					  ((T)2) * (x * z - y * r), ((T)2) * (y * z + x * r), ((T)1) - ((T)2) * xx - ((T)2) * yy, ((T)0),
					  ((T)0), ((T)0), ((T)0), ((T)1));
}

template<typename T> void quaternion<T>::from_euler(const vector3<T>& v) {
	x = sin((v.x - v.z) / ((T)2)) / sin(v.y / ((T)2));
	y = -sin((v.x + v.z) / ((T)2)) / cos(v.y / ((T)2));
	z = cos((v.x + v.z) / ((T)2)) / cos(v.y / ((T)2));
	r = -cos((v.x - v.z) / ((T)2)) / sin(v.y / ((T)2));
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
