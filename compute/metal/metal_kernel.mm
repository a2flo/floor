/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2021 Florian Ziesche
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

#include <floor/compute/compute_context.hpp>
#include <floor/compute/metal/metal_queue.hpp>
#include <floor/compute/metal/metal_buffer.hpp>
#include <floor/compute/metal/metal_image.hpp>
#include <floor/compute/metal/metal_device.hpp>
#include <floor/compute/metal/metal_args.hpp>
#include <floor/compute/metal/metal_argument_buffer.hpp>
#include <floor/compute/soft_printf.hpp>

struct metal_kernel::metal_encoder {
	id <MTLCommandBuffer> cmd_buffer { nil };
	id <MTLComputeCommandEncoder> encoder { nil };
};

static unique_ptr<metal_kernel::metal_encoder> create_encoder(const compute_queue& cqueue, const metal_kernel::metal_kernel_entry& entry) {
	id <MTLCommandBuffer> cmd_buffer = ((const metal_queue&)cqueue).make_command_buffer();
	auto ret = make_unique<metal_kernel::metal_encoder>(metal_kernel::metal_encoder { cmd_buffer, [cmd_buffer computeCommandEncoder] });
	[ret->encoder setComputePipelineState:(__bridge id <MTLComputePipelineState>)entry.kernel_state];
	return ret;
}

metal_kernel::metal_kernel(kernel_map_type&& kernels_) : kernels(move(kernels_)) {
}

void metal_kernel::execute(const compute_queue& cqueue,
						   const bool& is_cooperative,
						   const uint32_t& dim,
						   const uint3& global_work_size,
						   const uint3& local_work_size,
						   const vector<compute_kernel_arg>& args) const {
	// no cooperative support yet
	if (is_cooperative) {
		log_error("cooperative kernel execution is not supported for Metal");
		return;
	}
	
	// find entry for queue device
	const auto kernel_iter = get_kernel(cqueue);
	if(kernel_iter == kernels.cend()) {
		log_error("no kernel for this compute queue/device exists!");
		return;
	}
	
	// check work size (NOTE: will set elements to at least 1)
	const auto block_dim = check_local_work_size(kernel_iter->second, local_work_size);
	
	//
	auto encoder = create_encoder(cqueue, kernel_iter->second);
	
	// create implicit args
	vector<compute_kernel_arg> implicit_args;

	// create + init printf buffer if this function uses soft-printf
	shared_ptr<compute_buffer> printf_buffer;
	if (llvm_toolchain::has_flag<llvm_toolchain::FUNCTION_FLAGS::USES_SOFT_PRINTF>(kernel_iter->second.info->flags)) {
		printf_buffer = cqueue.get_device().context->create_buffer(cqueue, printf_buffer_size);
		printf_buffer->set_debug_label("printf_buffer");
		printf_buffer->write_from(uint2 { printf_buffer_header_size, printf_buffer_size }, cqueue);
		implicit_args.emplace_back(printf_buffer);
	}

	// set and handle kernel arguments
	const kernel_entry& entry = kernel_iter->second;
	metal_args::set_and_handle_arguments<metal_args::ENCODER_TYPE::COMPUTE>(encoder->encoder, { entry.info }, args, implicit_args);
	
	// run
	const uint3 grid_dim_overflow {
		dim >= 1 && global_work_size.x > 0 ? std::min(uint32_t(global_work_size.x % block_dim.x), 1u) : 0u,
		dim >= 2 && global_work_size.y > 0 ? std::min(uint32_t(global_work_size.y % block_dim.y), 1u) : 0u,
		dim >= 3 && global_work_size.z > 0 ? std::min(uint32_t(global_work_size.z % block_dim.z), 1u) : 0u
	};
	uint3 grid_dim { (global_work_size / block_dim) + grid_dim_overflow };
	grid_dim.max(1u);
	
	// TODO/NOTE: guarantee that all buffers have finished their prior processing
	const MTLSize metal_grid_dim { grid_dim.x, grid_dim.y, grid_dim.z };
	const MTLSize metal_block_dim { block_dim.x, block_dim.y, block_dim.z };
	[encoder->encoder dispatchThreadgroups:metal_grid_dim threadsPerThreadgroup:metal_block_dim];
	[encoder->encoder endEncoding];
	[encoder->cmd_buffer commit];
	
	// if soft-printf is being used, block/wait for completion here and read-back results
	if (llvm_toolchain::has_flag<llvm_toolchain::FUNCTION_FLAGS::USES_SOFT_PRINTF>(kernel_iter->second.info->flags)) {
		[encoder->cmd_buffer waitUntilCompleted];
		auto cpu_printf_buffer = make_unique<uint32_t[]>(printf_buffer_size / 4);
		printf_buffer->read(cqueue, cpu_printf_buffer.get());
		handle_printf_buffer(cpu_printf_buffer);
	}
}

