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

#ifndef __FLOOR_METAL_DEVICE_HPP__
#define __FLOOR_METAL_DEVICE_HPP__

#include <floor/compute/compute_device.hpp>
#include <floor/compute/metal/metal_common.hpp>

#if !defined(FLOOR_NO_METAL) && defined(__OBJC__)
#include <Metal/Metal.h>
#endif

FLOOR_PUSH_WARNINGS()
FLOOR_IGNORE_WARNING(weak-vtables)

class compute_queue;
class metal_device final : public compute_device {
public:
	metal_device();
	~metal_device() override {}
	
	// metal version which this device supports
	METAL_VERSION metal_version;
	
	// device family, currently 1 (A7), 2 (A8/A8X), 3 (A9/A9X/A10/A10X) and 10000 (anything on OS X)
	uint32_t family { 0u };
	
	// on iOS: 1 (iOS 8.x if A7/A8, or iOS 9.x for A9), 2 (iOS 9.x if A7/A8, iOS 10.x if A9), 3 (iOS 10.x if A7/A8)
	// on OS X: 1 if 10.11, 2 if 10.12, 3 if 10.13
	uint32_t family_version { 1u };
	
	// compute queue used for internal purposes (try not to use this ...)
	compute_queue* internal_queue { nullptr };
	
#if !defined(FLOOR_NO_METAL) && defined(__OBJC__)
	// actual metal device object
	id <MTLDevice> device { nil };
#else
	void* _device { nullptr };
#endif
	
};

FLOOR_POP_WARNINGS()

#endif
