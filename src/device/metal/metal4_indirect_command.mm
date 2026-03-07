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
#include <floor/device/metal/metal4_indirect_command.hpp>
#include <floor/device/metal/metal4_buffer.hpp>
#include <floor/device/metal/metal4_pipeline.hpp>
#include <floor/device/metal/metal4_args.hpp>
#include <floor/device/metal/metal4_queue.hpp>
#include <floor/device/metal/metal4_queue.hpp>
#include <floor/device/soft_printf.hpp>
#include <Metal/MTLIndirectCommandEncoder.h>

namespace fl {

metal4_indirect_command_pipeline::metal4_indirect_command_pipeline(const indirect_command_description& desc_,
																   const std::vector<std::unique_ptr<device>>& devices) :
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
	if (!desc.ignore_max_max_command_count_limit && desc.max_command_count > 16384u) {
		log_error("max supported command count by Metal is 16384, in indirect command pipeline \"$\"",
				  desc.debug_label);
		valid = false;
		return;
	}
	
	@autoreleasepool {
		// init descriptor from indirect_command_description
		MTLIndirectCommandBufferDescriptor* icb_desc = [MTLIndirectCommandBufferDescriptor new];
		
		icb_desc.commandTypes = 0;
		if (desc.command_type == indirect_command_description::COMMAND_TYPE::RENDER) {
			icb_desc.commandTypes |= (MTLIndirectCommandTypeDraw |
									  MTLIndirectCommandTypeDrawIndexed);
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
		icb_desc.inheritPipelineState = false;
		icb_desc.inheritBuffers = false;
		
		// disable everything we don't support or use right now
		icb_desc.maxKernelThreadgroupMemoryBindCount = 0u;
		icb_desc.maxObjectBufferBindCount = 0;
		icb_desc.maxMeshBufferBindCount = 0;
		icb_desc.maxObjectThreadgroupMemoryBindCount = 0;
		icb_desc.supportRayTracing = false;
		icb_desc.supportDynamicAttributeStride = false;
		icb_desc.supportColorAttachmentMapping = false;
		
		for (const auto& dev : devices) {
			const auto& mtl_dev = (const metal_device&)*dev;
			
			// create the actual indirect command buffer for this device
			metal_pipeline_entry entry { mtl_dev };
			entry.icb = [mtl_dev.device newIndirectCommandBufferWithDescriptor:icb_desc
															   maxCommandCount:desc.max_command_count
																	   options:(MTLResourceCPUCacheModeDefaultCache |
																				MTLResourceStorageModeShared |
																				MTLResourceHazardTrackingModeUntracked)];
			if (entry.icb == nil) {
				log_error("failed to create indirect command buffer for device \"$\" in indirect command pipeline \"$\"",
						  dev->name, desc.debug_label);
				return;
			}
			[entry.icb setPurgeableState:MTLPurgeableStateNonVolatile];
			
			entry.icb.label = (desc.debug_label.empty() ? @"metal_icb" :
							   [NSString stringWithUTF8String:(desc.debug_label).c_str()]);
			
#if FLOOR_METAL_DEBUG_RS
			mtl_dev.add_debug_allocation(entry.icb);
#endif
			
			pipelines.insert_or_assign(dev.get(), std::move(entry));
		}
	}
}

metal4_indirect_command_pipeline::~metal4_indirect_command_pipeline() {
	@autoreleasepool {
		reset();
#if FLOOR_METAL_DEBUG_RS
		for (auto&& pipeline : pipelines) {
			((const metal_device*)pipeline.first)->remove_debug_allocation(pipeline.second.icb);
		}
#endif
	}
}

metal4_indirect_command_pipeline::metal_pipeline_entry::metal_pipeline_entry(const metal_device& mtl_dev_) : metal4_resource_tracking(mtl_dev_) {}

metal4_indirect_command_pipeline::metal_pipeline_entry* metal4_indirect_command_pipeline::get_metal_pipeline_entry(const device& dev) const {
	if (const auto iter = pipelines.find(&dev); iter != pipelines.end()) {
		return &iter->second;
	}
	return nullptr;
}

indirect_render_command_encoder& metal4_indirect_command_pipeline::add_render_command(const device& dev,
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
	
	auto render_enc = std::make_unique<metal4_indirect_render_command_encoder>(*pipeline_entry, uint32_t(commands.size()), dev, pipeline, is_multi_view_);
	auto render_enc_ptr = render_enc.get();
	commands.emplace_back(std::move(render_enc));
	return *render_enc_ptr;
}

indirect_compute_command_encoder& metal4_indirect_command_pipeline::add_compute_command(const device& dev,
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
	
	auto compute_enc = std::make_unique<metal4_indirect_compute_command_encoder>(*pipeline_entry, uint32_t(commands.size()), dev, kernel_obj);
	auto compute_enc_ptr = compute_enc.get();
	commands.emplace_back(std::move(compute_enc));
	return *compute_enc_ptr;
}

void metal4_indirect_command_pipeline::complete(const device& dev) {
	auto pipeline_entry = get_metal_pipeline_entry(dev);
	if (!pipeline_entry) {
		throw std::runtime_error("no pipeline entry for device " + dev.name);
	}
	complete_pipeline(dev, *pipeline_entry);
}

void metal4_indirect_command_pipeline::complete() {
	for (auto&& pipeline : pipelines) {
		complete_pipeline(*pipeline.first, pipeline.second);
	}
}

void metal4_indirect_command_pipeline::update_resources(const device& dev, metal_pipeline_entry& entry) const {
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
		
		// ICB itself is also a resource that must be tracked
		entry.resource_container.add_resource(entry.icb);
		
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

void metal4_indirect_command_pipeline::complete_pipeline(const device& dev, metal_pipeline_entry& entry) {
	@autoreleasepool {
		const auto dev_queue = dev.context->get_device_default_queue(dev);
		const auto& mtl_queue = *(const metal4_queue*)dev_queue;
		
		update_resources(dev, entry);
		
		// optimize contained commands
		MTL4_CMD_BLOCK_RET(mtl_queue, "optimize_indirect", ({
			if (![entry.icb heap]) {
				[block_cmd_buffer.residency_set addAllocation:entry.icb];
				[block_cmd_buffer.residency_set commit];
			}
			id <MTL4ComputeCommandEncoder> encoder = [block_cmd_buffer.cmd_buffer computeCommandEncoder];
			[encoder optimizeIndirectCommandBuffer:entry.icb withRange:NSRange { .location = 0, .length = get_command_count() }];
			[encoder endEncoding];
		}), /* no return, ignore failure */, true);
		
		entry.is_complete = true;
	}
}

void metal4_indirect_command_pipeline::reset() {
	@autoreleasepool {
		for (auto&& pipeline : pipelines) {
			[pipeline.second.icb resetWithRange:NSRange { 0, desc.max_command_count }];
			pipeline.second.arg_buffers.clear();
			pipeline.second.arg_buffer_gens.clear();
			pipeline.second.clear_resources();
			pipeline.second.is_complete = false;
		}
		indirect_command_pipeline::reset();
	}
}

metal4_indirect_command_pipeline::metal_pipeline_entry::metal_pipeline_entry(metal_pipeline_entry&& entry) :
metal4_resource_tracking(*entry.get_device()), icb(entry.icb), printf_buffer(entry.printf_buffer) {
	@autoreleasepool {
		entry.icb = nil;
		entry.printf_buffer = nullptr;
	}
}

metal4_indirect_command_pipeline::metal_pipeline_entry& metal4_indirect_command_pipeline::metal_pipeline_entry::operator=(metal_pipeline_entry&& entry) {
	@autoreleasepool {
		icb = entry.icb;
		printf_buffer = entry.printf_buffer;
		entry.icb = nil;
		entry.printf_buffer = nullptr;
		return *this;
	}
}

metal4_indirect_command_pipeline::metal_pipeline_entry::~metal_pipeline_entry() {
	@autoreleasepool {
		if (icb) {
			[icb setPurgeableState:MTLPurgeableStateEmpty];
		}
		icb = nil;
	}
}

void metal4_indirect_command_pipeline::metal_pipeline_entry::printf_init(const device_queue& dev_queue) const {
	initialize_printf_buffer(dev_queue, *printf_buffer);
}

metal4_indirect_render_command_encoder::metal4_indirect_render_command_encoder(metal4_indirect_command_pipeline::metal_pipeline_entry& pipeline_entry_,
																			   const uint32_t command_idx_,
																			   const device& dev_, const graphics_pipeline& pipeline_,
																			   const bool is_multi_view_) :
indirect_render_command_encoder(dev_, pipeline_, is_multi_view_),
metal4_resource_container_t(),
pipeline_entry(pipeline_entry_), command_idx(command_idx_) {
	@autoreleasepool {
		const auto& mtl_render_pipeline = (const metal4_pipeline&)pipeline;
		const auto mtl_render_pipeline_entry = mtl_render_pipeline.get_metal_pipeline_entry(dev);
		if (!mtl_render_pipeline_entry || !mtl_render_pipeline_entry->pipeline_state) {
			throw std::runtime_error("no render pipeline entry exists for device " + dev.name);
		}
#if defined(FLOOR_DEBUG)
		const auto& desc = mtl_render_pipeline.get_description(is_multi_view);
		if (!desc.support_indirect_rendering) {
			log_error("graphics pipeline \"$\" specified for indirect render command does not support indirect rendering",
					  desc.debug_label);
			return;
		}
#endif
		bool has_soft_printf = false;
		if (mtl_render_pipeline_entry->vs_entry) {
			vs_info = mtl_render_pipeline_entry->vs_entry->info;
			has_soft_printf |= has_flag<toolchain::FUNCTION_FLAGS::USES_SOFT_PRINTF>(vs_info->flags);
		}
		if (mtl_render_pipeline_entry->fs_entry) {
			fs_info = mtl_render_pipeline_entry->fs_entry->info;
			has_soft_printf |= has_flag<toolchain::FUNCTION_FLAGS::USES_SOFT_PRINTF>(fs_info->flags);
		}
		command = [pipeline_entry.icb indirectRenderCommandAtIndex:command_idx];
		[command setRenderPipelineState:mtl_render_pipeline_entry->pipeline_state];
		
		// pipeline state is a resource that must be tracked
		add_resource(mtl_render_pipeline_entry->pipeline_state);
		
		if (has_soft_printf) {
			if (!pipeline_entry.printf_buffer) {
				auto internal_dev_queue = ((const metal_context*)dev.context)->get_device_default_queue(dev);
				pipeline_entry.printf_buffer = allocate_printf_buffer(*internal_dev_queue);
			}
		}
	}
}

metal4_indirect_render_command_encoder::~metal4_indirect_render_command_encoder() {
	@autoreleasepool {
		command = nil;
	}
}

void metal4_indirect_render_command_encoder::set_arguments_vector(std::vector<device_function_arg>&& args) {
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
		if (!metal4_args::set_and_handle_arguments<metal4_args::ENCODER_TYPE::INDIRECT_SHADER>(dev, command, { vs_info, fs_info },
																							   args, implicit_args, *this, nullptr,
																							   &pipeline_entry.arg_buffers, false)) {
			throw std::runtime_error("failed to encode shader arguments in \"" + vs_info->name + "\" + \"" +
									 (fs_info ? fs_info->name : "<no-fragment-shader>") + "\"");
		}
	}
}

indirect_render_command_encoder& metal4_indirect_render_command_encoder::draw(const uint32_t vertex_count,
																			  const uint32_t instance_count,
																			  const uint32_t first_vertex,
																			  const uint32_t first_instance) {
	const auto mtl_primitve = metal4_pipeline::metal_primitive_type_from_primitive(pipeline.get_description(is_multi_view).primitive);
	[command drawPrimitives:mtl_primitve
				vertexStart:first_vertex
				vertexCount:vertex_count
			  instanceCount:instance_count
			   baseInstance:first_instance];
	return *this;
}

indirect_render_command_encoder& metal4_indirect_render_command_encoder::draw_indexed(const device_buffer& index_buffer,
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
		[command drawIndexedPrimitives:mtl_primitve
							indexCount:index_count
							 indexType:mtl_index_type
						   indexBuffer:idx_buffer
					 indexBufferOffset:(first_index * index_size)
						 instanceCount:instance_count
							baseVertex:vertex_offset
						  baseInstance:first_instance];
		// need to track this as well if not heap allocated
		if (!idx_buffer.heap) {
			add_resource(idx_buffer);
		}
		return *this;
	}
}

