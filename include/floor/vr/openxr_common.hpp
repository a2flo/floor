/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2026 Florian Ziesche
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

// NOTE: this requires both VR and Vulkan support (no point in anything else)
#if !defined(FLOOR_NO_OPENXR) && !defined(FLOOR_NO_VULKAN)

namespace fl {

constexpr const char* xr_error_to_string(const int error_code) {
	switch (error_code) {
		case 0: return "XR_SUCCESS";
		case 1: return "XR_TIMEOUT_EXPIRED";
		case 3: return "XR_SESSION_LOSS_PENDING";
		case 4: return "XR_EVENT_UNAVAILABLE";
		case 7: return "XR_SPACE_BOUNDS_UNAVAILABLE";
		case 8: return "XR_SESSION_NOT_FOCUSED";
		case 9: return "XR_FRAME_DISCARDED";
		case -1: return "XR_ERROR_VALIDATION_FAILURE";
		case -2: return "XR_ERROR_RUNTIME_FAILURE";
		case -3: return "XR_ERROR_OUT_OF_MEMORY";
		case -4: return "XR_ERROR_API_VERSION_UNSUPPORTED";
		case -6: return "XR_ERROR_INITIALIZATION_FAILED";
		case -7: return "XR_ERROR_FUNCTION_UNSUPPORTED";
		case -8: return "XR_ERROR_FEATURE_UNSUPPORTED";
		case -9: return "XR_ERROR_EXTENSION_NOT_PRESENT";
		case -10: return "XR_ERROR_LIMIT_REACHED";
		case -11: return "XR_ERROR_SIZE_INSUFFICIENT";
		case -12: return "XR_ERROR_HANDLE_INVALID";
		case -13: return "XR_ERROR_INSTANCE_LOST";
		case -14: return "XR_ERROR_SESSION_RUNNING";
		case -16: return "XR_ERROR_SESSION_NOT_RUNNING";
		case -17: return "XR_ERROR_SESSION_LOST";
		case -18: return "XR_ERROR_SYSTEM_INVALID";
		case -19: return "XR_ERROR_PATH_INVALID";
		case -20: return "XR_ERROR_PATH_COUNT_EXCEEDED";
		case -21: return "XR_ERROR_PATH_FORMAT_INVALID";
		case -22: return "XR_ERROR_PATH_UNSUPPORTED";
		case -23: return "XR_ERROR_LAYER_INVALID";
		case -24: return "XR_ERROR_LAYER_LIMIT_EXCEEDED";
		case -25: return "XR_ERROR_SWAPCHAIN_RECT_INVALID";
		case -26: return "XR_ERROR_SWAPCHAIN_FORMAT_UNSUPPORTED";
		case -27: return "XR_ERROR_ACTION_TYPE_MISMATCH";
		case -28: return "XR_ERROR_SESSION_NOT_READY";
		case -29: return "XR_ERROR_SESSION_NOT_STOPPING";
		case -30: return "XR_ERROR_TIME_INVALID";
		case -31: return "XR_ERROR_REFERENCE_SPACE_UNSUPPORTED";
		case -32: return "XR_ERROR_FILE_ACCESS_ERROR";
		case -33: return "XR_ERROR_FILE_CONTENTS_INVALID";
		case -34: return "XR_ERROR_FORM_FACTOR_UNSUPPORTED";
		case -35: return "XR_ERROR_FORM_FACTOR_UNAVAILABLE";
		case -36: return "XR_ERROR_API_LAYER_NOT_PRESENT";
		case -37: return "XR_ERROR_CALL_ORDER_INVALID";
		case -38: return "XR_ERROR_GRAPHICS_DEVICE_INVALID";
		case -39: return "XR_ERROR_POSE_INVALID";
		case -40: return "XR_ERROR_INDEX_OUT_OF_RANGE";
		case -41: return "XR_ERROR_VIEW_CONFIGURATION_TYPE_UNSUPPORTED";
		case -42: return "XR_ERROR_ENVIRONMENT_BLEND_MODE_UNSUPPORTED";
		case -44: return "XR_ERROR_NAME_DUPLICATED";
		case -45: return "XR_ERROR_NAME_INVALID";
		case -46: return "XR_ERROR_ACTIONSET_NOT_ATTACHED";
		case -47: return "XR_ERROR_ACTIONSETS_ALREADY_ATTACHED";
		case -48: return "XR_ERROR_LOCALIZED_NAME_DUPLICATED";
		case -49: return "XR_ERROR_LOCALIZED_NAME_INVALID";
		case -50: return "XR_ERROR_GRAPHICS_REQUIREMENTS_CALL_MISSING";
		case -51: return "XR_ERROR_RUNTIME_UNAVAILABLE";
		case -1000710001: return "XR_ERROR_EXTENSION_DEPENDENCY_NOT_ENABLED";
		case -1000710000: return "XR_ERROR_PERMISSION_INSUFFICIENT";
		case -1000003000: return "XR_ERROR_ANDROID_THREAD_SETTINGS_ID_INVALID_KHR";
		case -1000003001: return "XR_ERROR_ANDROID_THREAD_SETTINGS_FAILURE_KHR";
		case -1000039001: return "XR_ERROR_CREATE_SPATIAL_ANCHOR_FAILED_MSFT";
		case -1000053000: return "XR_ERROR_SECONDARY_VIEW_CONFIGURATION_TYPE_NOT_ENABLED_MSFT";
		case -1000055000: return "XR_ERROR_CONTROLLER_MODEL_KEY_INVALID_MSFT";
		case -1000066000: return "XR_ERROR_REPROJECTION_MODE_UNSUPPORTED_MSFT";
		case -1000097000: return "XR_ERROR_COMPUTE_NEW_SCENE_NOT_COMPLETED_MSFT";
		case -1000097001: return "XR_ERROR_SCENE_COMPONENT_ID_INVALID_MSFT";
		case -1000097002: return "XR_ERROR_SCENE_COMPONENT_TYPE_MISMATCH_MSFT";
		case -1000097003: return "XR_ERROR_SCENE_MESH_BUFFER_ID_INVALID_MSFT";
		case -1000097004: return "XR_ERROR_SCENE_COMPUTE_FEATURE_INCOMPATIBLE_MSFT";
		case -1000097005: return "XR_ERROR_SCENE_COMPUTE_CONSISTENCY_MISMATCH_MSFT";
		case -1000101000: return "XR_ERROR_DISPLAY_REFRESH_RATE_UNSUPPORTED_FB";
		case -1000108000: return "XR_ERROR_COLOR_SPACE_UNSUPPORTED_FB";
		case -1000113000: return "XR_ERROR_SPACE_COMPONENT_NOT_SUPPORTED_FB";
		case -1000113001: return "XR_ERROR_SPACE_COMPONENT_NOT_ENABLED_FB";
		case -1000113002: return "XR_ERROR_SPACE_COMPONENT_STATUS_PENDING_FB";
		case -1000113003: return "XR_ERROR_SPACE_COMPONENT_STATUS_ALREADY_SET_FB";
		case -1000118000: return "XR_ERROR_UNEXPECTED_STATE_PASSTHROUGH_FB";
		case -1000118001: return "XR_ERROR_FEATURE_ALREADY_CREATED_PASSTHROUGH_FB";
		case -1000118002: return "XR_ERROR_FEATURE_REQUIRED_PASSTHROUGH_FB";
		case -1000118003: return "XR_ERROR_NOT_PERMITTED_PASSTHROUGH_FB";
		case -1000118004: return "XR_ERROR_INSUFFICIENT_RESOURCES_PASSTHROUGH_FB";
		case -1000118050: return "XR_ERROR_UNKNOWN_PASSTHROUGH_FB";
		case -1000119000: return "XR_ERROR_RENDER_MODEL_KEY_INVALID_FB";
		case 1000119020: return "XR_RENDER_MODEL_UNAVAILABLE_FB";
		case -1000124000: return "XR_ERROR_MARKER_NOT_TRACKED_VARJO";
		case -1000124001: return "XR_ERROR_MARKER_ID_INVALID_VARJO";
		case -1000138000: return "XR_ERROR_MARKER_DETECTOR_PERMISSION_DENIED_ML";
		case -1000138001: return "XR_ERROR_MARKER_DETECTOR_LOCATE_FAILED_ML";
		case -1000138002: return "XR_ERROR_MARKER_DETECTOR_INVALID_DATA_QUERY_ML";
		case -1000138003: return "XR_ERROR_MARKER_DETECTOR_INVALID_CREATE_INFO_ML";
		case -1000138004: return "XR_ERROR_MARKER_INVALID_ML";
		case -1000139000: return "XR_ERROR_LOCALIZATION_MAP_INCOMPATIBLE_ML";
		case -1000139001: return "XR_ERROR_LOCALIZATION_MAP_UNAVAILABLE_ML";
		case -1000139002: return "XR_ERROR_LOCALIZATION_MAP_FAIL_ML";
		case -1000139003: return "XR_ERROR_LOCALIZATION_MAP_IMPORT_EXPORT_PERMISSION_DENIED_ML";
		case -1000139004: return "XR_ERROR_LOCALIZATION_MAP_PERMISSION_DENIED_ML";
		case -1000139005: return "XR_ERROR_LOCALIZATION_MAP_ALREADY_EXISTS_ML";
		case -1000139006: return "XR_ERROR_LOCALIZATION_MAP_CANNOT_EXPORT_CLOUD_MAP_ML";
		case -1000140000: return "XR_ERROR_SPATIAL_ANCHORS_PERMISSION_DENIED_ML";
		case -1000140001: return "XR_ERROR_SPATIAL_ANCHORS_NOT_LOCALIZED_ML";
		case -1000140002: return "XR_ERROR_SPATIAL_ANCHORS_OUT_OF_MAP_BOUNDS_ML";
		case -1000140003: return "XR_ERROR_SPATIAL_ANCHORS_SPACE_NOT_LOCATABLE_ML";
		case -1000141000: return "XR_ERROR_SPATIAL_ANCHORS_ANCHOR_NOT_FOUND_ML";
		case -1000142001: return "XR_ERROR_SPATIAL_ANCHOR_NAME_NOT_FOUND_MSFT";
		case -1000142002: return "XR_ERROR_SPATIAL_ANCHOR_NAME_INVALID_MSFT";
		case 1000147000: return "XR_SCENE_MARKER_DATA_NOT_STRING_MSFT";
		case -1000169000: return "XR_ERROR_SPACE_MAPPING_INSUFFICIENT_FB";
		case -1000169001: return "XR_ERROR_SPACE_LOCALIZATION_FAILED_FB";
		case -1000169002: return "XR_ERROR_SPACE_NETWORK_TIMEOUT_FB";
		case -1000169003: return "XR_ERROR_SPACE_NETWORK_REQUEST_FAILED_FB";
		case -1000169004: return "XR_ERROR_SPACE_CLOUD_STORAGE_DISABLED_FB";
		case -1000266000: return "XR_ERROR_PASSTHROUGH_COLOR_LUT_BUFFER_SIZE_MISMATCH_META";
		case 1000291000: return "XR_ENVIRONMENT_DEPTH_NOT_AVAILABLE_META";
		case -1000306000: return "XR_ERROR_HINT_ALREADY_SET_QCOM";
		case -1000319000: return "XR_ERROR_NOT_AN_ANCHOR_HTC";
		case -1000429000: return "XR_ERROR_SPACE_NOT_LOCATABLE_EXT";
		case -1000429001: return "XR_ERROR_PLANE_DETECTION_PERMISSION_DENIED_EXT";
		case -1000469001: return "XR_ERROR_FUTURE_PENDING_EXT";
		case -1000469002: return "XR_ERROR_FUTURE_INVALID_EXT";
		case -1000473000: return "XR_ERROR_SYSTEM_NOTIFICATION_PERMISSION_DENIED_ML";
		case -1000473001: return "XR_ERROR_SYSTEM_NOTIFICATION_INCOMPATIBLE_SKU_ML";
		case -1000474000: return "XR_ERROR_WORLD_MESH_DETECTOR_PERMISSION_DENIED_ML";
		case -1000474001: return "XR_ERROR_WORLD_MESH_DETECTOR_SPACE_NOT_LOCATABLE_ML";
		case 1000482000: return "XR_ERROR_FACIAL_EXPRESSION_PERMISSION_DENIED_ML";
		case -1000571001: return "XR_ERROR_COLOCATION_DISCOVERY_NETWORK_FAILED_META";
		case -1000571002: return "XR_ERROR_COLOCATION_DISCOVERY_NO_DISCOVERY_METHOD_META";
		case 1000571003: return "XR_COLOCATION_DISCOVERY_ALREADY_ADVERTISING_META";
		case 1000571004: return "XR_COLOCATION_DISCOVERY_ALREADY_DISCOVERING_META";
		case -1000572002: return "XR_ERROR_SPACE_GROUP_NOT_FOUND_META";
		default: break;
	}
	return "<UNKNOWN_ERROR>";
}

} // namespace fl

