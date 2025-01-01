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

#if defined(FLOOR_COMPUTE)

//! this namespace contains device, compile and platform information that can be queried at compile-time.
//! note that most of this information can also be accessed through macros/defines, but this provides an
//! easier to use interface when using it with c++ constructs (templates enable_if, type_traits, etc).
namespace device_info {
	//! device and platform vendors
	enum class VENDOR : uint32_t {
		NVIDIA,
		INTEL,
		AMD,
		APPLE,
		HOST,
		KHRONOS,
		UNKNOWN
	};
	//! returns the device vendor
	constexpr VENDOR vendor() {
		return VENDOR::FLOOR_COMPUTE_INFO_VENDOR;
	}
	//! returns the platform vendor
	constexpr VENDOR platform_vendor() {
		return VENDOR::FLOOR_COMPUTE_INFO_PLATFORM_VENDOR;
	}
	
	//! device bitness
	//! NOTE: we only support 64-bit now
	constexpr uint32_t bitness() {
		return 64u;
	}
	
	//! device hardware types
	enum class TYPE : uint32_t {
		GPU,
		CPU,
		UNKNOWN
	};
	//! returns the device type
	constexpr TYPE type() {
		return TYPE::FLOOR_COMPUTE_INFO_TYPE;
	}
	
	//! operating systems
	enum class OS : uint32_t {
		IOS,
		VISIONOS,
		OSX,
		WINDOWS,
		LINUX,
		FREEBSD,
		OPENBSD,
		UNKNOWN
	};
	//! returns the operating system this is compiled with/for
	constexpr OS os() {
		return OS::FLOOR_COMPUTE_INFO_OS;
	}
	
	//! returns the operating system version
	//! NOTE: only returns a valid value on macOS and iOS
	//! macOS: identical to MAC_OS_X_VERSION_* macro
	//! iOS: identical to __IPHONE_*_* macro
	constexpr size_t os_version() {
		return FLOOR_COMPUTE_INFO_OS_VERSION;
	}
	
	//! returns true if the device has native fma instruction support
	constexpr bool has_fma() {
		return (FLOOR_COMPUTE_INFO_HAS_FMA != 0);
	}
	
	//! returns true if the device has native 64-bit atomics support
	//! NOTE: for OpenCL this is true if cl_khr_int64_base_atomics is supported
	//! NOTE: for CUDA this is true for all devices
	//! NOTE: for Metal this is false for all devices
	constexpr bool has_64_bit_atomics() {
		return (FLOOR_COMPUTE_INFO_HAS_64_BIT_ATOMICS != 0);
	}
	
	//! returns true if the device has native support for extended 64-bit atomics (min, max, and, or, xor),
	//! note that if this is false, these functions are still supported, but implemented through a CAS loop
	//! if the device has support for basic 64-bit atomics of course.
	//! NOTE: for OpenCL this is true if cl_khr_int64_extended_atomics is supported
	//! NOTE: for CUDA this is always true
	//! NOTE: for Metal this is false for all devices
	constexpr bool has_native_extended_64_bit_atomics() {
		return (FLOOR_COMPUTE_INFO_HAS_NATIVE_EXTENDED_64_BIT_ATOMICS != 0);
	}
	
	//! returns true if the device has native 32-bit float atomics support
	//! NOTE: for CUDA this is true for all devices
	//! NOTE: for Vulkan this is true if VK_EXT_shader_atomic_float with global/local float32 add/ld/st/xchg is supported
	constexpr bool has_32_bit_float_atomics() {
		return (FLOOR_COMPUTE_INFO_HAS_32_BIT_FLOAT_ATOMICS != 0);
	}
	
	//! returns true if the device supports atomic operations on pointer types
	constexpr bool has_pointer_atomics() {
		return has_64_bit_atomics(); // must support 64-bit atomics
	}
	
	//! returns true if the device has dedicated local memory h/w
	constexpr bool has_dedicated_local_memory() {
		return (FLOOR_COMPUTE_INFO_HAS_DEDICATED_LOCAL_MEMORY != 0);
	}
	
	//! returns the amount of the dedicated local memory that is supported by the device
	//! NOTE: for CUDA, this always equals the static memory size
	//! NOTE: generally, this should return a value >= 16KiB
	constexpr uint32_t dedicated_local_memory() {
		return FLOOR_COMPUTE_INFO_DEDICATED_LOCAL_MEMORY;
	}
	
	//! returns true if the device has primitive ID support
	constexpr bool has_primitive_id() {
		return (FLOOR_COMPUTE_INFO_HAS_PRIMITIVE_ID != 0);
	}
	
