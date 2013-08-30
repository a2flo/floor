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

#ifndef __FLOOR_CONST_STRING_HPP__
#define __FLOOR_CONST_STRING_HPP__

#include <type_traits>
#include "core/platform.h"

// NOTE: unless you have a lot of RAM and time to spare,
// don't try this with strings that have a length > 10000.
// the technical limit should be around 30k, depending on
// the actual used strings and compiler limits

template <typename type, size_t count> struct const_basic_string {
protected:
	// final const_basic_string construction
	template<ssize_t len, ssize_t n0, ssize_t n1, ssize_t index0, ssize_t index1, typename... chs,
			 typename std::enable_if<index0 >= n0, int>::type = 0,
			 typename std::enable_if<index1 >= n1, int>::type = 0>
	static constexpr const_basic_string<type, len> concat_unpack(type*, type*, chs... chars) {
		return const_basic_string<type, len> {{ chars... }};
	}
	
	// flip: str_0 has been unpacked, run the same code with str_1 (unpack str_1)
	template<ssize_t len, ssize_t n0, ssize_t n1, ssize_t index0, ssize_t index1, typename... chs,
			 typename std::enable_if<index0 >= n0, int>::type = 0,
			 typename std::enable_if<index1 < n1, int>::type = 0>
	static constexpr const_basic_string<type, len> concat_unpack(type* str_0, type* str_1, chs... chars) {
		return concat_unpack<len, n1, n0, index1, index0>(str_1, str_0, chars...);
	}
	
	// str recursion
	// NOTE: to trim down on template recursion in general and to increase the max string size
	// before hitting the max template recursion/depth limit, directly process strings with
	// a length > 128 and/or 16 in chunks of 128 characters, then 16 characters at a time
	// for small strings and for the tail of large strings, simply use the +1 recursion method
	template<ssize_t len, ssize_t n0, ssize_t n1, ssize_t index0, ssize_t index1, typename... chs,
			 typename std::enable_if<index0 >= (n0-16), int>::type = 0,
			 typename std::enable_if<index0 < n0, int>::type = 0>
	static constexpr const_basic_string<type, len> concat_unpack(type* str_0, type* str_1, chs... chars) {
		return concat_unpack<len, n0, n1, index0+1, index1>(str_0, str_1, chars..., str_0[index0]);
	}
	
	template<ssize_t len, ssize_t n0, ssize_t n1, ssize_t index0, ssize_t index1, typename... chs,
			 typename std::enable_if<index0 >= (n0-128), int>::type = 0,
			 typename std::enable_if<index0 < (n0-16), int>::type = 0,
			 typename std::enable_if<index0 < n0, int>::type = 0>
	static constexpr const_basic_string<type, len> concat_unpack(type* str_0, type* str_1, chs... chars) {
		return concat_unpack<len, n0, n1, index0+16, index1>(str_0, str_1, chars...,
															 str_0[index0+0], str_0[index0+1], str_0[index0+2], str_0[index0+3], str_0[index0+4], str_0[index0+5], str_0[index0+6], str_0[index0+7],
															 str_0[index0+8], str_0[index0+9], str_0[index0+10], str_0[index0+11], str_0[index0+12], str_0[index0+13], str_0[index0+14], str_0[index0+15]);
	}
	
	template<ssize_t len, ssize_t n0, ssize_t n1, ssize_t index0, ssize_t index1, typename... chs,
			 typename std::enable_if<index0 < (n0-128), int>::type = 0,
			 typename std::enable_if<index0 < n0, int>::type = 0>
	static constexpr const_basic_string<type, len> concat_unpack(type* str_0, type* str_1, chs... chars) {
		return concat_unpack<len, n0, n1, index0+128, index1>(str_0, str_1, chars...,
															  str_0[index0+0], str_0[index0+1], str_0[index0+2], str_0[index0+3], str_0[index0+4], str_0[index0+5], str_0[index0+6], str_0[index0+7],
															  str_0[index0+8], str_0[index0+9], str_0[index0+10], str_0[index0+11], str_0[index0+12], str_0[index0+13], str_0[index0+14], str_0[index0+15],
															  str_0[index0+16], str_0[index0+17], str_0[index0+18], str_0[index0+19], str_0[index0+20], str_0[index0+21], str_0[index0+22], str_0[index0+23],
															  str_0[index0+24], str_0[index0+25], str_0[index0+26], str_0[index0+27], str_0[index0+28], str_0[index0+29], str_0[index0+30], str_0[index0+31],
															  str_0[index0+32], str_0[index0+33], str_0[index0+34], str_0[index0+35], str_0[index0+36], str_0[index0+37], str_0[index0+38], str_0[index0+39],
															  str_0[index0+40], str_0[index0+41], str_0[index0+42], str_0[index0+43], str_0[index0+44], str_0[index0+45], str_0[index0+46], str_0[index0+47],
															  str_0[index0+48], str_0[index0+49], str_0[index0+50], str_0[index0+51], str_0[index0+52], str_0[index0+53], str_0[index0+54], str_0[index0+55],
															  str_0[index0+56], str_0[index0+57], str_0[index0+58], str_0[index0+59], str_0[index0+60], str_0[index0+61], str_0[index0+62], str_0[index0+63],
															  str_0[index0+64], str_0[index0+65], str_0[index0+66], str_0[index0+67], str_0[index0+68], str_0[index0+69], str_0[index0+70], str_0[index0+71],
															  str_0[index0+72], str_0[index0+73], str_0[index0+74], str_0[index0+75], str_0[index0+76], str_0[index0+77], str_0[index0+78], str_0[index0+79],
															  str_0[index0+80], str_0[index0+81], str_0[index0+82], str_0[index0+83], str_0[index0+84], str_0[index0+85], str_0[index0+86], str_0[index0+87],
															  str_0[index0+88], str_0[index0+89], str_0[index0+90], str_0[index0+91], str_0[index0+92], str_0[index0+93], str_0[index0+94], str_0[index0+95],
															  str_0[index0+96], str_0[index0+97], str_0[index0+98], str_0[index0+99], str_0[index0+100], str_0[index0+101], str_0[index0+102], str_0[index0+103],
															  str_0[index0+104], str_0[index0+105], str_0[index0+106], str_0[index0+107], str_0[index0+108], str_0[index0+109], str_0[index0+110], str_0[index0+111],
															  str_0[index0+112], str_0[index0+113], str_0[index0+114], str_0[index0+115], str_0[index0+116], str_0[index0+117], str_0[index0+118], str_0[index0+119],
															  str_0[index0+120], str_0[index0+121], str_0[index0+122], str_0[index0+123], str_0[index0+124], str_0[index0+125], str_0[index0+126], str_0[index0+127]);
	}
	
public:
	const type content[count];
	constexpr const type* data() const {
		return content;
	}
	
	template <size_t n> constexpr const_basic_string<type, n+count-1> operator+(const const_basic_string<type, n>& str) const {
		// (count-1) because we have to trim the \0 from the first string
		return concat_unpack<n+count-1, count-1, n, 0, 0>(content, str.content);
	}
	template <size_t n> constexpr const_basic_string<type, n+count-1> operator+(const char (&str)[n]) const {
		// (count-1) because we have to trim the \0 from the first string
		return concat_unpack<n+count-1, count-1, n, 0, 0>(content, str);
	}
	template <size_t n> friend constexpr const_basic_string<type, n+count-1> operator+(const char (&str)[n], const const_basic_string<type, count>& cbstr) {
		return concat_unpack<n+count-1, n-1, count, 0, 0>(str, cbstr.content);
	}
};

