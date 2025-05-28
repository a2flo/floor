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

#include <floor/device/metal/metal_queue.hpp>

#if !defined(FLOOR_NO_METAL)

#include <floor/darwin/darwin_helper.hpp>
#include <floor/device/metal/metal_indirect_command.hpp>
#include <floor/device/metal/metal_fence.hpp>
#include <floor/device/metal/metal_device.hpp>

namespace fl {

metal_queue::metal_queue(const device& dev_, id <MTLCommandQueue> queue_) : device_queue(dev_, QUEUE_TYPE::ALL), queue(queue_) {}

metal_queue::~metal_queue() {
	@autoreleasepool {
		queue = nil;
	}
}

void metal_queue::finish() const {
	@autoreleasepool {
		// need to copy current set of command buffers, so we don't deadlock when removing completed command buffers
		decltype(cmd_buffers) cur_cmd_buffers;
		{
			GUARD(cmd_buffers_lock);
			cur_cmd_buffers = cmd_buffers;
		}
		for (const auto& cmd_buffer : cur_cmd_buffers) {
			// we may only call "waitUntilCompleted" once the command buffer has been committed
			while ([cmd_buffer.first status] < MTLCommandBufferStatusCommitted) {
				std::this_thread::yield();
			}
			[cmd_buffer.first waitUntilCompleted];
		}
	}
}

void metal_queue::flush() const {
	@autoreleasepool {
		// need to copy current set of command buffers, so we don't deadlock when removing completed command buffers
		decltype(cmd_buffers) cur_cmd_buffers;
		{
			GUARD(cmd_buffers_lock);
			cur_cmd_buffers = cmd_buffers;
		}
		for(const auto& cmd_buffer : cur_cmd_buffers) {
			[cmd_buffer.first waitUntilScheduled];
		}
	}
}

void metal_queue::execute_indirect(const indirect_command_pipeline& indirect_cmd,
								   const indirect_execution_parameters_t& params,
								   kernel_completion_handler_f&& completion_handler,
								   const uint32_t command_offset,
								   const uint32_t command_count) const {
	if (command_count == 0) {
		return;
	}
	
#if defined(FLOOR_DEBUG)
	if (indirect_cmd.get_description().command_type != indirect_command_description::COMMAND_TYPE::COMPUTE) {
		log_error("specified indirect command pipeline \"$\" must be a compute pipeline",
				  indirect_cmd.get_description().debug_label);
		return;
	}
#endif
	
	@autoreleasepool {
		const auto& mtl_indirect_cmd = (const metal_indirect_command_pipeline&)indirect_cmd;
		const auto mtl_indirect_pipeline_entry = mtl_indirect_cmd.get_metal_pipeline_entry(dev);
		if (!mtl_indirect_pipeline_entry) {
			log_error("no indirect command pipeline state for device \"$\" in indirect command pipeline \"$\"",
					  dev.name, indirect_cmd.get_description().debug_label);
			return;
		}
		
		const auto range = mtl_indirect_cmd.compute_and_validate_command_range(command_offset, command_count);
		if (!range) {
			return;
		}
		
		// create and setup the compute encoder
		id <MTLCommandBuffer> cmd_buffer = make_command_buffer();
		id <MTLComputeCommandEncoder> encoder = [cmd_buffer computeCommandEncoderWithDispatchType:MTLDispatchTypeConcurrent];
		if (params.debug_label) {
			[encoder setLabel:[NSString stringWithUTF8String:params.debug_label]];
		}
		
		for (const auto& fence : params.wait_fences) {
			[encoder waitForFence:((const metal_fence*)fence)->get_metal_fence()];
		}
		
		const auto& mtl_dev = (const metal_device&)dev;
		if (!mtl_dev.heap_residency_set) {
			if (mtl_dev.heap_shared) {
				[encoder useHeap:mtl_dev.heap_shared];
			}
			if (mtl_dev.heap_private) {
				[encoder useHeap:mtl_dev.heap_private];
			}
		}
		
		// declare all used resources
		const auto& resources = mtl_indirect_pipeline_entry->get_resources();
		if (!resources.read_only.empty()) {
			[encoder useResources:resources.read_only.data()
							count:resources.read_only.size()
							usage:MTLResourceUsageRead];
		}
		if (!resources.read_write.empty()) {
			[encoder useResources:resources.read_write.data()
							count:resources.read_write.size()
							usage:(MTLResourceUsageRead | MTLResourceUsageWrite)];
		}
		if (!resources.read_only_images.empty()) {
			[encoder useResources:resources.read_only_images.data()
							count:resources.read_only_images.size()
							usage:MTLResourceUsageRead];
		}
		if (!resources.read_write_images.empty()) {
			[encoder useResources:resources.read_write_images.data()
							count:resources.read_write_images.size()
							usage:(MTLResourceUsageRead | MTLResourceUsageWrite)];
		}
		
		if (mtl_indirect_pipeline_entry->printf_buffer) {
			mtl_indirect_pipeline_entry->printf_init(*this);
		}
		
		[encoder executeCommandsInBuffer:mtl_indirect_pipeline_entry->icb
							   withRange:*range];
		
		for (const auto& fence : params.signal_fences) {
			[encoder updateFence:((const metal_fence*)fence)->get_metal_fence()];
		}
		[encoder endEncoding];
		
		if (mtl_indirect_pipeline_entry->printf_buffer) {
			mtl_indirect_pipeline_entry->printf_completion(*this, cmd_buffer);
		}
		
		if (completion_handler) {
			auto local_completion_handler = std::move(completion_handler);
			[cmd_buffer addCompletedHandler:^(id <MTLCommandBuffer>) {
				local_completion_handler();
			}];
		}
		
		[cmd_buffer commit];
		
		if (params.wait_until_completion) {
			[cmd_buffer waitUntilCompleted];
		}
	}
}

const void* metal_queue::get_queue_ptr() const {
	return (__bridge const void*)queue;
}

void* metal_queue::get_queue_ptr() {
	return (__bridge void*)queue;
}

id <MTLCommandQueue> metal_queue::get_queue() const {
	return queue;
}

id <MTLCommandBuffer> metal_queue::make_command_buffer() const {
	id <MTLCommandBuffer> cmd_buffer = [queue commandBufferWithUnretainedReferences];
	[cmd_buffer addCompletedHandler:^(id <MTLCommandBuffer> buffer) {
		GUARD(cmd_buffers_lock);
		const auto iter = find_if(cbegin(cmd_buffers), cend(cmd_buffers), [&buffer](const decltype(cmd_buffers)::value_type& elem) {
			return (elem.first == buffer);
		});
		if(iter == cend(cmd_buffers)) {
			log_error("failed to find Metal command buffer $X!", buffer);
			return;
		}
		
		if (iter->second) {
			const auto elapsed_time = ([buffer GPUEndTime] - [buffer GPUStartTime]);
			profiling_sum += uint64_t(elapsed_time * 1000000.0);
		}
		// else: nothing to do here
		
		cmd_buffers.erase(iter);
	}];
	{
		GUARD(cmd_buffers_lock);
		cmd_buffers.emplace_back(cmd_buffer, is_profiling);
	}
	return cmd_buffer;
}

void metal_queue::start_profiling() const {
	// signal new buffers that we're profiling
	is_profiling = true;
}

uint64_t metal_queue::stop_profiling() const {
	// wait on all buffers
	finish();
	
	const uint64_t ret = profiling_sum;
	
	// clear
	profiling_sum = 0;
	is_profiling = false;
	
	return ret;
}

void metal_queue::set_debug_label(const std::string& label) {
	if (queue) {
		queue.label = [NSString stringWithUTF8String:label.c_str()];
	}
}

} // namespace fl

#endif
