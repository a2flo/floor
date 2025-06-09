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

#if !defined(FLOOR_NO_METAL)
#include <floor/device/metal/metal_context.hpp>
#include <floor/core/platform.hpp>
#include <floor/core/logger.hpp>
#include <floor/core/core.hpp>
#include <floor/core/file_io.hpp>
#include <filesystem>

#if defined(__APPLE__)
#include <floor/darwin/darwin_helper.hpp>
#endif

#include <floor/device/toolchain.hpp>
#include <floor/device/universal_binary.hpp>
#include <floor/device/soft_printf.hpp>
#include <floor/device/metal/metal_buffer.hpp>
#include <floor/device/metal/metal_image.hpp>
#include <floor/device/metal/metal_device.hpp>
#include <floor/device/metal/metal_device_query.hpp>
#include <floor/device/metal/metal_fence.hpp>
#include <floor/device/metal/metal_program.hpp>
#include <floor/device/metal/metal_queue.hpp>
#include <floor/device/metal/metal_indirect_command.hpp>
#include <floor/device/metal/metal_pipeline.hpp>
#include <floor/device/metal/metal_pass.hpp>
#include <floor/device/metal/metal_renderer.hpp>
#include <floor/vr/vr_context.hpp>
#include <floor/floor.hpp>
#include <Metal/Metal.h>
#include <Metal/MTLCaptureScope.h>

// include again to clean up macro mess
#include <floor/core/essentials.hpp>

// only need this on macOS right now
#if !defined(FLOOR_IOS) && !defined(FLOOR_VISIONOS)

@protocol MTLDeviceSPI <MTLDevice>
@property(readonly) unsigned long long dedicatedMemorySize;
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
@property(readonly) unsigned long long featureProfile;
@property(nonatomic) BOOL metalAssertionsEnabled;
@property(readonly) unsigned long long doubleFPConfig;
@property(readonly) unsigned long long singleFPConfig;
@property(readonly) unsigned long long halfFPConfig;
@property(readonly, getter=isMagicMipmapSupported) BOOL magicMipmapSupported;
//@property(readonly) int llvmVersion;
@property(readonly) unsigned long long recommendedMaxWorkingSetSize;
@property(readonly) unsigned long long registryID;
@property(readonly) BOOL requiresIABEmulation;
@property(readonly) BOOL supportPriorityBand;

@optional
- (NSString *)productName;
- (NSString *)familyName;
- (NSString *)vendorName;
@end

#endif

#if FLOOR_METAL_INTERNAL_MEM_TRACKING_DEBUGGING
safe_mutex metal_mem_tracking_lock;
uint64_t metal_mem_tracking_leak_total { 0u };
#endif

