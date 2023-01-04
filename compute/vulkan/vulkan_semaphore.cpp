/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2023 Florian Ziesche
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

#include <floor/compute/vulkan/vulkan_semaphore.hpp>

#if !defined(FLOOR_NO_VULKAN)
#include <floor/core/logger.hpp>
#include <floor/core/core.hpp>
#include <floor/compute/vulkan/vulkan_semaphore.hpp>
#include <floor/compute/vulkan/vulkan_device.hpp>
#include <floor/compute/vulkan/vulkan_compute.hpp>

#if defined(__WINDOWS__)
#include <floor/core/platform_windows.hpp>
#include <floor/core/essentials.hpp>
#include <vulkan/vulkan_win32.h>
#endif

vulkan_semaphore::vulkan_semaphore(const compute_device& dev_, const bool is_export_sema_) : dev(dev_), is_export_sema(is_export_sema_) {
	const auto& vk_dev = (const vulkan_device&)dev;
	
	VkExportSemaphoreCreateInfo export_sema_info;
#if defined(__WINDOWS__)
	VkExportSemaphoreWin32HandleInfoKHR export_sema_win32_info;
#endif
	if (is_export_sema) {
#if defined(__WINDOWS__)
		if (core::is_windows_8_or_higher()) {
			export_sema_win32_info = {
				.sType = VK_STRUCTURE_TYPE_EXPORT_SEMAPHORE_WIN32_HANDLE_INFO_KHR,
				.pNext = nullptr,
				// NOTE: SECURITY_ATTRIBUTES are only required if we want a child process to inherit this handle
				//       -> we don't need this, so set it to nullptr
				.pAttributes = nullptr,
				.dwAccess = (DXGI_SHARED_RESOURCE_READ | DXGI_SHARED_RESOURCE_WRITE),
				.name = nullptr,
			};
		}
#endif
		
		export_sema_info = {
			.sType = VK_STRUCTURE_TYPE_EXPORT_SEMAPHORE_CREATE_INFO,
#if defined(__WINDOWS__)
			.pNext = (core::is_windows_8_or_higher() ? &export_sema_win32_info : nullptr),
			.handleTypes = (core::is_windows_8_or_higher() ?
							VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_WIN32_BIT :
							VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_WIN32_KMT_BIT),
#else
			.pNext = nullptr,
			.handleTypes = VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_FD_BIT,
#endif
		};
	}
	
	const VkSemaphoreCreateInfo sema_create_info {
		.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
		.pNext = (is_export_sema ? &export_sema_info : nullptr),
		.flags = 0,
	};
	VK_CALL_RET(vkCreateSemaphore(vk_dev.device, &sema_create_info, nullptr, &sema),
				"failed to create semaphore")
	

	// get shared handle
	if (is_export_sema) {
		const auto& vk_ctx = *((const vulkan_compute*)vk_dev.context);
#if defined(__WINDOWS__)
		VkSemaphoreGetWin32HandleInfoKHR get_win32_handle {
			.sType = VK_STRUCTURE_TYPE_SEMAPHORE_GET_WIN32_HANDLE_INFO_KHR,
			.pNext = nullptr,
			.semaphore = sema,
			.handleType = (VkExternalSemaphoreHandleTypeFlagBits)export_sema_info.handleTypes,
		};
		VK_CALL_RET(vk_ctx.vulkan_get_semaphore_win32_handle(vk_dev.device, &get_win32_handle, &shared_handle),
					"failed to retrieve shared win32 semaphore handle")
#else
		VkSemaphoreGetFdInfoKHR get_fd_handle {
			.sType = VK_STRUCTURE_TYPE_SEMAPHORE_GET_FD_INFO_KHR,
			.pNext = nullptr,
			.semaphore = sema,
			.handleType = VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_FD_BIT,
		};
		VK_CALL_RET(vk_ctx.vulkan_get_semaphore_fd(vk_dev.device, &get_fd_handle, &shared_handle),
					"failed to retrieve shared fd semaphore handle")
#endif
	}
}

vulkan_semaphore::~vulkan_semaphore() {
	const auto& vk_dev = (const vulkan_device&)dev;
	if (sema != nullptr) {
		vkDestroySemaphore(vk_dev.device, sema, nullptr);
	}
}

#endif
