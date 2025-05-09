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

#pragma once

#include <floor/device/cuda/cuda_common.hpp>
#include <floor/device/device.hpp>

namespace fl {

FLOOR_PUSH_WARNINGS()
FLOOR_IGNORE_WARNING(weak-vtables)

class cuda_context;
class cuda_device final : public device {
public:
	cuda_device();
	
	//! compute capability (aka sm_xx)
	uint2 sm { 5, 0 };
	
	//! for sm_90+: is architecture-accelerated codegen enabled?
	bool sm_aa { false };
	
	//! max supported PTX version
	uint2 ptx { 8, 0 };
	
	//! minimum required PTX version
	uint2 min_req_ptx { 6, 0 };
	
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
	//! CUDA requires a context for each device (no shared context)
	cu_context ctx { nullptr };
	
	//!
	cu_device device_id { 0u };
	
	//! the original device sampler init function pointer
	CU_API CU_RESULT (*sampler_init_func_ptr)(cu_texture_ref tex_ref) { nullptr };
#else
	void* _ctx { nullptr };
	int32_t _device_id { 0u };
	void* _sampler_init_func_ptr { nullptr };
#endif
	
	//! returns true if the specified object is the same object as this
	bool operator==(const cuda_device& dev) const {
		return (this == &dev);
	}
	
	//! makes the CUDA context of this device current/active in the current thread
	bool make_context_current() const;
	
};

} // namespace fl

FLOOR_POP_WARNINGS()
