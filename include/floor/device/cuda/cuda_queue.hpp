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

#pragma once

#include <floor/device/cuda/cuda_common.hpp>

#if !defined(FLOOR_NO_CUDA)

#include <floor/device/device_queue.hpp>

namespace fl {

class cuda_queue final : public device_queue {
public:
	explicit cuda_queue(const device& dev, const cu_stream queue);
	~cuda_queue() override;
	
	void finish() const override;
	void flush() const override;
	
	const void* get_queue_ptr() const override;
	void* get_queue_ptr() override;
	
	bool has_profiling_support() const override {
		return true;
	}
	void start_profiling() const override;
	uint64_t stop_profiling() const override;
	
protected:
	cu_stream queue;
	mutable cu_event prof_start { nullptr }, prof_stop { nullptr };
	
};

} // namespace fl

#endif
