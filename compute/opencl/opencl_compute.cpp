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

#include <floor/core/essentials.hpp>

#if !defined(FLOOR_NO_OPENCL)
#define FLOOR_OPENCL_INFO_FUNCS 1
#include <floor/compute/opencl/opencl_compute.hpp>
#include <floor/core/platform.hpp>
#include <floor/core/gl_support.hpp>
#include <floor/core/logger.hpp>
#include <floor/core/core.hpp>
#include <floor/core/file_io.hpp>

#if defined(__APPLE__)
#include <floor/darwin/darwin_helper.hpp>
#endif

#include <floor/compute/llvm_compute.hpp>
#include <floor/floor/floor.hpp>

opencl_compute::opencl_compute(const uint64_t platform_index_,
							   const bool gl_sharing,
							   const vector<string> whitelist) : compute_context() {
	// if no platform was specified, use the one in the config (or default one, which is 0)
	const auto platform_index = (platform_index_ == ~0u ? floor::get_opencl_platform() : platform_index_);
	
	// get platforms
	cl_uint platform_count = 0;
	CL_CALL_RET(clGetPlatformIDs(0, nullptr, &platform_count), "failed to get platform count")
	vector<cl_platform_id> platforms(platform_count);
	CL_CALL_RET(clGetPlatformIDs(platform_count, platforms.data(), nullptr), "failed to get platforms")
	
	// check if there are any platforms at all
	if(platforms.empty()) {
		log_error("no opencl platforms found!");
		return;
	}
	log_debug("found %u opencl platform%s", platforms.size(), (platforms.size() == 1 ? "" : "s"));
	
	// go through all platforms, starting with the user-specified one
	size_t first_platform_index = platform_index;
	if(platform_index >= platforms.size()) {
		log_warn("invalid platform index \"%s\" - starting at 0 instead!", platform_index);
		first_platform_index = 0;
	}
	
	for(size_t i = 0, p_idx = first_platform_index, p_count = platforms.size() + 1; i < p_count; ++i, p_idx = i - 1) {
		// skip already checked platform
		if(i > 0 && first_platform_index == p_idx) continue;
		const auto& platform = platforms[p_idx];
		log_debug("checking opencl platform #%u \"%s\" ...",
				  p_idx, cl_get_info<CL_PLATFORM_NAME>(platform));
		
		// get devices
		cl_uint all_device_count = 0;
		CL_CALL_CONT(clGetDeviceIDs(platform, CL_DEVICE_TYPE_ALL, 0, nullptr, &all_device_count),
					 "failed to get device count for platform")
		vector<cl_device_id> all_cl_devices(all_device_count);
		CL_CALL_CONT(clGetDeviceIDs(platform, CL_DEVICE_TYPE_ALL, all_device_count, all_cl_devices.data(), nullptr),
					 "failed to get devices for platform")
		
		log_debug("found %u opencl device%s", all_cl_devices.size(), (all_cl_devices.size() == 1 ? "" : "s"));
		
		// device whitelist
		vector<cl_device_id> ctx_cl_devices;
		if(!whitelist.empty()) {
			for(const auto& cl_dev : all_cl_devices) {
				// check type
				const auto dev_type = cl_get_info<CL_DEVICE_TYPE>(cl_dev);
				switch(dev_type) {
					case CL_DEVICE_TYPE_CPU:
						if(find(begin(whitelist), end(whitelist), "cpu") != end(whitelist)) {
							ctx_cl_devices.emplace_back(cl_dev);
							continue;
						}
						break;
					case CL_DEVICE_TYPE_GPU:
						if(find(begin(whitelist), end(whitelist), "gpu") != end(whitelist)) {
							ctx_cl_devices.emplace_back(cl_dev);
							continue;
						}
						break;
					case CL_DEVICE_TYPE_ACCELERATOR:
						if(find(begin(whitelist), end(whitelist), "accelerator") != end(whitelist)) {
							ctx_cl_devices.emplace_back(cl_dev);
							continue;
						}
						break;
					default: break;
				}
				
				// check name
				const auto dev_name = core::str_to_lower(cl_get_info<CL_DEVICE_NAME>(cl_dev));
				for(const auto& entry : whitelist) {
					if(dev_name.find(entry) != string::npos) {
						ctx_cl_devices.emplace_back(cl_dev);
						break;
					}
				}
			}
		}
		else ctx_cl_devices = all_cl_devices;
		
		if(ctx_cl_devices.empty()) {
			if(!all_cl_devices.empty()) log_warn("no devices left after applying whitelist!");
			continue;
		}
		
		//
#if defined(__APPLE__)
		platform_vendor = COMPUTE_VENDOR::APPLE;
		
		// drop all devices when using gl sharing (not adding cpu devices here, because this would cause other issues)
		if(gl_sharing) {
			ctx_cl_devices.clear();
		}
		
		cl_context_properties cl_properties[] {
			CL_CONTEXT_PLATFORM, (cl_context_properties)platform,
			gl_sharing ? CL_CONTEXT_PROPERTY_USE_CGL_SHAREGROUP_APPLE : 0,
#if !defined(FLOOR_IOS)
			gl_sharing ? (cl_context_properties)CGLGetShareGroup(CGLGetCurrentContext()) : 0,
#else
			gl_sharing ? (cl_context_properties)darwin_helper::get_eagl_sharegroup() : 0,
#endif
			0
		};
		
		CL_CALL_ERR_PARAM_CONT(ctx = clCreateContext(cl_properties, (cl_uint)ctx_cl_devices.size(),
													 (!ctx_cl_devices.empty() ? ctx_cl_devices.data() : nullptr),
													 clLogMessagesToStdoutAPPLE, nullptr, &ctx_error),
							   ctx_error, "failed to create opencl context")
		
#else
		// context with gl share group (cl/gl interop)
#if defined(__WINDOWS__)
		cl_context_properties cl_properties[] {
			CL_CONTEXT_PLATFORM, (cl_context_properties)platform,
			gl_sharing ? CL_GL_CONTEXT_KHR : 0,
			gl_sharing ? (cl_context_properties)wglGetCurrentContext() : 0,
			gl_sharing ? CL_WGL_HDC_KHR : 0,
			gl_sharing ? (cl_context_properties)wglGetCurrentDC() : 0,
			0
		};
#else // Linux and *BSD
		cl_context_properties cl_properties[] {
			CL_CONTEXT_PLATFORM, (cl_context_properties)platform,
			gl_sharing ? CL_GL_CONTEXT_KHR : 0,
			gl_sharing ? (cl_context_properties)glXGetCurrentContext() : 0,
			gl_sharing ? CL_GLX_DISPLAY_KHR : 0,
			gl_sharing ? (cl_context_properties)glXGetCurrentDisplay() : 0,
			0
		};
#endif
		
		CL_CALL_ERR_PARAM_CONT(ctx = clCreateContext(cl_properties, (cl_uint)ctx_cl_devices.size(), ctx_cl_devices.data(),
													 nullptr, nullptr, &ctx_error),
							   ctx_error, "failed to create opencl context")
#endif
		
		// success
		log_debug("created opencl context on platform \"%s\"!",
				  cl_get_info<CL_PLATFORM_NAME>(platform));
		log_msg("platform vendor: \"%s\"", cl_get_info<CL_PLATFORM_VENDOR>(platform));
		log_msg("platform version: \"%s\"", cl_get_info<CL_PLATFORM_VERSION>(platform));
		log_msg("platform profile: \"%s\"", cl_get_info<CL_PLATFORM_PROFILE>(platform));
		log_msg("platform extensions: \"%s\"", core::trim(cl_get_info<CL_PLATFORM_EXTENSIONS>(platform)));
		
#if !defined(__APPLE__)
		// get platform vendor
		const string platform_vendor_str = core::str_to_lower(cl_get_info<CL_PLATFORM_VENDOR>(platform));
		if(platform_vendor_str.find("nvidia") != string::npos) {
			platform_vendor = COMPUTE_VENDOR::NVIDIA;
		}
		else if(platform_vendor_str.find("amd") != string::npos ||
				platform_vendor_str.find("advanced micro devices") != string::npos) {
			platform_vendor = COMPUTE_VENDOR::AMD;
		}
		else if(platform_vendor_str.find("intel") != string::npos) {
			platform_vendor = COMPUTE_VENDOR::INTEL;
		}
#endif
		
		//
		const auto extract_cl_version = [](const string& cl_version_str, const string str_start) -> pair<bool, OPENCL_VERSION> {
			// "OpenCL X.Y" or "OpenCL C X.Y" required by spec (str_start must be either)
			const size_t start_len = str_start.length();
			if(cl_version_str.length() >= (start_len + 3) && cl_version_str.substr(0, start_len) == str_start) {
				const string version_str = cl_version_str.substr(start_len, cl_version_str.find(" ", start_len) - start_len);
				const size_t dot_pos = version_str.find(".");
				const auto major_version = stosize(version_str.substr(0, dot_pos));
				const auto minor_version = stosize(version_str.substr(dot_pos+1, version_str.length()-dot_pos-1));
				if(major_version > 2) {
					// major version is higher than 2 -> pretend we're running on CL 2.1
					return { true, OPENCL_VERSION::OPENCL_2_1 };
				}
				else if(major_version == 2) {
					switch(minor_version) {
						case 0: return { true, OPENCL_VERSION::OPENCL_2_0 };
						case 1:
						default: // default to CL 2.1
							return { true, OPENCL_VERSION::OPENCL_2_1 };
					}
				}
				else {
					switch(minor_version) {
						case 0: return { true, OPENCL_VERSION::OPENCL_1_0 };
						case 1: return { true, OPENCL_VERSION::OPENCL_1_1 };
						case 2:
						default: // default to CL 1.2
							return { true, OPENCL_VERSION::OPENCL_1_2 };
					}
				}
			}
			return { false, OPENCL_VERSION::OPENCL_1_0 };
		};
		
		// get platform cl version
		const string cl_version_str = cl_get_info<CL_PLATFORM_VERSION>(platform);
		const auto extracted_cl_version = extract_cl_version(cl_version_str, "OpenCL "); // "OpenCL X.Y" required by spec
		if(!extracted_cl_version.first) {
			log_error("invalid opencl platform version string: %s", cl_version_str);
		}
		platform_cl_version = extracted_cl_version.second;
		
		// pocl only identifies itself in the platform version string, not the vendor string
		if(cl_version_str.find("pocl") != string::npos) {
			platform_vendor = COMPUTE_VENDOR::POCL;
		}
		
		//
		log_msg("opencl platform \"%s\" version recognized as CL%s",
				  compute_vendor_to_string(platform_vendor),
				  (platform_cl_version == OPENCL_VERSION::OPENCL_1_0 ? "1.0" :
				   (platform_cl_version == OPENCL_VERSION::OPENCL_1_1 ? "1.1" :
					(platform_cl_version == OPENCL_VERSION::OPENCL_1_2 ? "1.2" :
					 (platform_cl_version == OPENCL_VERSION::OPENCL_2_0 ? "2.0" : "2.1")))));
		
		// handle device init
		ctx_devices.clear();
		ctx_devices = cl_get_info<CL_CONTEXT_DEVICES>(ctx);
		log_debug("found %u opencl device%s in context", ctx_devices.size(), (ctx_devices.size() == 1 ? "" : "s"));
		
		string dev_type_str;
		unsigned int gpu_counter = (unsigned int)compute_device::TYPE::GPU0;
		unsigned int cpu_counter = (unsigned int)compute_device::TYPE::CPU0;
		unsigned int fastest_cpu_score = 0;
		unsigned int fastest_gpu_score = 0;
		unsigned int cpu_score = 0;
		unsigned int gpu_score = 0;
		fastest_cpu_device = nullptr;
		fastest_gpu_device = nullptr;
		
		devices.clear();
		for(size_t j = 0; j < ctx_devices.size(); ++j) {
			auto& cl_dev = ctx_devices[j];
			
			auto device = make_shared<opencl_device>();
			devices.emplace_back(device);
			dev_type_str = "";
			
			device->ctx = ctx;
			device->compute_ctx = this;
			device->device_id = cl_dev;
			device->internal_type = (uint32_t)cl_get_info<CL_DEVICE_TYPE>(cl_dev);
			device->units = cl_get_info<CL_DEVICE_MAX_COMPUTE_UNITS>(cl_dev);
			device->clock = cl_get_info<CL_DEVICE_MAX_CLOCK_FREQUENCY>(cl_dev);
			device->global_mem_size = cl_get_info<CL_DEVICE_GLOBAL_MEM_SIZE>(cl_dev);
			device->local_mem_size = cl_get_info<CL_DEVICE_LOCAL_MEM_SIZE>(cl_dev);
			device->local_mem_dedicated = (cl_get_info<CL_DEVICE_LOCAL_MEM_TYPE>(cl_dev) == CL_LOCAL);
			device->constant_mem_size = cl_get_info<CL_DEVICE_MAX_CONSTANT_BUFFER_SIZE>(cl_dev);
			device->name = cl_get_info<CL_DEVICE_NAME>(cl_dev);
			device->vendor_name = cl_get_info<CL_DEVICE_VENDOR>(cl_dev);
			device->version_str = cl_get_info<CL_DEVICE_VERSION>(cl_dev);
			device->cl_version = extract_cl_version(device->version_str, "OpenCL ").second;
			device->driver_version_str = cl_get_info<CL_DRIVER_VERSION>(cl_dev);
			device->extensions = core::tokenize(core::trim(cl_get_info<CL_DEVICE_EXTENSIONS>(cl_dev)), ' ');
			
			device->max_mem_alloc = cl_get_info<CL_DEVICE_MAX_MEM_ALLOC_SIZE>(cl_dev);
			device->max_work_group_size = (uint32_t)cl_get_info<CL_DEVICE_MAX_WORK_GROUP_SIZE>(cl_dev);
			const auto max_work_group_item_sizes = cl_get_info<CL_DEVICE_MAX_WORK_ITEM_SIZES>(cl_dev);
			if(max_work_group_item_sizes.size() != 3) {
				log_warn("max workgroup sizes dim != 3: %u", max_work_group_item_sizes.size());
			}
			if(max_work_group_item_sizes.size() >= 1) device->max_work_group_item_sizes.x = (uint32_t)max_work_group_item_sizes[0];
			if(max_work_group_item_sizes.size() >= 2) device->max_work_group_item_sizes.y = (uint32_t)max_work_group_item_sizes[1];
			if(max_work_group_item_sizes.size() >= 3) device->max_work_group_item_sizes.z = (uint32_t)max_work_group_item_sizes[2];
			
			// for cpu devices: assume this is the host cpu and compute the simd-width dependent on that
			if(device->internal_type & CL_DEVICE_TYPE_CPU) {
				// always at least 4 (SSE, newer NEON), 8-wide if avx/avx2, 16-wide if avx-512
				device->simd_width = (core::cpu_has_avx() ? (core::cpu_has_avx512() ? 16 : 8) : 4);
			}
			
			if(device->internal_type == CL_DEVICE_TYPE_CPU) {
#if defined(__APPLE__)
				// apple is doing weird stuff again -> if device is a cpu, divide wg/item sizes by (at least) 8
				const auto size_div = std::max(8u, device->units); // might be cpu/unit count, don't have the h/w to test this -> assume at least 8
				if(device->max_work_group_size > size_div) device->max_work_group_size /= size_div;
				if(device->max_work_group_item_sizes.x > size_div) device->max_work_group_item_sizes.x /= size_div;
				if(device->max_work_group_item_sizes.y > size_div) device->max_work_group_item_sizes.y /= size_div;
				if(device->max_work_group_item_sizes.z > size_div) device->max_work_group_item_sizes.z /= size_div;
#else
				// intel cpu is reporting 8192, but this isn't actually working on SSE cpus (unsure about avx, so leaving it for now)
				// -> set it to 4096 as it is actually working
				if(platform_vendor == COMPUTE_VENDOR::INTEL &&
				   device->simd_width == 4 &&
				   device->max_work_group_size >= 8192) {
					device->max_work_group_size = 4096;
					device->max_work_group_item_sizes.min(device->max_work_group_size);
				}
#endif
			}
			
			device->image_support = (cl_get_info<CL_DEVICE_IMAGE_SUPPORT>(cl_dev) == 1);
			device->image_depth_support = core::contains(device->extensions, "cl_khr_depth_images");
			device->image_depth_write_support = device->image_depth_support;
			device->image_msaa_support = core::contains(device->extensions, "cl_khr_gl_msaa_sharing");
			device->image_msaa_write_support = false; // always false
			device->image_cube_support = false; // nope
			device->image_cube_write_support = false;
			device->image_mipmap_support = core::contains(device->extensions, "cl_khr_mipmap_image");
			device->image_mipmap_write_support = core::contains(device->extensions, "cl_khr_mipmap_image_writes");
			
			device->max_image_1d_buffer_dim = cl_get_info<CL_DEVICE_IMAGE_MAX_BUFFER_SIZE>(cl_dev);
			device->max_image_1d_dim = cl_get_info<CL_DEVICE_IMAGE2D_MAX_WIDTH>(cl_dev);
			device->max_image_2d_dim.set(cl_get_info<CL_DEVICE_IMAGE2D_MAX_WIDTH>(cl_dev),
										 cl_get_info<CL_DEVICE_IMAGE2D_MAX_HEIGHT>(cl_dev));
			device->max_image_3d_dim.set(cl_get_info<CL_DEVICE_IMAGE3D_MAX_WIDTH>(cl_dev),
										 cl_get_info<CL_DEVICE_IMAGE3D_MAX_HEIGHT>(cl_dev),
										 cl_get_info<CL_DEVICE_IMAGE3D_MAX_DEPTH>(cl_dev));
#if !defined(__APPLE__)
			device->double_support = (cl_get_info<CL_DEVICE_DOUBLE_FP_CONFIG>(cl_dev) != 0);
#else
			// not properly supported on os x (defined in the headers, but no backend implementation)
			device->double_support = false;
#endif
			device->bitness = cl_get_info<CL_DEVICE_ADDRESS_BITS>(cl_dev);
			device->max_work_item_sizes = (device->bitness == 32 ? // range: sizeof(size_t) -> clEnqueueNDRangeKernel
										   0xFFFF'FFFFull :
										   (device->bitness == 64 ?
											0xFFFF'FFFF'FFFF'FFFFull :
											(1ull << uint64_t(device->bitness)) - 1ull)); // just in case "address bits" is something weird
			device->unified_memory = (cl_get_info<CL_DEVICE_HOST_UNIFIED_MEMORY>(cl_dev) == 1);
			device->basic_64_bit_atomics_support = core::contains(device->extensions, "cl_khr_int64_base_atomics");
			device->extended_64_bit_atomics_support = core::contains(device->extensions, "cl_khr_int64_extended_atomics");
			device->sub_group_support = (core::contains(device->extensions, "cl_khr_subgroups") ||
										 core::contains(device->extensions, "cl_intel_subgroups") ||
										 (device->cl_version >= OPENCL_VERSION::OPENCL_2_1 && platform_cl_version >= OPENCL_VERSION::OPENCL_2_1));
			
			log_msg("address space size: %u", device->bitness);
			log_msg("max mem alloc: %u bytes / %u MB",
					device->max_mem_alloc,
					device->max_mem_alloc / 1024ULL / 1024ULL);
			log_msg("mem size: %u MB (global), %u KB (local), %u KB (constant)",
					device->global_mem_size / 1024ULL / 1024ULL,
					device->local_mem_size / 1024ULL,
					device->constant_mem_size / 1024ULL);
			log_msg("mem base address alignment: %u", cl_get_info<CL_DEVICE_MEM_BASE_ADDR_ALIGN>(cl_dev));
			log_msg("min data type alignment size: %u", cl_get_info<CL_DEVICE_MIN_DATA_TYPE_ALIGN_SIZE>(cl_dev));
			log_msg("host unified memory: %u", device->unified_memory);
			log_msg("max work-group size: %u", device->max_work_group_size);
			log_msg("max work-group item sizes: %v", device->max_work_group_item_sizes);
			log_msg("max work-item sizes: %v", device->max_work_item_sizes);
			log_msg("max param size: %u", cl_get_info<CL_DEVICE_MAX_PARAMETER_SIZE>(cl_dev));
			log_msg("double support: %b", device->double_support);
			log_msg("image support: %b", device->image_support);
			if(// pocl has no support for this yet
			   platform_vendor != COMPUTE_VENDOR::POCL) {
				const unsigned long long int printf_buffer_size = cl_get_info<CL_DEVICE_PRINTF_BUFFER_SIZE>(cl_dev);
				log_msg("printf buffer size: %u bytes / %u MB",
						printf_buffer_size,
						printf_buffer_size / 1024ULL / 1024ULL);
				log_msg("max sub-devices: %u", cl_get_info<CL_DEVICE_PARTITION_MAX_SUB_DEVICES>(cl_dev));
				log_msg("built-in kernels: %s", cl_get_info<CL_DEVICE_BUILT_IN_KERNELS>(cl_dev));
			}
			log_msg("extensions: \"%s\"", core::trim(cl_get_info<CL_DEVICE_EXTENSIONS>(cl_dev)));
			
			device->platform_vendor = platform_vendor;
			device->vendor = COMPUTE_VENDOR::UNKNOWN;
			string vendor_str = core::str_to_lower(device->vendor_name);
			if(strstr(vendor_str.c_str(), "nvidia") != nullptr) {
				device->vendor = COMPUTE_VENDOR::NVIDIA;
				device->simd_width = 32;
				device->simd_range = { device->simd_width, device->simd_width };
			}
			else if(strstr(vendor_str.c_str(), "intel") != nullptr) {
				device->vendor = COMPUTE_VENDOR::INTEL;
				device->simd_width = 16; // variable (8, 16 or 32), but 16 is a good estimate
				device->simd_range = { 8, 32 };
				// -> cpu simd width later
			}
			else if(strstr(vendor_str.c_str(), "apple") != nullptr) {
				device->vendor = COMPUTE_VENDOR::APPLE;
				// -> cpu simd width later
			}
			else if(strstr(vendor_str.c_str(), "amd") != nullptr ||
					strstr(vendor_str.c_str(), "advanced micro devices") != nullptr ||
					// "ati" should be tested last, since it also matches "corporation"
					strstr(vendor_str.c_str(), "ati") != nullptr) {
				device->vendor = COMPUTE_VENDOR::AMD;
				device->simd_width = 64;
				device->simd_range = { device->simd_width, device->simd_width };
				// -> cpu simd width later
			}
			
			// older pocl versions used an empty device name, but "pocl" is also contained in the device version
			if(device->version_str.find("pocl") != string::npos) {
				device->vendor = COMPUTE_VENDOR::POCL;
				
				// device unit count on pocl is 0 -> figure out how many logical cpus actually exist
				if(device->units == 0) {
					device->units = core::get_hw_thread_count();
				}
			}
			
			if(device->internal_type & CL_DEVICE_TYPE_CPU) {
				device->type = (compute_device::TYPE)cpu_counter;
				cpu_counter++;
				dev_type_str += "CPU ";
			}
			if(device->internal_type & CL_DEVICE_TYPE_GPU) {
				device->type = (compute_device::TYPE)gpu_counter;
				gpu_counter++;
				dev_type_str += "GPU ";
			}
			if(device->internal_type & CL_DEVICE_TYPE_ACCELERATOR) {
				dev_type_str += "Accelerator ";
			}
			if(device->internal_type & CL_DEVICE_TYPE_DEFAULT) {
				dev_type_str += "Default ";
			}
			
			const string cl_c_version_str = cl_get_info<CL_DEVICE_OPENCL_C_VERSION>(cl_dev);
			const auto extracted_cl_c_version = extract_cl_version(cl_c_version_str, "OpenCL C "); // "OpenCL C X.Y" required by spec
			if(!extracted_cl_c_version.first) {
				log_error("invalid opencl c version string: %s", cl_c_version_str);
			}
			device->c_version = extracted_cl_c_version.second;
			
			// there is no spir support on apple platforms, so don't even try this
			// also, pocl doesn't support, but can apparently handle llvm bitcode files
#if !defined(__APPLE__)
			if(!core::contains(device->extensions, "cl_khr_spir") &&
			   device->vendor != COMPUTE_VENDOR::POCL) {
				log_error("device \"%s\" does not support \"cl_khr_spir\", removing it!", device->name);
				devices.pop_back();
				continue;
			}
			log_msg("spir versions: %s", cl_get_info<CL_DEVICE_SPIR_VERSIONS>(cl_dev));
#endif
			
			//
			log_debug("%s(Units: %u, Clock: %u MHz, Memory: %u MB): %s %s, %s / %s / %s",
					  dev_type_str,
					  device->units,
					  device->clock,
					  (unsigned int)(device->global_mem_size / 1024ull / 1024ull),
					  device->vendor_name,
					  device->name,
					  device->version_str,
					  device->driver_version_str,
					  cl_c_version_str);
			
			// compute score and try to figure out which device is the fastest
			if(device->internal_type & CL_DEVICE_TYPE_CPU) {
				if(fastest_cpu_device == nullptr) {
					fastest_cpu_device = device;
					fastest_cpu_score = device->units * device->clock;
				}
				else {
					cpu_score = device->units * device->clock;
					if(cpu_score > fastest_cpu_score) {
						fastest_cpu_device = device;
						fastest_cpu_score = cpu_score;
					}
				}
			}
			else if(device->internal_type & CL_DEVICE_TYPE_GPU) {
				const auto compute_gpu_score = [](shared_ptr<compute_device> dev) -> unsigned int {
					unsigned int multiplier = 1;
					switch(dev->vendor) {
						case COMPUTE_VENDOR::NVIDIA:
							// fermi or kepler+ card if wg size is >= 1024
							multiplier = (dev->max_work_group_size >= 1024 ? 32 : 8);
							break;
						case COMPUTE_VENDOR::AMD:
							multiplier = 16;
							break;
							// none for INTEL
						default: break;
					}
					return multiplier * (dev->units * dev->clock);
				};
				
				if(fastest_gpu_device == nullptr) {
					fastest_gpu_device = device;
					fastest_gpu_score = compute_gpu_score(device);
				}
				else {
					gpu_score = compute_gpu_score(device);
					if(gpu_score > fastest_gpu_score) {
						fastest_gpu_device = device;
						fastest_gpu_score = gpu_score;
					}
				}
			}
		}
		
		// no supported devices found
		if(devices.empty()) {
			log_error("no supported device found on this platform!");
			continue;
		}
		
		// determine the fastest overall device
		if(fastest_cpu_device != nullptr || fastest_gpu_device != nullptr) {
			fastest_device = (fastest_gpu_score >= fastest_cpu_score ? fastest_gpu_device : fastest_cpu_device);
		}
		// else: no device at all, should have aborted earlier already
		
		//
		if(fastest_cpu_device != nullptr) {
			log_debug("fastest CPU device: %s %s (score: %u)",
					  fastest_cpu_device->vendor_name, fastest_cpu_device->name, fastest_cpu_score);
		}
		if(fastest_gpu_device != nullptr) {
			log_debug("fastest GPU device: %s %s (score: %u)",
					  fastest_gpu_device->vendor_name, fastest_gpu_device->name, fastest_gpu_score);
		}
		
		// if there has been no error (no continue) thus far, everything is okay with this platform and devices -> use it
		break;
	}
	
	// if absolutely no devices on any platform are supported, return (supported is still false)
	if(devices.empty()) {
		return;
	}
	// else: init successful, set supported to true
	supported = true;
	
	// context has been created, query image format information
	image_formats.clear();
	if(platform_vendor != COMPUTE_VENDOR::POCL) {
		cl_uint image_format_count = 0;
		clGetSupportedImageFormats(ctx, CL_MEM_READ_WRITE, CL_MEM_OBJECT_IMAGE2D, 0, nullptr, &image_format_count);
		image_formats.resize(image_format_count);
		clGetSupportedImageFormats(ctx, CL_MEM_READ_WRITE, CL_MEM_OBJECT_IMAGE2D, image_format_count, image_formats.data(), nullptr);
		if(image_formats.empty()) {
			log_error("no supported image formats!");
		}
	}
	else {
		// pocl has too many issues and doesn't have full image support
		// -> disable it and don't get any "supported" image formats
		for(const auto& dev : devices) {
			dev->image_support = false;
		}
	}
	
	// already create command queues for all devices, these will serve as the default queues and the ones returned
	// when first calling create_queue for a device (a second call will then create an actual new one)
	for(const auto& dev : devices) {
		create_queue(dev);
	}
	
	// un-#if-0 for debug output
#if 0
	const auto channel_order_to_string = [](const cl_channel_order& channel_order) -> string {
		switch(channel_order) {
			case CL_R: return "CL_R";
			case CL_A: return "CL_A";
			case CL_RG: return "CL_RG";
			case CL_RA: return "CL_RA";
			case CL_RGB: return "CL_RGB";
			case CL_RGBA: return "CL_RGBA";
			case CL_BGRA: return "CL_BGRA";
			case CL_ARGB: return "CL_ARGB";
			case CL_INTENSITY: return "CL_INTENSITY";
			case CL_LUMINANCE: return "CL_LUMINANCE";
			case CL_Rx: return "CL_Rx";
			case CL_RGx: return "CL_RGx";
			case CL_RGBx: return "CL_RGBx";
#if defined(CL_DEPTH)
			case CL_DEPTH: return "CL_DEPTH";
#endif
#if defined(CL_DEPTH_STENCIL)
			case CL_DEPTH_STENCIL: return "CL_DEPTH_STENCIL";
#endif
#if defined(CL_1RGB_APPLE)
			case CL_1RGB_APPLE: return "CL_1RGB_APPLE";
#endif
#if defined(CL_BGR1_APPLE)
			case CL_BGR1_APPLE: return "CL_BGR1_APPLE";
#endif
#if defined(CL_YCbYCr_APPLE)
			case CL_YCbYCr_APPLE: return "CL_YCbYCr_APPLE";
#endif
#if defined(CL_CbYCrY_APPLE)
			case CL_CbYCrY_APPLE: return "CL_CbYCrY_APPLE";
#endif
			default: break;
		}
		return to_string(channel_order);
	};
	const auto channel_type_to_string = [](const cl_channel_type& channel_type) -> string {
		switch(channel_type) {
			case CL_SNORM_INT8: return "CL_SNORM_INT8";
			case CL_SNORM_INT16: return "CL_SNORM_INT16";
			case CL_UNORM_INT8: return "CL_UNORM_INT8";
			case CL_UNORM_INT16: return "CL_UNORM_INT16";
			case CL_UNORM_SHORT_565: return "CL_UNORM_SHORT_565";
			case CL_UNORM_SHORT_555: return "CL_UNORM_SHORT_555";
			case CL_UNORM_INT_101010: return "CL_UNORM_INT_101010";
			case CL_SIGNED_INT8: return "CL_SIGNED_INT8";
			case CL_SIGNED_INT16: return "CL_SIGNED_INT16";
			case CL_SIGNED_INT32: return "CL_SIGNED_INT32";
			case CL_UNSIGNED_INT8: return "CL_UNSIGNED_INT8";
			case CL_UNSIGNED_INT16: return "CL_UNSIGNED_INT16";
			case CL_UNSIGNED_INT32: return "CL_UNSIGNED_INT32";
			case CL_HALF_FLOAT: return "CL_HALF_FLOAT";
			case CL_FLOAT: return "CL_FLOAT";
#if defined(CL_SFIXED14_APPLE)
			case CL_SFIXED14_APPLE: return "CL_SFIXED14_APPLE";
#endif
#if defined(CL_BIASED_HALF_APPLE)
			case CL_BIASED_HALF_APPLE: return "CL_BIASED_HALF_APPLE";
#endif
			default: break;
		}
		return to_string(channel_type);
	};
	
	for(const auto& format : image_formats) {
		log_undecorated("\t%s %s",
						channel_order_to_string(format.image_channel_order),
						channel_type_to_string(format.image_channel_data_type));
	}
#endif
}

