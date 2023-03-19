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

#include <floor/core/essentials.hpp>

#if !defined(FLOOR_NO_VULKAN)
#include <floor/core/platform.hpp>
#include <floor/compute/vulkan/vulkan_compute.hpp>
#include <floor/compute/spirv_handler.hpp>
#include <floor/core/logger.hpp>
#include <floor/core/core.hpp>
#include <floor/core/file_io.hpp>
#include <floor/compute/llvm_toolchain.hpp>
#include <floor/compute/universal_binary.hpp>
#include <floor/floor/floor.hpp>
#include <floor/floor/floor_version.hpp>
#include <floor/compute/device/sampler.hpp>
#include <floor/graphics/vulkan/vulkan_pipeline.hpp>
#include <floor/graphics/vulkan/vulkan_pass.hpp>
#include <floor/graphics/vulkan/vulkan_renderer.hpp>
#include <floor/vr/vr_context.hpp>
#include <regex>

#include <floor/core/platform_windows.hpp>

#include <SDL2/SDL_syswm.h>

#if defined(SDL_VIDEO_DRIVER_WINDOWS) || defined(__WINDOWS__)
#include <vulkan/vulkan_win32.h>
#endif
#if defined(SDL_VIDEO_DRIVER_X11)
#include <vulkan/vulkan_xlib.h>
#endif

#include <floor/core/essentials.hpp> // cleanup

#if defined(FLOOR_DEBUG)
static VKAPI_ATTR VkBool32 VKAPI_CALL vulkan_debug_callback(VkDebugUtilsMessageSeverityFlagBitsEXT severity,
															VkDebugUtilsMessageTypeFlagsEXT message_types floor_unused,
															const VkDebugUtilsMessengerCallbackDataEXT* cb_data,
															/* vulkan_compute* */ void* ctx) {
	const auto vk_ctx = (const vulkan_compute*)ctx;
	if (vk_ctx->is_vulkan_validation_ignored()) {
		return VK_FALSE; // don't abort
	}
	
	string debug_message;
	if (cb_data != nullptr) {
		debug_message += "\n\t";
		if (cb_data->pMessageIdName) {
			debug_message += cb_data->pMessageIdName + " ("s + to_string(cb_data->messageIdNumber) + ")\n";
		} else {
			debug_message += to_string(cb_data->messageIdNumber) + "\n";
		}
		
		vector<uint64_t> thread_ids;
		if (cb_data->pMessage) {
			auto tokens = core::tokenize(cb_data->pMessage, '|');
			for (auto& token : tokens) {
				token = core::trim(token);
				debug_message += "\t" + token + "\n";
				
				// if this is a threading error, extract the thread ids
				if (token.find("THREADING") != string::npos) {
					static const regex rx_thread_id("thread 0x([0-9a-fA-F]+)");
					smatch regex_result;
					while (regex_search(token, regex_result, rx_thread_id)) {
						thread_ids.emplace_back(strtoull(regex_result[1].str().c_str(), nullptr, 16));
						token = regex_result.suffix();
					}
				}
			}
		}
		
		if (cb_data->queueLabelCount > 0 && cb_data->pQueueLabels != nullptr) {
			debug_message += "\tqueue labels: ";
			for (uint32_t qidx = 0; qidx < cb_data->queueLabelCount; ++qidx) {
				debug_message += (cb_data->pQueueLabels[qidx].pLabelName ? cb_data->pQueueLabels[qidx].pLabelName : "<no-queue-label>");
				if (qidx + 1 < cb_data->queueLabelCount){
					debug_message += ", ";
				}
			}
			debug_message += '\n';
		}
		if (cb_data->cmdBufLabelCount > 0 && cb_data->pCmdBufLabels != nullptr) {
			debug_message += "\tcommand buffer labels: ";
			for (uint32_t cidx = 0; cidx < cb_data->cmdBufLabelCount; ++cidx) {
				debug_message += (cb_data->pCmdBufLabels[cidx].pLabelName ? cb_data->pCmdBufLabels[cidx].pLabelName : "<no-command-buffer-label>");
				if (cidx + 1 < cb_data->cmdBufLabelCount){
					debug_message += ", ";
				}
			}
			debug_message += '\n';
		}
		if (cb_data->objectCount > 0 && cb_data->pObjects != nullptr) {
			debug_message += "\tobjects:\n";
			for (uint32_t oidx = 0; oidx < cb_data->objectCount; ++oidx) {
				debug_message += "\t\t";
				debug_message += (cb_data->pObjects[oidx].pObjectName ? cb_data->pObjects[oidx].pObjectName : "<no-object-name>");
				debug_message += " ("s + vulkan_object_type_to_string(cb_data->pObjects[oidx].objectType) + ", ";
				debug_message += to_string(cb_data->pObjects[oidx].objectHandle) + ")\n";
			}
		}
		if (!thread_ids.empty()) {
			debug_message += "\tthreads:\n";
			for (const auto& tid : thread_ids) {
				debug_message += "\t\t" + to_string(tid);
#if !defined(__WINDOWS__)
				static constexpr const size_t max_thread_name_length { 16 };
				char thread_name[max_thread_name_length];
				if (pthread_getname_np((pthread_t)tid, thread_name, max_thread_name_length) == 0) {
					thread_name[max_thread_name_length - 1] = '\0';
					if (strlen(thread_name) > 0) {
						debug_message += " ("s + thread_name + ")";
					}
				}
#endif
				debug_message += '\n';
			}
		}
	} else {
		debug_message = " <callback-data-is-nullptr>";
	}
	
	if ((severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) > 0) {
		log_error("Vulkan error:$", debug_message);
	} else if ((severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) > 0) {
		log_warn("Vulkan warning:$", debug_message);
	} else if ((severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT) > 0) {
		log_msg("Vulkan info:$", debug_message);
	} else if ((severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT) > 0) {
		log_debug("Vulkan verbose:$", debug_message);
	} else {
		assert(false && "unknown severity");
	}
	logger::flush();
	return VK_FALSE; // don't abort
}
#endif

struct device_mem_info_t {
	bool valid { false };
	
