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

#include <floor/compute/metal/metal_compute.hpp>
#include <floor/core/platform.hpp>
#include <floor/core/gl_support.hpp>
#include <floor/core/logger.hpp>
#include <floor/core/core.hpp>
#include <floor/core/file_io.hpp>

#if !defined(FLOOR_NO_METAL)

#if defined(__APPLE__)
#include <floor/darwin/darwin_helper.hpp>
#endif

#include <floor/compute/llvm_compute.hpp>
#include <floor/compute/metal/metal_buffer.hpp>
#include <floor/compute/metal/metal_image.hpp>
#include <floor/compute/metal/metal_device.hpp>
#include <floor/compute/metal/metal_kernel.hpp>
#include <floor/compute/metal/metal_program.hpp>
#include <floor/compute/metal/metal_queue.hpp>
#include <floor/floor/floor.hpp>
#include <Metal/Metal.h>

// include again to clean up macro mess
#include <floor/core/essentials.hpp>

// only need this on os x right now (won't work on ios 8.0 anyways, different spi)
#if !defined(FLOOR_IOS)
typedef struct {
	unsigned int _field1;
	unsigned int _field2;
} CDStruct_c0454aff;

typedef struct {
	unsigned int _field1;
	unsigned int _field2;
	unsigned int _field3;
	unsigned int _field4;
	unsigned int _field5;
	unsigned int _field6;
	unsigned int _field7;
	unsigned int _field8;
	unsigned int _field9;
	unsigned int _field10;
	unsigned int _field11;
	unsigned int _field12;
	unsigned int _field13;
	unsigned int _field14;
	unsigned int _field15;
	unsigned int _field16;
	unsigned int _field17;
	unsigned int _field18;
	float _field19;
	float _field20;
	unsigned int _field21;
	unsigned int _field22;
	unsigned int _field23;
	unsigned int _field24;
	unsigned int _field25;
	unsigned int _field26;
	unsigned int _field27;
	unsigned int _field28;
	unsigned int _field29;
	unsigned int _field30;
	unsigned int _field31;
	unsigned int _field32;
	unsigned int _field33;
	unsigned int _field34;
} CDStruct_97d836cc;

@protocol MTLDeviceSPI <MTLDevice>
+ (void)registerDevices;
@property(readonly) unsigned int acceleratorPort;
@property(readonly) unsigned long long iosurfaceTextureAlignmentBytes;
@property(readonly) unsigned long long linearTextureAlignmentBytes;
@property(readonly) unsigned long long maxTextureLayers;
@property(readonly) unsigned long long maxTextureDimensionCube;
@property(readonly) unsigned long long maxTextureDepth3D;
@property(readonly) unsigned long long maxTextureHeight3D;
@property(readonly) unsigned long long maxTextureWidth3D;
@property(readonly) unsigned long long maxTextureHeight2D;
@property(readonly) unsigned long long maxTextureWidth2D;
@property(readonly) unsigned long long maxTextureWidth1D;
@property(readonly) unsigned long long minBufferNoCopyAlignmentBytes;
@property(readonly) unsigned long long minConstantBufferAlignmentBytes;
@property(readonly) unsigned long long maxBufferLength;
@property(readonly) unsigned long long maxVisibilityQueryOffset;
@property(readonly) float maxPointSize;
@property(readonly) float maxLineWidth;
@property(readonly) unsigned long long maxComputeThreadgroupMemory;
@property(readonly) unsigned long long maxTotalComputeThreadsPerThreadgroup;
@property(readonly) unsigned long long maxComputeLocalMemorySizes;
@property(readonly) unsigned long long maxComputeInlineDataSize;
@property(readonly) unsigned long long maxComputeSamplers;
@property(readonly) unsigned long long maxComputeTextures;
@property(readonly) unsigned long long maxComputeBuffers;
@property(readonly) unsigned long long maxFragmentInlineDataSize;
@property(readonly) unsigned long long maxFragmentSamplers;
@property(readonly) unsigned long long maxFragmentTextures;
@property(readonly) unsigned long long maxFragmentBuffers;
@property(readonly) unsigned long long maxInterpolants;
@property(readonly) unsigned long long maxVertexInlineDataSize;
@property(readonly) unsigned long long maxVertexSamplers;
@property(readonly) unsigned long long maxVertexTextures;
@property(readonly) unsigned long long maxVertexBuffers;
@property(readonly) unsigned long long maxVertexAttributes;
@property(readonly) unsigned long long maxColorAttachments;
@property(readonly) const CDStruct_97d836cc* limits;
@property(readonly) unsigned long long featureProfile;
@property(nonatomic) BOOL metalAssertionsEnabled;
@property(readonly) unsigned long long doubleFPConfig;
@property(readonly) unsigned long long singleFPConfig;
@property(readonly) unsigned long long halfFPConfig;
@property(readonly, getter=isMagicMipmapSupported) BOOL magicMipmapSupported;
- (CDStruct_c0454aff)pipelineCacheStats;
- (CDStruct_c0454aff)libraryCacheStats;
- (void)unloadShaderCaches;
- (id <MTLTexture>)newTextureWithDescriptor:(MTLTextureDescriptor *)arg1 iosurface:(struct __IOSurface *)arg2 plane:(unsigned long long)arg3;

