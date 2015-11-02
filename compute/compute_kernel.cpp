/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2015 Florian Ziesche
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

compute_kernel::~compute_kernel() {}

uint3 compute_kernel::check_local_work_size(const compute_kernel::kernel_entry& entry, const uint3& local_work_size) {
	// make sure all elements are always at least 1
	uint3 ret = local_work_size.maxed(1u);
	
	const auto work_group_size = ret.x * ret.y * ret.z;
	if(entry.max_local_work_size > 0 &&
	   work_group_size > entry.max_local_work_size) {
		// only warn/error once about this, don't want to spam the console/log unnecessarily
		bool do_warn = false;
		{
			GUARD(warn_map_lock);
			do_warn = (warn_map.count(&entry) == 0);
			warn_map.insert(&entry, true);
		}
		
		// return max possible local work size "{ max, 1, 1 }"
		// TODO: might want to have/keep a specific shape, or at least y == 2 if possible
		ret = { (uint32_t)entry.max_local_work_size, 1, 1 };
		
		if(do_warn) {
			log_error("specified work-group size (%u) too large for this device (max: %u) - using %v now!",
					  work_group_size, entry.max_local_work_size, ret);
		}
	}
	return ret;
}
