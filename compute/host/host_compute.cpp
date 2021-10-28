/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2021 Florian Ziesche
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
#include <floor/compute/device/host_limits.hpp>
#include <floor/compute/host/elf_binary.hpp>

#if defined(__APPLE__)
#include <floor/darwin/darwin_helper.hpp>
#endif

#include <floor/compute/llvm_toolchain.hpp>
#include <floor/compute/universal_binary.hpp>

#if (defined(__APPLE__) || defined(__FreeBSD__) || defined(__OpenBSD__))
#include <sys/types.h>
#include <sys/sysctl.h>
#endif

#if !defined(FLOOR_IOS)
#include <cpuid.h>
#else
#include <mach-o/arch.h>
#endif

#if defined(__WINDOWS__)
#include <floor/core/platform_windows.hpp>
#include <winreg.h>
#include <floor/core/essentials.hpp> // cleanup
#endif

// provided by SDL2
extern "C" DECLSPEC int SDLCALL SDL_GetSystemRAM();

host_compute::host_compute() : compute_context() {
	platform_vendor = COMPUTE_VENDOR::HOST;
	
	//
	devices.emplace_back(make_unique<host_device>());
	auto& device = (host_device&)*devices.back();
	device.context = this;
	
	// gather "device"/cpu information, this is very platform dependent
	string cpu_name;
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
	cpu_name = "Apple ARMv8";
	const auto nx_info = NXGetLocalArchInfo();
	if (nx_info) {
		cpu_name += "("s + nx_info->name + ", " + nx_info->description + ")";
	}
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
	cpu_clock = 1300; // at least
	
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
	DWORD val_size = sizeof(uint32_t);
	RegGetValue(HKEY_LOCAL_MACHINE, TEXT("HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0\\"), TEXT("~MHz"),
				RRF_RT_DWORD, nullptr, (LPBYTE)&cpu_clock_32, &val_size); // note: don't care about failure/return val
	cpu_clock = cpu_clock_32;
#else
#error "unsupported platform"
#endif
	if(cpu_name == "") cpu_name = "UNKNOWN CPU";
	
	device.name = cpu_name;
	device.units = core::get_hw_thread_count();
	device.clock = uint32_t(cpu_clock);
	device.global_mem_size = uint64_t(SDL_GetSystemRAM()) * 1024ull * 1024ull;
	device.max_mem_alloc = device.global_mem_size;
	device.constant_mem_size = device.global_mem_size; // not different from normal ram
	
	const auto lc_cpu_name = core::str_to_lower(device.name);
	if(lc_cpu_name.find("intel") != string::npos) {
		device.vendor = COMPUTE_VENDOR::INTEL;
		device.vendor_name = "Intel";
	}
	else if(lc_cpu_name.find("amd") != string::npos) {
		device.vendor = COMPUTE_VENDOR::AMD;
		device.vendor_name = "AMD";
	}
	else if(lc_cpu_name.find("apple") != string::npos) {
		device.vendor = COMPUTE_VENDOR::APPLE;
		device.vendor_name = "Apple";
	}
	// TODO: ARM cpu names?
	else {
		device.vendor = COMPUTE_VENDOR::HOST;
		device.vendor_name = "Host";
	}
	
#if 0 // mt-item
	device.max_total_local_size = device.units;
	device.max_local_size = { device.units, device.units, device.units };
#else // mt-group
	device.max_total_local_size = host_limits::max_total_local_size;
	device.max_local_size = { host_limits::max_total_local_size };
#endif
	device.max_image_1d_buffer_dim = { (size_t)std::min(device.max_mem_alloc, uint64_t(0xFFFFFFFFu)) };
	
	// figure out CPU tier
#if !defined(FLOOR_IOS)
	if (core::cpu_has_avx512()) {
		device.cpu_tier = HOST_CPU_TIER::X86_TIER_4;
	} else if (core::cpu_has_avx2() && core::cpu_has_fma()) {
		device.cpu_tier = HOST_CPU_TIER::X86_TIER_3;
	} else if (core::cpu_has_avx()) {
		device.cpu_tier = HOST_CPU_TIER::X86_TIER_2;
	} else {
		device.cpu_tier = HOST_CPU_TIER::X86_TIER_1;
	}
#else
	if (nx_info && nx_info->cpusubtype >= CPU_SUBTYPE_ARM64E) {
		device.cpu_tier = HOST_CPU_TIER::ARM_TIER_2;
	} else {
		device.cpu_tier = HOST_CPU_TIER::ARM_TIER_1;
	}
#endif
	
	//
	supported = true;
	fastest_cpu_device = devices[0].get();
	fastest_device = fastest_cpu_device;
	
	log_debug("CPU ($, Units: $, Clock: $ MHz, Memory: $ MB): $",
			  host_cpu_tier_to_string(device.cpu_tier),
			  fastest_cpu_device->units,
			  fastest_cpu_device->clock,
			  uint32_t(fastest_cpu_device->global_mem_size / 1024ull / 1024ull),
			  fastest_cpu_device->name);
	log_debug("fastest CPU device: $, $ (score: $)",
			  fastest_cpu_device->vendor_name, fastest_cpu_device->name, fastest_cpu_device->units * fastest_cpu_device->clock);
	
	// for now: only maintain a single queue
	main_queue = make_shared<host_queue>(*fastest_cpu_device);
}

