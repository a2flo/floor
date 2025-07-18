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

#include <floor/device/vulkan/vulkan_common.hpp>

#if !defined(FLOOR_NO_VULKAN)

#include <floor/core/logger.hpp>
#include <floor/threading/atomic_spin_lock.hpp>
#include <floor/device/vulkan/vulkan_buffer.hpp>
#include <floor/device/vulkan/vulkan_image.hpp>
#include <floor/device/device_function.hpp>
#include <floor/device/device_function_arg.hpp>

namespace fl {

class vulkan_device;
class vulkan_queue;
struct vulkan_encoder;
struct vulkan_command_buffer;
struct vulkan_function_entry;

namespace vulkan_args {
struct idx_handler;
struct transition_info_t;
} // namespace vulkan_args

class vulkan_function : public device_function {
public:
	using function_map_type = fl::flat_map<const vulkan_device*, std::shared_ptr<vulkan_function_entry>>;
	
	vulkan_function(const std::string_view function_name_, function_map_type&& functions);
	~vulkan_function() override = default;
	
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
				 kernel_completion_handler_f&& completion_handler) const override {
		// just forward to the other execute() function, without a cmd buffer
		execute(cqueue, nullptr, is_cooperative, wait_until_completion, dim, global_work_size, local_work_size, args,
				wait_fences, signal_fences, debug_label, std::forward<kernel_completion_handler_f>(completion_handler));
	}
	
	void execute(const device_queue& cqueue,
				 vulkan_command_buffer* external_cmd_buffer,
				 const bool& is_cooperative,
				 const bool& wait_until_completion,
				 const uint32_t& dim,
				 const uint3& global_work_size,
				 const uint3& local_work_size,
				 const std::vector<device_function_arg>& args,
				 const std::vector<const device_fence*>& wait_fences,
				 const std::vector<device_fence*>& signal_fences,
				 const char* debug_label,
				 kernel_completion_handler_f&& completion_handler) const;
	
	const function_entry* get_function_entry(const device& dev) const override;
	
	//! NOTE: if "simd_width" is empty, the required SIMD width or default device SIMD width will be used
	//! NOTE: if "simd_width" is not empty and a required SIMD width is set and the specified SIMD width doesn't match, this returns nullptr!
	VkPipeline get_pipeline_spec(const vulkan_device& dev,
								 vulkan_function_entry& entry,
								 const ushort3 work_group_size,
								 const std::optional<uint16_t> simd_width) const;
	
	//! returns true if the Vulkan function with the specified name should be logged/dumped
	static bool should_log_vulkan_binary(const std::string& function_name);
	
protected:
	mutable function_map_type functions;
	
	typename function_map_type::iterator get_function(const device_queue& queue) const;
	
	PLATFORM_TYPE get_platform_type() const override { return PLATFORM_TYPE::VULKAN; }
	
	std::shared_ptr<vulkan_encoder> create_encoder(const device_queue& queue,
											  const vulkan_command_buffer* cmd_buffer,
											  const VkPipeline pipeline,
											  const VkPipelineLayout pipeline_layout,
											  const std::vector<const vulkan_function_entry*>& entries,
											  const char* debug_label,
											  bool& success) const;
	
	bool set_and_handle_arguments(const bool is_shader,
								  vulkan_encoder& encoder,
								  const std::vector<const vulkan_function_entry*>& shader_entries,
								  const std::vector<device_function_arg>& args,
								  const std::vector<device_function_arg>& implicit_args,
								  vulkan_args::transition_info_t& transition_info) const;
	
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
