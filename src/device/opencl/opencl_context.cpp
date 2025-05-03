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

#include <floor/core/essentials.hpp>

#if !defined(FLOOR_NO_OPENCL)
#define FLOOR_OPENCL_INFO_FUNCS 1
#include <floor/device/opencl/opencl_context.hpp>
#include <floor/device/spirv_handler.hpp>
#include <floor/core/platform.hpp>
#include <floor/core/logger.hpp>
#include <floor/core/core.hpp>
#include <floor/core/file_io.hpp>
#include <floor/core/cpp_ext.hpp>
#include <floor/device/toolchain.hpp>
#include <floor/device/universal_binary.hpp>
#include <floor/floor.hpp>
#include <filesystem>

#include <floor/core/platform_windows.hpp>
#include <floor/core/essentials.hpp> // cleanup

namespace fl {

opencl_context::opencl_context(const DEVICE_CONTEXT_FLAGS ctx_flags,
							   const bool has_toolchain_,
							   const uint32_t platform_index_,
							   const std::vector<std::string> whitelist) : device_context(ctx_flags, has_toolchain_) {
	// if no platform was specified, use the one in the config (or default one, which is 0)
	const auto platform_index = (platform_index_ == ~0u ? floor::get_opencl_platform() : platform_index_);
	
	// get platforms
	cl_uint platform_count = 0;
	CL_CALL_RET(clGetPlatformIDs(0, nullptr, &platform_count), "failed to get platform count")
	std::vector<cl_platform_id> platforms(platform_count);
	CL_CALL_RET(clGetPlatformIDs(platform_count, platforms.data(), nullptr), "failed to get platforms")
	
	// check if there are any platforms at all
	if(platforms.empty()) {
		log_error("no OpenCL platforms found!");
		return;
	}
	log_debug("found $ OpenCL platform$", platforms.size(), (platforms.size() == 1 ? "" : "s"));
	
	// go through all platforms, starting with the user-specified one
	size_t first_platform_index = platform_index;
	if(platform_index >= platforms.size()) {
		log_warn("invalid platform index \"$\" - starting at 0 instead!", platform_index);
		first_platform_index = 0;
	}
	
	for(size_t i = 0, p_idx = first_platform_index, p_count = platforms.size() + 1; i < p_count; ++i, p_idx = i - 1) {
		// skip already checked platform
		if(i > 0 && first_platform_index == p_idx) continue;
		const auto& platform = platforms[p_idx];
		log_debug("checking OpenCL platform #$ \"$\" ...",
				  p_idx, cl_get_info<CL_PLATFORM_NAME>(platform));
		
		// get devices
		cl_uint all_device_count = 0;
		CL_CALL_CONT(clGetDeviceIDs(platform, CL_DEVICE_TYPE_ALL, 0, nullptr, &all_device_count),
					 "failed to get device count for platform")
		std::vector<cl_device_id> all_cl_devices(all_device_count);
		CL_CALL_CONT(clGetDeviceIDs(platform, CL_DEVICE_TYPE_ALL, all_device_count, all_cl_devices.data(), nullptr),
					 "failed to get devices for platform")
		
		log_debug("found $ OpenCL device$", all_cl_devices.size(), (all_cl_devices.size() == 1 ? "" : "s"));
		
		// device whitelist
		std::vector<cl_device_id> ctx_cl_devices;
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
					if(dev_name.find(entry) != std::string::npos) {
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
		
		cl_context_properties cl_properties[] {
			CL_CONTEXT_PLATFORM, (cl_context_properties)platform,
			0
		};
		
		CL_CALL_ERR_PARAM_CONT(ctx = clCreateContext(cl_properties, (cl_uint)ctx_cl_devices.size(), ctx_cl_devices.data(),
													 nullptr, nullptr, &ctx_error),
							   ctx_error, "failed to create OpenCL context")
		
		// success
		log_debug("created OpenCL context on platform \"$\"!",
				  cl_get_info<CL_PLATFORM_NAME>(platform));
		log_msg("platform vendor: \"$\"", cl_get_info<CL_PLATFORM_VENDOR>(platform));
		log_msg("platform version: \"$\"", cl_get_info<CL_PLATFORM_VERSION>(platform));
		log_msg("platform profile: \"$\"", cl_get_info<CL_PLATFORM_PROFILE>(platform));
		log_msg("platform extensions: \"$\"", core::trim(cl_get_info<CL_PLATFORM_EXTENSIONS>(platform)));
		
		// get platform vendor
		const std::string platform_vendor_str = core::str_to_lower(cl_get_info<CL_PLATFORM_VENDOR>(platform));
		if(platform_vendor_str.find("nvidia") != std::string::npos) {
			platform_vendor = VENDOR::NVIDIA;
		}
		else if(platform_vendor_str.find("amd") != std::string::npos ||
				platform_vendor_str.find("advanced micro devices") != std::string::npos) {
			platform_vendor = VENDOR::AMD;
		}
		else if(platform_vendor_str.find("intel") != std::string::npos) {
			platform_vendor = VENDOR::INTEL;
		}
		
		//
		const auto extract_cl_version = [](const std::string& cl_version_str, const std::string str_start) -> std::pair<bool, OPENCL_VERSION> {
			// "OpenCL X.Y" or "OpenCL C X.Y" required by spec (str_start must be either)
			const size_t start_len = str_start.length();
			if(cl_version_str.length() >= (start_len + 3) && cl_version_str.substr(0, start_len) == str_start) {
				const std::string version_str = cl_version_str.substr(start_len, cl_version_str.find(" ", start_len) - start_len);
				const size_t dot_pos = version_str.find(".");
				const auto major_version = stosize(version_str.substr(0, dot_pos));
				const auto minor_version = stosize(version_str.substr(dot_pos+1, version_str.length()-dot_pos-1));
				if(major_version > 3) {
					// major version is higher than 3 -> pretend we're running on CL 3.0
					return { true, OPENCL_VERSION::OPENCL_3_0 };
				} else if(major_version == 3) {
					switch(minor_version) {
						case 0: return { true, OPENCL_VERSION::OPENCL_3_0 };
						default: // default to CL 3.0
							return { true, OPENCL_VERSION::OPENCL_3_0 };
					}
				} else if(major_version == 2) {
					switch(minor_version) {
						case 0: return { true, OPENCL_VERSION::OPENCL_2_0 };
						case 1: return { true, OPENCL_VERSION::OPENCL_2_1 };
						case 2:
						default: // default to CL 2.2
							return { true, OPENCL_VERSION::OPENCL_2_2 };
					}
				} else {
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
		const std::string cl_version_str = cl_get_info<CL_PLATFORM_VERSION>(platform);
		const auto extracted_cl_version = extract_cl_version(cl_version_str, "OpenCL "); // "OpenCL X.Y" required by spec
		if(!extracted_cl_version.first) {
			log_error("invalid OpenCL platform version string: $", cl_version_str);
		}
		platform_cl_version = extracted_cl_version.second;
		if (platform_cl_version < OPENCL_VERSION::OPENCL_1_2) {
			log_error("OpenCL platform version must be 1.2+");
			continue;
		}
		bool check_spirv_support = (platform_cl_version >= OPENCL_VERSION::OPENCL_2_1);
		bool check_sub_group_support = (platform_cl_version >= OPENCL_VERSION::OPENCL_2_1);
		
		//
		log_msg("OpenCL platform \"$\" version recognized as CL$",
				vendor_to_string(platform_vendor),
				(platform_cl_version == OPENCL_VERSION::OPENCL_1_0 ? "1.0" :
				 (platform_cl_version == OPENCL_VERSION::OPENCL_1_1 ? "1.1" :
				  (platform_cl_version == OPENCL_VERSION::OPENCL_1_2 ? "1.2" :
				   (platform_cl_version == OPENCL_VERSION::OPENCL_2_0 ? "2.0" :
					(platform_cl_version == OPENCL_VERSION::OPENCL_2_1 ? "2.1" :
					 (platform_cl_version == OPENCL_VERSION::OPENCL_2_2 ? "2.2" : "3.0")))))));
		
		// handle device init
		ctx_devices.clear();
		ctx_devices = cl_get_info<CL_CONTEXT_DEVICES>(ctx);
		log_debug("found $ OpenCL device$ in context", ctx_devices.size(), (ctx_devices.size() == 1 ? "" : "s"));
		
		std::string dev_type_str;
		auto gpu_counter = (uint32_t)device::TYPE::GPU0;
		auto cpu_counter = (uint32_t)device::TYPE::CPU0;
		uint32_t fastest_cpu_score = 0;
		uint32_t fastest_gpu_score = 0;
		uint32_t cpu_score = 0;
		uint32_t gpu_score = 0;
		fastest_cpu_device = nullptr;
		fastest_gpu_device = nullptr;
		
		devices.clear();
		for(size_t j = 0; j < ctx_devices.size(); ++j) {
			auto& cl_dev = ctx_devices[j];
			
			const auto dev_version_str = cl_get_info<CL_DEVICE_VERSION>(cl_dev);
			const auto dev_cl_version = extract_cl_version(dev_version_str, "OpenCL ").second;
			if (dev_cl_version < OPENCL_VERSION::OPENCL_1_2) {
				log_error("ignoring device #$ with unsupported OpenCL version $ < 1.2", j, cl_version_to_string(dev_cl_version));
				continue;
			}
			
			devices.emplace_back(std::make_unique<opencl_device>());
			auto& dev = (opencl_device&)*devices.back();
			dev_type_str = "";
			
			dev.ctx = ctx;
			dev.context = this;
			dev.device_id = cl_dev;
			dev.internal_type = (uint32_t)cl_get_info<CL_DEVICE_TYPE>(cl_dev);
			dev.units = cl_get_info<CL_DEVICE_MAX_COMPUTE_UNITS>(cl_dev);
			dev.clock = cl_get_info<CL_DEVICE_MAX_CLOCK_FREQUENCY>(cl_dev);
			dev.global_mem_size = cl_get_info<CL_DEVICE_GLOBAL_MEM_SIZE>(cl_dev);
			dev.local_mem_size = cl_get_info<CL_DEVICE_LOCAL_MEM_SIZE>(cl_dev);
			dev.local_mem_dedicated = (cl_get_info<CL_DEVICE_LOCAL_MEM_TYPE>(cl_dev) == CL_LOCAL);
			dev.constant_mem_size = cl_get_info<CL_DEVICE_MAX_CONSTANT_BUFFER_SIZE>(cl_dev);
			dev.name = cl_get_info<CL_DEVICE_NAME>(cl_dev);
			dev.vendor_name = cl_get_info<CL_DEVICE_VENDOR>(cl_dev);
			dev.version_str = dev_version_str;
			dev.cl_version = dev_cl_version;
			dev.driver_version_str = cl_get_info<CL_DRIVER_VERSION>(cl_dev);
			dev.extensions = core::tokenize(core::trim(cl_get_info<CL_DEVICE_EXTENSIONS>(cl_dev)), ' ');
			
			dev.max_mem_alloc = cl_get_info<CL_DEVICE_MAX_MEM_ALLOC_SIZE>(cl_dev);
			dev.max_total_local_size = (uint32_t)cl_get_info<CL_DEVICE_MAX_WORK_GROUP_SIZE>(cl_dev);
			const auto max_local_size = cl_get_info<CL_DEVICE_MAX_WORK_ITEM_SIZES>(cl_dev);
			if(max_local_size.size() != 3) {
				log_warn("max local size dim != 3: $", max_local_size.size());
			}
			if(max_local_size.size() >= 1) dev.max_local_size.x = (uint32_t)max_local_size[0];
			if(max_local_size.size() >= 2) dev.max_local_size.y = (uint32_t)max_local_size[1];
			if(max_local_size.size() >= 3) dev.max_local_size.z = (uint32_t)max_local_size[2];
			
			// for CPU devices: assume this is the host CPU and compute the SIMD-width dependent on that
			if(dev.internal_type & CL_DEVICE_TYPE_CPU) {
				// always at least 4 (SSE, newer NEON), 8-wide if AVX/AVX2, 16-wide if AVX-512
				dev.simd_width = (core::cpu_has_avx() ? (core::cpu_has_avx512() ? 16 : 8) : 4);
			}
			
			// Intel CPU is reporting 8192, but actually using this leads to segfaults in various cases
			// -> limit to 1024 which is actually working properly
			if(dev.internal_type == CL_DEVICE_TYPE_CPU && platform_vendor == VENDOR::INTEL) {
				dev.max_total_local_size = 1024;
				dev.max_local_size.min(dev.max_total_local_size);
			}
			dev.max_resident_local_size = dev.max_total_local_size;
			
			dev.image_support = (cl_get_info<CL_DEVICE_IMAGE_SUPPORT>(cl_dev) == 1);
			if(!dev.image_support) {
				log_error("device \"$\" does not have basic image support, removing it!", dev.name);
				devices.pop_back();
				continue;
			}
			
			dev.image_depth_support = core::contains(dev.extensions, "cl_khr_depth_images");
			dev.image_depth_write_support = dev.image_depth_support;
			dev.image_msaa_support = core::contains(dev.extensions, "cl_khr_gl_msaa_sharing");
			dev.image_msaa_array_support = dev.image_msaa_support;
			dev.image_mipmap_support = core::contains(dev.extensions, "cl_khr_mipmap_image");
			dev.image_mipmap_write_support = core::contains(dev.extensions, "cl_khr_mipmap_image_writes");
			const auto read_write_images = cl_get_info<CL_DEVICE_MAX_READ_WRITE_IMAGE_ARGS>(cl_dev);
			dev.image_read_write_support = (read_write_images > 0);
			log_msg("read/write images: $", read_write_images);
			
			dev.max_image_1d_buffer_dim = cl_get_info<CL_DEVICE_IMAGE_MAX_BUFFER_SIZE>(cl_dev);
			dev.max_image_1d_dim = (uint32_t)cl_get_info<CL_DEVICE_IMAGE2D_MAX_WIDTH>(cl_dev);
			dev.max_image_2d_dim.set((uint32_t)cl_get_info<CL_DEVICE_IMAGE2D_MAX_WIDTH>(cl_dev),
										(uint32_t)cl_get_info<CL_DEVICE_IMAGE2D_MAX_HEIGHT>(cl_dev));
			dev.max_image_3d_dim.set((uint32_t)cl_get_info<CL_DEVICE_IMAGE3D_MAX_WIDTH>(cl_dev),
										(uint32_t)cl_get_info<CL_DEVICE_IMAGE3D_MAX_HEIGHT>(cl_dev),
										(uint32_t)cl_get_info<CL_DEVICE_IMAGE3D_MAX_DEPTH>(cl_dev));
			if(dev.image_mipmap_support) {
				dev.max_mip_levels = image_mip_level_count_from_max_dim(std::max(std::max(dev.max_image_2d_dim.max_element(),
																							 dev.max_image_3d_dim.max_element()),
																					dev.max_image_1d_dim));
			}
			
			dev.double_support = (cl_get_info<CL_DEVICE_DOUBLE_FP_CONFIG>(cl_dev) != 0);
			const auto device_bitness = cl_get_info<CL_DEVICE_ADDRESS_BITS>(cl_dev);
			if (device_bitness != 64) {
				log_error("dev \"$\" has an unsupported bitness $ (must be 64)!", dev.name, device_bitness);
				devices.pop_back();
				continue;
			}
			dev.max_global_size = 0xFFFF'FFFF'FFFF'FFFFull; // range: sizeof(size_t) -> clEnqueueNDRangeKernel
			dev.unified_memory = (cl_get_info<CL_DEVICE_HOST_UNIFIED_MEMORY>(cl_dev) == 1);
			dev.basic_64_bit_atomics_support = core::contains(dev.extensions, "cl_khr_int64_base_atomics");
			dev.extended_64_bit_atomics_support = core::contains(dev.extensions, "cl_khr_int64_extended_atomics");
			dev.sub_group_support = (core::contains(dev.extensions, "cl_khr_subgroups") ||
										core::contains(dev.extensions, "cl_intel_subgroups") ||
										(dev.cl_version >= OPENCL_VERSION::OPENCL_2_1 && platform_cl_version >= OPENCL_VERSION::OPENCL_2_1));
			if(dev.sub_group_support) {
				check_sub_group_support = true;
			}
			dev.required_size_sub_group_support = core::contains(dev.extensions, "cl_intel_required_subgroup_size");
			if(dev.required_size_sub_group_support) {
				const auto sub_group_sizes = cl_get_info<CL_DEVICE_SUB_GROUP_SIZES>(cl_dev);
				std::string sub_group_sizes_str;
				for(const auto& sg_size : sub_group_sizes) {
					sub_group_sizes_str += std::to_string(sg_size) + " ";
				}
				log_msg("supported sub-group sizes: $", sub_group_sizes_str);
			}
			
			log_msg("max mem alloc: $ bytes / $ MB",
					dev.max_mem_alloc,
					dev.max_mem_alloc / 1024ULL / 1024ULL);
			log_msg("mem size: $ MB (global), $ KB (local), $ KB (constant)",
					dev.global_mem_size / 1024ULL / 1024ULL,
					dev.local_mem_size / 1024ULL,
					dev.constant_mem_size / 1024ULL);
			log_msg("mem base address alignment: $", cl_get_info<CL_DEVICE_MEM_BASE_ADDR_ALIGN>(cl_dev));
			log_msg("min data type alignment size: $", cl_get_info<CL_DEVICE_MIN_DATA_TYPE_ALIGN_SIZE>(cl_dev));
			log_msg("host unified memory: $", dev.unified_memory);
			log_msg("max total local size: $", dev.max_total_local_size);
			log_msg("max local size: $", dev.max_local_size);
			log_msg("max global size: $", dev.max_global_size);
			log_msg("max param size: $", cl_get_info<CL_DEVICE_MAX_PARAMETER_SIZE>(cl_dev));
			log_msg("double support: $", dev.double_support);
			log_msg("image support: $", dev.image_support);
			const auto printf_buffer_size = cl_get_info<CL_DEVICE_PRINTF_BUFFER_SIZE>(cl_dev);
			log_msg("printf buffer size: $ bytes / $ MB",
					printf_buffer_size,
					printf_buffer_size / 1024ULL / 1024ULL);
			log_msg("max sub-devices: $", cl_get_info<CL_DEVICE_PARTITION_MAX_SUB_DEVICES>(cl_dev));
			log_msg("built-in kernels: $", cl_get_info<CL_DEVICE_BUILT_IN_KERNELS>(cl_dev));
			log_msg("extensions: \"$\"", core::trim(cl_get_info<CL_DEVICE_EXTENSIONS>(cl_dev)));
			
			dev.platform_vendor = platform_vendor;
			dev.vendor = VENDOR::UNKNOWN;
			std::string vendor_str = core::str_to_lower(dev.vendor_name);
			if(strstr(vendor_str.c_str(), "nvidia") != nullptr) {
				dev.vendor = VENDOR::NVIDIA;
				dev.simd_width = 32;
				dev.simd_range = { dev.simd_width, dev.simd_width };
			}
			else if(strstr(vendor_str.c_str(), "intel") != nullptr) {
				dev.vendor = VENDOR::INTEL;
				dev.simd_width = 16; // variable (8, 16 or 32), but 16 is a good estimate
				dev.simd_range = { 8, 32 };
				// -> CPU SIMD-width later
			}
			else if(strstr(vendor_str.c_str(), "apple") != nullptr) {
				dev.vendor = VENDOR::APPLE;
				// -> CPU SIMD-width later
			}
			else if(strstr(vendor_str.c_str(), "amd") != nullptr ||
					strstr(vendor_str.c_str(), "advanced micro devices") != nullptr ||
					// "ati" should be tested last, since it also matches "corporation"
					strstr(vendor_str.c_str(), "ati") != nullptr) {
				dev.vendor = VENDOR::AMD;
				dev.simd_width = cl_get_info<CL_DEVICE_WAVEFRONT_WIDTH_AMD>(cl_dev);
				dev.simd_range = { dev.simd_width, dev.simd_width };
				// -> CPU SIMD-width later
			}
			
			if(dev.internal_type & CL_DEVICE_TYPE_CPU) {
				dev.type = (device::TYPE)cpu_counter;
				cpu_counter++;
				dev_type_str += "CPU ";
			}
			if(dev.internal_type & CL_DEVICE_TYPE_GPU) {
				dev.type = (device::TYPE)gpu_counter;
				gpu_counter++;
				dev_type_str += "GPU ";
			}
			if(dev.internal_type & CL_DEVICE_TYPE_ACCELERATOR) {
				dev_type_str += "Accelerator ";
			}
			if(dev.internal_type & CL_DEVICE_TYPE_DEFAULT) {
				dev_type_str += "Default ";
			}
			
			const std::string cl_c_version_str = cl_get_info<CL_DEVICE_OPENCL_C_VERSION>(cl_dev);
			const auto extracted_cl_c_version = extract_cl_version(cl_c_version_str, "OpenCL C "); // "OpenCL C X.Y" required by spec
			if(!extracted_cl_c_version.first) {
				log_error("invalid OpenCL C version string: $", cl_c_version_str);
			}
			dev.c_version = extracted_cl_c_version.second;
			
			if(!core::contains(dev.extensions, "cl_khr_spir")) {
				log_error("device \"$\" does not support \"cl_khr_spir\", removing it!", dev.name);
				devices.pop_back();
				continue;
			}
			log_msg("spir versions: $", cl_get_info<CL_DEVICE_SPIR_VERSIONS>(cl_dev));
			
			// check spir-v support (core, extension, or forced for testing purposes)
			if (!floor::get_opencl_disable_spirv() /* disable takes prio over force-check */ &&
				core::contains(dev.extensions, "cl_khr_il_program")) {
				if (platform_cl_version >= OPENCL_VERSION::OPENCL_3_0) {
					const auto il_versions = cl_get_info<CL_DEVICE_ILS_WITH_VERSION>(cl_dev);
					if (il_versions.empty()) {
						log_error("device \"$\" does not support any IL version", dev.name);
						devices.pop_back();
						continue;
					}
					for (const auto& il_version : il_versions) {
						static const auto get_spirv_version = [](const uint32_t& version) -> std::pair<uint32_t, uint32_t> {
							return { (version & 0xFFC0'0000u) >> 22u, (version & 0x003F'F000u) >> 12u };
						};
						static constexpr const auto make_spirv_version = [](const uint32_t major, const uint32_t minor) constexpr {
							return (major << 22u) | (minor << 12);
						};
						const auto version = get_spirv_version(il_version.version);
						log_msg("supported IL: $: $.$", il_version.name, version.first, version.second);
						if (std::string(il_version.name) == "SPIR-V") {
							auto spirv_version = SPIRV_VERSION::NONE;
							switch (il_version.version) {
								case make_spirv_version(1, 0):
									spirv_version = SPIRV_VERSION::SPIRV_1_0;
									break;
								case make_spirv_version(1, 1):
									spirv_version = SPIRV_VERSION::SPIRV_1_1;
									break;
								case make_spirv_version(1, 2):
									spirv_version = SPIRV_VERSION::SPIRV_1_2;
									break;
								case make_spirv_version(1, 3):
									spirv_version = SPIRV_VERSION::SPIRV_1_3;
									break;
								case make_spirv_version(1, 4):
									spirv_version = SPIRV_VERSION::SPIRV_1_4;
									break;
								case make_spirv_version(1, 5):
									spirv_version = SPIRV_VERSION::SPIRV_1_5;
									break;
								case make_spirv_version(1, 6):
									spirv_version = SPIRV_VERSION::SPIRV_1_6;
									break;
							}
							if (spirv_version > dev.spirv_version) {
								dev.spirv_version = spirv_version;
							}
						}
					}
				} else if (platform_cl_version >= OPENCL_VERSION::OPENCL_2_1 ||
						   floor::get_opencl_force_spirv_check()) {
					check_spirv_support = true;
					
					const auto il_versions = cl_get_info<CL_DEVICE_IL_VERSION>(cl_dev);
					log_msg("IL versions: $", il_versions);
					
					// find the max supported SPIR-V OpenCL version
					const auto il_version_tokens = core::tokenize(core::trim(il_versions), ' ');
					for(const auto& il_token : il_version_tokens) {
						static constexpr const char spirv_id[] { "SPIR-V_" };
						if(il_token.find(spirv_id) == 0) {
							const auto dot_pos = il_token.rfind('.');
							if(dot_pos == std::string::npos) continue;
							const auto spirv_major = stou(il_token.substr(std::size(spirv_id) - 1,
																		  dot_pos - std::size(spirv_id) - 1));
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
										case 5:
											spirv_version = SPIRV_VERSION::SPIRV_1_5;
											break;
										default:
										case 6:
											spirv_version = SPIRV_VERSION::SPIRV_1_6;
											break;
									}
									break;
							}
							if(spirv_version > dev.spirv_version) {
								dev.spirv_version = spirv_version;
							}
						}
					}
				}
			}
			
			if(floor::get_opencl_force_spirv_check() &&
			   !floor::get_opencl_disable_spirv() &&
			   dev.spirv_version == SPIRV_VERSION::NONE) {
				dev.spirv_version = SPIRV_VERSION::SPIRV_1_0;
			}
			
			//
			log_debug("$(Units: $, Clock: $ MHz, Memory: $ MB): $ $, $ / $ / $",
					  dev_type_str,
					  dev.units,
					  dev.clock,
					  uint32_t(dev.global_mem_size / 1024ull / 1024ull),
					  dev.vendor_name,
					  dev.name,
					  dev.version_str,
					  dev.driver_version_str,
					  cl_c_version_str);
			
			// compute score and try to figure out which device is the fastest
			if(dev.internal_type & CL_DEVICE_TYPE_CPU) {
				if(fastest_cpu_device == nullptr) {
					fastest_cpu_device = &dev;
					fastest_cpu_score = dev.units * dev.clock;
				}
				else {
					cpu_score = dev.units * dev.clock;
					if(cpu_score > fastest_cpu_score) {
						fastest_cpu_device = &dev;
						fastest_cpu_score = cpu_score;
					}
				}
			}
			else if(dev.internal_type & CL_DEVICE_TYPE_GPU) {
				const auto compute_gpu_score = [](const device& dev_) -> uint32_t {
					uint32_t multiplier = 1;
					switch (dev_.vendor) {
						case VENDOR::NVIDIA:
							// Fermi or Kepler+ card if wg size is >= 1024
							multiplier = (dev_.max_total_local_size >= 1024 ? 32 : 8);
							break;
						case VENDOR::AMD:
							multiplier = 16;
							break;
						// none for INTEL
						default: break;
					}
					return multiplier * (dev_.units * dev_.clock);
				};
				
				if(fastest_gpu_device == nullptr) {
					fastest_gpu_device = &dev;
					fastest_gpu_score = compute_gpu_score(dev);
				}
				else {
					gpu_score = compute_gpu_score(dev);
					if(gpu_score > fastest_gpu_score) {
						fastest_gpu_device = &dev;
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
				// not compiling with OpenCL 2.1+ headers, which pretty much always means that the icd loader won't have
				// the core clCreateProgramWithIL symbol, and since I don't want to dlsym vendor libraries, fallback to
				// the extension method
				check_extension_ptr = true;
#else
				// we're compiling with OpenCL 2.1+ headers and this function *should* exist, but check for it just in case
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
			
			// last resort: if we compiled against a OpenCL 2.1+ header/lib, but aren't using a OpenCL 2.1+ platform,
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
			log_debug("fastest CPU device: $ $ (score: $)",
					  fastest_cpu_device->vendor_name, fastest_cpu_device->name, fastest_cpu_score);
		}
		if(fastest_gpu_device != nullptr) {
			log_debug("fastest GPU device: $ $ (score: $)",
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
	cl_uint image_format_count = 0;
	clGetSupportedImageFormats(ctx, CL_MEM_READ_WRITE, CL_MEM_OBJECT_IMAGE2D, 0, nullptr, &image_format_count);
	image_formats.resize(image_format_count);
	clGetSupportedImageFormats(ctx, CL_MEM_READ_WRITE, CL_MEM_OBJECT_IMAGE2D, image_format_count, image_formats.data(), nullptr);
	if(image_formats.empty()) {
		log_error("no supported image formats!");
	}
	
	// already create command queues for all devices, these will serve as the default queues and the ones returned
	// when first calling create_queue for a device (a second call will then create an actual new one)
	for(const auto& dev : devices) {
		create_queue(*dev);
	}
}

std::shared_ptr<device_queue> opencl_context::create_queue(const device& dev) const {
	// has a default queue already been created for this device?
	bool has_default_queue = false;
	std::shared_ptr<device_queue> dev_default_queue;
	for (auto&& default_queue : default_queues) {
		if(default_queue.first == &dev) {
			has_default_queue = true;
			dev_default_queue = default_queue.second;
			break;
		}
	}
	
	// has the user already created a queue for this device (i.e. accessed the default queue)?
	bool user_accessed = false;
	if(has_default_queue) {
		const auto iter = default_queues_user_accessed.find(&dev);
		if(iter != std::end(default_queues_user_accessed)) {
			user_accessed = iter->second;
		}
		
		// default queue exists and the user is creating a queue for this device for the first time
		// -> return the default queue
		if(!user_accessed) {
			// signal that the user has accessed the default queue, any subsequent create_queue calls will actually create a new queue
			default_queues_user_accessed.emplace(&dev, 1u);
			return dev_default_queue;
		}
	}
	
	// create the queue
	cl_int create_err = CL_SUCCESS;
#if /*defined(CL_VERSION_2_0) ||*/ 0 // TODO: should only be enabled if platform (and device?) support OpenCL 2.0+
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
		log_error("failed to create command queue: $: $", create_err, cl_error_to_string(create_err));
		return {};
	}
	
	auto ret = std::make_shared<opencl_queue>(dev, cl_queue);
	queues.push_back(ret);
	
	// set the default queue if it hasn't been set yet
	if(!has_default_queue) {
		default_queues.emplace(&dev, ret);
	}
	
	return ret;
}

const device_queue* opencl_context::get_device_default_queue(const device& dev) const {
	for (auto&& default_queue : default_queues) {
		if(default_queue.first == &dev) {
			return default_queue.second.get();
		}
	}
	// only happens if the context is invalid (the default queues haven't been created)
	log_error("no default queue for this device exists yet!");
	return nullptr;
}

std::unique_ptr<device_fence> opencl_context::create_fence(const device_queue&) const {
	log_error("fence creation not yet supported by opencl_context!");
	return {};
}

std::shared_ptr<device_buffer> opencl_context::create_buffer(const device_queue& cqueue,
														 const size_t& size, const MEMORY_FLAG flags) const {
	return add_resource(std::make_shared<opencl_buffer>(cqueue, size, flags));
}

std::shared_ptr<device_buffer> opencl_context::create_buffer(const device_queue& cqueue,
														 std::span<uint8_t> data,
														 const MEMORY_FLAG flags) const {
	return add_resource(std::make_shared<opencl_buffer>(cqueue, data.size_bytes(), data, flags));
}

std::shared_ptr<device_image> opencl_context::create_image(const device_queue& cqueue,
													   const uint4 image_dim,
													   const IMAGE_TYPE image_type,
													   std::span<uint8_t> data,
													   const MEMORY_FLAG flags,
													   const uint32_t mip_level_limit) const {
	return add_resource(std::make_shared<opencl_image>(cqueue, image_dim, image_type, data, flags, mip_level_limit));
}

std::shared_ptr<device_program> opencl_context::create_program_from_archive_binaries(universal_binary::archive_binaries& bins) {
	// create the program
	opencl_program::program_map_type prog_map;
	for (size_t i = 0, dev_count = devices.size(); i < dev_count; ++i) {
		const auto cl_dev = (const opencl_device*)devices[i].get();
		const auto& dev_best_bin = bins.dev_binaries[i];
		const auto func_info = universal_binary::translate_function_info(dev_best_bin.first->function_info);
		
		prog_map.insert_or_assign(cl_dev,
								  create_opencl_program_internal(*cl_dev,
																 dev_best_bin.first->data.data(),
																 dev_best_bin.first->data.size(),
																 func_info,
																 dev_best_bin.second.opencl.is_spir ?
																 toolchain::TARGET::SPIR :
																 toolchain::TARGET::SPIRV_OPENCL,
																 false /* TODO: true? */));
	}
	return add_program(std::move(prog_map));
}

std::shared_ptr<device_program> opencl_context::add_universal_binary(const std::string& file_name) {
	auto bins = universal_binary::load_dev_binaries_from_archive(file_name, *this);
	if (bins.ar == nullptr || bins.dev_binaries.empty()) {
		log_error("failed to load universal binary: $", file_name);
		return {};
	}
	return create_program_from_archive_binaries(bins);
}

std::shared_ptr<device_program> opencl_context::add_universal_binary(const std::span<const uint8_t> data) {
	auto bins = universal_binary::load_dev_binaries_from_archive(data, *this);
	if (bins.ar == nullptr || bins.dev_binaries.empty()) {
		log_error("failed to load universal binary (in-memory data)");
		return {};
	}
	return create_program_from_archive_binaries(bins);
}

std::shared_ptr<opencl_program> opencl_context::add_program(opencl_program::program_map_type&& prog_map) {
	// create the program object, which in turn will create function objects for all functions in the program,
	// for all devices contained in the program map
	auto prog = std::make_shared<opencl_program>(std::move(prog_map));
	{
		GUARD(programs_lock);
		programs.push_back(prog);
	}
	return prog;
}

std::shared_ptr<device_program> opencl_context::add_program_file(const std::string& file_name,
															 const std::string additional_options) {
	compile_options options { .cli = additional_options };
	return add_program_file(file_name, options);
}

std::shared_ptr<device_program> opencl_context::add_program_file(const std::string& file_name,
															 compile_options options) {
	// compile the source file for all devices in the context
	opencl_program::program_map_type prog_map;
	for(const auto& dev : devices) {
		options.target = (((const opencl_device&)*dev).spirv_version != SPIRV_VERSION::NONE ?
						  toolchain::TARGET::SPIRV_OPENCL : toolchain::TARGET::SPIR);
		prog_map.insert_or_assign((const opencl_device*)dev.get(),
								  create_opencl_program(*dev, toolchain::compile_program_file(*dev, file_name, options),
														options.target));
	}
	return add_program(std::move(prog_map));
}

std::shared_ptr<device_program> opencl_context::add_program_source(const std::string& source_code,
															   const std::string additional_options) {
	compile_options options { .cli = additional_options };
	return add_program_source(source_code, options);
}

std::shared_ptr<device_program> opencl_context::add_program_source(const std::string& source_code,
															   compile_options options) {
	// compile the source code for all devices in the context
	opencl_program::program_map_type prog_map;
	for(const auto& dev : devices) {
		options.target = (((const opencl_device&)*dev).spirv_version != SPIRV_VERSION::NONE ?
						  toolchain::TARGET::SPIRV_OPENCL : toolchain::TARGET::SPIR);
		prog_map.insert_or_assign((const opencl_device*)dev.get(),
								  create_opencl_program(*dev, toolchain::compile_program(*dev, source_code, options),
														options.target));
	}
	return add_program(std::move(prog_map));
}

opencl_program::opencl_program_entry opencl_context::create_opencl_program(const device& dev,
																		   toolchain::program_data program,
																		   const toolchain::TARGET& target) {
	if(!program.valid) {
		return {};
	}
	
	if (target == toolchain::TARGET::SPIRV_OPENCL) {
		// SPIR-V binary, loaded from a file
		size_t spirv_binary_size = 0;
		auto spirv_binary = spirv_handler::load_binary(program.data_or_filename, spirv_binary_size);
		if (!floor::get_toolchain_keep_temp() && file_io::is_file(program.data_or_filename)) {
			// cleanup if file exists
			std::error_code ec {};
			(void)std::filesystem::remove(program.data_or_filename, ec);
		}
		if (spirv_binary == nullptr) return {}; // already prints an error
		
		return create_opencl_program_internal((const opencl_device&)dev,
											  (const void*)spirv_binary.get(), spirv_binary_size,
											  program.function_info, target,
											  program.options.silence_debug_output);
	} else {
		// SPIR binary, already in memory
		return create_opencl_program_internal((const opencl_device&)dev,
											  (const void*)program.data_or_filename.data(),
											  program.data_or_filename.size(),
											  program.function_info, target,
											  program.options.silence_debug_output);
	}
	
}

opencl_program::opencl_program_entry
opencl_context::create_opencl_program_internal(const opencl_device& cl_dev,
											   const void* program_data,
											   const size_t& program_size,
											   const std::vector<toolchain::function_info>& functions,
											   const toolchain::TARGET& target,
											   const bool& silence_debug_output) {
	opencl_program::opencl_program_entry ret;
	ret.functions = functions;
	
	// create the program object ...
	cl_int create_err = CL_SUCCESS;
	if(target != toolchain::TARGET::SPIRV_OPENCL) {
		// OpenCL API handling
		cl_int binary_status = CL_SUCCESS;
		
		ret.program = clCreateProgramWithBinary(ctx, 1, &cl_dev.device_id,
												&program_size, (const unsigned char**)&program_data,
												&binary_status, &create_err);
		if(create_err != CL_SUCCESS) {
			log_error("failed to create OpenCL program: $: $", create_err, cl_error_to_string(create_err));
			log_error("devices binary status: $", std::to_string(binary_status));
			return ret;
		}
		else if(!silence_debug_output) {
			log_debug("successfully created OpenCL program!");
		}
	}
	else {
		ret.program = cl_create_program_with_il(ctx, program_data, program_size, &create_err);
		if(create_err != CL_SUCCESS) {
			log_error("failed to create OpenCL program from IL/SPIR-V: $: $", create_err, cl_error_to_string(create_err));
			return ret;
		}
		else if(!silence_debug_output) {
			log_debug("successfully created OpenCL program (from IL/SPIR-V)!");
		}
	}
	
	// ... and build it
	const std::string build_options {
		(target == toolchain::TARGET::SPIR ? " -x spir -spir-std=1.2" : "")
	};
	CL_CALL_ERR_PARAM_RET(clBuildProgram(ret.program,
										 1, &cl_dev.device_id,
										 build_options.c_str(), nullptr, nullptr),
						  build_err, "failed to build OpenCL program", ret)
	
	
	// print out build log
	if(!silence_debug_output) {
		log_debug("build log: $", cl_get_info<CL_PROGRAM_BUILD_LOG>(ret.program, cl_dev.device_id));
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

std::shared_ptr<device_program> opencl_context::add_precompiled_program_file(const std::string& file_name floor_unused,
																		 const std::vector<toolchain::function_info>& functions floor_unused) {
	// TODO: !
	log_error("not yet supported by opencl_context!");
	return {};
}

std::shared_ptr<device_program::program_entry> opencl_context::create_program_entry(const device& dev,
																				toolchain::program_data program,
																				const toolchain::TARGET target) {
	return std::make_shared<opencl_program::opencl_program_entry>(create_opencl_program(dev, program, target));
}

// from opencl_common: just forward to the context function
cl_int floor_opencl_get_kernel_sub_group_info(cl_kernel kernel,
											  const opencl_context* ctx,
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


cl_int opencl_context::get_kernel_sub_group_info(cl_kernel kernel,
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
				// OpenCL 2.1+ or cl_khr_subgroups or cl_intel_subgroups
				case CL_KERNEL_MAX_SUB_GROUP_SIZE_FOR_NDRANGE:
				case CL_KERNEL_SUB_GROUP_COUNT_FOR_NDRANGE:
					return cl_get_kernel_sub_group_info(kernel, device, param_name, input_value_size, input_value,
														param_value_size, param_value, param_value_size_ret);
					
				// OpenCL 2.1+
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

device_context::memory_usage_t opencl_context::get_memory_usage(const device& dev) const {
	const auto& cl_dev = (const opencl_device&)dev;
	
	const auto total_mem = cl_dev.global_mem_size;
	memory_usage_t ret {
		.global_mem_used = 0u, // NOTE/TODO: no standard way of getting this
		.global_mem_total = total_mem,
		.heap_used = 0u,
		.heap_total = 0u,
	};
	return ret;
}

} // namespace fl

#endif
