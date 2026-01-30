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

#include <floor/core/essentials.hpp>

#if !defined(FLOOR_NO_METAL) && defined(__OBJC__)
#include <cstdint>
#include <optional>
#import <Metal/Metal.h>

namespace fl::metal_device_query {

//! queried device info
struct device_info_t {
	//! SIMD width of the device
	uint32_t simd_width { 0u };
	//! number of compute units of the device (or 0 if unknown)
	uint32_t units { 0u };
};

//! tries to query device info for the specified device
std::optional<device_info_t> query(id <MTLDevice> device);

} // namespace fl::metal_device_query

#endif
