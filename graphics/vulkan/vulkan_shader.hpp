/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2020 Florian Ziesche
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

#ifndef __FLOOR_GRAPHICS_VULKAN_VULKAN_SHADER_HPP__
#define __FLOOR_GRAPHICS_VULKAN_VULKAN_SHADER_HPP__

#include <floor/compute/vulkan/vulkan_common.hpp>

#if !defined(FLOOR_NO_VULKAN)

#include <floor/compute/vulkan/vulkan_kernel.hpp>
#include <floor/compute/vulkan/vulkan_queue.hpp>
#include <floor/graphics/graphics_pipeline.hpp>
#include <floor/graphics/graphics_renderer.hpp>

struct vulkan_command_buffer;

class vulkan_shader final : public vulkan_kernel {
public:
	vulkan_shader(kernel_map_type&& kernels);
	~vulkan_shader() override = default;
	
	//! override, since execute is not supported/allowed with shaders
	void execute(const compute_queue& cqueue,
				 const bool& is_cooperative,
				 const uint32_t& dim,
				 const uint3& global_work_size,
				 const uint3& local_work_size,
				 const vector<compute_kernel_arg>& args) const override;
	
	
	//! sets and handles all vertex and fragment shader arguments and enqueue draw call(s)
	void draw(const compute_queue& cqueue,
			  const vulkan_command_buffer& cmd_buffer,
			  const VkPipeline pipeline,
			  const VkPipelineLayout pipeline_layout,
			  const vulkan_kernel_entry* vertex_shader,
			  const vulkan_kernel_entry* fragment_shader,
			  const vector<graphics_renderer::multi_draw_entry>* draw_entries,
			  const vector<graphics_renderer::multi_draw_indexed_entry>* draw_indexed_entries,
			  const vector<compute_kernel_arg>& args) const;
	
	//! sets and handles all vertex and fragment shader arguments and enqueue draw call(s)
	template <typename... Args>
	void draw(const compute_queue& cqueue,
			  const vulkan_command_buffer& cmd_buffer,
			  const VkPipeline pipeline,
			  const VkPipelineLayout pipeline_layout,
			  const vulkan_kernel_entry* vertex_shader,
			  const vulkan_kernel_entry* fragment_shader,
			  const vector<graphics_renderer::multi_draw_entry>* draw_entries,
			  const vector<graphics_renderer::multi_draw_indexed_entry>* draw_indexed_entries,
			  const Args&... args) const {
		draw(cqueue, cmd_buffer, pipeline, pipeline_layout, vertex_shader, fragment_shader, draw_entries, draw_indexed_entries, { args... });
	}

};

#endif

#endif
