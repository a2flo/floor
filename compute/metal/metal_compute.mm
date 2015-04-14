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
#if defined(FLOOR_IOS)
#include <floor/ios/ios_helper.hpp>
#else
#include <floor/osx/osx_helper.hpp>
#endif
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

void metal_compute::init(const bool use_platform_devices floor_unused,
						 const uint32_t platform_index_ floor_unused,
						 const bool gl_sharing floor_unused,
						 const unordered_set<string> device_restriction floor_unused) {
	// create the default device, exit if it fails
	auto mtl_device = MTLCreateSystemDefaultDevice();
	if(mtl_device == nil) return;
	
	devices.emplace_back(make_shared<metal_device>());
	auto device_sptr = devices.back();
	auto& device = *(metal_device*)device_sptr.get();
	device.device = mtl_device;
	fastest_gpu_device = device_sptr;
	fastest_device = device_sptr;
	
	// most of the device properties can't be querried, but are statically known (-> doc)
	device.name = [[mtl_device name] UTF8String];
	device.vendor_name = "Apple";
	device.driver_version_str = "1.0.0"; // for now, should update with iOS >8 (iOS8 versions: metal 1.0.0, air 1.6.0, language 1.0.0)
	device.type = compute_device::TYPE::GPU0;
	device.vendor = compute_device::VENDOR::APPLE;
	device.clock = 450; // actually unknown, and won't matter for now
	device.global_mem_size = (uint64_t)ios_helper::get_memory_size();
	device.constant_mem_size = 65536; // no idea if this is correct, but it's the min required size for opencl 1.2
	device.max_mem_alloc = device.global_mem_size / 4; // usually the case
	
	// hard to make this forward compatible, there is no direct "get family" call
	// -> just try the first 32 types, good enough for now
	for(uint32_t i = 0; i < 32; ++i) {
		if([mtl_device supportsFeatureSet:(MTLFeatureSet)i]) {
			device.family = i + 1;
			break;
		}
	}
	device.version_str = to_string(device.family);
	
	// init statically known device information (pulled from AGXMetal/AGXG*Device and apples doc)
	switch(device.family) {
		case 0:
			log_error("unsupported hardware - family is 0!");
			return;
		case 1:
			device.units = 4; // G6430
			device.local_mem_size = 16384;
			device.max_work_group_size = 512;
			device.mem_clock = 1333; // ram clock
			break;
		default:
			log_warn("unknown device family (%u), defaulting to family 2 (A8)", device.family);
		
		floor_fallthrough;
		case 2:
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
	device.max_work_group_item_sizes = { 512, 512, 512 }; // note: not entirely sure if this is correct,
	device.max_work_item_sizes = { sizeof(uint64_t) };    //       could also be (2^57, 2^57, 512)
	device.image_support = true;
	device.double_support = false; // double config is 0
	device.unified_memory = true;
	device.max_image_1d_dim = { 4096 };
	device.max_image_2d_dim = { 4096, 4096 };
	device.max_image_3d_dim = { 4096, 4096, 2048 };
	device.bitness = 64;
	
	// done
	supported = true;
	platform_vendor = compute_base::PLATFORM_VENDOR::APPLE;
	log_debug("GPU (Units: %u, Clock: %u MHz, Memory: %u MB): %s, family %u",
			  device.units,
			  device.clock,
			  (unsigned int)(device.global_mem_size / 1024ull / 1024ull),
			  device.name,
			  device.family);
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

void metal_compute::finish() {
	// TODO: !
}

void metal_compute::flush() {
	// TODO: !
}

void metal_compute::activate_context() {
	// TODO: !
}

void metal_compute::deactivate_context() {
	// TODO: !
}

shared_ptr<compute_program> metal_compute::add_program_file(const string& file_name,
															const string additional_options) {
	string code;
	if(!file_io::file_to_string(file_name, code)) {
		return {};
	}
	return add_program_source(code, additional_options);
}

shared_ptr<compute_program> metal_compute::add_program_source(const string& source_code,
															  const string additional_options) {
	// compile the source code to apple IR / air (this produces/returns an llvm 3.5 bitcode binary file)
	const auto program_data = llvm_compute::compile_program(fastest_device, source_code, additional_options, llvm_compute::TARGET::AIR);
	
	// TODO: as / ar / link / metallib
	
	// create the program/library object and build it (note: also need to create an dispatcht_data_t object ...)
	NSError* err { nil };
	dispatch_data_t ddt = dispatch_data_create(program_data.first.c_str(), program_data.first.size(),
											   dispatch_get_main_queue(), ^(void) {});
	id <MTLLibrary> program = [((metal_device*)fastest_device.get())->device newLibraryWithData:ddt error:&err];
	if(!program) {
		log_error("failed to create metal program/library: %s", (err != nil ? [[err localizedDescription] UTF8String] : "unknown"));
		return {};
	}
	
	// TODO: print out the build log
	
	// create the program object, which in turn will create kernel objects for all kernel functions in the program
	auto ret_program = make_shared<metal_program>((metal_device*)fastest_device.get(), program, program_data.second);
	programs.push_back(ret_program);
	return ret_program;
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
	
	auto ret_program = make_shared<metal_program>((metal_device*)fastest_device.get(), program, kernel_infos);
	programs.push_back(ret_program);
	return ret_program;
}

#endif