FLOOR_PUSH_WARNINGS()
FLOOR_IGNORE_WARNING(unused-macros)

#define XR_CALL_RET(call, error_msg, ...) do { \
	const auto call_err_var = call; \
	if (call_err_var != 0 /* XR_SUCCESS */) { \
		log_error("$: $: $", error_msg, call_err_var, xr_error_to_string(call_err_var)); \
		return __VA_ARGS__; \
	} \
} while(false)
#define XR_CALL_CONT(call, error_msg) { \
	const int32_t call_err_var = call; \
	if (call_err_var != 0 /* XR_SUCCESS */) { \
		log_error("$: $: $", error_msg, call_err_var, xr_error_to_string(call_err_var)); \
		continue; \
	} \
} do {} while(false)
#define XR_CALL_BREAK(call, error_msg) { \
	const int32_t call_err_var = call; \
	if (call_err_var != 0 /* XR_SUCCESS */) { \
		log_error("$: $: $", error_msg, call_err_var, xr_error_to_string(call_err_var)); \
		break; \
	} \
} do {} while(false)
#define XR_CALL_ERR_EXEC(call, error_msg, do_stuff) do { \
	const auto call_err_var = call; \
	if (call_err_var != 0 /* XR_SUCCESS */) { \
		log_error("$: $: $", error_msg, call_err_var, xr_error_to_string(call_err_var)); \
		do_stuff \
	} \
} while(false)
#define XR_CALL_IGNORE(call, error_msg) do { \
	const auto call_err_var = call; \
	if (call_err_var != 0 /* XR_SUCCESS */) { \
		log_error("$: $: $", error_msg, call_err_var, xr_error_to_string(call_err_var)); \
	} \
} while(false)

FLOOR_POP_WARNINGS()

#endif
