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

#ifndef __FLOOR_COMPUTE_QUEUE_HPP__
#define __FLOOR_COMPUTE_QUEUE_HPP__

#include <string>
#include <vector>
#include <floor/math/vector_lib.hpp>

#include <floor/compute/compute_kernel.hpp>

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wweak-vtables"
#endif

class compute_queue {
public:
	virtual ~compute_queue() = 0;
	
	//! blocks until all currently scheduled work in this queue has been executed
	virtual void finish() const = 0;
	
	//! flushes all scheduled work to the associated device
	virtual void flush() const = 0;
	
	//! implementation specific queue object ptr (cl_command_queue or CUStream, both "struct _ *")
	virtual const void* get_queue_ptr() const = 0;
	
	//! enqueues (and executes) the specified kernel into this queue
	template <typename... Args, class work_size_type_global, class work_size_type_local,
			  enable_if_t<((is_same<decay_t<work_size_type_global>, size1>::value ||
							is_same<decay_t<work_size_type_global>, size2>::value ||
							is_same<decay_t<work_size_type_global>, size3>::value) &&
						   is_same<decay_t<work_size_type_global>, decay_t<work_size_type_local>>::value), int> = 0>
	void execute(shared_ptr<compute_kernel> kernel,
				 work_size_type_global&& global_work_size,
				 work_size_type_local&& local_work_size,
				 Args&&... args) {
		kernel->execute(this, global_work_size, local_work_size, forward<Args>(args)...);
	}
	
protected:
	
};

#if defined(__clang__)
#pragma clang diagnostic pop
#endif

#endif
