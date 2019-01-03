/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2019 Florian Ziesche
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

#ifndef __FLOOR_HOST_QUEUE_HPP__
#define __FLOOR_HOST_QUEUE_HPP__

#include <floor/compute/host/host_common.hpp>

#if !defined(FLOOR_NO_HOST_COMPUTE)

#include <floor/compute/compute_queue.hpp>
#include <floor/compute/host/host_kernel.hpp>
#include <floor/compute/host/host_device.hpp>

class host_queue final : public compute_queue {
public:
	host_queue(shared_ptr<compute_device> device);
	
	void finish() const override;
	void flush() const override;
	
	const void* get_queue_ptr() const override;
	void* get_queue_ptr() override;
	
	bool has_profiling_support() const override {
		return true;
	}
	void start_profiling() override;
	uint64_t stop_profiling() override;
	
protected:
	uint64_t profiling_time { 0 };
	
};

#endif

#endif
