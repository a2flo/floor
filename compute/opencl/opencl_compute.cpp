/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2020 Florian Ziesche
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
#include <floor/compute/spirv_handler.hpp>
#include <floor/core/platform.hpp>
#include <floor/core/gl_support.hpp>
#include <floor/core/logger.hpp>
#include <floor/core/core.hpp>
#include <floor/core/file_io.hpp>
#include <floor/compute/llvm_toolchain.hpp>
#include <floor/compute/universal_binary.hpp>
#include <floor/floor/floor.hpp>

#include <floor/core/platform_windows.hpp>
#include <floor/core/essentials.hpp> // cleanup

opencl_compute::opencl_compute(const uint32_t platform_index_,
							   const bool gl_sharing_,
							   const vector<string> whitelist) : compute_context() {
	// if no platform was specified, use the one in the config (or default one, which is 0)
	const auto platform_index = (platform_index_ == ~0u ? floor::get_opencl_platform() : platform_index_);
	
	// disable gl sharing if the opengl renderer isn't used
	bool gl_sharing = gl_sharing_;
	if(gl_sharing && floor::get_renderer() != floor::RENDERER::OPENGL) {
		gl_sharing = false;
	}
	
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
		
		// success
		log_debug("created opencl context on platform \"%s\"!",
				  cl_get_info<CL_PLATFORM_NAME>(platform));
		log_msg("platform vendor: \"%s\"", cl_get_info<CL_PLATFORM_VENDOR>(platform));
		log_msg("platform version: \"%s\"", cl_get_info<CL_PLATFORM_VERSION>(platform));
		log_msg("platform profile: \"%s\"", cl_get_info<CL_PLATFORM_PROFILE>(platform));
		log_msg("platform extensions: \"%s\"", core::trim(cl_get_info<CL_PLATFORM_EXTENSIONS>(platform)));
		
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
					// major version is higher than 2 -> pretend we're running on CL 2.2
					return { true, OPENCL_VERSION::OPENCL_2_2 };
				}
				else if(major_version == 2) {
					switch(minor_version) {
						case 0: return { true, OPENCL_VERSION::OPENCL_2_0 };
						case 1: return { true, OPENCL_VERSION::OPENCL_2_1 };
						case 2:
						default: // default to CL 2.2
							return { true, OPENCL_VERSION::OPENCL_2_2 };
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
			return { false, OPENCL_VERSION::NONE };
		};
		
		// get platform cl version
		const string cl_version_str = cl_get_info<CL_PLATFORM_VERSION>(platform);
		const auto extracted_cl_version = extract_cl_version(cl_version_str, "OpenCL "); // "OpenCL X.Y" required by spec
		if(!extracted_cl_version.first) {
			log_error("invalid opencl platform version string: %s", cl_version_str);
		}
		platform_cl_version = extracted_cl_version.second;
		bool check_spirv_support = (platform_cl_version >= OPENCL_VERSION::OPENCL_2_1);
		bool check_sub_group_support = (platform_cl_version >= OPENCL_VERSION::OPENCL_2_1);
		
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
					 (platform_cl_version == OPENCL_VERSION::OPENCL_2_0 ? "2.0" :
					  (platform_cl_version == OPENCL_VERSION::OPENCL_2_1 ? "2.1" : "2.2"))))));
		
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
			
			devices.emplace_back(make_unique<opencl_device>());
			auto& device = (opencl_device&)*devices.back();
			dev_type_str = "";
			
			device.ctx = ctx;
			device.context = this;
			device.device_id = cl_dev;
			device.internal_type = (uint32_t)cl_get_info<CL_DEVICE_TYPE>(cl_dev);
			device.units = cl_get_info<CL_DEVICE_MAX_COMPUTE_UNITS>(cl_dev);
			device.clock = cl_get_info<CL_DEVICE_MAX_CLOCK_FREQUENCY>(cl_dev);
			device.global_mem_size = cl_get_info<CL_DEVICE_GLOBAL_MEM_SIZE>(cl_dev);
			device.local_mem_size = cl_get_info<CL_DEVICE_LOCAL_MEM_SIZE>(cl_dev);
			device.local_mem_dedicated = (cl_get_info<CL_DEVICE_LOCAL_MEM_TYPE>(cl_dev) == CL_LOCAL);
			device.constant_mem_size = cl_get_info<CL_DEVICE_MAX_CONSTANT_BUFFER_SIZE>(cl_dev);
			device.name = cl_get_info<CL_DEVICE_NAME>(cl_dev);
			device.vendor_name = cl_get_info<CL_DEVICE_VENDOR>(cl_dev);
			device.version_str = cl_get_info<CL_DEVICE_VERSION>(cl_dev);
			device.cl_version = extract_cl_version(device.version_str, "OpenCL ").second;
			device.driver_version_str = cl_get_info<CL_DRIVER_VERSION>(cl_dev);
			device.extensions = core::tokenize(core::trim(cl_get_info<CL_DEVICE_EXTENSIONS>(cl_dev)), ' ');
			
			device.max_mem_alloc = cl_get_info<CL_DEVICE_MAX_MEM_ALLOC_SIZE>(cl_dev);
			device.max_total_local_size = (uint32_t)cl_get_info<CL_DEVICE_MAX_WORK_GROUP_SIZE>(cl_dev);
			const auto max_local_size = cl_get_info<CL_DEVICE_MAX_WORK_ITEM_SIZES>(cl_dev);
			if(max_local_size.size() != 3) {
				log_warn("max local size dim != 3: %u", max_local_size.size());
			}
			if(max_local_size.size() >= 1) device.max_local_size.x = (uint32_t)max_local_size[0];
			if(max_local_size.size() >= 2) device.max_local_size.y = (uint32_t)max_local_size[1];
			if(max_local_size.size() >= 3) device.max_local_size.z = (uint32_t)max_local_size[2];
			
			// for cpu devices: assume this is the host cpu and compute the simd-width dependent on that
			if(device.internal_type & CL_DEVICE_TYPE_CPU) {
				// always at least 4 (SSE, newer NEON), 8-wide if avx/avx2, 16-wide if avx-512
				device.simd_width = (core::cpu_has_avx() ? (core::cpu_has_avx512() ? 16 : 8) : 4);
			}
			
			// intel cpu is reporting 8192, but actually using this leads to segfaults in various cases
			// -> limit to 1024 which is actually working properly
			if(device.internal_type == CL_DEVICE_TYPE_CPU && platform_vendor == COMPUTE_VENDOR::INTEL) {
				device.max_total_local_size = 1024;
				device.max_local_size.min(device.max_total_local_size);
			}
			
			device.image_support = (cl_get_info<CL_DEVICE_IMAGE_SUPPORT>(cl_dev) == 1);
			if(!device.image_support) {
				log_error("device \"%s\" does not have basic image support, removing it!", device.name);
				devices.pop_back();
				continue;
			}
			
			device.image_depth_support = core::contains(device.extensions, "cl_khr_depth_images");
			device.image_depth_write_support = device.image_depth_support;
			device.image_msaa_support = core::contains(device.extensions, "cl_khr_gl_msaa_sharing");
			device.image_msaa_write_support = false; // always false
			device.image_msaa_array_support = device.image_msaa_support;
			device.image_msaa_array_write_support = false; // always false
			device.image_cube_support = false; // nope
			device.image_cube_write_support = false;
			device.image_cube_array_support = false;
			device.image_cube_array_write_support = false;
			device.image_mipmap_support = core::contains(device.extensions, "cl_khr_mipmap_image");
			device.image_mipmap_write_support = core::contains(device.extensions, "cl_khr_mipmap_image_writes");
			device.image_offset_read_support = false; // never
			device.image_offset_write_support = false;
			const auto read_write_images = cl_get_info<CL_DEVICE_MAX_READ_WRITE_IMAGE_ARGS>(cl_dev);
			device.image_read_write_support = (read_write_images > 0);
			log_msg("read/write images: %u", read_write_images);
			
			device.max_image_1d_buffer_dim = cl_get_info<CL_DEVICE_IMAGE_MAX_BUFFER_SIZE>(cl_dev);
			device.max_image_1d_dim = (uint32_t)cl_get_info<CL_DEVICE_IMAGE2D_MAX_WIDTH>(cl_dev);
			device.max_image_2d_dim.set((uint32_t)cl_get_info<CL_DEVICE_IMAGE2D_MAX_WIDTH>(cl_dev),
										(uint32_t)cl_get_info<CL_DEVICE_IMAGE2D_MAX_HEIGHT>(cl_dev));
			device.max_image_3d_dim.set((uint32_t)cl_get_info<CL_DEVICE_IMAGE3D_MAX_WIDTH>(cl_dev),
										(uint32_t)cl_get_info<CL_DEVICE_IMAGE3D_MAX_HEIGHT>(cl_dev),
										(uint32_t)cl_get_info<CL_DEVICE_IMAGE3D_MAX_DEPTH>(cl_dev));
			if(device.image_mipmap_support) {
				device.max_mip_levels = image_mip_level_count_from_max_dim(std::max(std::max(device.max_image_2d_dim.max_element(),
																							  device.max_image_3d_dim.max_element()),
																					 device.max_image_1d_dim));
			}
			
			device.double_support = (cl_get_info<CL_DEVICE_DOUBLE_FP_CONFIG>(cl_dev) != 0);
			const auto device_bitness = cl_get_info<CL_DEVICE_ADDRESS_BITS>(cl_dev);
			if (device_bitness != 64) {
				log_error("device \"%s\" has an unsupported bitness %u (must be 64)!", device.name, device_bitness);
				devices.pop_back();
				continue;
			}
			device.max_global_size = 0xFFFF'FFFF'FFFF'FFFFull; // range: sizeof(size_t) -> clEnqueueNDRangeKernel
			device.unified_memory = (cl_get_info<CL_DEVICE_HOST_UNIFIED_MEMORY>(cl_dev) == 1);
			device.basic_64_bit_atomics_support = core::contains(device.extensions, "cl_khr_int64_base_atomics");
			device.extended_64_bit_atomics_support = core::contains(device.extensions, "cl_khr_int64_extended_atomics");
			device.sub_group_support = (core::contains(device.extensions, "cl_khr_subgroups") ||
										core::contains(device.extensions, "cl_intel_subgroups") ||
										(device.cl_version >= OPENCL_VERSION::OPENCL_2_1 && platform_cl_version >= OPENCL_VERSION::OPENCL_2_1));
			if(device.sub_group_support) {
				check_sub_group_support = true;
			}
			device.required_size_sub_group_support = core::contains(device.extensions, "cl_intel_required_subgroup_size");
			if(device.required_size_sub_group_support) {
				const auto sub_group_sizes = cl_get_info<CL_DEVICE_SUB_GROUP_SIZES>(cl_dev);
				string sub_group_sizes_str = "";
				for(const auto& sg_size : sub_group_sizes) {
					sub_group_sizes_str += to_string(sg_size) + " ";
				}
				log_msg("supported sub-group sizes: %v", sub_group_sizes_str);
			}
			
			log_msg("max mem alloc: %u bytes / %u MB",
					device.max_mem_alloc,
					device.max_mem_alloc / 1024ULL / 1024ULL);
			log_msg("mem size: %u MB (global), %u KB (local), %u KB (constant)",
					device.global_mem_size / 1024ULL / 1024ULL,
					device.local_mem_size / 1024ULL,
					device.constant_mem_size / 1024ULL);
			log_msg("mem base address alignment: %u", cl_get_info<CL_DEVICE_MEM_BASE_ADDR_ALIGN>(cl_dev));
			log_msg("min data type alignment size: %u", cl_get_info<CL_DEVICE_MIN_DATA_TYPE_ALIGN_SIZE>(cl_dev));
			log_msg("host unified memory: %u", device.unified_memory);
			log_msg("max total local size: %u", device.max_total_local_size);
			log_msg("max local size: %v", device.max_local_size);
			log_msg("max global size: %v", device.max_global_size);
			log_msg("max param size: %u", cl_get_info<CL_DEVICE_MAX_PARAMETER_SIZE>(cl_dev));
			log_msg("double support: %b", device.double_support);
			log_msg("image support: %b", device.image_support);
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
			
			device.platform_vendor = platform_vendor;
			device.vendor = COMPUTE_VENDOR::UNKNOWN;
			string vendor_str = core::str_to_lower(device.vendor_name);
			if(strstr(vendor_str.c_str(), "nvidia") != nullptr) {
				device.vendor = COMPUTE_VENDOR::NVIDIA;
				device.simd_width = 32;
				device.simd_range = { device.simd_width, device.simd_width };
			}
			else if(strstr(vendor_str.c_str(), "intel") != nullptr) {
				device.vendor = COMPUTE_VENDOR::INTEL;
				device.simd_width = 16; // variable (8, 16 or 32), but 16 is a good estimate
				device.simd_range = { 8, 32 };
				// -> cpu simd width later
			}
			else if(strstr(vendor_str.c_str(), "apple") != nullptr) {
				device.vendor = COMPUTE_VENDOR::APPLE;
				// -> cpu simd width later
			}
			else if(strstr(vendor_str.c_str(), "amd") != nullptr ||
					strstr(vendor_str.c_str(), "advanced micro devices") != nullptr ||
					// "ati" should be tested last, since it also matches "corporation"
					strstr(vendor_str.c_str(), "ati") != nullptr) {
				device.vendor = COMPUTE_VENDOR::AMD;
				device.simd_width = 64;
				device.simd_range = { device.simd_width, device.simd_width };
				// -> cpu simd width later
			}
			
			// older pocl versions used an empty device name, but "pocl" is also contained in the device version
			if(device.version_str.find("pocl") != string::npos) {
				device.vendor = COMPUTE_VENDOR::POCL;
				
				// device unit count on pocl is 0 -> figure out how many logical cpus actually exist
				if(device.units == 0) {
					device.units = core::get_hw_thread_count();
				}
			}
			
			if(device.internal_type & CL_DEVICE_TYPE_CPU) {
				device.type = (compute_device::TYPE)cpu_counter;
				cpu_counter++;
				dev_type_str += "CPU ";
			}
			if(device.internal_type & CL_DEVICE_TYPE_GPU) {
				device.type = (compute_device::TYPE)gpu_counter;
				gpu_counter++;
				dev_type_str += "GPU ";
			}
			if(device.internal_type & CL_DEVICE_TYPE_ACCELERATOR) {
				dev_type_str += "Accelerator ";
			}
			if(device.internal_type & CL_DEVICE_TYPE_DEFAULT) {
				dev_type_str += "Default ";
			}
			
			const string cl_c_version_str = cl_get_info<CL_DEVICE_OPENCL_C_VERSION>(cl_dev);
			const auto extracted_cl_c_version = extract_cl_version(cl_c_version_str, "OpenCL C "); // "OpenCL C X.Y" required by spec
			if(!extracted_cl_c_version.first) {
				log_error("invalid opencl c version string: %s", cl_c_version_str);
			}
			device.c_version = extracted_cl_c_version.second;
			
			// pocl doesn't support spir, but can apparently handle llvm bitcode files
			if(!core::contains(device.extensions, "cl_khr_spir") &&
			   device.vendor != COMPUTE_VENDOR::POCL) {
				log_error("device \"%s\" does not support \"cl_khr_spir\", removing it!", device.name);
				devices.pop_back();
				continue;
			}
			log_msg("spir versions: %s", cl_get_info<CL_DEVICE_SPIR_VERSIONS>(cl_dev));
			
			// check spir-v support (core, extension, or forced for testing purposes)
			if((platform_cl_version >= OPENCL_VERSION::OPENCL_2_1 ||
				core::contains(device.extensions, "cl_khr_il_program") ||
				floor::get_opencl_force_spirv_check()) &&
			   // disable takes prio over force-check
			   !floor::get_opencl_disable_spirv()) {
				check_spirv_support = true;
				
				const auto il_versions = cl_get_info<CL_DEVICE_IL_VERSION>(cl_dev);
				log_msg("IL versions: %s", il_versions);
				
				// find the max supported spir-v opencl version
				const auto il_version_tokens = core::tokenize(core::trim(il_versions), ' ');
				for(const auto& il_token : il_version_tokens) {
					static constexpr const char spirv_id[] { "SPIR-V_" };
					if(il_token.find(spirv_id) == 0) {
						const auto dot_pos = il_token.rfind('.');
						if(dot_pos == string::npos) continue;
						const auto spirv_major = stou(il_token.substr(size(spirv_id) - 1,
																	  dot_pos - size(spirv_id) - 1));
						const auto spirv_minor = stou(il_token.substr(dot_pos + 1,
																	  il_token.size() - dot_pos - 1));
						auto spirv_version = SPIRV_VERSION::NONE;
						switch(spirv_major) {
							default:
							case 1:
								switch(spirv_minor) {
									case 0:
										spirv_version = SPIRV_VERSION::SPIRV_1_0;
										break;
									case 1:
										spirv_version = SPIRV_VERSION::SPIRV_1_1;
										break;
									case 2:
										spirv_version = SPIRV_VERSION::SPIRV_1_2;
										break;
									case 3:
										spirv_version = SPIRV_VERSION::SPIRV_1_3;
										break;
									case 4:
										spirv_version = SPIRV_VERSION::SPIRV_1_4;
										break;
									default:
									case 5:
										spirv_version = SPIRV_VERSION::SPIRV_1_5;
										break;
								}
								break;
						}
						if(spirv_version > device.spirv_version) {
							device.spirv_version = spirv_version;
						}
					}
				}
			}
			
			if(floor::get_opencl_force_spirv_check() &&
			   !floor::get_opencl_disable_spirv() &&
			   device.spirv_version == SPIRV_VERSION::NONE) {
				device.spirv_version = SPIRV_VERSION::SPIRV_1_0;
			}
			
			//
			log_debug("%s(Units: %u, Clock: %u MHz, Memory: %u MB): %s %s, %s / %s / %s",
					  dev_type_str,
					  device.units,
					  device.clock,
					  (unsigned int)(device.global_mem_size / 1024ull / 1024ull),
					  device.vendor_name,
					  device.name,
					  device.version_str,
					  device.driver_version_str,
					  cl_c_version_str);
			
			// compute score and try to figure out which device is the fastest
			if(device.internal_type & CL_DEVICE_TYPE_CPU) {
				if(fastest_cpu_device == nullptr) {
					fastest_cpu_device = &device;
					fastest_cpu_score = device.units * device.clock;
				}
				else {
					cpu_score = device.units * device.clock;
					if(cpu_score > fastest_cpu_score) {
						fastest_cpu_device = &device;
						fastest_cpu_score = cpu_score;
					}
				}
			}
			else if(device.internal_type & CL_DEVICE_TYPE_GPU) {
				const auto compute_gpu_score = [](const compute_device& dev) -> uint32_t {
					unsigned int multiplier = 1;
					switch(dev.vendor) {
						case COMPUTE_VENDOR::NVIDIA:
							// fermi or kepler+ card if wg size is >= 1024
							multiplier = (dev.max_total_local_size >= 1024 ? 32 : 8);
							break;
						case COMPUTE_VENDOR::AMD:
							multiplier = 16;
							break;
						// none for INTEL
						default: break;
					}
					return multiplier * (dev.units * dev.clock);
				};
				
				if(fastest_gpu_device == nullptr) {
					fastest_gpu_device = &device;
					fastest_gpu_score = compute_gpu_score(device);
				}
				else {
					gpu_score = compute_gpu_score(device);
					if(gpu_score > fastest_gpu_score) {
						fastest_gpu_device = &device;
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
		
		// handle SPIR-V support (this is platform-specific)
		if(check_spirv_support) {
			// check for core function first
			bool check_extension_ptr = false;
			if(platform_cl_version >= OPENCL_VERSION::OPENCL_2_1) {
#if !defined(CL_VERSION_2_1) || 1 // workaround dll hell
				// not compiling with opencl 2.1+ headers, which pretty much always means that the icd loader won't have
				// the core clCreateProgramWithIL symbol, and since I don't want to dlsym vendor libraries, fallback to
				// the extension method
				check_extension_ptr = true;
#else
				// we're compiling with opencl 2.1+ headers and this function *should* exist, but check for it just in case
				cl_create_program_with_il = &clCreateProgramWithIL;
				check_extension_ptr = (cl_create_program_with_il == nullptr);
#endif
			}
			
			// if the platform is not 2.1+, but the extension is supported, get the function pointer to it
			if(platform_cl_version < OPENCL_VERSION::OPENCL_2_1 || check_extension_ptr) {
				cl_create_program_with_il = (decltype(cl_create_program_with_il))clGetExtensionFunctionAddressForPlatform(platform, "clCreateProgramWithILKHR");
				if(cl_create_program_with_il == nullptr) {
FLOOR_PUSH_WARNINGS()
FLOOR_IGNORE_WARNING(deprecated-declarations)
					cl_create_program_with_il = (decltype(cl_create_program_with_il))clGetExtensionFunctionAddress("clCreateProgramWithILKHR");
FLOOR_POP_WARNINGS()
				}
			}
			
			// last resort: if we compiled against a opencl 2.1+ header/lib, but aren't using a opencl 2.1+ platform,
			// still try the core function (which might yet work)
#if defined(CL_VERSION_2_1) && 0 // workaround dll hell
			if(cl_create_program_with_il == nullptr) {
				cl_create_program_with_il = &clCreateProgramWithIL;
			}
#endif
			
			if(cl_create_program_with_il == nullptr) {
				log_error("no valid clCreateProgramWithIL function has been found, disabling SPIR-V support");
				
				// disable spir-v on all devices
				for(auto& dev : devices) {
					((opencl_device&)*dev).spirv_version = SPIRV_VERSION::NONE;
				}
			}
		}
		
		// enable parameter workaround for all device that use spir-v
		// (this is necessary, because struct parameters in spir-v are currently bugged)
		if(floor::get_opencl_spirv_param_workaround()) {
			for(auto& dev : devices) {
				if(((opencl_device&)*dev).spirv_version != SPIRV_VERSION::NONE) {
					dev->param_workaround = true;
				}
			}
		}
		
		// handle sub-group support
		if(check_sub_group_support) {
			bool check_extension_ptr = false;
			if(platform_cl_version >= OPENCL_VERSION::OPENCL_2_1) {
#if !defined(CL_VERSION_2_1) || 1 // workaround dll hell
				check_extension_ptr = true;
#else
				cl_get_kernel_sub_group_info = &clGetKernelSubGroupInfo;
				check_extension_ptr = (cl_get_kernel_sub_group_info == nullptr);
#endif
			}
			
			// if the platform is not 2.1+, but the extension is supported, get the function pointer to it
			if(platform_cl_version < OPENCL_VERSION::OPENCL_2_1 || check_extension_ptr) {
				cl_get_kernel_sub_group_info = (decltype(cl_get_kernel_sub_group_info))clGetExtensionFunctionAddressForPlatform(platform, "clGetKernelSubGroupInfoKHR");
				if(cl_get_kernel_sub_group_info == nullptr) {
FLOOR_PUSH_WARNINGS()
FLOOR_IGNORE_WARNING(deprecated-declarations)
					cl_get_kernel_sub_group_info = (decltype(cl_get_kernel_sub_group_info))clGetExtensionFunctionAddress("clGetKernelSubGroupInfoKHR");
FLOOR_POP_WARNINGS()
				}
			}
			
			if(cl_get_kernel_sub_group_info == nullptr) {
				log_error("no valid clGetKernelSubGroupInfo function has been found, disabling sub-group support");
				
				// disable sub-group support on all devices
				for(auto& dev : devices) {
					dev->sub_group_support = false;
					((opencl_device&)*dev).required_size_sub_group_support = false;
				}
			}
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
		for(auto& dev : devices) {
			dev->image_support = false;
		}
	}
	
	// already create command queues for all devices, these will serve as the default queues and the ones returned
	// when first calling create_queue for a device (a second call will then create an actual new one)
	for(const auto& dev : devices) {
		create_queue(*dev);
	}
}

shared_ptr<compute_queue> opencl_compute::create_queue(const compute_device& dev) const {
	// has a default queue already been created for this device?
	bool has_default_queue = false;
	shared_ptr<compute_queue> dev_default_queue;
	for(const auto& default_queue : default_queues) {
		if(default_queue.first.get() == dev) {
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
			default_queues_user_accessed.insert(dev, true);
			return dev_default_queue;
		}
	}
	
	// create the queue
	cl_int create_err = CL_SUCCESS;
#if /*defined(CL_VERSION_2_0) ||*/ 0 // TODO: should only be enabled if platform (and device?) support opencl 2.0+
	const cl_queue_properties properties[] { 0 };
	cl_command_queue cl_queue = clCreateCommandQueueWithProperties(ctx, ((const opencl_device&)dev).device_id,
																   properties, &create_err);
#else
	
FLOOR_PUSH_WARNINGS() // silence "clCreateCommandQueue" is deprecated warning
FLOOR_IGNORE_WARNING(deprecated-declarations)
	
	cl_command_queue cl_queue = clCreateCommandQueue(ctx, ((const opencl_device&)dev).device_id, 0, &create_err);
	
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
		default_queues.insert(dev, ret);
	}
	
	return ret;
}

const compute_queue* opencl_compute::get_device_default_queue(const compute_device& dev) const {
	for(const auto& default_queue : default_queues) {
		if(default_queue.first.get() == dev) {
			return default_queue.second.get();
		}
	}
	// only happens if the context is invalid (the default queues haven't been created)
	log_error("no default queue for this device exists yet!");
	return nullptr;
}

shared_ptr<compute_buffer> opencl_compute::create_buffer(const compute_queue& cqueue,
														 const size_t& size, const COMPUTE_MEMORY_FLAG flags,
														 const uint32_t opengl_type) const {
	return make_shared<opencl_buffer>(cqueue, size, flags, opengl_type);
}

shared_ptr<compute_buffer> opencl_compute::create_buffer(const compute_queue& cqueue,
														 const size_t& size, void* data,
														 const COMPUTE_MEMORY_FLAG flags,
														 const uint32_t opengl_type) const {
	return make_shared<opencl_buffer>(cqueue, size, data, flags, opengl_type);
}

shared_ptr<compute_buffer> opencl_compute::wrap_buffer(const compute_queue& cqueue,
													   const uint32_t opengl_buffer,
													   const uint32_t opengl_type,
													   const COMPUTE_MEMORY_FLAG flags) const {
	const auto info = compute_buffer::get_opengl_buffer_info(opengl_buffer, opengl_type, flags);
	if(!info.valid) return {};
	return make_shared<opencl_buffer>(cqueue, info.size, nullptr,
									  flags | COMPUTE_MEMORY_FLAG::OPENGL_SHARING,
									  opengl_type, opengl_buffer);
}

shared_ptr<compute_buffer> opencl_compute::wrap_buffer(const compute_queue& cqueue,
													   const uint32_t opengl_buffer,
													   const uint32_t opengl_type,
													   void* data,
													   const COMPUTE_MEMORY_FLAG flags) const {
	const auto info = compute_buffer::get_opengl_buffer_info(opengl_buffer, opengl_type, flags);
	if(!info.valid) return {};
	return make_shared<opencl_buffer>(cqueue, info.size, data,
									  flags | COMPUTE_MEMORY_FLAG::OPENGL_SHARING,
									  opengl_type, opengl_buffer);
}

shared_ptr<compute_image> opencl_compute::create_image(const compute_queue& cqueue,
													   const uint4 image_dim,
													   const COMPUTE_IMAGE_TYPE image_type,
													   const COMPUTE_MEMORY_FLAG flags,
													   const uint32_t opengl_type) const {
	return make_shared<opencl_image>(cqueue, image_dim, image_type, nullptr, flags, opengl_type);
}

shared_ptr<compute_image> opencl_compute::create_image(const compute_queue& cqueue,
													   const uint4 image_dim,
													   const COMPUTE_IMAGE_TYPE image_type,
													   void* data,
													   const COMPUTE_MEMORY_FLAG flags,
													   const uint32_t opengl_type) const {
	return make_shared<opencl_image>(cqueue, image_dim, image_type, data, flags, opengl_type);
}

shared_ptr<compute_image> opencl_compute::wrap_image(const compute_queue& cqueue,
													 const uint32_t opengl_image,
													 const uint32_t opengl_target,
													 const COMPUTE_MEMORY_FLAG flags) const {
	const auto info = compute_image::get_opengl_image_info(opengl_image, opengl_target, flags);
	if(!info.valid) return {};
	return make_shared<opencl_image>(cqueue, info.image_dim, info.image_type, nullptr,
									 flags | COMPUTE_MEMORY_FLAG::OPENGL_SHARING,
									 opengl_target, opengl_image, &info);
}

shared_ptr<compute_image> opencl_compute::wrap_image(const compute_queue& cqueue,
													 const uint32_t opengl_image,
													 const uint32_t opengl_target,
													 void* data,
													 const COMPUTE_MEMORY_FLAG flags) const {
	const auto info = compute_image::get_opengl_image_info(opengl_image, opengl_target, flags);
	if(!info.valid) return {};
	return make_shared<opencl_image>(cqueue, info.image_dim, info.image_type, data,
									 flags | COMPUTE_MEMORY_FLAG::OPENGL_SHARING,
									 opengl_target, opengl_image, &info);
}

shared_ptr<compute_program> opencl_compute::add_universal_binary(const string& file_name) {
	auto bins = universal_binary::load_dev_binaries_from_archive(file_name, *this);
	if (bins.ar == nullptr || bins.dev_binaries.empty()) {
		log_error("failed to load universal binary: %s", file_name);
		return {};
	}
	
	// create the program
	opencl_program::program_map_type prog_map;
	prog_map.reserve(devices.size());
	for (size_t i = 0, dev_count = devices.size(); i < dev_count; ++i) {
		const auto& cl_dev = (const opencl_device&)devices[i];
		const auto& dev_best_bin = bins.dev_binaries[i];
		const auto func_info = universal_binary::translate_function_info(dev_best_bin.first->functions);
		
		prog_map.insert_or_assign(cl_dev,
								  create_opencl_program_internal(cl_dev,
																 dev_best_bin.first->data.data(),
																 dev_best_bin.first->data.size(),
																 func_info,
																 dev_best_bin.second.opencl.is_spir ?
																 llvm_toolchain::TARGET::SPIR :
																 llvm_toolchain::TARGET::SPIRV_OPENCL,
																 false /* TODO: true? */));
	}
	
	return add_program(move(prog_map));
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
	compile_options options { .cli = additional_options };
	return add_program_file(file_name, options);
}

shared_ptr<compute_program> opencl_compute::add_program_file(const string& file_name,
															 compile_options options) {
	// compile the source file for all devices in the context
	opencl_program::program_map_type prog_map;
	prog_map.reserve(devices.size());
	for(const auto& dev : devices) {
		options.target = (((const opencl_device&)*dev).spirv_version != SPIRV_VERSION::NONE ?
						  llvm_toolchain::TARGET::SPIRV_OPENCL : llvm_toolchain::TARGET::SPIR);
		prog_map.insert_or_assign((const opencl_device&)*dev,
								  create_opencl_program(*dev, llvm_toolchain::compile_program_file(*dev, file_name, options),
														options.target));
	}
	return add_program(move(prog_map));
}

shared_ptr<compute_program> opencl_compute::add_program_source(const string& source_code,
															   const string additional_options) {
	compile_options options { .cli = additional_options };
	return add_program_source(source_code, options);
}

shared_ptr<compute_program> opencl_compute::add_program_source(const string& source_code,
															   compile_options options) {
	// compile the source code for all devices in the context
	opencl_program::program_map_type prog_map;
	prog_map.reserve(devices.size());
	for(const auto& dev : devices) {
		options.target = (((const opencl_device&)*dev).spirv_version != SPIRV_VERSION::NONE ?
						  llvm_toolchain::TARGET::SPIRV_OPENCL : llvm_toolchain::TARGET::SPIR);
		prog_map.insert_or_assign((const opencl_device&)*dev,
								  create_opencl_program(*dev, llvm_toolchain::compile_program(*dev, source_code, options),
														options.target));
	}
	return add_program(move(prog_map));
}

opencl_program::opencl_program_entry opencl_compute::create_opencl_program(const compute_device& device,
																		   llvm_toolchain::program_data program,
																		   const llvm_toolchain::TARGET& target) {
	if(!program.valid) {
		return {};
	}
	
	if (target == llvm_toolchain::TARGET::SPIRV_OPENCL) {
		// SPIR-V binary, loaded from a file
		size_t spirv_binary_size = 0;
		auto spirv_binary = spirv_handler::load_binary(program.data_or_filename, spirv_binary_size);
		if (!floor::get_toolchain_keep_temp() && file_io::is_file(program.data_or_filename)) {
			// cleanup if file exists
			core::system("rm " + program.data_or_filename);
		}
		if (spirv_binary == nullptr) return {}; // already prints an error
		
		return create_opencl_program_internal((const opencl_device&)device,
											  (const void*)spirv_binary.get(), spirv_binary_size,
											  program.functions, target,
											  program.options.silence_debug_output);
	} else {
		// SPIR binary, alreay in memory
		return create_opencl_program_internal((const opencl_device&)device,
											  (const void*)program.data_or_filename.data(),
											  program.data_or_filename.size(),
											  program.functions, target,
											  program.options.silence_debug_output);
	}
	
}

opencl_program::opencl_program_entry
opencl_compute::create_opencl_program_internal(const opencl_device& cl_dev,
											   const void* program_data,
											   const size_t& program_size,
											   const vector<llvm_toolchain::function_info>& functions,
											   const llvm_toolchain::TARGET& target,
											   const bool& silence_debug_output) {
	opencl_program::opencl_program_entry ret;
	ret.functions = functions;
	
	// create the program object ...
	cl_int create_err = CL_SUCCESS;
	if(target != llvm_toolchain::TARGET::SPIRV_OPENCL) {
		// opencl api handling
		cl_int binary_status = CL_SUCCESS;
		
		ret.program = clCreateProgramWithBinary(ctx, 1, &cl_dev.device_id,
												&program_size, (const unsigned char**)&program_data,
												&binary_status, &create_err);
		if(create_err != CL_SUCCESS) {
			log_error("failed to create opencl program: %u: %s", create_err, cl_error_to_string(create_err));
			log_error("devices binary status: %s", to_string(binary_status));
			return ret;
		}
		else if(!silence_debug_output) {
			log_debug("successfully created opencl program!");
		}
	}
	else {
		ret.program = cl_create_program_with_il(ctx, program_data, program_size, &create_err);
		if(create_err != CL_SUCCESS) {
			log_error("failed to create opencl program from IL/SPIR-V: %u: %s", create_err, cl_error_to_string(create_err));
			return ret;
		}
		else if(!silence_debug_output) {
			log_debug("successfully created opencl program (from IL/SPIR-V)!");
		}
	}
	
	// ... and build it
	const string build_options {
		(target == llvm_toolchain::TARGET::SPIR ? " -x spir -spir-std=1.2" : "")
	};
	CL_CALL_ERR_PARAM_RET(clBuildProgram(ret.program,
										 1, &cl_dev.device_id,
										 build_options.c_str(), nullptr, nullptr),
						  build_err, "failed to build opencl program", ret)
	
	
	// print out build log
	if(!silence_debug_output) {
		log_debug("build log: %s", cl_get_info<CL_PROGRAM_BUILD_LOG>(ret.program, cl_dev.device_id));
	}
	
	// for testing purposes (if enabled in the config): retrieve the compiled binaries again
	if(floor::get_toolchain_log_binaries()) {
		const auto binaries = cl_get_info<CL_PROGRAM_BINARIES>(ret.program);
		if(binaries.size() > 0 && !binaries[0].empty()) {
			file_io::string_to_file("binary_" + core::to_file_name(cl_dev.name) + ".bin", binaries[0]);
		}
		else {
			log_error("failed to retrieve compiled binary");
		}
	}
	
	ret.valid = true;
	return ret;
}

shared_ptr<compute_program> opencl_compute::add_precompiled_program_file(const string& file_name floor_unused,
																		 const vector<llvm_toolchain::function_info>& functions floor_unused) {
	// TODO: !
	log_error("not yet supported by opencl_compute!");
	return {};
}

shared_ptr<compute_program::program_entry> opencl_compute::create_program_entry(const compute_device& device,
																				llvm_toolchain::program_data program,
																				const llvm_toolchain::TARGET target) {
	return make_shared<opencl_program::opencl_program_entry>(create_opencl_program(device, program, target));
}

// from opencl_common: just forward to the context function
cl_int floor_opencl_get_kernel_sub_group_info(cl_kernel kernel,
											  const opencl_compute* ctx,
											  cl_device_id device,
											  cl_kernel_sub_group_info param_name,
											  size_t input_value_size,
											  const void* input_value,
											  size_t param_value_size,
											  void* param_value,
											  size_t* param_value_size_ret) {
	if(ctx == nullptr) return CL_INVALID_VALUE;
	return ctx->get_kernel_sub_group_info(kernel, device, param_name, input_value_size, input_value,
										  param_value_size, param_value, param_value_size_ret);
}


cl_int opencl_compute::get_kernel_sub_group_info(cl_kernel kernel,
												 cl_device_id device,
												 cl_kernel_sub_group_info param_name,
												 size_t input_value_size,
												 const void* input_value,
												 size_t param_value_size,
												 void* param_value,
												 size_t* param_value_size_ret) const {
	if(cl_get_kernel_sub_group_info == nullptr) return CL_INVALID_VALUE;
	
	for(const auto& dev : devices) {
		const auto& cl_dev = (const opencl_device&)*dev;
		if(cl_dev.device_id == device) {
			switch(param_name) {
				// opencl 2.1+ or cl_khr_subgroups or cl_intel_subgroups
				case CL_KERNEL_MAX_SUB_GROUP_SIZE_FOR_NDRANGE:
				case CL_KERNEL_SUB_GROUP_COUNT_FOR_NDRANGE:
					return cl_get_kernel_sub_group_info(kernel, device, param_name, input_value_size, input_value,
														param_value_size, param_value, param_value_size_ret);
					
				// opencl 2.1+
				case CL_KERNEL_LOCAL_SIZE_FOR_SUB_GROUP_COUNT:
				case CL_KERNEL_MAX_NUM_SUB_GROUPS:
				case CL_KERNEL_COMPILE_NUM_SUB_GROUPS:
					if(platform_cl_version >= OPENCL_VERSION::OPENCL_2_1) {
						return cl_get_kernel_sub_group_info(kernel, device, param_name, input_value_size, input_value,
															param_value_size, param_value, param_value_size_ret);
					}
					return CL_INVALID_VALUE;
					
				// cl_intel_required_subgroup_size
				case CL_KERNEL_COMPILE_SUB_GROUP_SIZE:
					if(cl_dev.required_size_sub_group_support) {
						return cl_get_kernel_sub_group_info(kernel, device, param_name, input_value_size, input_value,
															param_value_size, param_value, param_value_size_ret);
					}
					return CL_INVALID_VALUE;
			}
		}
	}
	return CL_INVALID_DEVICE;
}

#endif
