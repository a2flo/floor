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

#include <floor/constexpr/const_string.hpp>
#include <floor/constexpr/const_array.hpp>
#include <floor/core/cpp_ext.hpp>
#include <tuple>

#if (!defined(FLOOR_DEVICE_METAL) && !defined(FLOOR_DEVICE_VULKAN) && !defined(FLOOR_DEVICE_HOST_COMPUTE_IS_DEVICE)) || defined(FLOOR_DEVICE_HAS_SOFT_PRINTF)

// silence clang warnings about non-literal format strings - while it might be right that
// these aren't proper c string literals, this whole thing wouldn't work if the input literals weren't
FLOOR_PUSH_WARNINGS()
FLOOR_IGNORE_WARNING(format-security)
FLOOR_IGNORE_WARNING(format-nonliteral)
FLOOR_IGNORE_WARNING(duplicate-enum)

namespace fl {

// "make_const_string" for strings in constant address space
template <size_t n> constexpr auto make_constant_string(const constant char (&str)[n]) {
	return const_string<n> { make_sized_array<n>((const char*)str) };
}

// (don't look at the man behind the curtain ...)
#define print(str, ...) ({ \
	using tupled_args_type = decltype(fl::device_logger::create_tupled_args(__VA_ARGS__)); \
	constexpr auto arg_types = fl::device_logger::process_args<fl::device_logger::str_dollar_count(str)>(tupled_args_type {}); \
	constexpr constant const auto pstr = fl::device_logger::make_printf_string<fl::device_logger::compute_expanded_len(arg_types)>(str, arg_types); \
	fl::device_logger::log(pstr.content.data, ##__VA_ARGS__); \
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
		INT32,
		UINT32,
		INT64,
		UINT64,
		STRING,
		FLOAT,
		DOUBLE,
		VEC,
		MAT4,
		__MAX_TYPE = MAT4,
		
		// next 8-bit: type specifics
		__SPEC_SHIFT = 8u,
		__SPEC_MASK = 0xFF00u,
		VEC1 = 1u << __SPEC_SHIFT,
		VEC2 = 2u << __SPEC_SHIFT,
		VEC3 = 3u << __SPEC_SHIFT,
		VEC4 = 4u << __SPEC_SHIFT,
		
		// upper 16-bit: additional type specifics
		// for VEC* and MAT4: contains the component type
		__ADD_SPEC_SHIFT = 16u,
		__ADD_SPEC_MASK = 0xFFFF0000u,
	};
	static_assert(uint32_t(ARG_TYPE::__MAX_TYPE) <= 0xFFu, "too many types");
	floor_enum_ext(ARG_TYPE)
	
	// helper function to create a type tuple of all argument types
	// (this is necessary, because non-constexpr args can't be touched directly with constexpr!)
	template <typename... Args>
	static constexpr auto create_tupled_args(Args&&...) {
		return std::tuple<std::decay_t<Args>...> {};
	}
	
	// type -> ARG_TYPE
	template <typename T> struct handle_arg_type {
		// will error on unhandled types later on
		static constexpr ARG_TYPE type() { return ARG_TYPE::INVALID; }
	};
	template <typename T>
	requires(ext::is_integral_v<T> &&
			 ext::is_signed_v<T> &&
			 !std::is_pointer<T>::value)
	struct handle_arg_type<T> {
		static constexpr ARG_TYPE type() {
			return (sizeof(T) <= 4 ? ARG_TYPE::INT32 : ARG_TYPE::INT64);
		}
	};
	template <typename T>
	requires(ext::is_integral_v<T> &&
			 !ext::is_signed_v<T> &&
			 !std::is_pointer<T>::value)
	struct handle_arg_type<T> {
		static constexpr ARG_TYPE type() {
			return (sizeof(T) <= 4 ? ARG_TYPE::UINT32 : ARG_TYPE::UINT64);
		}
	};
	template <typename T>
	requires(ext::is_floating_point_v<T>)
	struct handle_arg_type<T> {
		static constexpr ARG_TYPE type() { return (sizeof(T) <= 4 ? ARG_TYPE::FLOAT : ARG_TYPE::DOUBLE); }
	};
	template <typename T>
	requires(std::is_pointer<T>::value &&
			 (std::is_same_v<T, char*> ||
			  std::is_same_v<T, constant char*> ||
			  std::is_same_v<T, const char*> ||
			  std::is_same_v<T, const constant char*>))
	struct handle_arg_type<T> {
		static constexpr ARG_TYPE type() { return ARG_TYPE::STRING; }
	};
	template <typename T>
	requires(is_floor_vector<T>::value)
	struct handle_arg_type<T> {
		static constexpr ARG_TYPE type() {
			ARG_TYPE ret = ARG_TYPE::VEC;
			switch(T::dim()) {
				case 1: ret |= ARG_TYPE::VEC1; break;
				case 2: ret |= ARG_TYPE::VEC2; break;
				case 3: ret |= ARG_TYPE::VEC3; break;
				case 4: ret |= ARG_TYPE::VEC4; break;
				default: return ARG_TYPE::INVALID;
			}
			const auto scalar_arg_type = handle_arg_type<typename T::decayed_scalar_type>::type();
			if(scalar_arg_type == ARG_TYPE::INVALID) return ARG_TYPE::INVALID;
			ret |= (ARG_TYPE)(uint32_t(scalar_arg_type) << uint32_t(ARG_TYPE::__ADD_SPEC_SHIFT));
			return ret;
		}
	};
	template <typename T>
	requires(is_floor_matrix<T>::value)
	struct handle_arg_type<T> {
		static constexpr ARG_TYPE type() {
			ARG_TYPE ret = ARG_TYPE::MAT4;
			const auto scalar_arg_type = handle_arg_type<typename T::decayed_scalar_type>::type();
			if(scalar_arg_type == ARG_TYPE::INVALID) return ARG_TYPE::INVALID;
			ret |= (ARG_TYPE)(uint32_t(scalar_arg_type) << uint32_t(ARG_TYPE::__ADD_SPEC_SHIFT));
			return ret;
		}
	};
	