shared_ptr<compute_queue> opencl_compute::create_queue(shared_ptr<compute_device> dev) {
	if(dev == nullptr) return {};
	
	// has a default queue already been created for this device?
	bool has_default_queue = false;
	shared_ptr<opencl_queue> dev_default_queue;
	for(const auto& default_queue : default_queues) {
		if(default_queue.first == dev) {
			has_default_queue = true;
			dev_default_queue = default_queue.second;
			break;
		}
	}
	
	// has the user already created a queue for this device (i.e. accessed the default queue)?
	bool user_accessed = false;
	if(has_default_queue) {
		const auto iter = default_queues_user_accessed.find(dev);
		if(iter != end(default_queues_user_accessed)) {
			user_accessed = iter->second;
		}
		
		// default queue exists and the user is creating a queue for this device for the first time
		// -> return the default queue
		if(!user_accessed) {
			// signal that the user has accessed the default queue, any subsequent create_queue calls will actually create a new queue
			default_queues_user_accessed.emplace(dev, true);
			return dev_default_queue;
		}
	}
	
	// create the queue
	cl_int create_err = CL_SUCCESS;
#if /*defined(CL_VERSION_2_0) ||*/ defined(__APPLE__) // TODO: should only be enabled if platform (and device?) support opencl 2.0+
#if defined(__APPLE__) && !defined(CL_VERSION_2_0)
#define clCreateCommandQueueWithProperties clCreateCommandQueueWithPropertiesAPPLE
#define cl_queue_properties cl_queue_properties_APPLE
#endif
	const cl_queue_properties properties[] { 0 };
	cl_command_queue cl_queue = clCreateCommandQueueWithProperties(ctx, ((opencl_device*)dev.get())->device_id,
																   properties, &create_err);
#else
	
FLOOR_PUSH_WARNINGS() // silence "clCreateCommandQueue" is deprecated warning
FLOOR_IGNORE_WARNING(deprecated-declarations)
	
	cl_command_queue cl_queue = clCreateCommandQueue(ctx, ((opencl_device*)dev.get())->device_id, 0, &create_err);
	
FLOOR_POP_WARNINGS()
#endif
	if(create_err != CL_SUCCESS) {
		log_error("failed to create command queue: %u: %s", create_err, cl_error_to_string(create_err));
		return {};
	}
	
	auto ret = make_shared<opencl_queue>(dev, cl_queue);
	queues.push_back(ret);
	
	// set the default queue if it hasn't been set yet
	if(!has_default_queue) {
		default_queues.emplace_back(dev, ret);
	}
	
	return ret;
}

