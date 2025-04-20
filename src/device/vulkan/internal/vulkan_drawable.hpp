/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2025 Florian Ziesche
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

namespace fl {

struct vulkan_drawable_image_info {
	uint32_t index { ~0u };
	uint2 image_size;
	uint32_t layer_count { 1u };
	VkImage image { nullptr };
	VkImageView image_view { nullptr };
	VkFormat format { VK_FORMAT_UNDEFINED };
	VkAccessFlags2 access_mask { 0u };
	VkImageLayout layout { VK_IMAGE_LAYOUT_UNDEFINED };
	VkImageLayout present_layout { VK_IMAGE_LAYOUT_PRESENT_SRC_KHR };
	IMAGE_TYPE base_type { IMAGE_TYPE::NONE };
	uint32_t sema_index { ~0u };
	device_fence* acquisition_sema { nullptr };
	device_fence* present_sema { nullptr };
	//! NOTE: only valid if the device supports VK_EXT_swapchain_maintenance1
	VkFence present_fence { nullptr };
};

} // namespace fl

#endif // FLOOR_NO_VULKAN
