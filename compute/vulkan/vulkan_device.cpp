/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2020 Florian Ziesche
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

#include <floor/compute/vulkan/vulkan_device.hpp>

vulkan_device::vulkan_device() : compute_device() {
	// init statically known info
	local_mem_dedicated = true;
	
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
}
