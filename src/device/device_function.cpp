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

#include <floor/device/device_function.hpp>
#include <floor/device/device_queue.hpp>
#include <floor/core/logger.hpp>

namespace fl {

uint3 device_function::check_local_work_size(const uint3 wanted_local_work_size,
											 const uint3 max_local_size,
											 const uint32_t max_total_local_size) {
	// make sure all elements are always at least 1
	uint3 ret = wanted_local_work_size.maxed(1u);
	
	const auto wanted_max_local_size = ret.extent();
	if (max_total_local_size > 0 && wanted_max_local_size > max_total_local_size) {
		assert((max_local_size > 0u).all());
		
		// if local work size y-dim is > 1, max work-size is > 1 and device work-group item sizes y-dim is > 2, set it at least to 2
		// NOTE: this is usually a good idea for image accesses / cache use
		if (wanted_local_work_size.y > 1 && max_total_local_size > 1 && max_local_size.y > 1) {
			ret = { max_total_local_size / 2u, 2, 1 };
			// TODO: might want to have/keep a specific shape
		} else {
			// just return max possible local work size "{ max, 1, 1 }"
			ret = { max_total_local_size, 1, 1 };
		}
	}
	
	return ret;
}

uint3 device_function::check_local_work_size(const device_function::function_entry& entry, const uint3& local_work_size) const {
	const auto checked_local_work_size = check_local_work_size(local_work_size, entry.max_local_size, entry.max_total_local_size);
	if (checked_local_work_size != local_work_size) {
		// only warn/error once about this, don't want to spam the console/log unnecessarily
		bool do_warn = false;
		{
			GUARD(warn_map_lock);
			do_warn = (warn_map.count(&entry) == 0);
			warn_map.emplace(&entry, 1u);
		}
		
		if (do_warn) {
			log_error("$: specified work-group size ($) too large for this device (max: $) - using $ now!",
					  (entry.info ? entry.info->name : "<unknown>"), local_work_size.maxed(1u).extent(), entry.max_total_local_size,
					  checked_local_work_size);
		}
	}
	return checked_local_work_size;
}

std::unique_ptr<argument_buffer> device_function::create_argument_buffer(const device_queue& cqueue, const uint32_t& arg_index,
																		 const MEMORY_FLAG add_mem_flags,
																		 const bool zero_init) const {
	const auto& dev = cqueue.get_device();
	const auto entry = get_function_entry(dev);
	if (!entry || !entry->info) {
		log_error("no function entry/info \"$\" for device $", function_name, dev.name);
		return {};
	}
	
	// need to take care of argument index translation when stage_input arguments exist
	uint32_t idx = 0;
	for (uint32_t actual_idx = 0, count = uint32_t(entry->info->args.size()); idx <= count; ++actual_idx) {
		if (idx >= count) {
			log_error("argument index is out-of-bounds: $", arg_index);
			return {};
		}
		
		const auto& arg_info = entry->info->args[idx];
		if (actual_idx == arg_index) {
			if (!has_flag<toolchain::ARG_FLAG::ARGUMENT_BUFFER>(arg_info.flags)) {
				log_error("argument #$ is not an argument buffer", arg_index);
				return {};
			}
			break;
		}
		
		if (has_flag<toolchain::ARG_FLAG::STAGE_INPUT>(arg_info.flags)) {
			// skip to next actual arg
			while (idx < entry->info->args.size() &&
				   has_flag<toolchain::ARG_FLAG::STAGE_INPUT>(entry->info->args[idx].flags)) {
				++idx;
			}
		} else {
			++idx;
		}
	}
	
	return create_argument_buffer_internal(cqueue, *entry, entry->info->args[idx], arg_index, idx, add_mem_flags, zero_init);
}

std::unique_ptr<argument_buffer> device_function::create_argument_buffer_internal(const device_queue&,
																				  const function_entry&,
																				  const toolchain::arg_info&,
																				  const uint32_t&,
																				  const uint32_t&,
																				  const MEMORY_FLAG&,
																				  const bool) const {
	log_error("argument buffer creation not implemented for this backend");
	return {};
}

} // namespace fl
