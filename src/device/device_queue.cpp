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

#include <floor/device/device_queue.hpp>
#include <floor/core/core.hpp>
#include <floor/device/device_function.hpp>
#include <floor/device/indirect_command.hpp>

namespace fl {

void device_queue::start_profiling() const {
	finish();
	us_prof_start = core::unix_timestamp_us();
}

uint64_t device_queue::stop_profiling() const {
	finish();
	return core::unix_timestamp_us() - us_prof_start;
}

void device_queue::kernel_execute_forwarder(const device_function& kernel,
											 const bool is_cooperative,
											 const bool wait_until_completion,
											 const uint1& global_size, const uint1& local_size,
											 kernel_completion_handler_f&& completion_handler,
											 const std::vector<device_function_arg>& args) const {
	kernel.execute(*this, is_cooperative, wait_until_completion, 1, uint3 { global_size }, uint3 { local_size }, args,
				   {}, {}, nullptr, std::forward<kernel_completion_handler_f>(completion_handler));
}

void device_queue::kernel_execute_forwarder(const device_function& kernel,
											 const bool is_cooperative,
											 const bool wait_until_completion,
											 const uint2& global_size, const uint2& local_size,
											 kernel_completion_handler_f&& completion_handler,
											 const std::vector<device_function_arg>& args) const {
	kernel.execute(*this, is_cooperative, wait_until_completion, 2, uint3 { global_size }, uint3 { local_size }, args,
				   {}, {}, nullptr, std::forward<kernel_completion_handler_f>(completion_handler));
}

void device_queue::kernel_execute_forwarder(const device_function& kernel,
											 const bool is_cooperative,
											 const bool wait_until_completion,
											 const uint3& global_size, const uint3& local_size,
											 kernel_completion_handler_f&& completion_handler,
											 const std::vector<device_function_arg>& args) const {
	kernel.execute(*this, is_cooperative, wait_until_completion, 3, global_size, local_size, args,
				   {}, {}, nullptr, std::forward<kernel_completion_handler_f>(completion_handler));
}

void device_queue::execute_with_parameters(const device_function& kernel, const execution_parameters_t& params,
											kernel_completion_handler_f&& completion_handler) const {
	if (params.execution_dim < 1u || params.execution_dim > 3u) {
		log_error("invalid execution dim: $", params.execution_dim);
		return;
	}
	kernel.execute(*this, params.is_cooperative, params.wait_until_completion, params.execution_dim, params.global_work_size, params.local_work_size,
				   params.args,  params.wait_fences, params.signal_fences, params.debug_label,
				   std::forward<kernel_completion_handler_f>(completion_handler));
}

void device_queue::execute_indirect(const indirect_command_pipeline& indirect_cmd,
									const indirect_execution_parameters_t& params,
									kernel_completion_handler_f&& completion_handler,
									const uint32_t command_offset,
									const uint32_t command_count) const {
	if (command_count == 0) {
		return;
	}
	if (!dev.indirect_command_support || !dev.indirect_compute_command_support) {
		log_error("device $ has no support for indirect command pipelines", dev.name);
		return;
	}
	
#if defined(FLOOR_DEBUG)
	if (indirect_cmd.get_description().command_type != indirect_command_description::COMMAND_TYPE::COMPUTE) {
		log_error("specified indirect command pipeline \"$\" must be a compute pipeline",
				  indirect_cmd.get_description().debug_label);
		return;
	}
#endif
	
	const auto& generic_indirect_cmd = (const generic_indirect_command_pipeline&)indirect_cmd;
	const auto generic_indirect_pipeline_entry = generic_indirect_cmd.get_pipeline_entry(dev);
	if (!generic_indirect_pipeline_entry) {
		log_error("no indirect command pipeline state for device \"$\" in indirect command pipeline \"$\"",
				  dev.name, indirect_cmd.get_description().debug_label);
		return;
	}
	
	const auto range = generic_indirect_cmd.compute_and_validate_command_range(command_offset, command_count);
	if (!range) {
		return;
	}
	
	// wait fences handling: we add "wait_fences" to all kernel executions until we hit the first kernel that acts as a barrier,
	//                       i.e. it has wait_until_completion set -> after that, we can safely ignore wait_fences
	bool has_encountered_barrier = false;
	const std::vector<const device_fence*> empty_wait_fences;
	const std::vector<device_fence*> empty_signal_fences;
	for (uint32_t cmd_idx = range->offset, cmd_end = range->offset + range->count; cmd_idx < cmd_end; ++cmd_idx) {
		const auto& cmd = generic_indirect_pipeline_entry->commands[cmd_idx];
		// the last command is somewhat special: it is used both to execute the final completion handler + emit "signal_fences"
		const auto is_last_cmd = (cmd_idx + 1u == cmd_end);
		cmd.kernel_ptr->execute(*this, false, cmd.wait_until_completion, cmd.dim, cmd.global_work_size, cmd.local_work_size, cmd.args,
								!has_encountered_barrier && !params.wait_fences.empty() ? params.wait_fences : empty_wait_fences,
								!is_last_cmd ? empty_signal_fences : params.signal_fences,
								params.debug_label ? params.debug_label : generic_indirect_pipeline_entry->debug_label.c_str(),
								is_last_cmd && completion_handler ? completion_handler : kernel_completion_handler_f {});
		if (cmd.wait_until_completion) {
			has_encountered_barrier = true;
		}
	}
	
	if (params.wait_until_completion) {
		finish();
	}
}

const device_context& device_queue::get_context() const {
	return *dev.context;
}

device_context& device_queue::get_mutable_context() const {
	return *dev.context;
}

} // namespace fl
