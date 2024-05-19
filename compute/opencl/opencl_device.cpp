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

#include <floor/compute/opencl/opencl_device.hpp>

opencl_device::opencl_device() : compute_device() {
	local_mem_size = 16u * 1024u; // at least 16KiB
	
	// these are never supported
	image_msaa_write_support = false;
	image_msaa_array_write_support = false;
	image_cube_support = false;
	image_cube_write_support = false;
	image_cube_array_support = false;
	image_cube_array_write_support = false;
	image_offset_read_support = false;
	image_offset_write_support = false;
}
