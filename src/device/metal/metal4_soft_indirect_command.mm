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

#include <floor/core/essentials.hpp>

#if !defined(FLOOR_NO_METAL)
#include <floor/core/logger.hpp>
#include <floor/device/device_context.hpp>
#include <floor/device/metal/metal_context.hpp>
#include <floor/device/metal/metal_device.hpp>
#include <floor/device/metal/metal4_soft_indirect_command.hpp>
#include <floor/device/metal/metal4_argument_buffer.hpp>
#include <floor/device/metal/metal4_buffer.hpp>
#include <floor/device/metal/metal4_pipeline.hpp>
#include <floor/device/metal/metal4_function_entry.hpp>
#include <floor/device/metal/metal4_args.hpp>
#include <floor/device/metal/metal4_queue.hpp>
#include <floor/device/metal/metal4_queue.hpp>
#include <floor/device/soft_printf.hpp>
#include <Metal/MTLIndirectCommandEncoder.h>

namespace fl {

metal4_soft_indirect_command_pipeline::metal4_soft_indirect_command_pipeline(const indirect_command_description& desc_,
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
	
	@autoreleasepool {
		for (const auto& dev : devices) {
			const auto& mtl_dev = (const metal_device&)*dev;
			
			metal4_soft_pipeline_entry entry { mtl_dev };
			entry.debug_label = (desc.debug_label.empty() ? "generic_indirect_command_pipeline" : desc.debug_label);
			pipelines.insert_or_assign(dev.get(), std::move(entry));
		}
	}
}

metal4_soft_indirect_command_pipeline::~metal4_soft_indirect_command_pipeline() {
	@autoreleasepool {
		reset();
	}
}

metal4_soft_pipeline_entry::metal4_soft_pipeline_entry(const metal_device& mtl_dev_) : metal4_resource_tracking(mtl_dev_) {}

metal4_soft_pipeline_entry* metal4_soft_indirect_command_pipeline::get_metal_pipeline_entry(const device& dev) const {
	if (const auto iter = pipelines.find(&dev); iter != pipelines.end()) {
		return &iter->second;
	}
	return nullptr;
}

indirect_render_command_encoder& metal4_soft_indirect_command_pipeline::add_render_command(const device& dev,
																						   const graphics_pipeline& pipeline,
																						   const bool is_multi_view_) {
	if (desc.command_type != indirect_command_description::COMMAND_TYPE::RENDER) {
		throw std::runtime_error("adding render commands to a compute indirect command pipeline is not allowed");
	}
	
	const auto pipeline_entry = get_metal_pipeline_entry(dev);
	if (!pipeline_entry) {
		throw std::runtime_error("no pipeline entry for device " + dev.name);
	}
	if (commands.size() >= desc.max_command_count) {
		throw std::runtime_error("already encoded the max amount of commands in indirect command pipeline " + desc.debug_label);
	}
	
	auto render_enc = std::make_unique<metal4_soft_indirect_render_command_encoder>(*pipeline_entry, dev, pipeline, is_multi_view_);
	auto render_enc_ptr = render_enc.get();
	commands.emplace_back(std::move(render_enc));
	return *render_enc_ptr;
}

indirect_compute_command_encoder& metal4_soft_indirect_command_pipeline::add_compute_command(const device& dev,
																							 const device_function& kernel_obj) {
	if (desc.command_type != indirect_command_description::COMMAND_TYPE::COMPUTE) {
		throw std::runtime_error("adding compute commands to a render indirect command pipeline is not allowed");
	}
	
	const auto pipeline_entry = get_metal_pipeline_entry(dev);
	if (!pipeline_entry) {
		throw std::runtime_error("no pipeline entry for device " + dev.name);
	}
	if (commands.size() >= desc.max_command_count) {
		throw std::runtime_error("already encoded the max amount of commands in indirect command pipeline " + desc.debug_label);
	}
	
	auto compute_enc = std::make_unique<metal4_soft_indirect_compute_command_encoder>(*pipeline_entry, dev, kernel_obj);
	auto compute_enc_ptr = compute_enc.get();
	commands.emplace_back(std::move(compute_enc));
	return *compute_enc_ptr;
}

void metal4_soft_indirect_command_pipeline::complete(const device& dev) {
	auto pipeline_entry = get_metal_pipeline_entry(dev);
	if (!pipeline_entry) {
		throw std::runtime_error("no pipeline entry for device " + dev.name);
	}
	complete_pipeline(dev, *pipeline_entry);
}

void metal4_soft_indirect_command_pipeline::complete() {
	for (auto&& pipeline : pipelines) {
		complete_pipeline(*pipeline.first, pipeline.second);
	}
}

void metal4_soft_indirect_command_pipeline::update_resources(const device& dev, metal4_soft_pipeline_entry& entry) const {
	@autoreleasepool {
		if (entry.is_complete) {
			assert(entry.has_resources());
			// if the pipeline is already complete (i.e. resources were initialized at least once),
			// we only need to check if any argument buffers were updated
			bool is_dirty = false;
			for (size_t i = 0, count = entry.arg_buffers.size(); i < count; ++i) {
				if (entry.arg_buffers[i]->get_generation() != entry.arg_buffer_gens[i]) {
					is_dirty = true;
					break;
				}
			}
			// -> if none were updated, we don't need to do anything in here
			if (!is_dirty) [[likely]] {
				return;
			}
		}
		
		entry.clear_resources();
		
		// gather all (direct) resources for this device
		for (const auto& cmd : commands) {
			assert(cmd && &cmd->get_device() == &dev);
			if (auto cmd_res = dynamic_cast<metal4_resource_container_t*>(cmd.get()); cmd_res) {
				entry.add_resources(*cmd_res);
			}
		}
		
		// gather arg buffer resources + record their current generation
		if (!entry.arg_buffers.empty()) {
			if (!entry.is_complete) {
				if (entry.arg_buffers.size() > 1) {
					// on the first update (not complete yet): sort and unique argument buffers
					std::sort(entry.arg_buffers.begin(), entry.arg_buffers.end());
					auto last_iter = std::unique(entry.arg_buffers.begin(), entry.arg_buffers.end());
					if (last_iter != entry.arg_buffers.end()) {
						entry.arg_buffers.erase(last_iter, entry.arg_buffers.end());
					}
				}
				// also init the generations buffer
				entry.arg_buffer_gens.resize(entry.arg_buffers.size());
			}
			
			for (size_t i = 0, count = entry.arg_buffers.size(); i < count; ++i) {
				const auto& arg_buffer = entry.arg_buffers[i];
				entry.add_resources(arg_buffer->get_resources());
				entry.arg_buffer_gens[i] = arg_buffer->get_generation();
			}
		}
		
		entry.sort_and_unique_all_resources();
		entry.update_and_commit();
	}
}

void metal4_soft_indirect_command_pipeline::complete_pipeline(const device& dev, metal4_soft_pipeline_entry& entry) {
	@autoreleasepool {
		update_resources(dev, entry);
		entry.is_complete = true;
	}
}

void metal4_soft_indirect_command_pipeline::reset() {
	@autoreleasepool {
		for (auto&& pipeline : pipelines) {
			pipeline.second.compute_commands.clear();
			pipeline.second.render_commands.clear();
			pipeline.second.arg_buffers.clear();
			pipeline.second.arg_buffer_gens.clear();
			pipeline.second.clear_resources();
			pipeline.second.is_complete = false;
		}
		indirect_command_pipeline::reset();
	}
}

void metal4_soft_pipeline_entry::printf_init(const device_queue& dev_queue) const {
	initialize_printf_buffer(dev_queue, *printf_buffer);
}

metal4_soft_indirect_render_command_encoder::metal4_soft_indirect_render_command_encoder(metal4_soft_pipeline_entry& pipeline_entry_,
																						 const device& dev_,
																						 const graphics_pipeline& pipeline_,
																						 const bool is_multi_view_) :
indirect_render_command_encoder(dev_, pipeline_, is_multi_view_), metal4_resource_container_t(), pipeline_entry(pipeline_entry_) {
	@autoreleasepool {
		const auto& mtl_pipeline = (const metal4_pipeline&)pipeline;
		const auto mtl_render_pipeline_entry = mtl_pipeline.get_metal_pipeline_entry(dev);
		if (!mtl_render_pipeline_entry || !mtl_render_pipeline_entry->pipeline_state) {
			throw std::runtime_error("no render pipeline entry exists for device " + dev.name);
		}
		pipeline_state = mtl_render_pipeline_entry->pipeline_state;
		
		const auto& mtl_dev = (const metal_device&)dev;
		bool has_soft_printf = false;
		NSError* err = nil;
		if (mtl_render_pipeline_entry->vs_entry) {
			vs_info = mtl_render_pipeline_entry->vs_entry->info;
			has_soft_printf |= has_flag<toolchain::FUNCTION_FLAGS::USES_SOFT_PRINTF>(vs_info->flags);
			const auto vs_mtl_entry = (const metal4_function_entry*)mtl_render_pipeline_entry->vs_entry;
			vs_arg_table = [mtl_dev.device newArgumentTableWithDescriptor:vs_mtl_entry->arg_table_descriptor
																	error:&err];
			if (!vs_arg_table || err) {
				throw std::runtime_error("failed to create VS argument table for device " + dev.name + ": " +
										 (err ? [[err localizedDescription] UTF8String] : "<no-error-msg>"));
			}
		}
		if (mtl_render_pipeline_entry->fs_entry) {
			fs_info = mtl_render_pipeline_entry->fs_entry->info;
			has_soft_printf |= has_flag<toolchain::FUNCTION_FLAGS::USES_SOFT_PRINTF>(fs_info->flags);
			const auto fs_mtl_entry = (const metal4_function_entry*)mtl_render_pipeline_entry->fs_entry;
			fs_arg_table = [mtl_dev.device newArgumentTableWithDescriptor:fs_mtl_entry->arg_table_descriptor
																	error:&err];
			if (!fs_arg_table || err) {
				throw std::runtime_error("failed to create FS argument table for device " + dev.name +
										 (err ? [[err localizedDescription] UTF8String] : "<no-error-msg>"));
			}
		}
		
		// pipeline state is a resource that must be tracked
		add_resource(pipeline_state);
		
		if (has_soft_printf) {
			if (!pipeline_entry.printf_buffer) {
				auto internal_dev_queue = ((const metal_context*)dev.context)->get_device_default_queue(dev);
				pipeline_entry.printf_buffer = allocate_printf_buffer(*internal_dev_queue);
			}
		}
	}
}

metal4_soft_indirect_render_command_encoder::~metal4_soft_indirect_render_command_encoder() {
	@autoreleasepool {
		pipeline_state = nil;
		vs_arg_table = nil;
		fs_arg_table = nil;
	}
}

void metal4_soft_indirect_render_command_encoder::set_arguments_vector(std::vector<device_function_arg>&& args) {
	@autoreleasepool {
		assert(vs_info);
		std::vector<device_function_arg> implicit_args;
		const auto vs_has_soft_printf = toolchain::has_flag<toolchain::FUNCTION_FLAGS::USES_SOFT_PRINTF>(vs_info->flags);
		const auto fs_has_soft_printf = (fs_info && toolchain::has_flag<toolchain::FUNCTION_FLAGS::USES_SOFT_PRINTF>(fs_info->flags));
		if (vs_has_soft_printf || fs_has_soft_printf) {
			// NOTE: will use the same printf buffer here, but we need to specify it twice if both functions use it
			// NOTE: these are automatically added to the used resources
			if (vs_has_soft_printf) {
				implicit_args.emplace_back(pipeline_entry.printf_buffer);
			}
			if (fs_has_soft_printf) {
				implicit_args.emplace_back(pipeline_entry.printf_buffer);
			}
		}
		
		metal4_args::argument_table_encoders_t<2u> shader_encoders;
		shader_encoders.encoders[0] = {
			.arg_table = vs_arg_table,
			.constants_buffer = nullptr,
			.constant_buffer_info = nullptr,
		};
		if (fs_info) {
			shader_encoders.encoders[1] = {
				.arg_table = fs_arg_table,
				.constants_buffer = nullptr,
				.constant_buffer_info = nullptr,
			};
		}
		if (!metal4_args::set_and_handle_arguments<metal4_args::ENCODER_TYPE::ARGUMENT_TABLE, true, 2u>(dev, shader_encoders,
																										{ vs_info, fs_info },
																										args, implicit_args,
																										{ this, this }, nullptr,
																										&pipeline_entry.arg_buffers,
																										false)) {
			throw std::runtime_error("failed to encode shader arguments in \"" + vs_info->name + "\" + \"" +
									 (fs_info ? fs_info->name : "<no-fragment-shader>") + "\"");
		}
	}
}

indirect_render_command_encoder& metal4_soft_indirect_render_command_encoder::draw(const uint32_t vertex_count,
																				   const uint32_t instance_count,
																				   const uint32_t first_vertex,
																				   const uint32_t first_instance) {
	const auto mtl_primitve = metal4_pipeline::metal_primitive_type_from_primitive(pipeline.get_description(is_multi_view).primitive);
	pipeline_entry.render_commands.emplace_back(metal4_soft_pipeline_entry::render_command_t {
		.is_indexed = false,
		.vertex = {
			.primitive_type = mtl_primitve,
			.vertex_count = vertex_count,
			.instance_count = instance_count,
			.first_vertex = first_vertex,
			.first_instance = first_instance,
		}
	});
	return *this;
}

indirect_render_command_encoder& metal4_soft_indirect_render_command_encoder::draw_indexed(const device_buffer& index_buffer,
																						   const uint32_t index_count,
																						   const uint32_t instance_count,
																						   const uint32_t first_index,
																						   const int32_t vertex_offset,
																						   const uint32_t first_instance,
																						   const INDEX_TYPE index_type) {
	@autoreleasepool {
		const auto mtl_primitve = metal4_pipeline::metal_primitive_type_from_primitive(pipeline.get_description(is_multi_view).primitive);
		const auto mtl_index_type = metal4_pipeline::metal_index_type_from_index_type(index_type);
		const auto index_size = index_type_size(index_type);
		auto idx_buffer = index_buffer.get_underlying_metal4_buffer_safe()->get_metal_buffer();
		pipeline_entry.render_commands.emplace_back(metal4_soft_pipeline_entry::render_command_t {
			.is_indexed = true,
			.indexed = {
				.primitive_type = mtl_primitve,
				.index_type = mtl_index_type,
				.index_buffer_address = idx_buffer.gpuAddress + first_index * index_size,
				.index_buffer_length = index_count * index_size,
				.index_count = index_count,
				.instance_count = instance_count,
				.vertex_offset = vertex_offset,
				.base_instance = first_instance,
			}
		});
		
		// need to track this as well if not heap allocated
		if (!idx_buffer.heap) {
			add_resource(idx_buffer);
		}
		
		return *this;
	}
}

metal4_soft_indirect_compute_command_encoder::metal4_soft_indirect_compute_command_encoder(metal4_soft_pipeline_entry& pipeline_entry_,
																						   const device& dev_,
																						   const device_function& kernel_obj_) :
indirect_compute_command_encoder(dev_, kernel_obj_), metal4_resource_container_t(), pipeline_entry(pipeline_entry_) {
	@autoreleasepool {
		const auto mtl_function_entry = (const metal4_function_entry*)entry;
		if (!mtl_function_entry || !mtl_function_entry->kernel_state || !mtl_function_entry->info) {
			throw std::runtime_error("state is invalid or no compute pipeline entry exists for device " + dev.name);
		}
		func_info = mtl_function_entry->info;
		pipeline_state = mtl_function_entry->kernel_state;
		
		const auto& mtl_dev = (const metal_device&)dev;
		NSError* err = nil;
		arg_table = [mtl_dev.device newArgumentTableWithDescriptor:mtl_function_entry->arg_table_descriptor
															 error:&err];
		if (!arg_table || err) {
			throw std::runtime_error("failed to create kernel argument table for device " + dev.name + ": " +
									 (err ? [[err localizedDescription] UTF8String] : "<no-error-msg>"));
		}
		
		// pipeline state is a resource that must be tracked
		add_resource(pipeline_state);
		
		if (toolchain::has_flag<toolchain::FUNCTION_FLAGS::USES_SOFT_PRINTF>(mtl_function_entry->info->flags)) {
			if (!pipeline_entry.printf_buffer) {
				auto internal_dev_queue = ((const metal_context*)dev.context)->get_device_default_queue(dev);
				pipeline_entry.printf_buffer = allocate_printf_buffer(*internal_dev_queue);
			}
		}
	}
}

metal4_soft_indirect_compute_command_encoder::~metal4_soft_indirect_compute_command_encoder() {
	@autoreleasepool {
		pipeline_state = nil;
		arg_table = nil;
	}
}

void metal4_soft_indirect_compute_command_encoder::set_arguments_vector(std::vector<device_function_arg>&& args) {
	@autoreleasepool {
		std::vector<device_function_arg> implicit_args;
		if (toolchain::has_flag<toolchain::FUNCTION_FLAGS::USES_SOFT_PRINTF>(entry->info->flags)) {
			// NOTE: this is automatically added to the used resources
			implicit_args.emplace_back(pipeline_entry.printf_buffer);
		}
		metal4_args::argument_table_encoder_t args_encoder {
			.arg_table = arg_table,
			.constants_buffer = nullptr,
			.constant_buffer_info = nullptr,
		};
		if (!metal4_args::set_and_handle_arguments<metal4_args::ENCODER_TYPE::ARGUMENT_TABLE>(dev, { args_encoder }, { func_info },
																							  args, implicit_args, *this, nullptr,
																							  &pipeline_entry.arg_buffers, false)) {
			throw std::runtime_error("failed to encode kernel arguments in \"" + entry->info->name + "\"");
		}
	}
}

indirect_compute_command_encoder& metal4_soft_indirect_compute_command_encoder::execute(const uint32_t dim,
																						const uint3& global_work_size,
																						const uint3& local_work_size) {
	// compute sizes
	auto [grid_dim, block_dim] = ((const metal4_function&)kernel_obj).compute_grid_and_block_dim(*entry, dim,
																								 global_work_size, local_work_size);
	
	// encode
	pipeline_entry.compute_commands.emplace_back(metal4_soft_pipeline_entry::compute_command_t {
		.grid_dim = { grid_dim.x, grid_dim.y, grid_dim.z },
		.block_dim = { block_dim.x, block_dim.y, block_dim.z },
		.wait_until_completion = false,
	});
	
	return *this;
}

indirect_compute_command_encoder& metal4_soft_indirect_compute_command_encoder::barrier() {
	// nop if there isn't any command yet
	if (pipeline_entry.compute_commands.empty()) {
		return *this;
	}
	
	// make last command blocking
	pipeline_entry.compute_commands.back().wait_until_completion = true;
	return *this;
}

} // namespace fl

#endif
