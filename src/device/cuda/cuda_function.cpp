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

#include <floor/device/cuda/cuda_function.hpp>

#if !defined(FLOOR_NO_CUDA)

#include <floor/device/device_context.hpp>
#include <floor/device/device_queue.hpp>
#include <floor/device/cuda/cuda_device.hpp>
#include <floor/device/cuda/cuda_argument_buffer.hpp>

namespace fl {

cuda_function::cuda_function(const std::string_view function_name_, function_map_type&& functions_) :
device_function(function_name_), functions(std::move(functions_)) {
}

typename cuda_function::function_map_type::const_iterator cuda_function::get_function(const device_queue& cqueue) const {
	return functions.find((const cuda_device*)&cqueue.get_device());
}

struct cuda_completion_handler {
	kernel_completion_handler_f handler;
};
static safe_mutex completion_handlers_in_flight_lock;
static std::unordered_map<void*, std::shared_ptr<cuda_completion_handler>> completion_handlers_in_flight GUARDED_BY(completion_handlers_in_flight_lock);
static CU_API void cuda_stream_completion_callback(cu_stream stream floor_unused, CU_RESULT result floor_unused, void* user_data)
REQUIRES(!completion_handlers_in_flight_lock) {
	if (user_data == nullptr) {
		return;
	}
	std::shared_ptr<cuda_completion_handler> compl_handler;
	{
		GUARD(completion_handlers_in_flight_lock);
		auto iter = completion_handlers_in_flight.find(user_data);
		if (iter == completion_handlers_in_flight.end()) {
			log_error("invalid CUDA completion handler");
			return;
		}
		compl_handler = iter->second;
		completion_handlers_in_flight.erase(iter);
	}
	if (compl_handler->handler) {
		compl_handler->handler();
	}
}

void cuda_function::execute(const device_queue& cqueue,
							const bool& is_cooperative,
							const bool& wait_until_completion,
							const uint32_t& dim floor_unused,
							const uint3& global_work_size,
							const uint3& local_work_size,
							const std::vector<device_function_arg>& args,
							const std::vector<const device_fence*>& wait_fences floor_unused,
							const std::vector<device_fence*>& signal_fences floor_unused,
							const char* debug_label floor_unused,
							kernel_completion_handler_f&& completion_handler) const
REQUIRES(!completion_handlers_in_flight_lock) {
	// find entry for queue device
	const auto function_iter = get_function(cqueue);
	if(function_iter == functions.cend()) {
		log_error("no kernel \"$\" for this compute queue/device exists!", function_name);
		return;
	}
	
	const auto& cu_dev = (const cuda_device&)cqueue.get_device();
	if (!cu_dev.make_context_current()) {
		log_error("failed to make CUDA context current for execution of $", function_name);
		return;
	}
	
	// check work size (NOTE: will set elements to at least 1)
	const uint3 block_dim = check_local_work_size(function_iter->second, local_work_size);
	
	// set and handle function arguments
	static constexpr const size_t heap_protect {
#if defined(FLOOR_DEBUG)
		4096
#else
		0
#endif
	};
	std::vector<void*> function_params(args.size(), nullptr);
	auto function_params_data = std::make_unique<uint8_t[]>(function_iter->second.function_args_size + heap_protect);
	uint8_t* data = function_params_data.get();
	
	{
		auto param_iter = function_params.begin();
		for (const auto& arg : args) {
			void*& param = *param_iter++;
			
			if (auto buf_ptr = get_if<const device_buffer*>(&arg.var)) {
				param = data;
				memcpy(data, &((const cuda_buffer*)*buf_ptr)->get_cuda_buffer(), sizeof(cu_device_ptr));
				data += sizeof(cu_device_ptr);
			} else if ([[maybe_unused]] auto vec_buf_ptrs = get_if<const std::vector<device_buffer*>*>(&arg.var)) {
				log_error("array of buffers is not yet supported for CUDA");
				return;
			} else if ([[maybe_unused]] auto vec_buf_sptrs = get_if<const std::vector<std::shared_ptr<device_buffer>>*>(&arg.var)) {
				log_error("array of buffers is not yet supported for CUDA");
				return;
			} else if (auto img_ptr = get_if<const device_image*>(&arg.var)) {
				auto cu_img = (const cuda_image*)*img_ptr;
				
				// set this to the start
				param = data;
				
				// set texture+sampler objects
				const auto& textures = cu_img->get_cuda_textures();
				memcpy(data, textures.data(), textures.size() * sizeof(uint32_t));
				data += textures.size() * sizeof(uint32_t);
				
				// set surface object
				memcpy(data, &cu_img->get_cuda_surfaces()[0], sizeof(uint64_t));
				data += sizeof(uint64_t);
				
				// set ptr to surfaces lod buffer
				const auto lod_buffer = cu_img->get_cuda_surfaces_lod_buffer();
				if(lod_buffer != nullptr) {
					memcpy(data, &lod_buffer->get_cuda_buffer(), sizeof(cu_device_ptr));
				} else {
					memset(data, 0, sizeof(cu_device_ptr));
				}
				data += sizeof(cu_device_ptr);
				
				// set run-time image type
				memcpy(data, &cu_img->get_image_type(), sizeof(IMAGE_TYPE));
				data += sizeof(IMAGE_TYPE);
			} else if ([[maybe_unused]] auto vec_img_ptrs = get_if<const std::vector<device_image*>*>(&arg.var)) {
				log_error("array of images is not supported for CUDA");
				return;
			} else if ([[maybe_unused]] auto vec_img_sptrs = get_if<const std::vector<std::shared_ptr<device_image>>*>(&arg.var)) {
				log_error("array of images is not supported for CUDA");
				return;
			} else if (auto arg_buf_ptr = get_if<const argument_buffer*>(&arg.var)) {
				const auto cuda_storage_buffer = ((const cuda_buffer*)(*arg_buf_ptr)->get_storage_buffer())->get_cuda_buffer();
				param = data;
				memcpy(data, &cuda_storage_buffer, sizeof(cu_device_ptr));
				data += sizeof(cu_device_ptr);
			} else if (auto generic_arg_ptr = get_if<const void*>(&arg.var)) {
				param = data;
				memcpy(data, *generic_arg_ptr, arg.size);
				data += arg.size;
			} else {
				log_error("encountered invalid arg");
				return;
			}
		}
	}
	
	const auto written_args_size = std::distance(&function_params_data[0], data);
	if((size_t)written_args_size != function_iter->second.function_args_size) {
		log_error("invalid function parameters size (in $): got $, expected $",
				  function_iter->second.info->name,
				  written_args_size, function_iter->second.function_args_size);
		return;
	}
	
	// run
	const uint3 grid_dim_overflow {
		global_work_size.x > 0 ? std::min(uint32_t(global_work_size.x % block_dim.x), 1u) : 0u,
		global_work_size.y > 0 ? std::min(uint32_t(global_work_size.y % block_dim.y), 1u) : 0u,
		global_work_size.z > 0 ? std::min(uint32_t(global_work_size.z % block_dim.z), 1u) : 0u
	};
	uint3 grid_dim { (global_work_size / block_dim) + grid_dim_overflow };
	grid_dim.max(1u);
	
	// TODO: implement waiting for "wait_fences"
	
	uint32_t launch_attr_count = 0u;
	std::array<cu_launch_attribute, 4> launch_attrs;
	if (is_cooperative) {
		launch_attrs[launch_attr_count++] = {
			.type = CU_LAUNCH_ATTRIBUTE::COOPERATIVE,
			.value = {
				.cooperative = 1,
			},
		};
	}
	const cu_launch_config launch_config {
		.grid_dim_x = grid_dim.x,
		.grid_dim_y = grid_dim.y,
		.grid_dim_z = grid_dim.z,
		.block_dim_x = block_dim.x,
		.block_dim_y = block_dim.y,
		.block_dim_z = block_dim.z,
		.shared_memory_bytes = 0,
		.stream = (const_cu_stream)cqueue.get_queue_ptr(),
		.attrs = launch_attrs.data(),
		.num_attrs = launch_attr_count,
	};
	CU_CALL_NO_ACTION(cu_launch_kernel_ex(&launch_config, function_iter->second.function, &function_params[0], nullptr),
					  "failed to execute kernel: " + function_iter->second.info->name)
	
	// TODO: implement signaling of "signal_fences"
	
	if (completion_handler) {
		auto compl_handler = std::make_shared<cuda_completion_handler>();
		compl_handler->handler = std::move(completion_handler);
		{
			GUARD(completion_handlers_in_flight_lock);
			completion_handlers_in_flight.emplace(compl_handler.get(), compl_handler);
		}
		CU_CALL_NO_ACTION(cu_stream_add_callback((const_cu_stream)cqueue.get_queue_ptr(), &cuda_stream_completion_callback, compl_handler.get(), 0),
						  "failed to add kernel completion handler")
	}
	
	if (wait_until_completion) {
		// NOTE: we could create an event, record it and synchronize on it here, but this would have the same effect
		CU_CALL_RET(cu_stream_synchronize((const_cu_stream)cqueue.get_queue_ptr()), "failed to synchronize queue")
	}
}

const device_function::function_entry* cuda_function::get_function_entry(const device& dev) const {
	if (const auto iter = functions.find((const cuda_device*)&dev); iter != functions.end()) {
		return &iter->second;
	}
	return nullptr;
}

std::unique_ptr<argument_buffer> cuda_function::create_argument_buffer_internal(const device_queue& cqueue,
																		 const function_entry& kern_entry,
																		 const toolchain::arg_info& arg floor_unused,
																		 const uint32_t& user_arg_index,
																		 const uint32_t& ll_arg_index,
																		 const MEMORY_FLAG& add_mem_flags) const {
	const auto& dev = cqueue.get_device();
	const auto& cuda_entry = (const cuda_function_entry&)kern_entry;
	
	// check if info exists
	const auto& arg_info = cuda_entry.info->args[ll_arg_index].argument_buffer_info;
	if (!arg_info) {
		log_error("no argument buffer info for arg at index #$", user_arg_index);
		return {};
	}
	
	const auto arg_buffer_size = cuda_entry.info->args[ll_arg_index].size;
	if (arg_buffer_size == 0) {
		log_error("computed argument buffer size is 0");
		return {};
	}
	
	// create the argument buffer
	auto buf = dev.context->create_buffer(cqueue, arg_buffer_size, MEMORY_FLAG::READ | MEMORY_FLAG::HOST_WRITE | add_mem_flags);
	buf->set_debug_label(kern_entry.info->name + "_arg_buffer");
	return std::make_unique<cuda_argument_buffer>(*this, buf, *arg_info);
}

} // namespace fl

#endif
