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

#ifndef __FLOOR_METAL_INDIRECT_COMMAND_HPP__
#define __FLOOR_METAL_INDIRECT_COMMAND_HPP__

#include <floor/compute/indirect_command.hpp>

#if !defined(FLOOR_NO_METAL)
#include <floor/compute/compute_device.hpp>
#include <floor/core/flat_map.hpp>
#include <Metal/MTLIndirectCommandBuffer.h>
#include <floor/compute/metal/metal_resource_tracking.hpp>

class metal_indirect_command_pipeline final : public indirect_command_pipeline {
public:
	explicit metal_indirect_command_pipeline(const indirect_command_description& desc_,
											 const vector<unique_ptr<compute_device>>& devices);
	~metal_indirect_command_pipeline() override;
	
	//! all Metal pipeline state
	struct metal_pipeline_entry : public metal_resource_tracking {
		friend metal_indirect_command_pipeline;
		id <MTLIndirectCommandBuffer> icb;
	};
	
	//! return the device specific Metal pipeline state for the specified device (or nullptr if it doesn't exist)
	const metal_pipeline_entry* get_metal_pipeline_entry(const compute_device& dev) const;
	metal_pipeline_entry* get_metal_pipeline_entry(const compute_device& dev);
	
	indirect_render_command_encoder& add_render_command(const compute_queue& dev_queue, const graphics_pipeline& pipeline) override;
	indirect_compute_command_encoder& add_compute_command(const compute_queue& dev_queue, const compute_kernel& kernel_obj) override;
	void complete(const compute_device& dev) override;
	void complete() override;
	void reset() override;
	
	//! computes the command NSRange that is necessary for indirect command execution from the given parameters
	//! and validates if the given parameters specify a correct range, returning empty if invalid
	optional<NSRange> compute_and_validate_command_range(const uint32_t command_offset,
														 const uint32_t command_count) const;
	
protected:
	flat_map<const compute_device&, metal_pipeline_entry> pipelines;
	
	void complete_pipeline(const compute_device& dev, metal_pipeline_entry& entry);
	
};

class metal_indirect_render_command_encoder final : public indirect_render_command_encoder, public metal_resource_tracking {
public:
	metal_indirect_render_command_encoder(const metal_indirect_command_pipeline::metal_pipeline_entry& pipeline_entry_,
										  const uint32_t command_idx_,
										  const compute_queue& dev_queue_, const graphics_pipeline& pipeline_);
	~metal_indirect_render_command_encoder() override;
	
	void set_arguments_vector(const vector<compute_kernel_arg>& args) override;
	
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
	const metal_indirect_command_pipeline::metal_pipeline_entry& pipeline_entry;
	const uint32_t command_idx { 0u };
	const llvm_toolchain::function_info* vs_info { nullptr };
	const llvm_toolchain::function_info* fs_info { nullptr };
	
	id <MTLIndirectRenderCommand> command;
	
};

class metal_indirect_compute_command_encoder final : public indirect_compute_command_encoder, public metal_resource_tracking {
public:
	metal_indirect_compute_command_encoder(const metal_indirect_command_pipeline::metal_pipeline_entry& pipeline_entry_,
										   const uint32_t command_idx_,
										   const compute_queue& dev_queue_, const compute_kernel& kernel_obj_);
	~metal_indirect_compute_command_encoder() override;
	
	void set_arguments_vector(const vector<compute_kernel_arg>& args) override;
	
protected:
	const metal_indirect_command_pipeline::metal_pipeline_entry& pipeline_entry;
	const uint32_t command_idx { 0u };
	const compute_kernel::kernel_entry* kernel_entry { nullptr };
	
	id <MTLIndirectComputeCommand> command;
	
	indirect_compute_command_encoder& execute(const uint32_t dim,
											  const uint3& global_work_size,
											  const uint3& local_work_size) override;
	
};

#endif

#endif
