/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2016 Florian Ziesche
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

#ifndef __FLOOR_VULKAN_COMMON_HPP__
#define __FLOOR_VULKAN_COMMON_HPP__

#include <floor/core/essentials.hpp>

#if !defined(FLOOR_NO_VULKAN)

#include <vulkan/vulkan.h>

//! vulkan version of the platform/driver/device
enum class VULKAN_VERSION : uint32_t {
	VULKAN_1_0,
};

#endif // FLOOR_NO_VULKAN

#endif
