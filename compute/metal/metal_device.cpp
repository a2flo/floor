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
	image_cube_support = true;
	// image_cube_write_support decided later
	image_mipmap_support = true;
	image_mipmap_write_support = true;
	
	// seems to be true for all devices? (at least nvptx64, igil64 and A7+)
	bitness = 64;
	
	// for now (iOS9/OSX11 versions: metal 1.1.0, air 1.8.0, language 1.1.0)
	driver_version_str = "1.1.0";
}
