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

#ifndef __FLOOR_COMPUTE_DEVICE_LOGGER_HPP__
#define __FLOOR_COMPUTE_DEVICE_LOGGER_HPP__

#include <floor/constexpr/const_string.hpp>
#include <floor/constexpr/const_array.hpp>
#include <tuple>

// silence clang warnings about non-literal format strings - while it might be right that
// these aren't proper c string literals, this whole thing wouldn't work if the input literals weren't
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wformat-security"
#pragma clang diagnostic ignored "-Wformat-nonliteral"
#endif

// host code doesn't know "constant"
#ifndef constant
#define constant
#endif

// "make_const_string" for strings in constant address space
template <size_t n> constexpr auto make_constant_string(const constant char (&str)[n]) {
	return const_string<n> { make_sized_array<n>((const char*)str) };
}

// (don't look at the man behind the curtain ...)
#define _log(str, ...) ({ \
	typedef decltype(device_logger::create_tupled_args(__VA_ARGS__)) tupled_args_type; \
	constexpr auto arg_types = device_logger::process_args<device_logger::str_dollar_count(str)>(tupled_args_type {}); \
	constexpr constant const auto pstr = device_logger::make_printf_string<make_constant_string(str).hash(), \
																		   device_logger::compute_expanded_len(arg_types)>(str, arg_types); \
	printf(pstr.content.data, ##__VA_ARGS__); \
})


// compute device logging functions
class device_logger {
public:
	// internal representation of format types
	enum class ARG_TYPE : uint32_t {
		// lowest 8-bit: type
		__TYPE_SHIFT = 0u,
		__TYPE_MASK = 0xFFu,
		INVALID = 0,
		INT,
		UINT,
		INT64,
		UINT64,
		STRING,
		FLOAT,
		DOUBLE,
		VEC,
		__MAX_TYPE = VEC,
		
		// next 8-bit: type specifics
		__SPEC_SHIFT = 8u,
		__SPEC_MASK = 0xFF00u,
		VEC1 = 1u << __SPEC_SHIFT,
		VEC2 = 2u << __SPEC_SHIFT,
		VEC3 = 3u << __SPEC_SHIFT,
		VEC4 = 4u << __SPEC_SHIFT,
		
		// upper 16-bit: additional type specifics
		// for VEC*: contains the component type
		__ADD_SPEC_SHIFT = 16u,
		__ADD_SPEC_MASK = 0xFFFF0000u,
	};
	static_assert(uint32_t(ARG_TYPE::__MAX_TYPE) <= 0xFFu, "too many types");
	floor_enum_ext(ARG_TYPE)
	
	// helper function to create a type tuple of all argument types
	// (this is necessary, because non-constexpr args can't be touched directly with constexpr!)
	template <typename... Args>
	static constexpr auto create_tupled_args(Args&&...) {
		return tuple<decay_t<Args>...> {};
	}
	
	// type -> ARG_TYPE
	template <typename T, typename = void> struct handle_arg_type {
		static constexpr ARG_TYPE type() { return ARG_TYPE::INVALID; }
	};
	template <typename T> struct handle_arg_type<T, enable_if_t<(is_integral<T>::value &&
																 is_signed<T>::value)>> {
		static constexpr ARG_TYPE type() {
			return (sizeof(T) <= 4 ? ARG_TYPE::INT : ARG_TYPE::INT64);
		}
	};
	template <typename T> struct handle_arg_type<T, enable_if_t<(is_integral<T>::value &&
																 !is_signed<T>::value)>> {
		static constexpr ARG_TYPE type() {
			return (sizeof(T) <= 4 ? ARG_TYPE::UINT : ARG_TYPE::UINT64);
		}
	};
	template <typename T> struct handle_arg_type<T, enable_if_t<is_floating_point<T>::value>> {
		static constexpr ARG_TYPE type() { return (sizeof(T) == 4 ? ARG_TYPE::FLOAT : ARG_TYPE::DOUBLE); }
	};
	
	//
	template <size_t array_len>
	static constexpr void handle_arg_types(const_array<ARG_TYPE, array_len>& arg_types, size_t cur_idx) {
		// terminate
		arg_types[cur_idx] = ARG_TYPE::INVALID;
	}
	
	template <typename T, typename... Args, size_t array_len>
	static constexpr void handle_arg_types(const_array<ARG_TYPE, array_len>& arg_types, size_t cur_idx) {
		arg_types[cur_idx] = handle_arg_type<T>::type();
		handle_arg_types<Args...>(arg_types, cur_idx + 1);
	}
	