namespace fl {

metal_context::metal_context(const DEVICE_CONTEXT_FLAGS ctx_flags,
							 const bool has_toolchain_,
							 const bool enable_renderer_,
							 vr_context* vr_ctx_,
							 const std::vector<std::string> whitelist) :
device_context(ctx_flags, has_toolchain_), vr_ctx(vr_ctx_), enable_renderer(enable_renderer_) {
#if defined(FLOOR_IOS) || defined(FLOOR_VISIONOS)
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
	for (id <MTLDevice> dev in mtl_devices) {
		// check whitelist
		if (!whitelist.empty()) {
			const auto lc_dev_name = core::str_to_lower([[dev name] UTF8String]);
			bool found = false;
			for (const auto& entry : whitelist) {
				if (lc_dev_name.find(entry) != std::string::npos) {
					found = true;
					break;
				}
			}
			if (!found) {
				continue;
			}
		}
		
#if !TARGET_OS_SIMULATOR
		// device must support Metal 3
		if (![dev supportsFamily:MTLGPUFamilyMetal3]) {
			continue;
		}
#endif
#if !defined(FLOOR_IOS) && !defined(FLOOR_VISIONOS)
		// should be included in Metal 3 support, but just in case also require this
		if (![dev supportsFamily:MTLGPUFamilyMac2]) {
			continue;
		}
#endif
		
		// add device
		devices.emplace_back(std::make_unique<metal_device>());
		auto& device = (metal_device&)*devices.back();
		device.device = dev;
		device.context = this;
		device.name = [[dev name] UTF8String];
		device.type = (device::TYPE)(uint32_t(device::TYPE::GPU0) + device_num);
		++device_num;
		
		// query device info that is a bit more complicated to get (not via direct device query)
		const auto device_info = metal_device_query::query(dev);
		if (device_info) {
			device.simd_width = device_info->simd_width;
			device.simd_range = { device.simd_width, device.simd_width };
		}
		
#if defined(FLOOR_IOS) || defined(FLOOR_VISIONOS)
		// on ios, most of the device properties can't be querried, but are statically known (-> doc)
		device.vendor_name = "Apple";
		device.vendor = VENDOR::APPLE;
		device.clock = 450; // actually unknown, and won't matter for now
		device.global_mem_size = (uint64_t)darwin_helper::get_memory_size();
		device.constant_mem_size = 65536; // no idea if this is correct, but it's the min required size for OpenCL 1.2
		device.family_type = metal_device::FAMILY_TYPE::APPLE;
#if defined(FLOOR_VISIONOS)
#if !TARGET_OS_SIMULATOR
		device.platform_type = metal_device::PLATFORM_TYPE::VISIONOS;
#else
		device.platform_type = metal_device::PLATFORM_TYPE::VISIONOS_SIMULATOR;
#endif
#else
#if !TARGET_OS_SIMULATOR
		device.platform_type = metal_device::PLATFORM_TYPE::IOS;
#else
		device.platform_type = metal_device::PLATFORM_TYPE::IOS_SIMULATOR;
#endif
#endif
		
		// find max supported Apple* family
		static constexpr const auto max_gpu_family = MTLGPUFamily(1009) /* MTLGPUFamilyApple9 */;
		device.family_tier = 0;
		for (auto family = MTLGPUFamilyApple1; family <= max_gpu_family; family = MTLGPUFamily((NSInteger)family + 1)) {
			if ([dev supportsFamily:family]) {
				device.family_tier = (uint32_t(family) - uint32_t(MTLGPUFamilyApple1)) + 1u;
			}
		}
		assert(device.family_tier >= 7 || TARGET_OS_SIMULATOR);
		
		// figure out which Metal version we can use
#if defined(FLOOR_IOS)
		if (darwin_helper::get_system_version() >= 260000) {
			device.metal_software_version = METAL_VERSION::METAL_4_0;
			device.metal_language_version = METAL_VERSION::METAL_4_0;
		} else if (darwin_helper::get_system_version() >= 180000) {
			device.metal_software_version = METAL_VERSION::METAL_3_2;
			device.metal_language_version = METAL_VERSION::METAL_3_2;
		} else if (darwin_helper::get_system_version() >= 170000) {
			device.metal_software_version = METAL_VERSION::METAL_3_1;
			device.metal_language_version = METAL_VERSION::METAL_3_1;
		} else if (darwin_helper::get_system_version() >= 160000) {
			device.metal_software_version = METAL_VERSION::METAL_3_0;
			device.metal_language_version = METAL_VERSION::METAL_3_0;
		}
#elif defined(FLOOR_VISIONOS)
		if (darwin_helper::get_system_version() >= 260000) {
			device.metal_software_version = METAL_VERSION::METAL_4_0;
			device.metal_language_version = METAL_VERSION::METAL_4_0;
		} else if (darwin_helper::get_system_version() >= 20000) {
			device.metal_software_version = METAL_VERSION::METAL_3_2;
			device.metal_language_version = METAL_VERSION::METAL_3_2;
		}
#else
#error "unhandled OS"
#endif
		
		// init statically known device information (pulled from AGXMetal/AGXG*Device and apples doc)
		switch (device.family_tier) {
			// A14 / M1
			default:
			case 7:
				device.units = ([dev supportsFamily:MTLGPUFamilyMac2] ? 8 /* M2 */ : 4);
				device.mem_clock = 4233;
				device.max_image_1d_dim = { 16384 };
				device.max_image_2d_dim = { 16384, 16384 };
				device.max_total_local_size = 1024;
				break;
				
			// A15 / A16 / M2
			case 8:
#if defined(FLOOR_VISIONOS)
				device.units = 10;
#else
				device.units = ([dev supportsFamily:MTLGPUFamilyMac2] ? 8 /* M2 */ : 5);
#endif
				device.mem_clock = 3200; // or 2133, ...
				device.max_image_1d_dim = { 16384 };
				device.max_image_2d_dim = { 16384, 16384 };
				device.max_total_local_size = 1024;
				break;
				
			// A17 / A18 / M3 / M4
			case 9:
				device.units = 6;
				device.mem_clock = 3200; // TODO: RAM clock
				device.max_image_1d_dim = { 16384 };
				device.max_image_2d_dim = { 16384, 16384 };
				device.max_total_local_size = 1024;
				break;
		}
		device.local_mem_size = std::max(uint32_t([dev maxThreadgroupMemoryLength]), 32768u /* fallback */);
		device.max_global_size = { 0xFFFFFFFFu };
		device.double_support = false; // double config is 0
		device.unified_memory = true; // always true
		device.max_image_1d_buffer_dim = { 0 }; // N/A on metal
		device.max_image_3d_dim = { 2048, 2048, 2048 };
		if (device.simd_width == 0) {
			device.simd_width = 32; // always 32 for Apple A* series
			device.simd_range = { device.simd_width, device.simd_width };
		}
#else
		__unsafe_unretained id <MTLDeviceSPI> dev_spi = (id <MTLDeviceSPI>)dev;
		
		// on macOS, we can get to the device properties through MTLDeviceSPI
		device.vendor_name = [[dev_spi vendorName] UTF8String];
		const auto lc_vendor_name = core::str_to_lower(device.vendor_name);
		if (lc_vendor_name.find("intel") != std::string::npos) {
			device.vendor = VENDOR::INTEL;
			if (device.simd_width == 0) {
				device.simd_width = 16; // variable (8, 16 or 32), but 16 is a good estimate
				device.simd_range = { 8, 32 };
			}
		} else if (lc_vendor_name.find("amd") != std::string::npos) {
			device.vendor = VENDOR::AMD;
		} else if (lc_vendor_name.find("apple") != std::string::npos) {
			device.vendor = VENDOR::APPLE;
		} else {
			device.vendor = VENDOR::UNKNOWN;
		}
		device.global_mem_size = 1024ull * 1024ull * 1024ull; // assume 1GiB for now
		if ([dev_spi respondsToSelector:@selector(dedicatedMemorySize)]) {
			device.global_mem_size = [dev_spi dedicatedMemorySize];
		}
		device.constant_mem_size = 65536; // can't query this, so assume OpenCL minimum
		device.unified_memory = [dev hasUnifiedMemory];
		
		// there is no direct way of querying the highest available feature set
		// -> find the highest (currently known) version
		device.family_type = metal_device::FAMILY_TYPE::MAC;
		// always family tier 2 / Mac2 right now
		device.family_tier = 2;
		//device.image_read_write_support = true; // TODO: enable this when supported by the compiler
		device.platform_type = metal_device::PLATFORM_TYPE::MACOS;
		
		device.local_mem_size = [dev maxThreadgroupMemoryLength];
		device.max_total_local_size = (uint32_t)[dev_spi maxTotalComputeThreadsPerThreadgroup];
		// we sadly can't query the amount of compute units directly
		// -> we can figure out the amount for AMD GPUs via ioreg (-> metal_device_query)
		device.units = (device_info ? device_info->units : 0u);
		device.clock = 0;
		device.mem_clock = 0;
		device.max_global_size = { 0xFFFFFFFFu };
		device.double_support = ([dev_spi doubleFPConfig] > 0);
		device.max_image_1d_buffer_dim = { 0 }; // N/A on metal
		device.max_image_1d_dim = { (uint32_t)[dev_spi maxTextureWidth1D] };
		device.max_image_2d_dim = { (uint32_t)[dev_spi maxTextureWidth2D], (uint32_t)[dev_spi maxTextureHeight2D] };
		device.max_image_3d_dim = { (uint32_t)[dev_spi maxTextureWidth3D], (uint32_t)[dev_spi maxTextureHeight3D], (uint32_t)[dev_spi maxTextureDepth3D] };
		
		// figure out which Metal version we can use
		if (darwin_helper::get_system_version() >= 260000) {
			device.metal_software_version = METAL_VERSION::METAL_4_0;
			device.metal_language_version = METAL_VERSION::METAL_4_0;
		} else if (darwin_helper::get_system_version() >= 150000) {
			device.metal_software_version = METAL_VERSION::METAL_3_2;
			device.metal_language_version = METAL_VERSION::METAL_3_2;
		} else if (darwin_helper::get_system_version() >= 140000) {
			device.metal_software_version = METAL_VERSION::METAL_3_1;
			device.metal_language_version = METAL_VERSION::METAL_3_1;
		} else if (darwin_helper::get_system_version() >= 130000) {
			device.metal_software_version = METAL_VERSION::METAL_3_0;
			device.metal_language_version = METAL_VERSION::METAL_3_0;
		}
#endif
		assert(device.max_total_local_size == 1024u); // should always be the case with Metal3
		assert(device.local_mem_size >= 32768u);
		
		if (device.vendor == VENDOR::APPLE) {
			// from the limited amount of hardware info that Apple has shown for the M1,
			// we know that 8 GPU cores can have 24576 concurrent/resident threads
			// -> this translates to 3072 per "core"
			// NOTE: this may not be exactly how the hardware actually works and these threads may
			//       live in a GPU global space that may be dynamically redistributed
			device.max_resident_local_size = 3072u;
		} else {
			device.max_resident_local_size = device.max_total_local_size;
		}
		
		device.max_mem_alloc = [dev maxBufferLength];
		// adjust global memory size (might have been invalid)
		device.global_mem_size = std::max(device.global_mem_size, std::max([dev recommendedMaxWorkingSetSize], device.max_mem_alloc));
		device.max_group_size = { 0xFFFFFFFFu };
		device.max_local_size = {
			(uint32_t)[dev maxThreadsPerThreadgroup].width,
			(uint32_t)[dev maxThreadsPerThreadgroup].height,
			(uint32_t)[dev maxThreadsPerThreadgroup].depth
		};
		log_msg("max total local size: $' (max resident $')", device.max_total_local_size, device.max_resident_local_size);
		log_msg("max local size: $'", device.max_local_size);
		log_msg("max global size: $'", device.max_global_size);
		log_msg("SIMD width: $", device.simd_width);
		log_msg("argument buffer tier: $", [dev argumentBuffersSupport] + 1);
		log_msg("indirect commands: $", device.indirect_command_support ? "yes" : "no");
		
		device.max_mip_levels = image_mip_level_count_from_max_dim(std::max(std::max(device.max_image_2d_dim.max_element(),
																					 device.max_image_3d_dim.max_element()),
																			device.max_image_1d_dim));
		
		device.barycentric_coord_support = [dev supportsShaderBarycentricCoordinates];
		
		if (@available(macOS 15.0, iOS 18.0, visionOS 2.0, *)) {
			device.residency_set_support = true;
		}
		
		if ([dev respondsToSelector:@selector(architecture)]) {
			if (const auto arch_str = [[[dev architecture] name] UTF8String]; arch_str) {
				log_msg("architecture: $", arch_str);
			}
		}
		
		// done
		supported = true;
		platform_vendor = VENDOR::APPLE;
		log_debug("GPU (global: $' MB, local: $' bytes, unified: $): $, Metal $, family type $ tier $",
				  (uint32_t)(device.global_mem_size / 1024ull / 1024ull),
				  device.local_mem_size,
				  (device.unified_memory ? "yes" : "no"),
				  device.name,
				  metal_version_to_string(device.metal_software_version),
				  metal_device::family_type_to_string(device.family_type),
				  device.family_tier);
	}
	
	// check if there is any supported / whitelisted device
	if (devices.empty()) {
		log_error("no valid Metal device found!");
		return;
	}
	
	// figure out the fastest device
#if defined(FLOOR_IOS) || defined(FLOOR_VISIONOS)
	// only one device on iOS/visionOS
	fastest_gpu_device = devices[0].get();
	fastest_device = fastest_gpu_device;
#else
	// on macOS, this is tricky, because we don't get the compute units count and clock speed
	// -> assume devices are returned in order of their speed
	// -> assume that amd cards are faster than intel cards
	fastest_gpu_device = devices[0].get(); // start off with the first device
	if (fastest_gpu_device->vendor != VENDOR::AMD) { // if this is false, we already have an amd device
		for (size_t i = 1; i < devices.size(); ++i) {
			if (devices[i]->vendor == VENDOR::AMD) {
				// found an amd device, consider it the fastest
				fastest_gpu_device = devices[i].get();
				break;
			}
		}
	}
	fastest_device = fastest_gpu_device;
#endif
	log_debug("fastest GPU device: $", fastest_gpu_device->name);
	
	// create an internal queue, null buffer, (if enabled) soft-printf buffer cache for each device, and the internal heap(s)
	@autoreleasepool {
		for (auto& dev : devices) {
			auto mtl_dev = (metal_device*)dev.get();
			
			// allocate internal (experimental) heap
			if (has_flag<DEVICE_CONTEXT_FLAGS::__EXP_INTERNAL_HEAP>(context_flags)) {
				// determine heap sizes
				// NOTE: global_mem_size may be >= max_mem_alloc, but also ensure we don't allocate too much memory (10% safety margin)
				const auto safety_global_mem_size = uint64_t(double(dev->global_mem_size) * 0.9);
				uint64_t heap_size_private = std::min(uint64_t(double(safety_global_mem_size) * double(floor::get_heap_private_size())),
													  dev->max_mem_alloc);
				uint64_t heap_size_shared = std::min(uint64_t(double(safety_global_mem_size) * double(floor::get_heap_shared_size())),
													 dev->max_mem_alloc);
				assert(heap_size_private <= dev->max_mem_alloc && heap_size_shared <= dev->max_mem_alloc);
				if (!dev->unified_memory) {
					// we don't have any shared storage mode memory on GPUs w/o unified memory
					heap_size_shared = 0u;
				} else {
					if (floor::get_metal_shared_only_with_unified_memory()) {
						heap_size_private = 0u;
					}
				}
				if (heap_size_private + heap_size_shared > safety_global_mem_size) {
					// ensure the combined heap sizes aren't bigger than the max possible memory size
					const auto over_size = (heap_size_private + heap_size_shared) - safety_global_mem_size;
					const auto half_over_size = over_size / 2u;
					heap_size_private = (heap_size_private >= half_over_size ? heap_size_private - half_over_size : 0u);
					heap_size_shared = (heap_size_shared >= half_over_size ? heap_size_shared - half_over_size : 0u);
					log_warn("wanted heap sizes were too large ($' over $'), reducing them to private $', shared $'",
							 over_size, safety_global_mem_size, heap_size_private, heap_size_shared);
				}
				
				auto heap_desc = [[MTLHeapDescriptor alloc] init];
				heap_desc.cpuCacheMode = MTLCPUCacheModeDefaultCache;
				heap_desc.hazardTrackingMode = MTLHazardTrackingModeUntracked;
				heap_desc.type = MTLHeapTypeAutomatic;
				if (heap_size_private > 0u) {
					heap_desc.size = heap_size_private;
					heap_desc.storageMode = MTLStorageModePrivate;
					mtl_dev->heap_private = [mtl_dev->device newHeapWithDescriptor:heap_desc];
					if (mtl_dev->heap_private) {
						log_msg("allocated private storage heap of size $'", [mtl_dev->heap_private size]);
					} else {
						log_error("failed to allocate private storage heap of size $'", heap_size_private);
					}
				}
				if (heap_size_shared > 0u) {
					heap_desc.size = heap_size_shared;
					heap_desc.storageMode = MTLStorageModeShared;
					mtl_dev->heap_shared = [mtl_dev->device newHeapWithDescriptor:heap_desc];
					if (mtl_dev->heap_shared) {
						log_msg("allocated shared storage heap of size $'", [mtl_dev->heap_shared size]);
					} else {
						log_error("failed to allocate share storage heap of size $'", heap_size_shared);
					}
				}
				
				// allocate residency set for all heaps if supported
				if (mtl_dev->residency_set_support && (mtl_dev->heap_shared || mtl_dev->heap_private)) {
					MTLResidencySetDescriptor* res_set_desc = [[MTLResidencySetDescriptor alloc] init];
					res_set_desc.label = @"heap_residency_set";
					res_set_desc.initialCapacity = 2;
					NSError* res_set_error = nil;
					mtl_dev->heap_residency_set = [mtl_dev->device newResidencySetWithDescriptor:res_set_desc
																						   error:&res_set_error];
					if (res_set_error) {
						log_error("failed to create heap residency set for device $: $", mtl_dev->name,
								  [[res_set_error localizedDescription] UTF8String]);
						mtl_dev->heap_residency_set = nil;
					} else {
						if (mtl_dev->heap_shared) {
							[mtl_dev->heap_residency_set addAllocation:mtl_dev->heap_shared];
						}
						if (mtl_dev->heap_private) {
							[mtl_dev->heap_residency_set addAllocation:mtl_dev->heap_private];
						}
						[mtl_dev->heap_residency_set commit];
						[mtl_dev->heap_residency_set requestResidency];
						log_msg("using a global residency set for all heaps");
					}
				}
			}
			
			// queue
			auto dev_queue = create_queue(*dev);
			dev_queue->set_debug_label("default_queue");
			internal_queues.insert_or_assign(dev.get(), dev_queue);
			mtl_dev->internal_queue = dev_queue.get();
			
			// create null buffer
			auto null_buffer = create_buffer(*dev_queue, aligned_ptr<int>::page_size,
											 MEMORY_FLAG::READ | MEMORY_FLAG::HOST_READ_WRITE |
											 MEMORY_FLAG::NO_RESOURCE_TRACKING | MEMORY_FLAG::__EXP_HEAP_ALLOC);
			null_buffer->zero(*dev_queue);
			internal_null_buffers.insert_or_assign(dev.get(), null_buffer);
			
			// create soft-printf buffer cache
			if (floor::get_metal_soft_printf()) {
				std::array<std::shared_ptr<device_buffer>, soft_printf_buffer_count> dev_soft_printf_buffers;
				for (uint32_t buf_idx = 0; buf_idx < soft_printf_buffer_count; ++buf_idx) {
					dev_soft_printf_buffers[buf_idx] = allocate_printf_buffer(*dev_queue);
				}
				soft_printf_buffers.insert_or_assign(dev.get(), std::make_unique<soft_printf_buffer_rsrc_container_type>(std::move(dev_soft_printf_buffers)));
			}
		}
	}
	
	// init renderer
	if (enable_renderer) {
		render_device = (const metal_device*)fastest_gpu_device;
		auto mtl_dev = render_device->device;
		view = darwin_helper::create_metal_view(floor::get_window(), mtl_dev, hdr_metadata);
		if (view == nullptr) {
			log_error("failed to create Metal view!");
			supported = false;
		}
		
#if !defined(FLOOR_NO_OPENVR) || !defined(FLOOR_NO_OPENXR)
		if (vr_ctx) {
			if (!init_vr_renderer()) {
				log_error("failed to init VR renderer");
				vr_ctx = nullptr;
			}
		}
#endif
	}
}

std::shared_ptr<device_queue> metal_context::create_queue(const device& dev) const {
	@autoreleasepool {
		const auto& mtl_dev = (const metal_device&)dev;
		id <MTLCommandQueue> queue = [mtl_dev.device newCommandQueue];
		if (queue == nullptr) {
			log_error("failed to create command queue");
			return {};
		}
		
		// always add the heaps residency set to all queues so that they are always resident in all queues
		if (mtl_dev.heap_residency_set) {
			[queue addResidencySet:mtl_dev.heap_residency_set];
		}
		
		auto ret = std::make_shared<metal_queue>(dev, queue);
		queues.push_back(ret);
		return ret;
	}
}

const device_queue* metal_context::get_device_default_queue(const device& dev) const {
	if (const auto iter = internal_queues.find(&dev); iter != internal_queues.end()) {
		return iter->second.get();
	}
	log_error("no default queue exists for this device: $!", dev.name);
	return nullptr;
}

std::unique_ptr<device_fence> metal_context::create_fence(const device_queue& cqueue) const {
	@autoreleasepool {
		id <MTLFence> mtl_fence = [((const metal_device&)cqueue.get_device()).device newFence];
		return std::make_unique<metal_fence>(mtl_fence);
	}
}

const metal_buffer* metal_context::get_null_buffer(const device& dev) const {
	if (const auto iter = internal_null_buffers.find(&dev); iter != internal_null_buffers.end()) {
		return (const metal_buffer*)iter->second.get();
	}
	log_error("no null-buffer exists for this device: $!", dev.name);
	return nullptr;
}

std::pair<device_buffer*, uint32_t> metal_context::acquire_soft_printf_buffer(const device& dev) const {
	if (const auto iter = soft_printf_buffers.find(&dev); iter != soft_printf_buffers.end()) {
		return iter->second->acquire();
	}
	log_error("no soft-printf buffer cache exists for this device: $!", dev.name);
	return {};
}

void metal_context::release_soft_printf_buffer(const device& dev, const std::pair<device_buffer*, uint32_t>& buf) const {
	if (const auto iter = soft_printf_buffers.find(&dev); iter != soft_printf_buffers.end()) {
		iter->second->release(buf);
		return;
	}
	log_error("no soft-printf buffer cache exists for this device: $!", dev.name);
}

std::shared_ptr<device_buffer> metal_context::create_buffer(const device_queue& cqueue,
														const size_t& size, const MEMORY_FLAG flags) const {
	return add_resource(std::make_shared<metal_buffer>(cqueue, size,
												  flags | (has_flag<DEVICE_CONTEXT_FLAGS::NO_RESOURCE_TRACKING>(context_flags) ?
														   MEMORY_FLAG::NO_RESOURCE_TRACKING : MEMORY_FLAG::NONE)));
}

std::shared_ptr<device_buffer> metal_context::create_buffer(const device_queue& cqueue,
														std::span<uint8_t> data,
														const MEMORY_FLAG flags) const {
	return add_resource(std::make_shared<metal_buffer>(cqueue, data.size_bytes(), data,
												  flags | (has_flag<DEVICE_CONTEXT_FLAGS::NO_RESOURCE_TRACKING>(context_flags) ?
														   MEMORY_FLAG::NO_RESOURCE_TRACKING : MEMORY_FLAG::NONE)));
}

std::shared_ptr<device_image> metal_context::create_image(const device_queue& cqueue,
													  const uint4 image_dim,
													  const IMAGE_TYPE image_type,
													  std::span<uint8_t> data,
													  const MEMORY_FLAG flags,
													  const uint32_t mip_level_limit) const {
	return add_resource(std::make_shared<metal_image>(cqueue, image_dim, image_type, data,
												 flags | (has_flag<DEVICE_CONTEXT_FLAGS::NO_RESOURCE_TRACKING>(context_flags) ?
														  MEMORY_FLAG::NO_RESOURCE_TRACKING : MEMORY_FLAG::NONE),
												 mip_level_limit));
}

static std::shared_ptr<metal_program> add_metal_program(metal_program::program_map_type&& prog_map,
												   std::vector<std::shared_ptr<metal_program>>* programs,
												   atomic_spin_lock& programs_lock) REQUIRES(!programs_lock) {
	// create the program object, which in turn will create function objects for all functions in the program,
	// for all devices contained in the program map
	auto prog = std::make_shared<metal_program>(std::move(prog_map));
	{
		GUARD(programs_lock);
		programs->push_back(prog);
	}
	return prog;
}

std::shared_ptr<device_program> metal_context::create_program_from_archive_binaries(universal_binary::archive_binaries& bins) {
	// move the archive memory to a shared_ptr
	// NOTE: we need to do this because dispatch_data_t/newLibraryWithData will access the data after leaving this function,
	//       when the archive will have been destructed already if kept in the local unique_ptr
	std::shared_ptr<universal_binary::archive> ar(bins.ar.release());
	
	// create the program
	metal_program::program_map_type prog_map;
	@autoreleasepool {
		for (size_t i = 0, dev_count = devices.size(); i < dev_count; ++i) {
			const auto mtl_dev = (const metal_device*)devices[i].get();
			const auto& dev_best_bin = bins.dev_binaries[i];
			const auto func_info = universal_binary::translate_function_info(dev_best_bin.first->function_info);
			
			metal_program::metal_program_entry entry;
			entry.archive = ar; // ensure we keep the archive memory
			entry.functions = func_info;
			
			NSError* err { nil };
			dispatch_data_t lib_data = dispatch_data_create(dev_best_bin.first->data.data(), dev_best_bin.first->data.size(),
															dispatch_get_main_queue(), ^{} /* must be non-default */);
			entry.program = [mtl_dev->device newLibraryWithData:lib_data error:&err];
			if (!entry.program) {
				log_error("failed to create Metal program/library for device $: $",
						  mtl_dev->name, (err != nil ? [[err localizedDescription] UTF8String] : "unknown error"));
				return {};
			}
			entry.valid = true;
			
			prog_map.insert_or_assign(mtl_dev, entry);
		}
	}
	
	return add_metal_program(std::move(prog_map), &programs, programs_lock);
}

std::shared_ptr<device_program> metal_context::add_universal_binary(const std::string& file_name) {
	auto bins = universal_binary::load_dev_binaries_from_archive(file_name, *this);
	if (bins.ar == nullptr || bins.dev_binaries.empty()) {
		log_error("failed to load universal binary: $", file_name);
		return {};
	}
	return create_program_from_archive_binaries(bins);
}

std::shared_ptr<device_program> metal_context::add_universal_binary(const std::span<const uint8_t> data) {
	auto bins = universal_binary::load_dev_binaries_from_archive(data, *this);
	if (bins.ar == nullptr || bins.dev_binaries.empty()) {
		log_error("failed to load universal binary (in-memory data)");
		return {};
	}
	return create_program_from_archive_binaries(bins);
}

static metal_program::metal_program_entry create_metal_program(const metal_device& dev floor_unused_on_ios_and_visionos,
															   toolchain::program_data program) {
	metal_program::metal_program_entry ret;
	ret.functions = program.function_info;
	
	if(!program.valid) {
		return ret;
	}
	
#if !defined(FLOOR_IOS) && !defined(FLOOR_VISIONOS) // can only do this on macOS
	@autoreleasepool {
		// create the program/library object and build it (note: also need to create an dispatcht_data_t object ...)
		NSError* err { nil };
		const auto lib_file_name = [NSString stringWithUTF8String:program.data_or_filename.c_str()];
		if (!lib_file_name) {
			log_error("invalid library file name: $", program.data_or_filename);
			return ret;
		}
		auto lib_url = [NSURL fileURLWithPath:floor_force_nonnull(lib_file_name)];
		ret.program = [dev.device newLibraryWithURL:lib_url
												 error:&err];
		if(!floor::get_toolchain_keep_temp()) {
			// cleanup
			if(!floor::get_toolchain_debug()) {
				std::error_code ec {};
				(void)std::filesystem::remove(program.data_or_filename, ec);
			}
		}
		if(!ret.program) {
			log_error("failed to create Metal program/library for device $: $",
					  dev.name, (err != nil ? [[err localizedDescription] UTF8String] : "unknown error"));
			return ret;
		}
		
		// TODO: print out the build log
		ret.valid = true;
		return ret;
	}
#else
	log_error("this is not supported on iOS!");
	return ret;
#endif
}

std::shared_ptr<device_program> metal_context::add_program_file(const std::string& file_name,
															const std::string additional_options) {
	return add_program_file(file_name, compile_options { .cli = additional_options });
}

std::shared_ptr<device_program> metal_context::add_program_file(const std::string& file_name,
															compile_options options) {
	// compile the source file for all devices in the context
	metal_program::program_map_type prog_map;
	options.target = toolchain::TARGET::AIR;
	for(const auto& dev : devices) {
		prog_map.insert_or_assign((const metal_device*)dev.get(),
								  create_metal_program((const metal_device&)*dev,
													   toolchain::compile_program_file(*dev, file_name, options)));
	}
	return add_metal_program(std::move(prog_map), &programs, programs_lock);
}

std::shared_ptr<device_program> metal_context::add_program_source(const std::string& source_code,
															  const std::string additional_options) {
	return add_program_source(source_code, compile_options { .cli = additional_options });
}

std::shared_ptr<device_program> metal_context::add_program_source(const std::string& source_code,
															  compile_options options) {
	// compile the source code for all devices in the context
	metal_program::program_map_type prog_map;
	options.target = toolchain::TARGET::AIR;
	for(const auto& dev : devices) {
		prog_map.insert_or_assign((const metal_device*)dev.get(),
								  create_metal_program((const metal_device&)*dev,
													   toolchain::compile_program(*dev, source_code, options)));
	}
	return add_metal_program(std::move(prog_map), &programs, programs_lock);
}

std::shared_ptr<device_program> metal_context::add_precompiled_program_file(const std::string& file_name,
																		const std::vector<toolchain::function_info>& functions) {
	log_debug("loading mtllib: $", file_name);
	
	// assume pre-compiled program is the same for all devices
	metal_program::program_map_type prog_map;
	@autoreleasepool {
		for(const auto& dev : devices) {
			metal_program::metal_program_entry entry;
			entry.functions = functions;
			
			NSError* err { nil };
			const auto lib_file_name = [NSString stringWithUTF8String:file_name.c_str()];
			if (!lib_file_name) {
				log_error("invalid library file name: $", file_name);
				continue;
			}
			auto lib_url = [NSURL fileURLWithPath:floor_force_nonnull(lib_file_name)];
			entry.program = [((const metal_device&)*dev).device newLibraryWithURL:lib_url
																			error:&err];
			if(!entry.program) {
				log_error("failed to create Metal program/library for device $: $",
						  dev->name, (err != nil ? [[err localizedDescription] UTF8String] : "unknown error"));
				continue;
			}
			entry.valid = true;
			
			prog_map.insert_or_assign((const metal_device*)dev.get(), entry);
		}
	}
	return add_metal_program(std::move(prog_map), &programs, programs_lock);
}

std::shared_ptr<device_program::program_entry> metal_context::create_program_entry(const device& dev,
																			   toolchain::program_data program,
																			   const toolchain::TARGET) {
	return std::make_shared<metal_program::metal_program_entry>(create_metal_program((const metal_device&)dev, program));
}

std::shared_ptr<device_program> metal_context::create_metal_test_program(std::shared_ptr<device_program::program_entry> entry) {
	const auto metal_entry = (const metal_program::metal_program_entry*)entry.get();
	
	// find the device the specified program has been compiled for
	const metal_device* metal_dev = nullptr;
	for(auto& dev : devices) {
		if(((const metal_device&)*dev).device == [metal_entry->program device]) {
			metal_dev = (const metal_device*)&dev;
			break;
		}
	}
	if(metal_dev == nullptr) {
		log_error("program device is not part of this context");
		return nullptr;
	}
	
	// create/return the program
	metal_program::program_map_type prog_map;
	prog_map.emplace(metal_dev, *metal_entry);
	return std::make_shared<metal_program>(std::move(prog_map));
}

std::unique_ptr<indirect_command_pipeline> metal_context::create_indirect_command_pipeline(const indirect_command_description& desc) const {
	auto pipeline = std::make_unique<metal_indirect_command_pipeline>(desc, devices);
	if (!pipeline || !pipeline->is_valid()) {
		return {};
	}
	return pipeline;
}

MTLPixelFormat metal_context::get_metal_renderer_pixel_format() const {
	if (view == nullptr) {
		return MTLPixelFormatInvalid;
	}
	return darwin_helper::get_metal_pixel_format(view);
}

id <CAMetalDrawable> metal_context::get_metal_next_drawable(id <MTLCommandBuffer> cmd_buffer) const {
	@autoreleasepool {
		if (view == nullptr) {
			return nil;
		}
		return darwin_helper::get_metal_next_drawable(view, cmd_buffer);
	}
}

bool metal_context::init_vr_renderer() {
#if !defined(FLOOR_NO_OPENVR) || !defined(FLOOR_NO_OPENXR)
	if (!vr_ctx) {
		return false;
	}
	
	const auto& dev_queue = *get_device_default_queue(*render_device);
	
	const uint4 vr_screen_dim { floor::get_vr_physical_screen_size(), 2, 0 };
	const IMAGE_TYPE vr_image_type = (IMAGE_TYPE::RGBA8UI_NORM |
											  IMAGE_TYPE::IMAGE_2D_ARRAY |
											  IMAGE_TYPE::FLAG_RENDER_TARGET |
											  IMAGE_TYPE::READ_WRITE);
	for (uint32_t i = 0; i < vr_image_count; ++i) {
		vr_images[i].image = create_image(dev_queue, vr_screen_dim, vr_image_type,
										  MEMORY_FLAG::READ_WRITE | MEMORY_FLAG::HOST_READ_WRITE);
		vr_images[i].image->set_debug_label("VR screen image #" + std::to_string(i));
	}
	
	return true;
#else
	return false;
#endif
}

std::shared_ptr<device_image> metal_context::get_metal_next_vr_drawable() const NO_THREAD_SAFETY_ANALYSIS {
	if (!vr_ctx) {
		return {};
	}
	
	// manual index advance
	vr_image_index = (vr_image_index + 1) % vr_image_count;
	
	// lock this image until it has been submitted for present (also blocks until the wanted image is available)
	vr_images[vr_image_index].image_lock.lock();
	return vr_images[vr_image_index].image;
}

#if !defined(FLOOR_NO_OPENVR) || !defined(FLOOR_NO_OPENXR)
void metal_context::present_metal_vr_drawable(const device_queue& cqueue floor_unused_on_ios,
											  const device_image& img floor_unused_on_ios) const NO_THREAD_SAFETY_ANALYSIS {
	if (!vr_ctx) {
		return;
	}
	vr_ctx->present(cqueue, img);
	
	// unlock image again
	for (auto& vr_img : vr_images) {
		if (vr_img.image.get() == &img) {
			vr_img.image_lock.unlock();
			break;
		}
	}
}
#else
void metal_context::present_metal_vr_drawable(const device_queue&, const device_image&) const {
	// nop
}
#endif

std::unique_ptr<graphics_pipeline> metal_context::create_graphics_pipeline(const render_pipeline_description& pipeline_desc,
																	  const bool with_multi_view_support) const {
	auto pipeline = std::make_unique<metal_pipeline>(pipeline_desc, devices, with_multi_view_support && (vr_ctx != nullptr));
	if (!pipeline || !pipeline->is_valid()) {
		return {};
	}
	return pipeline;
}

std::unique_ptr<graphics_pass> metal_context::create_graphics_pass(const render_pass_description& pass_desc,
															  const bool with_multi_view_support) const {
	auto pass = std::make_unique<metal_pass>(pass_desc, with_multi_view_support && (vr_ctx != nullptr));
	if (!pass || !pass->is_valid()) {
		return {};
	}
	return pass;
}

std::unique_ptr<graphics_renderer> metal_context::create_graphics_renderer(const device_queue& cqueue,
																	  const graphics_pass& pass,
																	  const graphics_pipeline& pipeline,
																	  const bool create_multi_view_renderer) const {
	if (create_multi_view_renderer && !is_vr_supported()) {
		log_error("can't create a multi-view/VR graphics renderer when VR is not supported");
		return {};
	}
	
	auto renderer = std::make_unique<metal_renderer>(cqueue, pass, pipeline, create_multi_view_renderer);
	if (!renderer || !renderer->is_valid()) {
		return {};
	}
	return renderer;
}

IMAGE_TYPE metal_context::get_renderer_image_type() const {
	switch (get_metal_renderer_pixel_format()) {
		case MTLPixelFormatBGRA8Unorm:
			return IMAGE_TYPE::BGRA8UI_NORM;
		case MTLPixelFormatBGRA8Unorm_sRGB:
			return IMAGE_TYPE::BGRA8UI_NORM | IMAGE_TYPE::FLAG_SRGB;
		case MTLPixelFormatBGR10A2Unorm:
			return IMAGE_TYPE::A2BGR10UI_NORM;
		case MTLPixelFormatRGBA16Float:
			return IMAGE_TYPE::RGBA16F;
#if defined(FLOOR_IOS) || defined(FLOOR_VISIONOS)
		case MTLPixelFormatBGR10_XR:
			return IMAGE_TYPE::BGR10UI_NORM;
		case MTLPixelFormatBGR10_XR_sRGB:
			return IMAGE_TYPE::BGR10UI_NORM | IMAGE_TYPE::FLAG_SRGB;
		case MTLPixelFormatBGRA10_XR:
			return IMAGE_TYPE::BGRA10UI_NORM;
		case MTLPixelFormatBGRA10_XR_sRGB:
			return IMAGE_TYPE::BGRA10UI_NORM | IMAGE_TYPE::FLAG_SRGB;
#endif
		default: break;
	}
	return IMAGE_TYPE::NONE;
}

uint4 metal_context::get_renderer_image_dim() const {
	if (!vr_ctx) {
		if (view) {
			return { darwin_helper::get_metal_view_dim(view), 0, 0 };
		}
		return {};
	} else {
		return vr_images[0].image->get_image_dim();
	}
}

bool metal_context::is_vr_supported() const {
	return (vr_ctx ? true : false);
}

vr_context* metal_context::get_renderer_vr_context() const {
	return vr_ctx;
}

void metal_context::set_hdr_metadata(const hdr_metadata_t& hdr_metadata_) {
	device_context::set_hdr_metadata(hdr_metadata_);
	if (view != nullptr) {
		darwin_helper::set_metal_view_hdr_metadata(view, hdr_metadata);
	}
}

float metal_context::get_hdr_range_max() const {
	if (view != nullptr) {
		const auto edr_max = darwin_helper::get_metal_view_edr_max(view);
		if (edr_max <= 1.0f) {
			// SDR
			return 1.0f;
		}
		// normalize max nits to extended linear range (interpreting 1.0 as 80 nits):
		// (max nits * (100 nits / 80 nits)) / 100 nits
		return darwin_helper::get_metal_view_hdr_max_nits(view) * (1.0f / 80.0f);
	}
	return device_context::get_hdr_range_max();
}

float metal_context::get_hdr_display_max_nits() const {
	if (view != nullptr) {
		return darwin_helper::get_metal_view_hdr_max_nits(view);
	}
	return device_context::get_hdr_display_max_nits();
}

bool metal_context::start_metal_capture(const device& dev, const std::string& file_name) const {
	@autoreleasepool {
		MTLCaptureManager* capture_manager = [MTLCaptureManager sharedCaptureManager];
		if (![capture_manager supportsDestination:MTLCaptureDestinationGPUTraceDocument]) {
			log_error("can't capture GPU trace to file");
			return false;
		}
		
		MTLCaptureDescriptor* capture_desc = [[MTLCaptureDescriptor alloc] init];
		capture_manager.defaultCaptureScope =
		[capture_manager newCaptureScopeWithDevice:((const metal_device&)dev).device];
		capture_desc.captureObject = capture_manager.defaultCaptureScope;
		auto file_name_nsstr = [NSString stringWithUTF8String:file_name.c_str()];
		if (!file_name_nsstr) {
			log_error("invalid capture file name: $", file_name);
			return false;
		}
		capture_desc.outputURL = [NSURL fileURLWithPath:floor_force_nonnull(file_name_nsstr)];
		capture_desc.destination = MTLCaptureDestinationGPUTraceDocument;
		
		NSError* err { nil };
		if (![capture_manager startCaptureWithDescriptor:capture_desc error:&err]) {
			log_error("failed to start GPU trace capture: $",
					  (err != nil ? [[err localizedDescription] UTF8String] : "unknown error"));
			return false;
		}
		
		[capture_manager.defaultCaptureScope beginScope];
		
		return true;
	}
}

bool metal_context::stop_metal_capture() const {
	@autoreleasepool {
		MTLCaptureManager* capture_manager = [MTLCaptureManager sharedCaptureManager];
		[capture_manager.defaultCaptureScope endScope];
		[capture_manager stopCapture];
		capture_manager.defaultCaptureScope = nil;
		return true;
	}
}

device_context::memory_usage_t metal_context::get_memory_usage(const device& dev) const {
	const auto& mtl_dev = (const metal_device&)dev;
	memory_usage_t ret {
		.global_mem_used = [mtl_dev.device currentAllocatedSize],
		.global_mem_total = dev.global_mem_size,
		.heap_used = 0u,
		.heap_total = 0u,
	};
	
	if (mtl_dev.heap_shared) {
		ret.heap_used += [mtl_dev.heap_shared usedSize];
		ret.heap_total += [mtl_dev.heap_shared currentAllocatedSize];
	}
	if (mtl_dev.heap_private) {
		ret.heap_used += [mtl_dev.heap_private usedSize];
		ret.heap_total += [mtl_dev.heap_private currentAllocatedSize];
	}
	
	return ret;
}

} // namespace fl

#endif
