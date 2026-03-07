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

#include <floor/device/indirect_command.hpp>

namespace fl {

class generic_indirect_command_pipeline;

//! generic indirect pipeline entry used in generic_indirect_command_pipeline
struct generic_indirect_pipeline_entry {
	generic_indirect_pipeline_entry() = default;
	generic_indirect_pipeline_entry(generic_indirect_pipeline_entry&& entry) :
	dev(entry.dev), debug_label(std::move(entry.debug_label)), commands(std::move(entry.commands)) {
		entry.dev = nullptr;
	}
	generic_indirect_pipeline_entry& operator=(generic_indirect_pipeline_entry&& entry) {
		dev = entry.dev;
		debug_label = std::move(entry.debug_label);
		commands = std::move(entry.commands);
		return *this;
	}
	~generic_indirect_pipeline_entry() = default;
	
	friend generic_indirect_command_pipeline;
	device* dev { nullptr };
	std::string debug_label;
	
	struct compute_command_t {
		const device_function* kernel_ptr { nullptr };
		uint32_t dim { 0u };
		uint3 global_work_size;
		uint3 local_work_size;
		bool wait_until_completion { false };
	};
	
	struct render_command_t {
		const device_function* vs_ptr { nullptr };
		const device_function* fs_ptr { nullptr };
		const device_buffer* index_buffer { nullptr };
		union {
			struct {
				uint32_t vertex_count;
				uint32_t instance_count;
				uint32_t first_vertex;
				uint32_t first_instance;
			} vertex;
			struct {
				uint32_t index_count { 0u };
				uint32_t instance_count { 0u };
				uint32_t first_index { 0u };
				int32_t vertex_offset { 0 };
				uint32_t first_instance { 0u };
				INDEX_TYPE index_type { INDEX_TYPE::UINT };
			} indexed;
		};
	};
	
	struct generic_command_t {
		union {
			compute_command_t compute;
			render_command_t render;
		};
		std::vector<device_function_arg> args;
	};
	
	std::vector<generic_command_t> commands;
};

//! generic indirect command pipeline implementation (used by CUDA, Host-Compute, OpenCL)
//! NOTE: this only supports compute commands
class generic_indirect_command_pipeline final : public indirect_command_pipeline {
public:
	explicit generic_indirect_command_pipeline(const indirect_command_description& desc_,
											  const std::vector<std::unique_ptr<device>>& devices);
	~generic_indirect_command_pipeline() override = default;
	
	//! return the device specific pipeline state for the specified device (or nullptr if it doesn't exist)
	const generic_indirect_pipeline_entry* get_pipeline_entry(const device& dev) const;
	generic_indirect_pipeline_entry* get_pipeline_entry(const device& dev);
	
	indirect_render_command_encoder& add_render_command(const device& dev,
														const graphics_pipeline& pipeline,
														const bool is_multi_view) override;
	indirect_compute_command_encoder& add_compute_command(const device& dev, const device_function& kernel_obj) override;
	void complete(const device& dev) override;
	void complete() override;
	void reset() override;
	
	struct command_range_t {
		uint32_t offset { 0u };
		uint32_t count { 0u };
	};
	
protected:
	fl::flat_map<const device*, std::shared_ptr<generic_indirect_pipeline_entry>> pipelines;
	
};

//! generic indirect compute command encoder implementation (used by CUDA, Host-Compute, OpenCL)
class generic_indirect_compute_command_encoder final : public indirect_compute_command_encoder {
public:
	generic_indirect_compute_command_encoder(generic_indirect_pipeline_entry& pipeline_entry_,
											 const device& dev_, const device_function& kernel_obj_);
	~generic_indirect_compute_command_encoder() override = default;
	
	void set_arguments_vector(std::vector<device_function_arg>&& args) override;
	
	indirect_compute_command_encoder& barrier() override;
	
protected:
	generic_indirect_pipeline_entry& pipeline_entry;
	
	//! set via set_arguments_vector
	std::vector<device_function_arg> args;
	
	indirect_compute_command_encoder& execute(const uint32_t dim,
											  const uint3& global_work_size,
											  const uint3& local_work_size) override;
	
};

//! generic indirect render command encoder implementation
class generic_indirect_render_command_encoder final : public indirect_render_command_encoder {
public:
	generic_indirect_render_command_encoder(generic_indirect_pipeline_entry& pipeline_entry_,
											const device& dev_, const graphics_pipeline& pipeline_,
											const bool is_multi_view_);
	~generic_indirect_render_command_encoder() override = default;
	
	void set_arguments_vector(std::vector<device_function_arg>&& args) override;
	
	indirect_render_command_encoder& draw(const uint32_t vertex_count,
										  const uint32_t instance_count = 1u,
										  const uint32_t first_vertex = 0u,
										  const uint32_t first_instance = 0u) override;
	
	indirect_render_command_encoder& draw_indexed(const device_buffer& index_buffer,
												  const uint32_t index_count,
												  const uint32_t instance_count = 1u,
												  const uint32_t first_index = 0u,
												  const int32_t vertex_offset = 0u,
												  const uint32_t first_instance = 0u,
												  const INDEX_TYPE index_type = INDEX_TYPE::UINT) override;
	
	[[noreturn]] indirect_render_command_encoder& draw_patches(const std::vector<const device_buffer*> control_point_buffers,
															   const device_buffer& tessellation_factors_buffer,
															   const uint32_t patch_control_point_count,
															   const uint32_t patch_count,
															   const uint32_t first_patch = 0u,
															   const uint32_t instance_count = 1u,
															   const uint32_t first_instance = 0u) override;
	
	[[noreturn]] indirect_render_command_encoder& draw_patches_indexed(const std::vector<const device_buffer*> control_point_buffers,
																	   const device_buffer& control_point_index_buffer,
																	   const device_buffer& tessellation_factors_buffer,
																	   const uint32_t patch_control_point_count,
																	   const uint32_t patch_count,
																	   const uint32_t first_index = 0u,
																	   const uint32_t first_patch = 0u,
																	   const uint32_t instance_count = 1u,
																	   const uint32_t first_instance = 0u) override;
	
protected:
	generic_indirect_pipeline_entry& pipeline_entry;
	const device_function* vertex_shader { nullptr };
	const device_function::function_entry* vertex_entry { nullptr };
	const device_function* fragment_shader { nullptr };
	const device_function::function_entry* fragment_entry { nullptr };
	
	//! set via set_arguments_vector
	std::vector<device_function_arg> args;
	
};

} // namespace fl
