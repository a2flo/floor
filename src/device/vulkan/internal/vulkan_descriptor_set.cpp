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
#include <floor/device/vulkan/vulkan_common.hpp>
#include "vulkan_headers.hpp"
#include "vulkan_descriptor_set.hpp"
#include <floor/device/vulkan/vulkan_buffer.hpp>

namespace fl {

descriptor_buffer_instance_t vulkan_descriptor_buffer_container::acquire_descriptor_buffer() {
	auto acq_res = descriptor_buffers.acquire_resource_no_auto_release();
	assert(acq_res.res != nullptr);
	return { acq_res.res->first.get(), acq_res.res->second, acq_res.index(), this };
}

void vulkan_descriptor_buffer_container::release_descriptor_buffer(descriptor_buffer_instance_t& instance) {
	if (instance.desc_buffer == nullptr || instance.index == invalid_slot_idx) {
		return;
	}
	
	descriptor_buffers.release_resource(instance.index);
	
	instance.desc_buffer = nullptr;
	instance.index = invalid_slot_idx;
}

} // namespace fl

#endif // FLOOR_NO_VULKAN
