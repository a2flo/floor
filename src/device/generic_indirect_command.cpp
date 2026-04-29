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

#include <floor/device/generic_indirect_command.hpp>
#include <floor/device/device_buffer.hpp>
#include <floor/device/device_context.hpp>
#include <floor/device/graphics_pipeline.hpp>

namespace fl {

generic_indirect_command_pipeline::generic_indirect_command_pipeline(const indirect_command_description& desc_,
																	 const std::vector<std::unique_ptr<device>>& devices) :
indirect_command_pipeline(desc_) {
	if (!valid) {
		return;
	}
	if (devices.empty()) {
		log_error("no devices specified in indirect command pipeline \"$\"", desc.debug_label);
		valid = false;
		return;
	}
	for (const auto& dev_ptr : devices) {
		if ((desc.command_type == indirect_command_description::COMMAND_TYPE::RENDER ||
			 desc.command_type == indirect_command_description::COMMAND_TYPE::RENDER_MESH) &&
			!dev_ptr->indirect_render_command_support) {
			log_error("indirect render command pipelines are not supported by device \"$\" (pipeline \"$\")", dev_ptr->name, desc.debug_label);
			valid = false;
			return;
		}
		if (desc.command_type == indirect_command_description::COMMAND_TYPE::RENDER_MESH &&
			dev_ptr->indirect_render_command_support && !dev_ptr->mesh_shading_support) {
			log_error("indirect render-mesh command pipelines are not supported by device \"$\" (pipeline \"$\")", dev_ptr->name, desc.debug_label);
			valid = false;
			return;
		}
	}
	if (!desc.ignore_max_max_command_count_limit && desc.max_command_count > 16384u) {
		log_error("max supported command count is 16384, in indirect command pipeline \"$\"", desc.debug_label);
		valid = false;
		return;
	}
	
	for (const auto& dev_ptr : devices) {
		auto entry = std::make_shared<generic_indirect_pipeline_entry>();
		entry->dev = dev_ptr.get();
		entry->debug_label = (desc.debug_label.empty() ? "generic_indirect_command_pipeline" : desc.debug_label);
		entry->commands.reserve(desc.max_command_count);
#if defined(FLOOR_DEBUG)
		entry->has_mesh_support = (desc.command_type == indirect_command_description::COMMAND_TYPE::RENDER_MESH);
#endif
		pipelines.insert_or_assign(dev_ptr.get(), std::move(entry));
	}
}

const generic_indirect_pipeline_entry* generic_indirect_command_pipeline::get_pipeline_entry(const device& dev) const {
	if (const auto iter = pipelines.find(&dev); iter != pipelines.end()) {
		return iter->second.get();
	}
	return nullptr;
}

generic_indirect_pipeline_entry* generic_indirect_command_pipeline::get_pipeline_entry(const device& dev) {
	if (const auto iter = pipelines.find(&dev); iter != pipelines.end()) {
		return iter->second.get();
	}
	return nullptr;
}

indirect_compute_command_encoder& generic_indirect_command_pipeline::add_compute_command(const device& dev, const device_function& kernel_obj) {
	if (desc.command_type != indirect_command_description::COMMAND_TYPE::COMPUTE) {
		throw std::runtime_error("adding compute commands to a render indirect command pipeline is not allowed");
	}
	
	const auto pipeline_entry = get_pipeline_entry(dev);
	if (!pipeline_entry) {
		throw std::runtime_error("no pipeline entry for device " + dev.name);
	}
	if (commands.size() >= desc.max_command_count) {
		throw std::runtime_error("already encoded the max amount of commands in indirect command pipeline " + desc.debug_label);
	}
	
	auto compute_enc = std::make_unique<generic_indirect_compute_command_encoder>(*pipeline_entry, dev, kernel_obj);
	auto compute_enc_ptr = compute_enc.get();
	commands.emplace_back(std::move(compute_enc));
	return *compute_enc_ptr;
}

indirect_render_command_encoder& generic_indirect_command_pipeline::add_render_command(const device& dev,
																					   const graphics_pipeline& pipeline,
																					   const bool is_multi_view) {
	const auto is_mesh_shading = (desc.command_type == indirect_command_description::COMMAND_TYPE::RENDER_MESH);
	if (desc.command_type != indirect_command_description::COMMAND_TYPE::RENDER && !is_mesh_shading) {
		throw std::runtime_error("adding render commands to a compute indirect command pipeline is not allowed");
	} else if (!dev.indirect_render_command_support) {
		throw std::runtime_error("render indirect command pipeline not supported");
	}
	
	const auto pipeline_entry = get_pipeline_entry(dev);
	if (!pipeline_entry) {
		throw std::runtime_error("no pipeline entry for device " + dev.name);
	}
	if (commands.size() >= desc.max_command_count) {
		throw std::runtime_error("already encoded the max amount of commands in indirect command pipeline " + desc.debug_label);
	}
	
	auto render_enc = std::make_unique<generic_indirect_render_command_encoder>(*pipeline_entry, dev, pipeline,
																				is_multi_view, is_mesh_shading);
	auto render_enc_ptr = render_enc.get();
	commands.emplace_back(std::move(render_enc));
	return *render_enc_ptr;
}

void generic_indirect_command_pipeline::complete(const device& dev) {
	if (!get_pipeline_entry(dev)) {
		throw std::runtime_error("no pipeline entry for device " + dev.name);
	}
}

void generic_indirect_command_pipeline::complete() {
	// nop
}

void generic_indirect_command_pipeline::reset() {
	for (auto&& pipeline : pipelines) {
		// just clear all parameters
		pipeline.second->commands.clear();
	}
	indirect_command_pipeline::reset();
}

//
generic_indirect_compute_command_encoder::generic_indirect_compute_command_encoder(generic_indirect_pipeline_entry& pipeline_entry_,
																				   const device& dev_, const device_function& kernel_obj_) :
indirect_compute_command_encoder(dev_, kernel_obj_), pipeline_entry(pipeline_entry_) {
	if (!entry || !entry->info) {
		throw std::runtime_error("state is invalid or no compute pipeline entry exists for device " + dev.name);
	}
}

void generic_indirect_compute_command_encoder::set_arguments_vector(std::vector<device_function_arg>&& args_) {
	args = std::move(args_);
}

indirect_compute_command_encoder& generic_indirect_compute_command_encoder::execute(const uint32_t dim,
																					const uint3 global_work_size,
																					const uint3 local_work_size) {
	// add command
	pipeline_entry.commands.emplace_back(generic_indirect_pipeline_entry::generic_command_t {
		.compute = {
			.kernel_ptr = &kernel_obj,
			.dim = dim,
			.global_work_size = global_work_size,
			.local_work_size = local_work_size,
		},
		.args = std::move(args),
	});
	return *this;
}

indirect_compute_command_encoder& generic_indirect_compute_command_encoder::barrier() {
	// nop if there isn't any command yet
	if (pipeline_entry.commands.empty()) {
		return *this;
	}
	// make last command blocking
	// TODO: or do we need a full queue sync/finish?
	pipeline_entry.commands.back().compute.wait_until_completion = true;
	return *this;
}

generic_indirect_render_command_encoder::generic_indirect_render_command_encoder(generic_indirect_pipeline_entry& pipeline_entry_,
																				 const device& dev_, const graphics_pipeline& pipeline_,
																				 const bool is_multi_view_,
																				 const bool is_mesh_shading_) :
indirect_render_command_encoder(dev_, pipeline_, is_multi_view_, is_mesh_shading_), pipeline_entry(pipeline_entry_) {
	const auto& desc = pipeline.get_description(is_multi_view);
	vertex_shader = desc.vertex_shader;
	fragment_shader = desc.fragment_shader;
	task_shader = desc.task_shader;
	mesh_shader = desc.mesh_shader;
	
	if (vertex_shader) {
		const auto vs_entry = vertex_shader->get_function_entry(dev);
		if (!vs_entry || !vs_entry->info) {
			throw std::runtime_error("state is invalid or no graphics pipeline entry (vertex shader) exists for device " + dev.name);
		}
		vertex_entry = vs_entry;
	}
	
	if (fragment_shader) {
		const auto fs_entry = fragment_shader->get_function_entry(dev);
		if (!fs_entry || !fs_entry->info) {
			throw std::runtime_error("state is invalid or no graphics pipeline entry (fragment shader) exists for device " + dev.name);
		}
		fragment_entry = fs_entry;
	}
	
	if (task_shader) {
		const auto ts_entry = task_shader->get_function_entry(dev);
		if (!ts_entry || !ts_entry->info) {
			throw std::runtime_error("state is invalid or no graphics pipeline entry (task shader) exists for device " + dev.name);
		}
		task_entry = ts_entry;
	}
	
	if (mesh_shader) {
		const auto ms_entry = mesh_shader->get_function_entry(dev);
		if (!ms_entry || !ms_entry->info) {
			throw std::runtime_error("state is invalid or no graphics pipeline entry (mesh shader) exists for device " + dev.name);
		}
		mesh_entry = ms_entry;
	}
	
	validate_shader_combinations(dev, vertex_shader, task_shader, mesh_shader);
}

void generic_indirect_render_command_encoder::set_arguments_vector(std::vector<device_function_arg>&& args_) {
	args = std::move(args_);
}

generic_indirect_render_command_encoder::indirect_render_command_encoder&
generic_indirect_render_command_encoder::draw(const uint32_t vertex_count,
											  const uint32_t instance_count,
											  const uint32_t first_vertex,
											  const uint32_t first_instance) {
	assert(vertex_shader);
	pipeline_entry.commands.emplace_back(generic_indirect_pipeline_entry::generic_command_t {
		.render = {
			.vs_ptr = vertex_shader,
			.fs_ptr = fragment_shader,
			.index_buffer = nullptr,
			.vertex = {
				.vertex_count = vertex_count,
				.instance_count = instance_count,
				.first_vertex = first_vertex,
				.first_instance = first_instance,
			},
		},
		.args = std::move(args),
	});
	return *this;
}

generic_indirect_render_command_encoder::indirect_render_command_encoder&
generic_indirect_render_command_encoder::draw_indexed(const device_buffer& index_buffer,
													  const uint32_t index_count,
													  const uint32_t instance_count,
													  const uint32_t first_index,
													  const int32_t vertex_offset,
													  const uint32_t first_instance,
													  const INDEX_TYPE index_type) {
	assert(vertex_shader);
	pipeline_entry.commands.emplace_back(generic_indirect_pipeline_entry::generic_command_t {
		.render = {
			.vs_ptr = vertex_shader,
			.fs_ptr = fragment_shader,
			.index_buffer = &index_buffer,
			.indexed = {
				.index_count = index_count,
				.instance_count = instance_count,
				.first_index = first_index,
				.vertex_offset = vertex_offset,
				.first_instance = first_instance,
				.index_type = index_type,
			},
		},
		.args = std::move(args),
	});
	return *this;
}

generic_indirect_render_command_encoder::indirect_render_command_encoder&
generic_indirect_render_command_encoder::draw_mesh(const uint3 work_group_count,
												   const uint3 local_work_size_task,
												   const uint3 local_work_size_mesh) {
#if defined(FLOOR_DEBUG) // NOTE: this is already checked for during encoder creation, but still account for stupid mistakes in debug mode
	if (!pipeline_entry.has_mesh_support) {
		assert(false);
		throw std::runtime_error("mesh shading is not supported");
	}
#endif
	
	assert(mesh_shader);
	pipeline_entry.commands.emplace_back(generic_indirect_pipeline_entry::generic_command_t {
		.render = {
			.ts_ptr = task_shader,
			.ms_ptr = mesh_shader,
			.fs_ptr = fragment_shader,
			.mesh = {
				.work_group_count = work_group_count,
				.local_work_size_task = local_work_size_task,
				.local_work_size_mesh = local_work_size_mesh,
			},
		},
		.args = std::move(args),
	});
	return *this;
}

indirect_render_command_encoder&
generic_indirect_render_command_encoder::draw_patches(const std::vector<const device_buffer*>, const device_buffer&,
													  const uint32_t, const uint32_t, const uint32_t, const uint32_t, const uint32_t) {
	throw std::runtime_error("tessellation is not supported");
}

indirect_render_command_encoder&
generic_indirect_render_command_encoder::draw_patches_indexed(const std::vector<const device_buffer*>, const device_buffer&,
															  const device_buffer&, const uint32_t, const uint32_t, const uint32_t,
															  const uint32_t, const uint32_t, const uint32_t) {
	throw std::runtime_error("tessellation is not supported");
}

} // namespace fl
