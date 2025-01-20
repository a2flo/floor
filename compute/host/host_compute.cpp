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

#if !defined(FLOOR_NO_HOST_COMPUTE)
#include <floor/compute/host/host_compute.hpp>
#include <floor/core/platform.hpp>
#include <floor/core/logger.hpp>
#include <floor/core/core.hpp>
#include <floor/core/file_io.hpp>
#include <floor/compute/device/host_limits.hpp>
#include <floor/compute/host/elf_binary.hpp>
#include <floor/floor/floor.hpp>

#if defined(__APPLE__)
#include <floor/darwin/darwin_helper.hpp>
#endif

#include <floor/compute/llvm_toolchain.hpp>
#include <floor/compute/universal_binary.hpp>

#if (defined(__APPLE__) || defined(__FreeBSD__) || defined(__OpenBSD__))
#include <sys/types.h>
#include <sys/sysctl.h>
#endif

#if defined(__x86_64__)
#include <cpuid.h>
#endif

#if defined(__WINDOWS__)
#include <floor/core/platform_windows.hpp>
#include <winreg.h>
#include <floor/core/essentials.hpp> // cleanup
#endif

// provided by SDL3
#if defined(SDL_DECLSPEC)
extern "C" SDL_DECLSPEC int SDLCALL SDL_GetSystemRAM();
#else // TODO: remove this (keep it for older SDL3 versions for now)
extern "C" DECLSPEC int SDLCALL SDL_GetSystemRAM();
#endif