shared_ptr<compute_queue> opencl_compute::get_device_default_queue(shared_ptr<compute_device> dev) const {
	return get_device_default_queue(dev.get());
}

shared_ptr<compute_queue> opencl_compute::get_device_default_queue(const compute_device* dev) const {
	for(const auto& default_queue : default_queues) {
		if(default_queue.first.get() == dev) {
			return default_queue.second;
		}
	}
	// only happens if the context is invalid (the default queues haven't been created)
	log_error("no default queue for this device exists yet!");
	return {};
}

shared_ptr<compute_buffer> opencl_compute::create_buffer(const size_t& size, const COMPUTE_MEMORY_FLAG flags,
														 const uint32_t opengl_type) {
	// NOTE: device doesn't really matter in opencl (can't actually be specified), so simply use the "fastest"
	// device which uses the same context as all other devices (only context matters for opencl)
	return make_shared<opencl_buffer>((opencl_device*)fastest_device.get(), size, flags, opengl_type);
}

shared_ptr<compute_buffer> opencl_compute::create_buffer(const size_t& size, void* data, const COMPUTE_MEMORY_FLAG flags,
														 const uint32_t opengl_type) {
	return make_shared<opencl_buffer>((opencl_device*)fastest_device.get(), size, data, flags, opengl_type);
}

