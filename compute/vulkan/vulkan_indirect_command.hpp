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

#include <floor/compute/indirect_command.hpp>

#if !defined(FLOOR_NO_VULKAN)
#include <floor/compute/compute_device.hpp>
#include <floor/core/flat_map.hpp>
#include <floor/graphics/graphics_renderer.hpp>

//! -> vulkan_pipeline.hpp
struct vulkan_pipeline_state_t;
//! -> vulkan_pass.hpp
class vulkan_pass;

class vulkan_indirect_command_pipeline final : public indirect_command_pipeline {
public:
	explicit vulkan_indirect_command_pipeline(const indirect_command_description& desc_,
											  const vector<unique_ptr<compute_device>>& devices);
	~vulkan_indirect_command_pipeline() override;
	
	//! all Vulkan pipeline state
	struct vulkan_pipeline_entry {
		vulkan_pipeline_entry() = default;
		vulkan_pipeline_entry(vulkan_pipeline_entry&&);
		vulkan_pipeline_entry& operator=(vulkan_pipeline_entry&&);
		~vulkan_pipeline_entry();
		
		friend vulkan_indirect_command_pipeline;
		VkDevice vk_dev { nullptr };
		
		struct per_queue_data_t {
			//! Vulkan queue family index this was created for
			uint32_t queue_family_index { 0u };
			//! command pool for all commands in this pipeline
			VkCommandPool cmd_pool { nullptr };
			//! secondary command buffers: each will contain one "command"
			vector<VkCommandBuffer> cmd_buffers;
		};
		//! per queue family data
		//! currently: [all, compute-only] when there is a separate compute-only family and this is a COMPUTE pipeline, or [all] otherwise
		vector<per_queue_data_t> per_queue_data;
		
		//! single buffer that acts as the descriptor buffer for all commands
		//! NOTE: allocated based on max commands and max parameters (+implementation specific sizes/offsets)
		shared_ptr<compute_buffer> cmd_parameters;
		//! host-visible/coherent mapping of "cmd_parameters"
		void* mapped_cmd_parameters { nullptr };
		//! the max size per command that we have computed based on indirect_command_description
		size_t per_cmd_size { 0u };
		
		//! soft-printf handling
		mutable shared_ptr<compute_buffer> printf_buffer;
		void printf_init(const compute_queue& dev_queue) const;
		void printf_completion(const compute_queue& dev_queue, vulkan_command_buffer cmd_buffer) const;
	};
	
	//! return the device specific Vulkan pipeline state for the specified device (or nullptr if it doesn't exist)
	const vulkan_pipeline_entry* get_vulkan_pipeline_entry(const compute_device& dev) const;
	vulkan_pipeline_entry* get_vulkan_pipeline_entry(const compute_device& dev);
	
	indirect_render_command_encoder& add_render_command(const compute_device& dev,
														const graphics_pipeline& pipeline,
														const bool is_multi_view) override;
	indirect_compute_command_encoder& add_compute_command(const compute_device& dev,
														  const compute_kernel& kernel_obj) override;
	void complete(const compute_device& dev) override;
	void complete() override;
	void reset() override;
	
	struct command_range_t {
		uint32_t offset { 0u };
		uint32_t count { 0u };
	};
	
	//! computes the command command_range_t that is necessary for indirect command execution from the given parameters
	//! and validates if the given parameters specify a correct range, returning empty if invalid
	optional<command_range_t> compute_and_validate_command_range(const uint32_t command_offset,
																 const uint32_t command_count) const;
	
protected:
	floor_core::flat_map<const compute_device&, vulkan_pipeline_entry> pipelines;
	
	void complete_pipeline(const compute_device& dev, vulkan_pipeline_entry& entry);
	
};

