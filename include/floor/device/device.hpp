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

#include <string>
#include <vector>
#include <floor/device/device_common.hpp>
#include <floor/math/vector_lib.hpp>
#include <floor/core/enum_helpers.hpp>

namespace fl {

FLOOR_PUSH_WARNINGS()
FLOOR_IGNORE_WARNING(weak-vtables)

class device_context;
class device {
public:
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
	
	//! returns true if the device is a CPU
	bool is_cpu() const { return (type & TYPE::CPU) != TYPE::NONE; }
	//! returns true if the device is a GPU
	bool is_gpu() const { return (type & TYPE::GPU) != TYPE::NONE; }
	//! returns true if the device is neither a CPU nor a GPU
	bool is_no_cpu_or_gpu() const {
		return !(is_cpu() || is_gpu());
	}
	
	//! type for internal use (OpenCL: stores cl_device_type)
	uint32_t internal_type { 0u };
	
	//! vendor of this device
	VENDOR vendor { VENDOR::UNKNOWN };
	//! platform vendor of this device
	VENDOR platform_vendor { VENDOR::UNKNOWN };
	
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
	//! max amount of work-groups per dimension
	uint3 max_group_size;
	//! max total number of active work-items in a work-group
	uint32_t max_total_local_size { 0u };
	//! max total number of active work-items in a work-group when running a cooperative kernel
	uint32_t max_coop_total_local_size { 0u };
	//! max resident/concurrent number of work-items in an EU/SM/CU
	uint32_t max_resident_local_size { 0u };
	//! max amount of work-items that can be active/used per work-group per dimension
	uint3 max_local_size;
	//! max amount of work-items that can be active/used per dimension (generally: local size * group size)
	ulong3 max_global_size;
	//! max 1D image dimensions
	uint32_t max_image_1d_dim { 0u };
	//! max 1D buffer image dimensions
	size_t max_image_1d_buffer_dim { 0u };
	//! max 2D image dimensions
	uint2 max_image_2d_dim;
	//! max 3D image dimensions
	uint3 max_image_3d_dim;
	//! max amount of mip-levels that can exist
	uint32_t max_mip_levels { 1 };
	
	//! true if the device supports double precision floating point computation
	bool double_support { false };
	//! true if the device supports host unified memory/unified addressing
	bool unified_memory { false };
	//! true if the device has support for basic 64-bit atomic operations (add/sub/inc/dec/xchg/cmpxchg)
	bool basic_64_bit_atomics_support { false };
	//! true if the device has support for extended 64-bit atomic operations (min/max/and/or/xor)
	bool extended_64_bit_atomics_support { false };
	//! true if the device has native support for base 32-bit float operations (add/ld/st/xchg)
	bool basic_32_bit_float_atomics_support { false };
	//! true if the device supports sub-groups (OpenCL with extension; aka warp in cuda)
	bool sub_group_support { false };
	//! true if the device supports sub-group shuffle/swizzle
	bool sub_group_shuffle_support { false };
	//! true if the device supports cooperative kernel launchs
	bool cooperative_kernel_support { false };
	//! true if the device supports retrieving the primitive ID in the fragment shader
	bool primitive_id_support { false };
	//! true if the device supports retrieving the barycentric coordinate in the fragment shader
	bool barycentric_coord_support { false };
	
	//! true if images are supported by the device
	bool image_support { false };
	//! true if depth images are supported
	bool image_depth_support { false };
	//! true if writing depth images is supported
	bool image_depth_write_support { false };
	//! true if msaa images are supported
	bool image_msaa_support { false };
	//! true if writing msaa images is supported
	bool image_msaa_write_support { false };
	//! true if msaa array images are supported
	bool image_msaa_array_support { false };
	//! true if writing msaa array images is supported
	bool image_msaa_array_write_support { false };
	//! true if cube map images are supported
	bool image_cube_support { false };
	//! true if writing cube map images is supported
	bool image_cube_write_support { false };
	//! true if cube map array images are supported
	bool image_cube_array_support { false };
	//! true if writing cube map array images is supported
	bool image_cube_array_write_support { false };
	//! true if mip-map images are supported
	bool image_mipmap_support { false };
	//! true if writing mip-map images is supported
	bool image_mipmap_write_support { false };
	//! true if reading with an offset is supported in h/w
	bool image_offset_read_support { false };
	//! true if writing with an offset is supported in h/w
	bool image_offset_write_support { false };
	//! true if depth compare is supported in h/w (still supports s/w depth compare if false)
	bool image_depth_compare_support { false };
	//! true if image gather is supported
	bool image_gather_support { false };
	//! true if images that can both be read and written are supported
	bool image_read_write_support { false };
	
	//! true if anisotropic filtering is supported
	bool anisotropic_support { false };
	//! max anisotropy that is supported
	uint32_t max_anisotropy { 1u };
	
	//! true if the device supports indirect commands in general
	bool indirect_command_support { false };
	//! true if the device supports indirect compute commands
	bool indirect_compute_command_support { false };
	//! true if the device supports indirect render/graphics commands
	bool indirect_render_command_support { false };
	
	//! true if the device supports tessellation shaders
	bool tessellation_support { false };
	//! if tessellation shaders are supported, this specifies the max supported tessellation factor
	uint32_t max_tessellation_factor { 0u };
	
	//! true if the device has basic argument buffer support
	bool argument_buffer_support { false };
	//! true if the device supports images in argument buffers
	//! NOTE: otherwise, only buffers and simple variables/fields are supported
	bool argument_buffer_image_support { false };
	
	//! function parameter workaround (uses constant buffer instead of direct function parameter)
	bool param_workaround { false };
	
	//! device name in string form
	std::string name { "unknown" };
	//! device UUID (if present)
	std::array<uint8_t, 16> uuid { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
	//! true if the device can be identfied by a UUID and "uuid" is filled with the device UUID
	bool has_uuid { false };
	//! device vendor name in string form
	std::string vendor_name { "unknown" };
	//! device version in string form
	std::string version_str { "" };
	//! device driver version in string form
	std::string driver_version_str { "" };
	//! array of supported extensions (OpenCL/Vulkan only)
	std::vector<std::string> extensions;
	
	//! associated device_context this device is part of
	device_context* context { nullptr };
	
	//! returns true if the specified object is the same object as this
	bool operator==(const device& dev) const {
		return (this == &dev);
	}
	
};

FLOOR_POP_WARNINGS()

} // namespace fl