shared_ptr<compute_buffer> opencl_compute::create_buffer(shared_ptr<compute_device> device,
														 const size_t& size, const COMPUTE_MEMORY_FLAG flags,
														 const uint32_t opengl_type) {
	return make_shared<opencl_buffer>((opencl_device*)device.get(), size, flags, opengl_type);
}

shared_ptr<compute_buffer> opencl_compute::create_buffer(shared_ptr<compute_device> device,
														 const size_t& size, void* data,
														 const COMPUTE_MEMORY_FLAG flags,
														 const uint32_t opengl_type) {
	return make_shared<opencl_buffer>((opencl_device*)device.get(), size, data, flags, opengl_type);
}

shared_ptr<compute_buffer> opencl_compute::wrap_buffer(shared_ptr<compute_device> device,
													   const uint32_t opengl_buffer,
													   const uint32_t opengl_type,
													   const COMPUTE_MEMORY_FLAG flags) {
	const auto info = compute_buffer::get_opengl_buffer_info(opengl_buffer, opengl_type, flags);
	if(!info.valid) return {};
	return make_shared<opencl_buffer>((opencl_device*)device.get(), info.size, nullptr,
									  flags | COMPUTE_MEMORY_FLAG::OPENGL_SHARING,
									  opengl_type, opengl_buffer);
}