//
template <size_t count> using const_string = const_basic_string<const char, count>;

// const_string construction helper
// usage: constexpr auto some_str = make_const_string("my compile time string");
struct const_string_helper {
protected:
	template<ssize_t n, ssize_t index, typename... chs,
			 typename std::enable_if<index >= n, int>::type = 0>
	static constexpr const_string<n> char_unpack(const char*, chs... chars) {
		return const_string<n> {{ chars... }};
	}
	
	template<ssize_t n, ssize_t index, typename... chs,
			 typename std::enable_if<index >= (n-16), int>::type = 0,
			 typename std::enable_if<index < n, int>::type = 0>
	static constexpr const_string<n> char_unpack(const char* str, chs... chars) {
		return char_unpack<n, index+1>(str, chars..., str[index]);
	}
	
	template<ssize_t n, ssize_t index, typename... chs,
			 typename std::enable_if<index >= (n-128), int>::type = 0,
			 typename std::enable_if<index < (n-16), int>::type = 0,
			 typename std::enable_if<index < n, int>::type = 0>
	static constexpr const_string<n> char_unpack(const char* str, chs... chars) {
		return char_unpack<n, index+16>(str, chars...,
										str[index+0], str[index+1], str[index+2], str[index+3], str[index+4], str[index+5], str[index+6], str[index+7],
										str[index+8], str[index+9], str[index+10], str[index+11], str[index+12], str[index+13], str[index+14], str[index+15]);
	}
	
