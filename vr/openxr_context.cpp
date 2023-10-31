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

#include <floor/vr/openxr_context.hpp>

#if !defined(FLOOR_NO_OPENXR) && !defined(FLOOR_NO_VULKAN)
#include <floor/floor/floor.hpp>
#include <floor/floor/floor_version.hpp>
#include <floor/core/logger.hpp>
#include <floor/core/core.hpp>
#include <floor/compute/compute_queue.hpp>

#include <floor/compute/vulkan/vulkan_compute.hpp>
#include <floor/compute/vulkan/vulkan_queue.hpp>
#include <floor/compute/vulkan/vulkan_image.hpp>
#include <floor/compute/vulkan/vulkan_device.hpp>

#if defined(FLOOR_DEBUG)
static XRAPI_ATTR XrBool32 XRAPI_CALL openxr_debug_callback(XrDebugUtilsMessageSeverityFlagsEXT severity,
															XrDebugUtilsMessageTypeFlagsEXT message_types floor_unused,
															const XrDebugUtilsMessengerCallbackDataEXT* cb_data,
															/* openxr_context* */ void* ctx) {
	[[maybe_unused]] const auto xr_ctx = (const openxr_context*)ctx;

	string debug_message;
	if (cb_data != nullptr) {
		debug_message += "\n\t";
		debug_message += cb_data->messageId;
		if (cb_data->functionName) {
			debug_message += " in " + string(cb_data->functionName);
		}
		debug_message += "\n";

		if (cb_data->message) {
			auto tokens = core::tokenize(cb_data->message, '|');
			for (auto& token : tokens) {
				token = core::trim(token);
				debug_message += "\t" + token + "\n";
			}
		}

		if (cb_data->objectCount > 0 && cb_data->objects != nullptr) {
			debug_message += "\tobjects:\n";
			for (uint32_t oidx = 0; oidx < cb_data->objectCount; ++oidx) {
				debug_message += "\t\t";
				debug_message += (cb_data->objects[oidx].objectName ? cb_data->objects[oidx].objectName : "<no-object-name>");
				debug_message += " ("s + vulkan_object_type_to_string(cb_data->objects[oidx].objectType) + ", ";
				debug_message += to_string(cb_data->objects[oidx].objectHandle) + ")\n";
			}
		}
	} else {
		debug_message = " <callback-data-is-nullptr>";
	}

	if ((severity & XR_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) > 0) {
		log_error("OpenXR error:$", debug_message);
	} else if ((severity & XR_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) > 0) {
		log_warn("OpenXR warning:$", debug_message);
	} else if ((severity & XR_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT) > 0) {
		log_msg("OpenXR info:$", debug_message);
	} else if ((severity & XR_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT) > 0) {
		log_debug("OpenXR verbose:$", debug_message);
	} else {
		assert(false && "unknown severity");
	}
	logger::flush();
	return XR_FALSE; // don't abort
}
#endif