	//! returns true if the device has barycentric coordinate support
	constexpr bool has_barycentric_coord() {
		return (FLOOR_COMPUTE_INFO_HAS_BARYCENTRIC_COORD != 0);
	}
	
	//! returns the min part of the possible global id [min, max) range of this device
	constexpr uint32_t global_id_range_min() {
		return FLOOR_COMPUTE_INFO_GLOBAL_ID_RANGE_MIN;
	}
	
	//! returns the max part of the possible global id [min, max) range of this device
	constexpr uint32_t global_id_range_max() {
		return FLOOR_COMPUTE_INFO_GLOBAL_ID_RANGE_MAX;
	}
	
	//! returns the min part of the possible global size [min, max) range of this device
	constexpr uint32_t global_size_range_min() {
		return FLOOR_COMPUTE_INFO_GLOBAL_SIZE_RANGE_MIN;
	}
	
	//! returns the max part of the possible global size [min, max) range of this device
	constexpr uint32_t global_size_range_max() {
		return FLOOR_COMPUTE_INFO_GLOBAL_SIZE_RANGE_MAX;
	}
	
	//! returns the min part of the possible local id [min, max) range of this device
	constexpr uint32_t local_id_range_min() {
		return FLOOR_COMPUTE_INFO_LOCAL_ID_RANGE_MIN;
	}
	
	//! returns the max part of the possible local id [min, max) range of this device
	constexpr uint32_t local_id_range_max() {
		return FLOOR_COMPUTE_INFO_LOCAL_ID_RANGE_MAX;
	}
	
	//! returns the min part of the possible local size [min, max) range of this device
	constexpr uint32_t local_size_range_min() {
		return FLOOR_COMPUTE_INFO_LOCAL_SIZE_RANGE_MIN;
	}
	
	//! returns the max part of the possible local size [min, max) range of this device
	constexpr uint32_t local_size_range_max() {
		return FLOOR_COMPUTE_INFO_LOCAL_SIZE_RANGE_MAX;
	}
	
	//! returns the min part of the possible group id [min, max) range of this device
	constexpr uint32_t group_id_range_min() {
		return FLOOR_COMPUTE_INFO_GROUP_ID_RANGE_MIN;
	}
	
	//! returns the max part of the possible group id [min, max) range of this device
	constexpr uint32_t group_id_range_max() {
		return FLOOR_COMPUTE_INFO_GROUP_ID_RANGE_MAX;
	}
	
	//! returns the min part of the possible group size [min, max) range of this device
	constexpr uint32_t group_size_range_min() {
		return FLOOR_COMPUTE_INFO_GROUP_SIZE_RANGE_MIN;
	}
	
	//! returns the max part of the possible group size [min, max) range of this device
	constexpr uint32_t group_size_range_max() {
		return FLOOR_COMPUTE_INFO_GROUP_SIZE_RANGE_MAX;
	}
	
	//! returns the expected SIMD-width of the device (or 0 if unknown)
	//! NOTE: for certain devices this might be variable both at run-time and at compile-time,
	//!       use simd_width_min() and simd_width_max() to retrieve the expected range
	constexpr uint32_t simd_width() {
#if !defined(FLOOR_COMPUTE_INFO_SIMD_WIDTH_OVERRIDE)
		return FLOOR_COMPUTE_INFO_SIMD_WIDTH;
#else // provide mechanism to override the detected SIMD-width
		return FLOOR_COMPUTE_INFO_SIMD_WIDTH_OVERRIDE;
#endif
	}
	
	//! returns the minimum SIMD-width of the device (or 0 if unknown)
	constexpr uint32_t simd_width_min() {
		return FLOOR_COMPUTE_INFO_SIMD_WIDTH_MIN;
	}
	
	//! returns the maximum SIMD-width of the device (or 0 if unknown)
	constexpr uint32_t simd_width_max() {
		return FLOOR_COMPUTE_INFO_SIMD_WIDTH_MAX;
	}
	
	//! returns true if the device has a known SIMD-width (> 1) and this SIMD-width is known
	//! and fixed at compile-time and run-time (min and max are the same)
	constexpr bool has_fixed_known_simd_width() {
		return (FLOOR_COMPUTE_INFO_SIMD_WIDTH_MIN > 1u &&
				FLOOR_COMPUTE_INFO_SIMD_WIDTH_MIN == FLOOR_COMPUTE_INFO_SIMD_WIDTH_MAX);
	}
	
#if FLOOR_COMPUTE_INFO_HAS_SUB_GROUPS != 0
	//! returns the min part of the possible sub-group id [min, max) range of this device
	constexpr uint32_t sub_group_id_range_min() {
		return FLOOR_COMPUTE_INFO_SUB_GROUP_ID_RANGE_MIN;
	}
	
