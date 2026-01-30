/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2026 Florian Ziesche
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

// we don't want/need soft_f16 on any of the non-Host-Compute/graphics backends
#if !defined(FLOOR_DEVICE_CUDA) && !defined(FLOOR_DEVICE_OPENCL) && !defined(FLOOR_DEVICE_METAL) && !defined(FLOOR_DEVICE_VULKAN) && !defined(FLOOR_DEVICE_HOST_COMPUTE_IS_DEVICE)

#include <type_traits>
#if !defined(FLOOR_DEVICE) || defined(FLOOR_DEVICE_HOST_COMPUTE)
#include <cstdint>
#endif
#include <bit>

#if !defined(FLOOR_NO_MATH_STR)
#include <string>
#include <iostream>
#endif

#define FLOOR_HAS_NATIVE_FP16 1

namespace fl {

//! storage-only 16-bit half-precision floating point type
//! NOTE: this implementation is incomplete and not always constexpr at this point
struct soft_f16 {
	__fp16 value_fp16;
	
	enum : uint16_t {
		SIGN_MASK = 0x8000u,
		EXPONENT_MASK = 0x7C00u,
		MANTISSA_MASK = 0x03FFu,
	};
	
	//! default constructors
	constexpr soft_f16() noexcept = default;
	constexpr soft_f16(const soft_f16&) noexcept = default;
	constexpr soft_f16(soft_f16&&) noexcept = default;
	constexpr soft_f16& operator=(const soft_f16&) noexcept = default;
	constexpr soft_f16& operator=(soft_f16&&) noexcept = default;
	
	//! construction from a floating point value
	template <typename fp_type> requires(std::is_floating_point_v<fp_type> || std::is_same_v<fp_type, __fp16>)
	constexpr soft_f16(const fp_type& val) noexcept : value_fp16(__fp16(val)) {}
	//! assignment from a floating point value
	template <typename fp_type> requires(std::is_floating_point_v<fp_type> || std::is_same_v<fp_type, __fp16>)
	constexpr soft_f16& operator=(const fp_type& val) noexcept {
		value_fp16 = __fp16(val);
		return *this;
	}
	
	//! construction from an integral value
	template <typename int_type> requires(std::is_integral_v<int_type>)
	constexpr soft_f16(const int_type& val) noexcept : value_fp16(__fp16(float(val))) {}
	//! assignment from an integral value
	template <typename int_type> requires(std::is_integral_v<int_type>)
	constexpr soft_f16& operator=(const int_type& val) noexcept {
		value_fp16 = __fp16(float(val));
		return *this;
	}
	
	//! conversion to/from other types
	constexpr float to_float() const {
		return (float)value_fp16;
	}
	
	//! explicitly converts to float
	explicit constexpr operator float() const {
		return to_float();
	}
#if !defined(FLOOR_DEVICE_NO_DOUBLE)
	//! explicitly converts to double
	explicit constexpr operator double() const {
		return (double)to_float();
	}
#endif
#if !defined(FLOOR_DEVICE) || defined(FLOOR_DEVICE_HOST_COMPUTE)
	//! explicitly converts to long double
	explicit constexpr operator long double() const {
		return (long double)to_float();
	}
#endif
	
	//! explicitly converts to int32_t
	explicit constexpr operator int32_t() const {
		return (int32_t)to_float();
	}
	//! explicitly converts to int64_t
	explicit constexpr operator int64_t() const {
		return (int64_t)to_float();
	}
	
#define FLOOR_F16_OP(op) \
	constexpr soft_f16 operator op (const soft_f16& val) const noexcept { \
		return soft_f16 { __fp16(float(value_fp16) op float(val.value_fp16)) }; \
	} \
	constexpr soft_f16& operator op##= (const soft_f16& val) noexcept { \
		value_fp16 = __fp16(float(value_fp16) op float(val.value_fp16)); \
		return *this; \
	} \
	constexpr friend soft_f16 operator op (const float& lhs, const soft_f16& rhs) { \
		return soft_f16 { __fp16(lhs * float(rhs)) }; \
	}
	
	FLOOR_F16_OP(+)
	FLOOR_F16_OP(-)
	FLOOR_F16_OP(*)
	FLOOR_F16_OP(/)
	
#undef FLOOR_F16_OP
	
	static constexpr soft_f16 direct_u16_value_init(const uint16_t& val) {
		soft_f16 ret;
		ret.value_fp16 = __builtin_bit_cast(__fp16, val);
		return ret;
	}
	
	constexpr soft_f16 operator-() const {
		return -float(value_fp16);
	}
	constexpr soft_f16 operator!() const {
		return (float(value_fp16) == 0.0f ? 1.0f : 0.0f);
	}
	
	constexpr bool operator<(const soft_f16& val) const {
		return float(value_fp16) < float(val.value_fp16);
	}
	constexpr bool operator>(const soft_f16& val) const {
		return float(value_fp16) > float(val.value_fp16);
	}
	constexpr bool operator==(const soft_f16& val) const {
		return (std::bit_cast<uint16_t>(value_fp16) == std::bit_cast<uint16_t>(val.value_fp16));
	}
	constexpr bool operator!=(const soft_f16& val) const {
		return (std::bit_cast<uint16_t>(value_fp16) != std::bit_cast<uint16_t>(val.value_fp16));
	}
	constexpr bool operator<=(const soft_f16& val) const {
		return (*this < val || *this == val);
	}
	constexpr bool operator>=(const soft_f16& val) const {
		return (*this > val || *this == val);
	}
	
	constexpr bool is_nan() const {
		const auto value = std::bit_cast<uint16_t>(value_fp16);
		return ((value & EXPONENT_MASK) == EXPONENT_MASK) && ((value & MANTISSA_MASK) != 0);
	}
	constexpr bool is_infinite() const {
		const auto value = std::bit_cast<uint16_t>(value_fp16);
		return ((value & EXPONENT_MASK) == EXPONENT_MASK) && ((value & MANTISSA_MASK) == 0);
	}
	constexpr bool is_normal() const {
		const auto value = std::bit_cast<uint16_t>(value_fp16);
		return ((value & EXPONENT_MASK) != EXPONENT_MASK) && ((value & EXPONENT_MASK) != 0);
	}
	constexpr bool is_finite() const {
		const auto value = std::bit_cast<uint16_t>(value_fp16);
		return ((value & EXPONENT_MASK) != EXPONENT_MASK);
	}
	
	constexpr int clz() const {
		const auto value = std::bit_cast<uint16_t>(value_fp16);
		return __builtin_clzs(value);
	}
	
	constexpr int ctz() const {
		const auto value = std::bit_cast<uint16_t>(value_fp16);
		return __builtin_ctzs(value);
	}
	
	constexpr int popcount() const {
		const auto value = std::bit_cast<uint16_t>(value_fp16);
		return __builtin_popcount(uint32_t(value));
	}
	
#if !defined(FLOOR_NO_MATH_STR)
	
	//! returns a string representation of this half value
	operator std::string() const {
		return std::to_string(to_float());
	}
	
	//! ostream output of this half value
	friend std::ostream& operator<<(std::ostream& output, const soft_f16& val) {
		output << std::to_string(val.to_float());
		return output;
	}
	
#endif
	
};

using half = soft_f16;

} // namespace fl

#endif
