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

#include <floor/device/metal/metal_function.hpp>

#if !defined(FLOOR_NO_METAL)

#include <floor/device/device_context.hpp>
#include <floor/device/metal/metal_context.hpp>
#include <floor/device/metal/metal_queue.hpp>
#include <floor/device/metal/metal_buffer.hpp>
#include <floor/device/metal/metal_image.hpp>
#include <floor/device/metal/metal_device.hpp>
#include <floor/device/metal/metal_args.hpp>
#include <floor/device/metal/metal_argument_buffer.hpp>
#include <floor/device/metal/metal_fence.hpp>
#include <floor/device/soft_printf.hpp>

namespace fl {

struct metal_encoder {
	id <MTLCommandBuffer> cmd_buffer { nil };
	id <MTLComputeCommandEncoder> encoder { nil };
	metal_encoder(id <MTLCommandBuffer> cmd_buffer_, id <MTLComputeCommandEncoder> encoder_) : cmd_buffer(cmd_buffer_), encoder(encoder_) {}
	~metal_encoder() {
		@autoreleasepool {
			encoder = nil;
			cmd_buffer = nil;
		}
	}
};

static std::unique_ptr<metal_encoder> create_encoder(const device_queue& cqueue,
												const metal_function::metal_function_entry& entry,
												const char* debug_label) {
	id <MTLCommandBuffer> cmd_buffer = ((const metal_queue&)cqueue).make_command_buffer();
	id <MTLComputeCommandEncoder> encoder = [cmd_buffer computeCommandEncoderWithDispatchType:MTLDispatchTypeConcurrent];
	auto ret = std::make_unique<metal_encoder>(cmd_buffer, encoder);
	[ret->encoder setComputePipelineState:(__bridge __unsafe_unretained id <MTLComputePipelineState>)entry.kernel_state];
	if (debug_label) {
		[ret->encoder setLabel:[NSString stringWithUTF8String:debug_label]];
	}
	return ret;
}

metal_function::metal_function(const std::string_view function_name_, function_map_type&& functions_) :
device_function(function_name_), functions(std::move(functions_)) {
}

std::pair<uint3, uint3> metal_function::compute_grid_and_block_dim(const function_entry& entry,
															const uint32_t& dim,
															const uint3& global_work_size,
															const uint3& local_work_size) const {
	// check work size (NOTE: will set elements to at least 1)
	const auto block_dim = check_local_work_size(entry, local_work_size);
	const uint3 grid_dim_overflow {
		dim >= 1 && global_work_size.x > 0 ? std::min(uint32_t(global_work_size.x % block_dim.x), 1u) : 0u,
		dim >= 2 && global_work_size.y > 0 ? std::min(uint32_t(global_work_size.y % block_dim.y), 1u) : 0u,
		dim >= 3 && global_work_size.z > 0 ? std::min(uint32_t(global_work_size.z % block_dim.z), 1u) : 0u
	};
	uint3 grid_dim { (global_work_size / block_dim) + grid_dim_overflow };
	grid_dim.max(1u);
	return { grid_dim, block_dim };
}

void metal_function::execute(const device_queue& cqueue,
						   const bool& is_cooperative,
						   const bool& wait_until_completion,
						   const uint32_t& dim,
						   const uint3& global_work_size,
						   const uint3& local_work_size,
						   const std::vector<device_function_arg>& args,
						   const std::vector<const device_fence*>& wait_fences,
						   const std::vector<device_fence*>& signal_fences,
						   const char* debug_label,
						   kernel_completion_handler_f&& completion_handler) const {
	const auto dev = (const metal_device*)&cqueue.get_device();
	const auto ctx = (const metal_context*)dev->context;
	
	// no cooperative support yet
	if (is_cooperative) {
		log_error("cooperative kernel execution is not supported for Metal");
		return;
	}
	
	// find entry for queue device
	const auto function_iter = get_function(cqueue);
	if(function_iter == functions.cend()) {
		log_error("no function \"$\" for this compute queue/device exists!", function_name);
		return;
	}
	
	
	@autoreleasepool {
		auto encoder = create_encoder(cqueue, function_iter->second, debug_label);
		for (const auto& fence : wait_fences) {
			[encoder->encoder waitForFence:((const metal_fence*)fence)->get_metal_fence()];
		}
		
		// create implicit args
		std::vector<device_function_arg> implicit_args;
		
		// create + init printf buffer if this function uses soft-printf
		std::pair<device_buffer*, uint32_t> printf_buffer_rsrc;
		if (toolchain::has_flag<toolchain::FUNCTION_FLAGS::USES_SOFT_PRINTF>(function_iter->second.info->flags)) {
			printf_buffer_rsrc = ctx->acquire_soft_printf_buffer(*dev);
			if (printf_buffer_rsrc.first) {
				initialize_printf_buffer(cqueue, *printf_buffer_rsrc.first);
				implicit_args.emplace_back(*printf_buffer_rsrc.first);
			}
		}
		
		if (!dev->heap_residency_set) {
			if (dev->heap_shared) {
				[encoder->encoder useHeap:dev->heap_shared];
			}
			if (dev->heap_private) {
				[encoder->encoder useHeap:dev->heap_private];
			}
		}
		
		// set and handle function arguments
		const function_entry& entry = function_iter->second;
		metal_args::set_and_handle_arguments<metal_args::ENCODER_TYPE::COMPUTE>(*dev, encoder->encoder, { entry.info }, args, implicit_args);
		
		// compute sizes
		auto [grid_dim, block_dim] = compute_grid_and_block_dim(function_iter->second, dim, global_work_size, local_work_size);
		const MTLSize metal_grid_dim { grid_dim.x, grid_dim.y, grid_dim.z };
		const MTLSize metal_block_dim { block_dim.x, block_dim.y, block_dim.z };
		
		// run
		[encoder->encoder dispatchThreadgroups:metal_grid_dim threadsPerThreadgroup:metal_block_dim];
		for (const auto& fence : signal_fences) {
			[encoder->encoder updateFence:((const metal_fence*)fence)->get_metal_fence()];
		}
		[encoder->encoder endEncoding];
		
		// if soft-printf is being used, block/wait for completion here and read-back results
		if (toolchain::has_flag<toolchain::FUNCTION_FLAGS::USES_SOFT_PRINTF>(function_iter->second.info->flags)) {
			auto internal_dev_queue = ctx->get_device_default_queue(*dev);
			[encoder->cmd_buffer addCompletedHandler:^(id <MTLCommandBuffer>) {
				auto cpu_printf_buffer = std::make_unique<uint32_t[]>(printf_buffer_size / 4);
				printf_buffer_rsrc.first->read(*internal_dev_queue, cpu_printf_buffer.get());
				handle_printf_buffer(std::span { cpu_printf_buffer.get(), printf_buffer_size / 4 });
				ctx->release_soft_printf_buffer(*dev, printf_buffer_rsrc);
			}];
		}
		
		if (completion_handler) {
			auto local_completion_handler = std::move(completion_handler);
			[encoder->cmd_buffer addCompletedHandler:^(id <MTLCommandBuffer>) {
				local_completion_handler();
			}];
		}
		
		[encoder->cmd_buffer commit];
		
		if (wait_until_completion) {
			[encoder->cmd_buffer waitUntilCompleted];
#if defined(FLOOR_DEBUG)
			if ([encoder->cmd_buffer status] == MTLCommandBufferStatus::MTLCommandBufferStatusError && [encoder->cmd_buffer error]) {
				const std::string err_str = [[[encoder->cmd_buffer error] localizedDescription] UTF8String];
				log_error("failed to execute kernel: $: $", entry.info->name, err_str);
			}
#endif
		}
	}
}

typename metal_function::function_map_type::const_iterator metal_function::get_function(const device_queue& cqueue) const {
	return functions.find((const metal_device*)&cqueue.get_device());
}

const device_function::function_entry* metal_function::get_function_entry(const device& dev) const {
	if (const auto iter = functions.find((const metal_device*)&dev); iter != functions.end()) {
		return &iter->second;
	}
	return nullptr;
}

std::unique_ptr<argument_buffer> metal_function::create_argument_buffer_internal(const device_queue& cqueue,
																				 const function_entry& entry,
																				 const toolchain::arg_info& arg floor_unused,
																				 const uint32_t& user_arg_index,
																				 const uint32_t& ll_arg_index,
																				 const MEMORY_FLAG& add_mem_flags,
																				 const bool zero_init) const {
	@autoreleasepool {
		const auto& dev = cqueue.get_device();
		const auto& mtl_entry = (const metal_function_entry&)entry;
		auto mtl_func = (__bridge __unsafe_unretained id<MTLFunction>)mtl_entry.function;
		
		// check if info exists
		const auto& arg_info = mtl_entry.info->args[ll_arg_index].argument_buffer_info;
		if (!arg_info) {
			log_error("no argument buffer info for arg at index #$", user_arg_index);
			return {};
		}
		
		// find the Metal buffer index
		uint32_t buffer_idx = 0;
		for (uint32_t i = 0, count = uint32_t(mtl_entry.info->args.size()); i < std::min(ll_arg_index, count); ++i) {
			if (has_flag<ARG_FLAG::STAGE_INPUT>(mtl_entry.info->args[i].flags)) {
				// only tessellation evaluation shaders may contain buffers in stage_input
				if (mtl_entry.info->type == toolchain::FUNCTION_TYPE::TESSELLATION_EVALUATION) {
					buffer_idx += mtl_entry.info->args[i].size;
				}
			} else if (mtl_entry.info->args[i].image_type == ARG_IMAGE_TYPE::NONE) {
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
		// round up to next multiple of page size
		const auto arg_buffer_size_page = arg_buffer_size + (arg_buffer_size % aligned_ptr<int>::page_size != 0u ?
															 (aligned_ptr<int>::page_size - (arg_buffer_size % aligned_ptr<int>::page_size)) : 0u);
		
		// figure out the top level arg indices (we don't need to go deeper, since non-constant/buffer vars in nested structs are not supported right now)
		std::vector<uint32_t> arg_indices;
		uint32_t arg_idx_counter = 0;
		for (const auto& arg_buffer_arg : arg_info->args) {
			arg_indices.emplace_back(arg_idx_counter);
			if (has_flag<toolchain::ARG_FLAG::ARGUMENT_BUFFER>(arg_buffer_arg.flags)) {
				throw std::runtime_error("unsupported argument type in argument buffer (in " + mtl_entry.info->name + " #" + std::to_string(user_arg_index) + ")");
			}
			if (has_flag<toolchain::ARG_FLAG::STAGE_INPUT>(arg_buffer_arg.flags) ||
				has_flag<toolchain::ARG_FLAG::PUSH_CONSTANT>(arg_buffer_arg.flags) ||
				has_flag<toolchain::ARG_FLAG::SSBO>(arg_buffer_arg.flags) ||
				has_flag<toolchain::ARG_FLAG::IUB>(arg_buffer_arg.flags)) {
				throw std::runtime_error("invalid argument type in argument buffer (in " + mtl_entry.info->name + " #" + std::to_string(user_arg_index) + ")");
			}
			
			if (arg_buffer_arg.is_array()) {
				assert(arg_buffer_arg.array_extent > 0u);
				arg_idx_counter += arg_buffer_arg.array_extent;
			} else {
				++arg_idx_counter;
			}
		}
		
		// create the argument buffer
		// NOTE: the buffer has to be allocated in managed mode (dedicated GPU) or shared mode (integrated GPU) -> set appropriate flags
		std::shared_ptr<device_buffer> buf;
		aligned_ptr<uint8_t> storage_buffer_backing;
		if (dev.unified_memory) {
			buf = dev.context->create_buffer(cqueue, arg_buffer_size_page,
											 MEMORY_FLAG::READ |
											 MEMORY_FLAG::HOST_WRITE |
											 MEMORY_FLAG::__EXP_HEAP_ALLOC |
											 add_mem_flags);
			if (zero_init) {
				buf->zero(cqueue);
			}
		} else {
			storage_buffer_backing = make_aligned_ptr<uint8_t>(arg_buffer_size_page);
			memset(storage_buffer_backing.get(), 0, arg_buffer_size_page);
			buf = dev.context->create_buffer(cqueue, storage_buffer_backing.to_span(),
											 MEMORY_FLAG::READ |
											 MEMORY_FLAG::HOST_WRITE |
											 MEMORY_FLAG::USE_HOST_MEMORY |
											 add_mem_flags);
			if (zero_init && has_flag<MEMORY_FLAG::__EXP_HEAP_ALLOC>(add_mem_flags)) {
				// only need zero-init if allocated from heap, otherwise newly created buffer is zero-initialized already
				buf->zero(cqueue);
			}
		}
		buf->set_debug_label(entry.info->name + "_arg_buffer");
		return std::make_unique<metal_argument_buffer>(*this, buf, std::move(storage_buffer_backing), arg_encoder, *arg_info, std::move(arg_indices));
	}
}

} // namespace fl

#endif