openxr_context::openxr_context() : vr_context() {
	backend = VR_BACKEND::OPENXR;

	// query supported extensions and layers
	const auto query_extensions = [](const char* layer_name) -> unordered_map<string, XrExtensionProperties> {
		uint32_t ext_count = 0;
		XR_CALL_RET(xrEnumerateInstanceExtensionProperties(layer_name, 0, &ext_count, nullptr),
					"failed to query extension count" + (layer_name ? " for "s + layer_name : ""), {});
		if (ext_count == 0) {
			return {};
		}
		vector<XrExtensionProperties> extensions_vec(ext_count, { .type = XR_TYPE_EXTENSION_PROPERTIES, .next = nullptr });
		XR_CALL_RET(xrEnumerateInstanceExtensionProperties(layer_name, ext_count, &ext_count, extensions_vec.data()),
					"failed to query extensions" + (layer_name ? " for "s + layer_name : ""), {});
		unordered_map<string, XrExtensionProperties> extensions;
		extensions.reserve(extensions_vec.size());
		for (auto& ext : extensions_vec) {
			extensions.emplace(ext.extensionName, ext);
		}
		return extensions;
	};
	const auto extensions_to_string = [](const auto& extensions) {
		string ret;
		for (const auto& ext : extensions) {
			ret += ext.first + " "s;
		}
		return ret;
	};

	auto supported_extensions = query_extensions(nullptr);
	log_msg("OpenXR system extensions: $", extensions_to_string(supported_extensions));

	const auto supports_extension = [&supported_extensions](const string extension, const uint32_t version = 0) {
		if (version == 0) {
			return supported_extensions.contains(extension);
		}
		const auto ext_iter = supported_extensions.find(extension);
		return (ext_iter != supported_extensions.end() && ext_iter->second.extensionVersion >= version);
	};

	uint32_t layer_count = 0;
	XR_CALL_RET(xrEnumerateApiLayerProperties(0, &layer_count, nullptr),
				"failed to query layer count");
	vector<XrApiLayerProperties> layers;
	if (layer_count > 0) {
		layers.resize(layer_count, { .type = XR_TYPE_API_LAYER_PROPERTIES, .next = nullptr });
		XR_CALL_RET(xrEnumerateApiLayerProperties(layer_count, &layer_count, layers.data()), "failed to query layers");
	}

	for (size_t layer_idx = 0; layer_idx < layer_count; ++layer_idx) {
		const auto& layer = layers[layer_idx];
		auto layer_extensions = query_extensions(layer.layerName);
		string ext_str;
		if (!layer_extensions.empty()) {
			ext_str = ": ";
			ext_str += extensions_to_string(layer_extensions);
			supported_extensions.merge(layer_extensions);
		}
		log_msg("OpenXR layer: $ (v$, spec $.$.$, $)$", layer.layerName,
				layer.layerVersion,
				XR_VERSION_MAJOR(layer.specVersion),
				XR_VERSION_MINOR(layer.specVersion),
				XR_VERSION_PATCH(layer.specVersion),
				layer.description, ext_str);
	}
	const auto supports_layer = [&layers](const string_view layer_name) {
		return (std::find_if(layers.begin(), layers.end(), [&layer_name](const XrApiLayerProperties& layer) {
					return (layer.layerName == layer_name);
				}) != layers.end());
	};
	vector<const char*> instance_layers;
	if (supports_layer("XR_APILAYER_ULTRALEAP_hand_tracking")) {
		instance_layers.emplace_back("XR_APILAYER_ULTRALEAP_hand_tracking");
	}

	// check/add required extensions
	vector<const char*> instance_extensions;
	if (!supports_extension(XR_KHR_VULKAN_ENABLE2_EXTENSION_NAME)) {
		log_error("OpenXR: $ extension is required", XR_KHR_VULKAN_ENABLE2_EXTENSION_NAME);
		return;
	}
	instance_extensions.emplace_back(XR_KHR_VULKAN_ENABLE2_EXTENSION_NAME);

	// add optional extensions
#if defined(FLOOR_DEBUG)
	bool enable_debug_utils_callback = false;
	if (supports_extension(XR_EXT_DEBUG_UTILS_EXTENSION_NAME)) {
		instance_extensions.emplace_back(XR_EXT_DEBUG_UTILS_EXTENSION_NAME);
		enable_debug_utils_callback = true;

		// only enable validation if debug utils is also supported
		if (supports_layer("XR_APILAYER_LUNARG_core_validation")) {
			instance_layers.emplace_back("XR_APILAYER_LUNARG_core_validation");
		}

		// we don't want any console/file output from the validation layer, we have our own callback/logger
		SDL_setenv("XR_CORE_VALIDATION_EXPORT_TYPE", "none", 1);
	}
#endif
	if (floor::get_vr_hand_tracking() && supports_extension(XR_EXT_HAND_TRACKING_EXTENSION_NAME)) {
		instance_extensions.emplace_back(XR_EXT_HAND_TRACKING_EXTENSION_NAME);
		has_hand_tracking_support = true;

		// only enable these if the above is supported
		if (supports_extension(XR_EXT_HAND_JOINTS_MOTION_RANGE_EXTENSION_NAME)) {
			instance_extensions.emplace_back(XR_EXT_HAND_JOINTS_MOTION_RANGE_EXTENSION_NAME);
		}
		if (supports_extension(XR_ULTRALEAP_HAND_TRACKING_FOREARM_EXTENSION_NAME)) {
			instance_extensions.emplace_back(XR_ULTRALEAP_HAND_TRACKING_FOREARM_EXTENSION_NAME);
			has_hand_tracking_forearm_support = true;
		}
	}
	if (supports_extension(XR_EXT_PALM_POSE_EXTENSION_NAME)) {
		instance_extensions.emplace_back(XR_EXT_PALM_POSE_EXTENSION_NAME);
	}
	if (supports_extension(XR_FB_DISPLAY_REFRESH_RATE_EXTENSION_NAME)) {
		instance_extensions.emplace_back(XR_FB_DISPLAY_REFRESH_RATE_EXTENSION_NAME);
		can_query_display_refresh_rate = true;
	}
	if (floor::get_vr_trackers() && supports_extension(XR_HTCX_VIVE_TRACKER_INTERACTION_EXTENSION_NAME)) {
		instance_extensions.emplace_back(XR_HTCX_VIVE_TRACKER_INTERACTION_EXTENSION_NAME);
		has_tracker_interaction_support = true;
		// version 3 / OpenXR 1.0.29 added wrist and ankle role support
		if (supports_extension(XR_HTCX_VIVE_TRACKER_INTERACTION_EXTENSION_NAME, 3)) {
			has_tracker_interaction_with_wrist_ankle_support = true;
		}
	}
#if defined(__WINDOWS__)
	if (supports_extension("XR_KHR_win32_convert_performance_counter_time")) {
		instance_extensions.emplace_back("XR_KHR_win32_convert_performance_counter_time");
	}
#endif
#if defined(__linux__)
	if (supports_extension(XR_KHR_CONVERT_TIMESPEC_TIME_EXTENSION_NAME)) {
		instance_extensions.emplace_back(XR_KHR_CONVERT_TIMESPEC_TIME_EXTENSION_NAME);
	}
#endif

	// TODO: use XR_KHR_visibility_mask

	// controllers
	if (supports_extension(XR_EXT_HP_MIXED_REALITY_CONTROLLER_EXTENSION_NAME)) {
		instance_extensions.emplace_back(XR_EXT_HP_MIXED_REALITY_CONTROLLER_EXTENSION_NAME);
		has_hp_mixed_reality_controller_support = true;
	}
	if (supports_extension(XR_HTC_VIVE_COSMOS_CONTROLLER_INTERACTION_EXTENSION_NAME)) {
		instance_extensions.emplace_back(XR_HTC_VIVE_COSMOS_CONTROLLER_INTERACTION_EXTENSION_NAME);
		has_htc_vive_cosmos_controller_support = true;
	}
	if (supports_extension(XR_HTC_VIVE_FOCUS3_CONTROLLER_INTERACTION_EXTENSION_NAME)) {
		instance_extensions.emplace_back(XR_HTC_VIVE_FOCUS3_CONTROLLER_INTERACTION_EXTENSION_NAME);
		has_htc_vive_focus3_controller_support = true;
	}
	if (supports_extension(XR_HUAWEI_CONTROLLER_INTERACTION_EXTENSION_NAME)) {
		instance_extensions.emplace_back(XR_HUAWEI_CONTROLLER_INTERACTION_EXTENSION_NAME);
		has_huawei_controller_support = true;
	}
	if (supports_extension(XR_EXT_SAMSUNG_ODYSSEY_CONTROLLER_EXTENSION_NAME)) {
		instance_extensions.emplace_back(XR_EXT_SAMSUNG_ODYSSEY_CONTROLLER_EXTENSION_NAME);
		has_samsung_odyssey_controller_support = true;
	}
	if (supports_extension(XR_ML_ML2_CONTROLLER_INTERACTION_EXTENSION_NAME)) {
		instance_extensions.emplace_back(XR_ML_ML2_CONTROLLER_INTERACTION_EXTENSION_NAME);
		has_ml2_controller_support = true;
	}
	if (supports_extension(XR_FB_TOUCH_CONTROLLER_PRO_EXTENSION_NAME)) {
		instance_extensions.emplace_back(XR_FB_TOUCH_CONTROLLER_PRO_EXTENSION_NAME);
		has_fb_touch_controller_pro_support = true;
	}
	if (supports_extension(XR_FB_TOUCH_CONTROLLER_PROXIMITY_EXTENSION_NAME)) {
		instance_extensions.emplace_back(XR_FB_TOUCH_CONTROLLER_PROXIMITY_EXTENSION_NAME);
	}
	if (supports_extension(XR_BD_CONTROLLER_INTERACTION_EXTENSION_NAME)) {
		instance_extensions.emplace_back(XR_BD_CONTROLLER_INTERACTION_EXTENSION_NAME);
		has_bd_controller_support = true;
	}

	// init VR
	string used_layers, used_extensions;
	for (const auto& layer : instance_layers) {
		used_layers += layer + " "s;
	}
	for (const auto& ext : instance_extensions) {
		used_extensions += ext + " "s;
	}
	if (!used_layers.empty()) {
		log_msg("OpenXR: using layers: $", used_layers);
	}
	if (!used_extensions.empty()) {
		log_msg("OpenXR: using extensions: $", used_extensions);
	}
	XrInstanceCreateInfo instance_create_info {
		.type = XR_TYPE_INSTANCE_CREATE_INFO,
		.next = nullptr,
		.createFlags = 0,
		.applicationInfo = {
			.applicationName = "", // set later
			.applicationVersion = floor::get_app_version(),
			.engineName = "libfloor",
			.engineVersion = FLOOR_VERSION_U32,
			.apiVersion = XR_MAKE_VERSION(1, 0, 27),
		},
		.enabledApiLayerCount = uint32_t(instance_layers.size()),
		.enabledApiLayerNames = (!instance_layers.empty() ? instance_layers.data() : nullptr),
		.enabledExtensionCount = uint32_t(instance_extensions.size()),
		.enabledExtensionNames = (!instance_extensions.empty() ? instance_extensions.data() : nullptr),
	};
	const auto app_name_len = min(floor::get_app_name().size(), size_t(XR_MAX_APPLICATION_NAME_SIZE - 1));
	memcpy(&instance_create_info.applicationInfo.applicationName[0], floor::get_app_name().c_str(), app_name_len);
	instance_create_info.applicationInfo.applicationName[app_name_len] = '\0';
	XR_CALL_RET(xrCreateInstance(&instance_create_info, &instance), "failed to create OpenXR instance");

	XrInstanceProperties instance_props { .type = XR_TYPE_INSTANCE_PROPERTIES };
	XR_CALL_RET(xrGetInstanceProperties(instance, &instance_props), "failed to retrieve OpenXR instance properties");
	log_msg("OpenXR: runtime $, version $.$.$", instance_props.runtimeName,
			XR_VERSION_MAJOR(instance_props.runtimeVersion),
			XR_VERSION_MINOR(instance_props.runtimeVersion),
			XR_VERSION_PATCH(instance_props.runtimeVersion));

#if defined(FLOOR_DEBUG)
	// register debug callback
	if (enable_debug_utils_callback) {
		XR_CALL_RET(xrGetInstanceProcAddr(instance, "xrCreateDebugUtilsMessengerEXT",
										  reinterpret_cast<PFN_xrVoidFunction*>(&create_debug_utils_messenger)),
					"failed to query xrCreateDebugUtilsMessengerEXT function pointer");
		XR_CALL_RET(xrGetInstanceProcAddr(instance, "xrDestroyDebugUtilsMessengerEXT",
										  reinterpret_cast<PFN_xrVoidFunction*>(&destroy_debug_utils_messenger)),
					"failed to query xrDestroyDebugUtilsMessengerEXT function pointer");

		const XrDebugUtilsMessengerCreateInfoEXT create_info {
			.type = XR_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
			.next = nullptr,
			.messageSeverities = (XR_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT |
								  XR_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT),
			.messageTypes = (XR_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
							 XR_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
							 XR_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT |
							 XR_DEBUG_UTILS_MESSAGE_TYPE_CONFORMANCE_BIT_EXT),
			.userCallback = &openxr_debug_callback,
			.userData = this,
		};
		XR_CALL_RET(create_debug_utils_messenger(instance, &create_info, &debug_utils_messenger),
					"failed to create debug utils messenger");
	}
#endif

	// only PimaxXR has a known good Vulkan implementation
	if (string_view(instance_props.runtimeName).find("PimaxXR") != string::npos) {
		is_known_good_vulkan_backend = true;
	}

	XrSystemGetInfo sys_get_info {
		.type = XR_TYPE_SYSTEM_GET_INFO,
		.next = nullptr,
		.formFactor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY,
	};
	XR_CALL_RET(xrGetSystem(instance, &sys_get_info, &system_id), "failed to retrieve OpenXR system info");

	XrSystemHandTrackingPropertiesEXT hand_tracking_props {
		.type = XR_TYPE_SYSTEM_HAND_TRACKING_PROPERTIES_EXT,
		.next = nullptr,
		.supportsHandTracking = false,
	};
	XrSystemProperties system_props {
		.type = XR_TYPE_SYSTEM_PROPERTIES,
		.next = (has_hand_tracking_support ? &hand_tracking_props : nullptr),
	};
	XR_CALL_RET(xrGetSystemProperties(instance, system_id, &system_props),
				"failed to retrieve OpenXR system properties");
	log_msg("OpenXR: system: $, vendor: $X", system_props.systemName, system_props.vendorId);
	hmd_name = system_props.systemName;
	vendor_name = instance_props.runtimeName;
	log_msg("OpenXR: tracking: orientation $, position $",
			system_props.trackingProperties.orientationTracking,
			system_props.trackingProperties.positionTracking);
	log_msg("OpenXR: hand-tracking: $", hand_tracking_props.supportsHandTracking);
	has_hand_tracking_support = hand_tracking_props.supportsHandTracking;

	const uint2 max_swapchain_dim {
		system_props.graphicsProperties.maxSwapchainImageWidth,
		system_props.graphicsProperties.maxSwapchainImageHeight
	};
	swapchain_layer_count = system_props.graphicsProperties.maxLayerCount;
	log_msg("OpenXR: max swapchain dim: $, max layers: $", max_swapchain_dim, swapchain_layer_count);

	if (can_query_display_refresh_rate) {
		XR_CALL_IGNORE(xrGetInstanceProcAddr(instance, "xrGetDisplayRefreshRateFB",
											 reinterpret_cast<PFN_xrVoidFunction*>(&GetDisplayRefreshRateFB)),
					   "failed to query xrGetDisplayRefreshRateFB function pointer");
		if (!GetDisplayRefreshRateFB) {
			can_query_display_refresh_rate = false;
		}
	}

	if (has_tracker_interaction_support) {
		XR_CALL_IGNORE(xrGetInstanceProcAddr(instance, "xrEnumerateViveTrackerPathsHTCX",
											 reinterpret_cast<PFN_xrVoidFunction*>(&EnumerateViveTrackerPaths)),
					   "failed to query xrEnumerateViveTrackerPathsHTCX function pointer");
		if (!EnumerateViveTrackerPaths) {
			has_tracker_interaction_support = false;
		} else {
			log_msg("OpenXR: has tracker support");
			tracker_role_paths.fill(0u);
			tracker_pose_actions.fill(nullptr);
			tracker_spaces.fill(nullptr);
		}
	}

	uint32_t view_config_type_count = 0;
	XR_CALL_RET(xrEnumerateViewConfigurations(instance, system_id, 0, &view_config_type_count, nullptr),
				"failed to retrieve view configuration count");
	vector<XrViewConfigurationType> view_config_types(view_config_type_count);
	XR_CALL_RET(xrEnumerateViewConfigurations(instance, system_id, view_config_type_count, &view_config_type_count,
											  view_config_types.data()),
				"failed to retrieve view configurations");
	bool has_stereo = false;
	for (const auto& view_config_type : view_config_types) {
		if (view_config_type == XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO) {
			has_stereo = true;
			break;
		}
	}
	if (!has_stereo) {
		log_error("OpenXR: no stereo view configuration is supported");
		return;
	}

	XrViewConfigurationProperties view_config_props {
		.type = XR_TYPE_VIEW_CONFIGURATION_PROPERTIES,
		.next = nullptr,
	};
	XR_CALL_RET(xrGetViewConfigurationProperties(instance, system_id, XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO,
												 &view_config_props),
				"failed to retrieve view configuration properties");
	mutable_fov = view_config_props.fovMutable;

	uint32_t view_config_views_count = 0;
	XR_CALL_RET(xrEnumerateViewConfigurationViews(instance, system_id, XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO, 0,
												  &view_config_views_count, nullptr),
				"failed to retrieve view configuration views count");
	view_configs.resize(view_config_views_count, { .type = XR_TYPE_VIEW_CONFIGURATION_VIEW, .next = nullptr });
	XR_CALL_RET(xrEnumerateViewConfigurationViews(instance, system_id, XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO,
												  view_config_views_count, &view_config_views_count, view_configs.data()),
				"failed to retrieve view configuration views");
	for (const auto& view_config : view_configs) {
		log_msg("OpenXR view: rect $, max rect $, sample count $ / max $",
				uint2 { view_config.recommendedImageRectWidth, view_config.recommendedImageRectHeight },
				uint2 { view_config.maxImageRectWidth, view_config.maxImageRectHeight },
				view_config.recommendedSwapchainSampleCount, view_config.maxSwapchainSampleCount);
	}
	if (view_configs.size() != 2) {
		log_error("OpenXR: expected 2 views");
		return;
	}
	recommended_render_size = {
		view_configs[0].recommendedImageRectWidth,
		view_configs[0].recommendedImageRectHeight,
	};
	if ((recommended_render_size > max_swapchain_dim).any()) {
		log_error("recommended render size $ > max possible swapchain dim $",
				  recommended_render_size, max_swapchain_dim);
		return;
	}
	views.resize(view_configs.size(), { .type = XR_TYPE_VIEW, .next = nullptr });
	view_states.resize(view_configs.size());

	// query extension function pointers
	XR_CALL_RET(xrGetInstanceProcAddr(instance, "xrCreateVulkanInstanceKHR",
									  reinterpret_cast<PFN_xrVoidFunction*>(&CreateVulkanInstanceKHR)),
				"failed to query xrCreateVulkanInstanceKHR function pointer");
	XR_CALL_RET(xrGetInstanceProcAddr(instance, "xrCreateVulkanDeviceKHR",
									  reinterpret_cast<PFN_xrVoidFunction*>(&CreateVulkanDeviceKHR)),
				"failed to query xrCreateVulkanDeviceKHR function pointer");
	XR_CALL_RET(xrGetInstanceProcAddr(instance, "xrGetVulkanGraphicsDevice2KHR",
									  reinterpret_cast<PFN_xrVoidFunction*>(&GetVulkanGraphicsDevice2KHR)),
				"failed to query xrGetVulkanGraphicsDevice2KHR function pointer");
	XR_CALL_RET(xrGetInstanceProcAddr(instance, "xrGetVulkanGraphicsRequirements2KHR",
									  reinterpret_cast<PFN_xrVoidFunction*>(&GetVulkanGraphicsRequirements2KHR)),
				"failed to query xrGetVulkanGraphicsRequirements2KHR function pointer");

#if defined(__WINDOWS__)
	// we need to be able to convert the Windows perf counter value into SDL ticks
	// -> query current values from SDL and then solve for the start time/perf-counter
	const auto sdl_ticks = SDL_GetTicks64();
	const auto sdl_perf_counter = SDL_GetPerformanceCounter();
	win_perf_counter_freq = SDL_GetPerformanceFrequency();
	win_start_perf_counter = sdl_perf_counter - (sdl_ticks * win_perf_counter_freq) / 1000ull;

	XR_CALL_RET(xrGetInstanceProcAddr(instance, "xrConvertWin32PerformanceCounterToTimeKHR",
									  reinterpret_cast<PFN_xrVoidFunction*>(&ConvertWin32PerformanceCounterToTimeKHR)),
				"failed to query xrConvertWin32PerformanceCounterToTimeKHR function pointer");
	XR_CALL_RET(xrGetInstanceProcAddr(instance, "xrConvertTimeToWin32PerformanceCounterKHR",
									  reinterpret_cast<PFN_xrVoidFunction*>(&ConvertTimeToWin32PerformanceCounterKHR)),
				"failed to query xrConvertTimeToWin32PerformanceCounterKHR function pointer");
#endif

#if defined(__linux__)
	// we need to be able to convert the timespec into SDL ticks
	// -> query current values from SDL and then solve for the start time
	// NOTE: this is set by SDL (supported by glibc and musl) -> must have support for this
	static_assert(HAVE_CLOCK_GETTIME, "must have clock_gettime support");
	const auto sdl_ticks = SDL_GetTicks64(); // -> in ms
	const auto sdl_perf_counter = SDL_GetPerformanceCounter(); // -> in ns
	assert(SDL_GetPerformanceFrequency() == unix_perf_counter_freq);
	unix_start_time = sdl_perf_counter - sdl_ticks * 1000ull; // -> in ns

	XR_CALL_RET(xrGetInstanceProcAddr(instance, "xrConvertTimespecTimeToTimeKHR",
									  reinterpret_cast<PFN_xrVoidFunction*>(&ConvertTimespecTimeToTimeKHR)),
				"failed to query xrConvertTimespecTimeToTimeKHR function pointer");
	XR_CALL_RET(xrGetInstanceProcAddr(instance, "xrConvertTimeToTimespecTimeKHR",
									  reinterpret_cast<PFN_xrVoidFunction*>(&ConvertTimeToTimespecTimeKHR)),
				"failed to query xrConvertTimeToTimespecTimeKHR function pointer");
#endif

	// all done
	valid = true;
}

