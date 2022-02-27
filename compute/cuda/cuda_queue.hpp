/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2022 Florian Ziesche
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

class cuda_queue final : public compute_queue {
public:
	explicit cuda_queue(const compute_device& device, const cu_stream queue);
	~cuda_queue() override;
	
	void finish() const override;
	void flush() const override;
	
	void execute_indirect(const indirect_command_pipeline& indirect_cmd,
						  const uint32_t command_offset = 0u,
						  const uint32_t command_count = ~0u) const override;
	
	const void* get_queue_ptr() const override;
	void* get_queue_ptr() override;
	
	bool has_profiling_support() const override {
		return true;
	}
	void start_profiling() override;
	uint64_t stop_profiling() override;
	
protected:
	cu_stream queue;
	cu_event prof_start { nullptr }, prof_stop { nullptr };
	
};

#endif

#endif
