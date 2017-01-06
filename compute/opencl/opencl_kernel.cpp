/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2017 Florian Ziesche
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

opencl_kernel::opencl_kernel(kernel_map_type&& kernels_) : kernels(move(kernels_)) {
}

opencl_kernel::~opencl_kernel() {}

typename opencl_kernel::kernel_map_type::const_iterator opencl_kernel::get_kernel(const compute_queue* queue) const {
	return kernels.find((opencl_device*)queue->get_device().get());
}

void opencl_kernel::execute_internal(shared_ptr<arg_handler> handler,
									 compute_queue* queue,
									 const opencl_kernel_entry& entry,
									 const uint32_t& work_dim,
									 const uint3& global_work_size,
									 const uint3& local_work_size) const {
	const size3 global_ws { global_work_size };
	const size3 local_ws { local_work_size };
	
	const bool has_tmp_buffers = !handler->args.empty();
	cl_event wait_evt = nullptr;
	CL_CALL_RET(clEnqueueNDRangeKernel((cl_command_queue)queue->get_queue_ptr(),
									   entry.kernel, work_dim, nullptr,
									   global_ws.data(), local_ws.data(),
									   0, nullptr,
									   // when using the param workaround, we have created tmp buffers
									   // that need to be destroyed once the kernel has finished execution
									   handler->needs_param_workaround && has_tmp_buffers ? &wait_evt : nullptr),
				"failed to execute kernel " + entry.info->name)
	
	if(handler->needs_param_workaround && has_tmp_buffers) {
		task::spawn([handler, wait_evt]() {
			CL_CALL_IGNORE(clWaitForEvents(1, &wait_evt), "waiting for kernel execution failed");
			// NOTE: will hold onto all tmp buffers of handler until the end of this scope, then auto-destruct everything
		}, "kernel cleanup");
	}
}

shared_ptr<opencl_kernel::arg_handler> opencl_kernel::create_arg_handler(compute_queue* queue) const {
	auto handler = make_shared<arg_handler>();
	handler->device = queue->get_device().get();
	handler->needs_param_workaround = handler->device->param_workaround;
	return handler;
}

void opencl_kernel::set_const_kernel_argument(uint32_t& total_idx, uint32_t& arg_idx, arg_handler* handler, const opencl_kernel_entry& entry,
											  void* arg, const size_t arg_size) const {
	// if param workaround isn't needed, just set the arg
	if(!handler->needs_param_workaround) {
		CL_CALL_RET(clSetKernelArg(entry.kernel, arg_idx, arg_size, arg),
					"failed to set generic kernel argument #" + to_string(total_idx) + " (in kernel " + entry.info->name + ")");
		++arg_idx;
		return;
	}
	
	// if it is needed, create a tmp buffer, copy the arg data into it and set it as the kernel argument
	// TODO: alignment?
	auto param_buf = make_shared<opencl_buffer>((opencl_device*)handler->device, arg_size, arg,
												COMPUTE_MEMORY_FLAG::READ | COMPUTE_MEMORY_FLAG::HOST_WRITE);
	handler->args.emplace_back(param_buf);
	
	set_kernel_argument(total_idx, arg_idx, nullptr, entry, (const compute_buffer*)param_buf.get());
}

#endif