openxr_context::~openxr_context() {
	if (swapchain.swapchain) {
		XR_CALL_IGNORE(xrDestroySwapchain(swapchain.swapchain), "failed to destroy OpenXR swapchain");
	}
	if (vk_dev) {
		for (auto& image_view : swapchain.swapchain_vk_image_views) {
			if (image_view) {
				vkDestroyImageView(vk_dev->device, image_view, nullptr);
			}
		}
	}
	if (!base_actions.empty()) {
		for (auto& base_action : base_actions) {
			XR_CALL_IGNORE(xrDestroyAction(base_action.second.action), "failed to destroy OpenXR action");
		}
	}
	for (uint32_t hand_idx = 0; hand_idx < 2; ++hand_idx) {
		if (hand_trackers[hand_idx]) {
			XR_CALL_IGNORE(DestroyHandTracker(hand_trackers[hand_idx]), "failed to destroy OpenXR hand tracker");
		}
		if (hand_pose_actions[hand_idx]) {
			XR_CALL_IGNORE(xrDestroyAction(hand_pose_actions[hand_idx]), "failed to destroy OpenXR hand pose action");
		}
		if (hand_spaces[hand_idx]) {
			XR_CALL_IGNORE(xrDestroySpace(hand_spaces[hand_idx]), "failed to destroy OpenXR hand space");
		}
		if (hand_aim_pose_actions[hand_idx]) {
			XR_CALL_IGNORE(xrDestroyAction(hand_aim_pose_actions[hand_idx]), "failed to destroy OpenXR hand aim pose action");
		}
		if (hand_aim_spaces[hand_idx]) {
			XR_CALL_IGNORE(xrDestroySpace(hand_aim_spaces[hand_idx]), "failed to destroy OpenXR hand aim space");
		}
	}
	for (uint32_t i = 0; i < tracker_role_count; ++i) {
		base_actions.erase(EVENT_TYPE(uint32_t(EVENT_TYPE::VR_INTERNAL_TRACKER_HANDHELD_OBJECT) + i));
		if (tracker_spaces[i]) {
			XR_CALL_IGNORE(xrDestroySpace(tracker_spaces[i]),
						   "failed to destroy tracker pose action space");
			tracker_spaces[i] = nullptr;
		}
		if (tracker_pose_actions[i]) {
			XR_CALL_IGNORE(xrDestroyAction(tracker_pose_actions[i]),
						   "failed to destroy tracker pose action");
			tracker_pose_actions[i] = nullptr;
		}
	}
	if (tracker_input_action_set) {
		XR_CALL_IGNORE(xrDestroyActionSet(tracker_input_action_set),
					   "failed to destroy tracker action set");
		tracker_input_action_set = nullptr;
	}
	if (input_action_set) {
		XR_CALL_IGNORE(xrDestroyActionSet(input_action_set), "failed to destroy OpenXR input action set");
	}
	if (view_space) {
		XR_CALL_IGNORE(xrDestroySpace(view_space), "failed to destroy OpenXR view space");
	}
	if (scene_space) {
		XR_CALL_IGNORE(xrDestroySpace(scene_space), "failed to destroy OpenXR scene space");
	}
	if (session) {
		XR_CALL_IGNORE(xrDestroySession(session), "failed to destroy OpenXR session");
	}
#if defined(FLOOR_DEBUG)
	if (debug_utils_messenger) {
		XR_CALL_IGNORE(destroy_debug_utils_messenger(debug_utils_messenger), "failed to destroy debug utils messenger");
	}
#endif
	if (instance) {
		XR_CALL_IGNORE(xrDestroyInstance(instance), "failed to destroy OpenXR instance");
	}
}