shared_ptr<compute_queue> host_compute::create_queue(const compute_device& dev floor_unused) const {
	return main_queue;
}

shared_ptr<compute_buffer> host_compute::create_buffer(const compute_queue& cqueue,
													   const size_t& size, const COMPUTE_MEMORY_FLAG flags,
													   const uint32_t opengl_type) const {
	return make_shared<host_buffer>(cqueue, size, flags, opengl_type);
}

shared_ptr<compute_buffer> host_compute::create_buffer(const compute_queue& cqueue,
													   const size_t& size, void* data,
													   const COMPUTE_MEMORY_FLAG flags,
													   const uint32_t opengl_type) const {
	return make_shared<host_buffer>(cqueue, size, data, flags, opengl_type);
}

shared_ptr<compute_buffer> host_compute::wrap_buffer(const compute_queue& cqueue,
													 const uint32_t opengl_buffer,
													 const uint32_t opengl_type,
													 const COMPUTE_MEMORY_FLAG flags) const {
	const auto info = compute_buffer::get_opengl_buffer_info(opengl_buffer, opengl_type, flags);
	if(!info.valid) return {};
	return make_shared<host_buffer>(cqueue, info.size, nullptr,
									flags | COMPUTE_MEMORY_FLAG::OPENGL_SHARING,
									opengl_type, opengl_buffer);
}

shared_ptr<compute_buffer> host_compute::wrap_buffer(const compute_queue& cqueue,
													 const uint32_t opengl_buffer,
													 const uint32_t opengl_type,
													 void* data,
													 const COMPUTE_MEMORY_FLAG flags) const {
	const auto info = compute_buffer::get_opengl_buffer_info(opengl_buffer, opengl_type, flags);
	if(!info.valid) return {};
	return make_shared<host_buffer>(cqueue, info.size, data,
									flags | COMPUTE_MEMORY_FLAG::OPENGL_SHARING,
									opengl_type, opengl_buffer);
}

shared_ptr<compute_buffer> host_compute::wrap_buffer(const compute_queue& cqueue,
													 metal_buffer& mtl_buffer,
													 const COMPUTE_MEMORY_FLAG flags) const {
#if !defined(FLOOR_NO_METAL)
	return make_shared<host_buffer>(cqueue, ((const compute_buffer&)mtl_buffer).get_size(), nullptr,
									flags | COMPUTE_MEMORY_FLAG::METAL_SHARING, 0, 0, (compute_buffer*)&mtl_buffer);
#else
	return compute_context::wrap_buffer(cqueue, mtl_buffer, flags);
#endif
}

