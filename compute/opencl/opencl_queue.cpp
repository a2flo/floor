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

#include <floor/compute/opencl/opencl_queue.hpp>

#if !defined(FLOOR_NO_OPENCL)
#include <floor/core/logger.hpp>

opencl_queue::opencl_queue(const compute_device& device_, const cl_command_queue queue_) : compute_queue(device_), queue(queue_) {
}

void opencl_queue::finish() const {
	// TODO: error handling
	clFinish(queue);
}

void opencl_queue::flush() const {
	// TODO: error handling
	clFlush(queue);
}

void opencl_queue::execute_indirect(const indirect_command_pipeline& indirect_cmd floor_unused,
									const indirect_execution_parameters_t& params floor_unused,
									kernel_completion_handler_f&& completion_handler floor_unused,
									const uint32_t command_offset floor_unused,
									const uint32_t command_count floor_unused) const {
	// TODO: implement this
	log_error("indirect compute command execution is not implemented for OpenCL");
}

const void* opencl_queue::get_queue_ptr() const {
	return queue;
}

void* opencl_queue::get_queue_ptr() {
	return queue;
}

#endif