indirect_render_command_encoder& metal4_indirect_render_command_encoder::draw_patches(const std::vector<const device_buffer*> control_point_buffers [[maybe_unused]],
																					  const device_buffer& tessellation_factors_buffer [[maybe_unused]],
																					  const uint32_t patch_control_point_count [[maybe_unused]],
																					  const uint32_t patch_count [[maybe_unused]],
																					  const uint32_t first_patch [[maybe_unused]],
																					  const uint32_t instance_count [[maybe_unused]],
																					  const uint32_t first_instance [[maybe_unused]]) {
	throw std::runtime_error("tessellation is not supported in Metal 4");
}

indirect_render_command_encoder& metal4_indirect_render_command_encoder::draw_patches_indexed(const std::vector<const device_buffer*> control_point_buffers [[maybe_unused]],
																							  const device_buffer& control_point_index_buffer [[maybe_unused]],
																							  const device_buffer& tessellation_factors_buffer [[maybe_unused]],
																							  const uint32_t patch_control_point_count [[maybe_unused]],
																							  const uint32_t patch_count [[maybe_unused]],
																							  const uint32_t first_index [[maybe_unused]],
																							  const uint32_t first_patch [[maybe_unused]],
																							  const uint32_t instance_count [[maybe_unused]],
																							  const uint32_t first_instance [[maybe_unused]]) {
	throw std::runtime_error("tessellation is not supported in Metal 4");
}

