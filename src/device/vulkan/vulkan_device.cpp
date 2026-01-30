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

#include <floor/device/vulkan/vulkan_device.hpp>

namespace fl {

vulkan_device::vulkan_device() : device() {
	// init statically known info
	local_mem_dedicated = true;
	local_mem_size = 16u * 1024u; // at least 16KiB
	primitive_id_support = true;
	sub_group_support = true;
	sub_group_shuffle_support = true;
	sub_group_ballot_support = true;
	
	// enable all the things
	image_support = true;
	image_depth_support = true;
	image_depth_write_support = true;
	image_msaa_support = true;
	image_msaa_write_support = true;
	image_msaa_array_support = false; // determined later
	image_msaa_array_write_support = false;
	image_cube_support = true;
	image_cube_write_support = true;
	image_cube_array_support = false; // determined later
	image_cube_array_write_support = false;
	image_mipmap_support = true;
	image_mipmap_write_support = true;
	image_offset_read_support = true;
	image_offset_write_support = true;
	image_depth_compare_support = true;
	image_gather_support = false; // for now (needs floor support)
	image_read_write_support = false;
	
	// these will be overwritten later on init
	max_mip_levels = default_max_mip_levels; // 32768px
	
	max_inline_uniform_block_size = min_required_inline_uniform_block_size;
	max_inline_uniform_block_count = min_required_high_inline_uniform_block_count;
	
	max_descriptor_set_count = min_required_high_bound_descriptor_sets;
}

} // namespace fl
