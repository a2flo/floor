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

void metal_compute::init(const bool use_platform_devices floor_unused,
						 const uint64_t platform_index_ floor_unused,
						 const bool gl_sharing floor_unused,
						 const unordered_set<string> whitelist) {
#if defined(FLOOR_IOS)
	// create the default device, exit if it fails
	id <MTLDevice> mtl_device = MTLCreateSystemDefaultDevice();
	if(!mtl_device) return;
	vector<id <MTLDevice>> mtl_devices { mtl_device };
#else
	auto mtl_devices_arr = MTLCopyAllDevices();
	vector<id<MTLDeviceSPI>> mtl_devices;
	for(id<MTLDevice> dev : mtl_devices_arr) {
		mtl_devices.emplace_back(dev);
	}
#endif
	
	// go through all found devices (for ios, this should be one)
	uint32_t device_num = 0;
	for(auto& dev : mtl_devices) {
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
		devices.emplace_back(make_shared<metal_device>());
		auto device_sptr = devices.back();
		auto& device = *(metal_device*)device_sptr.get();
		device.device = dev;
		device.name = [[dev name] UTF8String];
		device.type = (compute_device::TYPE)(uint32_t(compute_device::TYPE::GPU0) + device_num);
		++device_num;
		
#if defined(FLOOR_IOS)
		// on ios, most of the device properties can't be querried, but are statically known (-> doc)
		device.vendor_name = "Apple";
		device.driver_version_str = "1.0.0"; // for now, should update with iOS >8 (iOS8 versions: metal 1.0.0, air 1.6.0, language 1.0.0)
		device.vendor = compute_device::VENDOR::APPLE;
		device.clock = 450; // actually unknown, and won't matter for now
		device.global_mem_size = (uint64_t)darwin_helper::get_memory_size();
		device.constant_mem_size = 65536; // no idea if this is correct, but it's the min required size for opencl 1.2
		device.max_mem_alloc = device.global_mem_size / 4; // usually the case
		
		// hard to make this forward compatible, there is no direct "get family" call
		// -> just try the first 32 types, good enough for now
		for(uint32_t i = 0; i < 32; ++i) {
			if([dev supportsFeatureSet:(MTLFeatureSet)i]) {
				device.family = i + 1;
				break;
			}
		}
		device.family_version = 1;
		device.version_str = to_string(device.family);
		
		// init statically known device information (pulled from AGXMetal/AGXG*Device and apples doc)
		switch(device.family) {
			case 0:
				log_error("unsupported hardware - family is 0!");
				return;
			case 3:
				device.family_version = 2;
				floor_fallthrough;
			case 1:
				device.family = 1;
				device.units = 4; // G6430
				device.local_mem_size = 16384;
				device.max_work_group_size = 512;
				device.mem_clock = 1600; // ram clock
				break;
				
			default:
				log_warn("unknown device family (%u), defaulting to family 2 (A8)", device.family);
				floor_fallthrough;
			case 4:
				device.family_version = 2;
				floor_fallthrough;
			case 2:
				device.family = 2;
				if(device.name.find("A8X") != string::npos) { // A8X
					device.units = 8; // GXA6850
					device.max_work_group_size = 1024;
				}
				else { // A8
					device.units = 4; // GX6450
					device.max_work_group_size = 512;
				}
				device.local_mem_size = 16384;
				device.mem_clock = 1600; // ram clock
				break;
		}
		device.max_work_group_item_sizes = { 512, 512, 512 };
		device.max_work_item_sizes = { 0xFFFFFFFFu };
		device.double_support = false; // double config is 0
		device.unified_memory = true;
		device.max_image_1d_dim = { 4096 };
		device.max_image_2d_dim = { 4096, 4096 };
		device.max_image_3d_dim = { 4096, 4096, 2048 };
#else
		// on os x, we can get to the device properties through MTLDeviceSPI
		device.vendor_name = [[dev vendorName] UTF8String];
		device.driver_version_str = "1.1.0"; // TODO: proper version info?
		const auto lc_vendor_name = core::str_to_lower(device.vendor_name);
		if(lc_vendor_name.find("nvidia") != string::npos) {
			device.vendor = compute_device::VENDOR::NVIDIA;
		}
		else if(lc_vendor_name.find("intel") != string::npos) {
			device.vendor = compute_device::VENDOR::INTEL;
		}
		else if(lc_vendor_name.find("amd") != string::npos) {
			device.vendor = compute_device::VENDOR::AMD;
		}
		else device.vendor = compute_device::VENDOR::UNKNOWN;
		device.max_mem_alloc = [dev maxBufferLength];
		// TODO: this is not correct, but there is no way to query the global mem size - however, global mem size is usually 4 * max alloc
		device.global_mem_size = device.max_mem_alloc * 4;
		device.constant_mem_size = 65536; // can't query this, so assume opencl minimum
		device.family = (uint32_t)[dev featureProfile];
		device.family_version = device.family - 10000 + 1;
		device.local_mem_size = [dev maxComputeThreadgroupMemory];
		device.max_work_group_size = (uint32_t)[dev maxTotalComputeThreadsPerThreadgroup];
		device.units = 0; // sadly unknown and impossible to query
		device.clock = 0;
		device.mem_clock = 0;
		device.max_work_group_item_sizes = { device.max_work_group_size, device.max_work_group_size, device.max_work_group_size };
		device.max_work_item_sizes = { 0xFFFFFFFFu };
		device.double_support = ([dev doubleFPConfig] > 0);
		device.unified_memory = true; // TODO: not sure about this?
		device.max_image_1d_dim = { [dev maxTextureWidth1D] };
		device.max_image_2d_dim = { [dev maxTextureWidth2D], [dev maxTextureHeight2D] };
		device.max_image_3d_dim = { [dev maxTextureWidth3D], [dev maxTextureHeight3D], [dev maxTextureDepth3D] };
#endif
		device.image_support = true;
		device.bitness = 64; // seems to be true for all devices? (at least nvptx64, igil64 and A7+)
		device.basic_64_bit_atomics_support = false; // not supported at all
		device.extended_64_bit_atomics_support = false; // not supported at all
		
		// done
		supported = true;
		platform_vendor = compute_base::PLATFORM_VENDOR::APPLE;
		log_debug("GPU (Memory: %u MB): %s, family %u",
				  (unsigned int)(device.global_mem_size / 1024ull / 1024ull),
				  device.name,
				  device.family);
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
	if(fastest_gpu_device->vendor != compute_device::VENDOR::NVIDIA &&
	   fastest_gpu_device->vendor != compute_device::VENDOR::AMD) { // if this is false, we already have a nvidia/amd device
		for(size_t i = 1; i < devices.size(); ++i) {
			if(devices[i]->vendor == compute_device::VENDOR::NVIDIA ||
			   devices[i]->vendor == compute_device::VENDOR::AMD) {
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
	
	auto ret = make_shared<metal_queue>(queue);
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

shared_ptr<compute_buffer> metal_compute::create_buffer(const size_t& size, const COMPUTE_MEMORY_FLAG flags,
														const uint32_t opengl_type) {
	return make_shared<metal_buffer>((metal_device*)fastest_device.get(), size, flags, opengl_type);
}

shared_ptr<compute_buffer> metal_compute::create_buffer(const size_t& size, void* data, const COMPUTE_MEMORY_FLAG flags,
														const uint32_t opengl_type) {
	return make_shared<metal_buffer>((metal_device*)fastest_device.get(), size, data, flags, opengl_type);
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

shared_ptr<compute_program> metal_compute::add_program_file(const string& file_name,
															const string additional_options) {
	return add_program(llvm_compute::compile_program_file(fastest_device, file_name, additional_options, llvm_compute::TARGET::AIR),
					   additional_options);
}

shared_ptr<compute_program> metal_compute::add_program_source(const string& source_code,
															  const string additional_options) {
	// compile the source code to apple IR / air (this produces/returns an llvm 3.5 bitcode binary file)
	return add_program(llvm_compute::compile_program(fastest_device, source_code, additional_options, llvm_compute::TARGET::AIR),
					   additional_options);
}

shared_ptr<compute_program> metal_compute::add_program(pair<string, vector<llvm_compute::kernel_info>> program_data,
													   const string additional_options floor_unused) {
	if(program_data.first.empty()) {
		log_error("compilation failed");
		return {};
	}
	
#if !defined(FLOOR_IOS) // can only do this on os x
#define FLOOR_METAL_TOOLS_PATH "/System/Library/PrivateFrameworks/GPUCompiler.framework/Versions/A/Libraries/"
	static constexpr const char* metal_as { FLOOR_METAL_TOOLS_PATH "metal-as" };
	static constexpr const char* metal_ar { FLOOR_METAL_TOOLS_PATH "metal-ar" };
	static constexpr const char* metallib { FLOOR_METAL_TOOLS_PATH "metallib" };
	if(!file_io::string_to_file("metal_tmp.ll", program_data.first)) {
		log_error("failed to write tmp metal ll file");
		return {};
	}
	core::system(metal_as + " -o=metal_tmp.air metal_tmp.ll"s);
	core::system(metal_ar + " r metal_tmp.a metal_tmp.air"s);
	core::system(metallib + " -o metal_tmp.metallib metal_tmp.a"s);
	
	string lib { "" };
	if(!file_io::file_to_string("metal_tmp.metallib", lib)) {
		log_error("failed to create or read metallib");
		return {};
	}
	
	// create the program/library object and build it (note: also need to create an dispatcht_data_t object ...)
	NSError* err { nil };
	dispatch_data_t ddt = dispatch_data_create(lib.c_str(), lib.size(), dispatch_get_main_queue(), ^(void) {});
	id <MTLLibrary> program = [((metal_device*)fastest_device.get())->device newLibraryWithData:ddt error:&err];
	if(!program) {
		log_error("failed to create metal program/library: %s", (err != nil ? [[err localizedDescription] UTF8String] : "unknown"));
		return {};
	}
	
	// TODO: print out the build log
	
	// create the program object, which in turn will create kernel objects for all kernel functions in the program
	auto ret_program = make_shared<metal_program>((metal_device*)fastest_device.get(), program, program_data.second);
	{
		GUARD(programs_lock);
		programs.push_back(ret_program);
	}
	return ret_program;
#else
	log_error("this is not supported on iOS!");
	return {};
#endif
}

shared_ptr<compute_program> metal_compute::add_precompiled_program_file(const string& file_name,
																		const vector<llvm_compute::kernel_info>& kernel_infos) {
	log_debug("loading mtllib: %s", file_name);
	
	string data;
	if(!file_io::file_to_string(file_name, data)) {
		return {};
	}
	
	NSError* err { nil };
	dispatch_data_t ddt = dispatch_data_create(data.c_str(), data.size(), dispatch_get_main_queue(), ^(void) {});
	id <MTLLibrary> program = [((metal_device*)fastest_device.get())->device newLibraryWithData:ddt error:&err];
	if(!program) {
		log_error("failed to create metal program/library: %s", (err != nil ? [[err localizedDescription] UTF8String] : "unknown"));
		return {};
	}
	
#if 0
	//auto path = [NSString stringWithUTF8String:file_name.c_str()];
	auto path = [NSString stringWithUTF8String:"/Users/flo/sync/floor_examples/nbody/src/nbody.cpp"];
	_MTLLibrary* lib = ([program respondsToSelector:@selector(baseObject)] ?
						[program performSelector:@selector(baseObject)] :
						program);
	for(id key in [lib functionDictionary]) {
		id <MTLFunctionSPI> func = [[lib functionDictionary] objectForKey:key];
		func.filePath = path;
	}
#endif
	
	auto ret_program = make_shared<metal_program>((metal_device*)fastest_device.get(), program, kernel_infos);
	{
		GUARD(programs_lock);
		programs.push_back(ret_program);
	}
	return ret_program;
}

#endif
