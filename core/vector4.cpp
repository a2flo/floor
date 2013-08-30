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

#include "vector4.h"

#if defined(OCLRASTER_EXPORT)
template class vector4<float>;
template class vector4<unsigned int>;
template class vector4<int>;
template class vector4<short>;
template class vector4<char>;
template class vector4<unsigned short>;
template class vector4<unsigned char>;
template class vector4<bool>;
template class vector4<size_t>;
template class vector4<ssize_t>;
template class vector4<long long int>;
template class vector4<unsigned long long int>;
#endif

template<> OCLRASTER_API float4& vector4<float>::floor() {
	x = floorf(x);
	y = floorf(y);
	z = floorf(z);
	w = floorf(w);
	return *this;
}

template<> OCLRASTER_API float4& vector4<float>::ceil() {
	x = ceilf(x);
	y = ceilf(y);
	z = ceilf(z);
	w = ceilf(w);
	return *this;
}

template<> OCLRASTER_API float4& vector4<float>::round() {
	x = roundf(x);
	y = roundf(y);
	z = roundf(z);
	w = roundf(w);
	return *this;
}

template<> OCLRASTER_API float4 vector4<float>::floored() const {
	return float4(floorf(x), floorf(y), floorf(z), floorf(w));
}

template<> OCLRASTER_API float4 vector4<float>::ceiled() const {
	return float4(ceilf(x), ceilf(y), ceilf(z), ceilf(w));
}

template<> OCLRASTER_API float4 vector4<float>::rounded() const {
	return float4(roundf(x), roundf(y), roundf(z), roundf(w));
}

//
template<> OCLRASTER_API bool4& vector4<bool>::floor() {
	return *this;
}

template<> OCLRASTER_API bool4& vector4<bool>::ceil() {
	return *this;
}

template<> OCLRASTER_API bool4& vector4<bool>::round() {
	return *this;
}

template<> OCLRASTER_API bool4 vector4<bool>::floored() const {
	return *this;
}

template<> OCLRASTER_API bool4 vector4<bool>::ceiled() const {
	return *this;
}

template<> OCLRASTER_API bool4 vector4<bool>::rounded() const {
	return *this;
}