	//! returns the max part of the possible sub-group id [min, max) range of this device
	constexpr uint32_t sub_group_id_range_max() {
		return FLOOR_COMPUTE_INFO_SUB_GROUP_ID_RANGE_MAX;
	}
	
	//! returns the min part of the possible sub-group local id [min, max) range of this device
	constexpr uint32_t sub_group_local_id_range_min() {
		return FLOOR_COMPUTE_INFO_SUB_GROUP_LOCAL_ID_RANGE_MIN;
	}
	
	//! returns the max part of the possible sub-group local id [min, max) range of this device
	constexpr uint32_t sub_group_local_id_range_max() {
		return FLOOR_COMPUTE_INFO_SUB_GROUP_LOCAL_ID_RANGE_MAX;
	}
	
	//! returns the min part of the possible sub-group size [min, max) range of this device
	constexpr uint32_t sub_group_size_range_min() {
		return FLOOR_COMPUTE_INFO_SUB_GROUP_SIZE_RANGE_MIN;
	}
	
	//! returns the max part of the possible sub-group size [min, max) range of this device
	constexpr uint32_t sub_group_size_range_max() {
		return FLOOR_COMPUTE_INFO_SUB_GROUP_SIZE_RANGE_MAX;
	}
	
	//! returns the min part of the possible #sub-groups [min, max) range of this device
	constexpr uint32_t num_sub_groups_range_min() {
		return FLOOR_COMPUTE_INFO_NUM_SUB_GROUPS_RANGE_MIN;
	}
	
	//! returns the max part of the possible #sub-groups [min, max) range of this device
	constexpr uint32_t num_sub_groups_range_max() {
		return FLOOR_COMPUTE_INFO_NUM_SUB_GROUPS_RANGE_MAX;
	}
#endif
	
	//! returns true if the device supports sub-groups (opencl with extension; always true with cuda)
	constexpr bool has_sub_groups() {
#if FLOOR_COMPUTE_INFO_HAS_SUB_GROUPS != 0
		return true;
#else
		return false;
#endif
	}
	
	//! returns true if the device supports sub-group shuffle/swizzle (opencl with extension; always with CUDA and macOS)
	constexpr bool has_sub_group_shuffle() {
#if FLOOR_COMPUTE_INFO_HAS_SUB_GROUP_SHUFFLE != 0
		return true;
#else
		return false;
#endif
	}
	
	//! returns true if the device supports cooperative kernel launchs (currently CUDA with sm_60+)
	constexpr bool has_cooperative_kernel_support() {
#if FLOOR_COMPUTE_INFO_HAS_COOPERATIVE_KERNEL != 0
		return true;
#else
		return false;
#endif
	}
	
	//! when using cuda, this returns the sm version of the device this is compiled for, returns 0 otherwise
	constexpr uint32_t cuda_sm() {
#if defined(FLOOR_COMPUTE_CUDA)
		return FLOOR_COMPUTE_INFO_CUDA_SM;
#else
		return 0;
#endif
	}
	
	//! when using cuda, this returns true if architecture-accelerated codegen is enabled, returns false otherwise
	constexpr bool cuda_sm_aa() {
#if defined(FLOOR_COMPUTE_CUDA)
		return (FLOOR_COMPUTE_INFO_CUDA_SM_AA != 0);
#else
		return false;
#endif
	}
	
	//! when using cuda, this returns the ptx version this is compiled for, returns 0 otherwise
	constexpr uint32_t cuda_ptx() {
#if defined(FLOOR_COMPUTE_CUDA)
		return FLOOR_COMPUTE_INFO_CUDA_PTX;
#else
		return 0;
#endif
	}
	
	//! returns true if images are supported by the device
	constexpr bool has_image_support() {
		return (FLOOR_COMPUTE_INFO_HAS_IMAGE_SUPPORT != 0);
	}
	
	//! returns true if depth images are supported
	constexpr bool has_image_depth_support() {
		return (FLOOR_COMPUTE_INFO_HAS_IMAGE_DEPTH_SUPPORT != 0);
	}
	
	//! returns true if writing depth images is supported
	constexpr bool has_image_depth_write_support() {
		return (FLOOR_COMPUTE_INFO_HAS_IMAGE_DEPTH_WRITE_SUPPORT != 0);
	}
	
