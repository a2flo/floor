/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2023 Florian Ziesche
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
	VULKAN_1_3,
};

constexpr VULKAN_VERSION vulkan_version_from_uint(const uint32_t major, const uint32_t minor) {
	if (major == 0 || major > 1) return VULKAN_VERSION::NONE;
	// major == 1
	switch (minor) {
		case 3: return VULKAN_VERSION::VULKAN_1_3;
		default: return VULKAN_VERSION::NONE;
	}
}

#if !defined(FLOOR_NO_VULKAN)

#include <vulkan/vulkan.h>

#if VK_HEADER_VERSION < 235
#error "Vulkan header version must at least be 235"
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
		case -13: return "VK_ERROR_UNKNOWN";
		case -1000069000: return "VK_ERROR_OUT_OF_POOL_MEMORY";
		case -1000072003: return "VK_ERROR_INVALID_EXTERNAL_HANDLE";
		case -1000161000: return "VK_ERROR_FRAGMENTATION";
		case -1000257000: return "VK_ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS";
		case -1000000000: return "VK_ERROR_SURFACE_LOST_KHR";
		case -1000000001: return "VK_ERROR_NATIVE_WINDOW_IN_USE_KHR";
		case 1000001003: return "VK_SUBOPTIMAL_KHR";
		case -1000001004: return "VK_ERROR_OUT_OF_DATE_KHR";
		case -1000003001: return "VK_ERROR_INCOMPATIBLE_DISPLAY_KHR";
		case -1000011001: return "VK_ERROR_VALIDATION_FAILED_EXT";
		case -1000012000: return "VK_ERROR_INVALID_SHADER_NV";
		case -1000158000: return "VK_ERROR_INVALID_DRM_FORMAT_MODIFIER_PLANE_LAYOUT_EXT";
		case -1000174001: return "VK_ERROR_NOT_PERMITTED_EXT";
		case -1000244000: return "VK_ERROR_INVALID_DEVICE_ADDRESS_EXT";
		case -1000255000: return "VK_ERROR_FULL_SCREEN_EXCLUSIVE_MODE_LOST_EXT";
		case 1000268000: return "VK_THREAD_IDLE_KHR";
		case 1000268001: return "VK_THREAD_DONE_KHR";
		case 1000268002: return "VK_OPERATION_DEFERRED_KHR";
		case 1000268003: return "VK_OPERATION_NOT_DEFERRED_KHR";
		case 1000297000: return "VK_PIPELINE_COMPILE_REQUIRED_EXT";
		default: break;
	}
	return "<UNKNOWN_ERROR>";
}

//! converts a VkObjectType to a human-readable string
constexpr const char* vulkan_object_type_to_string(const int& object_type) {
	switch (object_type) {
		case 0: return "unknown";
		case 1: return "instance";
		case 2: return "physical-device";
		case 3: return "device";
		case 4: return "queue";
		case 5: return "semaphore";
		case 6: return "command-buffer";
		case 7: return "fence";
		case 8: return "device-memory";
		case 9: return "buffer";
		case 10: return "image";
		case 11: return "event";
		case 12: return "query-pool";
		case 13: return "buffer-view";
		case 14: return "image-view";
		case 15: return "shader-module";
		case 16: return "pipeline-cache";
		case 17: return "pipeline-layout";
		case 18: return "render-pass";
		case 19: return "pipeline";
		case 20: return "descriptor-set-layout";
		case 21: return "sampler";
		case 22: return "descriptor-pool";
		case 23: return "descriptor-set";
		case 24: return "framebuffer";
		case 25: return "command-pool";
		case 1000156000: return "sampler-ycbcr-conversion";
		case 1000085000: return "descriptor-update-template";
		case 1000000000: return "surface-khr";
		case 1000001000: return "swapchain-khr";
		case 1000002000: return "display-khr";
		case 1000002001: return "display-mode-khr";
		case 1000011000: return "debug-report-callback-ext";
		case 1000128000: return "debug-utils-messenger-ext";
		case 1000150000: return "acceleration-structure-khr";
		case 1000160000: return "validation-cache-ext";
		case 1000165000: return "acceleration-structure-nv";
		case 1000210000: return "performance-configuration-intel";
		case 1000268000: return "deferred-operation-khr";
		case 1000277000: return "indirect-commands-layout-nv";
		case 1000295000: return "private-data-slot-ext";
		default: break;
	}
	return "<unknown-object-type>";
}

#define VK_CALL_RET(call, error_msg, ...) { \
	const auto call_err_var = call; \
	if(call_err_var != VK_SUCCESS) { \
		log_error("$: $: $", error_msg, call_err_var, vulkan_error_to_string(call_err_var)); \
		return __VA_ARGS__; \
	} \
}
#define VK_CALL_CONT(call, error_msg) { \
	const int32_t call_err_var = call; \
	if(call_err_var != VK_SUCCESS) { \
		log_error("$: $: $", error_msg, call_err_var, vulkan_error_to_string(call_err_var)); \
		continue; \
	} \
}
#define VK_CALL_BREAK(call, error_msg) { \
	const int32_t call_err_var = call; \
	if(call_err_var != VK_SUCCESS) { \
		log_error("$: $: $", error_msg, call_err_var, vulkan_error_to_string(call_err_var)); \
		break; \
	} \
}
#define VK_CALL_ERR_PARAM_RET(call, err_var_name, error_msg, ...) { \
	int32_t err_var_name = VK_SUCCESS; \
	call; \
	if(err_var_name != VK_SUCCESS) { \
		log_error("$: $: $", error_msg, err_var_name, vulkan_error_to_string(err_var_name)); \
		return __VA_ARGS__; \
	} \
}
#define VK_CALL_ERR_PARAM_CONT(call, err_var_name, error_msg) { \
	int32_t err_var_name = VK_SUCCESS; \
	call; \
	if(err_var_name != VK_SUCCESS) { \
		log_error("$: $: $", error_msg, err_var_name, vulkan_error_to_string(err_var_name)); \
		continue; \
	} \
}
#define VK_CALL_ERR_EXEC(call, error_msg, do_stuff) { \
	const auto call_err_var = call; \
	if(call_err_var != VK_SUCCESS) { \
		log_error("$: $: $", error_msg, call_err_var, vulkan_error_to_string(call_err_var)); \
		do_stuff \
	} \
}
#define VK_CALL_IGNORE(call, error_msg) { \
	const auto call_err_var = call; \
	if(call_err_var != VK_SUCCESS) { \
		log_error("$: $: $", error_msg, call_err_var, vulkan_error_to_string(call_err_var)); \
	} \
}

#endif // FLOOR_NO_VULKAN

#endif
