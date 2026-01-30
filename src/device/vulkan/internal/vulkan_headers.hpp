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

#if !defined(FLOOR_NO_VULKAN)

#if defined(__linux__)
#define VK_USE_PLATFORM_XLIB_KHR 1
#define VK_USE_PLATFORM_WAYLAND_KHR 1
#elif defined(__WINDOWS__)
#define VK_USE_PLATFORM_WIN32_KHR 1
#endif
#include "../../../../external/volk/volk.h"

#if VK_HEADER_VERSION < 321
#error "Vulkan header version must at least be 321"
#endif

#endif // FLOOR_NO_VULKAN
