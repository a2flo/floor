/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2018 Florian Ziesche
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

#ifndef __FLOOR_SOFT_I128_HPP__
#define __FLOOR_SOFT_I128_HPP__

// all 64-bit platforms provide __int128_t and __uint128_t,
// but none of the 32-bit platforms -> emulate it
#if defined(PLATFORM_X32) && !defined(FLOOR_NO_SOFT_I128)

#include <type_traits>
#if !defined(FLOOR_COMPUTE) || defined(FLOOR_COMPUTE_HOST)
#include <cstdint>
#endif
//#include <ostream>
using namespace std;

//! NOTE: this is non-functional for now and should only be used as a placeholder when no native 128-bit types exist
template <bool has_sign, typename i64_type = conditional_t<has_sign, int64_t, uint64_t>, typename ui64_type = uint64_t>
struct alignas(16) i128 {
	i64_type hi; //!< keeps the sign if i128 is signed
	ui64_type lo;
	
	//! default constructors
	constexpr i128() noexcept = default;
	constexpr i128(const i128&) noexcept = default;
	constexpr i128(i128&&) noexcept = default;
	
	//! construction from smaller unsigned integral type
	template <typename int_type, enable_if_t<(is_integral<int_type>::value &&
											  !is_signed<int_type>::value &&
											  sizeof(int_type) <= 8)>* = nullptr>
	constexpr i128(const int_type& val) noexcept : hi(0), lo(val) {}
	//! construction from smaller signed integral type
	template <typename int_type, enable_if_t<(is_integral<int_type>::value &&
											  is_signed<int_type>::value &&
											  sizeof(int_type) <= 8)>* = nullptr>
	constexpr i128(const int_type& val) noexcept : hi(val < (int_type)0 ? (i64_type)~0ull : 0), lo((ui64_type)val) {}
	//! construction from floating point type
	template <typename fp_type, enable_if_t<is_floating_point<fp_type>::value>* = nullptr>
	constexpr i128(const fp_type& val) noexcept : hi(val < (fp_type)0 ? (i64_type)~0ull : 0), lo((ui64_type)val) {}
	
	//! hi/lo construction
	constexpr i128(const i64_type& hi_, const ui64_type& lo_) noexcept : hi(hi_), lo(lo_) {}
	
	// non-functional, just a hack to make this compile
#define FLOOR_I128_OP(op) \
	constexpr i128 operator op (const i128& val) const noexcept { \
		auto rlo = lo op val.lo; \
		auto rhi = hi op val.hi; \
		return i128 { rhi, rlo }; \
	} \
	constexpr i128& operator op##= (const i128& val) noexcept { \
		lo = lo op val.lo; \
		hi = hi op val.hi; \
		return *this; \
	}
	
	FLOOR_I128_OP(+)
	FLOOR_I128_OP(-)
	FLOOR_I128_OP(*)
	FLOOR_I128_OP(%)
	FLOOR_I128_OP(&)
	FLOOR_I128_OP(|)
	FLOOR_I128_OP(^)
	FLOOR_I128_OP(<<)
	FLOOR_I128_OP(>>)
	
#undef FLOOR_I128_OP
	
	constexpr i128 operator-() const {
		return { -hi, -lo };
	}
	constexpr i128 operator!() const {
		if(hi == (i64_type)0 && lo == (ui64_type)0) {
			return { 0, 1 };
		}
		return { 0, 0 };
	}
	constexpr i128 operator~() const {
		return { ~hi, ~lo };
	}
	
	constexpr bool operator<(const i128& val) const {
		if(hi < val.hi) return true;
		if(hi > val.hi) return false;
		if(lo < val.lo) return true;
		return false;
	}
	constexpr bool operator>(const i128& val) const {
		if(hi > val.hi) return true;
		if(hi < val.hi) return false;
		if(lo > val.lo) return true;
		return false;
	}
	constexpr bool operator==(const i128& val) const {
		return (hi == val.hi && lo == val.lo);
	}
	constexpr bool operator!=(const i128& val) const {
		return (hi != val.hi || lo != val.lo);
	}
	constexpr bool operator<=(const i128& val) const {
		return (*this < val || *this == val);
	}
	constexpr bool operator>=(const i128& val) const {
		return (*this > val || *this == val);
	}
	
	//! conversion to other types
	template <typename dst_type>
	explicit constexpr operator dst_type() const {
		return (dst_type)lo;
	}
	
#if 0
	// disabled for now so <iostream> isn't needed
	friend ostream& operator<<(ostream& output, const i128& val) {
		// for now, always print hex ...
		output << hex << uppercase << "0x";
		if(val.hi != (i64_type)0) {
			output << val.hi;
		}
		output << val.lo << nouppercase << dec;
		return output;
	}
#endif
	
};

typedef i128<true> __int128_t;
typedef i128<false> __uint128_t;

#endif

#endif