@optional
@property(readonly, getter=isSystemDefaultDevice) BOOL systemDefaultDevice;
@property BOOL shaderDebugInfoCaching;
- (void)compilerPropagatesThreadPriority:(_Bool)arg1;
- (NSString *)productName;
- (NSString *)familyName;
- (NSString *)vendorName;
- (void)newComputePipelineStateWithDescriptor:(MTLComputePipelineDescriptor *)arg1 completionHandler:(void (^)(id <MTLComputePipelineState>, NSError *))arg2;
- (id <MTLComputePipelineState>)newComputePipelineStateWithDescriptor:(MTLComputePipelineDescriptor *)arg1 error:(id *)arg2;
- (void)unmapShaderSampleBuffer;
- (BOOL)mapShaderSampleBufferWithBuffer:(void*)arg1 capacity:(unsigned long long)arg2 size:(unsigned long long)arg3;
@end
#endif

@protocol MTLFunctionSPI <MTLFunction>
@property(readonly) long long lineNumber;
@property(copy) NSString *filePath;
@end

@interface _MTLLibrary : NSObject <MTLLibrary>
@property(readonly, retain, nonatomic) NSMutableDictionary *functionDictionary;
@end

metal_compute::metal_compute(const vector<string> whitelist) : compute_context() {
#if defined(FLOOR_IOS)
	// create the default device, exit if it fails
	id <MTLDevice> mtl_device = MTLCreateSystemDefaultDevice();
	if(!mtl_device) return;
	NSArray <id<MTLDevice>>* mtl_devices = (NSArray <id<MTLDevice>>*)[NSArray arrayWithObjects:mtl_device, nil];
#else
	NSArray <id<MTLDevice>>* mtl_devices = MTLCopyAllDevices();
#endif
	if(!mtl_devices) return;
	
	// go through all found devices (for ios, this should be one)
	uint32_t device_num = 0;
	for(id <MTLDevice> dev in mtl_devices) {
		// check whitelist
		if(!whitelist.empty()) {
			const auto lc_dev_name = core::str_to_lower([[dev name] UTF8String]);
			bool found = false;
			for(const auto& entry : whitelist) {
				if(lc_dev_name.find(entry) != string::npos) {
					found = true;
					break;
				}
			}
			if(!found) continue;
		}
		
		// add device
		auto device = make_shared<metal_device>();
		devices.emplace_back(device);
		device->device = dev;
		device->name = [[dev name] UTF8String];
		device->type = (compute_device::TYPE)(uint32_t(compute_device::TYPE::GPU0) + device_num);
		++device_num;
		
#if defined(FLOOR_IOS)
		// on ios, most of the device properties can't be querried, but are statically known (-> doc)
		device->vendor_name = "Apple";
		device->vendor = COMPUTE_VENDOR::APPLE;
		device->clock = 450; // actually unknown, and won't matter for now
		device->global_mem_size = (uint64_t)darwin_helper::get_memory_size();
		device->constant_mem_size = 65536; // no idea if this is correct, but it's the min required size for opencl 1.2
		
		// hard to make this forward compatible, there is no direct "get family" call
		// -> just try the first 32 types, good enough for now
		for(uint32_t i = 32; i > 0; --i) {
			if([dev supportsFeatureSet:(MTLFeatureSet)(i - 1)]) {
				device->family = i;
				break;
			}
		}
		device->family_version = 1;
		device->version_str = to_string(device->family);
		
		// init statically known device information (pulled from AGXMetal/AGXG*Device and apples doc)
		switch(device->family) {
			case 0:
				log_error("unsupported hardware - family is 0!");
				return;
				
			// A7/A7X
			case 3:
				device->family_version = 2;
				floor_fallthrough;
			case 1:
				device->family = 1;
				device->units = 4; // G6430
				device->mem_clock = 1600; // ram clock
				device->max_image_1d_dim = { 8192 };
				device->max_image_2d_dim = { 8192, 8192 };
				break;
			
			// A8/A8X
			case 4:
				device->family_version = 2;
				floor_fallthrough;
			case 2:
				device->family = 2;
				if(device->name.find("A8X") != string::npos) {
					device->units = 8; // GXA6850
				}
				else {
					device->units = 4; // GX6450
				}
				device->mem_clock = 1600; // ram clock
				device->max_image_1d_dim = { 8192 };
				device->max_image_2d_dim = { 8192, 8192 };
				break;
			
			// A9/A9X
			default:
				log_warn("unknown device family (%u), defaulting to family 3 (A9)", device->family);
				floor_fallthrough;
			case 5:
				device->family = 3;
				if(device->name.find("A9X") != string::npos) {
					device->units = 12; // GT7800/7900?
				}
				else {
					device->units = 6; // GT7600
				}
				device->mem_clock = 1600; // TODO: ram clock
				device->max_image_1d_dim = { 16384 };
				device->max_image_2d_dim = { 16384, 16384 };
				break;
		}
		device->local_mem_size = 16384;
		device->max_work_group_size = 512;
		device->max_work_item_sizes = { 0xFFFFFFFFu };
		device->double_support = false; // double config is 0
		device->unified_memory = true;
		device->max_image_1d_buffer_dim = { 0 }; // N/A on metal
		device->max_image_3d_dim = { 2048, 2048, 2048 };
		device->simd_width = 32; // always 32 for powervr 6 and 7 series
		device->simd_range = { device->simd_width, device->simd_width };
		device->image_cube_write_support = false;
#else
		__unsafe_unretained id <MTLDeviceSPI> dev_spi = (id <MTLDeviceSPI>)dev;
		
		// on os x, we can get to the device properties through MTLDeviceSPI
		device->vendor_name = [[dev_spi vendorName] UTF8String];
		const auto lc_vendor_name = core::str_to_lower(device->vendor_name);
		if(lc_vendor_name.find("nvidia") != string::npos) {
			device->vendor = COMPUTE_VENDOR::NVIDIA;
			device->simd_width = 32;
			device->simd_range = { device->simd_width, device->simd_width };
		}
		else if(lc_vendor_name.find("intel") != string::npos) {
			device->vendor = COMPUTE_VENDOR::INTEL;
			device->simd_width = 16; // variable (8, 16 or 32), but 16 is a good estimate
			device->simd_range = { 8, 32 };
		}
		else if(lc_vendor_name.find("amd") != string::npos) {
			device->vendor = COMPUTE_VENDOR::AMD;
			device->simd_width = 64;
			device->simd_range = { device->simd_width, device->simd_width };
		}
		else device->vendor = COMPUTE_VENDOR::UNKNOWN;
		device->global_mem_size = 1024ull * 1024ull * 1024ull; // assume 1GiB for now (TODO: any way to fix this?)
		device->constant_mem_size = 65536; // can't query this, so assume opencl minimum
		device->family = (uint32_t)[dev_spi featureProfile];
		device->family_version = device->family - 10000 + 1;
		device->local_mem_size = [dev_spi maxComputeThreadgroupMemory];
		device->max_work_group_size = (uint32_t)[dev_spi maxTotalComputeThreadsPerThreadgroup];
		device->units = 0; // sadly unknown and impossible to query
		device->clock = 0;
		device->mem_clock = 0;
		device->max_work_item_sizes = { 0xFFFFFFFFu };
		device->double_support = ([dev_spi doubleFPConfig] > 0);
		device->unified_memory = true; // TODO: not sure about this?
		device->max_image_1d_buffer_dim = { 0 }; // N/A on metal
		device->max_image_1d_dim = { (uint32_t)[dev_spi maxTextureWidth1D] };
		device->max_image_2d_dim = { (uint32_t)[dev_spi maxTextureWidth2D], (uint32_t)[dev_spi maxTextureHeight2D] };
		device->max_image_3d_dim = { (uint32_t)[dev_spi maxTextureWidth3D], (uint32_t)[dev_spi maxTextureHeight3D], (uint32_t)[dev_spi maxTextureDepth3D] };
		device->image_cube_write_support = true;
#endif
		device->max_mem_alloc = 256ull * 1024ull * 1024ull; // fixed 256MiB for all
		device->max_work_group_item_sizes = {
			(uint32_t)[dev maxThreadsPerThreadgroup].width,
			(uint32_t)[dev maxThreadsPerThreadgroup].height,
			(uint32_t)[dev maxThreadsPerThreadgroup].depth
		};
		log_msg("max work-group item sizes: %v", device->max_work_group_item_sizes);
		
		device->max_mip_levels = image_mip_level_count_from_max_dim(std::max(std::max(device->max_image_2d_dim.max_element(),
																					  device->max_image_3d_dim.max_element()),
																			 device->max_image_1d_dim));
		
		// done
		supported = true;
		platform_vendor = COMPUTE_VENDOR::APPLE;
		log_debug("GPU (Memory: %u MB): %s, family %u, family version %u",
				  (unsigned int)(device->global_mem_size / 1024ull / 1024ull),
				  device->name,
				  device->family,
				  device->family_version);
	}
	
	// check if there is any supported / whitelisted device
	if(devices.empty()) {
		log_error("no valid metal device found!");
		return;
	}
	
	// figure out the fastest device
#if defined(FLOOR_IOS)
	// only one device on ios
	fastest_gpu_device = devices[0];
	fastest_device = fastest_gpu_device;
#else
	// on os x, this is tricky, because we don't get the compute units count and clock speed
	// -> assume devices are returned in order of their speed
	// -> assume that nvidia and amd cards are faster than intel cards
	fastest_gpu_device = devices[0]; // start off with the first device
	if(fastest_gpu_device->vendor != COMPUTE_VENDOR::NVIDIA &&
	   fastest_gpu_device->vendor != COMPUTE_VENDOR::AMD) { // if this is false, we already have a nvidia/amd device
		for(size_t i = 1; i < devices.size(); ++i) {
			if(devices[i]->vendor == COMPUTE_VENDOR::NVIDIA ||
			   devices[i]->vendor == COMPUTE_VENDOR::AMD) {
				// found a nvidia or amd device, consider it the fastest
				fastest_gpu_device = devices[i];
				break;
			}
		}
	}
	fastest_device = fastest_gpu_device;
#endif
	log_debug("fastest GPU device: %s", fastest_gpu_device->name);
	
	// create an internal queue for each device
	for(auto& dev : devices) {
		auto dev_queue = create_queue(dev);
		internal_queues.emplace_back(dev, dev_queue);
		((metal_device*)dev.get())->internal_queue = dev_queue.get();
	}
}

