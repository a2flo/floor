/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2016 Florian Ziesche
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

#ifndef __FLOOR_COMPUTE_DEVICE_HOST_LIMITS_HPP__
#define __FLOOR_COMPUTE_DEVICE_HOST_LIMITS_HPP__

namespace host_limits {
	//! max amout of local memory that can be allocated per work-group
	static constexpr const size_t local_memory_size { 128ull * 1024ull * 1024ull };
	
	//! max supported image dim, identical for all image types
	static constexpr const uint32_t max_image_dim { 32768 };
	
	//! max amount of mip-map/lod levels that can exist (log2(max_image_dim) + 1)
	static constexpr const uint32_t max_mip_levels { 16 };
	static_assert((1u << (max_mip_levels - 1u)) == max_image_dim, "max #lod-levels must match max image dim");
#if defined(FLOOR_COMPUTE_INFO_MAX_MIP_LEVELS)
	static_assert(FLOOR_COMPUTE_INFO_MAX_MIP_LEVELS == max_mip_levels,
				  "update FLOOR_COMPUTE_INFO_MAX_MIP_LEVELS to match host_limits::max_mip_levels");
#endif
	
	//! max work-items per work-group (max #fibers)
	static constexpr const uint32_t max_work_group_size {
#if !defined(__WINDOWS__)
		1024
#else // due to memory restrictions with windows fibers, this shouldn't be higher than 64
		64
#endif
	};
	
}

#endif
