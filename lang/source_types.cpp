/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2016 Florian Ziesche
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

#include <floor/lang/source_types.hpp>
#include <floor/core/platform.hpp>

bool source_range::operator==(const char* str) const {
	const char* cptr = str;
	for(auto iter = begin; ; ++iter, ++cptr) {
		// check if end of str
		if(*cptr == '\0') {
			return (iter == end);
		}
		// if thats not the case and iter is at end: #source_range < #str
		else if(iter == end) {
			return false;
		}
		else if(*iter != *cptr) {
			return false;
		}
	}
	floor_unreachable();
}

bool source_range::operator==(const char& ch) const {
	return (size() == 1 && *begin == ch);
}

bool source_range::operator!=(const char* str) const {
	return !(*this == str);
}

bool source_range::operator!=(const char& ch) const {
	return !(*this == ch);
}

bool source_range::equal(const char* str, const size_t& len) const {
	if(size() != len) return false;
	return (*this == str);
}

bool source_range::unequal(const char* str, const size_t& len) const {
	return !(equal(str, len));
}

string source_range::to_string() const {
	return string(begin, end);
}

size_t source_range::size() const {
	return (size_t)distance(begin, end);
}
