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

#include <floor/core/essentials.hpp>

#if !defined(FLOOR_NO_VULKAN)
// NOTE: do not include vulkan_common.hpp in here

#include <floor/core/logger.hpp>
#include <floor/core/platform.hpp>

#if defined(__linux__)
#define VK_USE_PLATFORM_XLIB_KHR 1
#define VK_USE_PLATFORM_WAYLAND_KHR 1
#elif defined(__WINDOWS__)
#define VK_USE_PLATFORM_WIN32_KHR 1
#endif
#define VK_NO_PROTOTYPES 1
#include <vulkan/vulkan.h>

extern "C" {
FLOOR_PUSH_WARNINGS()
FLOOR_IGNORE_WARNING(zero-as-null-pointer-constant)
FLOOR_IGNORE_WARNING(cast-function-type-strict)

#define VOLK_IMPLEMENTATION 1
#include "../../../external/volk/volk.h"

FLOOR_POP_WARNINGS()
}

namespace fl {

//! initializes volk and Vulkan
//! NOTE: this is called from vulkan_context
extern bool floor_volk_init();
bool floor_volk_init() {
	static bool did_init = false;
	if (did_init) {
		return true;
	}
	
	if (const auto init_err = volkInitialize(); init_err != VK_SUCCESS) {
		log_error("failed to initialize Vulkan/volk: $", init_err);
		return false;
	}
	
	const auto vk_instance_version = volkGetInstanceVersion();
	log_msg("Vulkan instance version: $.$.$", VK_VERSION_MAJOR(vk_instance_version), VK_VERSION_MINOR(vk_instance_version),
			VK_VERSION_PATCH(vk_instance_version));
	
	did_init = true;
	return true;
}

extern void floor_volk_load_instance(VkInstance& instance);
void floor_volk_load_instance(VkInstance& instance) {
	volkLoadInstance(instance);
}

} // namespace fl

#endif
