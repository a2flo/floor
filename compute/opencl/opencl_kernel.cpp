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

#include <floor/compute/opencl/opencl_kernel.hpp>

#if !defined(FLOOR_NO_OPENCL)

#include <floor/compute/compute_queue.hpp>
#include <floor/compute/opencl/opencl_common.hpp>
#include <floor/compute/opencl/opencl_buffer.hpp>
#include <floor/compute/opencl/opencl_device.hpp>
#include <floor/threading/task.hpp>

opencl_kernel::opencl_kernel(const string_view kernel_name_, kernel_map_type&& kernels_) :
compute_kernel(kernel_name_), kernels(std::move(kernels_)) {
}

typename opencl_kernel::kernel_map_type::const_iterator opencl_kernel::get_kernel(const compute_queue& queue) const {
	return kernels.find((const opencl_device&)queue.get_device());
}

void opencl_kernel::execute(const compute_queue& cqueue,
							const bool& is_cooperative,
							const bool& wait_until_completion,
							const uint32_t& work_dim,
							const uint3& global_work_size,
							const uint3& local_work_size_,
							const vector<compute_kernel_arg>& args,
							const vector<const compute_fence*>& wait_fences floor_unused,
							const vector<compute_fence*>& signal_fences floor_unused,
							const char* debug_label floor_unused,
							kernel_completion_handler_f&& completion_handler) const
REQUIRES(!args_lock) {
	// no cooperative support yet
	if (is_cooperative) {
		log_error("cooperative kernel execution is not supported for OpenCL");
		return;
	}
	
	// find entry for queue device
	const auto kernel_iter = get_kernel(cqueue);
	if(kernel_iter == kernels.cend()) {
		log_error("no kernel \"$\" for this compute queue/device exists!", kernel_name);
		return;
	}
	
	// check work size
	const uint3 local_work_size = check_local_work_size(kernel_iter->second, local_work_size_);
	
	// create arg handler (needed if param workaround is necessary)
	auto handler = create_arg_handler(cqueue);
	
	// need to make sure that only one thread is setting kernel arguments at a time
	GUARD(args_lock);
	
	// set and handle kernel arguments
	uint32_t total_idx = 0, arg_idx = 0;
	const opencl_kernel_entry& entry = kernel_iter->second;
	for (const auto& arg : args) {
		if (auto buf_ptr = get_if<const compute_buffer*>(&arg.var)) {
			set_kernel_argument(total_idx, arg_idx, handler.get(), entry, *buf_ptr);
		} else if ([[maybe_unused]] auto vec_buf_ptrs = get_if<const vector<compute_buffer*>*>(&arg.var)) {
			log_error("array of buffers is not supported for OpenCL");
		} else if ([[maybe_unused]] auto vec_buf_sptrs = get_if<const vector<shared_ptr<compute_buffer>>*>(&arg.var)) {
			log_error("array of buffers is not supported for OpenCL");
		} else if (auto img_ptr = get_if<const compute_image*>(&arg.var)) {
			set_kernel_argument(total_idx, arg_idx, handler.get(), entry, *img_ptr);
		} else if ([[maybe_unused]] auto vec_img_ptrs = get_if<const vector<compute_image*>*>(&arg.var)) {
			log_error("array of images is not supported for OpenCL");
		} else if ([[maybe_unused]] auto vec_img_sptrs = get_if<const vector<shared_ptr<compute_image>>*>(&arg.var)) {
			log_error("array of images is not supported for OpenCL");
		} else if ([[maybe_unused]] auto arg_buf_ptr = get_if<const argument_buffer*>(&arg.var)) {
			log_error("argument buffer handling is not implemented yet for OpenCL");
			return;
		} else if (auto generic_arg_ptr = get_if<const void*>(&arg.var)) {
			set_const_kernel_argument(total_idx, arg_idx, handler.get(), entry, const_cast<void*>(*generic_arg_ptr) /* non-const b/c OpenCL */, arg.size);
		} else {
			log_error("encountered invalid arg");
			return;
		}
		++total_idx;
	}
	
	// run
	const size3 global_ws { global_work_size };
	const size3 local_ws { local_work_size };
	const bool has_tmp_buffers = !handler->args.empty();
	// TODO: implement waiting for "wait_fences"
	cl_event wait_evt = nullptr;
	CL_CALL_RET(clEnqueueNDRangeKernel((cl_command_queue)const_cast<void*>(cqueue.get_queue_ptr()),
									   entry.kernel, work_dim, nullptr,
									   global_ws.data(), local_ws.data(),
									   0, nullptr,
									   // when using the param workaround, we have created tmp buffers
									   // that need to be destroyed once the kernel has finished execution,
									   // we also need the wait event if a user-specified completion handler is set or wait_until_completion is true
									   (handler->needs_param_workaround && has_tmp_buffers) || completion_handler || wait_until_completion ? &wait_evt : nullptr),
				"failed to execute kernel: " + entry.info->name)
	
	// TODO: implement signaling of "signal_fences"
	
	if ((handler->needs_param_workaround && has_tmp_buffers) || completion_handler) {
		task::spawn([handler, wait_evt, user_compl_handler = std::move(completion_handler)]() {
			CL_CALL_IGNORE(clWaitForEvents(1, &wait_evt), "waiting for kernel execution failed")
			// NOTE: will hold onto all tmp buffers of handler until the end of this scope, then auto-destruct everything
			
			if (user_compl_handler) {
				user_compl_handler();
			}
		}, "kernel cleanup");
	}
	
	if (wait_until_completion) {
		CL_CALL_IGNORE(clWaitForEvents(1, &wait_evt), "waiting for kernel completion failed")
	}
}

