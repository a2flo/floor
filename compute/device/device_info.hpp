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

#ifndef __FLOOR_COMPUTE_DEVICE_DEVICE_INFO_HPP__
#define __FLOOR_COMPUTE_DEVICE_DEVICE_INFO_HPP__

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
		POCL,
		HOST,
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
	
	//! device bitness (32 or 64)
	constexpr uint32_t bitness() {
#if defined(PLATFORM_X32)
		return 32u;
#elif defined(PLATFORM_X64)
		return 64u;
#endif
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
	//! NOTE: only returns a valid value on OS X and iOS
	//! OS X: identical to MAC_OS_X_VERSION_10_* macro
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
	//! NOTE: for CUDA this is true for all sm_32+ devices
	//! NOTE: for Metal this is false for all devices
	constexpr bool has_native_extended_64_bit_atomics() {
		return (FLOOR_COMPUTE_INFO_HAS_NATIVE_EXTENDED_64_BIT_ATOMICS != 0);
	}
	
	//! returns true if the device supports atomic operations on pointer types
	constexpr bool has_pointer_atomics() {
#if defined(PLATFORM_X32)
		return true; // always true
#elif defined(PLATFORM_X64)
		return has_64_bit_atomics(); // must support 64-bit atomics otherwise
#endif
	}
	
	//! returns true if the device has dedicated local memory h/w
	constexpr bool has_dedicated_local_memory() {
		return (FLOOR_COMPUTE_INFO_HAS_DEDICATED_LOCAL_MEMORY != 0);
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
	
	//! returns true if the device supports sub-groups (opencl with extension; always true with cuda)
	constexpr bool has_sub_groups() {
#if defined(FLOOR_COMPUTE_INFO_HAS_SUB_GROUPS)
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
	
	//! returns true if cube map images are supported
	constexpr bool has_image_cube_support() {
		return (FLOOR_COMPUTE_INFO_HAS_IMAGE_CUBE_SUPPORT != 0);
	}
	
	//! returns true if writing cube map images is supported
	constexpr bool has_image_cube_write_support() {
		return (FLOOR_COMPUTE_INFO_HAS_IMAGE_CUBE_WRITE_SUPPORT != 0);
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
	
	//! returns the max amount of mip-levels that is supported by the device
	constexpr uint32_t max_mip_levels() {
		return FLOOR_COMPUTE_INFO_MAX_MIP_LEVELS;
	}
	
}

#endif

#endif