VkResult openxr_context::create_vulkan_instance(const VkInstanceCreateInfo& vk_create_info, VkInstance& vk_instance) {
	// check Vulkan requirements (need to do this, but don't care)
	XrGraphicsRequirementsVulkanKHR vk_reqs {
		.type = XR_TYPE_GRAPHICS_REQUIREMENTS_VULKAN_KHR,
		.next = nullptr,
	};
	XR_CALL_RET(GetVulkanGraphicsRequirements2KHR(instance, system_id, &vk_reqs),
				"failed to query Vulkan graphics requirements", VK_ERROR_INCOMPATIBLE_DRIVER);

	VkResult vk_result {};
	const XrVulkanInstanceCreateInfoKHR create_info {
		.type = XR_TYPE_VULKAN_INSTANCE_CREATE_INFO_KHR,
		.next = nullptr,
		.systemId = system_id,
		.createFlags = 0,
		.pfnGetInstanceProcAddr = &vkGetInstanceProcAddr,
		.vulkanCreateInfo = &vk_create_info,
		.vulkanAllocator = nullptr,
	};
	XR_CALL_RET(CreateVulkanInstanceKHR(instance, &create_info, &vk_instance, &vk_result),
				"failed to create Vulkan instance for OpenXR", vk_result);
	return VK_SUCCESS;
}

