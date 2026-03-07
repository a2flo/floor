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

#if !defined(FLOOR_NO_METAL)
#include <floor/device/device.hpp>
#include <floor/core/flat_map.hpp>
#include <Metal/MTLIndirectCommandBuffer.h>
#include <floor/device/metal/metal4_resource_tracking.hpp>

namespace fl {

class metal4_soft_indirect_command_pipeline;
class metal4_soft_indirect_compute_command_encoder;
class metal4_soft_indirect_render_command_encoder;
class metal4_pipeline;
class metal4_argument_buffer;

//! all Metal pipeline state
struct metal4_soft_pipeline_entry : public metal4_resource_tracking<true> {
	metal4_soft_pipeline_entry(const metal_device& mtl_dev);
	metal4_soft_pipeline_entry(metal4_soft_pipeline_entry&&) = default;
	metal4_soft_pipeline_entry& operator=(metal4_soft_pipeline_entry&&) = default;
	
	struct compute_command_t {
		MTLSize grid_dim { 0u, 0u, 0u };
		MTLSize block_dim { 0u, 0u, 0u };
		bool wait_until_completion { false };
	};
	
	struct render_command_t {
		bool is_indexed { true };
		union {
			struct {
				MTLPrimitiveType primitive_type;
				uint32_t vertex_count;
				uint32_t instance_count;
				uint32_t first_vertex;
				uint32_t first_instance;
			} vertex;
			struct {
				MTLPrimitiveType primitive_type { MTLPrimitiveTypeTriangle };
				MTLIndexType index_type { MTLIndexTypeUInt32 };
				MTLGPUAddress index_buffer_address { 0u };
				uint32_t index_buffer_length { 0u };
				uint32_t index_count { 0u };
				uint32_t instance_count { 0u };
				int32_t vertex_offset { 0 };
				uint32_t base_instance { 0u };
			} indexed;
		};
	};
	
	std::vector<compute_command_t> compute_commands;
	std::vector<render_command_t> render_commands;
	std::vector<const metal4_argument_buffer*> arg_buffers;
	std::vector<uint64_t> arg_buffer_gens;
	std::string debug_label;
	bool is_complete { false };
	
	//! soft-printf handling
	mutable std::shared_ptr<device_buffer> printf_buffer;
	void printf_init(const device_queue& dev_queue) const;
	
	friend metal4_soft_indirect_command_pipeline;
};

class metal4_soft_indirect_command_pipeline final : public indirect_command_pipeline {
public:
	explicit metal4_soft_indirect_command_pipeline(const indirect_command_description& desc_,
												   const std::vector<std::unique_ptr<device>>& devices);
	~metal4_soft_indirect_command_pipeline() override;
	
	//! return the device specific Metal pipeline state for the specified device (or nullptr if it doesn't exist)
	metal4_soft_pipeline_entry* get_metal_pipeline_entry(const device& dev) const;
	
	indirect_render_command_encoder& add_render_command(const device& dev_,
														const graphics_pipeline& pipeline,
														const bool is_multi_view) override;
	indirect_compute_command_encoder& add_compute_command(const device& dev_,
														  const device_function& kernel_obj) override;
	void complete(const device& dev) override;
	void complete() override;
	void reset() override;
	
	template <typename indirect_command_encoder_type>
	const indirect_command_encoder_type& get_command_as(const uint32_t command_index) const {
		if (command_index > commands.size()) {
			throw std::runtime_error("command index is out-of-bounds: " + std::to_string(command_index));
		}
		auto cmd_ptr = commands[command_index].get();
		if (!cmd_ptr) {
			throw std::runtime_error("command at index " + std::to_string(command_index) + " is empty");
		}
		if (auto wanted_cmd_ptr = dynamic_cast<const indirect_command_encoder_type*>(cmd_ptr); wanted_cmd_ptr) {
			return *wanted_cmd_ptr;
		}
		throw std::runtime_error("command at index " + std::to_string(command_index) + " is not of the requested type");
	}
	
	const metal4_soft_indirect_compute_command_encoder& get_compute_command(const uint32_t command_index) const {
		return get_command_as<metal4_soft_indirect_compute_command_encoder>(command_index);
	}
	
	const metal4_soft_indirect_render_command_encoder& get_render_command(const uint32_t command_index) const {
		return get_command_as<metal4_soft_indirect_render_command_encoder>(command_index);
	}
	
	void update_resources(const device& dev, metal4_soft_pipeline_entry& entry) const;
	
protected:
	mutable fl::flat_map<const device*, metal4_soft_pipeline_entry> pipelines;
	
	void complete_pipeline(const device& dev, metal4_soft_pipeline_entry& entry);
	
};

class metal4_soft_indirect_render_command_encoder final :
public indirect_render_command_encoder, public metal4_resource_container_t {
public:
	metal4_soft_indirect_render_command_encoder(metal4_soft_pipeline_entry& pipeline_entry_,
												const device& dev_, const graphics_pipeline& pipeline_, const bool is_multi_view_);
	~metal4_soft_indirect_render_command_encoder() override;
	
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
	
	[[noreturn]] indirect_render_command_encoder& draw_patches(const std::vector<const device_buffer*>, const device_buffer&, const uint32_t,
															   const uint32_t, const uint32_t, const uint32_t, const uint32_t) override  {
		throw std::runtime_error("tessellation is not supported in Metal 4");
	}
	
	[[noreturn]] indirect_render_command_encoder& draw_patches_indexed(const std::vector<const device_buffer*>, const device_buffer&,
																	   const device_buffer&, const uint32_t, const uint32_t, const uint32_t,
																	   const uint32_t, const uint32_t, const uint32_t) override  {
		throw std::runtime_error("tessellation is not supported in Metal 4");
	}
	
protected:
	metal4_soft_pipeline_entry& pipeline_entry;
	const toolchain::function_info* vs_info { nullptr };
	const toolchain::function_info* fs_info { nullptr };
	
public: // for easy access in metal4_renderer
	id <MTLRenderPipelineState> pipeline_state { nil };
	id <MTL4ArgumentTable> vs_arg_table { nil };
	id <MTL4ArgumentTable> fs_arg_table { nil };
	
};

class metal4_soft_indirect_compute_command_encoder final :
public indirect_compute_command_encoder, public metal4_resource_container_t {
public:
	metal4_soft_indirect_compute_command_encoder(metal4_soft_pipeline_entry& pipeline_entry_,
												 const device& dev_, const device_function& kernel_obj_);
	~metal4_soft_indirect_compute_command_encoder() override;
	
	void set_arguments_vector(std::vector<device_function_arg>&& args) override;
	
	indirect_compute_command_encoder& barrier() override;
	
protected:
	metal4_soft_pipeline_entry& pipeline_entry;
	const toolchain::function_info* func_info { nullptr };
	
	indirect_compute_command_encoder& execute(const uint32_t dim,
											  const uint3& global_work_size,
											  const uint3& local_work_size) override;
	
public: // for easy access in metal4_queue
	id <MTLComputePipelineState> pipeline_state { nil };
	id <MTL4ArgumentTable> arg_table { nil };
	
};

} // namespace fl

#endif
