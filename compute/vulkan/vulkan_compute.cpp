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

#include <floor/core/essentials.hpp>

#if !defined(FLOOR_NO_VULKAN)
#include <floor/core/platform.hpp>
#include <floor/compute/vulkan/vulkan_compute.hpp>
#include <floor/compute/spirv_handler.hpp>
#include <floor/core/gl_support.hpp>
#include <floor/core/logger.hpp>
#include <floor/core/core.hpp>
#include <floor/core/file_io.hpp>
#include <floor/compute/llvm_toolchain.hpp>
#include <floor/compute/universal_binary.hpp>
#include <floor/floor/floor.hpp>
#include <floor/floor/floor_version.hpp>
#include <floor/compute/device/sampler.hpp>

#if defined(FLOOR_DEBUG)
static VKAPI_ATTR VkBool32 VKAPI_CALL vulkan_debug_callback(VkDebugReportFlagsEXT flags floor_unused,
															VkDebugReportObjectTypeEXT object_type floor_unused,
															uint64_t object floor_unused,
															size_t location floor_unused,
															int32_t message_code,
															const char* layer_prefix,
															const char* message,
															/* vulkan_compute* */ void* ctx floor_unused) {
	log_error("vulkan error in layer %s: %u: %s", layer_prefix, message_code, message);
	return VK_FALSE; // don't abort
}
#endif

