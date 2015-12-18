/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2015 Florian Ziesche
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

#ifndef __FLOOR_HOST_DEVICE_HPP__
#define __FLOOR_HOST_DEVICE_HPP__

#include <floor/compute/compute_device.hpp>
#include <floor/core/core.hpp>
#include <floor/floor/floor_version.hpp>

FLOOR_PUSH_WARNINGS()
FLOOR_IGNORE_WARNING(weak-vtables)

class host_device final : public compute_device {
public:
	static constexpr const uint32_t host_compute_local_memory_size { 128ull * 1024ull * 1024ull };
	
	host_device() : compute_device() {
		// init statically known info
		type = type = compute_device::TYPE::CPU0;
		platform_vendor = COMPUTE_VENDOR::HOST;
		version_str = FLOOR_BUILD_VERSION_STR;
		driver_version_str = FLOOR_BUILD_VERSION_STR;
		
		local_mem_size = host_compute_local_memory_size;
		local_mem_dedicated = false;
		
		// always at least 4 (SSE, newer NEON), 8-wide if avx/avx, 16-wide if avx-512
		simd_width = (core::cpu_has_avx() ? (core::cpu_has_avx512() ? 16 : 8) : 4);
		
		max_work_item_sizes = { 0xFFFFFFFFu };
		
		// can technically use any dim as long as it fits into memory
		max_image_1d_dim = { 65536 };
		max_image_2d_dim = { 65536, 65536 };
		max_image_3d_dim = { 65536, 65536, 65536 };
		
		image_support = true;
		double_support = true;
		unified_memory = true;
		basic_64_bit_atomics_support = true;
		extended_64_bit_atomics_support = true;
		
#if defined(PLATFORM_X32)
		bitness = 32;
#elif defined(PLATFORM_X64)
		bitness = 64;
#endif
	}
	~host_device() override {}
	
#if !defined(FLOOR_NO_HOST_COMPUTE)
#endif
	
};

FLOOR_POP_WARNINGS()

#endif
