/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2020 Florian Ziesche
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

#ifndef __FLOOR_ENUM_HELPERS_HPP__
#define __FLOOR_ENUM_HELPERS_HPP__

#include <type_traits>
#include <functional>

#define enum_class_bitwise_or(enum_class) \
floor_inline_always friend constexpr enum_class operator|(const enum_class& e0, const enum_class& e1) { \
	return (enum_class)((std::underlying_type_t<enum_class>)e0 | \
						(std::underlying_type_t<enum_class>)e1); \
} \
floor_inline_always friend constexpr enum_class& operator|=(enum_class& e0, const enum_class& e1) { \
	e0 = e0 | e1; \
	return e0; \
}

#define enum_class_bitwise_and(enum_class) \
floor_inline_always friend constexpr enum_class operator&(const enum_class& e0, const enum_class& e1) { \
	return (enum_class)((std::underlying_type_t<enum_class>)e0 & \
						(std::underlying_type_t<enum_class>)e1); \
} \
floor_inline_always friend constexpr enum_class& operator&=(enum_class& e0, const enum_class& e1) { \
	e0 = e0 & e1; \
	return e0; \
}

#define enum_class_bitwise_or_global(enum_class) \
floor_inline_always constexpr enum_class operator|(const enum_class& e0, const enum_class& e1) { \
	return (enum_class)((std::underlying_type_t<enum_class>)e0 | \
						(std::underlying_type_t<enum_class>)e1); \
} \
floor_inline_always constexpr enum_class& operator|=(enum_class& e0, const enum_class& e1) { \
	e0 = e0 | e1; \
	return e0; \
}

#define enum_class_bitwise_and_global(enum_class) \
floor_inline_always constexpr enum_class operator&(const enum_class& e0, const enum_class& e1) { \
	return (enum_class)((std::underlying_type_t<enum_class>)e0 & \
						(std::underlying_type_t<enum_class>)e1); \
} \
floor_inline_always constexpr enum_class& operator&=(enum_class& e0, const enum_class& e1) { \
	e0 = e0 & e1; \
	return e0; \
}

#define enum_class_bitwise_complement(enum_class) \
floor_inline_always friend constexpr enum_class operator~(const enum_class& e0) { \
	return (enum_class)(~((std::underlying_type_t<enum_class>)e0)); \
}

#define enum_class_bitwise_complement_global(enum_class) \
floor_inline_always constexpr enum_class operator~(const enum_class& e0) { \
	return (enum_class)(~((std::underlying_type_t<enum_class>)e0)); \
}

#if !defined(FLOOR_COMPUTE) || defined(FLOOR_COMPUTE_HOST)
#define enum_class_hash(enum_class) \
namespace std { \
	template <> struct std::hash<enum_class> : public std::hash<std::underlying_type_t<enum_class>> { \
		size_t operator()(enum_class type) const noexcept { \
			return std::hash<std::underlying_type_t<enum_class>>::operator()((std::underlying_type_t<enum_class>)type); \
		} \
	}; \
}
#else
#define enum_class_hash(enum_class)
#endif

#define enum_class_has_flag(enum_class) \
template <enum_class flag, typename int_type = std::underlying_type_t<enum_class>> \
floor_inline_always static constexpr bool has_flag(const enum_class& enum_object) { \
	return ((int_type(flag) & int_type(enum_object)) == int_type(flag)); \
}

// combination of the above
#define floor_enum_ext(enum_class) \
enum_class_bitwise_or(enum_class) \
enum_class_bitwise_and(enum_class) \
enum_class_bitwise_complement(enum_class) \
enum_class_has_flag(enum_class)

#define floor_global_enum_ext(enum_class) \
enum_class_bitwise_or_global(enum_class) \
enum_class_bitwise_and_global(enum_class) \
enum_class_bitwise_complement_global(enum_class) \
enum_class_hash(enum_class) \
enum_class_has_flag(enum_class)

#define floor_global_enum_no_hash_ext(enum_class) \
enum_class_bitwise_or_global(enum_class) \
enum_class_bitwise_and_global(enum_class) \
enum_class_bitwise_complement_global(enum_class) \
enum_class_has_flag(enum_class)

#endif