class vulkan_indirect_render_command_encoder final : public indirect_render_command_encoder {
public:
	vulkan_indirect_render_command_encoder(const vulkan_indirect_command_pipeline::vulkan_pipeline_entry& pipeline_entry_,
										   const uint32_t command_idx_,
										   const compute_device& dev_, const graphics_pipeline& pipeline_,
										   const bool is_multi_view_);
	~vulkan_indirect_render_command_encoder() override;
	
	void set_arguments_vector(vector<compute_kernel_arg>&& args) override;
	
	indirect_render_command_encoder& draw(const uint32_t vertex_count,
										  const uint32_t instance_count = 1u,
										  const uint32_t first_vertex = 0u,
										  const uint32_t first_instance = 0u) override;
	
	indirect_render_command_encoder& draw_indexed(const compute_buffer& index_buffer,
												  const uint32_t index_count,
												  const uint32_t instance_count = 1u,
												  const uint32_t first_index = 0u,
												  const int32_t vertex_offset = 0u,
												  const uint32_t first_instance = 0u) override;
	
	indirect_render_command_encoder& draw_patches(const vector<const compute_buffer*> control_point_buffers,
												  const compute_buffer& tessellation_factors_buffer,
												  const uint32_t patch_control_point_count,
												  const uint32_t patch_count,
												  const uint32_t first_patch = 0u,
												  const uint32_t instance_count = 1u,
												  const uint32_t first_instance = 0u) override;
	
	indirect_render_command_encoder& draw_patches_indexed(const vector<const compute_buffer*> control_point_buffers,
														  const compute_buffer& control_point_index_buffer,
														  const compute_buffer& tessellation_factors_buffer,
														  const uint32_t patch_control_point_count,
														  const uint32_t patch_count,
														  const uint32_t first_index = 0u,
														  const uint32_t first_patch = 0u,
														  const uint32_t instance_count = 1u,
														  const uint32_t first_instance = 0u) override;
	
protected:
	const vulkan_indirect_command_pipeline::vulkan_pipeline_entry& pipeline_entry;
	const vulkan_pipeline_state_t* pipeline_state { nullptr };
	const uint32_t command_idx { 0u };
	const compute_kernel::kernel_entry* vs { nullptr };
	const compute_kernel::kernel_entry* fs { nullptr };
	const vulkan_pass* pass { nullptr };
	
	//! cmd buffer in "secondary_cmd_buffers"
	VkCommandBuffer cmd_buffer;
	
	//! associated Vulkan render pass
	VkRenderPass render_pass { nullptr };
	
	//! set via set_arguments_vector
	vector<compute_kernel_arg> args;
	//! internally set implicit args
	vector<compute_kernel_arg> implicit_args;
	
	void draw_internal(const graphics_renderer::multi_draw_entry* draw_entry,
					   const graphics_renderer::multi_draw_indexed_entry* draw_index_entry);
	
};

class vulkan_indirect_compute_command_encoder final : public indirect_compute_command_encoder {
public:
	vulkan_indirect_compute_command_encoder(const vulkan_indirect_command_pipeline::vulkan_pipeline_entry& pipeline_entry_,
											const uint32_t command_idx_,
											const compute_device& dev_, const compute_kernel& kernel_obj_);
	~vulkan_indirect_compute_command_encoder() override;
	
	void set_arguments_vector(vector<compute_kernel_arg>&& args) override;
	
	indirect_compute_command_encoder& barrier() override;
	
protected:
	const vulkan_indirect_command_pipeline::vulkan_pipeline_entry& pipeline_entry;
	const uint32_t command_idx { 0u };
	
	//! cmd buffer in "secondary_cmd_buffers" in each resp. per_queue_data
	VkCommandBuffer cmd_buffers[2] { nullptr, nullptr };
	
	//! set via set_arguments_vector
	vector<compute_kernel_arg> args;
	//! internally set implicit args
	vector<compute_kernel_arg> implicit_args;
	
	indirect_compute_command_encoder& execute(const uint32_t dim,
											  const uint3& global_work_size,
											  const uint3& local_work_size) override;
	
};

#endif