VkResult openxr_context::create_vulkan_device(const VkDeviceCreateInfo& vk_create_info, VkDevice& vk_dev_,
											  const VkPhysicalDevice& vk_phys_dev, VkInstance& vk_instance) {
	VkPhysicalDevice graphics_dev = nullptr;
	const XrVulkanGraphicsDeviceGetInfoKHR graphics_dev_info {
		.type = XR_TYPE_VULKAN_GRAPHICS_DEVICE_GET_INFO_KHR,
		.next = nullptr,
		.systemId = system_id,
		.vulkanInstance = vk_instance,
	};
	XR_CALL_RET(GetVulkanGraphicsDevice2KHR(instance, &graphics_dev_info, &graphics_dev),
				"failed to retrieve Vulkan physical device for OpenXR", VK_ERROR_INCOMPATIBLE_DRIVER);

	if (graphics_dev != vk_phys_dev) {
		log_error("Vulkan physical device requested by OpenXR does not match wanted Vulkan physical device");
		return VK_ERROR_INCOMPATIBLE_DRIVER;
	}

	VkResult vk_result {};
	const XrVulkanDeviceCreateInfoKHR create_info {
		.type = XR_TYPE_VULKAN_DEVICE_CREATE_INFO_KHR,
		.next = nullptr,
		.systemId = system_id,
		.createFlags = 0,
		.pfnGetInstanceProcAddr = &vkGetInstanceProcAddr,
		.vulkanPhysicalDevice = vk_phys_dev,
		.vulkanCreateInfo = &vk_create_info,
		.vulkanAllocator = nullptr,
	};
	XR_CALL_RET(CreateVulkanDeviceKHR(instance, &create_info, &vk_dev_, &vk_result),
				"failed to create Vulkan device for OpenXR", vk_result);
	return VK_SUCCESS;

}