metal4_indirect_compute_command_encoder::metal4_indirect_compute_command_encoder(metal4_indirect_command_pipeline::metal_pipeline_entry& pipeline_entry_,
																				 const uint32_t command_idx_,
																				 const device& dev_, const device_function& kernel_obj_) :
indirect_compute_command_encoder(dev_, kernel_obj_),
metal4_resource_container_t(),
pipeline_entry(pipeline_entry_), command_idx(command_idx_) {
	@autoreleasepool {
		const auto mtl_function_entry = (const metal4_function_entry*)entry;
		if (!mtl_function_entry || !mtl_function_entry->kernel_state || !mtl_function_entry->info) {
			throw std::runtime_error("state is invalid or no compute pipeline entry exists for device " + dev.name);
		}
#if defined(FLOOR_DEBUG)
		if (!mtl_function_entry->supports_indirect_compute) {
			log_error("kernel \"$\" specified for indirect compute command does not support indirect compute", mtl_function_entry->info->name);
			return;
		}
#endif
		command = [pipeline_entry.icb indirectComputeCommandAtIndex:command_idx];
		[command setComputePipelineState:mtl_function_entry->kernel_state];
		
		// pipeline state is a resource that must be tracked
		add_resource(mtl_function_entry->kernel_state);
		
		if (toolchain::has_flag<toolchain::FUNCTION_FLAGS::USES_SOFT_PRINTF>(mtl_function_entry->info->flags)) {
			if (!pipeline_entry.printf_buffer) {
				auto internal_dev_queue = ((const metal_context*)dev.context)->get_device_default_queue(dev);
				pipeline_entry.printf_buffer = allocate_printf_buffer(*internal_dev_queue);
			}
		}
	}
}

