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
	
#if !defined(FLOOR_NO_OPENCL)
	//! associated opencl context
	cl_context ctx { nullptr };
	
	//! associated opencl_compute context
	opencl_compute* compute_ctx { nullptr };
	
	//! opencl version of the device
	OPENCL_VERSION cl_version { OPENCL_VERSION::OPENCL_1_0 };
	
	//! opencl c version of the device
	OPENCL_VERSION c_version { OPENCL_VERSION::OPENCL_1_0 };
	
	//! the opencl device id
	cl_device_id device_id { nullptr };
#else
	void* _ctx { nullptr };
	void* _compute_ctx { nullptr };
	uint32_t _cl_version { 0 };
	uint32_t _c_version { 0 };
	void* _device_id { nullptr };
#endif
	
};

FLOOR_POP_WARNINGS()

#endif
