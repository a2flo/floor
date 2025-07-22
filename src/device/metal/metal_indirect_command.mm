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

#include <floor/core/essentials.hpp>

#if !defined(FLOOR_NO_METAL)
#include <floor/core/logger.hpp>
#include <floor/device/device_context.hpp>
#include <floor/device/metal/metal_context.hpp>
#include <floor/device/metal/metal_indirect_command.hpp>
#include <floor/device/metal/metal_device.hpp>
#include <floor/device/metal/metal_buffer.hpp>
#include <floor/device/metal/metal_pipeline.hpp>
#include <floor/device/metal/metal_args.hpp>
#include <floor/device/soft_printf.hpp>
#include <Metal/MTLIndirectCommandEncoder.h>

namespace fl {

metal_indirect_command_pipeline::metal_indirect_command_pipeline(const indirect_command_description& desc_,
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
	bool tessellation_support = true;
	for (const auto& dev : devices) {
		// NOTE: only need to check for general indirect command support here (if it's supported, both compute and render commands are supported as well)
		if (!dev->indirect_command_support) {
			log_error("specified device \"$\" has no support for indirect commands in indirect command pipeline \"$\"",
					  dev->name, desc.debug_label);
			valid = false;
			return;
		}
		if (!dev->tessellation_support) {
			tessellation_support = false;
		}
	}
	if (!desc.ignore_max_max_command_count_limit && desc.max_command_count > 16384u) {
		log_error("max supported command count by Metal is 16384, in indirect command pipeline \"$\"",
				  desc.debug_label);
		valid = false;
		return;
	}
	
	@autoreleasepool {
		// init descriptor from indirect_command_description
		MTLIndirectCommandBufferDescriptor* icb_desc = [[MTLIndirectCommandBufferDescriptor alloc] init];
		
		icb_desc.commandTypes = 0;
		if (desc.command_type == indirect_command_description::COMMAND_TYPE::RENDER) {
			icb_desc.commandTypes |= (MTLIndirectCommandTypeDraw |
									  MTLIndirectCommandTypeDrawIndexed);
			if (tessellation_support) {
				icb_desc.commandTypes |= (MTLIndirectCommandTypeDrawPatches |
										  MTLIndirectCommandTypeDrawIndexedPatches);
			}
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
			
			pipelines.insert_or_assign(dev.get(), std::move(entry));
		}
	}
}

metal_indirect_command_pipeline::~metal_indirect_command_pipeline() {
}

const metal_indirect_command_pipeline::metal_pipeline_entry* metal_indirect_command_pipeline::get_metal_pipeline_entry(const device& dev) const {
	if (const auto iter = pipelines.find(&dev); iter != pipelines.end()) {
		return &iter->second;
	}
	return nullptr;
}

metal_indirect_command_pipeline::metal_pipeline_entry* metal_indirect_command_pipeline::get_metal_pipeline_entry(const device& dev) {
	if (const auto iter = pipelines.find(&dev); iter != pipelines.end()) {
		return &iter->second;
	}
	return nullptr;
}

indirect_render_command_encoder& metal_indirect_command_pipeline::add_render_command(const device& dev,
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
	
	auto render_enc = std::make_unique<metal_indirect_render_command_encoder>(*pipeline_entry, uint32_t(commands.size()), dev, pipeline, is_multi_view_);
	auto render_enc_ptr = render_enc.get();
	commands.emplace_back(std::move(render_enc));
	return *render_enc_ptr;
}

indirect_compute_command_encoder& metal_indirect_command_pipeline::add_compute_command(const device& dev,
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
	
	auto compute_enc = std::make_unique<metal_indirect_compute_command_encoder>(*pipeline_entry, uint32_t(commands.size()), dev, kernel_obj);
	auto compute_enc_ptr = compute_enc.get();
	commands.emplace_back(std::move(compute_enc));
	return *compute_enc_ptr;
}

void metal_indirect_command_pipeline::complete(const device& dev) {
	auto pipeline_entry = get_metal_pipeline_entry(dev);
	if (!pipeline_entry) {
		throw std::runtime_error("no pipeline entry for device " + dev.name);
	}
	complete_pipeline(dev, *pipeline_entry);
}

void metal_indirect_command_pipeline::complete() {
	for (auto&& pipeline : pipelines) {
		complete_pipeline(*pipeline.first, pipeline.second);
	}
}

void metal_indirect_command_pipeline::complete_pipeline(const device& dev, metal_pipeline_entry& entry) {
	@autoreleasepool {
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
}

void metal_indirect_command_pipeline::reset() {
	@autoreleasepool {
		for (auto&& pipeline : pipelines) {
			[pipeline.second.icb resetWithRange:NSRange { 0, desc.max_command_count }];
			pipeline.second.clear_resources();
		}
		indirect_command_pipeline::reset();
	}
}

std::optional<NSRange> metal_indirect_command_pipeline::compute_and_validate_command_range(const uint32_t command_offset,
																					  const uint32_t command_count) const {
	NSRange range { command_offset, command_count };
	if (command_count == ~0u) {
		range.length = get_command_count();
	}
#if defined(FLOOR_DEBUG)
	{
		const auto cmd_count = get_command_count();
		if (command_offset != 0 || range.length != cmd_count) {
			static std::once_flag flag;
			std::call_once(flag, [] {
				// see below
				log_warn("efficient resource usage declarations when using partial command ranges is not implemented yet");
			});
		}
		if (cmd_count == 0) {
			log_warn("no commands in indirect command pipeline \"$\"", desc.debug_label);
		}
		if (range.location >= cmd_count) {
			log_error("out-of-bounds command offset $ for indirect command pipeline \"$\"",
					  range.location, desc.debug_label);
			return {};
		}
		uint32_t sum = 0;
		if (__builtin_uadd_overflow((uint32_t)range.location, (uint32_t)range.length, &sum)) {
			log_error("command offset $ + command count $ overflow for indirect command pipeline \"$\"",
					  range.location, range.length, desc.debug_label);
			return {};
		}
		if (sum > cmd_count) {
			log_error("out-of-bounds command count $ for indirect command pipeline \"$\"",
					  range.length, desc.debug_label);
			return {};
		}
	}
#endif
	// post count check, since this might have been modified, but we still want the debug messages
	if (range.length == 0) {
		return {};
	}
	
	return range;
}

metal_indirect_command_pipeline::metal_pipeline_entry::metal_pipeline_entry(metal_pipeline_entry&& entry) :
icb(entry.icb), printf_buffer(entry.printf_buffer) {
	@autoreleasepool {
		entry.icb = nil;
		entry.printf_buffer = nullptr;
	}
}

metal_indirect_command_pipeline::metal_pipeline_entry& metal_indirect_command_pipeline::metal_pipeline_entry::operator=(metal_pipeline_entry&& entry) {
	@autoreleasepool {
		icb = entry.icb;
		printf_buffer = entry.printf_buffer;
		entry.icb = nil;
		entry.printf_buffer = nullptr;
		return *this;
	}
}

metal_indirect_command_pipeline::metal_pipeline_entry::~metal_pipeline_entry() {
	@autoreleasepool {
		if (icb) {
			[icb setPurgeableState:MTLPurgeableStateEmpty];
		}
		icb = nil;
	}
}

void metal_indirect_command_pipeline::metal_pipeline_entry::printf_init(const device_queue& dev_queue) const {
	initialize_printf_buffer(dev_queue, *printf_buffer);
}

void metal_indirect_command_pipeline::metal_pipeline_entry::printf_completion(const device_queue& dev_queue, id <MTLCommandBuffer> cmd_buffer) const {
	auto internal_dev_queue = ((const metal_context&)dev_queue.get_context()).get_device_default_queue(dev_queue.get_device());
	[cmd_buffer addCompletedHandler:^(id <MTLCommandBuffer>) {
		auto cpu_printf_buffer = std::make_unique<uint32_t[]>(printf_buffer_size / 4);
		printf_buffer->read(*internal_dev_queue, cpu_printf_buffer.get());
		handle_printf_buffer(std::span { cpu_printf_buffer.get(), printf_buffer_size / 4 });
	}];
}

metal_indirect_render_command_encoder::metal_indirect_render_command_encoder(const metal_indirect_command_pipeline::metal_pipeline_entry& pipeline_entry_,
																			 const uint32_t command_idx_,
																			 const device& dev_, const graphics_pipeline& pipeline_,
																			 const bool is_multi_view_) :
indirect_render_command_encoder(dev_, pipeline_, is_multi_view_), pipeline_entry(pipeline_entry_), command_idx(command_idx_) {
	const auto& mtl_render_pipeline = (const metal_pipeline&)pipeline;
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
		has_soft_printf |= has_flag<FUNCTION_FLAGS::USES_SOFT_PRINTF>(vs_info->flags);
	}
	if (mtl_render_pipeline_entry->fs_entry) {
		fs_info = mtl_render_pipeline_entry->fs_entry->info;
		has_soft_printf |= has_flag<FUNCTION_FLAGS::USES_SOFT_PRINTF>(fs_info->flags);
	}
	command = [pipeline_entry.icb indirectRenderCommandAtIndex:command_idx];
	[command setRenderPipelineState:mtl_render_pipeline_entry->pipeline_state];
	
	if (has_soft_printf) {
		if (!pipeline_entry.printf_buffer) {
			auto internal_dev_queue = ((const metal_context*)dev.context)->get_device_default_queue(dev);
			pipeline_entry.printf_buffer = allocate_printf_buffer(*internal_dev_queue);
		}
	}
}

metal_indirect_render_command_encoder::~metal_indirect_render_command_encoder() {
	@autoreleasepool {
		command = nil;
	}
}

void metal_indirect_render_command_encoder::set_arguments_vector(std::vector<device_function_arg>&& args) {
	@autoreleasepool {
		std::vector<device_function_arg> implicit_args;
		const auto vs_has_soft_printf = (vs_info && toolchain::has_flag<toolchain::FUNCTION_FLAGS::USES_SOFT_PRINTF>(vs_info->flags));
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
		metal_args::set_and_handle_arguments<metal_args::ENCODER_TYPE::INDIRECT_SHADER>(dev, command,
																						{ vs_info, fs_info },
																						args, implicit_args,
																						nullptr,
																						&resources);
		sort_and_unique_all_resources();
	}
}

indirect_render_command_encoder& metal_indirect_render_command_encoder::draw(const uint32_t vertex_count,
																			 const uint32_t instance_count,
																			 const uint32_t first_vertex,
																			 const uint32_t first_instance) {
	const auto mtl_primitve = metal_pipeline::metal_primitive_type_from_primitive(pipeline.get_description(is_multi_view).primitive);
	[command drawPrimitives:mtl_primitve
				vertexStart:first_vertex
				vertexCount:vertex_count
			  instanceCount:instance_count
			   baseInstance:first_instance];
	return *this;
}

indirect_render_command_encoder& metal_indirect_render_command_encoder::draw_indexed(const device_buffer& index_buffer,
																					 const uint32_t index_count,
																					 const uint32_t instance_count,
																					 const uint32_t first_index,
																					 const int32_t vertex_offset,
																					 const uint32_t first_instance,
																					 const INDEX_TYPE index_type) {
	@autoreleasepool {
		const auto mtl_primitve = metal_pipeline::metal_primitive_type_from_primitive(pipeline.get_description(is_multi_view).primitive);
		const auto mtl_index_type = metal_pipeline::metal_index_type_from_index_type(index_type);
		const auto index_size = index_type_size(index_type);
		auto idx_buffer = index_buffer.get_underlying_metal_buffer_safe()->get_metal_buffer();
		[command drawIndexedPrimitives:mtl_primitve
							indexCount:index_count
							 indexType:mtl_index_type
						   indexBuffer:idx_buffer
					 indexBufferOffset:(first_index * index_size)
						 instanceCount:instance_count
							baseVertex:vertex_offset
						  baseInstance:first_instance];
		// need to track this as well!
		resources.read_only.emplace_back(idx_buffer);
		return *this;
	}
}

indirect_render_command_encoder& metal_indirect_render_command_encoder::draw_patches(const std::vector<const device_buffer*> control_point_buffers,
																					 const device_buffer& tessellation_factors_buffer,
																					 const uint32_t patch_control_point_count,
																					 const uint32_t patch_count,
																					 const uint32_t first_patch,
																					 const uint32_t instance_count,
																					 const uint32_t first_instance) {
	@autoreleasepool {
		uint32_t vbuffer_idx = 0u;
		for (const auto& vbuffer : control_point_buffers) {
			auto mtl_buf = vbuffer->get_underlying_metal_buffer_safe()->get_metal_buffer();
			[command setVertexBuffer:mtl_buf
							  offset:0u
							 atIndex:vbuffer_idx++];
			resources.read_only.emplace_back(mtl_buf);
		}
		
		auto factors_buffer = tessellation_factors_buffer.get_underlying_metal_buffer_safe()->get_metal_buffer();
		[command drawPatches:patch_control_point_count
				  patchStart:first_patch
				  patchCount:patch_count
			patchIndexBuffer:nil
	  patchIndexBufferOffset:0u
			   instanceCount:instance_count
				baseInstance:first_instance
	tessellationFactorBuffer:factors_buffer
tessellationFactorBufferOffset:0u
tessellationFactorBufferInstanceStride:0u];
		resources.read_only.emplace_back(factors_buffer);
		return *this;
	}
}

indirect_render_command_encoder& metal_indirect_render_command_encoder::draw_patches_indexed(const std::vector<const device_buffer*> control_point_buffers,
																							 const device_buffer& control_point_index_buffer,
																							 const device_buffer& tessellation_factors_buffer,
																							 const uint32_t patch_control_point_count,
																							 const uint32_t patch_count,
																							 const uint32_t first_index,
																							 const uint32_t first_patch,
																							 const uint32_t instance_count,
																							 const uint32_t first_instance) {
	@autoreleasepool {
		uint32_t vbuffer_idx = 0u;
		for (const auto& vbuffer : control_point_buffers) {
			auto mtl_buf = vbuffer->get_underlying_metal_buffer_safe()->get_metal_buffer();
			[command setVertexBuffer:mtl_buf
							  offset:0u
							 atIndex:vbuffer_idx++];
			resources.read_only.emplace_back(mtl_buf);
		}
		
		auto idx_buffer = control_point_index_buffer.get_underlying_metal_buffer_safe()->get_metal_buffer();
		auto factors_buffer = tessellation_factors_buffer.get_underlying_metal_buffer_safe()->get_metal_buffer();
		const auto index_size = index_type_size(pipeline.get_description(false).tessellation.index_type);
		[command drawIndexedPatches:patch_control_point_count
						 patchStart:first_patch
						 patchCount:patch_count
				   patchIndexBuffer:nil
			 patchIndexBufferOffset:0u
			controlPointIndexBuffer:idx_buffer
	  controlPointIndexBufferOffset:(first_index * index_size)
					  instanceCount:instance_count
					   baseInstance:first_instance
		   tessellationFactorBuffer:factors_buffer
	 tessellationFactorBufferOffset:0u
tessellationFactorBufferInstanceStride:0u];
		resources.read_only.emplace_back(idx_buffer);
		resources.read_only.emplace_back(factors_buffer);
		return *this;
	}
}

metal_indirect_compute_command_encoder::metal_indirect_compute_command_encoder(const metal_indirect_command_pipeline::metal_pipeline_entry& pipeline_entry_,
																			   const uint32_t command_idx_,
																			   const device& dev_, const device_function& kernel_obj_) :
indirect_compute_command_encoder(dev_, kernel_obj_), pipeline_entry(pipeline_entry_), command_idx(command_idx_) {
	const auto mtl_function_entry = (const metal_function::metal_function_entry*)entry;
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
	[command setComputePipelineState:(__bridge __unsafe_unretained id <MTLComputePipelineState>)mtl_function_entry->kernel_state];
	
	if (toolchain::has_flag<toolchain::FUNCTION_FLAGS::USES_SOFT_PRINTF>(mtl_function_entry->info->flags)) {
		if (!pipeline_entry.printf_buffer) {
			auto internal_dev_queue = ((const metal_context*)dev.context)->get_device_default_queue(dev);
			pipeline_entry.printf_buffer = allocate_printf_buffer(*internal_dev_queue);
		}
	}
}

metal_indirect_compute_command_encoder::~metal_indirect_compute_command_encoder() {
	@autoreleasepool {
		command = nil;
	}
}

void metal_indirect_compute_command_encoder::set_arguments_vector(std::vector<device_function_arg>&& args) {
	@autoreleasepool {
		std::vector<device_function_arg> implicit_args;
		if (toolchain::has_flag<toolchain::FUNCTION_FLAGS::USES_SOFT_PRINTF>(entry->info->flags)) {
			// NOTE: this is automatically added to the used resources
			implicit_args.emplace_back(pipeline_entry.printf_buffer);
		}
		metal_args::set_and_handle_arguments<metal_args::ENCODER_TYPE::INDIRECT_COMPUTE>(dev, command,
																						 { entry->info },
																						 args, implicit_args,
																						 nullptr,
																						 &resources);
		sort_and_unique_all_resources();
	}
}

indirect_compute_command_encoder& metal_indirect_compute_command_encoder::execute(const uint32_t dim,
																				  const uint3& global_work_size,
																				  const uint3& local_work_size) {
	// compute sizes
	auto [grid_dim, block_dim] = ((const metal_function&)kernel_obj).compute_grid_and_block_dim(*entry, dim, global_work_size, local_work_size);
	const MTLSize metal_grid_dim { grid_dim.x, grid_dim.y, grid_dim.z };
	const MTLSize metal_block_dim { block_dim.x, block_dim.y, block_dim.z };
	
	// encode
	[command concurrentDispatchThreadgroups:metal_grid_dim threadsPerThreadgroup:metal_block_dim];
	return *this;
}

indirect_compute_command_encoder& metal_indirect_compute_command_encoder::barrier() {
	[command setBarrier];
	return *this;
}

} // namespace fl

#endif
