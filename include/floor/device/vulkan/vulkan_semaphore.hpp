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

#include <floor/device/vulkan/vulkan_common.hpp>

#if !defined(FLOOR_NO_VULKAN)

namespace fl {

class device;

class vulkan_semaphore {
public:
	vulkan_semaphore(const device& dev_, const bool is_export_sema_ = false);
	~vulkan_semaphore();
	
	//! returns the Vulkan semaphore object
	const VkSemaphore& get_semaphore() const {
		return sema;
	}
	
	//! returns the Vulkan shared semaphore handle (nullptr/0 if !shared)
	const auto& get_shared_handle() const {
		return shared_handle;
	}
	
protected:
	VkSemaphore sema { nullptr };
	const device& dev;
	
	const bool is_export_sema { false };
	// shared semaphore handle when is_export_sema == true
#if defined(__WINDOWS__)
	void* /* HANDLE */ shared_handle { nullptr };
#else
	int shared_handle { 0 };
#endif
	
};

} // namespace fl

#endif
