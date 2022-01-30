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

#include <floor/core/essentials.hpp>

#if !defined(FLOOR_NO_METAL)
#include <floor/core/logger.hpp>
#include <floor/compute/metal/metal_indirect_command.hpp>
#include <floor/compute/metal/metal_device.hpp>
#include <floor/compute/metal/metal_buffer.hpp>
#include <floor/graphics/metal/metal_pipeline.hpp>
#include <floor/compute/metal/metal_args.hpp>
#include <Metal/MTLIndirectCommandEncoder.h>

metal_indirect_command_pipeline::metal_indirect_command_pipeline(const indirect_command_description& desc_,
																 const vector<unique_ptr<compute_device>>& devices) :
indirect_command_pipeline(desc_) {
	if (!valid) {
		return;
	}
	if (devices.empty()) {
		log_error("no devices specified in indirect command pipeline \"$\"",
				  desc.debug_label);
		valid = false;
		return;
	}
	for (const auto& dev : devices) {
		// NOTE: only need to check for general indirect command support here (if it's supported, both compute and render commands are supported as well)
		if (!dev->indirect_command_support) {
			log_error("specified device \"$\" has no support for indirect commands in indirect command pipeline \"$\"",
					  dev->name, desc.debug_label);
			valid = false;
			return;
		}
	}
	if (desc.max_command_count > 16384u) {
		log_error("max supported command count by Metal is 16384, in indirect command pipeline \"$\"",
				  desc.debug_label);
		valid = false;
		return;
	}
	
	// init descriptor from indirect_command_description
	MTLIndirectCommandBufferDescriptor* icb_desc = [[MTLIndirectCommandBufferDescriptor alloc] init];
	
	icb_desc.commandTypes = 0;
	if (desc.command_type == indirect_command_description::COMMAND_TYPE::RENDER) {
		icb_desc.commandTypes |= (MTLIndirectCommandTypeDraw |
								  MTLIndirectCommandTypeDrawIndexed /*| // TODO: enable once there is tessellation support
								  MTLIndirectCommandTypeDrawPatches |
								  MTLIndirectCommandTypeDrawIndexedPatches*/);
		icb_desc.maxVertexBufferBindCount = desc.max_vertex_buffer_count;
		icb_desc.maxFragmentBufferBindCount = desc.max_fragment_buffer_count;
		icb_desc.maxKernelBufferBindCount = 0;
	} else if (desc.command_type == indirect_command_description::COMMAND_TYPE::COMPUTE) {
		// NOTE: will only ever dispatch/execute equal-sized threadgroups
		icb_desc.commandTypes |= MTLIndirectCommandTypeConcurrentDispatch;
		icb_desc.maxKernelBufferBindCount = desc.max_kernel_buffer_count;
		icb_desc.maxVertexBufferBindCount = 0;
		icb_desc.maxFragmentBufferBindCount = 0;
	}
	
	// we never want to inherit anything
	icb_desc.inheritPipelineState = NO;
	icb_desc.inheritBuffers = NO;
	
	for (const auto& dev : devices) {
		const auto& mtl_dev = ((const metal_device&)*dev).device;
		
		// create the actual indirect command buffer for this device
		metal_pipeline_entry entry;
		entry.icb = [mtl_dev newIndirectCommandBufferWithDescriptor:icb_desc
													maxCommandCount:desc.max_command_count
															options:0];
		if (entry.icb == nil) {
			log_error("failed to create indirect command buffer for device \"$\" in indirect command pipeline \"$\"",
					  dev->name, desc.debug_label);
			return;
		}
		
		entry.icb.label = (desc.debug_label.empty() ? @"metal_icb" :
						   [NSString stringWithUTF8String:(desc.debug_label).c_str()]);
		
		pipelines.insert(*dev, entry);
	}
}

metal_indirect_command_pipeline::~metal_indirect_command_pipeline() {
}

const metal_indirect_command_pipeline::metal_pipeline_entry* metal_indirect_command_pipeline::get_metal_pipeline_entry(const compute_device& dev) const {
	const auto ret = pipelines.get(dev);
	return !ret.first ? nullptr : &ret.second->second;
}

metal_indirect_command_pipeline::metal_pipeline_entry* metal_indirect_command_pipeline::get_metal_pipeline_entry(const compute_device& dev) {
	auto ret = pipelines.get(dev);
	return !ret.first ? nullptr : &ret.second->second;
}

