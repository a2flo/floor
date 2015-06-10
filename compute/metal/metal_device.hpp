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

#if !defined(FLOOR_NO_METAL)
#include <floor/compute/metal/metal_common.hpp>
#include <Metal/Metal.h>
#endif

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wweak-vtables"
#endif

class metal_device final : public compute_device {
public:
	~metal_device() override {}
	
#if !defined(FLOOR_NO_METAL)
	// actual metal device object
	id <MTLDevice> device;
#endif
	
	// device family, currently 1 (A7), 2 (A8/A8X) and 10000 (anything on OS X)
	uint32_t family { 0u };
	
	// on iOS: 1 if iOS 8.x, 2 if iOS 9.x
	// on OS X: 1 if 10.11
	uint32_t family_version { 1u };
	
};

#if defined(__clang__)
#pragma clang diagnostic pop
#endif

#endif
