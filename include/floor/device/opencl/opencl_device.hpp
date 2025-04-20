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

#include <floor/device/opencl/opencl_common.hpp>
#include <floor/device/device.hpp>

namespace fl {

FLOOR_PUSH_WARNINGS()
FLOOR_IGNORE_WARNING(weak-vtables)

#if !defined(FLOOR_NO_OPENCL)
class opencl_context;
#endif

class opencl_device final : public device {
public:
	opencl_device();
	
	//! OpenCL version of the device
	OPENCL_VERSION cl_version { OPENCL_VERSION::NONE };
	
	//! OpenCL C version of the device
	OPENCL_VERSION c_version { OPENCL_VERSION::NONE };
	
	//! max supported spir-v version of the device
	SPIRV_VERSION spirv_version { SPIRV_VERSION::NONE };
	
#if !defined(FLOOR_NO_OPENCL)
	//! associated OpenCL context
	cl_context ctx { nullptr };
	
	//! the OpenCL device id
	cl_device_id device_id { nullptr };
#else
	void* _ctx { nullptr };
	void* _device_id { nullptr };
#endif
	
	//! true if the device supports cl_intel_required_subgroup_size
	bool required_size_sub_group_support { false };
	
	//! returns true if the specified object is the same object as this
	bool operator==(const opencl_device& dev) const {
		return (this == &dev);
	}
	
};

FLOOR_POP_WARNINGS()

} // namespace fl
