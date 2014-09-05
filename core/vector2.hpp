/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2014 Florian Ziesche
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

#ifndef __FLOOR_VECTOR_2_HPP__
#define __FLOOR_VECTOR_2_HPP__

#include "core/cpp_headers.hpp"

template <typename T> class vector2;
typedef vector2<float> float2;
typedef vector2<double> double2;
typedef vector2<unsigned int> uint2;
typedef vector2<int> int2;
typedef vector2<short> short2;
typedef vector2<bool> bool2;
typedef vector2<size_t> size2;
typedef vector2<ssize_t> ssize2;

typedef vector2<unsigned int> pnt;
typedef vector2<int> ipnt;
typedef vector2<float> coord;

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wtautological-compare"
#pragma clang diagnostic ignored "-Wpacked"
#endif

template <typename T> class FLOOR_API __attribute__((packed, aligned(sizeof(T)))) vector2 {
public:
	union {
		T x, u;
	};
	union {
		T y, v;
	};
	
	constexpr vector2() noexcept : x((T)0), y((T)0) {}
	constexpr vector2(vector2<T>&& vec2) noexcept : x(vec2.x), y(vec2.y) {}
	constexpr vector2(const vector2<T>& vec2) noexcept : x(vec2.x), y(vec2.y) {}
	constexpr vector2(const pair<T, T>& vec2) noexcept : x(vec2.first), y(vec2.second) {}
	constexpr vector2(const T& vx, const T& vy) noexcept : x(vx), y(vy) {}
	constexpr vector2(const T& f) noexcept : x(f), y(f) {}
	template <typename U> constexpr vector2(const vector2<U>& vec2) noexcept : x((T)vec2.x), y((T)vec2.y) {}
	
	vector2& operator=(const vector2& vec2) {
		this->x = vec2.x;
		this->y = vec2.y;
		return *this;
	}
	
	T& operator[](size_t index) const {
		return ((T*)this)[index];
	}
	
	friend ostream& operator<<(ostream& output, const vector2<T>& vec2) {
		output << "(" << vec2.x << ", " << vec2.y << ")";
		return output;
	}
	string to_string() const {
		stringstream sstr;
		sstr << *this;
		return sstr.str();
	}
	
	void set(const T& vx, const T& vy) {
		x = vx;
		y = vy;
	}
	void set(const vector2& vec2) {
		x = vec2.x;
		y = vec2.y;
	}
	
	vector2& floor();
	vector2& ceil();
	vector2& round();
	vector2& normalize();
	vector2& perpendicular();
	vector2 floored() const;
	vector2 ceiled() const;
	vector2 rounded() const;
	vector2 normalized() const;
	vector2 perpendiculared() const;
	
	friend vector2 operator*(const T& f, const vector2& v) {
		return vector2<T>(f * v.x, f * v.y);
	}
	vector2 operator*(const T& f) const {
		return vector2<T>(f * this->x, f * this->y);
	}
	vector2 operator/(const T& f) const {
		return vector2<T>(this->x / f, this->y / f);
	}
	
	vector2 operator-(const vector2<T>& vec2) const {
		return vector2<T>(this->x - vec2.x, this->y - vec2.y);
	}
	vector2 operator+(const vector2<T>& vec2) const {
		return vector2<T>(this->x + vec2.x, this->y + vec2.y);
	}
	vector2 operator*(const vector2<T>& vec2) const {
		return vector2<T>(this->x * vec2.x, this->y * vec2.y);
	}
	vector2 operator/(const vector2<T>& vec2) const {
		return vector2<T>(this->x / vec2.x, this->y / vec2.y);
	}
	
	vector2& operator+=(const vector2& v) {
		*this = *this + v;
		return *this;
	}
	vector2& operator-=(const vector2& v) {
		*this = *this - v;
		return *this;
	}
	vector2& operator*=(const vector2& v) {
		*this = *this * v;
		return *this;
	}
	vector2& operator/=(const vector2& v) {
		*this = *this / v;
		return *this;
	}
	vector2 operator%(const vector2& v) const;
	vector2& operator%=(const vector2& v);
	
	T length() const {
		return (T)sqrt(dot());
	}

	T distance(const vector2<T>& vec2) const {
		return (vec2 - *this).length();
	}
	
	T dot(const vector2<T>& vec2) const {
		return x*vec2.x + y*vec2.y;
	}
	T dot() const {
		return dot(*this);
	}
	
	bool is_null() const;
	bool is_nan() const;
	bool is_inf() const;
	bool is_equal(const vector2& v) const;
	bool is_equal(const vector2& v, const float epsilon) const;
	
	vector2 abs() const;
	
	vector2<T>& clamp(const T& vmin, const T& vmax) {
		x = (x < vmin ? vmin : (x > vmax ? vmax : x));
		y = (y < vmin ? vmin : (y > vmax ? vmax : y));
		return *this;
	}
	vector2<T>& clamp(const vector2<T>& vmin, const vector2<T>& vmax) {
		x = (x < vmin.x ? vmin.x : (x > vmax.x ? vmax.x : x));
		y = (y < vmin.y ? vmin.y : (y > vmax.y ? vmax.y : y));
		return *this;
	}
	vector2<T> sign() const {
		return vector2<T>(x < 0.0f ? -1.0f : 1.0f, y < 0.0f ? -1.0f : 1.0f);
	}
	
	// a component-wise minimum between two vector2s
	[[deprecated]] static const vector2 min(const vector2& v1, const vector2& v2) {
		return vector2(std::min(v1.x, v2.x), std::min(v1.y, v2.y));
	}
	
	// a component-wise maximum between two vector2s
	[[deprecated]] static const vector2 max(const vector2& v1, const vector2& v2) {
		return vector2(std::max(v1.x, v2.x), std::max(v1.y, v2.y));
	}
	
	//
	bool2 operator==(const vector2& v) const {
		return bool2(this->x == v.x, this->y == v.y);
	}
	
	// note: the following are only enabled for bool2
	template<class B = T, class = typename enable_if<is_same<B, bool>::value>::type>
	bool2 operator&(const bool2& bv) const {
		return bool2(x && bv.x, y && bv.y);
	}
	template<class B = T, class = typename enable_if<is_same<B, bool>::value>::type>
	bool2 operator|(const bool2& bv) const {
		return bool2(x || bv.x, y || bv.y);
	}
	template<class B = T, class = typename enable_if<is_same<B, bool>::value>::type>
	bool2 operator^(const bool& bl) const {
		return bool2(x ^ bl, y ^ bl);
	}
	template<class B = T, class = typename enable_if<is_same<B, bool>::value>::type>
	bool any() const {
		return (x || y);
	}
	template<class B = T, class = typename enable_if<is_same<B, bool>::value>::type>
	bool all() const {
		return (x && y);
	}
	
};