shared_ptr<compute_image> host_compute::create_image(const compute_queue& cqueue,
													 const uint4 image_dim,
													 const COMPUTE_IMAGE_TYPE image_type,
													 const COMPUTE_MEMORY_FLAG flags,
													 const uint32_t opengl_type) const {
	return make_shared<host_image>(cqueue, image_dim, image_type, nullptr, flags, opengl_type);
}

shared_ptr<compute_image> host_compute::create_image(const compute_queue& cqueue,
													 const uint4 image_dim,
													 const COMPUTE_IMAGE_TYPE image_type,
													 void* data,
													 const COMPUTE_MEMORY_FLAG flags,
													 const uint32_t opengl_type) const {
	return make_shared<host_image>(cqueue, image_dim, image_type, data, flags, opengl_type);
}

shared_ptr<compute_image> host_compute::wrap_image(const compute_queue& cqueue,
												   const uint32_t opengl_image,
												   const uint32_t opengl_target,
												   const COMPUTE_MEMORY_FLAG flags) const {
	const auto info = compute_image::get_opengl_image_info(opengl_image, opengl_target, flags);
	if(!info.valid) return {};
	return make_shared<host_image>(cqueue, info.image_dim, info.image_type, nullptr,
								   flags | COMPUTE_MEMORY_FLAG::OPENGL_SHARING,
								   opengl_target, opengl_image, &info);
}

shared_ptr<compute_image> host_compute::wrap_image(const compute_queue& cqueue,
												   const uint32_t opengl_image,
												   const uint32_t opengl_target,
												   void* data,
												   const COMPUTE_MEMORY_FLAG flags) const {
	const auto info = compute_image::get_opengl_image_info(opengl_image, opengl_target, flags);
	if(!info.valid) return {};
	return make_shared<host_image>(cqueue, info.image_dim, info.image_type, data,
								   flags | COMPUTE_MEMORY_FLAG::OPENGL_SHARING,
								   opengl_target, opengl_image, &info);
}

shared_ptr<compute_image> host_compute::wrap_image(const compute_queue& cqueue,
												   metal_image& mtl_image,
												   const COMPUTE_MEMORY_FLAG flags) const {
#if !defined(FLOOR_NO_METAL)
	return make_shared<host_image>(cqueue, ((const compute_image&)mtl_image).get_image_dim(), ((const compute_image&)mtl_image).get_image_type(), nullptr,
								   flags | COMPUTE_MEMORY_FLAG::METAL_SHARING, 0, 0, nullptr, (compute_image*)&mtl_image);
#else
	return compute_context::wrap_image(cqueue, mtl_image, flags);
#endif
}

shared_ptr<compute_program> host_compute::add_universal_binary(const string& file_name) {
	auto bins = universal_binary::load_dev_binaries_from_archive(file_name, *this);
	if (bins.ar == nullptr || bins.dev_binaries.empty()) {
		log_error("failed to load universal binary: $", file_name);
		return {};
	}
	
	// create the program
	host_program::program_map_type prog_map;
	prog_map.reserve(devices.size());
	for (size_t i = 0, dev_count = devices.size(); i < dev_count; ++i) {
		const auto& host_dev = (const host_device&)*devices[i];
		const auto& dev_best_bin = bins.dev_binaries[i];
		const auto func_info = universal_binary::translate_function_info(dev_best_bin.first->functions);
		prog_map.insert_or_assign(host_dev,
								  create_host_program_internal(host_dev,
															   {},
															   dev_best_bin.first->data.data(),
															   dev_best_bin.first->data.size(),
															   func_info,
															   false /* TODO: true? */));
	}
	
	return add_program(move(prog_map));
}

shared_ptr<host_program> host_compute::add_program(host_program::program_map_type&& prog_map) {
	// create the program object, which in turn will create kernel objects for all kernel functions in the program,
	// for all devices contained in the program map
	auto prog = make_shared<host_program>(*fastest_cpu_device, move(prog_map));
	{
		GUARD(programs_lock);
		programs.push_back(prog);
	}
	return prog;
}

shared_ptr<compute_program> host_compute::add_program_file(const string& file_name,
														   const string additional_options) {
	if (!has_host_device_support()) {
		return make_shared<host_program>(*fastest_device, host_program::program_map_type {});
	}
	compile_options options { .cli = additional_options };
	return add_program_file(file_name, options);
}

