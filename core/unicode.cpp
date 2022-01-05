/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2022 Florian Ziesche
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
#include <floor/darwin/darwin_helper.hpp>
#endif

namespace unicode {

template <typename iter_type>
static pair<bool, uint32_t> gen_iter_decode_utf8_char(iter_type& iter, const iter_type& end_iter) {
	// figure out how long the utf-8 char is (how many bytes)
	uint32_t size = 0;
	const uint32_t char_code = ((uint32_t)*iter) & 0xFFu;
	while((char_code & (1u << (7u - size))) != 0u) {
		++size;
	}
	
	// single "ASCII" byte
	if(size == 0u) return { true, char_code & 0x7Fu };
	// invalid since RFC 3629 (5 and 6 bytes long)
	else if(size >= 5u) return { false, 0u };
	// else: valid
	
	// AND lower (7 - size) bits of the first character
	uint32_t cur_code = char_code & ((1u << (7u - size)) - 1u);
	
	--size;
	cur_code <<= size * 6; // shift up
	for(uint32_t i = 0; i < size; i++) {
		// advance iter + check if iterator is past the end
		++iter;
		if(iter == end_iter) {
			return { false, 0u };
		}
		const auto cur_char_code = (uint32_t)*iter;
		
		// must be 0b10xxxxxx
		if((cur_char_code & 0x80u) != 0x80u) {
			return { false, 0u };
		}
		
		cur_code += (cur_char_code & 0x3Fu) << ((size - i - 1u) * 6u);
	}
	
	if(cur_code > 0x10FFFFu) {
		// invalid unicode range
		return { false, 0u };
	}
	return { true, cur_code };
}

pair<bool, uint32_t> decode_utf8_char(const char*& iter, const char* const& end_iter) {
	return gen_iter_decode_utf8_char(iter, end_iter);
}

pair<bool, uint32_t> decode_utf8_char(string::const_iterator& iter, const string::const_iterator& end_iter) {
	return gen_iter_decode_utf8_char(iter, end_iter);
}

vector<uint32_t> utf8_to_unicode(const string& str) {
	vector<uint32_t> ret;
	
	const auto end_iter = str.cend();
	for(auto iter = str.cbegin(); iter != end_iter; ++iter) {
		const auto code = decode_utf8_char(iter, end_iter);
		if(!code.first) return ret;
		ret.emplace_back(code.second);
	}
	
	return ret;
}

string unicode_to_utf8(const vector<uint32_t>& codes) {
	string ret;
	for(const auto& code : codes) {
		if((code & 0xFFFFFF80u) == 0u) {
			// ascii char, only accept 0x09 (tab) and 0x20 to 0x7F (0x00 to 0x1F are used as control bytes)
			if(code >= 0x20u || code == 0x09u) {
				ret += (char)(code & 0xFFu);
			}
		}
		else {
			// unicode char, convert to utf-8
			if(code >= 0x80u && code <= 0x7FFu) {
				// unicode: 00000yyy xxxxxxxx
				// uft-8  : 110yyyxx 10xxxxxx
				ret += (char)(0xC0u | ((code & 0x7C0u) >> 6u));
				ret += (char)(0x80u | (code & 0x3Fu));
			}
			else if(code >= 0x800u && code <= 0xFFFFu) {
				// unicode: yyyyyyyy xxxxxxxx
				// uft-8  : 1110yyyy 10yyyyxx 10xxxxxx
				ret += (char)(0xE0u | ((code & 0xF000u) >> 12u));
				ret += (char)(0x80u | ((code & 0xFC0u) >> 6u));
				ret += (char)(0x80u | (code & 0x3Fu));
			}
			else if(code >= 0x10000u && code <= 0x1FFFFFu) {
				// unicode: 000zzzzz yyyyyyyy xxxxxxxx
				// uft-8  : 11110zzz 10zzyyyy 10yyyyxx 10xxxxxx
				ret += (char)(0xF0u | ((code & 0x1C0000u) >> 18u));
				ret += (char)(0x80u | ((code & 0x3F000u) >> 12u));
				ret += (char)(0x80u | ((code & 0xFC0u) >> 6u));
				ret += (char)(0x80u | (code & 0x3Fu));
			}
			else {
				// invalid -> abort
				return ret;
			}
		}
	}
	return ret;
}

pair<bool, string::const_iterator> validate_utf8_string(const string& str) {
	const auto end_iter = str.cend();
	for(auto iter = str.cbegin(); iter != end_iter; ++iter) {
		const auto code = decode_utf8_char(iter, end_iter);
		if(!code.first) {
			return { false, iter };
		}
	}
	return { true, str.cend() };
}

#if defined(__APPLE__)
string utf8_decomp_to_precomp(const string& str) {
	return darwin_helper::utf8_decomp_to_precomp(str);
}
#endif

} // unicode
