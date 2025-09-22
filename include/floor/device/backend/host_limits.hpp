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

#pragma once

// id/size ranges
#define FLOOR_DEVICE_INFO_GLOBAL_ID_RANGE_MIN 0u
#define FLOOR_DEVICE_INFO_GLOBAL_ID_RANGE_MAX 0xFFFFFFFFu
#define FLOOR_DEVICE_INFO_GLOBAL_SIZE_RANGE_MIN 0u
#define FLOOR_DEVICE_INFO_GLOBAL_SIZE_RANGE_MAX 0xFFFFFFFFu
#define FLOOR_DEVICE_INFO_LOCAL_ID_RANGE_MIN 0u
#define FLOOR_DEVICE_INFO_LOCAL_ID_RANGE_MAX 1024u
#define FLOOR_DEVICE_INFO_LOCAL_SIZE_RANGE_MIN 1u
#define FLOOR_DEVICE_INFO_LOCAL_SIZE_RANGE_MAX (FLOOR_DEVICE_INFO_LOCAL_ID_RANGE_MAX + 1u)
#define FLOOR_DEVICE_INFO_GROUP_ID_RANGE_MIN 0u
#define FLOOR_DEVICE_INFO_GROUP_ID_RANGE_MAX 0xFFFFFFFFu
#define FLOOR_DEVICE_INFO_GROUP_SIZE_RANGE_MIN 1u
#define FLOOR_DEVICE_INFO_GROUP_SIZE_RANGE_MAX 0xFFFFFFFFu
#define FLOOR_DEVICE_INFO_SUB_GROUP_LOCAL_ID_RANGE_MIN 0u
#define FLOOR_DEVICE_INFO_SUB_GROUP_LOCAL_ID_RANGE_MAX 32u
#define FLOOR_DEVICE_INFO_SUB_GROUP_SIZE_RANGE_MIN 1u
#define FLOOR_DEVICE_INFO_SUB_GROUP_SIZE_RANGE_MAX (FLOOR_DEVICE_INFO_SUB_GROUP_LOCAL_ID_RANGE_MAX + 1u)
#define FLOOR_DEVICE_INFO_SUB_GROUP_ID_RANGE_MIN 0u
#define FLOOR_DEVICE_INFO_SUB_GROUP_ID_RANGE_MAX (FLOOR_DEVICE_INFO_LOCAL_ID_RANGE_MAX / FLOOR_DEVICE_INFO_SUB_GROUP_LOCAL_ID_RANGE_MAX)
#define FLOOR_DEVICE_INFO_NUM_SUB_GROUPS_RANGE_MIN 1u
#define FLOOR_DEVICE_INFO_NUM_SUB_GROUPS_RANGE_MAX (FLOOR_DEVICE_INFO_SUB_GROUP_ID_RANGE_MAX + 1u)

//! various host limits that are known at compile-time
namespace fl::host_limits {
	//! max amout of local memory that can be allocated per work-group
	static constexpr const size_t local_memory_size { 128ull * 1024ull };
	
	//! fixed SIMD-width
	static constexpr const uint32_t simd_width { FLOOR_DEVICE_INFO_SUB_GROUP_LOCAL_ID_RANGE_MAX };
	
	//! max amount of SIMD groups
	static constexpr const uint32_t max_simd_groups { FLOOR_DEVICE_INFO_SUB_GROUP_ID_RANGE_MAX };
	
	//! max supported image dim, identical for all image types
	static constexpr const uint32_t max_image_dim { 32768u };
	
	//! max amount of mip-map/lod levels that can exist (log2(max_image_dim) + 1)
	static constexpr const uint32_t max_mip_levels { 16u };
	static_assert((1u << (max_mip_levels - 1u)) == max_image_dim, "max #lod-levels must match max image dim");
#if defined(FLOOR_DEVICE_INFO_MAX_MIP_LEVELS)
	static_assert(FLOOR_DEVICE_INFO_MAX_MIP_LEVELS == max_mip_levels,
				  "update FLOOR_DEVICE_INFO_MAX_MIP_LEVELS to match host_limits::max_mip_levels");
#endif
	
	//! max work-items per work-group (max #fibers)
	static constexpr const uint32_t max_total_local_size {
		FLOOR_DEVICE_INFO_LOCAL_ID_RANGE_MAX
	};
	
} // namespace fl::host_limits
