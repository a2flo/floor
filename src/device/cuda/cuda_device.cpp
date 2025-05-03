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

#include <floor/device/cuda/cuda_device.hpp>
#include <floor/core/logger.hpp>

namespace fl {

cuda_device::cuda_device() : device() {
	// init statically known info
	type = device::TYPE::GPU;
	
	vendor = VENDOR::NVIDIA;
	platform_vendor = VENDOR::NVIDIA;
	vendor_name = "NVIDIA";
	
	simd_width = 32;
	simd_range = { simd_width, simd_width };
	max_total_local_size = 1024; // true for all GPUs right now
	max_resident_local_size = max_total_local_size; // minimum
	local_mem_dedicated = true;
	local_mem_size = 48u * 1024u; // always 48KiB for all supported generations
	double_support = true; // true for all GPUs since Fermi/sm_20
	basic_64_bit_atomics_support = true; // always true since Fermi/sm_20
	basic_32_bit_float_atomics_support = true; // always true since fermi/sm_20
	extended_64_bit_atomics_support = true; // always true since Kepler/sm_32
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
	image_depth_compare_support = false; // supported in PTX, but not supported by CUDA API (unless using internal API)
	image_gather_support = true;
	image_read_write_support = true;
	max_anisotropy = 16u;
	
	// will be overwritten later
	max_mip_levels = 16u; // 32768px
	
	// indirect compute pipelines are always supported
	indirect_command_support = true;
	indirect_compute_command_support = true;
}

bool cuda_device::make_context_current() const {
#if !defined(FLOOR_NO_CUDA)
	// NOTE: we manually store the current CUDA context for the current thread so that we don't incur an API call
	static thread_local cu_context thread_current_ctx { nullptr };
	if (thread_current_ctx != ctx) {
		CU_CALL_RET(cu_ctx_set_current(ctx), "failed to make CUDA context current", false)
		thread_current_ctx = ctx;
	}
#endif
	return true;
}

} // namespace fl