bool openxr_context::create_session(vulkan_compute& vk_ctx_, const vulkan_device& vk_dev_, const vulkan_queue& vk_queue) {
	vk_ctx = &vk_ctx_;
	vk_dev = &vk_dev_;

	// create session
	const XrGraphicsBindingVulkan2KHR vk_binding {
		.type = XR_TYPE_GRAPHICS_BINDING_VULKAN2_KHR,
		.next = nullptr,
		.instance = vk_ctx->get_vulkan_context(),
		.physicalDevice = vk_dev->physical_device,
		.device = vk_dev->device,
		.queueFamilyIndex = vk_queue.get_family_index(),
		.queueIndex = vk_queue.get_queue_index(),
	};
	const XrSessionCreateInfo session_create_info {
		.type = XR_TYPE_SESSION_CREATE_INFO,
		.next = &vk_binding,
		.createFlags = 0,
		.systemId = system_id,
	};
	XR_CALL_RET(xrCreateSession(instance, &session_create_info, &session),
				"failed to create session", false);

	// query display refresh rate
	while (can_query_display_refresh_rate) {
		XR_CALL_BREAK(GetDisplayRefreshRateFB(session, &display_frequency),
					  "failed to get display refresh rate");
		log_msg("OpenXR: display refresh rate: $", display_frequency);
		break;
	}

	// space setup
	uint32_t space_count = 0;
	XR_CALL_RET(xrEnumerateReferenceSpaces(session, 0, &space_count, nullptr),
				"failed to query space count", false);
	spaces.resize(space_count, XR_REFERENCE_SPACE_TYPE_MAX_ENUM);
	XR_CALL_RET(xrEnumerateReferenceSpaces(session, space_count, &space_count, spaces.data()),
				"failed to query spaces", false);
	string space_names;
	bool has_stage_space = false, has_view_space = false;
	for (const auto& space : spaces) {
		switch (space) {
			case XR_REFERENCE_SPACE_TYPE_VIEW:
				space_names += "view";
				has_view_space = true;
				break;
			case XR_REFERENCE_SPACE_TYPE_LOCAL:
				space_names += "local";
				break;
			case XR_REFERENCE_SPACE_TYPE_STAGE:
				space_names += "stage";
				has_stage_space = true;
				break;
			case XR_REFERENCE_SPACE_TYPE_UNBOUNDED_MSFT:
				space_names += "unbounded";
				break;
			case XR_REFERENCE_SPACE_TYPE_COMBINED_EYE_VARJO:
				space_names += "combined-eye";
				break;
			case XR_REFERENCE_SPACE_TYPE_LOCAL_FLOOR_EXT:
				space_names += "local-floor";
				break;
			default:
				space_names += "<unknown: " + to_string(space) + ">";
				break;
		}
		space_names += " ";
	}
	log_msg("OpenXR: supported spaces: $", space_names);

	// require stage and view space for now
	if (!has_stage_space) {
		log_error("OpenXR: stage space is not supported");
		return false;
	}
	if (!has_view_space) {
		log_error("OpenXR: view space is not supported");
		return false;
	}
	const XrReferenceSpaceCreateInfo ref_space_create_info {
		.type = XR_TYPE_REFERENCE_SPACE_CREATE_INFO,
		.next = nullptr,
		.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_STAGE,
		.poseInReferenceSpace = {
			.orientation = { 0.0f, 0.0f, 0.0f, 1.0f },
			.position = { 0.0f, 0.0f, 0.0f },
		},
	};
	XR_CALL_RET(xrCreateReferenceSpace(session, &ref_space_create_info, &scene_space),
				"failed to create reference stage scene space", false);

	const XrReferenceSpaceCreateInfo ref_view_space_create_info {
		.type = XR_TYPE_REFERENCE_SPACE_CREATE_INFO,
		.next = nullptr,
		.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_VIEW,
		.poseInReferenceSpace = {
			.orientation = { 0.0f, 0.0f, 0.0f, 1.0f },
			.position = { 0.0f, 0.0f, 0.0f },
		},
	};
	XR_CALL_RET(xrCreateReferenceSpace(session, &ref_view_space_create_info, &view_space),
				"failed to create reference view space", false);

	// create swapchain
	uint32_t swapchain_format_count = 0;
	XR_CALL_RET(xrEnumerateSwapchainFormats(session, 0, &swapchain_format_count, nullptr),
				"failed to retrieve swapchain formats count", false);
	vector<int64_t> swapchain_formats(swapchain_format_count);
	XR_CALL_RET(xrEnumerateSwapchainFormats(session, swapchain_format_count, &swapchain_format_count, swapchain_formats.data()),
				"failed to retrieve swapchain formats count", false);
	VkFormat swapchain_format = VK_FORMAT_UNDEFINED;
	if (floor::get_wide_gamut()) {
		for (const auto& format : swapchain_formats) {
			if (format == VK_FORMAT_R16G16B16A16_SFLOAT) {
				swapchain_format = VK_FORMAT_R16G16B16A16_SFLOAT;
				break;
			}
		}
		if (swapchain_format == VK_FORMAT_UNDEFINED) {
			log_error("failed to find a wide gamut swapchain format - falling back to an 8-bit format");
		}
	}
	if (swapchain_format == VK_FORMAT_UNDEFINED) {
		for (const auto& format : swapchain_formats) {
			if (format == VK_FORMAT_B8G8R8A8_UNORM) {
				swapchain_format = VK_FORMAT_B8G8R8A8_UNORM;
				break;
			}
		}
	}
	if (swapchain_format == VK_FORMAT_UNDEFINED) {
		log_error("failed to find a wanted swapchain format");
		return false;
	}
	log_msg("OpenXR: using swapchain format: $", swapchain_format);

	// -> create a single multi-layer swapchain
	const XrSwapchainCreateInfo swapchain_create_info{
		.type = XR_TYPE_SWAPCHAIN_CREATE_INFO,
		.next = nullptr,
		.createFlags = 0,
		.usageFlags = XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_SAMPLED_BIT |
					  XR_SWAPCHAIN_USAGE_UNORDERED_ACCESS_BIT | XR_SWAPCHAIN_USAGE_TRANSFER_DST_BIT |
					  XR_SWAPCHAIN_USAGE_TRANSFER_SRC_BIT,
		.format = swapchain_format,
		.sampleCount = 1,
		.width = recommended_render_size.x,
		.height = recommended_render_size.y,
		.faceCount = 1,
		.arraySize = uint32_t(views.size()),
		.mipCount = 1,
	};
	XR_CALL_RET(xrCreateSwapchain(session, &swapchain_create_info, &swapchain.swapchain), "failed to create swapchain", false);

	uint32_t swapchain_image_count = 0;
	XR_CALL_RET(xrEnumerateSwapchainImages(swapchain.swapchain, 0, &swapchain_image_count, nullptr),
				"failed to retrieve swapchain image count", false);

	log_msg("OpenXR: swapchain image count: $", swapchain_image_count);
	swapchain.swapchain_vk_images.resize(swapchain_image_count);
	for (uint32_t swapchain_image_idx = 0; swapchain_image_idx < swapchain_image_count; ++swapchain_image_idx) {
		swapchain.swapchain_vk_images[swapchain_image_idx] = {
			.type = XR_TYPE_SWAPCHAIN_IMAGE_VULKAN2_KHR,
			.next = nullptr,
			.image = nullptr,
		};
	}

	XR_CALL_RET(xrEnumerateSwapchainImages(swapchain.swapchain, swapchain_image_count, &swapchain_image_count,
										   reinterpret_cast<XrSwapchainImageBaseHeader*>(swapchain.swapchain_vk_images.data())),
				"failed to enumerate swapchain images", false);

	// wrap swapchain images
	swapchain.swapchain_images.resize(swapchain_image_count);
	swapchain.swapchain_vk_image_views.resize(swapchain_image_count);
	for (uint32_t swapchain_image_idx = 0; swapchain_image_idx < swapchain_image_count; ++swapchain_image_idx) {
		auto swapchain_image = swapchain.swapchain_vk_images[swapchain_image_idx].image;

		VkImageViewCreateInfo image_view_create_info {
			.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
			.pNext = nullptr,
			.flags = 0,
			.image = swapchain_image,
			.viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY,
			.format = swapchain_format,
			.components = {
				VK_COMPONENT_SWIZZLE_IDENTITY,
				VK_COMPONENT_SWIZZLE_IDENTITY,
				VK_COMPONENT_SWIZZLE_IDENTITY,
				VK_COMPONENT_SWIZZLE_IDENTITY
			},
			.subresourceRange = {
				.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
				.baseMipLevel = 0,
				.levelCount = 1,
				.baseArrayLayer = 0,
				.layerCount = uint32_t(views.size()),
			},
		};
		VK_CALL_RET(vkCreateImageView(vk_dev->device, &image_view_create_info, nullptr,
									  &swapchain.swapchain_vk_image_views[swapchain_image_idx]),
					"swapchain image view creation failed", false)
		vk_ctx->set_vulkan_debug_label(*vk_dev, VK_OBJECT_TYPE_IMAGE_VIEW,
									   uint64_t(swapchain.swapchain_vk_image_views[swapchain_image_idx]),
									   "vr_swapchain_image_#" + to_string(swapchain_image_idx));

		vulkan_image::external_vulkan_image_info info {
			.image = swapchain_image,
			.image_view = swapchain.swapchain_vk_image_views[swapchain_image_idx],
			.format = swapchain_format,
			.access_mask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
			.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
			.image_base_type = (COMPUTE_IMAGE_TYPE::IMAGE_2D_ARRAY |
								COMPUTE_IMAGE_TYPE::READ_WRITE |
								COMPUTE_IMAGE_TYPE::FLAG_RENDER_TARGET),
			.dim = { recommended_render_size.x, recommended_render_size.y, uint32_t(views.size()), 0 },
		};
		swapchain.swapchain_images[swapchain_image_idx] = make_unique<vulkan_image>(vk_queue, info);
		swapchain.swapchain_images[swapchain_image_idx]->set_debug_label("swapchain_image_#" + to_string(swapchain_image_idx));
	}

	// can now create input handling (-> openxr_input.cpp)
	if (!input_setup()) {
		log_error("OpenXR input setup failed");
		return false;
	}

	// all done
	return true;
}

