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

#include <floor/core/essentials.hpp>
#include <cstdint>
#include <functional>

namespace fl {

//! used to differentiate between the different platform implementations
enum class PLATFORM_TYPE : uint64_t {
	NONE	= 0u,
	OPENCL	= 1u,
	CUDA	= 2u,
	METAL	= 3u,
	HOST	= 4u,
	VULKAN	= 5u,
};

//! returns the string representation of the enum PLATFORM_TYPE
floor_inline_always static constexpr const char* platform_type_to_string(const PLATFORM_TYPE& vendor) {
	switch(vendor) {
		case PLATFORM_TYPE::OPENCL: return "OpenCL";
		case PLATFORM_TYPE::CUDA: return "CUDA";
		case PLATFORM_TYPE::METAL: return "Metal";
		case PLATFORM_TYPE::HOST: return "Host-Compute";
		case PLATFORM_TYPE::VULKAN: return "Vulkan";
		default: return "NONE";
	}
}

//! used to identify the platform and device vendor
enum class VENDOR : uint32_t {
	UNKNOWN,
	NVIDIA,
	INTEL,
	AMD,
	APPLE,
	HOST,
	KHRONOS,
	MESA,
};

//! returns the string representation of the enum VENDOR
floor_inline_always static constexpr const char* vendor_to_string(const VENDOR& vendor) {
	switch(vendor) {
		case VENDOR::NVIDIA: return "NVIDIA";
		case VENDOR::INTEL: return "INTEL";
		case VENDOR::AMD: return "AMD";
		case VENDOR::APPLE: return "APPLE";
		case VENDOR::HOST: return "HOST";
		case VENDOR::KHRONOS: return "KHRONOS";
		case VENDOR::MESA: return "MESA";
		default: return "UNKNOWN";
	}
}

//! user-definable kernel completion handler
using kernel_completion_handler_f = std::function<void()>;

} // namespace fl
