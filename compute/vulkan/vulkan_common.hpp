/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2019 Florian Ziesche
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

#ifndef __FLOOR_VULKAN_COMMON_HPP__
#define __FLOOR_VULKAN_COMMON_HPP__

#include <floor/core/essentials.hpp>
#include <cstdint>

//! vulkan version of the platform/driver/device
enum class VULKAN_VERSION : uint32_t {
	NONE,
	VULKAN_1_0,
	VULKAN_1_1,
};

constexpr VULKAN_VERSION vulkan_version_from_uint(const uint32_t major, const uint32_t minor) {
	if (major == 0 || major > 1) return VULKAN_VERSION::NONE;
	// major == 1
	switch (minor) {
		case 0: return VULKAN_VERSION::VULKAN_1_0;
		case 1: return VULKAN_VERSION::VULKAN_1_1;
		default: return VULKAN_VERSION::NONE;
	}
}

#if !defined(FLOOR_NO_VULKAN)

#include <vulkan/vulkan.h>

#if VK_HEADER_VERSION < 108
#error "Vulkan header version must at least be 108"
#endif

// workaround struct + enum rename
#if VK_HEADER_VERSION < 115
#define VkPhysicalDeviceShaderFloat16Int8FeaturesKHR VkPhysicalDeviceFloat16Int8FeaturesKHR
#define VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_FLOAT16_INT8_FEATURES_KHR VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FLOAT16_INT8_FEATURES_KHR
#endif

// for Vulkan resource sharing on Windows
#if defined(__WINDOWS__)
#if !defined(DXGI_SHARED_RESOURCE_READ)
#define DXGI_SHARED_RESOURCE_READ (0x80000000L)
#endif
#if !defined(DXGI_SHARED_RESOURCE_WRITE)
#define DXGI_SHARED_RESOURCE_WRITE (1)
#endif
#endif

constexpr const char* vulkan_error_to_string(const int& error_code) {
	// NOTE: don't use actual enums here so this doesn't have to rely on vulkan version or vendor specific headers
	switch(error_code) {
		case 0: return "VK_SUCCESS";
		case 1: return "VK_NOT_READY";
		case 2: return "VK_TIMEOUT";
		case 3: return "VK_EVENT_SET";
		case 4: return "VK_EVENT_RESET";
		case 5: return "VK_INCOMPLETE";
		case -1: return "VK_ERROR_OUT_OF_HOST_MEMORY";
		case -2: return "VK_ERROR_OUT_OF_DEVICE_MEMORY";
		case -3: return "VK_ERROR_INITIALIZATION_FAILED";
		case -4: return "VK_ERROR_DEVICE_LOST";
		case -5: return "VK_ERROR_MEMORY_MAP_FAILED";
		case -6: return "VK_ERROR_LAYER_NOT_PRESENT";
		case -7: return "VK_ERROR_EXTENSION_NOT_PRESENT";
		case -8: return "VK_ERROR_FEATURE_NOT_PRESENT";
		case -9: return "VK_ERROR_INCOMPATIBLE_DRIVER";
		case -10: return "VK_ERROR_TOO_MANY_OBJECTS";
		case -11: return "VK_ERROR_FORMAT_NOT_SUPPORTED";
		case -12: return "VK_ERROR_FRAGMENTED_POOL";
		case -1000000000: return "VK_ERROR_SURFACE_LOST_KHR";
		case -1000000001: return "VK_ERROR_NATIVE_WINDOW_IN_USE_KHR";
		case 1000001003: return "VK_SUBOPTIMAL_KHR";
		case -1000001004: return "VK_ERROR_OUT_OF_DATE_KHR";
		case -1000003001: return "VK_ERROR_INCOMPATIBLE_DISPLAY_KHR";
		case -1000011001: return "VK_ERROR_VALIDATION_FAILED_EXT";
		default: break;
	}
	return "<UNKNOWN_ERROR>";
}

#define VK_CALL_RET(call, error_msg, ...) { \
	const auto call_err_var = call; \
	if(call_err_var != VK_SUCCESS) { \
		log_error("%s: %u: %s", error_msg, call_err_var, vulkan_error_to_string(call_err_var)); \
		return __VA_ARGS__; \
	} \
}
#define VK_CALL_CONT(call, error_msg) { \
	const int32_t call_err_var = call; \
	if(call_err_var != VK_SUCCESS) { \
		log_error("%s: %u: %s", error_msg, call_err_var, vulkan_error_to_string(call_err_var)); \
		continue; \
	} \
}
#define VK_CALL_BREAK(call, error_msg) { \
	const int32_t call_err_var = call; \
	if(call_err_var != VK_SUCCESS) { \
		log_error("%s: %u: %s", error_msg, call_err_var, vulkan_error_to_string(call_err_var)); \
		break; \
	} \
}
#define VK_CALL_ERR_PARAM_RET(call, err_var_name, error_msg, ...) { \
	int32_t err_var_name = VK_SUCCESS; \
	call; \
	if(err_var_name != VK_SUCCESS) { \
		log_error("%s: %u: %s", error_msg, err_var_name, vulkan_error_to_string(err_var_name)); \
		return __VA_ARGS__; \
	} \
}
#define VK_CALL_ERR_PARAM_CONT(call, err_var_name, error_msg) { \
	int32_t err_var_name = VK_SUCCESS; \
	call; \
	if(err_var_name != VK_SUCCESS) { \
		log_error("%s: %u: %s", error_msg, err_var_name, vulkan_error_to_string(err_var_name)); \
		continue; \
	} \
}
#define VK_CALL_ERR_EXEC(call, error_msg, do_stuff) { \
	const auto call_err_var = call; \
	if(call_err_var != VK_SUCCESS) { \
		log_error("%s: %u: %s", error_msg, call_err_var, vulkan_error_to_string(call_err_var)); \
		do_stuff \
	} \
}
#define VK_CALL_IGNORE(call, error_msg) { \
	const auto call_err_var = call; \
	if(call_err_var != VK_SUCCESS) { \
		log_error("%s: %u: %s", error_msg, call_err_var, vulkan_error_to_string(call_err_var)); \
	} \
}

#endif // FLOOR_NO_VULKAN

#endif