shared_ptr<compute_buffer> opencl_compute::wrap_buffer(shared_ptr<compute_device> device,
													   const uint32_t opengl_buffer,
													   const uint32_t opengl_type,
													   void* data,
													   const COMPUTE_MEMORY_FLAG flags) {
	const auto info = compute_buffer::get_opengl_buffer_info(opengl_buffer, opengl_type, flags);
	if(!info.valid) return {};
	return make_shared<opencl_buffer>((opencl_device*)device.get(), info.size, data,
									  flags | COMPUTE_MEMORY_FLAG::OPENGL_SHARING,
									  opengl_type, opengl_buffer);
}

shared_ptr<compute_image> opencl_compute::create_image(shared_ptr<compute_device> device,
													   const uint4 image_dim,
													   const COMPUTE_IMAGE_TYPE image_type,
													   const COMPUTE_MEMORY_FLAG flags,
													   const uint32_t opengl_type) {
	return make_shared<opencl_image>((opencl_device*)device.get(), image_dim, image_type, nullptr, flags, opengl_type);
}

shared_ptr<compute_image> opencl_compute::create_image(shared_ptr<compute_device> device,
													   const uint4 image_dim,
													   const COMPUTE_IMAGE_TYPE image_type,
													   void* data,
													   const COMPUTE_MEMORY_FLAG flags,
													   const uint32_t opengl_type) {
	return make_shared<opencl_image>((opencl_device*)device.get(), image_dim, image_type, data, flags, opengl_type);
}

