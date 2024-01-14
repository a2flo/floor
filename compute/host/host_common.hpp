/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2024 Florian Ziesche
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

#ifndef __FLOOR_HOST_COMMON_HPP__
#define __FLOOR_HOST_COMMON_HPP__

#include <floor/core/essentials.hpp>
#include <cstdint>

//! supported CPU tiers
enum class HOST_CPU_TIER : uint64_t {
	//! x86-based CPUs
	__X86_OFFSET = 0u,
	__X86_RANGE = 999u,
	//! SSE 4.2 / POPCOUNT
	//! NOTE: targets Nehalem and higher (corei7 arch)
	X86_TIER_1 = (__X86_OFFSET + 1u),
	//! AVX v1
	//! NOTE: targets Sandy-Bridge / Bulldozer and higher (corei7-avx arch)
	X86_TIER_2 = (__X86_OFFSET + 2u),
	//! AVX v2 / FMA3 / F16C / BMI1+2
	//! NOTE: targets Haswell / Excavator / Zen 1 and higher (core-avx2 arch)
	X86_TIER_3 = (__X86_OFFSET + 3u),
	//! AVX-512 (F, CD, DQ, VL, BW)
	//! NOTE: targets Skylake-Server / Zen 4 and higher (skylake-avx512 arch)
	X86_TIER_4 = (__X86_OFFSET + 4u),
	//! AVX-512 (F, CD, DQ, VL, BW, IFMA, VBMI, VBMI2, VAES, BITALG, VPCLMULQDQ, GFNI, VNNI, VPOPCNTDQ, BF16)
	//! NOTE: targets Sapphire Rapids / Zen 4 and higher (x86-64-v4 arch + listed AVX-512 features + Zen tuning)
	X86_TIER_5 = (__X86_OFFSET + 5u),
	
	//! ARM-based CPUs
	__ARM_OFFSET = 1000u,
	__ARM_RANGE = 1999u,
	//! base ARMv8.0 (e.g. Cortex-A53, Cortex-A72, Apple A7 - A9)
	ARM_TIER_1 = (__ARM_OFFSET + 1u),
	//! ARMv8.1 + FP16 (e.g. Apple A10)
	ARM_TIER_2 = (__ARM_OFFSET + 2u),
	//! ARMv8.2 + FP16 (e.g. Cortex-A75, Apple A11, Carmel)
	ARM_TIER_3 = (__ARM_OFFSET + 3u),
	//! ARMv8.3 + FP16 (e.g. Apple A12)
	ARM_TIER_4 = (__ARM_OFFSET + 4u),
	//! ARMv8.4 + FP16 (e.g. Apple A13)
	ARM_TIER_5 = (__ARM_OFFSET + 5u),
	//! ARMv8.5 + FP16 + FP16FML (e.g. Apple A14, Apple M1)
	ARM_TIER_6 = (__ARM_OFFSET + 6u),
	//! ARMv8.6 + FP16 + FP16FML (e.g. Apple A15 - A17, Apple M2 - M3)
	ARM_TIER_7 = (__ARM_OFFSET + 7u),
};

constexpr const char* host_cpu_tier_to_string(const HOST_CPU_TIER& tier) {
	switch (tier) {
		case HOST_CPU_TIER::X86_TIER_1: return "x86 Tier 1";
		case HOST_CPU_TIER::X86_TIER_2: return "x86 Tier 2";
		case HOST_CPU_TIER::X86_TIER_3: return "x86 Tier 3";
		case HOST_CPU_TIER::X86_TIER_4: return "x86 Tier 4";
		case HOST_CPU_TIER::X86_TIER_5: return "x86 Tier 5";
		case HOST_CPU_TIER::ARM_TIER_1: return "ARM Tier 1";
		case HOST_CPU_TIER::ARM_TIER_2: return "ARM Tier 2";
		case HOST_CPU_TIER::ARM_TIER_3: return "ARM Tier 3";
		case HOST_CPU_TIER::ARM_TIER_4: return "ARM Tier 4";
		case HOST_CPU_TIER::ARM_TIER_5: return "ARM Tier 5";
		case HOST_CPU_TIER::ARM_TIER_6: return "ARM Tier 6";
		case HOST_CPU_TIER::ARM_TIER_7: return "ARM Tier 7";
		default: return "";
	}
}

// we only support one specific calling convention per arch
#if defined(__x86_64__)
#define FLOOR_HOST_COMPUTE_CC __attribute__((sysv_abi))
#elif defined(__aarch64__)
#if defined(__APPLE__)
// we can't use a different CC on Apple ARM, but the Darwin CC is close enough to AAPCS for this to not matter
#define FLOOR_HOST_COMPUTE_CC
#else
#define FLOOR_HOST_COMPUTE_CC __attribute__((pcs("aapcs")))
#endif
#else
#error "unhandled arch"
#endif

#endif
