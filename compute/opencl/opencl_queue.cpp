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

#define FLOOR_OPENCL_INFO_FUNCS 1
#include <floor/compute/opencl/opencl_queue.hpp>

#if !defined(FLOOR_NO_OPENCL)

opencl_queue::opencl_queue(const cl_command_queue& queue_) : queue(queue_) {
}

const void* opencl_queue::get_queue_ptr() const {
	return queue;
}

#endif