shared_ptr<compute_image> opencl_compute::wrap_image(shared_ptr<compute_device> device,
													 const uint32_t opengl_image,
													 const uint32_t opengl_target,
													 const COMPUTE_MEMORY_FLAG flags) {
	const auto info = compute_image::get_opengl_image_info(opengl_image, opengl_target, flags);
	if(!info.valid) return {};
	return make_shared<opencl_image>((opencl_device*)device.get(), info.image_dim, info.image_type, nullptr,
									 flags | COMPUTE_MEMORY_FLAG::OPENGL_SHARING,
									 opengl_target, opengl_image, &info);
}

shared_ptr<compute_image> opencl_compute::wrap_image(shared_ptr<compute_device> device,
													 const uint32_t opengl_image,
													 const uint32_t opengl_target,
													 void* data,
													 const COMPUTE_MEMORY_FLAG flags) {
	const auto info = compute_image::get_opengl_image_info(opengl_image, opengl_target, flags);
	if(!info.valid) return {};
	return make_shared<opencl_image>((opencl_device*)device.get(), info.image_dim, info.image_type, data,
									 flags | COMPUTE_MEMORY_FLAG::OPENGL_SHARING,
									 opengl_target, opengl_image, &info);
}

shared_ptr<opencl_program> opencl_compute::add_program(opencl_program::program_map_type&& prog_map) {
	// create the program object, which in turn will create kernel objects for all kernel functions in the program,
	// for all devices contained in the program map
	auto prog = make_shared<opencl_program>(move(prog_map));
	{
		GUARD(programs_lock);
		programs.push_back(prog);
	}
	return prog;
}

