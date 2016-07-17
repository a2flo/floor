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

#ifndef __FLOOR_OPENCL_DEVICE_HPP__
#define __FLOOR_OPENCL_DEVICE_HPP__

#include <floor/compute/opencl/opencl_common.hpp>
#include <floor/compute/compute_device.hpp>

FLOOR_PUSH_WARNINGS()
FLOOR_IGNORE_WARNING(weak-vtables)

#if !defined(FLOOR_NO_OPENCL)
class opencl_compute;
#endif

class opencl_device final : public compute_device {
public:
	~opencl_device() override {}
	
	//! opencl version of the device
	OPENCL_VERSION cl_version { OPENCL_VERSION::NONE };
	
	//! opencl c version of the device
	OPENCL_VERSION c_version { OPENCL_VERSION::NONE };
	
	//! max supported spir-v version of the device
	SPIRV_VERSION spirv_version { SPIRV_VERSION::NONE };
	
#if !defined(FLOOR_NO_OPENCL)
	//! associated opencl context
	cl_context ctx { nullptr };
	
	//! the opencl device id
	cl_device_id device_id { nullptr };
#else
	void* _ctx { nullptr };
	void* _device_id { nullptr };
#endif
	
	//! true if the device supports cl_intel_required_subgroup_size
	bool required_size_sub_group_support { false };
	
};

FLOOR_POP_WARNINGS()

#endif
