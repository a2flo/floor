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

#ifndef __FLOOR_COMPUTE_DEVICE_HPP__
#define __FLOOR_COMPUTE_DEVICE_HPP__

#include <string>
#include <vector>
#include <floor/compute/compute_common.hpp>
#include <floor/math/vector_lib.hpp>
#include <floor/core/enum_helpers.hpp>

FLOOR_PUSH_WARNINGS()
FLOOR_IGNORE_WARNING(weak-vtables)

class compute_device {
public:
	virtual ~compute_device() = 0;
	
	//! device types for device selection
	enum class TYPE : uint32_t {
		// sub-types
		GPU = (1u << 31u), //!< bit is set if device is a GPU (only use for testing)
		CPU = (1u << 30u), //!< bit is set if device is a CPU (only use for testing)
		FASTEST_FLAG = (1u << 29u), //!< bit is set if device the fastest of its group (only use for testing)
		__MAX_SUB_TYPE = FASTEST_FLAG, //!< don't use
		__MAX_SUB_TYPE_MASK = __MAX_SUB_TYPE - 1u, //!< don't use
		
		NONE = 0u, //!< select no device
		ANY = 1u, //!< select any device (usually the first)
		FASTEST = ANY | FASTEST_FLAG, //!< select fastest overall device
		FASTEST_GPU = GPU | FASTEST_FLAG, //!< select fastest GPU
		FASTEST_CPU = CPU | FASTEST_FLAG, //!< select fastest CPU
		
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
	floor_enum_ext(TYPE)
	
	//! types of this device
	TYPE type { TYPE::NONE };
	
	//! returns true if the device is a cpu
	bool is_cpu() const { return (type & TYPE::CPU) != TYPE::NONE; }
	//! returns true if the device is a gpu
	bool is_gpu() const { return (type & TYPE::GPU) != TYPE::NONE; }
	
	//! type for internal use (OpenCL: stores cl_device_type)
	uint32_t internal_type { 0u };
	
	//! vendor of this device
	COMPUTE_VENDOR vendor { COMPUTE_VENDOR::UNKNOWN };
	//! platform vendor of this device
	COMPUTE_VENDOR platform_vendor { COMPUTE_VENDOR::UNKNOWN };
	
	//! number of compute units in the device
	uint32_t units { 0u };
	//! expected SIMD-width of the device (or 0 if unknown)
	uint32_t simd_width { 0u };
	//! minimum/maximum SIMD-width for devices with a variable range
	uint2 simd_range;
	//! clock frequency in MHz
	uint32_t clock { 0u };
	//! memory clock frequency in MHz
	uint32_t mem_clock { 0u };
	//! global memory size in bytes
	uint64_t global_mem_size { 0u };
	//! local (OpenCL) / shared (CUDA) memory size in bytes
	uint64_t local_mem_size { 0u };
	//! true if dedicated local memory h/w exists, false if not (i.e. stored in global memory instead)
	bool local_mem_dedicated { false };
	//! constant memory size in bytes
	uint64_t constant_mem_size { 0u };
	//! max chunk size that can be allocated in global memory
	uint64_t max_mem_alloc { 0u };
	//! max number of active work-items in a work-group (CUDA: threads per block)
	uint32_t max_work_group_size { 0u };
	//! max amount of work-items that can be active/used per dimension
	//! (OpenCL: devices sizeof(size_t) range, CUDA: grid dim * block dim)
	ulong3 max_work_item_sizes;
	//! max amount of work-items that can be active/used per work-group (CUDA: block dim)
	uint3 max_work_group_item_sizes;
	//! max 1D image dimensions
	size_t max_image_1d_dim { 0u };
	//! max 1D buffer image dimensions
	size_t max_image_1d_buffer_dim { 0u };
	//! max 2D image dimensions
	size2 max_image_2d_dim;
	//! max 3D image dimensions
	size3 max_image_3d_dim;
	//! bitness of the device (32 or 64)
	uint32_t bitness { 32u };
	
	//! true if images are supported by the device
	bool image_support { false };
	//! true if the device supports double precision floating point computation
	bool double_support { false };
	//! true if the device supports host unified memory/unified addressing
	bool unified_memory { false };
	//! true if the device has support for basic 64-bit atomic operations (add/sub/inc/dec/xchg/cmpxchg)
	bool basic_64_bit_atomics_support { false };
	//! true if the device has support for extended 64-bit atomic operations (min/max/and/or/xor)
	bool extended_64_bit_atomics_support { false };
	//! true if the device supports sub-groups (opencl with extension; aka warp in cuda)
	bool sub_group_support { false };
	
	//! device name in string form
	string name { "unknown" };
	//! device vendor name in string form
	string vendor_name { "unknown" };
	//! device version in string form
	string version_str { "" };
	//! device driver version in string form
	string driver_version_str { "" };
	//! array of supported extensions (OpenCL only)
	vector<string> extensions;
	
};

FLOOR_POP_WARNINGS()

#endif
