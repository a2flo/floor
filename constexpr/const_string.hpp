/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2025 Florian Ziesche
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

#pragma once

#include <type_traits>
#if !defined(FLOOR_COMPUTE) || (defined(FLOOR_COMPUTE_HOST) && !defined(FLOOR_COMPUTE_HOST_DEVICE))
#include <cstdlib>
#endif
#include <floor/core/essentials.hpp>

// NOTE: there now is a std proposal that would add something very similar to this
// to the c++ standard (-> if this gets approved and implemented, replace this code)
// http://isocpp.org/files/papers/n4121.pdf

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
	
	//! construct from storage container
	template <size_t size>
	constexpr const_string(const storage_array<size>& storage) noexcept :
	content(storage) {}
	
	//! data access via a simple c string pointer
	constexpr const char* data() const { return &content.data[0]; }
	//! redundant size() function for usability (note that this of course also includes \0 chars)
	constexpr size_t size() const { return count; }
	
	//! concat operator: with another const_string
	template <size_t n> constexpr const_string<n + count - 1> operator+(const const_string<n>& str) const {
		return make_concat_array<n + count - 1>(count - 1, n, data(), str.data()).data;
	}
	//! concat operator: with a c string
	template <size_t n> constexpr const_string<n + count - 1> operator+(const char (&str)[n]) const {
		return make_concat_array<n + count - 1>(count - 1, n, data(), str).data;
	}
	//! concat operator: of a c string with a const_string
	template <size_t n> friend constexpr const_string<n + count - 1> operator+(const char (&str)[n], const const_string<count>& cstr) {
		return make_concat_array<n + count - 1>(n - 1, count, str, cstr.data()).data;
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
	constexpr bool operator==(const char* str) const {
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
#if !defined(FLOOR_COMPUTE) || (defined(FLOOR_COMPUTE_HOST) && !defined(FLOOR_COMPUTE_HOST_DEVICE))
	bool operator==(const string& str) const {
		if(str.size() != count) return false;
		for(size_t i = 0; i < count; ++i) {
			if(content.data[i] != str[i]) {
				return false;
			}
		}
		return true;
	}
#endif
	template <size_t n> constexpr bool operator!=(const const_string<n>& cstr) const {
		return !(*this == cstr);
	}
	template <size_t n> constexpr bool operator!=(const char (&str)[n]) const {
		return !(*this == str);
	}
	constexpr bool operator!=(const char* str) const {
		return !(*this == str);
	}
#if !defined(FLOOR_COMPUTE) || (defined(FLOOR_COMPUTE_HOST) && !defined(FLOOR_COMPUTE_HOST_DEVICE))
	bool operator!=(const string& str) const {
		return !(*this == str);
	}
#endif
	
	//
	constexpr const char& operator[](const size_t& index) const {
		return content.data[index];
	}
	
	//! computes the hash of the const_string (32-bit MurmurHash3)
	constexpr uint32_t hash() const {
		// credits: https://smhasher.googlecode.com/svn/trunk/MurmurHash3.cpp
		// -> MurmurHash3_x86_32, modified for constexpr
		constexpr const uint32_t seed = 0xF1002A2Eu;
		constexpr const uint32_t c1 = 0xcc9e2d51u;
		constexpr const uint32_t c2 = 0x1b873593u;
		const size_t nblocks = count / (size_t)4;
		uint32_t h1 = seed;
		
		//----------
		// body
		size_t offset = 0;
		for(size_t i = 0; i < nblocks; ++i, offset += 4) {
			uint32_t k1 = (uint32_t(content.data[offset + 3] << 24u) |
						   uint32_t(content.data[offset + 2] << 16u) |
						   uint32_t(content.data[offset + 1] << 8u) |
						   uint32_t(content.data[offset + 0]));
			
			k1 *= c1;
			k1 = rotl32(k1, 15);
			k1 *= c2;
			
			h1 ^= k1;
			h1 = rotl32(h1, 13);
			h1 = h1 * 5 + 0xe6546b64;
		}
		
		//----------
		// tail
		uint32_t k1 = 0;
		switch(count & 3u) {
			case 3:
				k1 ^= (uint32_t)(content.data[offset + 2] << 16u);
			
			[[fallthrough]];
			case 2:
				k1 ^= (uint32_t)(content.data[offset + 1] << 8u);
				
			[[fallthrough]];
			case 1:
				k1 ^= (uint32_t)(content.data[offset + 0]);
				k1 *= c1;
				k1 = rotl32(k1, 15);
				k1 *= c2;
				h1 ^= k1;
				break;
			
			case 0: break;
		}
		
		//----------
		// finalization
		h1 ^= (uint32_t)count;
		h1 ^= h1 >> 16;
		h1 *= 0x85ebca6b;
		h1 ^= h1 >> 13;
		h1 *= 0xc2b2ae35;
		h1 ^= h1 >> 16;
		return h1;
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
	
	//! hash computation helper function
	static constexpr uint32_t rotl32(uint32_t x, int r) {
		return (x << r) | (x >> (32 - r));
	}
};

#if !defined(FLOOR_COMPUTE) || (defined(FLOOR_COMPUTE_HOST) && !defined(FLOOR_COMPUTE_HOST_DEVICE))
//! prints/writes the content of the const_string to an ostream (note that this is obviously a non-constexpr function)
template <size_t n> ostream& operator<<(ostream& output, const const_string<n>& cstr) {
	output.write(&cstr.content.data[0], n);
	return output;
}
#endif

//
template <size_t len_0, size_t len_1> constexpr bool operator==(const char (&str)[len_0], const const_string<len_1>& cstr) {
	return (cstr == str);
}
template <size_t len_0> constexpr bool operator==(const char* str, const const_string<len_0>& cstr) {
	return (cstr == str);
}
#if !defined(FLOOR_COMPUTE) || (defined(FLOOR_COMPUTE_HOST) && !defined(FLOOR_COMPUTE_HOST_DEVICE))
template <size_t len_0> bool operator==(const string& str, const const_string<len_0>& cstr) {
	return (cstr == str);
}
#endif
template <size_t len_0, size_t len_1> constexpr bool operator!=(const char (&str)[len_0], const const_string<len_1>& cstr) {
	return (cstr != str);
}
template <size_t len_0> constexpr bool operator!=(const char* str, const const_string<len_0>& cstr) {
	return (cstr != str);
}
#if !defined(FLOOR_COMPUTE) || (defined(FLOOR_COMPUTE_HOST) && !defined(FLOOR_COMPUTE_HOST_DEVICE))
template <size_t len_0> bool operator!=(const string& str, const const_string<len_0>& cstr) {
	return (cstr != str);
}
#endif

//! creates a storage_array from a c string
template <size_t size> static constexpr auto make_sized_array(const char* str) {
	decay_t<decltype(const_string<size>::content)> ret {};
	for(size_t i = 0; i < size; ++i) {
		ret.data[i] = str[i];
	}
	return ret;
}

//! const_string udl for number literals only
template <char... chars> constexpr auto operator""_cs() {
	return const_string<sizeof...(chars)> {{ chars... }};
}
template <const_string str> constexpr auto operator""_cs() {
	return str;
}
#if (defined(FLOOR_COMPUTE_CUDA) || defined(FLOOR_COMPUTE_OPENCL) || defined(FLOOR_COMPUTE_METAL) || \
	 defined(FLOOR_COMPUTE_HOST_DEVICE) || defined(FLOOR_COMPUTE_VULKAN))
template <size_t count> const_string(const constant char (&str)[count]) -> const_string<count>;
#else
template <size_t count> const_string(const char (&str)[count]) -> const_string<count>;
#endif

//! create a const_string<*> from a c string (with size info)
template <size_t n> constexpr auto make_const_string(const char (&str)[n]) {
	return const_string<n> { str };
}
template <size_t n> constexpr auto CS(const char (&str)[n]) {
	return const_string<n> { str };
}