host_compute::host_compute(const COMPUTE_CONTEXT_FLAGS ctx_flags, const bool has_toolchain_) : compute_context(ctx_flags, has_toolchain_) {
	platform_vendor = COMPUTE_VENDOR::HOST;
	
	//
	devices.emplace_back(make_unique<host_device>());
	auto& device = (host_device&)*devices.back();
	device.context = this;
	
	// gather "device"/cpu information, this is very platform dependent
	string cpu_name;
	uint64_t cpu_clock = 0;
	
	// we can get the actual cpu name quite easily on x86 through cpuid instructions
#if defined(__x86_64__)
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
#elif defined(__APPLE__) && defined(__aarch64__)
	cpu_name = "Apple ARMv8";
	
	// if brand_string contains a proper non-generic name, use that as the CPU name
	string cpu_brand(64, 0);
	size_t cpu_brand_size = cpu_brand.size() - 1;
	sysctlbyname("machdep.cpu.brand_string", &cpu_brand[0], &cpu_brand_size, nullptr, 0);
	if (cpu_brand != "Apple processor") {
		cpu_name = cpu_brand;
	}
#else
#error "unhandled arch"
#endif
	
	// now onto getting the cpu clock speed:
#if (defined(__APPLE__) && !defined(FLOOR_IOS) && !defined(FLOOR_VISIONOS)) || defined(__FreeBSD__)
	// can simply use sysctl on macOS (and freebsd)
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
	
#elif (defined(__APPLE__) && defined(FLOOR_VISIONOS)) // can't query this on visionOS, hardcore it with somewhat accurate values
	cpu_clock = 3500; // at least
	
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
	
	device.max_total_local_size = host_limits::max_total_local_size;
	device.max_local_size = { host_limits::max_total_local_size };
	device.max_image_1d_buffer_dim = { (size_t)std::min(device.max_mem_alloc, uint64_t(0xFFFFFFFFu)) };
	
	// figure out CPU tier
#if defined(__x86_64__)
	if (core::cpu_has_avx512()) {
		if (core::cpu_has_avx512_tier_5()) {
			device.cpu_tier = HOST_CPU_TIER::X86_TIER_5;
		} else {
			device.cpu_tier = HOST_CPU_TIER::X86_TIER_4;
		}
	} else if (core::cpu_has_avx2() && core::cpu_has_fma()) {
		device.cpu_tier = HOST_CPU_TIER::X86_TIER_3;
	} else if (core::cpu_has_avx()) {
		device.cpu_tier = HOST_CPU_TIER::X86_TIER_2;
	} else {
		device.cpu_tier = HOST_CPU_TIER::X86_TIER_1;
	}
#elif defined(__aarch64__)
#if defined(__APPLE__)
	// figure out the actual ARM core/ISA
	uint32_t cpufamily = 0;
	size_t cpufamily_size = sizeof(cpufamily);
	sysctlbyname("hw.cpufamily", &cpufamily, &cpufamily_size, nullptr, 0);
	switch (cpufamily) {
		default:
			// default to highest tier for all unknown (newer) cores
			device.cpu_tier = HOST_CPU_TIER::ARM_TIER_7;
			break;
		case 0x37a09642 /* Cyclone A7 */:
		case 0x2c91a47e /* Typhoon A8 */:
		case 0x92fb37c8 /* Twister A9 */:
			device.cpu_tier = HOST_CPU_TIER::ARM_TIER_1;
			break;
		case 0x67ceee93 /* Hurricane/Zephyr A10 */:
			device.cpu_tier = HOST_CPU_TIER::ARM_TIER_2;
			break;
		case 0xe81e7ef6 /* Monsoon/Mistral A11 */:
			device.cpu_tier = HOST_CPU_TIER::ARM_TIER_3;
			break;
		case 0x07d34b9f /* Vortex/Tempest A12 */:
			device.cpu_tier = HOST_CPU_TIER::ARM_TIER_4;
			break;
		case 0x462504d2 /* Lightning/Thunder A13 */:
			device.cpu_tier = HOST_CPU_TIER::ARM_TIER_5;
			break;
		case 0x1b588bb3 /* Firestorm/Icestorm A14 & M1 */:
			device.cpu_tier = HOST_CPU_TIER::ARM_TIER_6;
			break;
		case 0xda33d83d /* Blizzard/Avalanche A15 & M2 */:
		case 0x8765edea /* Everest/Sawtooth A16 */:
		case 0x2876f5b5 /* Coll A17 */:
		case 0xfa33415e /* Ibiza M3 */:
		case 0x72015832 /* Palma M3 Max */:
		case 0x5f4dea93 /* Lobos M3 Pro */:
			device.cpu_tier = HOST_CPU_TIER::ARM_TIER_7;
			break;
		case 0x6f5129ac /* Donan M4 */:
		case 0x17d5b93a /* Brava M4 Max */:
		case 0x75d4acb9 /* Tahiti A18 Pro */:
		case 0x204526d0 /* Tupai A18 */:
			device.cpu_tier = HOST_CPU_TIER::ARM_TIER_7; // TODO: TIER_8, ARMv9.2
			break;
	}
	
#else
	// TODO: handle this on non-Apple platforms
	device.cpu_tier = HOST_CPU_TIER::ARM_TIER_1;
#endif
#else
#error "unhandled arch"
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

std::optional<uint32_t> host_compute::get_max_distinct_queue_count(const compute_device&) const {
	return 1;
}

std::optional<uint32_t> host_compute::get_max_distinct_compute_queue_count(const compute_device&) const {
	return 1;
}

vector<shared_ptr<compute_queue>> host_compute::create_distinct_queues(const compute_device& dev, const uint32_t wanted_count) const {
	if (wanted_count == 0) {
		return {};
	}
	return { create_queue(dev) };
}

vector<shared_ptr<compute_queue>> host_compute::create_distinct_compute_queues(const compute_device& dev, const uint32_t wanted_count) const {
	return create_distinct_queues(dev, wanted_count);
}

unique_ptr<compute_fence> host_compute::create_fence(const compute_queue&) const {
	log_error("fence creation not yet supported by host_compute!");
	return {};
}

shared_ptr<compute_buffer> host_compute::create_buffer(const compute_queue& cqueue,
													   const size_t& size, const COMPUTE_MEMORY_FLAG flags) const {
	return add_resource(make_shared<host_buffer>(cqueue, size, flags));
}

shared_ptr<compute_buffer> host_compute::create_buffer(const compute_queue& cqueue,
													   std::span<uint8_t> data,
													   const COMPUTE_MEMORY_FLAG flags) const {
	return add_resource(make_shared<host_buffer>(cqueue, data.size_bytes(), data, flags));
}

shared_ptr<compute_buffer> host_compute::wrap_buffer(const compute_queue& cqueue,
													 metal_buffer& mtl_buffer,
													 const COMPUTE_MEMORY_FLAG flags) const {
#if !defined(FLOOR_NO_METAL)
	return add_resource(make_shared<host_buffer>(cqueue, ((const compute_buffer&)mtl_buffer).get_size(), std::span<uint8_t> {},
												 flags | COMPUTE_MEMORY_FLAG::METAL_SHARING, (compute_buffer*)&mtl_buffer));
#else
	return compute_context::wrap_buffer(cqueue, mtl_buffer, flags);
#endif
}

shared_ptr<compute_buffer> host_compute::wrap_buffer(const compute_queue& cqueue,
													 vulkan_buffer& vk_buffer,
													 const COMPUTE_MEMORY_FLAG flags) const {
#if !defined(FLOOR_NO_VULKAN)
	return add_resource(make_shared<host_buffer>(cqueue, ((const compute_buffer&)vk_buffer).get_size(), std::span<uint8_t> {},
												 flags | COMPUTE_MEMORY_FLAG::VULKAN_SHARING, (compute_buffer*)&vk_buffer));
#else
	return compute_context::wrap_buffer(cqueue, vk_buffer, flags);
#endif
}

shared_ptr<compute_image> host_compute::create_image(const compute_queue& cqueue,
													 const uint4 image_dim,
													 const COMPUTE_IMAGE_TYPE image_type,
													 const COMPUTE_MEMORY_FLAG flags) const {
	return add_resource(make_shared<host_image>(cqueue, image_dim, image_type, std::span<uint8_t> {}, flags));
}

shared_ptr<compute_image> host_compute::create_image(const compute_queue& cqueue,
													 const uint4 image_dim,
													 const COMPUTE_IMAGE_TYPE image_type,
													 std::span<uint8_t> data,
													 const COMPUTE_MEMORY_FLAG flags) const {
	return add_resource(make_shared<host_image>(cqueue, image_dim, image_type, data, flags));
}

shared_ptr<compute_image> host_compute::wrap_image(const compute_queue& cqueue,
												   metal_image& mtl_image,
												   const COMPUTE_MEMORY_FLAG flags) const {
#if !defined(FLOOR_NO_METAL)
	return add_resource(make_shared<host_image>(cqueue, ((const compute_image&)mtl_image).get_image_dim(),
												((const compute_image&)mtl_image).get_image_type(), std::span<uint8_t> {},
												flags | COMPUTE_MEMORY_FLAG::METAL_SHARING, (compute_image*)&mtl_image));
#else
	return compute_context::wrap_image(cqueue, mtl_image, flags);
#endif
}

shared_ptr<compute_image> host_compute::wrap_image(const compute_queue& cqueue,
												   vulkan_image& vk_image,
												   const COMPUTE_MEMORY_FLAG flags) const {
#if !defined(FLOOR_NO_VULKAN)
	return add_resource(make_shared<host_image>(cqueue, ((const compute_image&)vk_image).get_image_dim(),
												((const compute_image&)vk_image).get_image_type(), std::span<uint8_t> {},
												flags | COMPUTE_MEMORY_FLAG::VULKAN_SHARING, (compute_image*)&vk_image));
#else
	return compute_context::wrap_image(cqueue, vk_image, flags);
#endif
}

shared_ptr<compute_program> host_compute::create_program_from_archive_binaries(universal_binary::archive_binaries& bins) {
	// create the program
	host_program::program_map_type prog_map;
	prog_map.reserve(devices.size());
	for (size_t i = 0, dev_count = devices.size(); i < dev_count; ++i) {
		const auto& host_dev = (const host_device&)*devices[i];
		const auto& dev_best_bin = bins.dev_binaries[i];
		const auto func_info = universal_binary::translate_function_info(dev_best_bin.first->function_info);
		prog_map.insert_or_assign(host_dev,
								  create_host_program_internal(host_dev,
															   {},
															   dev_best_bin.first->data.data(),
															   dev_best_bin.first->data.size(),
															   func_info,
															   false /* TODO: true? */));
	}
	return add_program(std::move(prog_map));
}

shared_ptr<compute_program> host_compute::add_universal_binary(const string& file_name) {
	auto bins = universal_binary::load_dev_binaries_from_archive(file_name, *this);
	if (bins.ar == nullptr || bins.dev_binaries.empty()) {
		log_error("failed to load universal binary: $", file_name);
		return {};
	}
	return create_program_from_archive_binaries(bins);
}

shared_ptr<compute_program> host_compute::add_universal_binary(const span<const uint8_t> data) {
	auto bins = universal_binary::load_dev_binaries_from_archive(data, *this);
	if (bins.ar == nullptr || bins.dev_binaries.empty()) {
		log_error("failed to load universal binary (in-memory data)");
		return {};
	}
	return create_program_from_archive_binaries(bins);
}

shared_ptr<host_program> host_compute::add_program(host_program::program_map_type&& prog_map) {
	// create the program object, which in turn will create kernel objects for all kernel functions in the program,
	// for all devices contained in the program map
	auto prog = make_shared<host_program>(*fastest_cpu_device, std::move(prog_map));
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
	return add_program(std::move(prog_map));
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
	return add_program(std::move(prog_map));
}

host_program::host_program_entry host_compute::create_host_program(const host_device& device,
																   llvm_toolchain::program_data program) {
	if(!program.valid) {
		return {};
	}
	return create_host_program_internal(device, program.data_or_filename.data(), nullptr, 0,
										program.function_info, program.options.silence_debug_output);
}

host_program::host_program_entry host_compute::create_host_program_internal(const host_device& device floor_unused /* TODO: use device */,
																			const optional<string> elf_bin_file_name,
																			const uint8_t* elf_bin_data,
																			const size_t elf_bin_size,
																			const vector<llvm_toolchain::function_info>& function_info,
																			const bool& silence_debug_output) {
	host_program::host_program_entry ret;
	ret.functions = function_info;
	
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
	ret.program = std::move(bin);
	
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
	return make_shared<compute_program::program_entry>(compute_program::program_entry { {}, program.function_info, true });
}

bool host_compute::has_host_device_support() const {
	return floor::get_host_run_on_device();
}

unique_ptr<indirect_command_pipeline> host_compute::create_indirect_command_pipeline(const indirect_command_description& desc floor_unused) const {
	// TODO: !
	log_error("not yet supported by host_compute!");
	return {};
}

#endif
