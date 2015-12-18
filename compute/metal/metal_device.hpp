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

#ifndef __FLOOR_METAL_DEVICE_HPP__
#define __FLOOR_METAL_DEVICE_HPP__

#include <floor/compute/compute_device.hpp>

#if !defined(FLOOR_NO_METAL) && defined(__OBJC__)
#include <floor/compute/metal/metal_common.hpp>
#include <Metal/Metal.h>
#endif

FLOOR_PUSH_WARNINGS()
FLOOR_IGNORE_WARNING(weak-vtables)

class compute_queue;
class metal_device final : public compute_device {
public:
	metal_device() : compute_device() {
		// init statically known info
		type = compute_device::TYPE::GPU;
		platform_vendor = COMPUTE_VENDOR::APPLE;
		
		local_mem_dedicated = true;
		image_support = true;
		
		// seems to be true for all devices? (at least nvptx64, igil64 and A7+)
		bitness = 64;
		
		// for now (iOS9/OSX11 versions: metal 1.1.0, air 1.8.0, language 1.1.0)
		driver_version_str = "1.1.0";
	}
	~metal_device() override {}
	
	// device family, currently 1 (A7), 2 (A8/A8X), 3 (A9/A9X) and 10000 (anything on OS X)
	uint32_t family { 0u };
	
	// on iOS: 1 (iOS 8.x if A7/A8, or iOS 9.x for A9), 2 (iOS 9.x if A7/A8)
	// on OS X: 1 if 10.11
	uint32_t family_version { 1u };
	
	// compute queue used for internal purposes (try not to use this ...)
	compute_queue* internal_queue { nullptr };
	
#if !defined(FLOOR_NO_METAL) && defined(__OBJC__)
	// actual metal device object
	id <MTLDevice> device;
#endif
	
};

FLOOR_POP_WARNINGS()

#endif