shared_ptr<compute_queue> metal_compute::create_queue(shared_ptr<compute_device> dev) {
	if(dev == nullptr) {
		log_error("nullptr is not a valid device!");
		return {};
	}
	
	id <MTLCommandQueue> queue = [((metal_device*)dev.get())->device newCommandQueue];
	if(queue == nullptr) {
		log_error("failed to create command queue");
		return {};
	}
	
	auto ret = make_shared<metal_queue>(dev, queue);
	queues.push_back(ret);
	return ret;
}

shared_ptr<compute_queue> metal_compute::get_device_internal_queue(shared_ptr<compute_device> dev) const {
	return get_device_internal_queue(dev.get());
}

shared_ptr<compute_queue> metal_compute::get_device_internal_queue(const compute_device* dev) const {
	for(const auto& dev_queue : internal_queues) {
		if(dev_queue.first.get() == dev) {
			return dev_queue.second;
		}
	}
	log_error("no internal queue exists for this device: %s!", dev->name);
	return {};
}

shared_ptr<compute_buffer> metal_compute::create_buffer(shared_ptr<compute_device> device,
														const size_t& size, const COMPUTE_MEMORY_FLAG flags,
														const uint32_t opengl_type) {
	return make_shared<metal_buffer>((metal_device*)device.get(), size, flags, opengl_type);
}

