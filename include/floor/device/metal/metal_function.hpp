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

#include <floor/core/logger.hpp>
#include <floor/threading/atomic_spin_lock.hpp>
#include <floor/device/device_buffer.hpp>
#include <floor/device/toolchain.hpp>
#include <floor/device/device_function.hpp>

namespace fl {

class metal_buffer;
class metal_device;

class metal_function : public device_function {
public:
	struct metal_function_entry : function_entry {
		const void* function { nullptr };
		const void* kernel_state { nullptr };
		bool supports_indirect_compute { false };
	};
	using function_map_type = fl::flat_map<const metal_device*, metal_function_entry>;
	
	metal_function(const std::string_view function_name_, function_map_type&& functions);
	~metal_function() override = default;
	
	void execute(const device_queue& cqueue,
				 const bool& is_cooperative,
				 const bool& wait_until_completion,
				 const uint32_t& dim,
				 const uint3& global_work_size,
				 const uint3& local_work_size,
				 const std::vector<device_function_arg>& args,
				 const std::vector<const device_fence*>& wait_fences,
				 const std::vector<device_fence*>& signal_fences,
				 const char* debug_label,
				 kernel_completion_handler_f&& completion_handler) const override;
	
	const function_entry* get_function_entry(const device& dev) const override;
	
	//! helper function to compute the Metal grid dim ("#threadgroups") and block dim ("threads per threadgroup")
	std::pair<uint3, uint3> compute_grid_and_block_dim(const function_entry& entry,
												  const uint32_t& dim,
												  const uint3& global_work_size,
												  const uint3& local_work_size) const;
	
protected:
	const function_map_type functions;
	
	PLATFORM_TYPE get_platform_type() const override { return PLATFORM_TYPE::METAL; }
	
	typename function_map_type::const_iterator get_function(const device_queue& queue) const;
	
	std::unique_ptr<argument_buffer> create_argument_buffer_internal(const device_queue& cqueue,
																	 const function_entry& entry,
																	 const toolchain::arg_info& arg,
																	 const uint32_t& user_arg_index,
																	 const uint32_t& ll_arg_index,
																	 const MEMORY_FLAG& add_mem_flags,
																	 const bool zero_init) const override;
	
};

} // namespace fl

#endif
