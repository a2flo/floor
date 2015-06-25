/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2015 Florian Ziesche
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

#include <floor/compute/metal/metal_kernel.hpp>

#if !defined(FLOOR_NO_METAL)

#include <floor/compute/metal/metal_queue.hpp>
#include <floor/compute/metal/metal_buffer.hpp>

struct metal_kernel::metal_encoder {
	id <MTLCommandBuffer> cmd_buffer;
	id <MTLComputeCommandEncoder> encoder;
	unordered_set<metal_buffer*> buffers;
};

metal_kernel::metal_kernel(const void* kernel_,
						   const void* kernel_state_,
						   const metal_device* device floor_unused,
						   const llvm_compute::kernel_info& info) :
kernel(kernel_), kernel_state(kernel_state_), func_name(info.name) {
}

metal_kernel::~metal_kernel() {}

void metal_kernel::execute_internal(compute_queue* queue floor_unused,
									shared_ptr<metal_encoder> encoder,
									const uint3& grid_dim,
									const uint3& block_dim) NO_THREAD_SAFETY_ANALYSIS /* fails on looped locking? */ {
	// lock all buffers
	// TODO/NOTE: a) this is very deadlock-susceptible (use shared locking if going to use this in the future)
	//            b) this doesn't actually guarantee that all buffers have finished their prior processing?
	for(auto& buffer : encoder->buffers) {
		buffer->_lock();
	}
	
	const MTLSize metal_grid_dim { grid_dim.x, grid_dim.y, grid_dim.z };
	const MTLSize metal_block_dim { block_dim.x, block_dim.y, block_dim.z };
	[encoder->encoder dispatchThreadgroups:metal_grid_dim threadsPerThreadgroup:metal_block_dim];
	[encoder->encoder endEncoding];
	
	// unlock all buffers again when this has finished executing
	[encoder->cmd_buffer addCompletedHandler:[encoder](id <MTLCommandBuffer>) NO_THREAD_SAFETY_ANALYSIS {
		for(auto& buffer : encoder->buffers) {
			buffer->_unlock();
		}
	}];
	
	[encoder->cmd_buffer commit]; // TODO: or just enqueue? speed diff?
}

shared_ptr<metal_kernel::metal_encoder> metal_kernel::create_encoder(compute_queue* queue) {
	id <MTLCommandBuffer> cmd_buffer = ((metal_queue*)queue)->make_command_buffer();
	auto ret = make_shared<metal_encoder>(metal_encoder {
		cmd_buffer, [cmd_buffer computeCommandEncoder], {}
	});
	[ret->encoder setComputePipelineState:(__bridge id <MTLComputePipelineState>)kernel_state];
	return ret;
}

void metal_kernel::set_const_parameter(metal_encoder* encoder, const uint32_t& num,
									   const void* ptr, const size_t& size) {
	[encoder->encoder setBytes:ptr length:size atIndex:num];
}

void metal_kernel::set_kernel_argument(const uint32_t num,
									   metal_encoder* encoder,
									   shared_ptr<compute_buffer> arg) {
	[encoder->encoder setBuffer:((metal_buffer*)arg.get())->get_metal_buffer()
						 offset:0
						atIndex:num];
	encoder->buffers.emplace((metal_buffer*)arg.get());
}

#endif
