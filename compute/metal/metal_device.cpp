/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2019 Florian Ziesche
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

#include <floor/compute/metal/metal_device.hpp>

metal_device::metal_device() : compute_device() {
	// init statically known info
	type = compute_device::TYPE::GPU;
	platform_vendor = COMPUTE_VENDOR::APPLE;
	
	local_mem_dedicated = true;
	
	image_support = true;
	image_depth_support = true;
	image_depth_write_support = false;
	image_msaa_support = true;
	image_msaa_write_support = false;
	image_msaa_array_support = false; // doesn't exist
	image_msaa_array_write_support = false;
	image_cube_support = true;
	// image_cube_write_support, image_cube_array* decided later
	image_mipmap_support = true;
	image_mipmap_write_support = true;
	image_offset_read_support = true;
	image_offset_write_support = false;
	image_depth_compare_support = true;
	image_gather_support = true;
	
	// good default
#if !defined(FLOOR_IOS)
	max_total_local_size = 1024;
#else
	max_total_local_size = 512;
#endif
	
	// for now (iOS9/OSX11 versions: metal 1.1.0, air 1.8.0, language 1.1.0)
	metal_version = METAL_VERSION::METAL_1_1;
	driver_version_str = "1.1.0";
}