metal4_indirect_compute_command_encoder::~metal4_indirect_compute_command_encoder() {
	@autoreleasepool {
		command = nil;
	}
}

void metal4_indirect_compute_command_encoder::set_arguments_vector(std::vector<device_function_arg>&& args) {
	@autoreleasepool {
		std::vector<device_function_arg> implicit_args;
		if (toolchain::has_flag<toolchain::FUNCTION_FLAGS::USES_SOFT_PRINTF>(entry->info->flags)) {
			// NOTE: this is automatically added to the used resources
			implicit_args.emplace_back(pipeline_entry.printf_buffer);
		}
		if (!metal4_args::set_and_handle_arguments<metal4_args::ENCODER_TYPE::INDIRECT_COMPUTE>(dev, command, { entry->info },
																								args, implicit_args, *this, nullptr,
																								&pipeline_entry.arg_buffers, false)) {
			throw std::runtime_error("failed to encode kernel arguments in \"" + entry->info->name + "\"");
		}
	}
}

indirect_compute_command_encoder& metal4_indirect_compute_command_encoder::execute(const uint32_t dim,
																				   const uint3& global_work_size,
																				   const uint3& local_work_size) {
	// compute sizes
	auto [grid_dim, block_dim] = ((const metal4_function&)kernel_obj).compute_grid_and_block_dim(*entry, dim,
																								 global_work_size, local_work_size);
	const MTLSize metal_grid_dim { grid_dim.x, grid_dim.y, grid_dim.z };
	const MTLSize metal_block_dim { block_dim.x, block_dim.y, block_dim.z };
	
	// encode
	[command concurrentDispatchThreadgroups:metal_grid_dim threadsPerThreadgroup:metal_block_dim];
	return *this;
}

indirect_compute_command_encoder& metal4_indirect_compute_command_encoder::barrier() {
	[command setBarrier];
	return *this;
}

} // namespace fl

#endif
