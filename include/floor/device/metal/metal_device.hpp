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
#include <floor/device/metal/metal_common.hpp>

#if !defined(FLOOR_NO_METAL) && defined(__OBJC__)
#include <Metal/Metal.h>
#endif

namespace fl {

FLOOR_PUSH_WARNINGS()
FLOOR_IGNORE_WARNING(weak-vtables)

class device_queue;
class metal_device final : public device {
public:
	metal_device();
	
	// Metal software version (Metal API) which this device supports
	METAL_VERSION metal_software_version { METAL_VERSION::METAL_3_0 };
	
	// Metal language version (kernels/shaders) which this device supports
	METAL_VERSION metal_language_version { METAL_VERSION::METAL_3_0 };
	
	enum class FAMILY_TYPE : uint32_t {
		APPLE, //!< iOS, tvOS, visionOS, ...
		MAC,
		COMMON,
		IOS_MAC,
	};
	static constexpr const char* family_type_to_string(const FAMILY_TYPE& fam_type) {
		switch (fam_type) {
			case FAMILY_TYPE::APPLE: return "Apple";
			case FAMILY_TYPE::MAC: return "Mac";
			case FAMILY_TYPE::COMMON: return "Common";
			case FAMILY_TYPE::IOS_MAC: return "iOS-Mac";
		}
		floor_unreachable();
	}
	
	// device family type
	FAMILY_TYPE family_type { FAMILY_TYPE::COMMON };
	
	// device family tier
	uint32_t family_tier { 2u };
	
	//! support Apple platforms
	enum class PLATFORM_TYPE {
		MACOS,
		IOS,
		VISIONOS,
		IOS_SIMULATOR,
		VISIONOS_SIMULATOR,
	};
	// device platform type
	PLATFORM_TYPE platform_type {
#if defined(FLOOR_IOS)
		PLATFORM_TYPE::IOS
#elif defined(FLOOR_VISIONOS)
		PLATFORM_TYPE::VISIONOS
#else
		PLATFORM_TYPE::MACOS
#endif
	};
	static constexpr const char* platform_type_to_string(const PLATFORM_TYPE& pltfrm_type) {
		switch (pltfrm_type) {
			case PLATFORM_TYPE::IOS: return "iOS";
			case PLATFORM_TYPE::IOS_SIMULATOR: return "iOS simulator";
			case PLATFORM_TYPE::VISIONOS: return "visionOS";
			case PLATFORM_TYPE::VISIONOS_SIMULATOR: return "visionOS simulator";
			case PLATFORM_TYPE::MACOS: return "macOS";
		}
		floor_unreachable();
	}
	
	//! true if the device has support for SIMD reduction operations
	bool simd_reduction { false };
	
	//! true if the device has support residency sets
	bool residency_set_support { false };
	
	// compute queue used for internal purposes (try not to use this ...)
	device_queue* internal_queue { nullptr };
	
#if !defined(FLOOR_NO_METAL) && defined(__OBJC__)
	// actual Metal device object
	id <MTLDevice> device { nil };
	
	//! internal memory heaps
	//! NOTE: these exists by default, unless DEVICE_CONTEXT_FLAGS::DISABLE_HEAP was specified,
	//!       or "shared_only_with_unified_memory" config option was set to disable private heaps
	id <MTLHeap> heap_private { nil };
	id <MTLHeap> heap_shared { nil };
	
	//! residency set containing all valid heaps
	id <MTLResidencySet> heap_residency_set { nil };
#else
	void* _device { nullptr };
	void* _heap_private { nullptr };
	void* _heap_shared { nullptr };
	void* _heap_residency_set { nullptr };
#endif
	
	//! returns true if the specified object is the same object as this
	bool operator==(const metal_device& dev) const {
		return (this == &dev);
	}
	
};

} // namespace fl

FLOOR_POP_WARNINGS()
