/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2022 Florian Ziesche
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

descriptor_set_instance_t vulkan_descriptor_set_container::acquire_descriptor_set() {
	auto [desc_set, index] = descriptor_sets.acquire();
	return { desc_set, index, *this };
}

void vulkan_descriptor_set_container::release_descriptor_set(descriptor_set_instance_t& instance) {
	if (instance.desc_set == nullptr || instance.index == ~0u) {
		return;
	}
	
	descriptor_sets.release({ instance.desc_set, instance.index });
	
	instance.desc_set = nullptr;
	instance.index = ~0u;
}

#endif