typename metal_kernel::kernel_map_type::const_iterator metal_kernel::get_kernel(const compute_queue& cqueue) const {
	return kernels.find((const metal_device&)cqueue.get_device());
}

const compute_kernel::kernel_entry* metal_kernel::get_kernel_entry(const compute_device& dev) const {
	const auto ret = kernels.get((const metal_device&)dev);
	return !ret.first ? nullptr : &ret.second->second;
}

unique_ptr<argument_buffer> metal_kernel::create_argument_buffer_internal(const compute_queue& cqueue,
																		  const kernel_entry& entry,
																		  const llvm_toolchain::arg_info& arg floor_unused,
																		  const uint32_t& arg_index) const {
	const auto& dev = cqueue.get_device();
	const auto& mtl_entry = (const metal_kernel_entry&)entry;
	auto mtl_func = (__bridge id<MTLFunction>)mtl_entry.kernel;
	
	// check if info exists
	const auto& arg_info = mtl_entry.info->args[arg_index].argument_buffer_info;
	if (!arg_info) {
		log_error("no argument buffer info for arg at index #%u", arg_index);
		return {};
	}
	
	// find the metal buffer index
	uint32_t buffer_idx = 0;
	for (uint32_t i = 0, count = uint32_t(mtl_entry.info->args.size()); i < min(arg_index, count); ++i) {
		if (mtl_entry.info->args[i].image_type == ARG_IMAGE_TYPE::NONE) {
			// all args except for images are buffers
			++buffer_idx;
		}
	}
	
	// create a dummy encoder so that we can retrieve the necessary buffer length (and do some validity checking)
	id <MTLArgumentEncoder> arg_encoder = [mtl_func newArgumentEncoderWithBufferIndex:buffer_idx];
	if (!arg_encoder) {
		log_error("failed to create argument encoder");
		return {};
	}
	
	const auto arg_buffer_size = (uint64_t)[arg_encoder encodedLength];
	if (arg_buffer_size == 0) {
		log_error("computed argument buffer size is 0");
		return {};
	}
	// round up to next multiple of 4096
	const auto arg_buffer_size_4k = arg_buffer_size + (arg_buffer_size % 4096u != 0u ? (4096u - (arg_buffer_size % 4096u)) : 0u);
	
	// create the argument buffer
	// NOTE: the buffer has to be allocated in managed mode (macOS) or shared mode (iOS) -> set appropriate flags
	auto storage_buffer_backing = make_aligned_ptr<uint8_t>(arg_buffer_size_4k);
	memset(storage_buffer_backing.get(), 0, arg_buffer_size_4k);
	auto buf = dev.context->create_buffer(cqueue, arg_buffer_size_4k, storage_buffer_backing.get(),
										  COMPUTE_MEMORY_FLAG::READ |
										  COMPUTE_MEMORY_FLAG::HOST_WRITE |
										  COMPUTE_MEMORY_FLAG::USE_HOST_MEMORY);
	buf->set_debug_label(entry.info->name + "_arg_buffer");
	return make_unique<metal_argument_buffer>(*this, buf, move(storage_buffer_backing), arg_encoder, *arg_info);
}

#endif
