/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2017 Florian Ziesche
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

#ifndef __FLOOR_OPENCL_QUEUE_HPP__
#define __FLOOR_OPENCL_QUEUE_HPP__

#include <floor/compute/opencl/opencl_common.hpp>

#if !defined(FLOOR_NO_OPENCL)

#include <floor/compute/compute_queue.hpp>
#include <floor/compute/opencl/opencl_kernel.hpp>

class opencl_queue final : public compute_queue {
public:
	opencl_queue(shared_ptr<compute_device> device, const cl_command_queue queue);
	
	void finish() const override;
	void flush() const override;
	
	const void* get_queue_ptr() const override;
	void* get_queue_ptr() override;
	
protected:
	cl_command_queue queue;
	
};

#endif

#endif
