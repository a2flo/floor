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

#include <floor/compute/cuda/cuda_device.hpp>

cuda_device::cuda_device() : compute_device() {
	// init statically known info
	type = compute_device::TYPE::GPU;
	
	vendor = COMPUTE_VENDOR::NVIDIA;
	platform_vendor = COMPUTE_VENDOR::NVIDIA;
	vendor_name = "NVIDIA";
	
	simd_width = 32;
	simd_range = { simd_width, simd_width };
	max_total_local_size = 1024; // true for all gpus right now
	local_mem_dedicated = true;
	local_mem_size = 48u * 1024u; // always 48KiB for all supported generations
	double_support = true; // true for all gpus since fermi/sm_20
	basic_64_bit_atomics_support = true; // always true since fermi/sm_20
	basic_32_bit_float_atomics_support = true; // always true since fermi/sm_20
	sub_group_support = true;
	sub_group_shuffle_support = true; // since Kepler/sm_30
	argument_buffer_support = true;
	
	image_support = true;
	image_depth_support = true;
	image_depth_write_support = true;
	image_msaa_support = true; // at least sm_30, which is required for images anyways
	image_msaa_write_support = false;
	image_msaa_array_support = true;
	image_msaa_array_write_support = false;
	image_cube_support = true;
	image_cube_write_support = false;
	image_cube_array_support = true;
	image_cube_array_write_support = false;
	image_mipmap_support = true;
	image_mipmap_write_support = true;
	image_offset_read_support = true;
	image_offset_write_support = false;
	image_depth_compare_support = false; // supported in ptx, but not supported by cuda api (unless using internal api)
	image_gather_support = true;
	image_read_write_support = true;
	max_anisotropy = 16u;
}
