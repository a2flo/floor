/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2016 Florian Ziesche
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

#include <floor/compute/vulkan/vulkan_queue.hpp>

#if !defined(FLOOR_NO_VULKAN)

vulkan_queue::vulkan_queue(shared_ptr<compute_device> device_, const VkQueue queue_) : compute_queue(device_), queue(queue_) {
}

void vulkan_queue::finish() const {
	// TODO: error handling
	// TODO: implement this
}

void vulkan_queue::flush() const {
	// TODO: error handling
	// TODO: implement this
}

const void* vulkan_queue::get_queue_ptr() const {
	return queue;
}

#endif