	//! returns true if msaa images are supported
	constexpr bool has_image_msaa_support() {
		return (FLOOR_COMPUTE_INFO_HAS_IMAGE_MSAA_SUPPORT != 0);
	}
	
	//! returns true if writing msaa images is supported
	constexpr bool has_image_msaa_write_support() {
		return (FLOOR_COMPUTE_INFO_HAS_IMAGE_MSAA_WRITE_SUPPORT != 0);
	}
	
	//! returns true if msaa array images are supported
	constexpr bool has_image_msaa_array_support() {
		return (FLOOR_COMPUTE_INFO_HAS_IMAGE_MSAA_ARRAY_SUPPORT != 0);
	}
	
	//! returns true if writing msaa array images is supported
	constexpr bool has_image_msaa_array_write_support() {
		return (FLOOR_COMPUTE_INFO_HAS_IMAGE_MSAA_ARRAY_WRITE_SUPPORT != 0);
	}
	
	//! returns true if cube map images are supported
	constexpr bool has_image_cube_support() {
		return (FLOOR_COMPUTE_INFO_HAS_IMAGE_CUBE_SUPPORT != 0);
	}
	
	//! returns true if writing cube map images is supported
	constexpr bool has_image_cube_write_support() {
		return (FLOOR_COMPUTE_INFO_HAS_IMAGE_CUBE_WRITE_SUPPORT != 0);
	}
	
	//! returns true if cube map array images are supported
	constexpr bool has_image_cube_array_support() {
		return (FLOOR_COMPUTE_INFO_HAS_IMAGE_CUBE_ARRAY_SUPPORT != 0);
	}
	
	//! returns true if writing cube map array images is supported
	constexpr bool has_image_cube_array_write_support() {
		return (FLOOR_COMPUTE_INFO_HAS_IMAGE_CUBE_ARRAY_WRITE_SUPPORT != 0);
	}
	
	//! returns true if mip-map images are supported
	constexpr bool has_image_mipmap_support() {
		return (FLOOR_COMPUTE_INFO_HAS_IMAGE_MIPMAP_SUPPORT != 0);
	}
	
	//! returns true if writing mip-map images is supported
	constexpr bool has_image_mipmap_write_support() {
		return (FLOOR_COMPUTE_INFO_HAS_IMAGE_MIPMAP_WRITE_SUPPORT != 0);
	}
	
	//! returns true if reading with an offset is supported in h/w
	constexpr bool has_image_offset_read_support() {
		return (FLOOR_COMPUTE_INFO_HAS_IMAGE_OFFSET_READ_SUPPORT != 0);
	}
	
	//! returns true if writing with an offset is supported in h/w
	constexpr bool has_image_offset_write_support() {
		return (FLOOR_COMPUTE_INFO_HAS_IMAGE_OFFSET_WRITE_SUPPORT != 0);
	}
	
	//! returns true if depth compare is supported in h/w
	constexpr bool has_image_depth_compare_support() {
		return (FLOOR_COMPUTE_INFO_HAS_IMAGE_DEPTH_COMPARE_SUPPORT != 0);
	}
	
	//! returns true if image gather is supported
	constexpr bool has_image_gather_support() {
		return (FLOOR_COMPUTE_INFO_HAS_IMAGE_GATHER_SUPPORT != 0);
	}
	
	//! returns true if images that can both be read and written are supported
	constexpr bool has_image_read_write_support() {
		return (FLOOR_COMPUTE_INFO_HAS_IMAGE_READ_WRITE_SUPPORT != 0);
	}
	
	//! returns the max amount of mip-levels that is supported by the device
	constexpr uint32_t max_mip_levels() {
		return FLOOR_COMPUTE_INFO_MAX_MIP_LEVELS;
	}
	
	//! returns true if the backend and device have general indirect command support
	constexpr bool has_indirect_command_support() {
		return (FLOOR_COMPUTE_INFO_INDIRECT_COMMAND_SUPPORT != 0);
	}
	
	//! returns true if the backend and device have general indirect compute command support
	constexpr bool has_indirect_compute_command_support() {
		return (FLOOR_COMPUTE_INFO_INDIRECT_COMPUTE_COMMAND_SUPPORT != 0);
	}
	
	//! returns true if the backend and device have general indirect render command support
	constexpr bool has_indirect_render_command_support() {
		return (FLOOR_COMPUTE_INFO_INDIRECT_RENDER_COMMAND_SUPPORT != 0);
	}
	
	//! returns true if the backend and device have tessellation shader support
	constexpr bool has_tessellation_support() {
		return (FLOOR_COMPUTE_INFO_TESSELLATION_SUPPORT != 0);
	}
	