shared_ptr<compute_program> host_compute::add_program_file(const string& file_name,
														   compile_options options) {
	if (!has_host_device_support()) {
		return make_shared<host_program>(*fastest_device, host_program::program_map_type {});
	}
	
	// compile the source file for all devices in the context
	host_program::program_map_type prog_map;
	prog_map.reserve(devices.size());
	options.target = llvm_toolchain::TARGET::HOST_COMPUTE_CPU;
	for(const auto& dev : devices) {
		const auto& host_dev = (const host_device&)*dev;
		prog_map.insert_or_assign(host_dev,
								  create_host_program(host_dev, llvm_toolchain::compile_program_file(*dev, file_name, options)));
	}
	return add_program(move(prog_map));
}

shared_ptr<compute_program> host_compute::add_program_source(const string& source_code,
															 const string additional_options) {
	if (!has_host_device_support()) {
		return make_shared<host_program>(*fastest_device, host_program::program_map_type {});
	}
	
	compile_options options { .cli = additional_options };
	return add_program_source(source_code, options);
}

shared_ptr<compute_program> host_compute::add_program_source(const string& source_code,
															 compile_options options) {
	if (!has_host_device_support()) {
		return make_shared<host_program>(*fastest_device, host_program::program_map_type {});
	}
	
	// compile the source code for all devices in the context
	host_program::program_map_type prog_map;
	prog_map.reserve(devices.size());
	options.target = llvm_toolchain::TARGET::HOST_COMPUTE_CPU;
	for(const auto& dev : devices) {
		const auto& host_dev = (const host_device&)*dev;
		prog_map.insert_or_assign(host_dev,
								  create_host_program(host_dev, llvm_toolchain::compile_program(*dev, source_code, options)));
	}
	return add_program(move(prog_map));
}

host_program::host_program_entry host_compute::create_host_program(const host_device& device,
																   llvm_toolchain::program_data program) {
	if(!program.valid) {
		return {};
	}
	return create_host_program_internal(device, program.data_or_filename.data(), nullptr, 0,
										program.functions, program.options.silence_debug_output);
}

host_program::host_program_entry host_compute::create_host_program_internal(const host_device& device floor_unused /* TODO: use device */,
																			const optional<string> elf_bin_file_name,
																			const uint8_t* elf_bin_data,
																			const size_t elf_bin_size,
																			const vector<llvm_toolchain::function_info>& functions,
																			const bool& silence_debug_output) {
	host_program::host_program_entry ret;
	ret.functions = functions;
	
	unique_ptr<elf_binary> bin;
	if (elf_bin_file_name && !elf_bin_file_name->empty()) {
		bin = make_unique<elf_binary>(*elf_bin_file_name);
	} else if (elf_bin_data != nullptr && elf_bin_size > 0) {
		bin = make_unique<elf_binary>(elf_bin_data, elf_bin_size);
	} else {
		log_error("invalid ELF binary specification");
		return {};
	}
	if (!bin->is_valid()) {
		log_error("failed to load ELF binary");
		return {};
	}
	ret.program = move(bin);
	
	if (!silence_debug_output) {
		log_debug("successfully created host program!");
	}
	
	ret.valid = true;
	return ret;
}

shared_ptr<compute_program> host_compute::add_precompiled_program_file(const string& file_name floor_unused,
																	   const vector<llvm_toolchain::function_info>& functions floor_unused) {
	log_error("not supported by host_compute!");
	return {};
}

shared_ptr<compute_program::program_entry> host_compute::create_program_entry(const compute_device& device floor_unused,
																			  llvm_toolchain::program_data program,
																			  const llvm_toolchain::TARGET) {
	return make_shared<compute_program::program_entry>(compute_program::program_entry { {}, program.functions, true });
}

bool host_compute::has_host_device_support() const {
#if defined(__WINDOWS__) || defined(FLOOR_IOS)
	return false;
#else
	return true;
#endif
}

#endif
