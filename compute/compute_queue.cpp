/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2022 Florian Ziesche
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

#include <floor/compute/compute_queue.hpp>
#include <floor/core/core.hpp>
#include <floor/compute/compute_kernel.hpp>

void compute_queue::start_profiling() {
	finish();
	us_prof_start = core::unix_timestamp_us();
}

uint64_t compute_queue::stop_profiling() {
	finish();
	return core::unix_timestamp_us() - us_prof_start;
}

void compute_queue::kernel_execute_forwarder(const compute_kernel& kernel,
											 const bool is_cooperative,
											 const bool wait_until_completion,
											 const uint1& global_size, const uint1& local_size,
											 kernel_completion_handler_f&& completion_handler,
											 const vector<compute_kernel_arg>& args) const {
	kernel.execute(*this, is_cooperative, wait_until_completion, 1, uint3 { global_size }, uint3 { local_size }, args,
				   {}, {}, nullptr, std::forward<kernel_completion_handler_f>(completion_handler));
}

void compute_queue::kernel_execute_forwarder(const compute_kernel& kernel,
											 const bool is_cooperative,
											 const bool wait_until_completion,
											 const uint2& global_size, const uint2& local_size,
											 kernel_completion_handler_f&& completion_handler,
											 const vector<compute_kernel_arg>& args) const {
	kernel.execute(*this, is_cooperative, wait_until_completion, 2, uint3 { global_size }, uint3 { local_size }, args,
				   {}, {}, nullptr, std::forward<kernel_completion_handler_f>(completion_handler));
}

void compute_queue::kernel_execute_forwarder(const compute_kernel& kernel,
											 const bool is_cooperative,
											 const bool wait_until_completion,
											 const uint3& global_size, const uint3& local_size,
											 kernel_completion_handler_f&& completion_handler,
											 const vector<compute_kernel_arg>& args) const {
	kernel.execute(*this, is_cooperative, wait_until_completion, 3, global_size, local_size, args,
				   {}, {}, nullptr, std::forward<kernel_completion_handler_f>(completion_handler));
}

void compute_queue::execute_with_parameters(const compute_kernel& kernel, const execution_parameters_t& params,
											kernel_completion_handler_f&& completion_handler) const {
	if (params.execution_dim < 1u || params.execution_dim > 3u) {
		log_error("invalid execution dim: $", params.execution_dim);
		return;
	}
	kernel.execute(*this, params.is_cooperative, params.wait_until_completion, params.execution_dim, params.global_work_size, params.local_work_size,
				   params.args,  params.wait_fences, params.signal_fences, params.debug_label,
				   std::forward<kernel_completion_handler_f>(completion_handler));
}