	// -> vulkan_device.hpp for info
	shared_ptr<VkPhysicalDeviceMemoryProperties> mem_props;
	uint32_t device_mem_index { ~0u };
	uint32_t host_mem_cached_index { ~0u };
	uint32_t device_mem_host_coherent_index { ~0u };
	unordered_set<uint32_t> device_mem_indices;
	unordered_set<uint32_t> host_mem_cached_indices;
	unordered_set<uint32_t> device_mem_host_coherent_indices;
	bool prefer_host_coherent_mem { false };
};
static device_mem_info_t handle_and_select_device_memory(const VkPhysicalDevice& dev, const decltype(VkPhysicalDeviceProperties::deviceName)& dev_name) {
	device_mem_info_t ret {};
	
	// retrieve memory info
	ret.mem_props = make_shared<VkPhysicalDeviceMemoryProperties>();
	vkGetPhysicalDeviceMemoryProperties(dev, ret.mem_props.get());
	const auto& props = *ret.mem_props;
	
	// find largest memory of a specific type
	struct largest_heap_type_t {
		uint64_t size { 0u };
		uint32_t heap_idx { ~0u };
		uint32_t type_idx { ~0u };
	};
	const auto find_largest_memory = [&props](const VkMemoryHeapFlagBits& heap_flags,
											  const VkMemoryPropertyFlagBits& prop_flags) -> largest_heap_type_t {
		largest_heap_type_t largest_mem;
		for (uint32_t heap_idx = 0; heap_idx < props.memoryHeapCount; ++heap_idx) {
			if ((props.memoryHeaps[heap_idx].flags & heap_flags) == heap_flags) {
				for (uint32_t type_idx = 0; type_idx < props.memoryTypeCount; ++type_idx) {
					if (props.memoryTypes[type_idx].heapIndex == heap_idx &&
						(props.memoryTypes[type_idx].propertyFlags & prop_flags) == prop_flags) {
						if (largest_mem.heap_idx == ~0u || props.memoryHeaps[heap_idx].size > largest_mem.size) {
							largest_mem = {
								.size = props.memoryHeaps[heap_idx].size,
								.heap_idx = heap_idx,
								.type_idx = type_idx,
							};
						}
						break;
					}
				}
			}
		}
		return largest_mem;
	};
	
	// find largest device-local memory
	const auto largest_device_mem = find_largest_memory(VK_MEMORY_HEAP_DEVICE_LOCAL_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	if (largest_device_mem.heap_idx == ~0u) {
		log_error("no valid device-local memory found for device $", dev_name);
		return { .valid = false };
	}
	ret.device_mem_index = largest_device_mem.type_idx;
	for (uint32_t type_idx = 0; type_idx < props.memoryTypeCount; ++type_idx) {
		if (props.memoryTypes[type_idx].propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) {
			ret.device_mem_indices.emplace(type_idx);
		}
	}
	
	// find largest host-cached memory
	const auto largest_host_cached_mem = find_largest_memory(VkMemoryHeapFlagBits(0u),
															 VkMemoryPropertyFlagBits(VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
																					  VK_MEMORY_PROPERTY_HOST_CACHED_BIT));
	if (largest_host_cached_mem.heap_idx == ~0u) {
		log_error("no valid host-cached memory found for device $", dev_name);
		return { .valid = false };
	}
	ret.host_mem_cached_index = largest_host_cached_mem.type_idx;
	for (uint32_t type_idx = 0; type_idx < props.memoryTypeCount; ++type_idx) {
		if (props.memoryTypes[type_idx].propertyFlags & (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_CACHED_BIT)) {
			ret.host_mem_cached_indices.emplace(type_idx);
		}
	}
	
	// find largest device-local / host-coherent memory
	const auto largest_device_host_coherent_mem = find_largest_memory(VK_MEMORY_HEAP_DEVICE_LOCAL_BIT,
																	  VkMemoryPropertyFlagBits(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT |
																							   VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
																							   VK_MEMORY_PROPERTY_HOST_COHERENT_BIT));
	if (largest_device_host_coherent_mem.heap_idx == ~0u) {
		log_error("no valid device-local / host-coherent memory found for device $", dev_name);
		return { .valid = false };
	}
	ret.device_mem_host_coherent_index = largest_device_host_coherent_mem.type_idx;
	for (uint32_t type_idx = 0; type_idx < props.memoryTypeCount; ++type_idx) {
		if (props.memoryTypes[type_idx].propertyFlags & (VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT |
														 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
														 VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) {
			ret.device_mem_host_coherent_indices.emplace(type_idx);
		}
	}
	
	// prefer device-local/host-coherent if it is the same size as the device-local memory or it's larger than host-cached memory
	if (props.memoryHeaps[props.memoryTypes[ret.device_mem_host_coherent_index].heapIndex].size ==
		props.memoryHeaps[props.memoryTypes[ret.device_mem_index].heapIndex].size ||
		props.memoryHeaps[props.memoryTypes[ret.device_mem_host_coherent_index].heapIndex].size >=
		props.memoryHeaps[props.memoryTypes[ret.host_mem_cached_index].heapIndex].size) {
		ret.prefer_host_coherent_mem = true;
	}
	
	ret.valid = true;
	return ret;
}

vulkan_compute::vulkan_compute(const COMPUTE_CONTEXT_FLAGS ctx_flags, const bool enable_renderer_, vr_context* vr_ctx_, const vector<string> whitelist) :
compute_context(ctx_flags), vr_ctx(vr_ctx_), enable_renderer(enable_renderer_) {
	if(enable_renderer) {
		screen.x11_forwarding = floor::is_x11_forwarding();

		if (floor::get_vr()) {
			if (screen.x11_forwarding) {
				log_error("VR and X11 forwarding are mutually exclusive");
				return;
			}
		}
	}
	
	// create a vulkan instance (context)
	const auto min_vulkan_api_version = floor::get_vulkan_api_version();
	if (min_vulkan_api_version.x == 1 && min_vulkan_api_version.y < 3) {
		log_error("Vulkan version must at least be 1.3");
		return;
	}
	const VkApplicationInfo app_info {
		.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
		.pNext = nullptr,
		.pApplicationName = floor::get_app_name().c_str(),
		.applicationVersion = floor::get_app_version(),
		.pEngineName = "libfloor",
		.engineVersion = FLOOR_VERSION_U32,
		.apiVersion = VK_MAKE_VERSION(min_vulkan_api_version.x, min_vulkan_api_version.y, min_vulkan_api_version.z),
	};
	
	// TODO: query exts
	// NOTE: even without surface/xlib extension, this isn't able to start without an x session / headless right now (at least on nvidia drivers)
	set<string> instance_extensions {
#if defined(FLOOR_DEBUG)
		VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
#endif
		VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME,
	};
	if(enable_renderer && !screen.x11_forwarding) {
		instance_extensions.emplace(VK_KHR_SURFACE_EXTENSION_NAME);
#if defined(SDL_VIDEO_DRIVER_WINDOWS)
		instance_extensions.emplace(VK_KHR_WIN32_SURFACE_EXTENSION_NAME);
#elif defined(SDL_VIDEO_DRIVER_X11)
		// SDL only supports xlib
		instance_extensions.emplace(VK_KHR_XLIB_SURFACE_EXTENSION_NAME);
#endif

#if defined(SDL_VIDEO_DRIVER_WINDOWS) // seems to only exist on windows (and android) right now
		instance_extensions.emplace(VK_EXT_SWAPCHAIN_COLOR_SPACE_EXTENSION_NAME);
#endif
	}

#if !defined(FLOOR_NO_VR)
	if (vr_ctx) {
		const auto vr_instance_exts = vr_ctx->get_vulkan_instance_extensions();
		if (vr_instance_exts) {
			const string vr_instance_exts_str(vr_instance_exts.get());
			const auto vr_instance_ext_strs = core::tokenize(vr_instance_exts_str, ' ');
			for (const auto& ext : vr_instance_ext_strs) {
				instance_extensions.emplace(ext);
			}
		}
	}
#endif
	
	const vector<const char*> instance_layers {
#if defined(FLOOR_DEBUG)
		"VK_LAYER_KHRONOS_validation",
#endif
	};

	vector<const char*> instance_extensions_ptrs;
	string inst_exts_str, inst_layers_str;
	for(const auto& ext : instance_extensions) {
		inst_exts_str += ext;
		inst_exts_str += " ";
		instance_extensions_ptrs.emplace_back(ext.c_str());
	}
	for(const auto& layer : instance_layers) {
		inst_layers_str += layer;
		inst_layers_str += " ";
	}
	log_debug("using instance extensions: $", inst_exts_str);
	log_debug("using instance layers: $", inst_layers_str);

	const VkInstanceCreateInfo instance_info {
		.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
		.pNext = nullptr,
		.flags = 0,
		.pApplicationInfo = &app_info,
		.enabledLayerCount = (uint32_t)size(instance_layers),
		.ppEnabledLayerNames = size(instance_layers) > 0 ? instance_layers.data() : nullptr,
		.enabledExtensionCount = (uint32_t)size(instance_extensions_ptrs),
		.ppEnabledExtensionNames = size(instance_extensions_ptrs) > 0 ? instance_extensions_ptrs.data() : nullptr,
	};
	VK_CALL_RET(vkCreateInstance(&instance_info, nullptr, &ctx), "failed to create vulkan instance")
	
#if defined(FLOOR_DEBUG)
	// create and register debug messenger
	create_debug_utils_messenger = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(ctx, "vkCreateDebugUtilsMessengerEXT");
	if (create_debug_utils_messenger == nullptr) {
		log_error("failed to retrieve vkCreateDebugUtilsMessengerEXT function pointer");
		return;
	}
	destroy_debug_utils_messenger = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(ctx, "vkDestroyDebugUtilsMessengerEXT");
	set_debug_utils_object_name = (PFN_vkSetDebugUtilsObjectNameEXT)vkGetInstanceProcAddr(ctx, "vkSetDebugUtilsObjectNameEXT");
	cmd_begin_debug_utils_label = (PFN_vkCmdBeginDebugUtilsLabelEXT)vkGetInstanceProcAddr(ctx, "vkCmdBeginDebugUtilsLabelEXT");
	cmd_end_debug_utils_label = (PFN_vkCmdEndDebugUtilsLabelEXT)vkGetInstanceProcAddr(ctx, "vkCmdEndDebugUtilsLabelEXT");
	const VkDebugUtilsMessengerCreateInfoEXT debug_messenger_info {
		.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
		.pNext = nullptr,
		.flags = 0,
		.messageSeverity = (VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT |
							VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT
							// | VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT
							// | VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT
							),
		.messageType = (VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
						VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
						VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT),
		.pfnUserCallback = &vulkan_debug_callback,
		.pUserData = this,
	};
	VK_CALL_RET(create_debug_utils_messenger(ctx, &debug_messenger_info, nullptr, &debug_utils_messenger),
				"failed to create debug messenger")
#endif
	
	// get external memory functions
#if defined(__WINDOWS__)
	get_memory_win32_handle = (PFN_vkGetMemoryWin32HandleKHR)vkGetInstanceProcAddr(ctx, "vkGetMemoryWin32HandleKHR");
	if (get_memory_win32_handle == nullptr) {
		log_error("failed to retrieve vkGetMemoryWin32HandleKHR function pointer");
		return;
	}
	get_semaphore_win32_handle = (PFN_vkGetSemaphoreWin32HandleKHR)vkGetInstanceProcAddr(ctx, "vkGetSemaphoreWin32HandleKHR");
	if (get_semaphore_win32_handle == nullptr) {
		log_error("failed to retrieve vkGetSemaphoreWin32HandleKHR function pointer");
		return;
	}
#else
	get_memory_fd = (PFN_vkGetMemoryFdKHR)vkGetInstanceProcAddr(ctx, "vkGetMemoryFdKHR");
	if (get_memory_fd == nullptr) {
		log_error("failed to retrieve vkGetMemoryFdKHR function pointer");
		return;
	}
	get_semaphore_fd = (PFN_vkGetSemaphoreFdKHR)vkGetInstanceProcAddr(ctx, "vkGetSemaphoreFdKHR");
	if (get_semaphore_fd == nullptr) {
		log_error("failed to retrieve vkGetSemaphoreFdKHR function pointer");
		return;
	}
#endif

	// get HDR function
	if (floor::get_hdr()) {
		vk_set_hdr_metadata = (PFN_vkSetHdrMetadataEXT)vkGetInstanceProcAddr(ctx, "vkSetHdrMetadataEXT");
		if (vk_set_hdr_metadata == nullptr) {
			log_error("failed to retrieve vkSetHdrMetadataEXT function pointer");
			hdr_supported = false;
		} else {
			hdr_supported = true;
		}
	} else {
		hdr_supported = false;
	}
	
	// get layers
	uint32_t layer_count = 0;
	VK_CALL_RET(vkEnumerateInstanceLayerProperties(&layer_count, nullptr), "failed to retrieve instance layer properties count")
	vector<VkLayerProperties> layers(layer_count);
	if(layer_count > 0) {
		VK_CALL_RET(vkEnumerateInstanceLayerProperties(&layer_count, layers.data()), "failed to retrieve instance layer properties")
	}
	log_debug("found $ vulkan layer$", layer_count, (layer_count == 1 ? "" : "s"));
	
	// get devices
	uint32_t device_count = 0;
	VK_CALL_RET(vkEnumeratePhysicalDevices(ctx, &device_count, nullptr), "failed to retrieve device count")
	vector<VkPhysicalDevice> queried_devices(device_count);
	VK_CALL_RET(vkEnumeratePhysicalDevices(ctx, &device_count, queried_devices.data()), "failed to retrieve devices")
	log_debug("found $ vulkan device$", device_count, (device_count == 1 ? "" : "s"));

	auto gpu_counter = (underlying_type_t<compute_device::TYPE>)compute_device::TYPE::GPU0;
	auto cpu_counter = (underlying_type_t<compute_device::TYPE>)compute_device::TYPE::CPU0;
	for(const auto& phys_dev : queried_devices) {
		// get device props and features
		VkPhysicalDeviceProperties props;
		vkGetPhysicalDeviceProperties(phys_dev, &props);
		
		// check whitelist
		if(!whitelist.empty()) {
			const auto lc_dev_name = core::str_to_lower(props.deviceName);
			bool found = false;
			for(const auto& entry : whitelist) {
				if(lc_dev_name.find(entry) != string::npos) {
					found = true;
					break;
				}
			}
			if(!found) continue;
		}
		
		// handle device queue info + create queue info, we're going to create as many queues as are allowed by the device
		uint32_t queue_family_count = 0;
		vkGetPhysicalDeviceQueueFamilyProperties(phys_dev, &queue_family_count, nullptr);
		if(queue_family_count == 0) {
			log_error("device $ supports no queue families", props.deviceName);
			continue;
		}
		
		vector<VkQueueFamilyProperties> dev_queue_family_props(queue_family_count);
		vkGetPhysicalDeviceQueueFamilyProperties(phys_dev, &queue_family_count, dev_queue_family_props.data());
		
		// create priorities array once (all set to 0 for now)
		uint32_t max_queue_count = 0;
		for(uint32_t i = 0; i < queue_family_count; ++i) {
			max_queue_count = max(max_queue_count, dev_queue_family_props[i].queueCount);
		}
		if(max_queue_count == 0) {
			log_error("device $ supports no queues", props.deviceName);
			continue;
		}
		const vector<float> priorities(max_queue_count, 0.0f);
		
		vector<VkDeviceQueueCreateInfo> queue_create_info(queue_family_count);
		for(uint32_t i = 0; i < queue_family_count; ++i) {
			auto& queue_info = queue_create_info[i];
			queue_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
			queue_info.pNext = nullptr;
			queue_info.flags = 0;
			queue_info.queueFamilyIndex = i;
			queue_info.queueCount = dev_queue_family_props[i].queueCount;
			queue_info.pQueuePriorities = priorities.data();
		}
		
		// query other device features
		VkPhysicalDeviceDescriptorBufferFeaturesEXT desc_buf_features {
			.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_BUFFER_FEATURES_EXT,
			.pNext = nullptr,
			.descriptorBuffer = false,
			.descriptorBufferCaptureReplay = false,
			.descriptorBufferImageLayoutIgnored = false,
			.descriptorBufferPushDescriptors = false,
		};
		VkPhysicalDeviceFragmentShaderBarycentricFeaturesKHR barycentric_features {
			.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_SHADER_BARYCENTRIC_FEATURES_KHR,
			.pNext = &desc_buf_features,
			.fragmentShaderBarycentric = false,
		};
		VkPhysicalDeviceShaderAtomicFloatFeaturesEXT shader_atomic_float_features {
			.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_ATOMIC_FLOAT_FEATURES_EXT,
			.pNext = &barycentric_features,
			.shaderBufferFloat32Atomics = false,
			.shaderBufferFloat32AtomicAdd = false,
			.shaderBufferFloat64Atomics = false,
			.shaderBufferFloat64AtomicAdd = false,
			.shaderSharedFloat32Atomics = false,
			.shaderSharedFloat32AtomicAdd = false,
			.shaderSharedFloat64Atomics = false,
			.shaderSharedFloat64AtomicAdd = false,
			.shaderImageFloat32Atomics = false,
			.shaderImageFloat32AtomicAdd = false,
			.sparseImageFloat32Atomics = false,
			.sparseImageFloat32AtomicAdd = false,
		};
		VkPhysicalDeviceVulkan11Features vulkan11_features {
			.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES,
			.pNext = &shader_atomic_float_features,
			.storageBuffer16BitAccess = true,
			.uniformAndStorageBuffer16BitAccess = true,
			.storagePushConstant16 = false,
			.storageInputOutput16 = false,
			.multiview = true,
			.multiviewGeometryShader = true,
			.multiviewTessellationShader = true,
			.variablePointersStorageBuffer = true,
			.variablePointers = true,
			.protectedMemory = false,
			.samplerYcbcrConversion = false,
			.shaderDrawParameters = false,
		};
		VkPhysicalDeviceVulkan12Features vulkan12_features {
			.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
			.pNext = &vulkan11_features,
			.samplerMirrorClampToEdge = false,
			.drawIndirectCount = false,
			.storageBuffer8BitAccess = false,
			.uniformAndStorageBuffer8BitAccess = false,
			.storagePushConstant8 = false,
			.shaderBufferInt64Atomics = false,
			.shaderSharedInt64Atomics = false,
			.shaderFloat16 = false,
			.shaderInt8 = false,
			.descriptorIndexing = false,
			.shaderInputAttachmentArrayDynamicIndexing = false,
			.shaderUniformTexelBufferArrayDynamicIndexing = false,
			.shaderStorageTexelBufferArrayDynamicIndexing = false,
			.shaderUniformBufferArrayNonUniformIndexing = false,
			.shaderSampledImageArrayNonUniformIndexing = false,
			.shaderStorageBufferArrayNonUniformIndexing = false,
			.shaderStorageImageArrayNonUniformIndexing = false,
			.shaderInputAttachmentArrayNonUniformIndexing = false,
			.shaderUniformTexelBufferArrayNonUniformIndexing = false,
			.shaderStorageTexelBufferArrayNonUniformIndexing = false,
			.descriptorBindingUniformBufferUpdateAfterBind = false,
			.descriptorBindingSampledImageUpdateAfterBind = false,
			.descriptorBindingStorageImageUpdateAfterBind = false,
			.descriptorBindingStorageBufferUpdateAfterBind = false,
			.descriptorBindingUniformTexelBufferUpdateAfterBind = false,
			.descriptorBindingStorageTexelBufferUpdateAfterBind = false,
			.descriptorBindingUpdateUnusedWhilePending = false,
			.descriptorBindingPartiallyBound = false,
			.descriptorBindingVariableDescriptorCount = false,
			.runtimeDescriptorArray = false,
			.samplerFilterMinmax = false,
			.scalarBlockLayout = false,
			.imagelessFramebuffer = false,
			.uniformBufferStandardLayout = false,
			.shaderSubgroupExtendedTypes = false,
			.separateDepthStencilLayouts = false,
			.hostQueryReset = false,
			.timelineSemaphore = false,
			.bufferDeviceAddress = false,
			.bufferDeviceAddressCaptureReplay = false,
			.bufferDeviceAddressMultiDevice = false,
			.vulkanMemoryModel = false,
			.vulkanMemoryModelDeviceScope = false,
			.vulkanMemoryModelAvailabilityVisibilityChains = false,
			.shaderOutputViewportIndex = false,
			.shaderOutputLayer = false,
			.subgroupBroadcastDynamicId = false,
		};
		VkPhysicalDeviceVulkan13Features vulkan13_features {
			.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
			.pNext = &vulkan12_features,
			.robustImageAccess = false,
			.inlineUniformBlock = false,
			.descriptorBindingInlineUniformBlockUpdateAfterBind = false,
			.pipelineCreationCacheControl = false,
			.privateData = false,
			.shaderDemoteToHelperInvocation = false,
			.shaderTerminateInvocation = false,
			.subgroupSizeControl = false,
			.computeFullSubgroups = false,
			.synchronization2 = false,
			.textureCompressionASTC_HDR = false,
			.shaderZeroInitializeWorkgroupMemory = false,
			.dynamicRendering = false,
			.shaderIntegerDotProduct = false,
			.maintenance4 = false,
		};
		VkPhysicalDeviceFeatures2 features_2 {
			.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
			.pNext = &vulkan13_features,
			.features = {
				.shaderInt64 = true,
			},
		};
		vkGetPhysicalDeviceFeatures2(phys_dev, &features_2);
		
		VkPhysicalDeviceDescriptorBufferPropertiesEXT desc_buf_props {
			.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_BUFFER_PROPERTIES_EXT,
			.pNext = nullptr,
			.combinedImageSamplerDescriptorSingleArray = false,
			.bufferlessPushDescriptors = false,
			.allowSamplerImageViewPostSubmitCreation = false,
			.descriptorBufferOffsetAlignment = {},
			.maxDescriptorBufferBindings = 0,
			.maxResourceDescriptorBufferBindings = 0,
			.maxSamplerDescriptorBufferBindings = 0,
			.maxEmbeddedImmutableSamplerBindings = 0,
			.maxEmbeddedImmutableSamplers = 0,
			.bufferCaptureReplayDescriptorDataSize = 0,
			.imageCaptureReplayDescriptorDataSize = 0,
			.imageViewCaptureReplayDescriptorDataSize = 0,
			.samplerCaptureReplayDescriptorDataSize = 0,
			.accelerationStructureCaptureReplayDescriptorDataSize = 0,
			.samplerDescriptorSize = 0,
			.combinedImageSamplerDescriptorSize = 0,
			.sampledImageDescriptorSize = 0,
			.storageImageDescriptorSize = 0,
			.uniformTexelBufferDescriptorSize = 0,
			.robustUniformTexelBufferDescriptorSize = 0,
			.storageTexelBufferDescriptorSize = 0,
			.robustStorageTexelBufferDescriptorSize = 0,
			.uniformBufferDescriptorSize = 0,
			.robustUniformBufferDescriptorSize = 0,
			.storageBufferDescriptorSize = 0,
			.robustStorageBufferDescriptorSize = 0,
			.inputAttachmentDescriptorSize = 0,
			.accelerationStructureDescriptorSize = 0,
			.maxSamplerDescriptorBufferRange = {},
			.maxResourceDescriptorBufferRange = {},
			.samplerDescriptorBufferAddressSpaceSize = {},
			.resourceDescriptorBufferAddressSpaceSize = {},
			.descriptorBufferAddressSpaceSize = {},
		};
		VkPhysicalDeviceMultiviewProperties multiview_props {
			.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MULTIVIEW_PROPERTIES,
			.pNext = &desc_buf_props,
			.maxMultiviewViewCount = 0,
			.maxMultiviewInstanceIndex = 0,
		};
		VkPhysicalDeviceVulkan11Properties vulkan11_props {
			.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_PROPERTIES,
			.pNext = &multiview_props,
			.deviceUUID = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
			.driverUUID = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
			.deviceLUID = { 0, 0, 0, 0, 0, 0, 0, 0 },
			.deviceNodeMask = 0,
			.deviceLUIDValid = false,
			.subgroupSize = 0,
			.subgroupSupportedStages = {},
			.subgroupSupportedOperations = {},
			.subgroupQuadOperationsInAllStages = false,
			.pointClippingBehavior = {},
			.maxMultiviewViewCount = 0,
			.maxMultiviewInstanceIndex = 0,
			.protectedNoFault = false,
			.maxPerSetDescriptors = 0,
			.maxMemoryAllocationSize = 0,
		};
		VkPhysicalDeviceVulkan12Properties vulkan12_props {
			.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_PROPERTIES,
			.pNext = &vulkan11_props,
			.driverID = {},
			.driverName = "",
			.driverInfo = "",
			.conformanceVersion = {},
			.denormBehaviorIndependence = {},
			.roundingModeIndependence = {},
			.shaderSignedZeroInfNanPreserveFloat16 = false,
			.shaderSignedZeroInfNanPreserveFloat32 = false,
			.shaderSignedZeroInfNanPreserveFloat64 = false,
			.shaderDenormPreserveFloat16 = false,
			.shaderDenormPreserveFloat32 = false,
			.shaderDenormPreserveFloat64 = false,
			.shaderDenormFlushToZeroFloat16 = false,
			.shaderDenormFlushToZeroFloat32 = false,
			.shaderDenormFlushToZeroFloat64 = false,
			.shaderRoundingModeRTEFloat16 = false,
			.shaderRoundingModeRTEFloat32 = false,
			.shaderRoundingModeRTEFloat64 = false,
			.shaderRoundingModeRTZFloat16 = false,
			.shaderRoundingModeRTZFloat32 = false,
			.shaderRoundingModeRTZFloat64 = false,
			.maxUpdateAfterBindDescriptorsInAllPools = 0,
			.shaderUniformBufferArrayNonUniformIndexingNative = false,
			.shaderSampledImageArrayNonUniformIndexingNative = false,
			.shaderStorageBufferArrayNonUniformIndexingNative = false,
			.shaderStorageImageArrayNonUniformIndexingNative = false,
			.shaderInputAttachmentArrayNonUniformIndexingNative = false,
			.robustBufferAccessUpdateAfterBind = false,
			.quadDivergentImplicitLod = false,
			.maxPerStageDescriptorUpdateAfterBindSamplers = 0,
			.maxPerStageDescriptorUpdateAfterBindUniformBuffers = 0,
			.maxPerStageDescriptorUpdateAfterBindStorageBuffers = 0,
			.maxPerStageDescriptorUpdateAfterBindSampledImages = 0,
			.maxPerStageDescriptorUpdateAfterBindStorageImages = 0,
			.maxPerStageDescriptorUpdateAfterBindInputAttachments = 0,
			.maxPerStageUpdateAfterBindResources = 0,
			.maxDescriptorSetUpdateAfterBindSamplers = 0,
			.maxDescriptorSetUpdateAfterBindUniformBuffers = 0,
			.maxDescriptorSetUpdateAfterBindUniformBuffersDynamic = 0,
			.maxDescriptorSetUpdateAfterBindStorageBuffers = 0,
			.maxDescriptorSetUpdateAfterBindStorageBuffersDynamic = 0,
			.maxDescriptorSetUpdateAfterBindSampledImages = 0,
			.maxDescriptorSetUpdateAfterBindStorageImages = 0,
			.maxDescriptorSetUpdateAfterBindInputAttachments = 0,
			.supportedDepthResolveModes = {},
			.supportedStencilResolveModes = {},
			.independentResolveNone = false,
			.independentResolve = false,
			.filterMinmaxSingleComponentFormats = false,
			.filterMinmaxImageComponentMapping = false,
			.maxTimelineSemaphoreValueDifference = 0,
			.framebufferIntegerColorSampleCounts = {},
		};
		VkPhysicalDeviceVulkan13Properties vulkan13_props {
			.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_PROPERTIES,
			.pNext = &vulkan12_props,
			.minSubgroupSize = 0,
			.maxSubgroupSize = 0,
			.maxComputeWorkgroupSubgroups = 0,
			.requiredSubgroupSizeStages = 0,
			.maxInlineUniformBlockSize = 0,
			.maxPerStageDescriptorInlineUniformBlocks = 0,
			.maxPerStageDescriptorUpdateAfterBindInlineUniformBlocks = 0,
			.maxDescriptorSetInlineUniformBlocks = 0,
			.maxDescriptorSetUpdateAfterBindInlineUniformBlocks = 0,
			.maxInlineUniformTotalSize = 0,
			.integerDotProduct8BitUnsignedAccelerated = false,
			.integerDotProduct8BitSignedAccelerated = false,
			.integerDotProduct8BitMixedSignednessAccelerated = false,
			.integerDotProduct4x8BitPackedUnsignedAccelerated = false,
			.integerDotProduct4x8BitPackedSignedAccelerated = false,
			.integerDotProduct4x8BitPackedMixedSignednessAccelerated = false,
			.integerDotProduct16BitUnsignedAccelerated = false,
			.integerDotProduct16BitSignedAccelerated = false,
			.integerDotProduct16BitMixedSignednessAccelerated = false,
			.integerDotProduct32BitUnsignedAccelerated = false,
			.integerDotProduct32BitSignedAccelerated = false,
			.integerDotProduct32BitMixedSignednessAccelerated = false,
			.integerDotProduct64BitUnsignedAccelerated = false,
			.integerDotProduct64BitSignedAccelerated = false,
			.integerDotProduct64BitMixedSignednessAccelerated = false,
			.integerDotProductAccumulatingSaturating8BitUnsignedAccelerated = false,
			.integerDotProductAccumulatingSaturating8BitSignedAccelerated = false,
			.integerDotProductAccumulatingSaturating8BitMixedSignednessAccelerated = false,
			.integerDotProductAccumulatingSaturating4x8BitPackedUnsignedAccelerated = false,
			.integerDotProductAccumulatingSaturating4x8BitPackedSignedAccelerated = false,
			.integerDotProductAccumulatingSaturating4x8BitPackedMixedSignednessAccelerated = false,
			.integerDotProductAccumulatingSaturating16BitUnsignedAccelerated = false,
			.integerDotProductAccumulatingSaturating16BitSignedAccelerated = false,
			.integerDotProductAccumulatingSaturating16BitMixedSignednessAccelerated = false,
			.integerDotProductAccumulatingSaturating32BitUnsignedAccelerated = false,
			.integerDotProductAccumulatingSaturating32BitSignedAccelerated = false,
			.integerDotProductAccumulatingSaturating32BitMixedSignednessAccelerated = false,
			.integerDotProductAccumulatingSaturating64BitUnsignedAccelerated = false,
			.integerDotProductAccumulatingSaturating64BitSignedAccelerated = false,
			.integerDotProductAccumulatingSaturating64BitMixedSignednessAccelerated = false,
			.storageTexelBufferOffsetAlignmentBytes = 0,
			.storageTexelBufferOffsetSingleTexelAlignment = 0,
			.uniformTexelBufferOffsetAlignmentBytes = 0,
			.uniformTexelBufferOffsetSingleTexelAlignment = 0,
			.maxBufferSize = 0,
		};
		VkPhysicalDeviceProperties2 props_2 {
			.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2,
			.pNext = &vulkan13_props,
		};
		vkGetPhysicalDeviceProperties2(phys_dev, &props_2);

#if !defined(FLOOR_NO_VR)
		if (vr_ctx) {
			if (!vulkan11_features.multiview ||
				!vulkan11_features.multiviewGeometryShader ||
				!vulkan11_features.multiviewTessellationShader) {
				log_error("VR requirements not met: multi-view features are not supported by $", props.deviceName);
				continue;
			}
			if (multiview_props.maxMultiviewViewCount < 2) {
				log_error("VR requirements not met: multi-view count must be >= 2 for device $", props.deviceName);
				continue;
			}
		}
#endif
		
		// check required features
		if (!vulkan12_features.vulkanMemoryModel ||
			!vulkan12_features.vulkanMemoryModelDeviceScope) {
			log_error("Vulkan memory model is not supported by $", props.deviceName);
			continue;
		}
		
		if (!vulkan12_features.bufferDeviceAddress) {
			log_error("buffer device addresses are not supported by $", props.deviceName);
			continue;
		}
		
		if (!vulkan12_features.shaderInt8) {
			log_error("Int8 is not suppported by $", props.deviceName);
			continue;
		}
		if (!features_2.features.shaderInt16) {
			log_error("Int16 is not suppported by $", props.deviceName);
			continue;
		}
		if (!features_2.features.shaderInt64) {
			log_error("Int64 is not suppported by $", props.deviceName);
			continue;
		}
		
		if (!vulkan11_features.variablePointers ||
			!vulkan11_features.variablePointersStorageBuffer) {
			log_error("variable pointers are not supported by $", props.deviceName);
			continue;
		}
		
		if (!vulkan12_features.storageBuffer8BitAccess ||
			!vulkan12_features.uniformAndStorageBuffer8BitAccess) {
			log_error("8-bit storage not supported by $", props.deviceName);
			continue;
		}
		if (!vulkan11_features.storageBuffer16BitAccess ||
			!vulkan11_features.uniformAndStorageBuffer16BitAccess) {
			log_error("16-bit storage not supported by $", props.deviceName);
			continue;
		}
		
		if (!vulkan12_features.scalarBlockLayout) {
			log_error("scalar block layout is not supported by $", props.deviceName);
			continue;
		}
		
		if (!vulkan12_features.uniformBufferStandardLayout) {
			log_error("uniform buffer standard layout is not supported by $", props.deviceName);
			continue;
		}
		
		if (!vulkan13_features.inlineUniformBlock) {
			log_error("inline uniform blocks are not supported by $", props.deviceName);
			continue;
		}
		
		// NOTE: only require "load, store and exchange atomic operations" support on SSBOs and local memory
		if (!shader_atomic_float_features.shaderBufferFloat32Atomics ||
			!shader_atomic_float_features.shaderSharedFloat32Atomics) {
			log_error("basic float32 atomics are not supported by $", props.deviceName);
			continue;
		}
		
		if (!vulkan13_features.maintenance4) {
			log_error("maintenance4 is not supported by $", props.deviceName);
			continue;
		}
		
		if (!vulkan13_features.synchronization2) {
			log_error("synchronization2 is not supported by $", props.deviceName);
			continue;
		}
		
		// check IUB limits
		if (vulkan13_props.maxInlineUniformBlockSize < vulkan_device::min_required_inline_uniform_block_size) {
			log_error("max inline uniform block size of $ is below the required limit of $ (for device $)",
					  vulkan13_props.maxInlineUniformBlockSize, vulkan_device::min_required_inline_uniform_block_size,
					  props.deviceName);
			continue;
		}
		if (vulkan13_props.maxDescriptorSetInlineUniformBlocks < vulkan_device::min_required_inline_uniform_block_count) {
			log_error("max inline uniform block count of $ is below the required limit of $ (for device $)",
					  vulkan13_props.maxDescriptorSetInlineUniformBlocks, vulkan_device::min_required_inline_uniform_block_count,
					  props.deviceName);
			continue;
		}
		
		// handle/retrieve/check device memory properties / heaps / types and select appropriate ones
		auto dev_mem_info = handle_and_select_device_memory(phys_dev, props.deviceName);
		if (!dev_mem_info.valid) {
			log_error("memory requirements not met by device $", props.deviceName);
			continue;
		}
		
		// create device
		const vector<const char*> device_layers {
#if defined(FLOOR_DEBUG)
			"VK_LAYER_LUNARG_standard_validation",
#endif
		};

		uint32_t dev_ext_count = 0;
		vkEnumerateDeviceExtensionProperties(phys_dev, nullptr, &dev_ext_count, nullptr);
		vector<VkExtensionProperties> supported_dev_exts(dev_ext_count);
		vkEnumerateDeviceExtensionProperties(phys_dev, nullptr, &dev_ext_count, supported_dev_exts.data());
		set<string> device_supported_extensions_set;
		set<string> device_extensions_set;
		static constexpr const array filtered_exts{
			"VK_KHR_external_",
			"VK_KHR_device_group",
			"VK_KHR_win32_keyed_mutex",
			"VK_KHR_shader_clock",
			"VK_KHR_shader_subgroup_extended_types",
			"VK_KHR_spirv_1_4",
			"VK_KHR_timeline_semaphore",
			"VK_KHR_buffer_device_address",
			"VK_KHR_separate_depth_stencil_layouts",
			"VK_KHR_acceleration_structure",
			"VK_KHR_fragment_shading_rate",
			"VK_KHR_ray_query",
			"VK_KHR_ray_tracing_pipeline",
			"VK_KHR_workgroup_memory_explicit_layout",
			"VK_KHR_video_queue",
			"VK_KHR_video_decode_queue",
			"VK_KHR_video_encode_queue",
			"VK_KHR_present_id",
			"VK_KHR_present_wait",
			"VK_KHR_ray_tracing_maintenance1",
			"VK_KHR_incremental_present",
			"VK_KHR_video_decode_h264",
			"VK_KHR_video_decode_h265",
		};
		for (const auto& ext : supported_dev_exts) {
			string ext_name = ext.extensionName;
			device_supported_extensions_set.emplace(ext_name);

			// only add all KHR by default
			if (ext_name.find("VK_KHR_") == string::npos) {
				continue;
			}
			// filter out certain extensions that we don't want
			bool is_filtered = false;
			for (const auto& filtered_ext : filtered_exts) {
				if (ext_name.find(filtered_ext) != string::npos) {
					is_filtered = true;
					break;
				}
			}
			// also filter out any swapchain exts when no direct rendering is used
			if (!enable_renderer || screen.x11_forwarding) {
				if (ext_name.find("VK_KHR_swapchain" /* _* */) != string::npos) {
					continue;
				}
			}
			if (is_filtered) continue;
			device_extensions_set.emplace(ext_name);
		}

#if !defined(FLOOR_NO_VR)
		// handle VR extensions
		if (vr_ctx) {
			const auto vr_dev_exts = vr_ctx->get_vulkan_device_extensions(phys_dev);
			if (vr_dev_exts) {
				const string vr_dev_exts_str(vr_dev_exts.get());
				const auto vr_dev_ext_strs = core::tokenize(vr_dev_exts_str, ' ');
				for (const auto& ext_str : vr_dev_ext_strs) {
					device_extensions_set.emplace(ext_str);
				}
			}
		}
#endif

		// add other required or optional extensions
		device_extensions_set.emplace(VK_KHR_EXTERNAL_MEMORY_EXTENSION_NAME);
		device_extensions_set.emplace(VK_KHR_EXTERNAL_SEMAPHORE_EXTENSION_NAME);
		device_extensions_set.emplace(VK_EXT_SHADER_ATOMIC_FLOAT_EXTENSION_NAME);
		device_extensions_set.emplace(VK_KHR_MAINTENANCE_4_EXTENSION_NAME);
#if defined(__WINDOWS__)
		device_extensions_set.emplace(VK_KHR_EXTERNAL_MEMORY_WIN32_EXTENSION_NAME);
		device_extensions_set.emplace(VK_KHR_EXTERNAL_SEMAPHORE_WIN32_EXTENSION_NAME);
#else
		device_extensions_set.emplace(VK_KHR_EXTERNAL_MEMORY_FD_EXTENSION_NAME);
		device_extensions_set.emplace(VK_KHR_EXTERNAL_SEMAPHORE_FD_EXTENSION_NAME);
#endif
		if (enable_renderer && !screen.x11_forwarding) {
			if (device_supported_extensions_set.count(VK_EXT_HDR_METADATA_EXTENSION_NAME)) {
				device_extensions_set.emplace(VK_EXT_HDR_METADATA_EXTENSION_NAME);
			} else {
				hdr_supported = false;
			}
		} else {
			hdr_supported = false;
		}
		if (barycentric_features.fragmentShaderBarycentric) {
			device_extensions_set.emplace(VK_KHR_FRAGMENT_SHADER_BARYCENTRIC_EXTENSION_NAME);
		}
		if (desc_buf_features.descriptorBuffer) {
			device_extensions_set.emplace(VK_EXT_DESCRIPTOR_BUFFER_EXTENSION_NAME);
		}

		// deal with swapchain ext
		auto swapchain_ext_iter = find(begin(device_extensions_set), end(device_extensions_set), VK_KHR_SWAPCHAIN_EXTENSION_NAME);
		if (enable_renderer && !screen.x11_forwarding) {
			if (swapchain_ext_iter == device_extensions_set.end()) {
				log_error("$ extension is not supported by the device", VK_KHR_SWAPCHAIN_EXTENSION_NAME);
				continue;
			}
		} else {
			if (swapchain_ext_iter != device_extensions_set.end()) {
				// remove again, since we don't want/need it
				device_extensions_set.erase(swapchain_ext_iter);
			}
		}

		// set -> vector
		vector<string> device_extensions;
		device_extensions.reserve(device_extensions_set.size());
		device_extensions.assign(begin(device_extensions_set), end(device_extensions_set));
		
		string dev_exts_str, dev_layers_str;
		for(const auto& ext : device_extensions) {
			dev_exts_str += ext;
			dev_exts_str += " ";
		}
		for(const auto& layer : device_layers) {
			dev_layers_str += layer;
			dev_layers_str += " ";
		}
		log_debug("using device extensions: $", dev_exts_str);
		log_debug("using device layers: $", dev_layers_str);
		
		vector<const char*> device_extensions_ptrs;
		for(const auto& ext : device_extensions) {
			device_extensions_ptrs.emplace_back(ext.c_str());
		}
		
		// ext feature enablement
		vulkan13_features.inlineUniformBlock = true;
		vulkan13_features.maintenance4 = true;
		// NOTE: shaderFloat16 and shaderInt8 are optional
		
		const VkDeviceCreateInfo dev_info {
			.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
			.pNext = &features_2,
			.flags = 0,
			.queueCreateInfoCount = queue_family_count,
			.pQueueCreateInfos = queue_create_info.data(),
			.enabledLayerCount = (uint32_t)size(device_layers),
			.ppEnabledLayerNames = size(device_layers) > 0 ? device_layers.data() : nullptr,
			.enabledExtensionCount = (uint32_t)size(device_extensions_ptrs),
			.ppEnabledExtensionNames = size(device_extensions_ptrs) > 0 ? device_extensions_ptrs.data() : nullptr,
			// NOTE: must be nullptr when using .pNext with VkPhysicalDeviceFeatures2
			.pEnabledFeatures = nullptr,
		};
		
		VkDevice dev;
		VK_CALL_CONT(vkCreateDevice(phys_dev, &dev_info, nullptr, &dev), "failed to create device \""s + props.deviceName + "\"")
		
		// add device
		devices.emplace_back(make_unique<vulkan_device>());
		auto& device = (vulkan_device&)*devices.back();
		physical_devices.emplace_back(phys_dev);
		logical_devices.emplace_back(dev);
		device.context = this;
		device.physical_device = phys_dev;
		device.device = dev;
		device.name = props.deviceName;
		set_vulkan_debug_label(device, VK_OBJECT_TYPE_DEVICE, uint64_t(dev), device.name);
		copy_n(begin(vulkan11_props.deviceUUID), device.uuid.size(), begin(device.uuid));
		device.has_uuid = true;
		device.platform_vendor = COMPUTE_VENDOR::KHRONOS; // not sure what to set here
		device.version_str = (to_string(VK_VERSION_MAJOR(props.apiVersion)) + "." +
							  to_string(VK_VERSION_MINOR(props.apiVersion)) + "." +
							  to_string(VK_VERSION_PATCH(props.apiVersion)));
		device.driver_version_str = to_string(props.driverVersion);
		device.extensions = device_extensions;
		
		// TODO: determine context/platform vulkan version
		device.vulkan_version = vulkan_version_from_uint(VK_VERSION_MAJOR(props.apiVersion), VK_VERSION_MINOR(props.apiVersion));
		// "A Vulkan 1.3 implementation must support the 1.0, 1.1, 1.2, 1.3, 1.4, 1.5, and 1.6 versions of SPIR-V"
		device.spirv_version = SPIRV_VERSION::SPIRV_1_6;
		
		if(props.vendorID < 0x10000) {
			switch(props.vendorID) {
				case 0x1002:
					device.vendor = COMPUTE_VENDOR::AMD;
					device.vendor_name = "AMD";
					device.driver_version_str = to_string(VK_VERSION_MAJOR(props.driverVersion)) + ".";
					device.driver_version_str += to_string(VK_VERSION_MINOR(props.driverVersion)) + ".";
					device.driver_version_str += to_string(VK_VERSION_PATCH(props.driverVersion));
					break;
				case 0x10DE:
					device.vendor = COMPUTE_VENDOR::NVIDIA;
					device.vendor_name = "NVIDIA";
					device.driver_version_str = to_string((props.driverVersion >> 22u) & 0x3FF) + ".";
					device.driver_version_str += to_string((props.driverVersion >> 14u) & 0xFF) + ".";
					device.driver_version_str += to_string((props.driverVersion >> 6u) & 0xFF);
					break;
				case 0x8086:
					device.vendor = COMPUTE_VENDOR::INTEL;
					device.vendor_name = "INTEL";
					break;
				default:
					device.vendor = COMPUTE_VENDOR::UNKNOWN;
					device.vendor_name = "UNKNOWN";
					break;
			}
		}
		else {
			// khronos assigned vendor id (not handling this for now)
			device.vendor = COMPUTE_VENDOR::KHRONOS;
			device.vendor_name = "Khronos assigned vendor";
		}
		
		device.internal_type = props.deviceType;
		switch(props.deviceType) {
			// TODO: differentiate these?
			case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU:
			case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:
			case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:
				device.type = (compute_device::TYPE)gpu_counter;
				++gpu_counter;
				if(fastest_gpu_device == nullptr) {
					fastest_gpu_device = &device;
				}
				break;
			case VK_PHYSICAL_DEVICE_TYPE_CPU:
				device.type = (compute_device::TYPE)cpu_counter;
				++cpu_counter;
				if(fastest_cpu_device == nullptr) {
					fastest_cpu_device = &device;
				}
				break;
			case VK_PHYSICAL_DEVICE_TYPE_OTHER:
			default:
				// not handled
				break;
		}
		
		// queue count info
		device.queue_counts.resize(queue_family_count);
		for(uint32_t i = 0; i < queue_family_count; ++i) {
			device.queue_counts[i] = dev_queue_family_props[i].queueCount;
		}
		
		// limits
		const auto& limits = props.limits;
		device.constant_mem_size = limits.maxUniformBufferRange; // not an exact match, but usually the same
		device.local_mem_size = limits.maxComputeSharedMemorySize;
		
		device.max_total_local_size = limits.maxComputeWorkGroupInvocations;
		device.max_local_size = { limits.maxComputeWorkGroupSize[0], limits.maxComputeWorkGroupSize[1], limits.maxComputeWorkGroupSize[2] };
		device.max_group_size = { limits.maxComputeWorkGroupCount[0], limits.maxComputeWorkGroupCount[1], limits.maxComputeWorkGroupCount[2] };
		device.max_global_size = device.max_local_size * device.max_group_size;
		device.max_push_constants_size = limits.maxPushConstantsSize;
		
		device.max_image_1d_dim = limits.maxImageDimension1D;
		device.max_image_1d_buffer_dim = limits.maxTexelBufferElements;
		device.max_image_2d_dim = { limits.maxImageDimension2D, limits.maxImageDimension2D };
		device.max_image_3d_dim = { limits.maxImageDimension3D, limits.maxImageDimension3D, limits.maxImageDimension3D };
		device.max_mip_levels = image_mip_level_count_from_max_dim(std::max(std::max(device.max_image_2d_dim.max_element(),
																					  device.max_image_3d_dim.max_element()),
																			 device.max_image_1d_dim));
		log_debug("max img / mip: $, $, $ -> $",
				  device.max_image_1d_dim, device.max_image_2d_dim, device.max_image_3d_dim,
				  device.max_mip_levels);
		
		device.image_msaa_array_support = features_2.features.shaderStorageImageMultisample;
		device.image_msaa_array_write_support = device.image_msaa_array_support;
		device.image_cube_array_support = features_2.features.imageCubeArray;
		device.image_cube_array_write_support = device.image_cube_array_support;
		
		device.anisotropic_support = features_2.features.samplerAnisotropy;
		device.max_anisotropy = (device.anisotropic_support ? uint32_t(limits.maxSamplerAnisotropy) : 1u);
		
		device.int16_support = features_2.features.shaderInt16;
		device.float16_support = vulkan12_features.shaderFloat16;
		device.double_support = features_2.features.shaderFloat64;
		
		device.basic_32_bit_float_atomics_support = (shader_atomic_float_features.shaderBufferFloat32Atomics &&
													 shader_atomic_float_features.shaderBufferFloat32AtomicAdd &&
													 shader_atomic_float_features.shaderSharedFloat32Atomics &&
													 shader_atomic_float_features.shaderSharedFloat32AtomicAdd);
		
		device.max_inline_uniform_block_size = vulkan13_props.maxInlineUniformBlockSize;
		device.max_inline_uniform_block_count = vulkan13_props.maxDescriptorSetInlineUniformBlocks;
		log_msg("inline uniform block: max size $, max IUBs $, max total size: $'",
				device.max_inline_uniform_block_size,
				device.max_inline_uniform_block_count,
				vulkan13_props.maxInlineUniformTotalSize);
		
		device.min_storage_buffer_offset_alignment = (uint32_t)limits.minStorageBufferOffsetAlignment;
		
		device.barycentric_coord_support = barycentric_features.fragmentShaderBarycentric;
		
		if (desc_buf_features.descriptorBuffer && FLOOR_USE_VK_DESC_BUFFER) {
			if (desc_buf_props.storageBufferDescriptorSize > vulkan_buffer::max_ssbo_descriptor_size) {
				log_error("failed argument/descriptor buffer support: SSBO descriptor size is too large: $ (want $ bytes or less)",
						  desc_buf_props.storageBufferDescriptorSize, vulkan_buffer::max_ssbo_descriptor_size);
			} else {
				device.descriptor_buffer_support = true;
				device.desc_buffer_sizes.sampled_image = uint32_t(desc_buf_props.sampledImageDescriptorSize);
				device.desc_buffer_sizes.storage_image = uint32_t(desc_buf_props.storageImageDescriptorSize);
				device.desc_buffer_sizes.ubo = uint32_t(desc_buf_props.uniformBufferDescriptorSize);
				device.desc_buffer_sizes.ssbo = uint32_t(desc_buf_props.storageBufferDescriptorSize);
				log_msg("descriptor buffer: UBO $, SSBO $, image: $ / $",
						device.desc_buffer_sizes.ubo, device.desc_buffer_sizes.ssbo,
						device.desc_buffer_sizes.sampled_image, device.desc_buffer_sizes.storage_image);
				
				// init extension functions if we haven't done this yet
				if (!get_descriptor_set_layout_binding_offset) {
					get_descriptor_set_layout_binding_offset = (PFN_vkGetDescriptorSetLayoutBindingOffsetEXT)vkGetInstanceProcAddr(ctx, "vkGetDescriptorSetLayoutBindingOffsetEXT");
					get_descriptor_set_layout_size = (PFN_vkGetDescriptorSetLayoutSizeEXT)vkGetInstanceProcAddr(ctx, "vkGetDescriptorSetLayoutSizeEXT");
					get_descriptor = (PFN_vkGetDescriptorEXT)vkGetInstanceProcAddr(ctx, "vkGetDescriptorEXT");
					cmd_bind_descriptor_buffers = (PFN_vkCmdBindDescriptorBuffersEXT)vkGetInstanceProcAddr(ctx, "vkCmdBindDescriptorBuffersEXT");
					cmd_bind_descriptor_buffer_embedded_samplers = (PFN_vkCmdBindDescriptorBufferEmbeddedSamplersEXT)vkGetInstanceProcAddr(ctx, "vkCmdBindDescriptorBufferEmbeddedSamplersEXT");
					cmd_set_descriptor_buffer_offsets = (PFN_vkCmdSetDescriptorBufferOffsetsEXT)vkGetInstanceProcAddr(ctx, "vkCmdSetDescriptorBufferOffsetsEXT");
				}
				
				// this extension enables argument buffer support as well as indirect command support,
				// but we also need a minimum amount of bindable descriptor sets
				if (props.limits.maxBoundDescriptorSets < vulkan_device::min_required_bound_descriptor_sets_for_argument_buffer_support) {
					log_error("failed argument buffer support: max number of bound descriptor sets $ < required count $",
							  props.limits.maxBoundDescriptorSets, vulkan_device::min_required_bound_descriptor_sets_for_argument_buffer_support);
				} else {
					device.argument_buffer_support = true;
					device.argument_buffer_image_support = true;
					
#if 0 // WIP
					device.indirect_command_support = true;
					device.indirect_render_command_support = true;
					device.indirect_compute_command_support = true;
#endif
				}
			}
		}
		
		// set memory info
		device.mem_props = dev_mem_info.mem_props;
		device.device_mem_index = dev_mem_info.device_mem_index;
		device.host_mem_cached_index = dev_mem_info.host_mem_cached_index;
		device.device_mem_host_coherent_index = dev_mem_info.device_mem_host_coherent_index;
		device.device_mem_indices = std::move(dev_mem_info.device_mem_indices);
		device.host_mem_cached_indices = std::move(dev_mem_info.host_mem_cached_indices);
		device.device_mem_host_coherent_indices = std::move(dev_mem_info.device_mem_host_coherent_indices);
		device.prefer_host_coherent_mem = dev_mem_info.prefer_host_coherent_mem;
		
		device.global_mem_size = device.mem_props->memoryHeaps[device.mem_props->memoryTypes[device.device_mem_index].heapIndex].size;
		device.max_mem_alloc = min(device.global_mem_size,
								   (device.prefer_host_coherent_mem ?
									device.mem_props->memoryHeaps[device.mem_props->memoryTypes[device.device_mem_host_coherent_index].heapIndex].size :
									device.mem_props->memoryHeaps[device.mem_props->memoryTypes[device.host_mem_cached_index].heapIndex].size));
		
		log_msg("using memory type #$ for device allocations", device.device_mem_index);
		log_msg("using memory type #$ for device host-coherent/host-visible allocations", device.device_mem_host_coherent_index);
		log_msg("using memory type #$ for cached host-visible allocations", device.host_mem_cached_index);
		log_msg("prefer device-local/host-coherent over host-cached: $", device.prefer_host_coherent_mem);
		
		log_msg("max mem alloc: $ bytes / $ MB",
				device.max_mem_alloc,
				device.max_mem_alloc / 1024ULL / 1024ULL);
		log_msg("mem size: $ MB (global), $ KB (local), $ KB (constant)",
				device.global_mem_size / 1024ULL / 1024ULL,
				device.local_mem_size / 1024ULL,
				device.constant_mem_size / 1024ULL);
		
		log_msg("max total local size: $", device.max_total_local_size);
		log_msg("max local size: $", device.max_local_size);
		log_msg("max global size: $", device.max_global_size);
		log_msg("max group size: $", device.max_group_size);
		log_msg("queue families: $", queue_family_count);
		log_msg("max queues (family #0): $", device.queue_counts[0]);
		
		// TODO: other device flags
		// TODO: fastest device selection, tricky to do without a unit count
		
		// done
		log_debug("$ (Memory: $ MB): $ $, API: $, driver: $",
				  (device.is_gpu() ? "GPU" : (device.is_cpu() ? "CPU" : "UNKNOWN")),
				  (uint32_t)(device.global_mem_size / 1024ull / 1024ull),
				  device.vendor_name,
				  device.name,
				  device.version_str,
				  device.driver_version_str);
	}
	
	// if there are no devices left, init has failed
	if(devices.empty()) {
		if(!queried_devices.empty()) log_warn("no devices left after checking requirements and applying whitelist!");
		return;
	}
	
	// already create command queues for all devices, these will serve as the default queues and the ones returned
	// when first calling create_queue for a device (a second call will then create an actual new one)
	for(auto& dev : devices) {
		auto default_queue = create_queue(*dev);
		default_queue->set_debug_label("default_queue");
		default_queues.insert(*dev, default_queue);
		
		// reset idx to 0 so that the first user request gets the same queue
		((vulkan_device&)*dev).cur_queue_idx = 0;
	}
	
	// create fixed sampler sets for all devices
	create_fixed_sampler_set();
	
	// workaround non-existent fastest device selection
	fastest_device = devices[0].get();
	
	// init renderer
	if(enable_renderer) {
		if(!init_renderer()) {
			return;
		}
	}
	
	// successfully initialized everything and we have at least one device
	supported = true;
}

vulkan_compute::~vulkan_compute() {
#if defined(FLOOR_DEBUG)
	if (destroy_debug_utils_messenger != nullptr && debug_utils_messenger != nullptr) {
		destroy_debug_utils_messenger(ctx, debug_utils_messenger, nullptr);
	}
#endif
	
	if(!screen.x11_forwarding) {
		if(!screen.swapchain_image_views.empty()) {
			for(auto& image_view : screen.swapchain_image_views) {
				vkDestroyImageView(screen.render_device->device, image_view, nullptr);
			}
		}
		
		if(screen.swapchain != nullptr) {
			vkDestroySwapchainKHR(screen.render_device->device, screen.swapchain, nullptr);
		}
		
		if(screen.surface != nullptr) {
			vkDestroySurfaceKHR(ctx, screen.surface, nullptr);
		}
	}
	else {
		screen.x11_screen = nullptr;
	}
	
	// TODO: destroy all else
}

bool vulkan_compute::init_renderer() {
	// TODO: support window resizing
	screen.size = floor::get_physical_screen_size();
	
	// will always use the "fastest" device for now
	// TODO: config option to select the rendering device
	const auto& vk_queue = (const vulkan_queue&)*get_device_default_queue(*fastest_device);
	screen.render_device = (const vulkan_device*)fastest_device;
	
	// with X11 forwarding we can't do any of this, because DRI* is not available
	// -> emulate the behavior
	if(screen.x11_forwarding) {
		screen.image_count = 1;
		screen.format = VK_FORMAT_B8G8R8A8_UNORM;
		screen.color_space = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
		if (const auto screen_img_type = vulkan_image::image_type_from_vulkan_format(screen.format); screen_img_type) {
			screen.image_type = *screen_img_type;
		} else {
			log_error("invalid screen format");
			return false;
		}
		screen.x11_screen = static_pointer_cast<vulkan_image>(create_image(vk_queue, screen.size,
																		   COMPUTE_IMAGE_TYPE::IMAGE_2D |
																		   COMPUTE_IMAGE_TYPE::BGRA8UI_NORM |
																		   COMPUTE_IMAGE_TYPE::READ_WRITE |
																		   COMPUTE_IMAGE_TYPE::FLAG_RENDER_TARGET,
																		   COMPUTE_MEMORY_FLAG::READ_WRITE |
																		   COMPUTE_MEMORY_FLAG::HOST_READ_WRITE));
		if(!screen.x11_screen) {
			log_error("failed to create image/render-target for x11 forwarding");
			return false;
		}
		screen.x11_screen->set_debug_label("x11_screen");
		
		screen.swapchain_images.emplace_back(screen.x11_screen->get_vulkan_image());
		screen.swapchain_image_views.emplace_back(screen.x11_screen->get_vulkan_image_view());
		
		return true;
	}
	
	// query SDL window / video driver info that we need to create a vulkan surface
	SDL_SysWMinfo wm_info;
	SDL_VERSION(&wm_info.version)
	if(!SDL_GetWindowWMInfo(floor::get_window(), &wm_info)) {
		log_error("failed to retrieve window info: $", SDL_GetError());
		return false;
	}
	
#if defined(SDL_VIDEO_DRIVER_WINDOWS)
	const VkWin32SurfaceCreateInfoKHR surf_create_info {
		.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR,
		.pNext = nullptr,
		.flags = 0,
		.hinstance = GetModuleHandle(nullptr),
		.hwnd = wm_info.info.win.window,
	};
	VK_CALL_RET(vkCreateWin32SurfaceKHR(ctx, &surf_create_info, nullptr, &screen.surface),
				"failed to create win32 surface", false)
#elif defined(SDL_VIDEO_DRIVER_X11)
	const VkXlibSurfaceCreateInfoKHR surf_create_info {
		.sType = VK_STRUCTURE_TYPE_XLIB_SURFACE_CREATE_INFO_KHR,
		.pNext = nullptr,
		.flags = 0,
		.dpy = wm_info.info.x11.display,
		.window = wm_info.info.x11.window,
	};
	VK_CALL_RET(vkCreateXlibSurfaceKHR(ctx, &surf_create_info, nullptr, &screen.surface),
				"failed to create xlib surface", false)
#endif
	
#if defined(SDL_VIDEO_DRIVER_WINDOWS) || defined(SDL_VIDEO_DRIVER_X11)
	// TODO: vkGetPhysicalDeviceXlibPresentationSupportKHR/vkGetPhysicalDeviceWin32PresentationSupportKHR
	
	// verify if surface is actually usable
	VkBool32 supported = false;
	VK_CALL_RET(vkGetPhysicalDeviceSurfaceSupportKHR(screen.render_device->physical_device, vk_queue.get_family_index(),
													 screen.surface, &supported),
				"failed to query surface presentability", false)
	if(!supported) {
		log_error("surface is not presentable");
		return false;
	}
	
	// query formats and figure out which one we should use (depending on set wide-gamut and HDR flags)
	// NOTE: we do at least need/want VK_FORMAT_B8G8R8A8_UNORM
	uint32_t format_count = 0;
	VK_CALL_RET(vkGetPhysicalDeviceSurfaceFormatsKHR(screen.render_device->physical_device, screen.surface, &format_count, nullptr),
				"failed to query presentable surface formats count", false)
	if(format_count == 0) {
		log_error("surface doesn't support any formats");
		return false;
	}
	vector<VkSurfaceFormatKHR> formats(format_count);
	VK_CALL_RET(vkGetPhysicalDeviceSurfaceFormatsKHR(screen.render_device->physical_device, screen.surface, &format_count, formats.data()),
				"failed to query presentable surface formats", false)
#if 0
	for (const auto& format : formats) {
		log_msg("supported screen format: $X, $X", format.format, format.colorSpace);
	}
#endif
	
	// fallback
	screen.format = formats[0].format;
	screen.color_space = formats[0].colorSpace;
	
	const bool want_wide_color = (floor::get_wide_gamut() || floor::get_hdr());
	screen.has_wide_gamut = false;
	if (!want_wide_color) {
		// just find and use VK_FORMAT_B8G8R8A8_UNORM
		for (const auto& format : formats) {
			if (format.format == VK_FORMAT_B8G8R8A8_UNORM) {
				screen.format = VK_FORMAT_B8G8R8A8_UNORM;
				screen.color_space = format.colorSpace;
				break;
			}
		}
		const auto img_type = vulkan_image::image_type_from_vulkan_format(screen.format);
		log_debug("using screen format $ ($X)", compute_image::image_type_to_string(*img_type), screen.format);
	} else {
		// checks if the color space actually is wide-gamut
		const auto is_wide_gamut_color_space = [](const auto& color_space) {
			switch (color_space) {
				default:
				case VK_COLOR_SPACE_SRGB_NONLINEAR_KHR:
				case VK_COLOR_SPACE_EXTENDED_SRGB_LINEAR_EXT:
				case VK_COLOR_SPACE_BT709_LINEAR_EXT:
				case VK_COLOR_SPACE_BT709_NONLINEAR_EXT:
				case VK_COLOR_SPACE_PASS_THROUGH_EXT:
				case VK_COLOR_SPACE_EXTENDED_SRGB_NONLINEAR_EXT:
				case VK_COLOR_SPACE_DISPLAY_NATIVE_AMD:
					return false; // nope
				case VK_COLOR_SPACE_DISPLAY_P3_NONLINEAR_EXT:
				case VK_COLOR_SPACE_DISPLAY_P3_LINEAR_EXT:
				case VK_COLOR_SPACE_DCI_P3_NONLINEAR_EXT:
				case VK_COLOR_SPACE_BT2020_LINEAR_EXT:
				case VK_COLOR_SPACE_HDR10_ST2084_EXT:
				case VK_COLOR_SPACE_DOLBYVISION_EXT:
				case VK_COLOR_SPACE_HDR10_HLG_EXT:
				case VK_COLOR_SPACE_ADOBERGB_LINEAR_EXT:
				case VK_COLOR_SPACE_ADOBERGB_NONLINEAR_EXT:
					return true;
			}
		};

		// list of formats that we want to use/support (in order of priority)
		static constexpr const array wide_color_tiers {
			VK_FORMAT_R16G16B16A16_SFLOAT,
			VK_FORMAT_R16G16B16A16_UNORM,
			VK_FORMAT_R16G16B16A16_SNORM,
			VK_FORMAT_A2B10G10R10_UNORM_PACK32,
			VK_FORMAT_A2R10G10B10_UNORM_PACK32,
		};
		bool found_format = false;
		for (const auto& wanted_format : wide_color_tiers) {
			for (const auto& format : formats) {
				if (format.format == wanted_format && is_wide_gamut_color_space(format.colorSpace)) {
					screen.format = format.format;
					screen.color_space = format.colorSpace;
					found_format = true;
					break;
				}
			}
			if (found_format) {
				break;
			}
		}
		
		if (found_format) {
			// if we get here, we know that the format is also representable by COMPUTE_IMAGE_TYPE
			const auto img_type = vulkan_image::image_type_from_vulkan_format(screen.format);
			log_debug("wide-gamut enabled (using format $ ($X))", compute_image::image_type_to_string(*img_type), screen.format);
			screen.has_wide_gamut = true;
		} else {
			log_error("did not find a supported wide-gamut format");
		}
	}
	
	const auto screen_img_type = vulkan_image::image_type_from_vulkan_format(screen.format);
	if (!screen_img_type) {
		log_error("no matching image type for Vulkan format: $X", screen.format);
		return false;
	}
	screen.image_type = *screen_img_type;
	if (image_bits_of_channel(screen.image_type, 0) < 8) {
		log_error("screen format bit-depth is too low (must at least be 8-bit)");
		return false;
	}
	
	if (floor::get_hdr() && hdr_supported) {
		if (!screen.has_wide_gamut) {
			log_error("can't enable/use HDR when wide-gamut format + color space aren't supported");
			hdr_supported = false;
		} else {
			bool has_hdr_color_space = false;
			switch (screen.color_space) {
				default:
					break; // nope
				case VK_COLOR_SPACE_HDR10_ST2084_EXT:
				case VK_COLOR_SPACE_DOLBYVISION_EXT:
				case VK_COLOR_SPACE_HDR10_HLG_EXT:
					has_hdr_color_space = true;
					break;
			}

			string color_space_name = "unknown";
			switch (screen.color_space) {
				case VK_COLOR_SPACE_SRGB_NONLINEAR_KHR:
					color_space_name = "sRGB (non-linear)";
					break;
				case VK_COLOR_SPACE_DISPLAY_P3_NONLINEAR_EXT:
					color_space_name = "Display-P3 (non-linear)";
					break;
				case VK_COLOR_SPACE_EXTENDED_SRGB_LINEAR_EXT:
					color_space_name = "extended sRGB (linear)";
					break;
				case VK_COLOR_SPACE_DISPLAY_P3_LINEAR_EXT:
					color_space_name = "Display-P3 (linear)";
					break;
				case VK_COLOR_SPACE_DCI_P3_NONLINEAR_EXT:
					color_space_name = "DCI-P3 (non-linear)";
					break;
				case VK_COLOR_SPACE_BT709_LINEAR_EXT:
					color_space_name = "BT.709 (linear)";
					break;
				case VK_COLOR_SPACE_BT709_NONLINEAR_EXT:
					color_space_name = "BT.709 (non-linear)";
					break;
				case VK_COLOR_SPACE_BT2020_LINEAR_EXT:
					color_space_name = "BT.2020 (linear)";
					break;
				case VK_COLOR_SPACE_HDR10_ST2084_EXT:
					color_space_name = "HDR10 ST 2084";
					break;
				case VK_COLOR_SPACE_DOLBYVISION_EXT:
					color_space_name = "Dolby Vision";
					break;
				case VK_COLOR_SPACE_HDR10_HLG_EXT:
					color_space_name = "HDR10 HLG";
					break;
				case VK_COLOR_SPACE_ADOBERGB_LINEAR_EXT:
					color_space_name = "Adobe RGB (linear)";
					break;
				case VK_COLOR_SPACE_ADOBERGB_NONLINEAR_EXT:
					color_space_name = "Adobe RGB (non-linear)";
					break;
				case VK_COLOR_SPACE_PASS_THROUGH_EXT:
					color_space_name = "pass-through";
					break;
				case VK_COLOR_SPACE_EXTENDED_SRGB_NONLINEAR_EXT:
					color_space_name = "extended sRGB (non-linear)";
					break;
				case VK_COLOR_SPACE_DISPLAY_NATIVE_AMD:
					color_space_name = "display native (AMD)";
					break;
				default:
					break;
			}
			
			if (!has_hdr_color_space) {
				log_error("can't enable/use HDR with a non-HDR color space ($X: $)", screen.color_space, color_space_name);
				hdr_supported = false;
			} else {
				// create/set HDR metadata
				screen.hdr_metadata = VkHdrMetadataEXT {};
				set_vk_screen_hdr_metadata();
				log_debug("HDR enabled (using color space $ ($X))", color_space_name, screen.color_space);
			}
		}
	}
	
	//
	VkSurfaceCapabilitiesKHR surface_caps;
	VK_CALL_RET(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(screen.render_device->physical_device, screen.surface, &surface_caps),
				"failed to query surface capabilities", false)
	VkExtent2D surface_size = surface_caps.currentExtent;
	if(surface_size.width == 0xFFFFFFFFu) {
		surface_size.width = screen.size.x;
		surface_size.height = screen.size.y;
	}
	
	// try using triple buffering
	if(surface_caps.minImageCount < 3) {
		surface_caps.minImageCount = 3;
	}
	
	// choose present mode (vsync is always supported)
	VkPresentModeKHR present_mode = VK_PRESENT_MODE_FIFO_KHR;
	if(!floor::get_vsync()) {
		uint32_t mode_count = 0;
		VK_CALL_RET(vkGetPhysicalDeviceSurfacePresentModesKHR(screen.render_device->physical_device, screen.surface, &mode_count, nullptr),
					"failed to query surface present mode count", false)
		vector<VkPresentModeKHR> present_modes(mode_count);
		VK_CALL_RET(vkGetPhysicalDeviceSurfacePresentModesKHR(screen.render_device->physical_device, screen.surface, &mode_count, present_modes.data()),
					"failed to query surface present modes", false)
		if(find(present_modes.begin(), present_modes.end(), VK_PRESENT_MODE_IMMEDIATE_KHR) != present_modes.end()) {
			present_mode = VK_PRESENT_MODE_IMMEDIATE_KHR;
		}
	}
	
	// swap chain creation
	const VkSwapchainCreateInfoKHR swapchain_create_info {
		.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
		.pNext = nullptr,
		.flags = 0,
		.surface = screen.surface,
		.minImageCount = surface_caps.minImageCount,
		.imageFormat = screen.format,
		.imageColorSpace = screen.color_space,
		.imageExtent = surface_size,
		.imageArrayLayers = 1,
		.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
		// TODO: handle separate present queue (must be VK_SHARING_MODE_CONCURRENT then + specify queues)
		.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
		.queueFamilyIndexCount = 0,
		.pQueueFamilyIndices = nullptr,
		// TODO: VK_SURFACE_TRANSFORM_INHERIT_BIT_KHR?
		.preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR,
		// TODO: VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR?
		.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
		.presentMode = present_mode,
		// TODO: true for better perf, but can't exec frag shaders on clipped pixels
		.clipped = false,
		.oldSwapchain = nullptr,
	};
	VK_CALL_RET(vkCreateSwapchainKHR(screen.render_device->device, &swapchain_create_info, nullptr, &screen.swapchain),
				"failed to create swapchain", false)
	
	// now that we have a swapchain, actually set the HDR metadata for it (if HDR was enabled and can be used)
	if (screen.hdr_metadata) {
		vulkan_set_hdr_metadata(screen.render_device->device, 1, &screen.swapchain, &*screen.hdr_metadata);
	}
	
	// get all swapchain images + create views
	screen.image_count = 0;
	VK_CALL_RET(vkGetSwapchainImagesKHR(screen.render_device->device, screen.swapchain, &screen.image_count, nullptr),
				"failed to query swapchain image count", false)
	screen.swapchain_images.resize(screen.image_count);
	screen.swapchain_image_views.resize(screen.image_count);
	screen.render_semas.resize(screen.image_count);
	VK_CALL_RET(vkGetSwapchainImagesKHR(screen.render_device->device, screen.swapchain, &screen.image_count, screen.swapchain_images.data()),
				"failed to retrieve swapchain images", false)
	
	VkImageViewCreateInfo image_view_create_info {
		.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
		.pNext = nullptr,
		.flags = 0,
		.image = nullptr,
		.viewType = VK_IMAGE_VIEW_TYPE_2D,
		.format = screen.format,
		// actually want RGBA here (not BGRA)
		.components = {
			VK_COMPONENT_SWIZZLE_R,
			VK_COMPONENT_SWIZZLE_G,
			VK_COMPONENT_SWIZZLE_B,
			VK_COMPONENT_SWIZZLE_A
		},
		.subresourceRange = {
			.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
			.baseMipLevel = 0,
			.levelCount = 1,
			.baseArrayLayer = 0,
			.layerCount = 1,
		},
	};
	for(uint32_t i = 0; i < screen.image_count; ++i) {
		image_view_create_info.image = screen.swapchain_images[i];
		VK_CALL_RET(vkCreateImageView(screen.render_device->device, &image_view_create_info, nullptr, &screen.swapchain_image_views[i]),
					"image view creation failed", false)
		set_vulkan_debug_label(*screen.render_device, VK_OBJECT_TYPE_IMAGE_VIEW, uint64_t(screen.swapchain_image_views[i]),
							   "screen_image:" + to_string(i));
	}

#if !defined(FLOOR_NO_VR)
	if (vr_ctx) {
		if (!init_vr_renderer()) {
			return false;
		}
	}
#endif
	
	return true;
#else
	log_error("unsupported video driver");
	return false;
#endif
}

bool vulkan_compute::init_vr_renderer() {
	// create 2D 2-layer images for rendering (with aliasing support, so we can grab each layer individually)
	const auto& vk_queue = (const vulkan_queue&)*get_device_default_queue(*screen.render_device);
	vr_screen.render_device = screen.render_device;

	vr_screen.size = floor::get_vr_physical_screen_size();
	vr_screen.layer_count = 2;
	vr_screen.image_count = 2;
	// TODO: properly support wide-gamut (VK_FORMAT_R16G16B16A16_SFLOAT is overkill and VK_FORMAT_A2R10G10B10_UINT_PACK32 can't be used as a color att)
	vr_screen.has_wide_gamut = floor::get_wide_gamut();
	//vr_screen.has_wide_gamut = false;
	vr_screen.format = (vr_screen.has_wide_gamut ? VK_FORMAT_R16G16B16A16_SFLOAT : VK_FORMAT_B8G8R8A8_UNORM);
	vr_screen.color_space = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
	// NOTE: we'll be using multi-view / layered rendering
	vr_screen.image_type = *vulkan_image::image_type_from_vulkan_format(vr_screen.format);
	vr_screen.image_type |= COMPUTE_IMAGE_TYPE::IMAGE_2D_ARRAY | COMPUTE_IMAGE_TYPE::FLAG_RENDER_TARGET | COMPUTE_IMAGE_TYPE::READ_WRITE;
	for (uint32_t i = 0; i < vr_screen.image_count; ++i) {
		vr_screen.images.emplace_back(create_image(vk_queue, uint3 { vr_screen.size, vr_screen.layer_count },
												   vr_screen.image_type,
												   COMPUTE_MEMORY_FLAG::READ_WRITE |
												   COMPUTE_MEMORY_FLAG::HOST_READ_WRITE |
												   COMPUTE_MEMORY_FLAG::VULKAN_ALIASING));
		vr_screen.images.back()->set_debug_label("VR screen image #" + to_string(i));
	}
	vr_screen.image_locks.resize(vr_screen.image_count);
	return true;
}

pair<bool, const vulkan_compute::drawable_image_info> vulkan_compute::acquire_next_image(const bool get_multi_view_drawable) NO_THREAD_SAFETY_ANALYSIS {
	const auto& dev_queue = *get_device_default_queue(*screen.render_device);
	const auto& vk_queue = (const vulkan_queue&)dev_queue;

	if (get_multi_view_drawable && vr_ctx) {
		// manual index advance
		vr_screen.image_index = (vr_screen.image_index + 1) % vr_screen.image_count;

		// lock this image until it has been submitted for present (also blocks until the wanted image is available)
		vr_screen.image_locks[vr_screen.image_index].lock();
		auto vr_image = (vulkan_image*)vr_screen.images[vr_screen.image_index].get();

		// transition / make the render target writable
		vr_image->transition_write(dev_queue, nullptr);

		return {
			true,
			{
				.index = vr_screen.image_index,
				.image_size = vr_screen.size,
				.layer_count = vr_screen.layer_count,
				.image = vr_image->get_vulkan_image(),
				.image_view = vr_image->get_vulkan_image_view(),
				.format = vr_screen.format,
				.access_mask = vr_image->get_vulkan_access_mask(),
				.layout = vr_image->get_vulkan_image_info()->imageLayout,
				.present_layout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
				.base_type = COMPUTE_IMAGE_TYPE::IMAGE_2D_ARRAY,
			}
		};
	}
	
	if (screen.x11_forwarding) {
		screen.x11_screen->transition(dev_queue, nullptr /* create a cmd buffer */,
									  VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
									  VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT);
		return {
			true,
			{
				.index = 0u,
				.image_size = screen.size,
				.layer_count = 1,
				.image = screen.x11_screen->get_vulkan_image(),
			}
		};
	}
	
	const drawable_image_info dummy_ret {};
	
	// create new sema and acquire image
	const VkSemaphoreCreateInfo sema_create_info {
		.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
		.pNext = nullptr,
		.flags = 0,
	};
	VkSemaphore sema { nullptr };
	VK_CALL_RET(vkCreateSemaphore(screen.render_device->device, &sema_create_info, nullptr, &sema),
				"failed to create semaphore", { false, dummy_ret })
	
	const auto acq_result = vkAcquireNextImageKHR(screen.render_device->device, screen.swapchain, UINT64_MAX, sema,
												  nullptr, &screen.image_index);
	if (acq_result != VK_SUCCESS && acq_result != VK_SUBOPTIMAL_KHR) {
		log_error("failed to acquire next presentable image: $: $", acq_result, vulkan_error_to_string(acq_result));
		return { false, dummy_ret };
	}
	screen.render_semas[screen.image_index] = sema;
	
	// transition image
	const auto dst_access_mask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	const auto dst_layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	VK_CMD_BLOCK_RET(vk_queue, "image drawable transition", ({
		const VkImageMemoryBarrier image_barrier {
			.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
			.pNext = nullptr,
			.srcAccessMask = 0,
			.dstAccessMask = dst_access_mask,
			.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
			.newLayout = dst_layout,
			.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
			.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
			.image = screen.swapchain_images[screen.image_index],
			.subresourceRange = {
				.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
				.baseMipLevel = 0,
				.levelCount = 1,
				.baseArrayLayer = 0,
				.layerCount = 1,
			},
		};
		vkCmdPipelineBarrier(cmd_buffer.cmd_buffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
							 0, 0, nullptr, 0, nullptr, 1, &image_barrier);
	}), (pair { false, dummy_ret }), true /* always blocking */, &screen.render_semas[screen.image_index], 1, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
	
	return {
		true,
		{
			.index = screen.image_index,
			.image_size = screen.size,
			.layer_count = 1,
			.image = screen.swapchain_images[screen.image_index],
			.image_view = screen.swapchain_image_views[screen.image_index],
			.format = screen.format,
			.access_mask = dst_access_mask,
			.layout = dst_layout,
			.present_layout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
			.base_type = COMPUTE_IMAGE_TYPE::IMAGE_2D,
		}
	};
}

bool vulkan_compute::present_image(const drawable_image_info& drawable) {
	const auto& dev_queue = *get_device_default_queue(*screen.render_device);
	const auto& vk_queue = (const vulkan_queue&)dev_queue;
	
	if(screen.x11_forwarding) {
		screen.x11_screen->transition(dev_queue, nullptr /* create a cmd buffer */,
									  VK_ACCESS_TRANSFER_READ_BIT,
									  VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
									  VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT);
		
		// grab the current image buffer data (read-only + blocking) ...
		auto img_data = (uchar4*)screen.x11_screen->map(dev_queue,
														COMPUTE_MEMORY_MAP_FLAG::READ |
														COMPUTE_MEMORY_MAP_FLAG::BLOCK);
		
		// ... and blit it into the window
		const auto wnd_surface = SDL_GetWindowSurface(floor::get_window());
		SDL_LockSurface(wnd_surface);
		const uint2 render_dim = screen.size.minned(floor::get_physical_screen_size());
		const uint2 scale = render_dim / screen.size;
		for(uint32_t y = 0; y < screen.size.y; ++y) {
			uint32_t* px_ptr = (uint32_t*)wnd_surface->pixels + ((size_t)wnd_surface->pitch / sizeof(uint32_t)) * y;
			uint32_t img_idx = screen.size.x * y * scale.y;
			for(uint32_t x = 0; x < screen.size.x; ++x, img_idx += scale.x) {
				*px_ptr++ = SDL_MapRGB(wnd_surface->format, img_data[img_idx].z, img_data[img_idx].y, img_data[img_idx].x);
			}
		}
		screen.x11_screen->unmap(dev_queue, img_data);
		
		SDL_UnlockSurface(wnd_surface);
		SDL_UpdateWindowSurface(floor::get_window());
		return true;
	}
	
	// transition to present mode
	VK_CMD_BLOCK(vk_queue, "image present transition", ({
		const VkImageMemoryBarrier present_image_barrier {
			.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
			.pNext = nullptr,
			.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
			.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT,
			.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
			.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
			.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
			.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
			.image = drawable.image,
			.subresourceRange = {
				.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
				.baseMipLevel = 0,
				.levelCount = 1,
				.baseArrayLayer = 0,
				.layerCount = 1,
			},
		};
		vkCmdPipelineBarrier(cmd_buffer.cmd_buffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
							 0, 0, nullptr, 0, nullptr, 1, &present_image_barrier);
	}), true /* always blocking */);
	
	return queue_present(drawable);
}

bool vulkan_compute::queue_present(const drawable_image_info& drawable) NO_THREAD_SAFETY_ANALYSIS {
	if (vr_ctx && drawable.layer_count > 1) {
#if !defined(FLOOR_NO_VR)
		const auto& dev_queue = *get_device_default_queue(*vr_screen.render_device);
		auto& present_image = (vulkan_image&)*vr_screen.images[drawable.index];

		// keep image state in sync
		present_image.update_with_external_vulkan_state(drawable.present_layout, VK_ACCESS_MEMORY_READ_BIT);

		// present both eyes (+temporarily disable validation, because this will throw errors)
		ignore_validation = true;
		vr_ctx->present(dev_queue, present_image);
		ignore_validation = false;
		vr_ctx->update();

		// unlock image again
		vr_screen.image_locks[drawable.index].unlock();
#endif
	} else {
		const auto& dev_queue = *get_device_default_queue(*screen.render_device);
		const auto& vk_queue = (const vulkan_queue&)dev_queue;

		// present window image
		const VkPresentInfoKHR present_info {
			.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
			.pNext = nullptr,
			.waitSemaphoreCount = 0,
			.pWaitSemaphores = nullptr,
			.swapchainCount = 1,
			.pSwapchains = &screen.swapchain,
			.pImageIndices = &drawable.index,
			.pResults = nullptr,
		};
		const auto present_result = vkQueuePresentKHR((VkQueue)const_cast<void*>(vk_queue.get_queue_ptr()), &present_info);
		if (present_result != VK_SUCCESS && present_result != VK_SUBOPTIMAL_KHR) {
			log_error("failed to present: $: $", present_result, vulkan_error_to_string(present_result));
			return false;
		}

		// cleanup
		vkDestroySemaphore(screen.render_device->device, screen.render_semas[drawable.index], nullptr);
		screen.render_semas[drawable.index] = nullptr;
	}
	
	return true;
}

shared_ptr<compute_queue> vulkan_compute::create_queue(const compute_device& dev) const {
	const auto& vulkan_dev = (const vulkan_device&)dev;
	
	// can only create a certain amount of queues per device with vulkan, so handle this + handle the queue index
	if(vulkan_dev.cur_queue_idx >= vulkan_dev.queue_counts[0]) {
		log_warn("too many queues were created (max: $), wrapping around to #0 again", vulkan_dev.queue_counts[0]);
		vulkan_dev.cur_queue_idx = 0;
	}
	const auto next_queue_index = vulkan_dev.cur_queue_idx++;
	
	VkQueue queue_obj;
	const uint32_t family_index = 0; // always family #0 for now
	vkGetDeviceQueue(vulkan_dev.device, family_index, next_queue_index, &queue_obj);
	if(queue_obj == nullptr) {
		log_error("failed to retrieve vulkan device queue");
		return {};
	}
	
	const string queue_name = "queue:" + (next_queue_index == 0 ? "default" : to_string(next_queue_index));
	set_vulkan_debug_label(vulkan_dev, VK_OBJECT_TYPE_QUEUE, uint64_t(queue_obj), queue_name);
	
	auto ret = make_shared<vulkan_queue>(dev, queue_obj, family_index);
	queues.push_back(ret);
	return ret;
}

const compute_queue* vulkan_compute::get_device_default_queue(const compute_device& dev) const {
	for(const auto& default_queue : default_queues) {
		if(default_queue.first.get() == dev) {
			return default_queue.second.get();
		}
	}
	// only happens if the context is invalid (the default queues haven't been created)
	log_error("no default queue for this device exists yet!");
	return nullptr;
}

unique_ptr<compute_fence> vulkan_compute::create_fence(const compute_queue&) const {
	log_error("fence creation not yet supported by vulkan_compute!");
	return {};
}

shared_ptr<compute_buffer> vulkan_compute::create_buffer(const compute_queue& cqueue,
														 const size_t& size, const COMPUTE_MEMORY_FLAG flags,
														 const uint32_t opengl_type) const {
	return add_resource(make_shared<vulkan_buffer>(cqueue, size, flags, opengl_type));
}

shared_ptr<compute_buffer> vulkan_compute::create_buffer(const compute_queue& cqueue,
														 const size_t& size, void* data,
														 const COMPUTE_MEMORY_FLAG flags,
														 const uint32_t opengl_type) const {
	return add_resource(make_shared<vulkan_buffer>(cqueue, size, data, flags, opengl_type));
}

shared_ptr<compute_buffer> vulkan_compute::wrap_buffer(const compute_queue&, const uint32_t, const uint32_t,
													   const COMPUTE_MEMORY_FLAG) const {
	log_error("not supported by vulkan_compute!");
	return {};
}

shared_ptr<compute_buffer> vulkan_compute::wrap_buffer(const compute_queue&, const uint32_t, const uint32_t,
													   void*, const COMPUTE_MEMORY_FLAG) const {
	log_error("not supported by vulkan_compute!");
	return {};
}

shared_ptr<compute_image> vulkan_compute::create_image(const compute_queue& cqueue,
													   const uint4 image_dim,
													   const COMPUTE_IMAGE_TYPE image_type,
													   const COMPUTE_MEMORY_FLAG flags,
													   const uint32_t opengl_type) const {
	return add_resource(make_shared<vulkan_image>(cqueue, image_dim, image_type, nullptr, flags, opengl_type));
}

shared_ptr<compute_image> vulkan_compute::create_image(const compute_queue& cqueue,
													   const uint4 image_dim,
													   const COMPUTE_IMAGE_TYPE image_type,
													   void* data,
													   const COMPUTE_MEMORY_FLAG flags,
													   const uint32_t opengl_type) const {
	return add_resource(make_shared<vulkan_image>(cqueue, image_dim, image_type, data, flags, opengl_type));
}

shared_ptr<compute_image> vulkan_compute::wrap_image(const compute_queue&, const uint32_t, const uint32_t,
													 const COMPUTE_MEMORY_FLAG) const {
	log_error("not supported by vulkan_compute!");
	return {};
}

shared_ptr<compute_image> vulkan_compute::wrap_image(const compute_queue&, const uint32_t, const uint32_t,
													 void*, const COMPUTE_MEMORY_FLAG) const {
	log_error("not supported by vulkan_compute!");
	return {};
}

shared_ptr<compute_program> vulkan_compute::add_universal_binary(const string& file_name) {
	auto bins = universal_binary::load_dev_binaries_from_archive(file_name, *this);
	if (bins.ar == nullptr || bins.dev_binaries.empty()) {
		log_error("failed to load universal binary: $", file_name);
		return {};
	}
	
	// create the program
	vulkan_program::program_map_type prog_map;
	prog_map.reserve(devices.size());
	for (size_t i = 0, dev_count = devices.size(); i < dev_count; ++i) {
		const auto& vlk_dev = (const vulkan_device&)*devices[i];
		const auto& dev_best_bin = bins.dev_binaries[i];
		const auto func_info = universal_binary::translate_function_info(dev_best_bin.first->functions);
		
		auto container = spirv_handler::load_container_from_memory(dev_best_bin.first->data.data(),
																   dev_best_bin.first->data.size(),
																   file_name);
		if(!container.valid) return {}; // already prints an error
		
		prog_map.insert_or_assign(vlk_dev,
								  create_vulkan_program_internal(vlk_dev, container, func_info, file_name));
	}
	
	return add_program(std::move(prog_map));
}

shared_ptr<vulkan_program> vulkan_compute::add_program(vulkan_program::program_map_type&& prog_map) {
	// create the program object, which in turn will create kernel objects for all kernel functions in the program,
	// for all devices contained in the program map
	auto prog = make_shared<vulkan_program>(std::move(prog_map));
	{
		GUARD(programs_lock);
		programs.push_back(prog);
	}
	return prog;
}

shared_ptr<compute_program> vulkan_compute::add_program_file(const string& file_name,
															 const string additional_options) {
	compile_options options { .cli = additional_options };
	return add_program_file(file_name, options);
}

shared_ptr<compute_program> vulkan_compute::add_program_file(const string& file_name,
															 compile_options options) {
	// compile the source file for all devices in the context
	vulkan_program::program_map_type prog_map;
	prog_map.reserve(devices.size());
	options.target = llvm_toolchain::TARGET::SPIRV_VULKAN;
	for(const auto& dev : devices) {
		prog_map.insert_or_assign((const vulkan_device&)*dev,
								  create_vulkan_program(*dev, llvm_toolchain::compile_program_file(*dev, file_name, options)));
	}
	return add_program(std::move(prog_map));
}

shared_ptr<compute_program> vulkan_compute::add_program_source(const string& source_code,
															   const string additional_options) {
	compile_options options { .cli = additional_options };
	return add_program_source(source_code, options);
}

shared_ptr<compute_program> vulkan_compute::add_program_source(const string& source_code,
															   compile_options options) {
	// compile the source code for all devices in the context
	vulkan_program::program_map_type prog_map;
	prog_map.reserve(devices.size());
	options.target = llvm_toolchain::TARGET::SPIRV_VULKAN;
	for(const auto& dev : devices) {
		prog_map.insert_or_assign((const vulkan_device&)*dev,
								  create_vulkan_program(*dev, llvm_toolchain::compile_program(*dev, source_code, options)));
	}
	return add_program(std::move(prog_map));
}

vulkan_program::vulkan_program_entry vulkan_compute::create_vulkan_program(const compute_device& device,
																		   llvm_toolchain::program_data program) {
	if(!program.valid) {
		return {};
	}
	
	auto container = spirv_handler::load_container(program.data_or_filename);
	if(!floor::get_toolchain_keep_temp() && file_io::is_file(program.data_or_filename)) {
		// cleanup if file exists
		core::system("rm " + program.data_or_filename);
	}
	if(!container.valid) return {}; // already prints an error
	
	return create_vulkan_program_internal((const vulkan_device&)device, container, program.functions,
										  program.data_or_filename);
}

vulkan_program::vulkan_program_entry
vulkan_compute::create_vulkan_program_internal(const vulkan_device& device,
											   const spirv_handler::container& container,
											   const vector<llvm_toolchain::function_info>& functions,
											   const string& identifier) {
	vulkan_program::vulkan_program_entry ret;
	ret.functions = functions;
	
	// create modules
	ret.programs.reserve(container.entries.size());
	for(const auto& entry : container.entries) {
		// map functions (names) to the module index
		const auto mod_idx = (uint32_t)ret.programs.size();
		for(const auto& func_name : entry.function_names) {
			ret.func_to_mod_map.emplace(func_name, mod_idx);
		}
		
		const VkShaderModuleCreateInfo module_info {
			.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
			.pNext = nullptr,
			.flags = 0,
			.codeSize = entry.data_word_count * 4,
			.pCode = &container.spirv_data[entry.data_offset],
		};
		VkShaderModule module { nullptr };
		VK_CALL_RET(vkCreateShaderModule(device.device, &module_info, nullptr, &module),
					"failed to create shader module (\"" + identifier + "\") for device \"" + device.name + "\"", ret)
		set_vulkan_debug_label(device, VK_OBJECT_TYPE_SHADER_MODULE, uint64_t(module), identifier);
		ret.programs.emplace_back(module);
	}

	ret.valid = true;
	return ret;
}

shared_ptr<compute_program> vulkan_compute::add_precompiled_program_file(const string& file_name,
																		 const vector<llvm_toolchain::function_info>& functions) {
	// TODO: allow spir-v container?
	size_t code_size = 0;
	auto code = spirv_handler::load_binary(file_name, code_size);
	if(code == nullptr) return {};
	
	const VkShaderModuleCreateInfo module_info {
		.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
		.pNext = nullptr,
		.flags = 0,
		.codeSize = code_size,
		.pCode = code.get(),
	};
	
	// assume pre-compiled program is the same for all devices
	vulkan_program::program_map_type prog_map;
	prog_map.reserve(devices.size());
	for(const auto& dev : devices) {
		vulkan_program::vulkan_program_entry entry;
		entry.functions = functions;
		
		VkShaderModule module { nullptr };
		const auto& vk_dev = (const vulkan_device&)dev;
		VK_CALL_CONT(vkCreateShaderModule(vk_dev.device, &module_info, nullptr, &module),
					 "failed to create shader module (\"" + file_name + "\") for device \"" + dev->name + "\"")
		set_vulkan_debug_label(vk_dev, VK_OBJECT_TYPE_SHADER_MODULE, uint64_t(module), file_name);
		entry.programs.emplace_back(module);
		entry.valid = true;
		
		prog_map.insert_or_assign((const vulkan_device&)*dev, entry);
	}
	return add_program(std::move(prog_map));
}

shared_ptr<compute_program::program_entry> vulkan_compute::create_program_entry(const compute_device& device,
																				llvm_toolchain::program_data program,
																				const llvm_toolchain::TARGET) {
	return make_shared<vulkan_program::vulkan_program_entry>(create_vulkan_program((const vulkan_device&)device, program));
}

void vulkan_compute::create_fixed_sampler_set() const {
	union vulkan_fixed_sampler {
		struct {
			//! nearest or linear, includes mip-map filtering
			uint32_t filter : 1;
			//! 0 = clamp to edge, 1 = repeat
			uint32_t address_mode : 1;
			//! -> sampler.hpp: never, less, equal, less or equal, greater, not equal, greater or equal, always
			// TODO: get rid of always, b/c the compiler takes care of these (never as well, but we need a way to signal "no depth compare")
			uint32_t compare_mode : 3;
			//! unused
			uint32_t _unused : 27;
		};
		uint32_t value;
	};
	static_assert(sizeof(vulkan_fixed_sampler) == 4, "invalid sampler size");
	static_assert(uint32_t(VK_COMPARE_OP_NEVER) == uint32_t(COMPARE_FUNCTION::NEVER) &&
				  uint32_t(VK_COMPARE_OP_LESS) == uint32_t(COMPARE_FUNCTION::LESS) &&
				  uint32_t(VK_COMPARE_OP_EQUAL) == uint32_t(COMPARE_FUNCTION::EQUAL) &&
				  uint32_t(VK_COMPARE_OP_LESS_OR_EQUAL) == uint32_t(COMPARE_FUNCTION::LESS_OR_EQUAL) &&
				  uint32_t(VK_COMPARE_OP_GREATER) == uint32_t(COMPARE_FUNCTION::GREATER) &&
				  uint32_t(VK_COMPARE_OP_NOT_EQUAL) == uint32_t(COMPARE_FUNCTION::NOT_EQUAL) &&
				  uint32_t(VK_COMPARE_OP_GREATER_OR_EQUAL) == uint32_t(COMPARE_FUNCTION::GREATER_OR_EQUAL) &&
				  uint32_t(VK_COMPARE_OP_ALWAYS) == uint32_t(COMPARE_FUNCTION::ALWAYS),
				  "failed depth compare function sanity check");
	
	// 5 bits -> 32 combinations
	static constexpr const uint32_t max_combinations { 32 };
	
	for (auto& dev : devices) {
		auto& vk_dev = (vulkan_device&)*dev;
		vk_dev.fixed_sampler_set.resize(max_combinations, nullptr);
		vk_dev.fixed_sampler_image_info.resize(max_combinations,
											   VkDescriptorImageInfo { nullptr, nullptr, VK_IMAGE_LAYOUT_UNDEFINED });
	}

	// create the samplers for all devices
	for (uint32_t combination = 0; combination < max_combinations; ++combination) {
		const vulkan_fixed_sampler smplr { .value = combination };
		const VkFilter filter = (smplr.filter == 0 ? VK_FILTER_NEAREST : VK_FILTER_LINEAR);
		const VkSamplerMipmapMode mipmap_filter = (smplr.filter == 0 ?
												   VK_SAMPLER_MIPMAP_MODE_NEAREST : VK_SAMPLER_MIPMAP_MODE_LINEAR);
		const VkSamplerAddressMode address_mode = (smplr.address_mode == 0 ?
												   VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE : VK_SAMPLER_ADDRESS_MODE_REPEAT);
		VkSamplerCreateInfo sampler_create_info {
			.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
			.pNext = nullptr,
			.flags = 0,
			.magFilter = filter,
			.minFilter = filter,
			.mipmapMode = mipmap_filter,
			.addressModeU = address_mode,
			.addressModeV = address_mode,
			.addressModeW = address_mode,
			.mipLodBias = 0.0f,
			// always enable anisotropic filtering when using linear filtering
			.anisotropyEnable = (smplr.filter != 0),
			.maxAnisotropy = 0.0f,
			.compareEnable = (smplr.compare_mode != 0),
			// NOTE: this matches 1:1, we will filter out NEVER/NONE and ALWAYS read ops in the compiler
			.compareOp = (VkCompareOp)smplr.compare_mode,
			.minLod = 0.0f,
			.maxLod = (smplr.filter != 0 ? 32.0f : 0.0f),
			.borderColor = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK,
			.unnormalizedCoordinates = false,
		};
		
		for (auto& dev : devices) {
			auto& vk_dev = (vulkan_device&)*dev;
			if (sampler_create_info.anisotropyEnable) {
				sampler_create_info.anisotropyEnable = vk_dev.anisotropic_support;
				sampler_create_info.maxAnisotropy = float(vk_dev.max_anisotropy);
			}
			
			VK_CALL_CONT(vkCreateSampler(vk_dev.device, &sampler_create_info, nullptr, &vk_dev.fixed_sampler_set[combination]),
						 "failed to create sampler (#" + to_string(combination) + ")")
			set_vulkan_debug_label(vk_dev, VK_OBJECT_TYPE_SAMPLER, uint64_t(vk_dev.fixed_sampler_set[combination]),
								   "immutable_sampler:" + to_string(combination));
			
			vk_dev.fixed_sampler_image_info[combination].sampler = vk_dev.fixed_sampler_set[combination];
		}
	}
	
	// create the descriptor set for all devices
	for (auto& dev : devices) {
		auto& vk_dev = (vulkan_device&)*dev;
		if (!vk_dev.descriptor_buffer_support) {
			// -> legacy descriptor set
			const VkDescriptorSetLayoutBinding fixed_samplers_desc_set_layout {
				.binding = 0,
				.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER,
				.descriptorCount = max_combinations,
				.stageFlags = VK_SHADER_STAGE_ALL,
				.pImmutableSamplers = vk_dev.fixed_sampler_set.data(),
			};
			const VkDescriptorSetLayoutCreateInfo desc_set_layout_info {
				.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
				.pNext = nullptr,
				.flags = 0,
				.bindingCount = 1,
				.pBindings = &fixed_samplers_desc_set_layout,
			};
			
			VK_CALL_CONT(vkCreateDescriptorSetLayout(vk_dev.device, &desc_set_layout_info, nullptr,
													 &vk_dev.fixed_sampler_desc_set_layout),
						 "failed to create fixed sampler set descriptor set layout")
			set_vulkan_debug_label(vk_dev, VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT, uint64_t(vk_dev.fixed_sampler_desc_set_layout),
								   "immutable_sampler_descriptor_set_layout");
			
			// TODO: use device global desc pool allocation once this is in place
			const VkDescriptorPoolSize desc_pool_size {
				.type = VK_DESCRIPTOR_TYPE_SAMPLER,
				.descriptorCount = max_combinations,
			};
			const VkDescriptorPoolCreateInfo desc_pool_info {
				.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
				.pNext = nullptr,
				.flags = 0,
				.maxSets = 1,
				.poolSizeCount = 1,
				.pPoolSizes = &desc_pool_size,
			};
			VK_CALL_CONT(vkCreateDescriptorPool(vk_dev.device, &desc_pool_info, nullptr, &vk_dev.legacy_fixed_sampler_desc_pool),
						 "failed to create fixed sampler set descriptor pool")
			set_vulkan_debug_label(vk_dev, VK_OBJECT_TYPE_DESCRIPTOR_POOL, uint64_t(vk_dev.legacy_fixed_sampler_desc_pool),
								   "immutable_sampler_descriptor_pool");
			
			// allocate descriptor set
			const VkDescriptorSetAllocateInfo desc_set_alloc_info {
				.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
				.pNext = nullptr,
				.descriptorPool = vk_dev.legacy_fixed_sampler_desc_pool,
				.descriptorSetCount = 1,
				.pSetLayouts = &vk_dev.fixed_sampler_desc_set_layout,
			};
			VK_CALL_CONT(vkAllocateDescriptorSets(vk_dev.device, &desc_set_alloc_info, &vk_dev.legacy_fixed_sampler_desc_set),
						 "failed to allocate fixed sampler set descriptor set")
			set_vulkan_debug_label(vk_dev, VK_OBJECT_TYPE_DESCRIPTOR_SET, uint64_t(vk_dev.legacy_fixed_sampler_desc_set),
								   "immutable_sampler_descriptor_set");
			
			// TODO: cleanup!
		} else {
			// -> for descriptor buffer usage
			// NOTE: sampler arrays are not supported -> must create #max_combinations individual bindings/samplers
			vector<VkDescriptorSetLayoutBinding> fixed_sampler_bindings(max_combinations);
			for (uint32_t i = 0; i < max_combinations; ++i) {
				fixed_sampler_bindings[i] = {
					.binding = i,
					.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER,
					.descriptorCount = 1,
					.stageFlags = VK_SHADER_STAGE_ALL,
					.pImmutableSamplers = &vk_dev.fixed_sampler_set[i],
				};
			}
			const VkDescriptorSetLayoutCreateInfo desc_set_layout_info {
				.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
				.pNext = nullptr,
				.flags = (VK_DESCRIPTOR_SET_LAYOUT_CREATE_EMBEDDED_IMMUTABLE_SAMPLERS_BIT_EXT |
						  VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT),
				.bindingCount = max_combinations,
				.pBindings = fixed_sampler_bindings.data(),
			};
			
			VK_CALL_CONT(vkCreateDescriptorSetLayout(vk_dev.device, &desc_set_layout_info, nullptr,
													 &vk_dev.fixed_sampler_desc_set_layout),
						 "failed to create fixed sampler set descriptor set layout")
			set_vulkan_debug_label(vk_dev, VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT, uint64_t(vk_dev.fixed_sampler_desc_set_layout),
								   "immutable_sampler_descriptor_set_layout");
		}
	}
}

unique_ptr<graphics_pipeline> vulkan_compute::create_graphics_pipeline(const render_pipeline_description& pipeline_desc,
																	   const bool with_multi_view_support) const {
	auto pipeline = make_unique<vulkan_pipeline>(pipeline_desc, devices, with_multi_view_support && (vr_ctx != nullptr));
	if (!pipeline || !pipeline->is_valid()) {
		return {};
	}
	return pipeline;
}

unique_ptr<graphics_pass> vulkan_compute::create_graphics_pass(const render_pass_description& pass_desc,
															   const bool with_multi_view_support) const {
	auto pass = make_unique<vulkan_pass>(pass_desc, devices, with_multi_view_support && (vr_ctx != nullptr));
	if (!pass || !pass->is_valid()) {
		return {};
	}
	return pass;
}

unique_ptr<graphics_renderer> vulkan_compute::create_graphics_renderer(const compute_queue& cqueue,
																	   const graphics_pass& pass,
																	   const graphics_pipeline& pipeline,
																	   const bool create_multi_view_renderer) const {
	if (create_multi_view_renderer && !is_vr_supported()) {
		log_error("can't create a multi-view/VR graphics renderer when VR is not supported");
		return {};
	}

	auto renderer = make_unique<vulkan_renderer>(cqueue, pass, pipeline, create_multi_view_renderer);
	if (!renderer || !renderer->is_valid()) {
		return {};
	}
	return renderer;
}

COMPUTE_IMAGE_TYPE vulkan_compute::get_renderer_image_type() const {
	return (!vr_ctx ? screen.image_type : vr_screen.image_type);
}

uint4 vulkan_compute::get_renderer_image_dim() const {
	if (!vr_ctx) {
		return { screen.size, 0, 0 };
	} else {
		return { vr_screen.size, vr_screen.layer_count, 0 };
	}
}

bool vulkan_compute::is_vr_supported() const {
	return (vr_ctx ? true : false);
}

vr_context* vulkan_compute::get_renderer_vr_context() const {
	return vr_ctx;
}

void vulkan_compute::set_hdr_metadata(const hdr_metadata_t& hdr_metadata_) {
	compute_context::set_hdr_metadata(hdr_metadata_);
	
	// update screen and swapchain HDR metadata (if we previously enabled HDR support)
	set_vk_screen_hdr_metadata();
	if (screen.hdr_metadata) {
		vulkan_set_hdr_metadata(screen.render_device->device, 1, &screen.swapchain, &*screen.hdr_metadata);
	}
}

void vulkan_compute::set_vk_screen_hdr_metadata() {
	if (!screen.hdr_metadata) {
		return;
	}
	
	screen.hdr_metadata = {
		.sType = VK_STRUCTURE_TYPE_HDR_METADATA_EXT,
		.pNext = nullptr,
		.displayPrimaryRed = { hdr_metadata.primaries[0].x, hdr_metadata.primaries[0].y },
		.displayPrimaryGreen = { hdr_metadata.primaries[1].x, hdr_metadata.primaries[1].y },
		.displayPrimaryBlue = { hdr_metadata.primaries[2].x, hdr_metadata.primaries[2].y },
		.whitePoint = { hdr_metadata.white_point.x, hdr_metadata.white_point.y },
		.maxLuminance = hdr_metadata.luminance.y,
		.minLuminance = hdr_metadata.luminance.x,
		.maxContentLightLevel = hdr_metadata.max_content_light_level,
		.maxFrameAverageLightLevel = hdr_metadata.max_average_light_level,
	};
}

#if defined(FLOOR_DEBUG)
void vulkan_compute::set_vulkan_debug_label(const vulkan_device& dev, const VkObjectType type, const uint64_t& handle, const string& label) const {
	if (set_debug_utils_object_name == nullptr) {
		return;
	}
	
	const VkDebugUtilsObjectNameInfoEXT name_info {
		.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
		.pNext = nullptr,
		.objectType = type,
		.objectHandle = handle,
		.pObjectName = label.c_str(),
	};
	set_debug_utils_object_name(dev.device, &name_info);
}

void vulkan_compute::vulkan_begin_cmd_debug_label(const VkCommandBuffer& cmd_buffer, const string& label) const {
	if (cmd_begin_debug_utils_label == nullptr) {
		return;
	}
	
	const VkDebugUtilsLabelEXT debug_label {
		.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT,
		.pNext = nullptr,
		.pLabelName = label.c_str(),
		.color = {},
	};
	cmd_begin_debug_utils_label(cmd_buffer, &debug_label);
}

void vulkan_compute::vulkan_end_cmd_debug_label(const VkCommandBuffer& cmd_buffer) const {
	if (cmd_end_debug_utils_label == nullptr) {
		return;
	}
	cmd_end_debug_utils_label(cmd_buffer);
}
#endif

unique_ptr<indirect_command_pipeline> vulkan_compute::create_indirect_command_pipeline(const indirect_command_description& desc floor_unused) const {
	// TODO: !
	log_error("not yet supported by vulkan_compute!");
	return {};
}

#endif