	// creates the array of argument types necessary to create the format string
	template<size_t dollar_count, typename... Args>
	static constexpr auto process_args(tuple<Args...> dummy_tupple) {
		// counts must match (fail early if not)
		static_assert(tuple_size<decltype(dummy_tupple)>() == dollar_count, "invalid arg count");
		
		//
		const_array<ARG_TYPE, dollar_count + 1> arg_types {};
		handle_arg_types<Args...>(arg_types, 0);
		return arg_types;
	}
	
	// count '$' symbols
	template<size_t len>
	static constexpr size_t str_dollar_count(const constant char (&str)[len]) {
		size_t dollar_count = 0;
		for(size_t i = 0; i < len; ++i) {
			if(str[i] == '$') ++dollar_count;
		}
		return dollar_count;
	}
	
	// computes the additional storage requirements due to the format string
	template<size_t array_len>
	static constexpr size_t compute_expanded_len(const const_array<ARG_TYPE, array_len>& arg_types) {
		size_t ret = 0;
		// iterate over arg types (-1, b/c it is terminated by an additional INVALID type to properly handle "no args")
		for(size_t i = 0; i < (array_len - 1); ++i) {
			switch(arg_types[i] & ARG_TYPE::__TYPE_MASK) {
				case ARG_TYPE::INT:
				case ARG_TYPE::UINT:
				case ARG_TYPE::FLOAT:
				case ARG_TYPE::DOUBLE:
				case ARG_TYPE::STRING:
					++ret;
					break;
				case ARG_TYPE::INT64:
				case ARG_TYPE::UINT64:
					ret += 3;
					break;
				case ARG_TYPE::VEC: {
					// vector component count
					const auto count = (uint32_t(arg_types[i] & ARG_TYPE::__SPEC_MASK) >>
										uint32_t(ARG_TYPE::__SPEC_SHIFT));
					// vector component type
					size_t type_size = 0;
					switch((ARG_TYPE)(uint32_t(arg_types[i] & ARG_TYPE::__ADD_SPEC_MASK) >>
									  uint32_t(ARG_TYPE::__ADD_SPEC_SHIFT))) {
						case ARG_TYPE::INT:
						case ARG_TYPE::UINT:
						case ARG_TYPE::FLOAT:
						case ARG_TYPE::DOUBLE:
						case ARG_TYPE::STRING:
							type_size = 1;
							break;
						case ARG_TYPE::INT64:
						case ARG_TYPE::UINT64:
							type_size = 3;
							break;
						default:
							type_size = 1;
							break;
					}
					// +2 for parentheses (...)
					ret += count * type_size + 2u;
				} break;
				case ARG_TYPE::INVALID:
				default:
					break;
			}
		}
		return ret;
	}
	
	// creates the actual printf format string
	template<uint32_t hash, size_t expanded_len, size_t len, size_t array_len>
	static constexpr auto make_printf_string(const constant char (&str)[len],
											 const const_array<ARG_TYPE, array_len>& arg_types) {
		// +1 for '\n'
		char pstr[len + expanded_len + 1] {};
		
		size_t arg_num = 0, str_idx = 0, out_idx = 0;
		for(; str_idx < (len - 1); ++str_idx) {
			if(str[str_idx] == '$') {
				const auto arg_type = (arg_types[arg_num] & ARG_TYPE::__TYPE_MASK);
				if(arg_type == ARG_TYPE::INVALID ||
				   arg_type > ARG_TYPE::__MAX_TYPE) {
					// invalid, replace with whitespace (shouldn't happen)
					pstr[out_idx++] = ' ';
				}
				else {
					pstr[out_idx++] = '%';
					switch(arg_type) {
						case ARG_TYPE::INT:
							pstr[out_idx++] = 'd';
							break;
						case ARG_TYPE::UINT:
							pstr[out_idx++] = 'u';
							break;
						case ARG_TYPE::FLOAT:
						case ARG_TYPE::DOUBLE:
							pstr[out_idx++] = 'f';
							break;
						case ARG_TYPE::STRING:
							pstr[out_idx++] = 's';
							break;
						case ARG_TYPE::INT64:
							pstr[out_idx++] = 'l';
							pstr[out_idx++] = 'l';
							pstr[out_idx++] = 'd';
							break;
						case ARG_TYPE::UINT64:
							pstr[out_idx++] = 'l';
							pstr[out_idx++] = 'l';
							pstr[out_idx++] = 'u';
							break;
						case ARG_TYPE::VEC:
							// TODO: handle this
							pstr[out_idx++] = 'f';
							break;
						default: break;
					}
				}
				++arg_num;
			}
			else {
				// just copy
				pstr[out_idx++] = str[str_idx];
			}
		}
		pstr[out_idx++] = '\n';
		pstr[out_idx++] = '\0';
		
		return make_const_string(pstr);
	}
	
};

// cleanup
#if defined(__clang__)
#pragma clang diagnostic pop
#endif

#endif
