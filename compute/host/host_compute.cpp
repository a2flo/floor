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

#include <floor/core/essentials.hpp>

#if !defined(FLOOR_NO_HOST_COMPUTE)
#include <floor/compute/host/host_compute.hpp>
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
#include <floor/floor/floor_version.hpp>

#if (defined(__APPLE__) || defined(__FreeBSD__) || defined(__OpenBSD__))
#include <sys/types.h>
#include <sys/sysctl.h>
#endif

#if !defined(FLOOR_IOS)
#include <cpuid.h>
#endif

void host_compute::init(const bool use_platform_devices floor_unused,
						const uint32_t platform_index_ floor_unused,
						const bool gl_sharing floor_unused,
						const unordered_set<string> whitelist floor_unused) {
	platform_vendor = PLATFORM_VENDOR::HOST;
	
	//
	devices.emplace_back(make_shared<host_device>());
	auto device_sptr = devices.back();
	auto& device = *(host_device*)device_sptr.get();
	
	// gather "device"/cpu information, this is very platform dependent
	string cpu_name = "";
	uint64_t cpu_clock = 0;
	
	// we can get the actual cpu name quite easily on x86 through cpuid instructions
#if !defined(FLOOR_IOS)
	// cpuid magic
	uint32_t eax, ebx, ecx, edx;
	__cpuid(0x80000000u, eax, ebx, ecx, edx);
	if(eax >= 0x80000004u) {
		char cpuid_name[49];
		memset(cpuid_name, 0, size(cpuid_name));
		size_t i = 0;
		for(uint32_t id = 0x80000002u; id <= 0x80000004u; ++id) {
			uint32_t vals[4];
			__cpuid(id, vals[0], vals[1], vals[2], vals[3]);
			for(size_t vidx = 0; vidx < size(vals); ++vidx) {
				for(size_t bidx = 0; bidx < 4; ++bidx) {
					cpuid_name[i++] = char(vals[vidx] & 0xFFu);
					vals[vidx] >>= 8u;
				}
			}
		}
		cpu_name = core::trim(cpuid_name);
	}
#else // this can't be done on ARM or iOS however (TODO: handle other arm cpus)
	// -> hardcode the name for now
#if defined(PLATFORM_X32)
	cpu_name = "Apple ARMv7";
#else
	cpu_name = "Apple ARMv8";
#endif
#endif
	
	// now onto getting the cpu clock speed:
#if (defined(__APPLE__) && !defined(FLOOR_IOS)) || defined(__FreeBSD__) // can simply use sysctl on os x (and freebsd)
	size_t cpu_freq_size = sizeof(uint64_t);
	sysctlbyname(
#if defined(__APPLE__)
				 "hw.cpufrequency",
#elif defined(__FreeBSD__)
				 "hw.clockrate",
#endif
				 &cpu_clock, &cpu_freq_size, nullptr, 0);
#if defined(__APPLE__)
	cpu_clock /= 1000000ull; // to mhz
#endif
	
#elif (defined(__APPLE__) && defined(FLOOR_IOS)) // can't query this on ios, hardcore it with somewhat accurate values
#if defined(PLATFORM_X32)
	cpu_clock = 800; // at least
#else
	cpu_clock = 1300; // at least
#endif
	
#elif defined(__OpenBSD__) // also sysctl, but different
	uint32_t cpu_clock_32 = 0;
	size_t cpu_clock_size = sizeof(hw_thread_count);
	static const int sysctl_clock_cmd[2] { CTL_HW, HW_CPUSPEED };
	sysctl(sysctl_clock_cmd, 2, &cpu_clock_32, &cpu_clock_size, nullptr, 0);
	cpu_clock = cpu_clock_32;
#elif defined(__linux__)
	// linux has no proper sysctl, query /proc/cpuinfo instead and do some parsing ...
	const auto cpuinfo = core::tokenize(file_io::file_to_string_poll("/proc/cpuinfo"), '\n');
	for(const auto& elem : cpuinfo) {
		// this should handle getting the cpu name for arm cpus (at least on linux)
		if(cpu_name == "" &&
		   elem.find("model name") != string::npos) {
			const auto colon_pos = elem.find(": ");
			if(colon_pos != string::npos) {
				cpu_name = elem.substr(colon_pos + 2);
			}
		}
		if(elem.find("cpu MHz") != string::npos) {
			const auto colon_pos = elem.find(": ");
			if(colon_pos != string::npos) {
				cpu_clock = uint32_t(round(stod(elem.substr(colon_pos + 2))));
			}
		}
	}
#elif defined(__WINDOWS__)
	// registry fun
	uint32_t cpu_clock_32 = 0;
	const uint32_t val_size = sizeof(uint32_t);
	RegGetValue(HKEY_LOCAL_MACHINE, TEXT("HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0\\"), TEXT("~MHz"),
				RRF_RT_DWORD, nullptr, (LPBYTE)&cpu_clock_32, (LPDWORD)&val_size); // note: don't care about failure/return val
	cpu_clock = cpu_clock_32;
#else
#error "unsupported platform"
#endif
	if(cpu_name == "") cpu_name = "UNKNOWN CPU";
	
	device.type = compute_device::TYPE::CPU0;
	device.name = cpu_name;
	device.units = core::get_hw_thread_count();
	device.clock = uint32_t(cpu_clock);
	device.global_mem_size = uint64_t(SDL_GetSystemRAM()) * 1024ull * 1024ull;
	device.max_mem_alloc = device.global_mem_size;
	device.local_mem_size = 128ull * 1024ull * 1024ull; // TODO: proper value?
	device.constant_mem_size = 128ull * 1024ull * 1024ull; // TODO: proper value?
	device.version_str = FLOOR_BUILD_VERSION_STR;
	device.driver_version_str = FLOOR_BUILD_VERSION_STR;
	
	const auto lc_cpu_name = core::str_to_lower(device.name);
	if(lc_cpu_name.find("intel") != string::npos) {
		device.vendor = compute_device::VENDOR::INTEL;
		device.vendor_name = "Intel";
	}
	else if(lc_cpu_name.find("amd") != string::npos) {
		device.vendor = compute_device::VENDOR::AMD;
		device.vendor_name = "AMD";
	}
	else if(lc_cpu_name.find("apple") != string::npos) {
		device.vendor = compute_device::VENDOR::APPLE;
		device.vendor_name = "Apple";
	}
	// TODO: ARM cpu names?
	else {
		device.vendor = compute_device::VENDOR::HOST;
		device.vendor_name = "Host";
	}
	
	device.max_work_group_size = device.units;
	device.max_work_group_item_sizes = { device.units, device.units, device.units };
	device.max_work_item_sizes = { sizeof(uint64_t) };
	device.max_image_1d_dim = { 0 };
	device.max_image_2d_dim = { 0, 0 };
	device.max_image_3d_dim = { 0, 0, 0 };
	device.image_support = false; // TODO: add image s/w support
	device.double_support = true;
	device.unified_memory = true;
	device.basic_64_bit_atomics_support = true; // TODO: also true on arm?
	device.extended_64_bit_atomics_support = true;
#if defined(PLATFORM_X32)
	device.bitness = 32;
#elif defined(PLATFORM_X64)
	device.bitness = 64;
#endif
	
	//
	fastest_cpu_device = devices[0];
	fastest_device = fastest_cpu_device;
	if(fastest_cpu_device != nullptr) {
		log_debug("CPU (Units: %u, Clock: %u MHz, Memory: %u MB): %s",
				  fastest_cpu_device->units,
				  fastest_cpu_device->clock,
				  (unsigned int)(fastest_cpu_device->global_mem_size / 1024ull / 1024ull),
				  fastest_cpu_device->name);
		log_debug("fastest CPU device: %s, %s (score: %u)",
				  fastest_cpu_device->vendor_name, fastest_cpu_device->name, fastest_cpu_device->units * fastest_cpu_device->clock);
	}
	
	// for now: only maintain a single queue
	main_queue = make_shared<host_queue>(fastest_cpu_device);
}

