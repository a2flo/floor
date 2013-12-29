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
#include "core/platform.hpp"

template <size_t count> struct const_string {
public:
	//! since we can't assign c arrays directly, do this via a helper class
	template <size_t n> struct storage_array {
		char data[n];
	};
	
	//! string content
	const storage_array<count> content;
	
	//! construct from c string (with size info)
	constexpr const_string(const char (&str)[count]) noexcept :
	content(make_array(str)) {}
	
	//! data access via a simple c string pointer
	constexpr const char* data() const { return &content.data[0]; }
	//! redundant size() function for usability (note that this of course also includes \0 chars)
	constexpr size_t size() const { return count; }
	
	//! concat operator: with another const_string
	template <size_t n> constexpr const_string<n + count - 1> operator+(const const_string<n>& str) const {
		return make_concat_array<n + count - 1>(count - 1, n, data(), str.data()).data;
	}
	//! concat operator: with another a c string
	template <size_t n> constexpr const_string<n + count - 1> operator+(const char (&str)[n]) const {
		return make_concat_array<n + count - 1>(count - 1, n, data(), str).data;
	}
	//! concat operator: of a c string with a const_string
	template <size_t n> friend constexpr const_string<n + count - 1> operator+(const char (&str)[n], const const_string<count>& cstr) {
		return make_concat_array<n + count - 1>(n - 1, count, str, cstr.data()).data;
	}
	
	//! prints/writes the content of the const_string to an ostream (note that this is obviously a non-constexpr function)
	template <size_t n> friend ostream& operator<<(ostream& output, const const_string<n>& cstr) {
		output.write(&cstr.content.data[0], n);
		return output;
	}
	
	//
	template <size_t n> constexpr bool operator==(const const_string<n>& cstr) const {
		if(n != count) return false;
		for(size_t i = 0; i < count; ++i) {
			if(content.data[i] != cstr.content.data[i]) {
				return false;
			}
		}
		return true;
	}
	template <size_t n> constexpr bool operator==(const char (&str)[n]) const {
		if(n != count) return false;
		for(size_t i = 0; i < count; ++i) {
			if(content.data[i] != str[i]) {
				return false;
			}
		}
		return true;
	}
	template <size_t n> constexpr bool operator==(const char* str) const {
		if(str == nullptr) return false;
		for(size_t i = 0; i < count; ++i) {
			if(content.data[i] != str[i]) {
				return false;
			}
			// check for end of str and if we have also reached the end for the const_string
			if(str[i] == '\0' && (i + 1) < count) {
				// not the end of const_string
				return false;
			}
		}
		// reached the end of const_string
		if(str[count - 1] != '\0') {
			// not the end of str
			return false;
		}
		return true;
	}
	bool operator==(const string& str) const {
		if(str.size() != count) return false;
		for(size_t i = 0; i < count; ++i) {
			if(content.data[i] != str[i]) {
				return false;
			}
		}
		return true;
	}
	template <size_t n> constexpr bool operator!=(const const_string<n>& cstr) const {
		return !(*this == cstr);
	}
	template <size_t n> constexpr bool operator!=(const char (&str)[n]) const {
		return !(*this == str);
	}
	template <size_t n> constexpr bool operator!=(const char* str) const {
		return !(*this == str);
	}
	bool operator!=(const string& str) const {
		return !(*this == str);
	}
	
	//
	template <size_t len_0, size_t len_1> friend constexpr bool operator==(const char (&str)[len_0], const const_string<len_1>& cstr) {
		return (cstr == str);
	}
	template <size_t len_0> friend constexpr bool operator==(const char* str, const const_string<len_0>& cstr) {
		return (cstr == str);
	}
	template <size_t len_0> friend bool operator==(const string& str, const const_string<len_0>& cstr) {
		return (cstr == str);
	}
	template <size_t len_0, size_t len_1> friend constexpr bool operator!=(const char (&str)[len_0], const const_string<len_1>& cstr) {
		return (cstr != str);
	}
	template <size_t len_0> friend constexpr bool operator!=(const char* str, const const_string<len_0>& cstr) {
		return (cstr != str);
	}
	template <size_t len_0> friend bool operator!=(const string& str, const const_string<len_0>& cstr) {
		return (cstr != str);
	}
	
protected:
	//! creates a storage_array from a c string
	template <size_t n> static constexpr storage_array<n> make_array(const char (&str)[n]) {
		storage_array<n> ret {};
		for(size_t i = 0; i < n; ++i) {
			ret.data[i] = str[i];
		}
		return ret;
	}
	
	//! creates a storage array from two c strings
	//! (note that len_0 should have been substracted by 1 beforehand, so that the \0 of the first string isn't copied)
	template <size_t n> static constexpr storage_array<n> make_concat_array(const size_t len_0, const size_t len_1,
																			const char* str_0, const char* str_1) {
		storage_array<n> ret {};
		size_t i = 0;
		for(; i < len_0; ++i) {
			ret.data[i] = str_0[i];
		}
		for(size_t j = 0; j < len_1; ++j, ++i) {
			ret.data[i] = str_1[j];
		}
		return ret;
	}
};

//! const_string udl for number literals only
template <char... chars> constexpr auto operator"" _cs() {
    return const_string<sizeof...(chars)> {{ chars... }};
}

//! create a const_string<*> from a c string (with size info)
template <size_t n> constexpr auto make_const_string(const char (&str)[n]) {
	return const_string<n> { str };
}
template <size_t n> constexpr auto CS(const char (&str)[n]) {
	return const_string<n> { str };
}

#endif
