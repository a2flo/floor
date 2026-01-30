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

#include <floor/device/device.hpp>
#include <floor/device/host/host_common.hpp>

namespace fl {

FLOOR_PUSH_WARNINGS()
FLOOR_IGNORE_WARNING(weak-vtables)

class device_context;

class host_device final : public device {
public:
	host_device();
	
	//! CPU tier
	HOST_CPU_TIER cpu_tier {
#if defined(__x86_64__)
		HOST_CPU_TIER::X86_TIER_1
#elif defined(__aarch64__)
		HOST_CPU_TIER::ARM_TIER_1
#else
#error "unhandled arch"
#endif
	};
	
	//! this represents the actual native SIMD/vector-width rather than the emulated SIMD-width
	uint32_t native_simd_width { 1u };
	
	//! returns true if the specified object is the same object as this
	bool operator==(const host_device& dev) const {
		return (this == &dev);
	}
	
};

FLOOR_POP_WARNINGS()

} // namespace fl