vr_context::swapchain_info_t openxr_context::get_swapchain_info() const {
	if (swapchain.swapchain_images.empty()) {
		throw runtime_error("swapchain has not been initialized");
	}
	return {
		.image_count = uint32_t(swapchain.swapchain_images.size()),
		.image_type = swapchain.swapchain_images[0]->get_image_type(),
	};
}

string openxr_context::get_vulkan_instance_extensions() const {
	return {}; // not needed for XR_KHR_vulkan_enable2
}

string openxr_context::get_vulkan_device_extensions([[maybe_unused]] VkPhysicalDevice_T* physical_device) const {
	return {}; // not needed for XR_KHR_vulkan_enable2
}

vector<shared_ptr<event_object>> openxr_context::handle_input() {
	vector<shared_ptr<event_object>> events;

	if (!handle_input_internal(events)) {
		log_error("failed to handle input");
	}

	XrEventDataBuffer xr_event { .type = XR_TYPE_EVENT_DATA_BUFFER, .next = nullptr };
	auto& base_header = *reinterpret_cast<XrEventDataBaseHeader*>(&xr_event);
	for (;;) {
		base_header.type = XR_TYPE_EVENT_DATA_BUFFER;
		if (xrPollEvent(instance, &xr_event) != XR_SUCCESS) {
			break;
		}

		switch (xr_event.type) {
			case XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED: {
				const auto& session_state_changed_event = *reinterpret_cast<XrEventDataSessionStateChanged*>(&xr_event);
				switch (session_state_changed_event.state) {
					case XR_SESSION_STATE_READY: {
						// begin session
						assert(session_state_changed_event.session == session);
						const XrSessionBeginInfo session_begin_info{
							.type = XR_TYPE_SESSION_BEGIN_INFO,
							.next = nullptr,
							.primaryViewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO,
						};
						XR_CALL_IGNORE(xrBeginSession(session, &session_begin_info), "failed to begin session");
						break;
					}
					case XR_SESSION_STATE_FOCUSED:
						is_focused = true;
						break;
					case XR_SESSION_STATE_STOPPING:
					case XR_SESSION_STATE_EXITING:
					case XR_SESSION_STATE_LOSS_PENDING: {
						is_focused = false;
						if (session) {
							assert(session_state_changed_event.session == session);
							XR_CALL_IGNORE(xrEndSession(session), "failed to end session");
						}
						break;
					}
					default:
						// ignore
						break;
				}
				break;
			}
			case XR_TYPE_EVENT_DATA_INSTANCE_LOSS_PENDING: {
				// TODO: !
				break;
			}
			case XR_TYPE_EVENT_DATA_INTERACTION_PROFILE_CHANGED: {
				update_hand_controller_types();
				if (has_tracker_interaction_support) {
					tracker_enumerate();
				}
				break;
			}
			case XR_TYPE_EVENT_DATA_VIVE_TRACKER_CONNECTED_HTCX: {
				if (!has_tracker_interaction_support) {
					break;
				}
				tracker_enumerate();
				break;
			}
			default:
				break;
		}
	}

	return events;
}

compute_image* openxr_context::acquire_next_image() REQUIRES(!view_states_lock) {
	cur_swapchain_image = nullptr; // reset

	// wait for next frame
	const XrFrameWaitInfo frame_wait_info {
		.type = XR_TYPE_FRAME_WAIT_INFO,
		.next = nullptr,
	};
	cur_frame_state = XrFrameState {
		.type = XR_TYPE_FRAME_STATE,
		.next = nullptr,
	};
	XR_CALL_RET(xrWaitFrame(session, &frame_wait_info, &cur_frame_state),
				"frame wait failed", nullptr);

	// begin frame
	const XrFrameBeginInfo frame_begin_info {
		.type = XR_TYPE_FRAME_BEGIN_INFO,
		.next = nullptr,
	};
	XR_CALL_RET(xrBeginFrame(session, &frame_begin_info),
				"failed to begin frame", nullptr);

	cur_layer_projection_views = {{
		{
			.type = XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW,
			.next = nullptr,
		},
		{
			.type = XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW,
			.next = nullptr,
		}
	}};
	cur_layer_projection = {
		.type = XR_TYPE_COMPOSITION_LAYER_PROJECTION,
		.next = nullptr,
	};

	if (!cur_frame_state.shouldRender) {
		return nullptr;
	}

	cur_layer_projection.layerFlags = 0;
	cur_layer_projection.space = scene_space;
	cur_layer_projection.viewCount = uint32_t(views.size());
	cur_layer_projection.views = cur_layer_projection_views.data();

	const XrViewLocateInfo view_locate_info {
		.type = XR_TYPE_VIEW_LOCATE_INFO,
		.next = nullptr,
		.viewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO,
		.displayTime = cur_frame_state.predictedDisplayTime,
		.space = scene_space,
	};
	XrViewState view_state {
		.type = XR_TYPE_VIEW_STATE,
		.next = nullptr,
		.viewStateFlags = 0,
	};
	uint32_t view_count = 0;
	XR_CALL_RET(xrLocateViews(session, &view_locate_info, &view_state, uint32_t(views.size()), &view_count,
							  views.data()),
				"failed to locate views", nullptr);

	XrSpaceLocation space_location {
		.type = XR_TYPE_SPACE_LOCATION,
		.next = nullptr,
	};
	XR_CALL_RET(xrLocateSpace(view_space, scene_space, cur_frame_state.predictedDisplayTime, &space_location),
				"failed to locate space", nullptr);
	float3 hmd_view_position {
		space_location.pose.position.x,
		space_location.pose.position.y,
		space_location.pose.position.z
	};
	quaternionf hmd_view_quat {
		space_location.pose.orientation.x,
		space_location.pose.orientation.y,
		space_location.pose.orientation.z,
		space_location.pose.orientation.w
	};

	{
		// update view state
		GUARD(view_states_lock);
		hmd_view_state.position = hmd_view_position;
		hmd_view_state.orientation = hmd_view_quat;
		for (uint32_t view_index = 0; view_index < view_count; ++view_index) {
			view_states[view_index].position = {
				views[view_index].pose.position.x,
				views[view_index].pose.position.y,
				views[view_index].pose.position.z
			};
			view_states[view_index].orientation = {
				views[view_index].pose.orientation.x,
				views[view_index].pose.orientation.y,
				views[view_index].pose.orientation.z,
				views[view_index].pose.orientation.w
			};
			// store in the same order as OpenVR
			view_states[view_index].fov = {
				views[view_index].fov.angleLeft,
				views[view_index].fov.angleRight,
				views[view_index].fov.angleUp,
				views[view_index].fov.angleDown
			};
		}
	}

	for (uint32_t view_index = 0; view_index < view_count; ++view_index) {
		cur_layer_projection_views[view_index].pose = views[view_index].pose;
		cur_layer_projection_views[view_index].fov = views[view_index].fov;
		cur_layer_projection_views[view_index].subImage = XrSwapchainSubImage {
			.swapchain = swapchain.swapchain,
			.imageRect = {
				.offset = { 0, 0 },
				.extent = { int(recommended_render_size.x), int(recommended_render_size.y) },
			},
			.imageArrayIndex = view_index,
		};
	}

	// acquire and present/blit VR images
	const XrSwapchainImageAcquireInfo acq_info{
		.type = XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO,
		.next = nullptr,
	};
	uint32_t swapchain_image_idx = ~0u;
	XR_CALL_RET(xrAcquireSwapchainImage(swapchain.swapchain, &acq_info, &swapchain_image_idx),
				"failed to acquire swapchain image", nullptr);
	if (swapchain_image_idx >= swapchain.swapchain_vk_images.size()) {
		log_error("swapchain image index is out-of-bounds: $", swapchain_image_idx);
		return nullptr;
	}

	const XrSwapchainImageWaitInfo wait_info{
		.type = XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO,
		.next = nullptr,
		.timeout = XR_INFINITE_DURATION,
	};
	XR_CALL_RET(xrWaitSwapchainImage(swapchain.swapchain, &wait_info),
				"failed to wait for swapchain image", nullptr);

	cur_swapchain_image = swapchain.swapchain_images[swapchain_image_idx].get();
	return cur_swapchain_image;
}

