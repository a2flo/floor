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
	vector<metal_buffer*> buffers;
};

metal_kernel::metal_kernel(const void* kernel_,
						   const void* kernel_state_,
						   const metal_device* device,
						   const llvm_compute::kernel_info& info) :
kernel(kernel_), kernel_state(kernel_state_), func_name(info.name) {
	// create buffers for all kernel param<> parameters
	const auto arg_count = info.args.size();
	param_buffers.resize(arg_count);
	for(size_t i = 0; i < arg_count; ++i) {
		if(info.args[i].address_space == llvm_compute::kernel_info::ARG_ADDRESS_SPACE::CONSTANT) {
			param_buffers[i].buffer = make_unique<metal_buffer>(device, info.args[i].size, nullptr,
																COMPUTE_MEMORY_FLAG::READ | COMPUTE_MEMORY_FLAG::HOST_WRITE);
			param_buffers[i].cur_value.resize(info.args[i].size, 0xCCu); // not ideal to do it this way, but should just work
		}
		else param_buffers[i] = { nullptr, {} };
	}
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
	
	logger::flush();
	
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

void metal_kernel::set_const_parameter(compute_queue* queue, metal_encoder* encoder, const uint32_t& num,
									   const void* ptr, const size_t& size) {
	// debug checks (size / buffer)
#if defined(FLOOR_DEBUG) || 1
	if(num >= param_buffers.size()) {
		log_error("%s: num out-of-range: %u", func_name, num);
		return;
	}
#endif
	
	auto& param_buffer = param_buffers[num];
#if defined(FLOOR_DEBUG) || 1
	if(param_buffers[num].buffer == nullptr) {
		log_error("%s: parameter buffer not allocated or argument #%u is not a parameter!", func_name, num);
		return;
	}
	if(param_buffers[num].cur_value.size() != size) {
		log_error("%s: parameter #%u size mismatch: expected %u, got %u!", func_name, num, param_buffers[num].cur_value.size(), size);
		return;
	}
#endif
	
	// check if we actually need to update the parameter
	if(memcmp(ptr, param_buffer.cur_value.data(), size) != 0) {
		memcpy(param_buffer.cur_value.data(), ptr, size);
		shared_ptr<compute_queue> queue_sptr(queue, [](compute_queue*) { /* non-owning */ });
		param_buffer.buffer->write(queue_sptr, ptr, size);
	}
	[encoder->encoder setBuffer:param_buffer.buffer->get_metal_buffer()
						 offset:0
						atIndex:num];
	encoder->buffers.emplace_back(param_buffer.buffer.get());
}

void metal_kernel::set_kernel_argument(const uint32_t num,
									   compute_queue* queue floor_unused,
									   metal_encoder* encoder,
									   shared_ptr<compute_buffer> arg) {
	[encoder->encoder setBuffer:((metal_buffer*)arg.get())->get_metal_buffer()
						 offset:0
						atIndex:num];
	encoder->buffers.emplace_back((metal_buffer*)arg.get());
}

#endif
