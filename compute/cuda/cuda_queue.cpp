/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2015 Florian Ziesche
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

#include <floor/compute/cuda/cuda_queue.hpp>

#if !defined(FLOOR_NO_CUDA)

cuda_queue::cuda_queue(const cu_stream queue_) : queue(queue_) {
}

void cuda_queue::finish() const {
	CU_CALL_RET(cu_stream_synchronize(queue),
				"failed to finish (synchronize) queue");
}

void cuda_queue::flush() const {
	// nop on cuda
}

const void* cuda_queue::get_queue_ptr() const {
	return queue;
}

#endif