struct rect {
	union {
		struct {
			unsigned int x1;
			unsigned int y1;
			unsigned int x2;
			unsigned int y2;
		};
	};
	
	void set(const unsigned int& x1_, const unsigned int& y1_, const unsigned int& x2_, const unsigned int& y2_) {
		x1 = x1_; y1 = y1_; x2 = x2_; y2 = y2_;
	}
	
	friend ostream& operator<<(ostream& output, const rect& r) {
		output << "(" << r.x1 << ", " << r.y1 << ") x (" << r.x2 << ", " << r.y2 << ")";
		return output;
	}
	
	rect() : x1(0), y1(0), x2(0), y2(0) {}
	rect(const rect& r) : x1(r.x1), y1(r.y1), x2(r.x2), y2(r.y2) {}
	rect(const unsigned int& x1_, const unsigned int& y1_, const unsigned int& x2_, const unsigned int& y2_) : x1(x1_), y1(y1_), x2(x2_), y2(y2_) {}
	~rect() {}
};

template<> vector2<float> vector2<float>::operator%(const vector2& v) const; // specialize for float (mod)
template<> vector2<double> vector2<double>::operator%(const vector2& v) const; // specialize for double (mod)
template<typename T> vector2<T> vector2<T>::operator%(const vector2<T>& v) const {
	return vector2<T>(x % v.x, y % v.y);
}

template<> vector2<float>& vector2<float>::operator%=(const vector2& v); // specialize for float (mod)
template<> vector2<double>& vector2<double>::operator%=(const vector2& v); // specialize for double (mod)
template<typename T> vector2<T>& vector2<T>::operator%=(const vector2<T>& v) {
	x %= v.x;
	y %= v.y;
	return *this;
}