shared_ptr<compute_buffer> metal_compute::create_buffer(shared_ptr<compute_device> device,
														const size_t& size, void* data,
														const COMPUTE_MEMORY_FLAG flags,
														const uint32_t opengl_type) {
	return make_shared<metal_buffer>((metal_device*)device.get(), size, data, flags, opengl_type);
}

shared_ptr<compute_buffer> metal_compute::wrap_buffer(shared_ptr<compute_device> device floor_unused,
													  const uint32_t opengl_buffer floor_unused,
													  const uint32_t opengl_type floor_unused,
													  const COMPUTE_MEMORY_FLAG flags floor_unused) {
	log_error("opengl buffer sharing not supported by metal!");
	return {};
}

shared_ptr<compute_buffer> metal_compute::wrap_buffer(shared_ptr<compute_device> device floor_unused,
													  const uint32_t opengl_buffer floor_unused,
													  const uint32_t opengl_type floor_unused,
													  void* data floor_unused,
													  const COMPUTE_MEMORY_FLAG flags floor_unused) {
	log_error("opengl buffer sharing not supported by metal!");
	return {};
}

shared_ptr<compute_image> metal_compute::create_image(shared_ptr<compute_device> device,
													  const uint4 image_dim,
													  const COMPUTE_IMAGE_TYPE image_type,
													  const COMPUTE_MEMORY_FLAG flags,
													  const uint32_t opengl_type) {
	return make_shared<metal_image>((metal_device*)device.get(), image_dim, image_type, nullptr, flags, opengl_type);
}

