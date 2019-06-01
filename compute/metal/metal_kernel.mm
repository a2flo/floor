/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2019 Florian Ziesche
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
	
	// set and handle kernel arguments
	uint32_t total_idx = 0, buffer_idx = 0, texture_idx = 0;
	const kernel_entry& entry = kernel_iter->second;
	for (const auto& arg : args) {
		if (auto buf_ptr = get_if<const compute_buffer*>(&arg.var)) {
			set_kernel_argument(total_idx, buffer_idx, texture_idx, *encoder, entry, *buf_ptr);
		} else if (auto img_ptr = get_if<const compute_image*>(&arg.var)) {
			set_kernel_argument(total_idx, buffer_idx, texture_idx, *encoder, entry, *img_ptr);
		} else if (auto vec_img_ptrs = get_if<const vector<compute_image*>*>(&arg.var)) {
			set_kernel_argument(total_idx, buffer_idx, texture_idx, *encoder, entry, **vec_img_ptrs);
		} else if (auto vec_img_sptrs = get_if<const vector<shared_ptr<compute_image>>*>(&arg.var)) {
			set_kernel_argument(total_idx, buffer_idx, texture_idx, *encoder, entry, **vec_img_sptrs);
		} else if (auto generic_arg_ptr = get_if<const void*>(&arg.var)) {
			set_const_argument(*encoder, buffer_idx, *generic_arg_ptr, arg.size);
		} else {
			log_error("encountered invalid arg");
			return;
		}
		++total_idx;
	}
	
	// create + init printf buffer if this function uses soft-printf
	shared_ptr<compute_buffer> printf_buffer;
	if (llvm_toolchain::function_info::has_flag<llvm_toolchain::function_info::FUNCTION_FLAGS::USES_SOFT_PRINTF>(kernel_iter->second.info->flags)) {
		printf_buffer = cqueue.get_device().context->create_buffer(cqueue, printf_buffer_size);
		printf_buffer->write_from(uint2 { printf_buffer_header_size, printf_buffer_size }, cqueue);
		set_kernel_argument(total_idx++, buffer_idx, texture_idx, *encoder, entry, printf_buffer.get());
	}
	
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
	if (llvm_toolchain::function_info::has_flag<llvm_toolchain::function_info::FUNCTION_FLAGS::USES_SOFT_PRINTF>(kernel_iter->second.info->flags)) {
		[encoder->cmd_buffer waitUntilCompleted];
		auto cpu_printf_buffer = make_unique<uint32_t[]>(printf_buffer_size / 4);
		printf_buffer->read(cqueue, cpu_printf_buffer.get());
		handle_printf_buffer(cpu_printf_buffer);
	}
}

typename metal_kernel::kernel_map_type::const_iterator metal_kernel::get_kernel(const compute_queue& cqueue) const {
	return kernels.find((const metal_device&)cqueue.get_device());
}

void metal_kernel::set_const_argument(metal_encoder& encoder, uint32_t& buffer_idx,
									  const void* ptr, const size_t& size) const {
	[encoder.encoder setBytes:ptr length:size atIndex:buffer_idx++];
}

void metal_kernel::set_kernel_argument(const uint32_t, uint32_t& buffer_idx, uint32_t&,
									   metal_encoder& encoder, const kernel_entry&,
									   const compute_buffer* arg) const {
	[encoder.encoder setBuffer:((const metal_buffer*)arg)->get_metal_buffer()
						offset:0
					   atIndex:buffer_idx++];
}

void metal_kernel::set_kernel_argument(const uint32_t total_idx, uint32_t&, uint32_t& texture_idx,
									   metal_encoder& encoder, const kernel_entry& entry,
									   const compute_image* arg) const {
	[encoder.encoder setTexture:((const metal_image*)arg)->get_metal_image()
						atIndex:texture_idx++];
	
	// if this is a read/write image, add it again (one is read-only, the other is write-only)
	if(entry.info->args[total_idx].image_access == llvm_toolchain::function_info::ARG_IMAGE_ACCESS::READ_WRITE) {
		[encoder.encoder setTexture:((const metal_image*)arg)->get_metal_image()
							atIndex:texture_idx++];
	}
}

void metal_kernel::set_kernel_argument(const uint32_t, uint32_t&, uint32_t& texture_idx,
									   metal_encoder& encoder, const kernel_entry&,
									   const vector<shared_ptr<compute_image>>& arg) const {
	const auto count = arg.size();
	if(count < 1) return;
	
	vector<id <MTLTexture>> mtl_img_array(count, nil);
	for(size_t i = 0; i < count; ++i) {
		mtl_img_array[i] = ((metal_image*)arg[i].get())->get_metal_image();
	}
	
	[encoder.encoder setTextures:mtl_img_array.data()
					   withRange:NSRange { texture_idx, count }];
	texture_idx += count;
}

void metal_kernel::set_kernel_argument(const uint32_t, uint32_t&, uint32_t& texture_idx,
									   metal_encoder& encoder, const kernel_entry&,
									   const vector<compute_image*>& arg) const {
	const auto count = arg.size();
	if(count < 1) return;
	
	vector<id <MTLTexture>> mtl_img_array(count, nil);
	for(size_t i = 0; i < count; ++i) {
		mtl_img_array[i] = ((metal_image*)arg[i])->get_metal_image();
	}
	
	[encoder.encoder setTextures:mtl_img_array.data()
					   withRange:NSRange { texture_idx, count }];
	texture_idx += count;
}

const compute_kernel::kernel_entry* metal_kernel::get_kernel_entry(const compute_device& dev) const {
	const auto ret = kernels.get((const metal_device&)dev);
	return !ret.first ? nullptr : &ret.second->second;
}

#endif
