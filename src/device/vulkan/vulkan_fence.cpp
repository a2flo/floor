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

#include <floor/core/essentials.hpp>

#if !defined(FLOOR_NO_VULKAN)
#include "internal/vulkan_headers.hpp"
#include <floor/device/vulkan/vulkan_fence.hpp>
#include <floor/device/vulkan/vulkan_device.hpp>
#include <floor/device/vulkan/vulkan_context.hpp>
#include "internal/vulkan_debug.hpp"
#include <floor/core/logger.hpp>

namespace fl {

vulkan_fence::vulkan_fence(const device& dev_, const bool create_binary_sema) : device_fence(), dev(dev_), is_binary(create_binary_sema) {
	const auto& vk_dev = (const vulkan_device&)dev;
	const VkSemaphoreTypeCreateInfo sema_type_create_info {
		.sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO,
		.pNext = nullptr,
		.semaphoreType = (!create_binary_sema ? VK_SEMAPHORE_TYPE_TIMELINE : VK_SEMAPHORE_TYPE_BINARY),
		.initialValue = last_value,
	};
	const VkSemaphoreCreateInfo create_info {
		.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
		.pNext = &sema_type_create_info,
		.flags = 0,
	};
	VK_CALL_ERR_EXEC(vkCreateSemaphore(vk_dev.device, &create_info, nullptr, &semaphore),
					 "failed to create Vulkan semaphore", throw std::runtime_error("failed to create Vulkan semaphore");)
	
	if (is_binary) {
		signal_value = 1; // always 1 for binary semas
	}
}

vulkan_fence::~vulkan_fence() {
	if (semaphore) {
		const auto& vk_dev = (const vulkan_device&)dev;
		vkDestroySemaphore(vk_dev.device, semaphore, nullptr);
		semaphore = nullptr;
	}
}

uint64_t vulkan_fence::get_unsignaled_value() const {
	return last_value;
}

uint64_t vulkan_fence::get_signaled_value() const {
	return signal_value;
}

bool vulkan_fence::next_signal_value() {
	if (is_binary) {
		return false;
	}
	
	const auto& vk_dev = (const vulkan_device&)dev;
	VK_CALL_RET(vkGetSemaphoreCounterValue(vk_dev.device, semaphore, &last_value),
				"failed to query semaphore value", false)
	signal_value = last_value + 1;
	return true;
}

void vulkan_fence::set_debug_label(const std::string& label) {
	device_fence::set_debug_label(label);
	set_vulkan_debug_label((const vulkan_device&)dev, VK_OBJECT_TYPE_SEMAPHORE, uint64_t(semaphore), label);
}

} // namespace fl

#endif
