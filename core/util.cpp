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

#include "util.h"
#include "platform.h"
#include "core/event_objects.h"

#if !defined(__WINDOWS__) || defined(WIN_UNIXENV)
template <> float converter<string, float>::convert(const string& var) {
	OCLRASTER_CONVERT_VAR_TO_BUFFER;
	return strtof(buffer.str().c_str(), nullptr);
}

template <> unsigned int converter<string, unsigned int>::convert(const string& var) {
	OCLRASTER_CONVERT_VAR_TO_BUFFER;
	return (unsigned int)strtoul(buffer.str().c_str(), nullptr, 10);
}

template <> int converter<string, int>::convert(const string& var) {
	OCLRASTER_CONVERT_VAR_TO_BUFFER;
	return (int)strtol(buffer.str().c_str(), nullptr, 10);
}

template <> bool converter<string, bool>::convert(const string& var) {
	return (var == "true" || var == "1" ? true : false);
}

#if !defined(WIN_UNIXENV)
template <> unsigned long long int converter<string, unsigned long long int>::convert(const string& var) {
	OCLRASTER_CONVERT_VAR_TO_BUFFER;
	return strtoull(buffer.str().c_str(), nullptr, 10);
}
#endif

#if defined(OCLRASTER_IOS)
template <> unsigned long int converter<string, unsigned long int>::convert(const string& var) {
	OCLRASTER_CONVERT_VAR_TO_BUFFER;
	return (unsigned long int)strtoull(buffer.str().c_str(), nullptr, 10);
}
#endif

#if defined(PLATFORM_X64)
template <> size_t converter<string, size_t>::convert(const string& var) {
	OCLRASTER_CONVERT_VAR_TO_BUFFER;
	return (size_t)strtoull(buffer.str().c_str(), nullptr, 10);
}

template <> ssize_t converter<string, ssize_t>::convert(const string& var) {
	OCLRASTER_CONVERT_VAR_TO_BUFFER;
	return (ssize_t)strtoll(buffer.str().c_str(), nullptr, 10);
}
#endif
#endif

const char* oclraster_exception::what() const throw () {
	return error_str.c_str();
}

// from event_objects.h:
EVENT_TYPE operator&(const EVENT_TYPE& e0, const EVENT_TYPE& e1) {
	return (EVENT_TYPE)((typename underlying_type<EVENT_TYPE>::type)e0 &
						(typename underlying_type<EVENT_TYPE>::type)e1);
}
