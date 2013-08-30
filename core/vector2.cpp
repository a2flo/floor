/*
 *  Flexible OpenCL Rasterizer (oclraster)
 *  Copyright (C) 2012 - 2013 Florian Ziesche
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

#include "vector2.h"

#if defined(OCLRASTER_EXPORT)
// instantiate
template class vector2<float>;
template class vector2<double>;
template class vector2<unsigned int>;
template class vector2<int>;
template class vector2<short>;
template class vector2<bool>;
template class vector2<size_t>;
template class vector2<ssize_t>;
#endif

//
template<> OCLRASTER_API float2& vector2<float>::floor() {
	x = floorf(x);
	y = floorf(y);
	return *this;
}

template<> OCLRASTER_API float2& vector2<float>::ceil() {
	x = ceilf(x);
	y = ceilf(y);
	return *this;
}

template<> OCLRASTER_API float2& vector2<float>::round() {
	x = roundf(x);
	y = roundf(y);
	return *this;
}

template<> OCLRASTER_API float2 vector2<float>::floored() const {
	return float2(::floorf(x), ::floorf(y));
}

template<> OCLRASTER_API float2 vector2<float>::ceiled() const {
	return float2(::ceilf(x), ::ceilf(y));
}

template<> OCLRASTER_API float2 vector2<float>::rounded() const {
	return float2(::roundf(x), ::roundf(y));
}

//
template<> OCLRASTER_API bool2& vector2<bool>::floor() {
	return *this;
}

template<> OCLRASTER_API bool2& vector2<bool>::ceil() {
	return *this;
}

template<> OCLRASTER_API bool2& vector2<bool>::round() {
	return *this;
}

template<> OCLRASTER_API bool2 vector2<bool>::floored() const {
	return *this;
}

template<> OCLRASTER_API bool2 vector2<bool>::ceiled() const {
	return *this;
}

template<> OCLRASTER_API bool2 vector2<bool>::rounded() const {
	return *this;
}

//
template<> OCLRASTER_API float2 vector2<float>::operator%(const float2& v) const {
	return float2(fmodf(x, v.x), fmodf(y, v.y));
}

template<> OCLRASTER_API float2& vector2<float>::operator%=(const float2& v) {
	x = fmodf(x, v.x);
	y = fmodf(y, v.y);
	return *this;
}

template<> OCLRASTER_API double2 vector2<double>::operator%(const double2& v) const {
	return double2(fmod(x, v.x), fmod(y, v.y));
}

template<> OCLRASTER_API double2& vector2<double>::operator%=(const double2& v) {
	x = fmod(x, v.x);
	y = fmod(y, v.y);
	return *this;
}

template<> OCLRASTER_API vector2<float> vector2<float>::abs() const {
	return vector2<float>(fabs(x), fabs(y));
}

template<> OCLRASTER_API vector2<bool> vector2<bool>::abs() const {
	return vector2<bool>(*this);
}
