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

#include <string>
#include <string_view>
#include <vector>
#include <floor/math/vector_lib.hpp>
#include <floor/device/device_common.hpp>
#include <floor/device/device_function_arg.hpp>
#include <floor/device/device_fence.hpp>
#include <floor/device/device_memory_flags.hpp>
#include <floor/device/toolchain.hpp>
#include <floor/device/argument_buffer.hpp>
#include <floor/core/flat_map.hpp>
#include <floor/threading/atomic_spin_lock.hpp>

namespace fl {

FLOOR_PUSH_WARNINGS()
FLOOR_IGNORE_WARNING(weak-vtables)

class device_queue;
class device_buffer;
class device_image;

class device_function {
public:
	constexpr device_function(const std::string_view function_name_) noexcept : function_name(function_name_) {}
	virtual ~device_function() = default;
	
	struct function_entry {
		const toolchain::function_info* info { nullptr };
		uint32_t max_total_local_size { 0u };
		uint3 max_local_size;
		uint32_t required_simd_width { 0u };
	};
	//! returns the internal function entry for the specified device
	virtual const function_entry* get_function_entry(const device& dev) const = 0;
	
	//! don't call this directly, call the execute function in a device_queue object instead!
	virtual void execute(const device_queue& cqueue,
						 const bool& is_cooperative,
						 const bool& wait_until_completion,
						 const uint32_t& dim,
						 const uint3& global_work_size,
						 const uint3& local_work_size,
						 const std::vector<device_function_arg>& args,
						 const std::vector<const device_fence*>& wait_fences,
						 const std::vector<device_fence*>& signal_fences,
						 const char* debug_label = nullptr,
						 kernel_completion_handler_f&& completion_handler = {}) const = 0;
	
	//! creates an argument buffer for the specified argument index,
	//! "add_mem_flags" may set additional memory flags (already read-write and using host-memory by default)
	//! "zero_init" specifies if the argument buffer data is zero-initialized (default)
	//! NOTE: this will perform basic validity checking and automatically compute the necessary buffer size
	virtual std::unique_ptr<argument_buffer> create_argument_buffer(const device_queue& cqueue, const uint32_t& arg_index,
																	const MEMORY_FLAG add_mem_flags = MEMORY_FLAG::NONE,
																	const bool zero_init = true) const;
	
	//! checks the specified local work size against the max local work size in the function_entry,
	//! and will compute a proper local work size if the specified one is invalid
	//! NOTE: will only warn/error once per function per device
	uint3 check_local_work_size(const function_entry& entry,
								const uint3& local_work_size) const REQUIRES(!warn_map_lock);
	//! static standalone variant of the above
	//! NOTE: this will not warn/error
	static uint3 check_local_work_size(const uint3 wanted_local_work_size,
									   const uint3 max_local_size,
									   const uint32_t max_total_local_size);
	
protected:
	//! function name of this function
	const std::string_view function_name;
	
	//! same as the one in device_context, but this way we don't need access to that object
	virtual PLATFORM_TYPE get_platform_type() const = 0;
	
	mutable atomic_spin_lock warn_map_lock;
	//! used to prevent console/log spam by remembering if a warning/error has already been printed for a function
	mutable fl::flat_map<const function_entry*, uint8_t> warn_map GUARDED_BY(warn_map_lock);
	
	//! internal function to create the actual argument buffer (should be implemented by backends)
	virtual std::unique_ptr<argument_buffer> create_argument_buffer_internal(const device_queue& cqueue,
																			 const function_entry& entry,
																			 const toolchain::arg_info& arg,
																			 const uint32_t& user_arg_index,
																			 const uint32_t& ll_arg_index,
																			 const MEMORY_FLAG& add_mem_flags,
																			 const bool zero_init) const;
	
};

FLOOR_POP_WARNINGS()

} // namespace fl
