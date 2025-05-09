/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2025 Florian Ziesche
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

#include <floor/device/metal/metal_common.hpp>

#if !defined(FLOOR_NO_METAL)

#include <floor/device/device_queue.hpp>
#include <floor/threading/thread_safety.hpp>
#if defined(__OBJC__)
#include <Metal/Metal.h>
#endif

namespace fl {

class metal_queue final : public device_queue {
public:
#if defined(__OBJC__)
	explicit metal_queue(const device& dev, id <MTLCommandQueue> floor_nonnull queue);
	~metal_queue() override;
#endif
	
	void finish() const override REQUIRES(!cmd_buffers_lock);
	void flush() const override REQUIRES(!cmd_buffers_lock);
	
	void execute_indirect(const indirect_command_pipeline& indirect_cmd,
						  const indirect_execution_parameters_t& params,
						  kernel_completion_handler_f&& completion_handler,
						  const uint32_t command_offset,
						  const uint32_t command_count) const override REQUIRES(!cmd_buffers_lock);
	
	const void* floor_nonnull get_queue_ptr() const override;
	void* floor_nonnull get_queue_ptr() override;
	
#if defined(__OBJC__)
	id <MTLCommandQueue> floor_nonnull get_queue() const;
	id <MTLCommandBuffer> floor_nonnull make_command_buffer() const REQUIRES(!cmd_buffers_lock);
#endif
	
	bool has_profiling_support() const override {
		return true;
	}
	void start_profiling() const override;
	uint64_t stop_profiling() const override REQUIRES(!cmd_buffers_lock);
	
	void set_debug_label(const std::string& label) override;
	
protected:
#if defined(__OBJC__)
	id <MTLCommandQueue> floor_null_unspecified queue { nil };
#else
	void* floor_null_unspecified queue { nullptr };
#endif
	
	mutable safe_recursive_mutex cmd_buffers_lock;
#if defined(__OBJC__)
	mutable std::vector<std::pair<id <MTLCommandBuffer>, bool>> cmd_buffers GUARDED_BY(cmd_buffers_lock);
#else
	mutable std::vector<std::pair<void*, bool>> cmd_buffers;
#endif
	
	mutable std::atomic<bool> is_profiling { false };
	mutable std::atomic<uint64_t> profiling_sum { 0 };
	
};

} // namespace fl

#endif