	//
	template <size_t array_len>
	static constexpr void handle_arg_types(const_array<ARG_TYPE, array_len>& arg_types, size_t cur_idx) {
		// terminate
		arg_types[cur_idx] = ARG_TYPE::INVALID;
	}
	
	template <typename T, typename... Args, size_t array_len>
	static constexpr void handle_arg_types(const_array<ARG_TYPE, array_len>& arg_types, size_t cur_idx) {
		using decayed_type = decay_as_t<T>;
		static_assert(handle_arg_type<decayed_type>::type() != ARG_TYPE::INVALID, "unsupported type");
		arg_types[cur_idx] = handle_arg_type<decayed_type>::type();
		handle_arg_types<Args...>(arg_types, cur_idx + 1);
	}
	
	// creates the array of argument types necessary to create the format string
	template<size_t dollar_count, typename... Args>
	static constexpr auto process_args(std::tuple<Args...> dummy_tupple) {
		// counts must match (fail early if not)
		static_assert(std::tuple_size<decltype(dummy_tupple)>() == dollar_count, "invalid arg count");
		
		// map types to ARG_TYPEs
		const_array<ARG_TYPE, dollar_count + 1> arg_types {};
		handle_arg_types<Args...>(arg_types, 0);
		floor_return_no_nrvo(arg_types);
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
				case ARG_TYPE::INT32:
				case ARG_TYPE::UINT32:
				case ARG_TYPE::FLOAT:
				case ARG_TYPE::DOUBLE:
				case ARG_TYPE::STRING:
					++ret; // d/u/f/s
					break;
				case ARG_TYPE::INT64:
				case ARG_TYPE::UINT64:
					ret += 3; // lld/llu
					break;
				case ARG_TYPE::VEC: {
					// vector component count
					const auto count = (uint32_t(arg_types[i] & ARG_TYPE::__SPEC_MASK) >>
										uint32_t(ARG_TYPE::__SPEC_SHIFT));
					// vector component type
					size_t type_size = 0;
					switch((ARG_TYPE)(uint32_t(arg_types[i] & ARG_TYPE::__ADD_SPEC_MASK) >>
									  uint32_t(ARG_TYPE::__ADD_SPEC_SHIFT))) {
						case ARG_TYPE::INT32:
						case ARG_TYPE::UINT32:
						case ARG_TYPE::FLOAT:
						case ARG_TYPE::DOUBLE:
						case ARG_TYPE::STRING:
							type_size = 2; // %*
							break;
						case ARG_TYPE::INT64:
						case ARG_TYPE::UINT64:
							type_size = 4; // %ll*
							break;
						default:
							type_size = 0;
							break;
					}
					// (count - 1) * 2 for ", "; +2 for parentheses "(comp, ...)"; -1 for initial '$'
					ret += count * type_size + (count - 1) * 2u + 2u - 1u;
				} break;
				case ARG_TYPE::MAT4: {
					// matrix element type
					size_t type_size = 0;
					switch((ARG_TYPE)(uint32_t(arg_types[i] & ARG_TYPE::__ADD_SPEC_MASK) >>
									  uint32_t(ARG_TYPE::__ADD_SPEC_SHIFT))) {
						case ARG_TYPE::INT32:
						case ARG_TYPE::UINT32:
						case ARG_TYPE::STRING:
							type_size = 2; // %*
							break;
						case ARG_TYPE::FLOAT:
						case ARG_TYPE::DOUBLE:
							type_size = 4; // %.6f
							break;
						case ARG_TYPE::INT64:
						case ARG_TYPE::UINT64:
							type_size = 4; // %ll*
							break;
						default:
							type_size = 0;
							break;
					}
					// 16 elements, 4 per line, elements separated by <tab>, line front and back single char, newline at line end,
					// -1 for initial $, +1 for stripped \n at the end
					ret += (16 * type_size +
							(3 /* tabs per line */ * 4 /* elems per line */) +
							(3 /* front char, back char, newline */ * 4));
				} break;
				case ARG_TYPE::INVALID:
				default:
					break;
			}
		}
		return ret;
	}
	
	// creates the actual printf format string
	template <size_t expanded_len, size_t len, size_t array_len>
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
					if(arg_type != ARG_TYPE::VEC && arg_type != ARG_TYPE::MAT4) {
						pstr[out_idx++] = '%';
					}
					switch(arg_type) {
						case ARG_TYPE::INT32:
							pstr[out_idx++] = 'd';
							break;
						case ARG_TYPE::UINT32:
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
						case ARG_TYPE::VEC: {
							pstr[out_idx++] = '(';
							const auto count = (uint32_t(arg_types[arg_num] & ARG_TYPE::__SPEC_MASK) >>
												uint32_t(ARG_TYPE::__SPEC_SHIFT));
							const auto comp_type = (ARG_TYPE)(uint32_t(arg_types[arg_num] & ARG_TYPE::__ADD_SPEC_MASK) >>
															  uint32_t(ARG_TYPE::__ADD_SPEC_SHIFT));
							for(size_t i = 0; i < count; ++i) {
								if(comp_type == ARG_TYPE::INVALID ||
								   comp_type > ARG_TYPE::__MAX_TYPE) {
									continue;
								}
								pstr[out_idx++] = '%';
								switch(comp_type) {
									case ARG_TYPE::INT32:
										pstr[out_idx++] = 'd';
										break;
									case ARG_TYPE::UINT32:
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
									default: break;
								}
								
								if(i < (count - 1)) {
									pstr[out_idx++] = ',';
									pstr[out_idx++] = ' ';
								}
							}
							pstr[out_idx++] = ')';
						} break;
						case ARG_TYPE::MAT4: {
							const auto comp_type = (ARG_TYPE)(uint32_t(arg_types[arg_num] & ARG_TYPE::__ADD_SPEC_MASK) >>
															  uint32_t(ARG_TYPE::__ADD_SPEC_SHIFT));
							for(size_t line = 0; line < 4; ++line) {
								if(line == 0) pstr[out_idx++] = '/';
								else if(line == 3) pstr[out_idx++] = '\\';
								else pstr[out_idx++] = '|';
								
								for(size_t elem = 0; elem < 4; ++elem) {
									if(elem > 0) pstr[out_idx++] = '\t';
									
									if(comp_type == ARG_TYPE::INVALID ||
									   comp_type > ARG_TYPE::__MAX_TYPE) {
										continue;
									}
									pstr[out_idx++] = '%';
									switch(comp_type) {
										case ARG_TYPE::INT32:
											pstr[out_idx++] = 'd';
											break;
										case ARG_TYPE::UINT32:
											pstr[out_idx++] = 'u';
											break;
										case ARG_TYPE::FLOAT:
										case ARG_TYPE::DOUBLE:
											pstr[out_idx++] = '.';
											pstr[out_idx++] = '6';
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
										default: break;
									}
								}
								
								if(line == 0) pstr[out_idx++] = '\\';
								else if(line == 3) pstr[out_idx++] = '/';
								else pstr[out_idx++] = '|';
								
								if(line < 3) pstr[out_idx++] = '\n';
							}
						} break;
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
	
	template <typename T>
	static constexpr auto tupled_arg(T&& arg) {
		if constexpr (is_floor_vector<T>::value) {
			return arg.as_tuple_ref();
		} else if constexpr (is_floor_matrix<T>::value) {
			return arg.as_tuple_ref();
		} else {
			return std::tuple<T>(arg);
		}
	}
	
	// final call: forward to printf
#if !defined(FLOOR_DEVICE_CUDA) && !defined(FLOOR_DEVICE_HAS_SOFT_PRINTF)
	template <typename... Args>
	static void log(const constant char* format, Args&&... args) {
		std::apply(printf, std::tuple_cat(std::tie(format), tupled_arg(std::forward<Args>(args))...));
	}
#elif defined(FLOOR_DEVICE_CUDA) || defined(FLOOR_DEVICE_HAS_SOFT_PRINTF)
	// need a slightly different approach for CUDA and soft-printf, because printf type can't be infered,
	// because it's a variadic template and not a variadic c function
	template<typename Tuple, size_t... I>
	static constexpr auto _log_final(Tuple&& args, std::index_sequence<I...>) {
		return printf(std::get<I>(std::forward<Tuple>(args))...);
	}
	
	template<typename Tuple, typename Indices = std::make_index_sequence<std::tuple_size<Tuple>::value>>
	static constexpr auto _log_fwd(Tuple&& args) {
		return _log_final(std::forward<Tuple>(args), Indices());
	}
	
	template <size_t format_N, typename... Args>
	static void log(const constant char (&format)[format_N], Args&&... args) {
		_log_fwd(std::tuple_cat(std::tie(format), tupled_arg(std::forward<Args>(args))...));
	}
#endif
	
};

} // namespace fl

// cleanup
FLOOR_POP_WARNINGS()

#else

// ignore for unsupported backends
#define print(...)

#endif