indirect_render_command_encoder& metal_indirect_command_pipeline::add_render_command(const compute_queue& dev_queue, const graphics_pipeline& pipeline) {
	if (desc.command_type != indirect_command_description::COMMAND_TYPE::RENDER) {
		throw runtime_error("adding render commands to a compute indirect command pipeline is not allowed");
	}
	
	const auto pipeline_entry = get_metal_pipeline_entry(dev_queue.get_device());
	if (!pipeline_entry) {
		throw runtime_error("no pipeline entry for device " + dev_queue.get_device().name);
	}
	if (commands.size() >= desc.max_command_count) {
		throw runtime_error("already encoded the max amount of commands in indirect command pipeline " + desc.debug_label);
	}
	
	auto render_enc = make_unique<metal_indirect_render_command_encoder>(*pipeline_entry, uint32_t(commands.size()), dev_queue, pipeline);
	auto render_enc_ptr = render_enc.get();
	commands.emplace_back(move(render_enc));
	return *render_enc_ptr;
}

indirect_compute_command_encoder& metal_indirect_command_pipeline::add_compute_command(const compute_queue& dev_queue, const compute_kernel& kernel_obj) {
	if (desc.command_type != indirect_command_description::COMMAND_TYPE::COMPUTE) {
		throw runtime_error("adding compute commands to a render indirect command pipeline is not allowed");
	}
	
	const auto pipeline_entry = get_metal_pipeline_entry(dev_queue.get_device());
	if (!pipeline_entry) {
		throw runtime_error("no pipeline entry for device " + dev_queue.get_device().name);
	}
	if (commands.size() >= desc.max_command_count) {
		throw runtime_error("already encoded the max amount of commands in indirect command pipeline " + desc.debug_label);
	}
	
	auto compute_enc = make_unique<metal_indirect_compute_command_encoder>(*pipeline_entry, uint32_t(commands.size()), dev_queue, kernel_obj);
	auto compute_enc_ptr = compute_enc.get();
	commands.emplace_back(move(compute_enc));
	return *compute_enc_ptr;
}

void metal_indirect_command_pipeline::complete(const compute_device& dev) {
	auto pipeline_entry = get_metal_pipeline_entry(dev);
	if (!pipeline_entry) {
		throw runtime_error("no pipeline entry for device " + dev.name);
	}
	complete_pipeline(dev, *pipeline_entry);
}

void metal_indirect_command_pipeline::complete() {
	for (auto& pipeline : pipelines) {
		complete_pipeline(pipeline.first, pipeline.second);
	}
}

void metal_indirect_command_pipeline::complete_pipeline(const compute_device& dev, metal_pipeline_entry& entry) {
	// gather all resources for this device
	entry.clear_resources();
	for (const auto& cmd : commands) {
		if (!cmd || &cmd->get_device() != &dev) {
			continue;
		}
		if (auto res_tracking = dynamic_cast<metal_resource_tracking*>(cmd.get()); res_tracking) {
			entry.add_resources(res_tracking->get_resources());
		}
	}
	entry.sort_and_unique_all_resources();
}

void metal_indirect_command_pipeline::reset() {
	for (auto& pipeline : pipelines) {
		[pipeline.second.icb resetWithRange:NSRange { 0, desc.max_command_count }];
		pipeline.second.clear_resources();
	}
	indirect_command_pipeline::reset();
}

metal_indirect_render_command_encoder::metal_indirect_render_command_encoder(const metal_indirect_command_pipeline::metal_pipeline_entry& pipeline_entry_,
																			 const uint32_t command_idx_,
																			 const compute_queue& dev_queue_, const graphics_pipeline& pipeline_) :
indirect_render_command_encoder(dev_queue_, pipeline_), pipeline_entry(pipeline_entry_), command_idx(command_idx_) {
	const auto& mtl_render_pipeline = (const metal_pipeline&)pipeline;
	const auto mtl_render_pipeline_entry = mtl_render_pipeline.get_metal_pipeline_entry(dev_queue.get_device());
	if (!mtl_render_pipeline_entry || !mtl_render_pipeline_entry->pipeline_state) {
		throw runtime_error("no render pipeline entry exists for device " + dev_queue.get_device().name);
	}
#if defined(FLOOR_DEBUG)
	const auto& desc = mtl_render_pipeline.get_description(false);
	if (!desc.support_indirect_rendering) {
		log_error("graphics pipeline \"$\" specified for indirect render command does not support indirect rendering",
				  desc.debug_label);
		return;
	}
#endif
	if (mtl_render_pipeline_entry->vs_entry) {
		vs_info = mtl_render_pipeline_entry->vs_entry->info;
	}
	if (mtl_render_pipeline_entry->fs_entry) {
		fs_info = mtl_render_pipeline_entry->fs_entry->info;
	}
	command = [pipeline_entry.icb indirectRenderCommandAtIndex:command_idx];
	[command setRenderPipelineState:mtl_render_pipeline_entry->pipeline_state];
}