shared_ptr<opencl_kernel::arg_handler> opencl_kernel::create_arg_handler(const compute_queue& cqueue) const {
	auto handler = make_shared<arg_handler>();
	handler->cqueue = &cqueue;
	handler->device = &cqueue.get_device();
	handler->needs_param_workaround = handler->device->param_workaround;
	return handler;
}

void opencl_kernel::set_const_kernel_argument(uint32_t& total_idx, uint32_t& arg_idx, arg_handler* handler, const opencl_kernel_entry& entry,
											  void* arg, const size_t arg_size) const {
	// if param workaround isn't needed, just set the arg
	if(!handler->needs_param_workaround) {
		CL_CALL_RET(clSetKernelArg(entry.kernel, arg_idx, arg_size, arg),
					"failed to set generic kernel argument #" + to_string(total_idx) + " (in kernel " + entry.info->name + ")")
		++arg_idx;
		return;
	}
	
	// if it is needed, create a tmp buffer, copy the arg data into it and set it as the kernel argument
	// TODO: alignment?
	auto param_buf = make_shared<opencl_buffer>(*handler->cqueue, arg_size, std::span<uint8_t> { (uint8_t*)arg, arg_size },
												COMPUTE_MEMORY_FLAG::READ | COMPUTE_MEMORY_FLAG::HOST_WRITE);
	handler->args.emplace_back(param_buf);
	
	set_kernel_argument(total_idx, arg_idx, nullptr, entry, (const compute_buffer*)param_buf.get());
}

void opencl_kernel::set_kernel_argument(uint32_t& total_idx, uint32_t& arg_idx, arg_handler*,
										const opencl_kernel_entry& entry,
										const compute_buffer* arg) const {
	CL_CALL_RET(clSetKernelArg(entry.kernel, arg_idx, sizeof(cl_mem),
							   &((const opencl_buffer*)arg)->get_cl_buffer()),
				"failed to set buffer kernel argument #" + to_string(total_idx) + " (in kernel " + entry.info->name + ")")
	++arg_idx;
}

void opencl_kernel::set_kernel_argument(uint32_t& total_idx, uint32_t& arg_idx, arg_handler* handler,
										const opencl_kernel_entry& entry,
										const compute_image* arg) const {
	CL_CALL_RET(clSetKernelArg(entry.kernel, arg_idx, sizeof(cl_mem),
							   &((const opencl_image*)arg)->get_cl_image()),
				"failed to set image kernel argument #" + to_string(total_idx) + " (in kernel " + entry.info->name + ")")
	++arg_idx;
	
	// legacy s/w read/write image -> set it twice
	if(entry.info->args[total_idx].image_access == llvm_toolchain::ARG_IMAGE_ACCESS::READ_WRITE &&
	   !handler->device->image_read_write_support) {
		CL_CALL_RET(clSetKernelArg(entry.kernel, arg_idx, sizeof(cl_mem),
								   &((const opencl_image*)arg)->get_cl_image()),
					"failed to set image kernel argument #" + to_string(total_idx) + " (in kernel " + entry.info->name + ")")
		++arg_idx;
	}
}

const compute_kernel::kernel_entry* opencl_kernel::get_kernel_entry(const compute_device& dev) const {
	const auto ret = kernels.get((const opencl_device&)dev);
	return !ret.first ? nullptr : &ret.second->second;
}

#endif
