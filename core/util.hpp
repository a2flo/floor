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

#ifndef __FLOOR_UTIL_HPP__
#define __FLOOR_UTIL_HPP__

#include <floor/core/cpp_headers.hpp>

// misc
#if !defined(FLOOR_NO_EXCEPTIONS)
class floor_exception : public exception {
protected:
	string error_str;
public:
	floor_exception(const string& error_str_) : error_str(error_str_) {}
	virtual const char* what() const noexcept;
};
#endif

#endif