shared_ptr<compute_program> opencl_compute::add_program_file(const string& file_name,
															 const string additional_options) {
	// compile the source file for all devices in the context
	opencl_program::program_map_type prog_map;
	prog_map.reserve(devices.size());
	for(const auto& dev : devices) {
		prog_map.insert_or_assign((opencl_device*)dev.get(),
								  create_opencl_program(dev, llvm_compute::compile_program_file(dev, file_name, additional_options,
#if !defined(__APPLE__)
																								llvm_compute::TARGET::SPIR
#else
																								llvm_compute::TARGET::APPLECL
#endif
																								)));
	}
	return add_program(move(prog_map));
}

shared_ptr<compute_program> opencl_compute::add_program_source(const string& source_code,
															   const string additional_options) {
	// compile the source code for all devices in the context
	opencl_program::program_map_type prog_map;
	prog_map.reserve(devices.size());
	for(const auto& dev : devices) {
		prog_map.insert_or_assign((opencl_device*)dev.get(),
								  create_opencl_program(dev, llvm_compute::compile_program(dev, source_code, additional_options,
#if !defined(__APPLE__)
																						   llvm_compute::TARGET::SPIR
#else
																						   llvm_compute::TARGET::APPLECL
#endif
																						   )));
	}
	return add_program(move(prog_map));
}

