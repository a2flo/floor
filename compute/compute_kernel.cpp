/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2024 Florian Ziesche
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

#include <floor/compute/compute_kernel.hpp>
#include <floor/compute/compute_queue.hpp>
#include <floor/core/logger.hpp>

uint3 compute_kernel::check_local_work_size(const compute_kernel::kernel_entry& entry, const uint3& local_work_size) const {
	// make sure all elements are always at least 1
	uint3 ret = local_work_size.maxed(1u);
	
	const auto work_group_size = ret.x * ret.y * ret.z;
	if(entry.max_total_local_size > 0 &&
	   work_group_size > entry.max_total_local_size) {
		// only warn/error once about this, don't want to spam the console/log unnecessarily
		bool do_warn = false;
		{
			GUARD(warn_map_lock);
			do_warn = (warn_map.count(&entry) == 0);
			warn_map.insert(&entry, true);
		}
		
		// if local work size y-dim is > 1, max work-size is > 1 and device work-group item sizes y-dim is > 2, set it at least to 2
		// note that this is usually a good idea for image accesses / cache use
		if(local_work_size.y > 1 && entry.max_total_local_size > 1 && entry.max_local_size.y > 1) {
			ret = { (uint32_t)(entry.max_total_local_size / 2u), 2, 1 };
			// TODO: might want to have/keep a specific shape
		}
		// just return max possible local work size "{ max, 1, 1 }"
		else ret = { (uint32_t)entry.max_total_local_size, 1, 1 };
		
		if(do_warn) {
			log_error("specified work-group size ($) too large for this device (max: $) - using $ now!",
					  work_group_size, entry.max_total_local_size, ret);
		}
	}
	return ret;
}

unique_ptr<argument_buffer> compute_kernel::create_argument_buffer(const compute_queue& cqueue, const uint32_t& arg_index,
																   const COMPUTE_MEMORY_FLAG add_mem_flags) const {
	const auto& dev = cqueue.get_device();
	const auto entry = get_kernel_entry(dev);
	if (!entry || !entry->info) {
		log_error("no kernel entry/info for device $", dev.name);
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
			if (arg_info.special_type != llvm_toolchain::SPECIAL_TYPE::ARGUMENT_BUFFER) {
				log_error("argument #$ is not an argument buffer", arg_index);
				return {};
			}
			break;
		}
		
		if (arg_info.special_type == llvm_toolchain::SPECIAL_TYPE::STAGE_INPUT) {
			// skip to next actual arg
			while (idx < entry->info->args.size() &&
				   entry->info->args[idx].special_type == llvm_toolchain::SPECIAL_TYPE::STAGE_INPUT) {
				++idx;
			}
		} else {
			++idx;
		}
	}
	
	return create_argument_buffer_internal(cqueue, *entry, entry->info->args[idx], arg_index, idx, add_mem_flags);
}

unique_ptr<argument_buffer> compute_kernel::create_argument_buffer_internal(const compute_queue&,
																			const kernel_entry&,
																			const llvm_toolchain::arg_info&,
																			const uint32_t&,
																			const uint32_t&,
																			const COMPUTE_MEMORY_FLAG&) const {
	log_error("argument buffer creation not implemented for this backend");
	return {};
}