	template<ssize_t n, ssize_t index, typename... chs,
			 typename std::enable_if<index < (n-128), int>::type = 0,
			 typename std::enable_if<index < n, int>::type = 0>
	static constexpr const_string<n> char_unpack(const char* str, chs... chars) {
		return char_unpack<n, index+128>(str, chars...,
										 str[index+0], str[index+1], str[index+2], str[index+3], str[index+4], str[index+5], str[index+6], str[index+7],
										 str[index+8], str[index+9], str[index+10], str[index+11], str[index+12], str[index+13], str[index+14], str[index+15],
										 str[index+16], str[index+17], str[index+18], str[index+19], str[index+20], str[index+21], str[index+22], str[index+23],
										 str[index+24], str[index+25], str[index+26], str[index+27], str[index+28], str[index+29], str[index+30], str[index+31],
										 str[index+32], str[index+33], str[index+34], str[index+35], str[index+36], str[index+37], str[index+38], str[index+39],
										 str[index+40], str[index+41], str[index+42], str[index+43], str[index+44], str[index+45], str[index+46], str[index+47],
										 str[index+48], str[index+49], str[index+50], str[index+51], str[index+52], str[index+53], str[index+54], str[index+55],
										 str[index+56], str[index+57], str[index+58], str[index+59], str[index+60], str[index+61], str[index+62], str[index+63],
										 str[index+64], str[index+65], str[index+66], str[index+67], str[index+68], str[index+69], str[index+70], str[index+71],
										 str[index+72], str[index+73], str[index+74], str[index+75], str[index+76], str[index+77], str[index+78], str[index+79],
										 str[index+80], str[index+81], str[index+82], str[index+83], str[index+84], str[index+85], str[index+86], str[index+87],
										 str[index+88], str[index+89], str[index+90], str[index+91], str[index+92], str[index+93], str[index+94], str[index+95],
										 str[index+96], str[index+97], str[index+98], str[index+99], str[index+100], str[index+101], str[index+102], str[index+103],
										 str[index+104], str[index+105], str[index+106], str[index+107], str[index+108], str[index+109], str[index+110], str[index+111],
										 str[index+112], str[index+113], str[index+114], str[index+115], str[index+116], str[index+117], str[index+118], str[index+119],
										 str[index+120], str[index+121], str[index+122], str[index+123], str[index+124], str[index+125], str[index+126], str[index+127]);
	}
	
public:
	template <ssize_t n> static constexpr const_string<n> make_const_string(const char (&str)[n]) {
		return char_unpack<n, 0>(str);
	}
};

template <size_t n> constexpr const_string<n> make_const_string(const char (&str)[n]) {
	return const_string_helper::make_const_string(str);
}

#endif
