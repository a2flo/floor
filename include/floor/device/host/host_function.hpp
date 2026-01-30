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

#include <floor/device/host/host_common.hpp>

#if !defined(FLOOR_NO_HOST_COMPUTE)

#include <floor/device/device_function.hpp>

namespace fl {

class host_context;
class host_device;
class elf_binary;
struct host_function_wrapper;

// host function execution implementation:
// multi-threaded, each logical CPU ("h/w thread") corresponds to one work-group
// NOTE: has no intra-group parallelism, but has inter-group parallelism
// NOTE: uses fibers when encountering a barrier, running all fibers up to the barrier, then continuing
class host_function final : public device_function {
public:
	struct host_function_entry : function_entry {
		//! for device Host-Compute: the loaded ELF binary program
		std::shared_ptr<elf_binary> program;
		//! for non-device Host-Compute: dummy function info
		toolchain::function_info host_function_info;
	};
	using function_map_type = fl::flat_map<const host_device*, host_function_entry>;
	
	//! constructor for functions built using the host compiler / vanilla toolchain
	host_function(const std::string_view function_name_, const void* function, host_function_entry&& entry);
	//! constructor for functions built using the floor Host-Compute device toolchain
	host_function(const std::string_view function_name_, function_map_type&& functions);
	~host_function() override = default;
	
	void execute(const device_queue& cqueue,
				 const bool& is_cooperative,
				 const bool& wait_until_completion,
				 const uint32_t& work_dim,
				 const uint3& global_work_size,
				 const uint3& local_work_size,
				 const std::vector<device_function_arg>& args,
				 const std::vector<const device_fence*>& wait_fences,
				 const std::vector<device_fence*>& signal_fences,
				 const char* debug_label,
				 kernel_completion_handler_f&& completion_handler) const override;
	
	const function_entry* get_function_entry(const device&) const override;
	
protected:
	const void* function { nullptr };
	host_function_entry entry;
	
	const function_map_type functions {};
	
	PLATFORM_TYPE get_platform_type() const override { return PLATFORM_TYPE::HOST; }
	
	//! Host-Compute "host" execution
	void execute_host(const host_function_wrapper& func,
					  const uint32_t& cpu_count,
					  const uint3& group_dim,
					  const uint3& group_size,
					  const uint3& global_dim,
					  const uint3& local_dim,
					  const uint32_t& work_dim) const;
	
	//! Host-Compute "device" execution
	void execute_device(const host_function_entry& func_entry,
						const uint32_t& cpu_count,
						const uint3& group_dim,
						const uint3& local_dim,
						const uint32_t& work_dim,
						const std::vector<const void*>& vptr_args) const;
	
	typename function_map_type::const_iterator get_function(const device_queue& cqueue) const;
	
	std::unique_ptr<argument_buffer> create_argument_buffer_internal(const device_queue& cqueue,
																	 const function_entry& entry,
																	 const toolchain::arg_info& arg,
																	 const uint32_t& user_arg_index,
																	 const uint32_t& ll_arg_index,
																	 const MEMORY_FLAG& add_mem_flags,
																	 const bool zero_init) const override;
	
	friend host_context;
	static void init();
	
};

} // namespace fl

//! Host-Compute device specific barrier
extern "C" void floor_host_compute_device_barrier() FLOOR_HOST_COMPUTE_CC;
//! Host-Compute device specific SIMD barrier
extern "C" void floor_host_compute_device_simd_barrier() FLOOR_HOST_COMPUTE_CC;
//! Host-Compute device specific printf buffer
extern "C" uint32_t* floor_host_compute_device_printf_buffer() FLOOR_HOST_COMPUTE_CC;

//! Host-Compute (host) local memory offset retrieval
extern uint32_t floor_host_compute_thread_local_memory_offset_get() FLOOR_HOST_COMPUTE_CC;
//! Host-Compute (host) global index retrieval
extern fl::uint3 floor_host_compute_global_idx_get() FLOOR_HOST_COMPUTE_CC;
//! Host-Compute (host) local index retrieval
extern fl::uint3 floor_host_compute_local_idx_get() FLOOR_HOST_COMPUTE_CC;
//! Host-Compute (host) group index retrieval
extern fl::uint3 floor_host_compute_group_idx_get() FLOOR_HOST_COMPUTE_CC;
//! Host-Compute (host) work dim retrieval
extern uint32_t floor_host_compute_work_dim_get() FLOOR_HOST_COMPUTE_CC;
//! Host-Compute (host) global work size/dim retrieval
extern fl::uint3 floor_host_compute_global_work_size_get() FLOOR_HOST_COMPUTE_CC;
//! Host-Compute (host) local work size/dim retrieval
extern fl::uint3 floor_host_compute_local_work_size_get() FLOOR_HOST_COMPUTE_CC;
//! Host-Compute (host) group size/dim retrieval
extern fl::uint3 floor_host_compute_group_size_get() FLOOR_HOST_COMPUTE_CC;
//! Host-Compute (host) sub-group index retrieval
extern uint32_t floor_host_compute_sub_group_id_get() FLOOR_HOST_COMPUTE_CC;
//! Host-Compute (host) sub-group local index retrieval
extern uint32_t floor_host_compute_sub_group_local_id_get() FLOOR_HOST_COMPUTE_CC;
//! Host-Compute (host) sub-group size/dim retrieval
extern uint32_t floor_host_compute_sub_group_size_get() FLOOR_HOST_COMPUTE_CC;
//! Host-Compute (host) sub-group count retrieval
extern uint32_t floor_host_compute_num_sub_groups_get() FLOOR_HOST_COMPUTE_CC;

#endif