shared_ptr<compute_queue> host_compute::create_queue(shared_ptr<compute_device> dev) {
	if(dev == nullptr) return {};
	return main_queue;
}

shared_ptr<compute_buffer> host_compute::create_buffer(const size_t& size, const COMPUTE_MEMORY_FLAG flags,
													   const uint32_t opengl_type) {
	return make_shared<host_buffer>((host_device*)fastest_device.get(), size, flags, opengl_type);
}

shared_ptr<compute_buffer> host_compute::create_buffer(const size_t& size, void* data, const COMPUTE_MEMORY_FLAG flags,
													   const uint32_t opengl_type) {
	return make_shared<host_buffer>((host_device*)fastest_device.get(), size, data, flags, opengl_type);
}

shared_ptr<compute_buffer> host_compute::create_buffer(shared_ptr<compute_device> device,
													   const size_t& size, const COMPUTE_MEMORY_FLAG flags,
													   const uint32_t opengl_type) {
	return make_shared<host_buffer>((host_device*)device.get(), size, flags, opengl_type);
}

shared_ptr<compute_buffer> host_compute::create_buffer(shared_ptr<compute_device> device,
													   const size_t& size, void* data,
													   const COMPUTE_MEMORY_FLAG flags,
													   const uint32_t opengl_type) {
	return make_shared<host_buffer>((host_device*)device.get(), size, data, flags, opengl_type);
}