shared_ptr<compute_image> metal_compute::create_image(shared_ptr<compute_device> device,
													  const uint4 image_dim,
													  const COMPUTE_IMAGE_TYPE image_type,
													  void* data,
													  const COMPUTE_MEMORY_FLAG flags,
													  const uint32_t opengl_type) {
	return make_shared<metal_image>((metal_device*)device.get(), image_dim, image_type, data, flags, opengl_type);
}

shared_ptr<compute_image> metal_compute::wrap_image(shared_ptr<compute_device> device floor_unused,
													const uint32_t opengl_image floor_unused,
													const uint32_t opengl_target floor_unused,
													const COMPUTE_MEMORY_FLAG flags floor_unused) {
	log_error("opengl image sharing not supported by metal!");
	return {};
}

shared_ptr<compute_image> metal_compute::wrap_image(shared_ptr<compute_device> device floor_unused,
													const uint32_t opengl_image floor_unused,
													const uint32_t opengl_target floor_unused,
													void* data floor_unused,
													const COMPUTE_MEMORY_FLAG flags floor_unused) {
	log_error("opengl image sharing not supported by metal!");
	return {};
}

static shared_ptr<metal_program> add_metal_program(metal_program::program_map_type&& prog_map,
												   vector<shared_ptr<metal_program>>* programs,
												   atomic_spin_lock& programs_lock) REQUIRES(!programs_lock) {
	// create the program object, which in turn will create kernel objects for all kernel functions in the program,
	// for all devices contained in the program map
	auto prog = make_shared<metal_program>(move(prog_map));
	{
		GUARD(programs_lock);
		programs->push_back(prog);
	}
	return prog;
}

static metal_program::metal_program_entry create_metal_program(const metal_device* device floor_unused_on_ios,
															   llvm_compute::program_data program) {
	metal_program::metal_program_entry ret;
	ret.functions = program.functions;
	
	if(!program.valid) {
		return ret;
	}
	
#if !defined(FLOOR_IOS) // can only do this on os x
#define FLOOR_METAL_TOOLS_PATH "/System/Library/PrivateFrameworks/GPUCompiler.framework/Versions/A/Libraries/"
	static constexpr const char* metal_ar { FLOOR_METAL_TOOLS_PATH "metal-ar" };
	static constexpr const char* metal_opt { FLOOR_METAL_TOOLS_PATH "metal-opt" };
	static constexpr const char* metallib { FLOOR_METAL_TOOLS_PATH "metallib" };
	enum : uint32_t {
		METAL_OPT_AIR_FILE = 0,
		METAL_ARCHIVE_FILE,
		METAL_LIB_FILE,
		__MAX_METAL_FILE
	};
	auto tmp_files = core::create_tmp_file_names(array<const char*, __MAX_METAL_FILE> {{
		"metal_opt_air_",
		"metal_ar_",
		"metal_lib_",
	}}, array<const char*, __MAX_METAL_FILE> {{
		".air",
		".a",
		".metallib",
	}});
	
	//
	if(!floor::get_compute_debug()) {
		core::system(metal_opt + " -Oz "s + program.data_or_filename + " -o " + tmp_files[METAL_OPT_AIR_FILE]);
	}
	else {
		tmp_files[METAL_OPT_AIR_FILE] = program.data_or_filename;
	}
	core::system(metal_ar + " r "s + tmp_files[METAL_ARCHIVE_FILE] + " " + tmp_files[METAL_OPT_AIR_FILE]);
	core::system(metallib + " -o "s + tmp_files[METAL_LIB_FILE] + " " + tmp_files[METAL_ARCHIVE_FILE]);
	
	const auto cleanup = [&tmp_files, unopt_file = program.data_or_filename]() {
		core::system("rm "s + tmp_files[METAL_OPT_AIR_FILE]);
		if(!floor::get_compute_debug()) {
			core::system("rm "s + unopt_file);
		}
		core::system("rm "s + tmp_files[METAL_ARCHIVE_FILE]);
		core::system("rm "s + tmp_files[METAL_LIB_FILE]);
	};
	
	// create the program/library object and build it (note: also need to create an dispatcht_data_t object ...)
	NSError* err { nil };
	ret.program = [device->device newLibraryWithFile:[NSString stringWithUTF8String:tmp_files[METAL_LIB_FILE].c_str()]
											   error:&err];
	if(!floor::get_compute_keep_temp()) cleanup();
	if(!ret.program) {
		log_error("failed to create metal program/library for device %s: %s",
				  device->name, (err != nil ? [[err localizedDescription] UTF8String] : "unknown error"));
		return ret;
	}
	
	// TODO: print out the build log
	ret.valid = true;
	return ret;
#else
	log_error("this is not supported on iOS!");
	return ret;
#endif
}

