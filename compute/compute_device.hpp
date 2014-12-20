/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2014 Florian Ziesche
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

#ifndef __FLOOR_COMPUTE_DEVICE_HPP__
#define __FLOOR_COMPUTE_DEVICE_HPP__

#include <string>
#include <vector>
#include <floor/math/vector.hpp>

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wweak-vtables"
#endif

class compute_device {
public:
	virtual ~compute_device() = 0;
	
	//! device types for device selection
	enum class TYPE : uint32_t {
		// sub-types
		GPU = (1u << 31u), //!< bit is set if device is a GPU (only use for testing)
		CPU = (1u << 30u), //!< bit is set if device is a CPU (only use for testing)
		FASTEST = (1u << 29u), //!< bit is set if device the fastest of its group (only use for testing)
		__MAX_SUB_TYPE = FASTEST, //!< don't use
		__MAX_SUB_TYPE_MASK = __MAX_SUB_TYPE - 1u, //!< don't use
		
		NONE = 0u, //!< select no device
		FASTEST_GPU = GPU | FASTEST, //!< select fastest GPU
		FASTEST_CPU = CPU | FASTEST, //!< select fastest CPU
		
		ALL_GPU = GPU | __MAX_SUB_TYPE_MASK, //!< select all GPUs
		ALL_CPU = CPU | __MAX_SUB_TYPE_MASK, //!< select all CPUs
		ALL_DEVICES = GPU | CPU | __MAX_SUB_TYPE_MASK, //!< select all devices
		
		GPU0 = GPU, //!< first GPU
		GPU1,
		GPU2,
		GPU3,
		GPU4,
		GPU5,
		GPU6,
		GPU7,
		GPU255 = GPU0 + 255u, //!< 256th GPU (this should be enough)
		
		CPU0 = CPU, //!< first CPU
		CPU1,
		CPU2,
		CPU3,
		CPU4,
		CPU5,
		CPU6,
		CPU7,
		CPU255 = CPU0 + 255u, //!< 256th CPU
	};
	//! types of this device
	TYPE type { TYPE::NONE };
	
	//! type for internal use (OpenCL: stores cl_device_type, CUDA: N/A)
	uint32_t internal_type { 0u };
	
	//! opencl and cuda device vendors
	enum class VENDOR : uint32_t {
		NVIDIA,
		INTEL,
		AMD,
		APPLE,
		FREEOCL,
		POCL,
		UNKNOWN
	};
	//! vendor of this device
	VENDOR vendor { VENDOR::UNKNOWN };
	
	//! number of compute units in the device
	uint32_t units { 0u };
	//! clock frequency in MHz
	uint32_t clock { 0u };
	//! global memory size in bytes
	uint64_t global_mem_size { 0u };
	//! local (OpenCL) / shared (CUDA) memory size in bytes
	uint64_t local_mem_size { 0u };
	//! constant memory size in bytes
	uint64_t constant_mem_size { 0u };
	//! max chunk size that can be allocated in global memory
	uint64_t max_mem_alloc { 0u };
	//! max number of active work-items in a work-group
	size_t max_workgroup_size { 0u };
	//! max amount of work-items that can be active/used per dimension
	size3 max_workgroup_sizes;
	//! max 2D image dimensions
	size2 max_image_2d_dim;
	//! max 3D image dimensions
	size3 max_image_3d_dim;
	
	//! true if images are supported by the device
	bool image_support { false };
	//! true if the device supports double precision floating point computation
	bool double_support { false };
	
	//! device name in string form
	string name { "unknown" };
	//! device vendor name in string form
	string vendor_name { "unknown" };
	//! device version in string form
	string version_str { "" };
	//! device driver version in string form
	string driver_version_str { "" };
	//! array of supported extensions (OpenCL only, CUDA: TBD)
	vector<string> extensions;
	
};

#if defined(__clang__)
#pragma clang diagnostic pop
#endif

#endif