opencl_program::opencl_program_entry opencl_compute::create_opencl_program(shared_ptr<compute_device> device,
																		   pair<string, vector<llvm_compute::kernel_info>> program_data) {
	opencl_program::opencl_program_entry ret;
	ret.kernels_info = program_data.second;
	const auto cl_dev = (const opencl_device*)device.get();
	
	if(program_data.first.empty()) {
		log_error("compilation failed");
		return ret;
	}

	// opencl api handling
	const size_t length = program_data.first.size();
	const unsigned char* binary_ptr = (const unsigned char*)program_data.first.data();
	cl_int binary_status = CL_SUCCESS;
	
	// create the program object ...
	cl_int create_err = CL_SUCCESS;
	ret.program = clCreateProgramWithBinary(ctx, 1, &cl_dev->device_id,
											&length, &binary_ptr, &binary_status, &create_err);
	if(create_err != CL_SUCCESS) {
		log_error("failed to create opencl program: %u: %s", create_err, cl_error_to_string(create_err));
		log_error("devices binary status: %s", to_string(binary_status));
		return ret;
	}
	else log_debug("successfully created opencl program!");
	
	// ... and build it
	const string build_options {
#if !defined(__APPLE__)
		+ " -x spir -spir-std=1.2"
#endif
	};
	CL_CALL_ERR_PARAM_RET(clBuildProgram(ret.program,
										 1, &cl_dev->device_id,
										 build_options.c_str(), nullptr, nullptr),
						  build_err, "failed to build opencl program", {});
	
	
	// print out build log
	log_debug("build log: %s", cl_get_info<CL_PROGRAM_BUILD_LOG>(ret.program, cl_dev->device_id));
	
	// for testing purposes (if enabled in the config): retrieve the compiled binaries again
	if(floor::get_compute_keep_binaries()) {
		const auto binaries = cl_get_info<CL_PROGRAM_BINARIES>(ret.program);
		file_io::string_to_file("binary_" + core::to_file_name(device->name) + ".bin", binaries[0]);
	}
	
	ret.valid = true;
	return ret;
}

shared_ptr<compute_program> opencl_compute::add_precompiled_program_file(const string& file_name floor_unused,
																		 const vector<llvm_compute::kernel_info>& kernel_infos floor_unused) {
	// TODO: !
	log_error("not yet supported by opencl_compute!");
	return {};
}

shared_ptr<compute_program::program_entry> opencl_compute::create_program_entry(shared_ptr<compute_device> device,
																				pair<string, vector<llvm_compute::kernel_info>> program_data) {
	return make_shared<opencl_program::opencl_program_entry>(create_opencl_program(device, program_data));
}

#endif