vulkan_compute::vulkan_compute(const bool enable_renderer_, const vector<string> whitelist) :
compute_context(), enable_renderer(enable_renderer_) {
	if(enable_renderer) {
		screen.x11_forwarding = floor::is_x11_forwarding();
	}
	
	// create a vulkan instance (context)
	const auto min_vulkan_api_version = floor::get_vulkan_api_version();
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
	vector<const char*> instance_extensions {
#if defined(FLOOR_DEBUG)
		VK_EXT_DEBUG_REPORT_EXTENSION_NAME,
#endif
		VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME,
	};
	if(enable_renderer && !screen.x11_forwarding) {
		instance_extensions.emplace_back(VK_KHR_SURFACE_EXTENSION_NAME);
#if defined(SDL_VIDEO_DRIVER_WINDOWS)
		instance_extensions.emplace_back(VK_KHR_WIN32_SURFACE_EXTENSION_NAME);
#elif defined(SDL_VIDEO_DRIVER_X11)
		// SDL only supports xlib
		instance_extensions.emplace_back(VK_KHR_XLIB_SURFACE_EXTENSION_NAME);
#endif

#if defined(SDL_VIDEO_DRIVER_WINDOWS) // seems to only exist on windows (and android) right now
		instance_extensions.emplace_back(VK_EXT_SWAPCHAIN_COLOR_SPACE_EXTENSION_NAME);
#endif
	}
	
	const vector<const char*> instance_layers {
#if defined(FLOOR_DEBUG)
		"VK_LAYER_LUNARG_standard_validation",
#endif
	};
	
	string inst_exts_str = "", inst_layers_str = "";
	for(const auto& ext : instance_extensions) {
		inst_exts_str += ext;
		inst_exts_str += " ";
	}
	for(const auto& layer : instance_layers) {
		inst_layers_str += layer;
		inst_layers_str += " ";
	}
	log_debug("using instance extensions: %s", inst_exts_str);
	log_debug("using instance layers: %s", inst_layers_str);

	const VkInstanceCreateInfo instance_info {
		.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
		.pNext = nullptr,
		.flags = 0,
		.pApplicationInfo = &app_info,
		.enabledLayerCount = (uint32_t)size(instance_layers),
		.ppEnabledLayerNames = size(instance_layers) > 0 ? instance_layers.data() : nullptr,
		.enabledExtensionCount = (uint32_t)size(instance_extensions),
		.ppEnabledExtensionNames = size(instance_extensions) > 0 ? instance_extensions.data() : nullptr,
	};
	VK_CALL_RET(vkCreateInstance(&instance_info, nullptr, &ctx), "failed to create vulkan instance")
	
#if defined(FLOOR_DEBUG)
	// register debug callback
	create_debug_report_callback = (PFN_vkCreateDebugReportCallbackEXT)vkGetInstanceProcAddr(ctx, "vkCreateDebugReportCallbackEXT");
	if(create_debug_report_callback == nullptr) {
		log_error("failed to retrieve vkCreateDebugReportCallbackEXT function pointer");
		return;
	}
	destroy_debug_report_callback = (PFN_vkDestroyDebugReportCallbackEXT)vkGetInstanceProcAddr(ctx, "vkDestroyDebugReportCallbackEXT");
	const VkDebugReportCallbackCreateInfoEXT debug_cb_info {
		.sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CREATE_INFO_EXT,
		.pNext = nullptr,
		.flags = (VK_DEBUG_REPORT_WARNING_BIT_EXT |
				  VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT |
				  VK_DEBUG_REPORT_ERROR_BIT_EXT
				  //| VK_DEBUG_REPORT_DEBUG_BIT_EXT
				  //| VK_DEBUG_REPORT_INFORMATION_BIT_EXT
				  ),
		.pfnCallback = &vulkan_debug_callback,
		.pUserData = this,
	};
	VK_CALL_RET(create_debug_report_callback(ctx, &debug_cb_info, nullptr, &debug_callback),
				"failed to register debug callback")
#endif
	
	// get layers
	uint32_t layer_count = 0;
	VK_CALL_RET(vkEnumerateInstanceLayerProperties(&layer_count, nullptr), "failed to retrieve instance layer properties count")
	vector<VkLayerProperties> layers(layer_count);
	if(layer_count > 0) {
		VK_CALL_RET(vkEnumerateInstanceLayerProperties(&layer_count, layers.data()), "failed to retrieve instance layer properties")
	}
	log_debug("found %u vulkan layer%s", layer_count, (layer_count == 1 ? "" : "s"));
	
	// get devices
	uint32_t device_count = 0;
	VK_CALL_RET(vkEnumeratePhysicalDevices(ctx, &device_count, nullptr), "failed to retrieve device count")
	vector<VkPhysicalDevice> queried_devices(device_count);
	VK_CALL_RET(vkEnumeratePhysicalDevices(ctx, &device_count, queried_devices.data()), "failed to retrieve devices")
	log_debug("found %u vulkan device%s", device_count, (device_count == 1 ? "" : "s"));

	auto gpu_counter = (underlying_type_t<compute_device::TYPE>)compute_device::TYPE::GPU0;
	auto cpu_counter = (underlying_type_t<compute_device::TYPE>)compute_device::TYPE::CPU0;
	for(const auto& phys_dev : queried_devices) {
		// get device props and features
		VkPhysicalDeviceProperties props;
		vkGetPhysicalDeviceProperties(phys_dev, &props);
		VkPhysicalDeviceFeatures features;
		vkGetPhysicalDeviceFeatures(phys_dev, &features);
		
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
		
		// devices must support int64
		if (!features.shaderInt64) {
			log_error("device %s does not support shaderInt64", props.deviceName);
			continue;
		}
		
		// handle device queue info + create queue info, we're going to create as many queues as are allowed by the device
		uint32_t queue_family_count = 0;
		vkGetPhysicalDeviceQueueFamilyProperties(phys_dev, &queue_family_count, nullptr);
		if(queue_family_count == 0) {
			log_error("device %s supports no queue families", props.deviceName);
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
			log_error("device %s supports no queues", props.deviceName);
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

		// add other required or optional extensions
		//device_extensions_set.emplace(VK_KHR_VULKAN_MEMORY_MODEL_EXTENSION_NAME); // NOTE: will be required in the future
		//device_extensions_set.emplace(VK_KHR_VARIABLE_POINTERS_EXTENSION_NAME); // NOTE: will be required in the future
		if (device_supported_extensions_set.count(VK_EXT_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME)) { // NOTE: will be required in the future
			device_extensions_set.emplace(VK_EXT_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME);
		}
		if (device_supported_extensions_set.count(VK_EXT_SCALAR_BLOCK_LAYOUT_EXTENSION_NAME)) { // NOTE: will be required in the future
			device_extensions_set.emplace(VK_EXT_SCALAR_BLOCK_LAYOUT_EXTENSION_NAME);
		}
		if (enable_renderer && !screen.x11_forwarding) {
			if (device_supported_extensions_set.count(VK_EXT_HDR_METADATA_EXTENSION_NAME)) {
				device_extensions_set.emplace(VK_EXT_HDR_METADATA_EXTENSION_NAME);
			}
		}

		// deal with swapchain ext
		auto swapchain_ext_iter = find(begin(device_extensions_set), end(device_extensions_set), VK_KHR_SWAPCHAIN_EXTENSION_NAME);
		if (enable_renderer && !screen.x11_forwarding) {
			if (swapchain_ext_iter == device_extensions_set.end()) {
				log_error("%s extension is not supported by the device", VK_KHR_SWAPCHAIN_EXTENSION_NAME);
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
		
		string dev_exts_str = "", dev_layers_str = "";
		for(const auto& ext : device_extensions) {
			dev_exts_str += ext;
			dev_exts_str += " ";
		}
		for(const auto& layer : device_layers) {
			dev_layers_str += layer;
			dev_layers_str += " ";
		}
		log_debug("using device extensions: %s", dev_exts_str);
		log_debug("using device layers: %s", dev_layers_str);
		
		vector<const char*> device_extensions_ptrs;
		for(const auto& ext : device_extensions) {
			device_extensions_ptrs.emplace_back(ext.c_str());
		}
		
		const VkDeviceCreateInfo dev_info {
			.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
			.pNext = nullptr,
			.flags = 0,
			.queueCreateInfoCount = queue_family_count,
			.pQueueCreateInfos = queue_create_info.data(),
			.enabledLayerCount = (uint32_t)size(device_layers),
			.ppEnabledLayerNames = size(device_layers) > 0 ? device_layers.data() : nullptr,
			.enabledExtensionCount = (uint32_t)size(device_extensions_ptrs),
			.ppEnabledExtensionNames = size(device_extensions_ptrs) > 0 ? device_extensions_ptrs.data() : nullptr,
			.pEnabledFeatures = &features // enable all that is supported
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
		device.platform_vendor = COMPUTE_VENDOR::KHRONOS; // not sure what to set here
		device.version_str = (to_string(VK_VERSION_MAJOR(props.apiVersion)) + "." +
							  to_string(VK_VERSION_MINOR(props.apiVersion)) + "." +
							  to_string(VK_VERSION_PATCH(props.apiVersion)));
		device.driver_version_str = to_string(props.driverVersion);
		device.extensions = device_extensions;
		
		// TODO: determine context/platform vulkan version
		device.vulkan_version = vulkan_version_from_uint(VK_VERSION_MAJOR(props.apiVersion), VK_VERSION_MINOR(props.apiVersion));
		if (device.vulkan_version == VULKAN_VERSION::VULKAN_1_0) {
			device.spirv_version = SPIRV_VERSION::SPIRV_1_0;
		} else if (device.vulkan_version >= VULKAN_VERSION::VULKAN_1_1) {
			// "A Vulkan 1.1 implementation must support the 1.0, 1.1, 1.2, and 1.3 versions of SPIR-V"
			device.spirv_version = SPIRV_VERSION::SPIRV_1_3;
		}
		
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
		log_debug("max img / mip: %v, %v, %v -> %u",
				  device.max_image_1d_dim, device.max_image_2d_dim, device.max_image_3d_dim,
				  device.max_mip_levels);
		
		device.image_msaa_array_support = features.shaderStorageImageMultisample;
		device.image_msaa_array_write_support = device.image_msaa_array_support;
		device.image_cube_array_support = features.imageCubeArray;
		device.image_cube_array_write_support = device.image_cube_array_support;
		
		device.anisotropic_support = features.samplerAnisotropy;
		device.max_anisotropy = (device.anisotropic_support ? limits.maxSamplerAnisotropy : 0.0f);
		
		device.int16_support = features.shaderInt16;
		//device.float16_support = features.shaderFloat16; // TODO: cap doesn't exist, but extension(s) do?
		device.double_support = features.shaderFloat64;

		// retrieve memory info
		device.mem_props = make_shared<VkPhysicalDeviceMemoryProperties>();
		vkGetPhysicalDeviceMemoryProperties(phys_dev, device.mem_props.get());
		
		// global memory (heap with local bit)
		// for now, just assume the correct data is stored in the heap flags
		auto& mem_props = *device.mem_props.get();
		for(uint32_t i = 0; i < mem_props.memoryHeapCount; ++i) {
			if(mem_props.memoryHeaps[i].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) {
				device.global_mem_size = mem_props.memoryHeaps[i].size;
				device.max_mem_alloc = mem_props.memoryHeaps[i].size; // TODO: min(gpu heap, host heap)?
				break;
			}
		}
		for(uint32_t i = 0; i < mem_props.memoryTypeCount; ++i) {
			// preferred index handling
			if(device.device_mem_index == ~0u &&
			   mem_props.memoryTypes[i].propertyFlags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) {
				device.device_mem_index = i;
				log_msg("using memory type #%u for device allocations", device.device_mem_index);
			}
			if(mem_props.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) {
				// we preferably want to allocate both cached and uncached host visible memory,
				// but if this isn't possible, just stick with the one that works at all
				if(mem_props.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_HOST_CACHED_BIT) {
					device.host_mem_cached_index = i;
				}
				else {
					device.host_mem_uncached_index = i;
				}
			}
			
			// handling of all available indices
			// NOTE: some drivers contain multiple entries of the same type and do actually require specific ones from that set
			if(mem_props.memoryTypes[i].propertyFlags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) {
				device.device_mem_indices.emplace(i);
			}
			if(mem_props.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) {
				if(mem_props.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_HOST_CACHED_BIT) {
					device.host_mem_cached_indices.emplace(i);
				}
				else {
					device.host_mem_uncached_indices.emplace(i);
				}
			}
		}
		if(device.device_mem_index == ~0u) {
			log_error("no device memory found");
		}
		if(device.host_mem_cached_index == ~0u &&
		   device.host_mem_uncached_index == ~0u) {
			log_error("no host-visible memory found");
		}
		else {
			// fallback if either isn't available (see above)
			if(device.host_mem_cached_index == ~0u) {
				device.host_mem_cached_index = device.host_mem_uncached_index;
			}
			else if(device.host_mem_uncached_index == ~0u) {
				device.host_mem_uncached_index = device.host_mem_cached_index;
			}
			log_msg("using memory type #%u for cached host-visible allocations", device.host_mem_cached_index);
			log_msg("using memory type #%u for uncached host-visible allocations", device.host_mem_uncached_index);
		}
		if(device.device_mem_index == device.host_mem_cached_index ||
		   device.device_mem_index == device.host_mem_uncached_index) {
			//device.unified_memory = true; // TODO: -> vulkan_memory.cpp
		}
		
		log_msg("max mem alloc: %u bytes / %u MB",
				device.max_mem_alloc,
				device.max_mem_alloc / 1024ULL / 1024ULL);
		log_msg("mem size: %u MB (global), %u KB (local), %u KB (constant)",
				device.global_mem_size / 1024ULL / 1024ULL,
				device.local_mem_size / 1024ULL,
				device.constant_mem_size / 1024ULL);
		
		log_msg("max total local size: %u", device.max_total_local_size);
		log_msg("max local size: %v", device.max_local_size);
		log_msg("max global size: %v", device.max_global_size);
		log_msg("max group size: %v", device.max_group_size);
		log_msg("queue families: %u", queue_family_count);
		log_msg("max queues (family #0): %u", device.queue_counts[0]);
		
		// TODO: other device flags
		// TODO: fastest device selection, tricky to do without a unit count
		
		// done
		log_debug("%s (Memory: %u MB): %s %s, API: %s, driver: %s",
				  (device.is_gpu() ? "GPU" : (device.is_cpu() ? "CPU" : "UNKNOWN")),
				  (uint32_t)(device.global_mem_size / 1024ull / 1024ull),
				  device.vendor_name,
				  device.name,
				  device.version_str,
				  device.driver_version_str);
	}
	
	// if there are no devices left, init has failed
	if(devices.empty()) {
		if(!queried_devices.empty()) log_warn("no devices left after applying whitelist!");
		return;
	}
	
	// already create command queues for all devices, these will serve as the default queues and the ones returned
	// when first calling create_queue for a device (a second call will then create an actual new one)
	for(auto& dev : devices) {
		default_queues.insert(*dev, create_queue(*dev));
		
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
	if(destroy_debug_report_callback != nullptr &&
	   debug_callback != nullptr) {
		destroy_debug_report_callback(ctx, debug_callback, nullptr);
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
		
		screen.swapchain_images.emplace_back(screen.x11_screen->get_vulkan_image());
		screen.swapchain_image_views.emplace_back(screen.x11_screen->get_vulkan_image_view());
		
		return true;
	}
	
	// query SDL window / video driver info that we need to create a vulkan surface
	SDL_SysWMinfo wm_info;
	SDL_VERSION(&wm_info.version)
	if(!SDL_GetWindowWMInfo(floor::get_window(), &wm_info)) {
		log_error("failed to retrieve window info: %s", SDL_GetError());
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
	
	// query formats and try to use VK_FORMAT_B8G8R8A8_UNORM if possible
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
	screen.format = formats[0].format;
	screen.color_space = formats[0].colorSpace;
	for(const auto& format : formats) {
		// use VK_FORMAT_B8G8R8A8_UNORM if we can
		if(format.format == VK_FORMAT_B8G8R8A8_UNORM) {
			screen.format = VK_FORMAT_B8G8R8A8_UNORM;
			screen.color_space = format.colorSpace;
			break;
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
	}
	
	return true;
#else
	log_error("unsupported video driver");
	return false;
#endif
}

pair<bool, vulkan_compute::drawable_image_info> vulkan_compute::acquire_next_image() {
	const auto& dev_queue = *get_device_default_queue(*screen.render_device);
	const auto& vk_queue = (const vulkan_queue&)dev_queue;
	
	if(screen.x11_forwarding) {
		screen.x11_screen->transition(dev_queue, nullptr /* create a cmd buffer */,
									  VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
									  VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT);
		return {
			true,
			{
				.index = 0u,
				.image_size = screen.size,
				.image = screen.x11_screen->get_vulkan_image(),
			}
		};
	}
	
	const drawable_image_info dummy_ret {
		.index = ~0u,
		.image_size = 0u,
		.image = nullptr,
	};
	
	// create new sema and acquire image
	const VkSemaphoreCreateInfo sema_create_info {
		.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
		.pNext = nullptr,
		.flags = 0,
	};
	VkSemaphore sema { nullptr };
	VK_CALL_RET(vkCreateSemaphore(screen.render_device->device, &sema_create_info, nullptr, &sema),
				"failed to create semaphore", { false, dummy_ret })
	
	VK_CALL_RET(vkAcquireNextImageKHR(screen.render_device->device, screen.swapchain, UINT64_MAX, sema,
									  nullptr, &screen.image_index),
				"failed to acquire next presentable image", { false, dummy_ret })
	screen.render_semas[screen.image_index] = sema;
	
	// transition image
	auto cmd_buffer = vk_queue.make_command_buffer("image drawable transition");
	const VkCommandBufferBeginInfo begin_info {
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
		.pNext = nullptr,
		.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
		.pInheritanceInfo = nullptr,
	};
	VK_CALL_RET(vkBeginCommandBuffer(cmd_buffer.cmd_buffer, &begin_info),
				"failed to begin command buffer", { false, dummy_ret })
	
	const VkImageMemoryBarrier image_barrier {
		.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
		.pNext = nullptr,
		.srcAccessMask = 0,
		.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
		.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
		.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
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
	
	VK_CALL_RET(vkEndCommandBuffer(cmd_buffer.cmd_buffer),
				"failed to end command buffer", { false, dummy_ret })
	vk_queue.submit_command_buffer(cmd_buffer, true, // TODO: don't block?
								   &screen.render_semas[screen.image_index], 1,
								   VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
	
	return {
		true,
		{
			.index = screen.image_index,
			.image_size = screen.size,
			.image = screen.swapchain_images[screen.image_index],
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
	auto cmd_buffer = vk_queue.make_command_buffer("image present transition");
	const VkCommandBufferBeginInfo begin_info {
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
		.pNext = nullptr,
		.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
		.pInheritanceInfo = nullptr,
	};
	VK_CALL_RET(vkBeginCommandBuffer(cmd_buffer.cmd_buffer, &begin_info),
				"failed to begin command buffer", false)
	
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
	
	VK_CALL_RET(vkEndCommandBuffer(cmd_buffer.cmd_buffer),
				"failed to end command buffer", false)
	vk_queue.submit_command_buffer(cmd_buffer, true); // TODO: don't block?
	
	// present
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
	VK_CALL_RET(vkQueuePresentKHR((VkQueue)const_cast<void*>(vk_queue.get_queue_ptr()), &present_info),
				"failed to present", false)
	
	// cleanup
	vkDestroySemaphore(screen.render_device->device, screen.render_semas[drawable.index], nullptr);
	screen.render_semas[drawable.index] = nullptr;
	
	return true;
}

shared_ptr<compute_queue> vulkan_compute::create_queue(const compute_device& dev) const {
	const auto& vulkan_dev = (const vulkan_device&)dev;
	
	// can only create a certain amount of queues per device with vulkan, so handle this + handle the queue index
	if(vulkan_dev.cur_queue_idx >= vulkan_dev.queue_counts[0]) {
		log_warn("too many queues were created (max: %u), wrapping around to #0 again", vulkan_dev.queue_counts[0]);
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

shared_ptr<compute_buffer> vulkan_compute::create_buffer(const compute_queue& cqueue,
														 const size_t& size, const COMPUTE_MEMORY_FLAG flags,
														 const uint32_t opengl_type) const {
	return make_shared<vulkan_buffer>(cqueue, size, flags, opengl_type);
}

shared_ptr<compute_buffer> vulkan_compute::create_buffer(const compute_queue& cqueue,
														 const size_t& size, void* data,
														 const COMPUTE_MEMORY_FLAG flags,
														 const uint32_t opengl_type) const {
	return make_shared<vulkan_buffer>(cqueue, size, data, flags, opengl_type);
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
	return make_shared<vulkan_image>(cqueue, image_dim, image_type, nullptr, flags, opengl_type);
}

shared_ptr<compute_image> vulkan_compute::create_image(const compute_queue& cqueue,
													   const uint4 image_dim,
													   const COMPUTE_IMAGE_TYPE image_type,
													   void* data,
													   const COMPUTE_MEMORY_FLAG flags,
													   const uint32_t opengl_type) const {
	return make_shared<vulkan_image>(cqueue, image_dim, image_type, data, flags, opengl_type);
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
		log_error("failed to load universal binary: %s", file_name);
		return {};
	}
	
	// create the program
	vulkan_program::program_map_type prog_map;
	prog_map.reserve(devices.size());
	for (size_t i = 0, dev_count = devices.size(); i < dev_count; ++i) {
		const auto& vlk_dev = (const vulkan_device&)devices[i];
		const auto& dev_best_bin = bins.dev_binaries[i];
		const auto func_info = universal_binary::translate_function_info(dev_best_bin.first->functions);
		
		auto container = spirv_handler::load_container_from_memory(dev_best_bin.first->data.data(),
																   dev_best_bin.first->data.size(),
																   file_name);
		if(!container.valid) return {}; // already prints an error
		
		prog_map.insert_or_assign(vlk_dev,
								  create_vulkan_program_internal(vlk_dev, container, func_info, file_name));
	}
	
	return add_program(move(prog_map));
}

shared_ptr<vulkan_program> vulkan_compute::add_program(vulkan_program::program_map_type&& prog_map) {
	// create the program object, which in turn will create kernel objects for all kernel functions in the program,
	// for all devices contained in the program map
	auto prog = make_shared<vulkan_program>(move(prog_map));
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
	return add_program(move(prog_map));
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
	return add_program(move(prog_map));
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
		VK_CALL_CONT(vkCreateShaderModule(((const vulkan_device&)dev).device, &module_info, nullptr, &module),
					 "failed to create shader module (\"" + file_name + "\") for device \"" + dev->name + "\"")
		entry.programs.emplace_back(module);
		entry.valid = true;
		
		prog_map.insert_or_assign((const vulkan_device&)*dev, entry);
	}
	return add_program(move(prog_map));
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
	
	for(auto& dev : devices) {
		auto& vk_dev = (vulkan_device&)*dev;
		vk_dev.fixed_sampler_set.resize(max_combinations, nullptr);
		vk_dev.fixed_sampler_image_info.resize(max_combinations,
											   VkDescriptorImageInfo { nullptr, nullptr, VK_IMAGE_LAYOUT_UNDEFINED });
	}

	// create the samplers for all devices
	for(uint32_t combination = 0; combination < max_combinations; ++combination) {
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
		
		for(auto& dev : devices) {
			auto& vk_dev = (vulkan_device&)*dev;
			if(sampler_create_info.anisotropyEnable) {
				sampler_create_info.anisotropyEnable = vk_dev.anisotropic_support;
				sampler_create_info.maxAnisotropy = vk_dev.max_anisotropy;
			}
			
			VK_CALL_CONT(vkCreateSampler(vk_dev.device, &sampler_create_info, nullptr, &vk_dev.fixed_sampler_set[combination]),
						 "failed to create sampler (#" + to_string(combination) + ")")
			
			vk_dev.fixed_sampler_image_info[combination].sampler = vk_dev.fixed_sampler_set[combination];
		}
	}
	
	// create the descriptor set for all devices
	VkDescriptorSetLayoutBinding fixed_samplers_desc_set_layout {
		.binding = 0,
		.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER,
		.descriptorCount = max_combinations,
		.stageFlags = VK_SHADER_STAGE_ALL,
		.pImmutableSamplers = nullptr,
	};
	const VkDescriptorSetLayoutCreateInfo desc_set_layout_info {
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
		.pNext = nullptr,
		.flags = 0,
		.bindingCount = 1,
		.pBindings = &fixed_samplers_desc_set_layout,
	};
	for(auto& dev : devices) {
		auto& vk_dev = (vulkan_device&)*dev;
		fixed_samplers_desc_set_layout.pImmutableSamplers = vk_dev.fixed_sampler_set.data();
		VK_CALL_CONT(vkCreateDescriptorSetLayout(vk_dev.device, &desc_set_layout_info, nullptr,
												 &vk_dev.fixed_sampler_desc_set_layout),
					 "failed to create fixed sampler set descriptor set layout")
		
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
		VK_CALL_CONT(vkCreateDescriptorPool(vk_dev.device, &desc_pool_info, nullptr, &vk_dev.fixed_sampler_desc_pool),
					 "failed to create fixed sampler set descriptor pool")
		
		// allocate descriptor set
		const VkDescriptorSetAllocateInfo desc_set_alloc_info {
			.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
			.pNext = nullptr,
			.descriptorPool = vk_dev.fixed_sampler_desc_pool,
			.descriptorSetCount = 1,
			.pSetLayouts = &vk_dev.fixed_sampler_desc_set_layout,
		};
		VK_CALL_CONT(vkAllocateDescriptorSets(vk_dev.device, &desc_set_alloc_info, &vk_dev.fixed_sampler_desc_set),
					 "failed to allocate fixed sampler set descriptor set")
	}
	
	// TODO: cleanup!
}

#endif