shared_ptr<compute_program> metal_compute::add_program_file(const string& file_name,
															const string additional_options) {
	return add_program_file(file_name, compile_options { .cli = additional_options });
}

shared_ptr<compute_program> metal_compute::add_program_file(const string& file_name,
															compile_options options) {
	// compile the source file for all devices in the context
	metal_program::program_map_type prog_map;
	prog_map.reserve(devices.size());
	options.target = llvm_compute::TARGET::AIR;
	for(const auto& dev : devices) {
		prog_map.insert_or_assign((metal_device*)dev.get(),
								  create_metal_program((metal_device*)dev.get(),
													   llvm_compute::compile_program_file(dev, file_name, options)));
	}
	return add_metal_program(move(prog_map), &programs, programs_lock);
}

shared_ptr<compute_program> metal_compute::add_program_source(const string& source_code,
															  const string additional_options) {
	return add_program_source(source_code, compile_options { .cli = additional_options });
}

shared_ptr<compute_program> metal_compute::add_program_source(const string& source_code,
															  compile_options options) {
	// compile the source code for all devices in the context
	metal_program::program_map_type prog_map;
	prog_map.reserve(devices.size());
	options.target = llvm_compute::TARGET::AIR;
	for(const auto& dev : devices) {
		prog_map.insert_or_assign((metal_device*)dev.get(),
								  create_metal_program((metal_device*)dev.get(),
													   llvm_compute::compile_program(dev, source_code, options)));
	}
	return add_metal_program(move(prog_map), &programs, programs_lock);
}

shared_ptr<compute_program> metal_compute::add_precompiled_program_file(const string& file_name,
																		const vector<llvm_compute::function_info>& functions) {
	log_debug("loading mtllib: %s", file_name);
	
	// assume pre-compiled program is the same for all devices
	metal_program::program_map_type prog_map;
	prog_map.reserve(devices.size());
	for(const auto& dev : devices) {
		metal_program::metal_program_entry entry;
		entry.functions = functions;
		
		NSError* err { nil };
		entry.program = [((metal_device*)dev.get())->device newLibraryWithFile:[NSString stringWithUTF8String:file_name.c_str()]
																		 error:&err];
		if(!entry.program) {
			log_error("failed to create metal program/library for device %s: %s",
					  dev->name, (err != nil ? [[err localizedDescription] UTF8String] : "unknown error"));
			continue;
		}
		entry.valid = true;
		
		prog_map.insert_or_assign((metal_device*)dev.get(), entry);
	}
	return add_metal_program(move(prog_map), &programs, programs_lock);
}

shared_ptr<compute_program::program_entry> metal_compute::create_program_entry(shared_ptr<compute_device> device,
																			   llvm_compute::program_data program,
																			   const llvm_compute::TARGET) {
	return make_shared<metal_program::metal_program_entry>(create_metal_program((metal_device*)device.get(), program));
}

#endif