template<> vector2<bool>& vector2<bool>::floor();
template<> vector2<float>& vector2<float>::floor();
template<typename T> vector2<T>& vector2<T>::floor() {
	x = (T)::floor(x);
	y = (T)::floor(y);
	return *this;
}

template<> vector2<bool>& vector2<bool>::ceil();
template<> vector2<float>& vector2<float>::ceil();
template<typename T> vector2<T>& vector2<T>::ceil() {
	x = (T)::ceil(x);
	y = (T)::ceil(y);
	return *this;
}

template<> vector2<bool>& vector2<bool>::round();
template<> vector2<float>& vector2<float>::round();
template<typename T> vector2<T>& vector2<T>::round() {
	x = (T)::round(x);
	y = (T)::round(y);
	return *this;
}

template<typename T> vector2<T>& vector2<T>::normalize() {
	if(!is_null()) {
		*this = *this / length();
	}
	return *this;
}

template<typename T> vector2<T>& vector2<T>::perpendicular() {
	const T tmp = x;
	x = -y;
	y = tmp;
	return *this;
}

template<> vector2<bool> vector2<bool>::floored() const;
template<> vector2<float> vector2<float>::floored() const;
template<typename T> vector2<T> vector2<T>::floored() const {
	return vector2<T>((T)::floor(x), (T)::floor(y));
}

template<> vector2<bool> vector2<bool>::ceiled() const;
template<> vector2<float> vector2<float>::ceiled() const;
template<typename T> vector2<T> vector2<T>::ceiled() const {
	return vector2<T>((T)::ceil(x), (T)::ceil(y));
}

template<> vector2<bool> vector2<bool>::rounded() const;
template<> vector2<float> vector2<float>::rounded() const;
template<typename T> vector2<T> vector2<T>::rounded() const {
	return vector2<T>((T)::round(x), (T)::round(y));
}
template<typename T> vector2<T> vector2<T>::normalized() const {
	vector2 ret;
	if(!is_null()) {
		ret = *this / length();
	}
	return ret;
}
template<typename T> vector2<T> vector2<T>::perpendiculared() const {
	return vector2<T>((T)-y, (T)x);
}

template<typename T> bool vector2<T>::is_null() const {
	return (this->x == (T)0 && this->y == (T)0 ? true : false);
}

template<typename T> bool vector2<T>::is_nan() const {
	if(!numeric_limits<T>::has_quiet_NaN) return false;
	
	const T nan(numeric_limits<T>::quiet_NaN());
	if(x == nan || y == nan) {
		return true;
	}
	return false;
}

template<typename T> bool vector2<T>::is_inf() const {
	if(!numeric_limits<T>::has_infinity) return false;
	
	const T inf(numeric_limits<T>::infinity());
	if(x == inf || x == -inf || y == inf || y == -inf) {
		return true;
	}
	return false;
}

template<typename T> bool vector2<T>::is_equal(const vector2& v) const {
	if(this->x == v.x && this->y == v.y) {
		return true;
	}
	return false;
}

template<typename T> bool vector2<T>::is_equal(const vector2& v, const float epsilon) const {
	if((v.x - epsilon < x && x < v.x + epsilon) &&
	   (v.y - epsilon < y && y < v.y + epsilon)) {
		return true;
	}
	return false;
}

template<> vector2<float> vector2<float>::abs() const; // specialize for float
template<> vector2<bool> vector2<bool>::abs() const; // specialize for bool, no need for abs here
template<typename T> vector2<T> vector2<T>::abs() const {
	return vector2<T>((T)::llabs((long long int)x), (T)::llabs((long long int)y));
}

#if defined(FLOOR_EXPORT)
// only instantiate this in the vector2.cpp
extern template class vector2<float>;
extern template class vector2<double>;
extern template class vector2<unsigned int>;
extern template class vector2<int>;
extern template class vector2<short>;
extern template class vector2<bool>;
extern template class vector2<size_t>;
extern template class vector2<ssize_t>;
#endif

#if defined(__clang__)
#pragma clang diagnostic pop
#endif

#endif
