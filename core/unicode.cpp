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

#include <floor/core/unicode.hpp>

#if defined(__APPLE__)
#if !defined(FLOOR_IOS)
#include <floor/osx/osx_helper.hpp>
#else
#include <floor/ios/ios_helper.hpp>
#endif
#endif

vector<unsigned int> unicode::utf8_to_unicode(const string& str) {
	vector<unsigned int> ret;
	
	for(auto iter = str.cbegin(); iter != str.cend(); iter++) {
		unsigned int size = 0;
		const unsigned int char_code = ((unsigned int)*iter) & 0xFF;
		while((char_code & (1 << (7 - size))) != 0) {
			size++;
		}
		
		if(size == 0) {
			ret.emplace_back(char_code & 0x7F);
			continue;
		}
		else if(size >= 5) {
			// invalid -> abort
			return ret;
		}
		
		// AND lower (7 - size) bits of the first character
		unsigned int cur_code = char_code & ((1 << (7 - size)) - 1);
		
		size--;
		cur_code <<= size * 6; // shift up
		
		for(unsigned int i = 0; i < size; i++) {
			cur_code += (((unsigned int)*++iter) & 0x3F) << ((size - i - 1) * 6);
		}
		if(cur_code > 0x10FFFF) {
			// invalid -> abort
			return ret;
		}
		ret.emplace_back(cur_code);
	}
	
	return ret;
}

string unicode::unicode_to_utf8(const vector<unsigned int>& codes) {
	string ret = "";
	for(const auto& code : codes) {
		if((code & 0xFFFFFF80) == 0) {
			// ascii char, only accept 0x09 (tab) and 0x20 to 0x7F (0x00 to 0x1F are used as control bytes)
			if(code >= 0x20 || code == 0x09) {
				ret += (char)(code & 0xFF);
			}
		}
		else {
			// unicode char, convert to utf-8
			if(code >= 0x80 && code <= 0x7FF) {
				// unicode: 00000yyy xxxxxxxx
				// uft-8  : 110yyyxx 10xxxxxx
				ret += (char)(0xC0 | ((code & 0x7C0) >> 6));
				ret += (char)(0x80 | (code & 0x3F));
			}
			else if(code >= 0x800 && code <= 0xFFFF) {
				// unicode: yyyyyyyy xxxxxxxx
				// uft-8  : 1110yyyy 10yyyyxx 10xxxxxx
				ret += (char)(0xE0 | ((code & 0xF000) >> 12));
				ret += (char)(0x80 | ((code & 0xFC0) >> 6));
				ret += (char)(0x80 | (code & 0x3F));
			}
			else if(code >= 0x10000 && code <= 0x1FFFFF) {
				// unicode: 000zzzzz yyyyyyyy xxxxxxxx
				// uft-8  : 11110zzz 10zzyyyy 10yyyyxx 10xxxxxx
				ret += (char)(0xF0 | ((code & 0x1C0000) >> 18));
				ret += (char)(0x80 | ((code & 0x3F000) >> 12));
				ret += (char)(0x80 | ((code & 0xFC0) >> 6));
				ret += (char)(0x80 | (code & 0x3F));
			}
			else {
				// invalid -> abort
				return ret;
			}
		}
	}
	return ret;
}

#if defined(__APPLE__)
string unicode::utf8_decomp_to_precomp(const string& str) {
#if !defined(FLOOR_IOS)
	return osx_helper::utf8_decomp_to_precomp(str);
#else
	return ios_helper::utf8_decomp_to_precomp(str);
#endif
}
#endif
