/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2019 Florian Ziesche
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

#ifndef __FLOOR_UNICODE_HPP__
#define __FLOOR_UNICODE_HPP__

#include <vector>
#include <string>
#include <utility>
#include <cstdint>
using namespace std;

//! unicode routines
namespace unicode {
	vector<unsigned int> utf8_to_unicode(const string& str);
	string unicode_to_utf8(const vector<unsigned int>& codes);
	
	//! decodes a single multi-byte utf-8 character to a utf-32/32-bit uint,
	//! returns <false, 0> if the utf-8 code point is invalid and <true, utf-32 code> if valid
	//! NOTE: the specified 'iter' is advanced while doing this (will point to the last code point byte)
	pair<bool, uint32_t> decode_utf8_char(const char*& iter, const char* const& end_iter);
	pair<bool, uint32_t> decode_utf8_char(string::const_iterator& iter, const string::const_iterator& end_iter);
	
	//! checks if a string is a valid utf-8 string, returns <true, cend> if valid
	//! and <false, iterator to invalid code sequence> if invalid
	pair<bool, string::const_iterator> validate_utf8_string(const string& str);
	
#if defined(__APPLE__)
	string utf8_decomp_to_precomp(const string& str);
#endif
	
} // unicode

#endif
