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

#include <floor/device/host/host_queue.hpp>

#if !defined(FLOOR_NO_HOST_COMPUTE)
#include <floor/core/logger.hpp>
#include <chrono>

namespace fl {

host_queue::host_queue(const device& dev_) : device_queue(dev_, QUEUE_TYPE::ALL) {
}

void host_queue::finish() const {
	// nop
}

void host_queue::flush() const {
	// nop
}

const void* host_queue::get_queue_ptr() const {
	return this;
}

void* host_queue::get_queue_ptr() {
	return this;
}

static inline uint64_t clock_in_us() {
	return (uint64_t)std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now().time_since_epoch()).count();
}

void host_queue::start_profiling() const {
	profiling_time = clock_in_us();
}

uint64_t host_queue::stop_profiling() const {
	const auto elapsed_time = clock_in_us() - profiling_time;
	profiling_time = 0;
	return elapsed_time;
}

} // namespace fl

#endif
