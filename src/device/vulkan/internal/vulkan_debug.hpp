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
#include "vulkan_headers.hpp"
#include <floor/device/vulkan/vulkan_device.hpp>

namespace fl {

#if defined(FLOOR_DEBUG)

VKAPI_ATTR VkBool32 VKAPI_CALL vulkan_debug_callback(VkDebugUtilsMessageSeverityFlagBitsEXT severity,
													 VkDebugUtilsMessageTypeFlagsEXT message_types floor_unused,
													 const VkDebugUtilsMessengerCallbackDataEXT* cb_data,
													 /* vulkan_context* */ void* ctx);

//! sets a Vulkan debug label on the specified object/handle, on the specified device
void set_vulkan_debug_label(const vulkan_device& dev, const VkObjectType type, const uint64_t& handle, const std::string& name);

//! begins a Vulkan command buffer debug label block
void vulkan_begin_cmd_debug_label(const VkCommandBuffer& cmd_buffer, const char* label);

//! ends a Vulkan command buffer debug label block
void vulkan_end_cmd_debug_label(const VkCommandBuffer& cmd_buffer);

//! inserts a Vulkan command buffer debug label
void vulkan_insert_cmd_debug_label(const VkCommandBuffer& cmd_buffer, const char* label);

#else

static inline void set_vulkan_debug_label(const vulkan_device&, const VkObjectType, const uint64_t&, const std::string&) {}
static inline void vulkan_begin_cmd_debug_label(const VkCommandBuffer&, const char*) {}
static inline void vulkan_end_cmd_debug_label(const VkCommandBuffer&) {}
static inline void vulkan_insert_cmd_debug_label(const VkCommandBuffer&, const char*) {}

#endif

} // namespace fl

#endif // FLOOR_NO_VULKAN
