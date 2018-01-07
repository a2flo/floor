/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2018 Florian Ziesche
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

#ifndef __FLOOR_CUDA_QUEUE_HPP__
#define __FLOOR_CUDA_QUEUE_HPP__

#include <floor/compute/cuda/cuda_common.hpp>

#if !defined(FLOOR_NO_CUDA)

#include <floor/compute/compute_queue.hpp>
#include <floor/compute/cuda/cuda_kernel.hpp>

class cuda_queue final : public compute_queue {
public:
	cuda_queue(shared_ptr<compute_device> device, const cu_stream queue);
	
	void finish() const override;
	void flush() const override;
	
	const void* get_queue_ptr() const override;
	void* get_queue_ptr() override;
	
protected:
	cu_stream queue;
	
};

#endif

#endif
