/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2015 Florian Ziesche
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

#define FLOOR_OPENCL_INFO_FUNCS 1
#include <floor/compute/opencl/opencl_compute.hpp>
#include <floor/core/platform.hpp>
#include <floor/core/gl_support.hpp>
#include <floor/core/logger.hpp>
#include <floor/core/core.hpp>
#include <floor/core/file_io.hpp>

#if !defined(FLOOR_NO_OPENCL)

#if defined(__APPLE__)
#if defined(FLOOR_IOS)
#include <floor/ios/ios_helper.hpp>
#else
#include <floor/osx/osx_helper.hpp>
#endif
#endif

#include <floor/compute/llvm_compute.hpp>

void opencl_compute::init(const bool use_platform_devices,
						  const size_t platform_index,
						  const bool gl_sharing,
						  const unordered_set<string> device_restriction) {
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
		log_warn("invalid platform index \"%s\" - starting at 0 instead!");
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
		
		if(use_platform_devices) {
			log_debug("found %u opencl device%s", all_cl_devices.size(), (all_cl_devices.size() == 1 ? "" : "s"));
		}
		
		//
		vector<cl_device_id> ctx_cl_devices;
#if defined(__APPLE__)
		platform_vendor = PLATFORM_VENDOR::APPLE;
		
		// if gl sharing is enabled, but a device restriction is specified that doesn't contain "GPU",
		// an opengl sharegroup (gl sharing) may not be used, since this would add gpu devices to the context
		bool apple_gl_sharing = gl_sharing;
		if(!device_restriction.empty() && device_restriction.count("GPU") == 0) {
			log_error("opencl device restriction set to disallow GPUs, but gl sharing is enabled - disabling gl sharing!");
			apple_gl_sharing = false;
		}
		
		cl_context_properties cl_properties[] {
			CL_CONTEXT_PLATFORM, (cl_context_properties)platform,
			apple_gl_sharing ? CL_CONTEXT_PROPERTY_USE_CGL_SHAREGROUP_APPLE : 0,
#if !defined(FLOOR_IOS)
			apple_gl_sharing ? (cl_context_properties)CGLGetShareGroup(CGLGetCurrentContext()) : 0,
#else
			apple_gl_sharing ? (cl_context_properties)ios_helper::get_eagl_sharegroup() : 0,
#endif
			0
		};
		
		// from cl_gl_ext.h:
		// "If the <num_devices> and <devices> argument values to clCreateContext are 0 and NULL respectively,
		// all CL compliant devices in the CGL share group will be used to create the context.
		// Additional CL devices can also be specified using the <num_devices> and <devices> arguments.
		// These, however, cannot be GPU devices. On Mac OS X, you can add the CPU to the list of CL devices
		// (in addition to the CL compliant devices in the CGL share group) used to create the CL context.
		// Note that if a CPU device is specified, the CGL share group must also include the GL float renderer;
		// Otherwise CL_INVALID_DEVICE will be returned."
		// -> create a vector of all cpu devices and create the context
		if(apple_gl_sharing) {
			if(device_restriction.empty() || device_restriction.count("CPU") > 0) {
				for(const auto& device : all_cl_devices) {
					if(cl_get_info<CL_DEVICE_TYPE>(device) == CL_DEVICE_TYPE_CPU) {
						ctx_cl_devices.emplace_back(device);
					}
				}
			}
		}
		else {
			ctx_cl_devices = all_cl_devices;
		}
		
		CL_CALL_ERR_PARAM_CONT(ctx = clCreateContext(cl_properties, (cl_uint)ctx_cl_devices.size(), ctx_cl_devices.data(),
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
		
		if(use_platform_devices) {
			CL_CALL_ERR_PARAM_CONT(ctx = clCreateContext(cl_properties, (cl_uint)ctx_cl_devices.size(), ctx_cl_devices.data(),
														 nullptr, nullptr, &ctx_error),
								   ctx_error, "failed to create opencl context")
		}
		else {
			CL_CALL_ERR_PARAM_CONT(ctx = clCreateContextFromType(cl_properties, CL_DEVICE_TYPE_ALL,
														 nullptr, nullptr, &ctx_error),
								   ctx_error, "failed to create opencl context")
		}
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
		const string platform_str = cl_get_info<CL_PLATFORM_NAME>(platform);
		const string platform_vendor_str = core::str_to_lower(platform_str);
		if(platform_vendor_str.find("nvidia") != string::npos) {
			platform_vendor = PLATFORM_VENDOR::NVIDIA;
		}
		else if(platform_vendor_str.find("amd") != string::npos) {
			platform_vendor = PLATFORM_VENDOR::AMD;
		}
		else if(platform_vendor_str.find("intel") != string::npos) {
			platform_vendor = PLATFORM_VENDOR::INTEL;
		}
		else if(platform_vendor_str.find("freeocl") != string::npos) {
			platform_vendor = PLATFORM_VENDOR::FREEOCL;
		}
#endif
		
		//
		const auto extract_cl_version = [](const string& cl_version_str, const string str_start) -> pair<bool, OPENCL_VERSION> {
			// "OpenCL X.Y" or "OpenCL C X.Y" required by spec (str_start must be either)
			const size_t start_len = str_start.length();
			if(cl_version_str.length() >= (start_len + 3) && cl_version_str.substr(0, start_len) == str_start) {
				const string version_str = cl_version_str.substr(start_len, cl_version_str.find(" ", start_len) - start_len);
				const size_t dot_pos = version_str.find(".");
				if(string2size_t(version_str.substr(0, dot_pos)) > 1) {
					// major version is higher than 1 -> pretend we're running on CL 2.0
					return { true, OPENCL_VERSION::OPENCL_2_0 };
				}
				else {
					switch(string2size_t(version_str.substr(dot_pos+1, version_str.length()-dot_pos-1))) {
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
			platform_vendor = PLATFORM_VENDOR::POCL;
		}
		
		//
		log_msg("opencl platform \"%s\" version recognized as CL%s",
				  platform_vendor_to_str(platform_vendor),
				  (platform_cl_version == OPENCL_VERSION::OPENCL_1_0 ? "1.0" :
				   (platform_cl_version == OPENCL_VERSION::OPENCL_1_1 ? "1.1" :
					(platform_cl_version == OPENCL_VERSION::OPENCL_1_2 ? "1.2" : "2.0"))));
		
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
			
			// device restriction
			if(!device_restriction.empty()) {
				switch(cl_get_info<CL_DEVICE_TYPE>(cl_dev)) {
					case CL_DEVICE_TYPE_CPU:
						if(device_restriction.count("CPU") == 0) continue;
						break;
					case CL_DEVICE_TYPE_GPU:
						if(device_restriction.count("GPU") == 0) continue;
						break;
					case CL_DEVICE_TYPE_ACCELERATOR:
						if(device_restriction.count("ACCELERATOR") == 0) continue;
						break;
					default: break;
				}
			}
			
			devices.emplace_back(make_shared<opencl_device>());
			auto device_sptr = devices.back();
			auto& device = *(opencl_device*)device_sptr.get();
			dev_type_str = "";
			
			device.device_id = cl_dev;
			device.internal_type = (uint32_t)cl_get_info<CL_DEVICE_TYPE>(cl_dev);
			device.units = cl_get_info<CL_DEVICE_MAX_COMPUTE_UNITS>(cl_dev);
			device.clock = cl_get_info<CL_DEVICE_MAX_CLOCK_FREQUENCY>(cl_dev);
			device.global_mem_size = cl_get_info<CL_DEVICE_GLOBAL_MEM_SIZE>(cl_dev);
			device.local_mem_size = cl_get_info<CL_DEVICE_LOCAL_MEM_SIZE>(cl_dev);
			device.constant_mem_size = cl_get_info<CL_DEVICE_MAX_CONSTANT_BUFFER_SIZE>(cl_dev);
			device.name = cl_get_info<CL_DEVICE_NAME>(cl_dev);
			device.vendor_name = cl_get_info<CL_DEVICE_VENDOR>(cl_dev);
			device.version_str = cl_get_info<CL_DEVICE_VERSION>(cl_dev);
			device.driver_version_str = cl_get_info<CL_DRIVER_VERSION>(cl_dev);
			device.extensions = core::tokenize(core::trim(cl_get_info<CL_DEVICE_EXTENSIONS>(cl_dev)), ' ');
			
			device.max_mem_alloc = cl_get_info<CL_DEVICE_MAX_MEM_ALLOC_SIZE>(cl_dev);
			device.max_workgroup_size = cl_get_info<CL_DEVICE_MAX_WORK_GROUP_SIZE>(cl_dev);
			const auto max_workgroup_sizes = cl_get_info<CL_DEVICE_MAX_WORK_ITEM_SIZES>(cl_dev);
			if(max_workgroup_sizes.size() != 3) {
				log_warn("max workgroup sizes dim != 3: %u", max_workgroup_sizes.size());
			}
			if(max_workgroup_sizes.size() >= 1) device.max_workgroup_sizes.x = max_workgroup_sizes[0];
			if(max_workgroup_sizes.size() >= 2) device.max_workgroup_sizes.y = max_workgroup_sizes[1];
			if(max_workgroup_sizes.size() >= 3) device.max_workgroup_sizes.z = max_workgroup_sizes[2];
			
			device.image_support = (cl_get_info<CL_DEVICE_IMAGE_SUPPORT>(cl_dev) == 1);
			device.max_image_2d_dim.set(cl_get_info<CL_DEVICE_IMAGE2D_MAX_WIDTH>(cl_dev),
										cl_get_info<CL_DEVICE_IMAGE2D_MAX_HEIGHT>(cl_dev));
			device.max_image_3d_dim.set(cl_get_info<CL_DEVICE_IMAGE3D_MAX_WIDTH>(cl_dev),
										cl_get_info<CL_DEVICE_IMAGE3D_MAX_HEIGHT>(cl_dev),
										cl_get_info<CL_DEVICE_IMAGE3D_MAX_DEPTH>(cl_dev));
			device.double_support = (cl_get_info<CL_DEVICE_DOUBLE_FP_CONFIG>(cl_dev) != 0);
			
			log_msg("address space size: %u", cl_get_info<CL_DEVICE_ADDRESS_BITS>(cl_dev));
			log_msg("max mem alloc: %u bytes / %u MB",
					device.max_mem_alloc,
					device.max_mem_alloc / 1024ULL / 1024ULL);
			log_msg("mem size: %u MB (global), %u KB (local), %u KB (constant)",
					device.global_mem_size / 1024ULL / 1024ULL,
					device.local_mem_size / 1024ULL,
					device.constant_mem_size / 1024ULL);
			log_msg("mem base address alignment: %u", cl_get_info<CL_DEVICE_MEM_BASE_ADDR_ALIGN>(cl_dev));
			log_msg("min data type alignment size: %u", cl_get_info<CL_DEVICE_MIN_DATA_TYPE_ALIGN_SIZE>(cl_dev));
			log_msg("host unified memory: %u", cl_get_info<CL_DEVICE_HOST_UNIFIED_MEMORY>(cl_dev));
			log_msg("max_wg_size: %u", device.max_workgroup_size);
			log_msg("max_wi_sizes: %v", device.max_workgroup_sizes);
			log_msg("max param size: %u", cl_get_info<CL_DEVICE_MAX_PARAMETER_SIZE>(cl_dev));
			log_msg("double support: %b", device.double_support);
			log_msg("image support: %b", device.image_support);
			if(// pocl has no support for this yet
			   platform_vendor != PLATFORM_VENDOR::POCL) {
				const unsigned long long int printf_buffer_size = cl_get_info<CL_DEVICE_PRINTF_BUFFER_SIZE>(cl_dev);
				log_msg("printf buffer size: %u bytes / %u MB",
						printf_buffer_size,
						printf_buffer_size / 1024ULL / 1024ULL);
				log_msg("max sub-devices: %u", cl_get_info<CL_DEVICE_PARTITION_MAX_SUB_DEVICES>(cl_dev));
				if(platform_vendor != PLATFORM_VENDOR::FREEOCL) {
					// this is broken on freeocl
					log_msg("built-in kernels: %s", cl_get_info<CL_DEVICE_BUILT_IN_KERNELS>(cl_dev));
				}
			}
			log_msg("extensions: \"%s\"", core::trim(cl_get_info<CL_DEVICE_EXTENSIONS>(cl_dev)));
			
			device.vendor = compute_device::VENDOR::UNKNOWN;
			string vendor_str = core::str_to_lower(device.vendor_name);
			if(strstr(vendor_str.c_str(), "nvidia") != nullptr) {
				device.vendor = compute_device::VENDOR::NVIDIA;
			}
			else if(strstr(vendor_str.c_str(), "intel") != nullptr) {
				device.vendor = compute_device::VENDOR::INTEL;
			}
			else if(strstr(vendor_str.c_str(), "apple") != nullptr) {
				device.vendor = compute_device::VENDOR::APPLE;
			}
			else if(strstr(vendor_str.c_str(), "amd") != nullptr ||
					// "ati" should be tested last, since it also matches "corporation"
					strstr(vendor_str.c_str(), "ati") != nullptr) {
				device.vendor = compute_device::VENDOR::AMD;
			}
			
			// freeocl and pocl use an empty device name, but "FreeOCL"/"pocl" is contained in the device version
			if(device.version_str.find("FreeOCL") != string::npos) {
				device.vendor = compute_device::VENDOR::FREEOCL;
			}
			if(device.version_str.find("pocl") != string::npos) {
				device.vendor = compute_device::VENDOR::POCL;
				
				// device unit count on pocl is 0 -> figure out how many logical cpus actually exist
				if(device.units == 0) {
#if defined(__FreeBSD__)
					int core_count = 0;
					size_t size = sizeof(core_count);
					sysctlbyname("hw.ncpu", &core_count, &size, nullptr, 0);
					device.units = core_count;
#else
					// TODO: other platforms?
					device.units = 1;
#endif
				}
			}
			
			if(device.internal_type & CL_DEVICE_TYPE_CPU) {
				device.type = (compute_device::TYPE)cpu_counter;
				cpu_counter++;
				dev_type_str += "CPU ";
				
				if(fastest_cpu_device == nullptr) {
					fastest_cpu_device = device_sptr;
					fastest_cpu_score = device.units * device.clock;
				}
				else {
					cpu_score = device.units * device.clock;
					if(cpu_score > fastest_cpu_score) {
						fastest_cpu_device = device_sptr;
						fastest_cpu_score = cpu_score;
					}
				}
			}
			if(device.internal_type & CL_DEVICE_TYPE_GPU) {
				device.type = (compute_device::TYPE)gpu_counter;
				gpu_counter++;
				dev_type_str += "GPU ";
				const auto compute_gpu_score = [](const compute_device& dev) -> unsigned int {
					unsigned int multiplier = 1;
					switch(dev.vendor) {
						case compute_device::VENDOR::NVIDIA:
							// fermi or kepler+ card if wg size is >= 1024
							multiplier = (dev.max_workgroup_size >= 1024 ? 32 : 8);
							break;
						case compute_device::VENDOR::AMD:
							multiplier = 16;
							break;
						// none for INTEL
						default: break;
					}
					return multiplier * (dev.units * dev.clock);
				};
				
				if(fastest_gpu_device == nullptr) {
					fastest_gpu_device = device_sptr;
					fastest_gpu_score = compute_gpu_score(device);
				}
				else {
					gpu_score = compute_gpu_score(device);
					if(gpu_score > fastest_gpu_score) {
						fastest_gpu_device = device_sptr;
						fastest_gpu_score = gpu_score;
					}
				}
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
			
			// TYPE (Units: %, Clock: %): Name, Vendor, Version, Driver Version
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

			// there is no spir support on apple platforms, so don't even try this
			// TODO: figure out how to hook into apples llvm opencl compiler (which is based on clang/llvm 3.2 as well)
#if !defined(__APPLE__)
			if(!core::contains(device.extensions, "cl_khr_spir")) {
				log_error("device does not support \"cl_khr_spir\", removing it!");
				devices.pop_back();
				continue;
			}
#endif
		}
		
		// no supported devices found
		if(devices.empty()) {
			log_error("no supported device found on this platform!");
			continue;
		}
		
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
	
	// if absolutely no devices on any platform are supported, disable opencl support
	if(devices.empty()) {
		supported = false;
		return;
	}
	// else: init successful, set supported to true
	supported = true;
	
	// context has been created, query image format information
	image_formats.clear();
	if(platform_vendor != PLATFORM_VENDOR::POCL) {
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
		return uint2string(channel_order);
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
		return uint2string(channel_type);
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
	
	// create the queue (w/ or w/o profiling support depending on the define)
	cl_int create_err = CL_SUCCESS;
	// TODO: add support for clCreateCommandQueueWithPropertiesAPPLE?
	cl_command_queue cl_queue = clCreateCommandQueue(ctx, ((opencl_device*)dev.get())->device_id,
#if !defined(FLOOR_CL_PROFILING)
													 0
#else
													 CL_QUEUE_PROFILING_ENABLE
#endif
													 , &create_err);
	if(create_err != CL_SUCCESS) {
		log_error("failed to create command queue: %u", create_err);
		return {};
	}
	
	auto ret = make_shared<opencl_queue>(cl_queue);
	queues.push_back(ret);
	return ret;
}

shared_ptr<compute_buffer> opencl_compute::create_buffer(const size_t& size, const COMPUTE_BUFFER_FLAG flags) {
	return make_shared<opencl_buffer>(ctx, size, flags);
}

shared_ptr<compute_buffer> opencl_compute::create_buffer(const size_t& size, const void* data, const COMPUTE_BUFFER_FLAG flags) {
	return make_shared<opencl_buffer>(ctx, size, data, flags);
}

void opencl_compute::finish() {
	// TODO: !
}

void opencl_compute::flush() {
	// TODO: !
}

void opencl_compute::activate_context() {
	// TODO: !
}

void opencl_compute::deactivate_context() {
	// TODO: !
}

weak_ptr<compute_program> opencl_compute::add_program_file(const string& file_name,
														   const string additional_options) {
	string code;
	if(!file_io::file_to_string(file_name, code)) {
		return {};
	}
	return add_program_source(code, additional_options);
}

weak_ptr<compute_program> opencl_compute::add_program_source(const string& source_code,
															 const string additional_options) {
	// compile the source code to spir 1.2 (this produces/returns an llvm bitcode binary file)
	const auto spir_bc = llvm_compute::compile_program(source_code, additional_options, llvm_compute::TARGET::SPIR);
	
	// opencl api handling
	vector<size_t> length_ptrs(devices.size());
	vector<const unsigned char*> binary_ptrs(devices.size());
	vector<cl_int> binary_status(devices.size());
	for(size_t i = 0; i < devices.size(); ++i) {
		length_ptrs[i] = spir_bc.size();
		binary_ptrs[i] = (const unsigned char*)spir_bc.data();
		binary_status[i] = CL_SUCCESS;
	}
	
	// create the program object ...
	cl_int create_err = CL_SUCCESS;
	const cl_program program = clCreateProgramWithBinary(ctx, (cl_uint)ctx_devices.size(), (const cl_device_id*)&ctx_devices[0],
														 &length_ptrs[0], &binary_ptrs[0], &binary_status[0], &create_err);
	if(create_err != CL_SUCCESS) {
		log_error("failed to create opencl program: %u", create_err);
		log_error("devices binary status: %s", [&binary_status] {
			string ret;
			for(const auto& status : binary_status) {
				ret += to_string(status) + " ";
			}
			return ret;
		}());
		return {};
	}
	else log_debug("successfully created opencl program!");
	
	// ... and build it
	CL_CALL_ERR_PARAM_RET(clBuildProgram(program,
										 0, nullptr, // build for all devices specified when the program was created
										 additional_options.c_str(), nullptr, nullptr),
						  build_err, "failed to build opencl program", {});
	
	
	// print out build log
	for(const auto& device : ctx_devices) {
		log_debug("build log: %s", cl_get_info<CL_PROGRAM_BUILD_LOG>(program, device));
	}
	
#if 1
	// for testing purposes: retrieve the compiled binaries again
	const auto binaries = cl_get_info<CL_PROGRAM_BINARIES>(program);
	for(size_t i = 0; i < binaries.size(); ++i) {
		file_io::string_to_file("binary_" + to_string(i) + ".bin", binaries[i]);
	}
#endif
	
	// create the program object, which in turn will create kernel objects for all kernel functions in the program
	auto ret_program = make_shared<opencl_program>(program);
	programs.push_back(ret_program);
	return ret_program;
}

#endif
