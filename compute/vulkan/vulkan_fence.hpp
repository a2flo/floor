/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2024 Florian Ziesche
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

#ifndef __FLOOR_VULKAN_FENCE_HPP__
#define __FLOOR_VULKAN_FENCE_HPP__

#include <floor/compute/compute_fence.hpp>
#include <floor/compute/vulkan/vulkan_common.hpp>

#if !defined(FLOOR_NO_VULKAN)
#include <floor/compute/compute_device.hpp>

FLOOR_PUSH_WARNINGS()
FLOOR_IGNORE_WARNING(weak-vtables)

//! NOTE: the way "compute_fence" is used/usable, there is not direct match in Vulkan,
//!       we can however use a timeline semaphore (which is more powerful) to simulate the behavior
//! NOTE: the underyling value is increasing monotonically, i.e. when signaling this, the next value
//!       will be: "get_current_value() + 1"
//! NOTE: it is still possible to create binary semaphores by setting "create_binary_sema" to true (in that case, ignore the above NOTE)
class vulkan_fence final : public compute_fence {
public:
	vulkan_fence(const compute_device& dev, const bool create_binary_sema = false);
	~vulkan_fence() override;
	
	//! returns the Vulkan specific "fence" object (a timeline semaphore)
	const VkSemaphore& get_vulkan_fence() const {
		return semaphore;
	}
	
	const compute_device& get_device() const {
		return dev;
	}
	
	//! returns the current unsignaled value of the underyling semaphore
	uint64_t get_unsignaled_value() const;
	
	//! returns the next value that is considered to be "signaled"
	uint64_t get_signaled_value() const;
	
	//! sets the next signal value and updates the unsignaled value
	bool next_signal_value();
	
	void set_debug_label(const string& label) override;
	
protected:
	VkSemaphore semaphore { nullptr };
	const compute_device& dev;
	uint64_t last_value { 0 };
	uint64_t signal_value { 0 };
	const bool is_binary { false };
	
};

FLOOR_POP_WARNINGS()

#endif

#endif
