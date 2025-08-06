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

#include <floor/device/vulkan/vulkan_function.hpp>
#include <floor/device/vulkan/vulkan_queue.hpp>
#include <floor/device/graphics_pipeline.hpp>
#include <floor/device/graphics_renderer.hpp>

namespace fl {

struct vulkan_command_buffer;

class vulkan_shader final : public vulkan_function {
public:
	vulkan_shader(function_map_type&& functions);
	~vulkan_shader() override = default;
	
	//! override, since execute is not supported/allowed with shaders
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
	
	
	//! sets and handles all vertex and fragment shader arguments and enqueue draw call(s),
	//! returns all required image layout transition barriers
	std::vector<VkImageMemoryBarrier2> draw(const device_queue& cqueue,
											const vulkan_command_buffer& cmd_buffer,
											const VkPipeline pipeline,
											const VkPipelineLayout pipeline_layout,
											const vulkan_function_entry* vertex_shader,
											const vulkan_function_entry* fragment_shader,
											const std::vector<graphics_renderer::multi_draw_entry>* draw_entries,
											const std::vector<graphics_renderer::multi_draw_indexed_entry>* draw_indexed_entries,
											const std::vector<device_function_arg>& args) const;
	
	//! sets and handles all vertex and fragment shader arguments and enqueue draw call(s),
	//! returns all required image layout transition barriers
	template <typename... Args>
	std::vector<VkImageMemoryBarrier2> draw(const device_queue& cqueue,
											const vulkan_command_buffer& cmd_buffer,
											const VkPipeline pipeline,
											const VkPipelineLayout pipeline_layout,
											const vulkan_function_entry* vertex_shader,
											const vulkan_function_entry* fragment_shader,
											const std::vector<graphics_renderer::multi_draw_entry>* draw_entries,
											const std::vector<graphics_renderer::multi_draw_indexed_entry>* draw_indexed_entries,
											const Args&... args) const {
		return draw(cqueue, cmd_buffer, pipeline, pipeline_layout, vertex_shader, fragment_shader, draw_entries, draw_indexed_entries, { args... });
	}
	
};

} // namespace fl

#endif
