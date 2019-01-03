/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2019 Florian Ziesche
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
	
	// iOS: < 10000, macOS: >= 10000
	uint32_t feature_set { 0u };
	
	// device family
	uint32_t family { 1u };
	
	// device family version (OS/Metal version dependent)
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