metal_indirect_render_command_encoder::~metal_indirect_render_command_encoder() {
	// nop
}

void metal_indirect_render_command_encoder::set_arguments_vector(const vector<compute_kernel_arg>& args) {
	metal_args::set_and_handle_arguments<metal_args::ENCODER_TYPE::INDIRECT_SHADER>(dev_queue.get_device(), command,
																					{ vs_info, fs_info },
																					args, {},
																					nullptr,
																					&resources);
	sort_and_unique_all_resources();
}

indirect_render_command_encoder& metal_indirect_render_command_encoder::draw(const uint32_t vertex_count,
																			 const uint32_t instance_count,
																			 const uint32_t first_vertex,
																			 const uint32_t first_instance) {
	const auto mtl_primitve = metal_pipeline::metal_primitive_type_from_primitive(pipeline.get_description(false).primitive);
	[command drawPrimitives:mtl_primitve
				vertexStart:first_vertex
				vertexCount:vertex_count
			  instanceCount:instance_count
			   baseInstance:first_instance];
	return *this;
}

indirect_render_command_encoder& metal_indirect_render_command_encoder::draw_indexed(const compute_buffer& index_buffer,
																					 const uint32_t index_count,
																					 const uint32_t instance_count,
																					 const uint32_t first_index,
																					 const int32_t vertex_offset,
																					 const uint32_t first_instance) {
	const auto mtl_primitve = metal_pipeline::metal_primitive_type_from_primitive(pipeline.get_description(false).primitive);
	auto idx_buffer = ((const metal_buffer&)index_buffer).get_metal_buffer();
	[command drawIndexedPrimitives:mtl_primitve
						indexCount:index_count
						 indexType:MTLIndexTypeUInt32
					   indexBuffer:idx_buffer
				 indexBufferOffset:first_index
					 instanceCount:instance_count
						baseVertex:vertex_offset
					  baseInstance:first_instance];
	// need to track this as well!
	resources.read_only.emplace_back(idx_buffer);
	return *this;
}

metal_indirect_compute_command_encoder::metal_indirect_compute_command_encoder(const metal_indirect_command_pipeline::metal_pipeline_entry& pipeline_entry_,
																			   const uint32_t command_idx_,
																			   const compute_queue& dev_queue_, const compute_kernel& kernel_obj_) :
indirect_compute_command_encoder(dev_queue_, kernel_obj_), pipeline_entry(pipeline_entry_), command_idx(command_idx_) {
	const auto& mtl_kernel = (const metal_kernel&)kernel_obj_;
	const auto mtl_kernel_entry = (const metal_kernel::metal_kernel_entry*)mtl_kernel.get_kernel_entry(dev_queue.get_device());
	if (!mtl_kernel_entry || !mtl_kernel_entry->kernel_state || !mtl_kernel_entry->info) {
		throw runtime_error("state is invalid or no compute pipeline entry exists for device " + dev_queue.get_device().name);
	}
#if defined(FLOOR_DEBUG)
	if (!mtl_kernel_entry->supports_indirect_compute) {
		log_error("kernel \"$\" specified for indirect compute command does not support indirect compute", mtl_kernel_entry->info->name);
		return;
	}
#endif
	kernel_entry = mtl_kernel_entry;
	command = [pipeline_entry.icb indirectComputeCommandAtIndex:command_idx];
	[command setComputePipelineState:(__bridge id <MTLComputePipelineState>)mtl_kernel_entry->kernel_state];
}

metal_indirect_compute_command_encoder::~metal_indirect_compute_command_encoder() {
	// nop
}

void metal_indirect_compute_command_encoder::set_arguments_vector(const vector<compute_kernel_arg>& args) {
	metal_args::set_and_handle_arguments<metal_args::ENCODER_TYPE::INDIRECT_COMPUTE>(dev_queue.get_device(), command,
																					 { kernel_entry->info },
																					 args, {},
																					 nullptr,
																					 &resources);
	sort_and_unique_all_resources();
}

indirect_compute_command_encoder& metal_indirect_compute_command_encoder::execute(const uint32_t dim,
																				  const uint3& global_work_size,
																				  const uint3& local_work_size) {
	// compute sizes
	auto [grid_dim, block_dim] = ((const metal_kernel&)kernel_obj).compute_grid_and_block_dim(*kernel_entry, dim, global_work_size, local_work_size);
	const MTLSize metal_grid_dim { grid_dim.x, grid_dim.y, grid_dim.z };
	const MTLSize metal_block_dim { block_dim.x, block_dim.y, block_dim.z };
	
	// encode
	[command concurrentDispatchThreadgroups:metal_grid_dim threadsPerThreadgroup:metal_block_dim];
	return *this;
}

#endif
