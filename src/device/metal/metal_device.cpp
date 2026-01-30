/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2026 Florian Ziesche
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

#include <floor/device/metal/metal_device.hpp>

namespace fl {

metal_device::metal_device() : device() {
	// init statically known info
	type = device::TYPE::GPU;
	platform_vendor = VENDOR::APPLE;
	
	local_mem_dedicated = true;
	local_mem_size = 32u * 1024u; // at least 32KiB
	
	sub_group_support = true;
	sub_group_shuffle_support = true;
	sub_group_ballot_support = true;
	basic_32_bit_float_atomics_support = true;
	simd_reduction = true;
	
	image_support = true;
	image_depth_support = true;
	image_depth_write_support = false;
	image_msaa_support = true;
	image_msaa_write_support = false;
	image_msaa_array_support = false; // doesn't exist
	image_msaa_array_write_support = false;
	image_cube_support = true;
	image_cube_write_support = true;
	image_cube_array_support = true;
	image_cube_array_write_support = true;
	image_mipmap_support = true;
	image_mipmap_write_support = true;
	image_offset_read_support = true;
	image_offset_write_support = false;
	image_depth_compare_support = true;
	image_gather_support = true;
	max_anisotropy = 16u;
	
	// will be overwritten later
	max_mip_levels = 15u; // 16384px
	
	// tessellation is always supported with 64 max factor
	tessellation_support = true;
	max_tessellation_factor = 64u;
	
	primitive_id_support = true;
	
	argument_buffer_support = true;
	argument_buffer_image_support = true;
	
	// we always have indirect command support
	indirect_render_command_support = true;
	
	// always the case on Metal3 GPUs
	max_total_local_size = 1024;
	
	driver_version_str = "";
}

} // namespace fl
