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

#include <floor/device/host/host_device.hpp>
#include <floor/device/backend/host_limits.hpp>
#include <floor/core/core.hpp>
#include <floor/floor_version.hpp>

namespace fl {

host_device::host_device() : device() {
	// init statically known info
	type = type = device::TYPE::CPU0;
	platform_vendor = VENDOR::HOST;
	version_str = FLOOR_BUILD_VERSION_STR;
	driver_version_str = FLOOR_BUILD_VERSION_STR;
	
	local_mem_size = host_limits::local_memory_size;
	local_mem_dedicated = false;
	
	// always at least 4 (SSE, newer NEON), 8-wide if AVX/AVX2, 16-wide if AVX-512
	simd_width = (core::cpu_has_avx() ? (core::cpu_has_avx512() ? 16 : 8) : 4);
	simd_range = { 1, simd_width };
	
	max_global_size = { 0xFFFFFFFFu };
	
	// can technically use any dim as long as it fits into memory
	// TODO: should actually check for these when creating an image
	max_image_1d_dim = { host_limits::max_image_dim };
	max_image_2d_dim = { host_limits::max_image_dim };
	max_image_3d_dim = { host_limits::max_image_dim };
	max_mip_levels = host_limits::max_mip_levels;
	
	double_support = true;
	unified_memory = true;
	basic_64_bit_atomics_support = true;
	extended_64_bit_atomics_support = true;
	
	image_support = true;
	image_depth_support = true;
	image_depth_write_support = true;
	image_msaa_support = false; // TODO: implement this
	image_msaa_write_support = false;
	image_msaa_array_support = false;
	image_msaa_array_write_support = false;
	image_cube_support = true;
	image_cube_write_support = true;
	image_cube_array_support = true;
	image_cube_array_write_support = true;
	image_mipmap_support = true;
	image_mipmap_write_support = true;
	image_offset_read_support = true;
	image_offset_write_support = true;
	image_depth_compare_support = true;
	image_gather_support = false; // for now
	image_read_write_support = true;
	
	argument_buffer_support = true;
	
	// indirect compute pipelines are always supported
	indirect_command_support = true;
	indirect_compute_command_support = true;
}

} // namespace fl
