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

#include <floor/device/cuda/cuda_queue.hpp>

#if !defined(FLOOR_NO_CUDA)

#include <floor/core/logger.hpp>

namespace fl {

cuda_queue::cuda_queue(const device& dev_, const cu_stream queue_) : device_queue(dev_, QUEUE_TYPE::ALL), queue(queue_) {
	CU_CALL_NO_ACTION(cu_event_create(&prof_start, CU_EVENT_FLAGS::BLOCKING_SYNC), "failed to create profiling event")
	CU_CALL_NO_ACTION(cu_event_create(&prof_stop, CU_EVENT_FLAGS::BLOCKING_SYNC), "failed to create profiling event")
}

cuda_queue::~cuda_queue() {
	if(prof_start != nullptr) {
		CU_CALL_NO_ACTION(cu_event_destroy(prof_start), "failed to destroy profiling event")
	}
	if(prof_stop != nullptr) {
		CU_CALL_NO_ACTION(cu_event_destroy(prof_stop), "failed to destroy profiling event")
	}
}

void cuda_queue::finish() const {
	CU_CALL_RET(cu_stream_synchronize(queue),
				"failed to finish (synchronize) queue")
}

void cuda_queue::flush() const {
	// nop on cuda
}

const void* cuda_queue::get_queue_ptr() const {
	return queue;
}

void* cuda_queue::get_queue_ptr() {
	return queue;
}

void cuda_queue::start_profiling() const {
	CU_CALL_NO_ACTION(cu_event_record(prof_start, queue), "failed to record profiling event")
}

uint64_t cuda_queue::stop_profiling() const {
	CU_CALL_RET(cu_event_record(prof_stop, queue), "failed to record profiling event", 0)
	CU_CALL_RET(cu_event_synchronize(prof_stop), "failed to synchronize profiling event", 0)
	
	float elapsed_time = 0.0f;
	CU_CALL_RET(cu_event_elapsed_time(&elapsed_time, prof_start, prof_stop),
				"failed to compute elapsed time between profiling events", 0)
	return uint64_t(double(elapsed_time) * 1000.0);
}

} // namespace fl

#endif
