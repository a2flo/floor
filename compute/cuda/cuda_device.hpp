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

#ifndef __FLOOR_CUDA_DEVICE_HPP__
#define __FLOOR_CUDA_DEVICE_HPP__

#include <floor/compute/cuda/cuda_common.hpp>
#include <floor/compute/compute_device.hpp>

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wweak-vtables"
#endif

class FLOOR_API cuda_device final : public compute_device {
public:
	~cuda_device() override {}
	
	//! compute capability (aka sm_xx)
	uint2 sm { 2, 0 };
	
	//!
	uint32_t max_registers_per_block { 0u };
	
	//!
	uint32_t l2_cache_size { 0u };
	
	//!
	uint32_t vendor_id { 0u };
	
	//!
	uint32_t warp_size { 0u };
	
	//!
	uint32_t mem_bus_width { 0u };
	
	//!
	uint32_t async_engine_count { 0u };
	
#if !defined(FLOOR_NO_CUDA)
	// cuda requires a context for each device (no shared context)
	CUcontext ctx { nullptr };
	
	//!
	CUdevice device_id { 0u };
#endif
	
};

#if defined(__clang__)
#pragma clang diagnostic pop
#endif

#endif