	//! returns the max supported tessellation factor that is supported by the device
	constexpr uint32_t max_tessellation_factor() {
		return FLOOR_COMPUTE_INFO_MAX_TESSELLATION_FACTOR;
	}
	
	//! returns true if basic argument buffers are supported by the device
	constexpr bool has_argument_buffer_support() {
		return (FLOOR_COMPUTE_INFO_HAS_ARGUMENT_BUFFER_SUPPORT != 0);
	}
	
	//! returns true if images and image arrays are supported in argument buffers
	//! NOTE: otherwise, only buffers and simple variables/fields are supported
	constexpr bool has_argument_buffer_image_support() {
		return (FLOOR_COMPUTE_INFO_HAS_ARGUMENT_BUFFER_IMAGE_SUPPORT != 0);
	}
	
	//! returns true if the device requires that the work-group size X is a power-of-two
	//! NOTE: true for Vulkan, false for all other devices/backends
	constexpr bool requires_work_group_size_x_is_pot() {
#if defined(FLOOR_COMPUTE_VULKAN)
		return true;
#else
		return false;
#endif
	}
	
}

//! range attribute containing the global [min, max) id range
#define FLOOR_GLOBAL_ID_RANGE_ATTR [[range(FLOOR_COMPUTE_INFO_GLOBAL_ID_RANGE_MIN, FLOOR_COMPUTE_INFO_GLOBAL_ID_RANGE_MAX)]]
//! range attribute containing the global [min, max) size range
#define FLOOR_GLOBAL_SIZE_RANGE_ATTR [[range(FLOOR_COMPUTE_INFO_GLOBAL_SIZE_RANGE_MIN, FLOOR_COMPUTE_INFO_GLOBAL_SIZE_RANGE_MAX)]]
//! range attribute containing the local [min, max) id range
#define FLOOR_LOCAL_ID_RANGE_ATTR [[range(FLOOR_COMPUTE_INFO_LOCAL_ID_RANGE_MIN, FLOOR_COMPUTE_INFO_LOCAL_ID_RANGE_MAX)]]
//! range attribute containing the local [min, max) size range
#define FLOOR_LOCAL_SIZE_RANGE_ATTR [[range(FLOOR_COMPUTE_INFO_LOCAL_SIZE_RANGE_MIN, FLOOR_COMPUTE_INFO_LOCAL_SIZE_RANGE_MAX)]]
//! range attribute containing the group [min, max) id range
#define FLOOR_GROUP_ID_RANGE_ATTR [[range(FLOOR_COMPUTE_INFO_GROUP_ID_RANGE_MIN, FLOOR_COMPUTE_INFO_GROUP_ID_RANGE_MAX)]]
//! range attribute containing the group [min, max) size range
#define FLOOR_GROUP_SIZE_RANGE_ATTR [[range(FLOOR_COMPUTE_INFO_GROUP_SIZE_RANGE_MIN, FLOOR_COMPUTE_INFO_GROUP_SIZE_RANGE_MAX)]]

#if FLOOR_COMPUTE_INFO_HAS_SUB_GROUPS != 0
//! range attribute containing the sub-group [min, max) id range
#define FLOOR_SUB_GROUP_ID_RANGE_ATTR [[range(FLOOR_COMPUTE_INFO_SUB_GROUP_ID_RANGE_MIN, FLOOR_COMPUTE_INFO_SUB_GROUP_ID_RANGE_MAX)]]
//! range attribute containing the sub-group [min, max) local id range
#define FLOOR_SUB_GROUP_LOCAL_ID_RANGE_ATTR [[range(FLOOR_COMPUTE_INFO_SUB_GROUP_LOCAL_ID_RANGE_MIN, FLOOR_COMPUTE_INFO_SUB_GROUP_LOCAL_ID_RANGE_MAX)]]
//! range attribute containing the sub-group [min, max) size range
#define FLOOR_SUB_GROUP_SIZE_RANGE_ATTR [[range(FLOOR_COMPUTE_INFO_SUB_GROUP_SIZE_RANGE_MIN, FLOOR_COMPUTE_INFO_SUB_GROUP_SIZE_RANGE_MAX)]]
//! range attribute containing the number of sub-groups [min, max) range
#define FLOOR_NUM_SUB_GROUPS_RANGE_ATTR [[range(FLOOR_COMPUTE_INFO_NUM_SUB_GROUPS_RANGE_MIN, FLOOR_COMPUTE_INFO_NUM_SUB_GROUPS_RANGE_MAX)]]
#endif

#endif
