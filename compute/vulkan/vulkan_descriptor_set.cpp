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

#include <floor/compute/vulkan/vulkan_descriptor_set.hpp>

#if !defined(FLOOR_NO_VULKAN)

#include <floor/compute/vulkan/vulkan_buffer.hpp>

descriptor_buffer_instance_t vulkan_descriptor_buffer_container::acquire_descriptor_buffer() {
	auto [res, index] = descriptor_buffers.acquire();
	return { res.first.get(), res.second, index, this };
}

void vulkan_descriptor_buffer_container::release_descriptor_buffer(descriptor_buffer_instance_t& instance) {
	if (instance.desc_buffer == nullptr || instance.index == ~0u) {
		return;
	}
	
	descriptor_buffers.release(instance.index);
	
	instance.desc_buffer = nullptr;
	instance.index = ~0u;
}

#endif
