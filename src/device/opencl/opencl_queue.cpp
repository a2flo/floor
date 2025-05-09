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

#include <floor/device/opencl/opencl_queue.hpp>

#if !defined(FLOOR_NO_OPENCL)
#include <floor/core/logger.hpp>

namespace fl {

opencl_queue::opencl_queue(const device& dev_, const cl_command_queue queue_) : device_queue(dev_, QUEUE_TYPE::ALL), queue(queue_) {
}

void opencl_queue::finish() const {
	// TODO: error handling
	clFinish(queue);
}

void opencl_queue::flush() const {
	// TODO: error handling
	clFlush(queue);
}

const void* opencl_queue::get_queue_ptr() const {
	return queue;
}

void* opencl_queue::get_queue_ptr() {
	return queue;
}

} // namespace fl

#endif