shared_ptr<compute_buffer> host_compute::wrap_buffer(shared_ptr<compute_device> device floor_unused,
													 const uint32_t opengl_buffer floor_unused,
													 const uint32_t opengl_type floor_unused,
													 const COMPUTE_MEMORY_FLAG flags floor_unused) {
	// TODO: implement this
	return {};
}

shared_ptr<compute_buffer> host_compute::wrap_buffer(shared_ptr<compute_device> device floor_unused,
													 const uint32_t opengl_buffer floor_unused,
													 const uint32_t opengl_type floor_unused,
													 void* data floor_unused,
													 const COMPUTE_MEMORY_FLAG flags floor_unused) {
	// TODO: implement this
	return {};
}

shared_ptr<compute_image> host_compute::create_image(shared_ptr<compute_device> device,
													 const uint4 image_dim,
													 const COMPUTE_IMAGE_TYPE image_type,
													 const COMPUTE_MEMORY_FLAG flags,
													 const uint32_t opengl_type) {
	return make_shared<host_image>((host_device*)device.get(), image_dim, image_type, nullptr, flags, opengl_type);
}

shared_ptr<compute_image> host_compute::create_image(shared_ptr<compute_device> device,
													 const uint4 image_dim,
													 const COMPUTE_IMAGE_TYPE image_type,
													 void* data,
													 const COMPUTE_MEMORY_FLAG flags,
													 const uint32_t opengl_type) {
	return make_shared<host_image>((host_device*)device.get(), image_dim, image_type, data, flags, opengl_type);
}

shared_ptr<compute_image> host_compute::wrap_image(shared_ptr<compute_device> device floor_unused,
												   const uint32_t opengl_image floor_unused,
												   const uint32_t opengl_target floor_unused,
												   const COMPUTE_MEMORY_FLAG flags floor_unused) {
	// TODO: implement this
	return {};
}

shared_ptr<compute_image> host_compute::wrap_image(shared_ptr<compute_device> device floor_unused,
												   const uint32_t opengl_image floor_unused,
												   const uint32_t opengl_target floor_unused,
												   void* data floor_unused,
												   const COMPUTE_MEMORY_FLAG flags floor_unused) {
	// TODO: implement this
	return {};
}

void host_compute::finish() {
	// TODO: !
}

void host_compute::flush() {
	// TODO: !
}

void host_compute::activate_context() {
	// TODO: !
}

void host_compute::deactivate_context() {
	// TODO: !
}

shared_ptr<compute_program> host_compute::add_program_file(const string& file_name floor_unused,
														   const string additional_options floor_unused) {
	return make_shared<host_program>();
}

shared_ptr<compute_program> host_compute::add_program_source(const string& source_code floor_unused,
															 const string additional_options floor_unused) {
	return make_shared<host_program>();
}

shared_ptr<compute_program> host_compute::add_precompiled_program_file(const string& file_name floor_unused,
																	   const vector<llvm_compute::kernel_info>& kernel_infos floor_unused) {
	// TODO: !
	log_error("not yet supported by host_compute!");
	return {};
}

#endif