bool openxr_context::present([[maybe_unused]] const compute_queue& cqueue, const compute_image* image) REQUIRES(!view_states_lock) {
#if defined(FLOOR_DEBUG)
	// check if specified queue and image are actually from Vulkan
	if (const auto vk_queue_ptr = dynamic_cast<const vulkan_queue*>(&cqueue); !vk_queue_ptr) {
		log_error("specified queue is not a Vulkan queue");
		return false;
	}
	if (image) {
		if (const auto vk_image_ptr = dynamic_cast<const vulkan_image*>(image); !vk_image_ptr) {
			log_error("specified queue is not a Vulkan image");
			return false;
		}
	}
#endif

	if (cur_swapchain_image != image) {
		log_error("the image provided to present() is not the same as the one returned by acquire_next_image()");
		return false;
	}

	if (cur_frame_state.shouldRender && image) {
		const XrSwapchainImageReleaseInfo release_info{
			.type = XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO,
			.next = nullptr,
		};
		XR_CALL_RET(xrReleaseSwapchainImage(swapchain.swapchain, &release_info),
					"failed to release swapchain image", false);
	}

	// end frame
	array<XrCompositionLayerBaseHeader*, 1> layers {{ reinterpret_cast<XrCompositionLayerBaseHeader*>(&cur_layer_projection) }};
	const XrFrameEndInfo frame_end_info {
		.type = XR_TYPE_FRAME_END_INFO,
		.next = nullptr,
		.displayTime = cur_frame_state.predictedDisplayTime,
		.environmentBlendMode = XR_ENVIRONMENT_BLEND_MODE_OPAQUE,
		.layerCount = cur_frame_state.shouldRender ? 1u : 0u,
		.layers = cur_frame_state.shouldRender ? layers.data() : nullptr,
	};
	XR_CALL_RET(xrEndFrame(session, &frame_end_info),
				"failed to end frame", false);

	return true;
}

vr_context::frame_view_state_t openxr_context::get_frame_view_state(const float& z_near, const float& z_far,
																	const bool with_position_in_mvm) const REQUIRES(!view_states_lock) {
	decltype(view_states) frame_view_states;
	decltype(hmd_view_state) frame_hmd_view_state;
	{
		GUARD(view_states_lock);
		frame_view_states = view_states;
		frame_hmd_view_state = hmd_view_state;
	}

	const auto project_matrix = [&frame_view_states, &z_near, &z_far](const VR_EYE eye) {
		const auto fov = frame_view_states[eye == VR_EYE::LEFT ? 0 : 1].fov.taned();
		return matrix4f::perspective<true /* !pre_adjusted_fov */, true /* is_right_handed */, true /* is_only_positive_z */>
			(fov.x, fov.y, -fov.z, -fov.w, z_near, z_far);
	};

	const auto hmd_position = -frame_hmd_view_state.position; // swap from/to for more logical handling
	const auto eye_distance = (frame_view_states[0].position - frame_view_states[1].position).length();
	const auto view_matrix = [&frame_view_states, &hmd_position, &eye_distance, &with_position_in_mvm](const VR_EYE eye) {
		// rotation matrix from view orientation
		auto mat = frame_view_states[eye == VR_EYE::LEFT ? 0 : 1].orientation.normalized().to_matrix4().inverted();

		// always add relative eye position/offset
		float3 pos { eye_distance * (eye == VR_EYE::LEFT ? 0.5f : -0.5f), 0.0f, 0.0f };
		if (with_position_in_mvm) {
			pos += hmd_position;
		}
		mat.set_translation(pos.x, pos.y, pos.z);

		return mat;
	};

	return {
		hmd_position,
		eye_distance,
		view_matrix(VR_EYE::LEFT),
		view_matrix(VR_EYE::RIGHT),
		project_matrix(VR_EYE::LEFT),
		project_matrix(VR_EYE::RIGHT)
	};
}

optional<XrPath> openxr_context::to_path(const std::string& str) {
	XrPath path { 0 };
	XR_CALL_RET(xrStringToPath(instance, str.c_str(), &path),
				"failed to convert string \"" + str + "\" to XrPath", {});
	return path;
}

optional<XrPath> openxr_context::to_path(const char* str) {
	XrPath path { 0 };
	XR_CALL_RET(xrStringToPath(instance, str, &path),
				"failed to convert string \"" + (str ? string(str) : "nullptr") + "\" to XrPath", {});
	return path;
}

XrPath openxr_context::to_path_or_throw(const std::string& str) {
	XrPath path { 0 };
	XR_CALL_ERR_EXEC(xrStringToPath(instance, str.c_str(), &path),
					 "failed to convert string \"" + str + "\" to XrPath",
					 throw std::runtime_error("failed to convert string \"" + str + "\" to XrPath"););
	return path;
}

XrPath openxr_context::to_path_or_throw(const char* str) {
	XrPath path { 0 };
	XR_CALL_ERR_EXEC(xrStringToPath(instance, str, &path),
					 "failed to convert string \"" + (str ? string(str) : "nullptr") + "\" to XrPath",
					 throw std::runtime_error("failed to convert string \"" + (str ? string(str) : "nullptr") +
											  "\" to XrPath"););
	return path;
}

optional<string> openxr_context::path_to_string(const XrPath& path) {
	if (path == XR_NULL_PATH) {
		return {}; // invalid
	}

	uint32_t size = 0u;
	XR_CALL_RET(xrPathToString(instance, path, 0u, &size, nullptr),
				"failed to query XrPath string length", {});
	if (size <= 1) {
		return ""; // empty string
	}

	string str(size, '\0');
	XR_CALL_RET(xrPathToString(instance, path, size, &size, str.data()),
				"failed to convert XrPath to string", {});
	// remove \0 terminator
	str.pop_back();
	return str;
}

#endif
