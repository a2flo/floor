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

//! used to differentiate between the different compute implementations
enum class COMPUTE_TYPE : uint64_t {
	NONE	= 0u,
	OPENCL	= 1u,
	CUDA	= 2u,
	METAL	= 3u,
	HOST	= 4u,
	VULKAN	= 5u,
};

//! returns the string representation of the enum COMPUTE_TYPE
floor_inline_always static constexpr const char* compute_type_to_string(const COMPUTE_TYPE& vendor) {
	switch(vendor) {
		case COMPUTE_TYPE::OPENCL: return "OpenCL";
		case COMPUTE_TYPE::CUDA: return "CUDA";
		case COMPUTE_TYPE::METAL: return "Metal";
		case COMPUTE_TYPE::HOST: return "Host Compute";
		case COMPUTE_TYPE::VULKAN: return "Vulkan";
		default: return "NONE";
	}
}

//! used to identify the platform and device vendor
enum class COMPUTE_VENDOR : uint32_t {
	UNKNOWN,
	NVIDIA,
	INTEL,
	AMD,
	APPLE,
	HOST,
	KHRONOS,
};

//! returns the string representation of the enum COMPUTE_VENDOR
floor_inline_always static constexpr const char* compute_vendor_to_string(const COMPUTE_VENDOR& vendor) {
	switch(vendor) {
		case COMPUTE_VENDOR::NVIDIA: return "NVIDIA";
		case COMPUTE_VENDOR::INTEL: return "INTEL";
		case COMPUTE_VENDOR::AMD: return "AMD";
		case COMPUTE_VENDOR::APPLE: return "APPLE";
		case COMPUTE_VENDOR::HOST: return "HOST";
		case COMPUTE_VENDOR::KHRONOS: return "KHRONOS";
		default: return "UNKNOWN";
	}
}

//! user-definable kernel completion handler
using kernel_completion_handler_f = std::function<void()>;
