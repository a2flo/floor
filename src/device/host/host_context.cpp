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
#include <floor/device/host/host_context.hpp>
#include <floor/core/platform.hpp>
#include <floor/core/logger.hpp>
#include <floor/core/core.hpp>
#include <floor/core/file_io.hpp>
#include <floor/device/backend/host_limits.hpp>
#include <floor/device/host/elf_binary.hpp>
#include <floor/device/host/host_function.hpp>
#include <floor/threading/thread_helpers.hpp>
#include <floor/floor.hpp>

#if defined(__APPLE__)
#include <floor/darwin/darwin_helper.hpp>
#endif

#include <floor/device/toolchain.hpp>
#include <floor/device/universal_binary.hpp>

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
extern "C" SDL_DECLSPEC int SDLCALL SDL_GetSystemRAM();

namespace fl {

host_context::host_context(const DEVICE_CONTEXT_FLAGS ctx_flags, const bool has_toolchain_) : device_context(ctx_flags, has_toolchain_) {
	platform_vendor = VENDOR::HOST;
	
	//
	devices.emplace_back(std::make_unique<host_device>());
	auto& device = (host_device&)*devices.back();
	device.context = this;
	
	// gather "device"/CPU information, this is very platform dependent
	std::string cpu_name = core::get_cpu_name();
	if (cpu_name.empty()) {
		cpu_name = "UNKNOWN CPU";
	}
	uint64_t cpu_clock = 0;
	
	// now onto getting the CPU clock speed:
#if (defined(__APPLE__) && !defined(FLOOR_IOS) && !defined(FLOOR_VISIONOS)) || defined(__FreeBSD__)
	// can simply use sysctl on macOS (and FreeBSD)
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
		// this should handle getting the CPU name for ARM CPUs (at least on linux)
		if(cpu_name == "" &&
		   elem.find("model name") != std::string::npos) {
			const auto colon_pos = elem.find(": ");
			if(colon_pos != std::string::npos) {
				cpu_name = elem.substr(colon_pos + 2);
			}
		}
		if(elem.find("cpu MHz") != std::string::npos) {
			const auto colon_pos = elem.find(": ");
			if(colon_pos != std::string::npos) {
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
	
	device.name = cpu_name;
	device.units = get_logical_core_count();
	device.clock = uint32_t(cpu_clock);
	device.global_mem_size = core::get_total_system_memory();
	device.max_mem_alloc = device.global_mem_size;
	device.constant_mem_size = device.global_mem_size; // not different from normal ram
	
	const auto lc_cpu_name = core::str_to_lower(device.name);
	if(lc_cpu_name.find("intel") != std::string::npos) {
		device.vendor = VENDOR::INTEL;
		device.vendor_name = "Intel";
	}
	else if(lc_cpu_name.find("amd") != std::string::npos) {
		device.vendor = VENDOR::AMD;
		device.vendor_name = "AMD";
	}
	else if(lc_cpu_name.find("apple") != std::string::npos) {
		device.vendor = VENDOR::APPLE;
		device.vendor_name = "Apple";
	}
	// TODO: ARM CPU names?
	else {
		device.vendor = VENDOR::HOST;
		device.vendor_name = "Host";
	}
	
	device.max_total_local_size = host_limits::max_total_local_size;
	device.max_resident_local_size = device.max_total_local_size;
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
	host_function::init();
	
	log_debug("CPU ($, Units: $, Clock: $ MHz, Memory: $ MB): $",
			  host_cpu_tier_to_string(device.cpu_tier),
			  fastest_cpu_device->units,
			  fastest_cpu_device->clock,
			  uint32_t(fastest_cpu_device->global_mem_size / 1024ull / 1024ull),
			  fastest_cpu_device->name);
	log_debug("fastest CPU device: $, $ (score: $)",
			  fastest_cpu_device->vendor_name, fastest_cpu_device->name, fastest_cpu_device->units * fastest_cpu_device->clock);
	
	// for now: only maintain a single queue
	main_queue = std::make_shared<host_queue>(*fastest_cpu_device);
}

std::shared_ptr<device_queue> host_context::create_queue(const device& dev floor_unused) const {
	return main_queue;
}

std::optional<uint32_t> host_context::get_max_distinct_queue_count(const device&) const {
	return 1;
}

std::optional<uint32_t> host_context::get_max_distinct_compute_queue_count(const device&) const {
	return 1;
}

std::vector<std::shared_ptr<device_queue>> host_context::create_distinct_queues(const device& dev, const uint32_t wanted_count) const {
	if (wanted_count == 0) {
		return {};
	}
	return { create_queue(dev) };
}

std::vector<std::shared_ptr<device_queue>> host_context::create_distinct_compute_queues(const device& dev, const uint32_t wanted_count) const {
	return create_distinct_queues(dev, wanted_count);
}

std::unique_ptr<device_fence> host_context::create_fence(const device_queue&) const {
	log_error("fence creation not yet supported by Host-Compute!");
	return {};
}

std::shared_ptr<device_buffer> host_context::create_buffer(const device_queue& cqueue,
													   const size_t& size, const MEMORY_FLAG flags) const {
	return add_resource(std::make_shared<host_buffer>(cqueue, size, flags));
}

std::shared_ptr<device_buffer> host_context::create_buffer(const device_queue& cqueue,
													   std::span<uint8_t> data,
													   const MEMORY_FLAG flags) const {
	return add_resource(std::make_shared<host_buffer>(cqueue, data.size_bytes(), data, flags));
}

std::shared_ptr<device_buffer> host_context::wrap_buffer(const device_queue& cqueue,
													 metal_buffer& mtl_buffer,
													 const MEMORY_FLAG flags) const {
#if !defined(FLOOR_NO_METAL)
	return add_resource(std::make_shared<host_buffer>(cqueue, ((const device_buffer&)mtl_buffer).get_size(), std::span<uint8_t> {},
												 flags | MEMORY_FLAG::METAL_SHARING, (device_buffer*)&mtl_buffer));
#else
	return device_context::wrap_buffer(cqueue, mtl_buffer, flags);
#endif
}

std::shared_ptr<device_buffer> host_context::wrap_buffer(const device_queue& cqueue,
													 vulkan_buffer& vk_buffer,
													 const MEMORY_FLAG flags) const {
#if !defined(FLOOR_NO_VULKAN)
	return add_resource(std::make_shared<host_buffer>(cqueue, ((const device_buffer&)vk_buffer).get_size(), std::span<uint8_t> {},
												 flags | MEMORY_FLAG::VULKAN_SHARING, (device_buffer*)&vk_buffer));
#else
	return device_context::wrap_buffer(cqueue, vk_buffer, flags);
#endif
}

std::shared_ptr<device_image> host_context::create_image(const device_queue& cqueue,
													 const uint4 image_dim,
													 const IMAGE_TYPE image_type,
													 std::span<uint8_t> data,
													 const MEMORY_FLAG flags,
													 const uint32_t mip_level_limit) const {
	return add_resource(std::make_shared<host_image>(cqueue, image_dim, image_type, data, flags, nullptr, mip_level_limit));
}

std::shared_ptr<device_image> host_context::wrap_image(const device_queue& cqueue,
												   metal_image& mtl_image,
												   const MEMORY_FLAG flags) const {
#if !defined(FLOOR_NO_METAL)
	return add_resource(std::make_shared<host_image>(cqueue, ((const device_image&)mtl_image).get_image_dim(),
												((const device_image&)mtl_image).get_image_type(), std::span<uint8_t> {},
												flags | MEMORY_FLAG::METAL_SHARING, (device_image*)&mtl_image));
#else
	return device_context::wrap_image(cqueue, mtl_image, flags);
#endif
}

std::shared_ptr<device_image> host_context::wrap_image(const device_queue& cqueue,
												   vulkan_image& vk_image,
												   const MEMORY_FLAG flags) const {
#if !defined(FLOOR_NO_VULKAN)
	return add_resource(std::make_shared<host_image>(cqueue, ((const device_image&)vk_image).get_image_dim(),
												((const device_image&)vk_image).get_image_type(), std::span<uint8_t> {},
												flags | MEMORY_FLAG::VULKAN_SHARING, (device_image*)&vk_image));
#else
	return device_context::wrap_image(cqueue, vk_image, flags);
#endif
}

std::shared_ptr<device_program> host_context::create_program_from_archive_binaries(universal_binary::archive_binaries& bins) {
	// create the program
	host_program::program_map_type prog_map;
	for (size_t i = 0, dev_count = devices.size(); i < dev_count; ++i) {
		const auto host_dev = (const host_device*)devices[i].get();
		const auto& dev_best_bin = bins.dev_binaries[i];
		const auto func_info = universal_binary::translate_function_info(dev_best_bin);
		prog_map.insert_or_assign(host_dev,
								  create_host_program_internal(*host_dev,
															   {},
															   dev_best_bin.first->data.data(),
															   dev_best_bin.first->data.size(),
															   func_info,
															   false /* TODO: true? */));
	}
	return add_program(std::move(prog_map));
}

std::shared_ptr<device_program> host_context::add_universal_binary(const std::string& file_name) {
	if (!has_host_device_support()) {
		return std::make_shared<host_program>(*fastest_device, host_program::program_map_type {});
	}
	
	auto bins = universal_binary::load_dev_binaries_from_archive(file_name, *this);
	if (bins.ar == nullptr || bins.dev_binaries.empty()) {
		log_error("failed to load universal binary: $", file_name);
		return {};
	}
	return create_program_from_archive_binaries(bins);
}

std::shared_ptr<device_program> host_context::add_universal_binary(const std::span<const uint8_t> data) {
	if (!has_host_device_support()) {
		return std::make_shared<host_program>(*fastest_device, host_program::program_map_type {});
	}
	
	auto bins = universal_binary::load_dev_binaries_from_archive(data, *this);
	if (bins.ar == nullptr || bins.dev_binaries.empty()) {
		log_error("failed to load universal binary (in-memory data)");
		return {};
	}
	return create_program_from_archive_binaries(bins);
}

std::shared_ptr<host_program> host_context::add_program(host_program::program_map_type&& prog_map) {
	// create the program object, which in turn will create function objects for all functions in the program,
	// for all devices contained in the program map
	auto prog = std::make_shared<host_program>(*fastest_cpu_device, std::move(prog_map));
	{
		GUARD(programs_lock);
		programs.push_back(prog);
	}
	return prog;
}

std::shared_ptr<device_program> host_context::add_program_file(const std::string& file_name,
														   const std::string additional_options) {
	if (!has_host_device_support()) {
		return std::make_shared<host_program>(*fastest_device, host_program::program_map_type {});
	}
	compile_options options { .cli = additional_options };
	return add_program_file(file_name, options);
}

std::shared_ptr<device_program> host_context::add_program_file(const std::string& file_name,
														   compile_options options) {
	if (!has_host_device_support()) {
		return std::make_shared<host_program>(*fastest_device, host_program::program_map_type {});
	}
	
	// compile the source file for all devices in the context
	host_program::program_map_type prog_map;
	options.target = toolchain::TARGET::HOST_COMPUTE_CPU;
	for(const auto& dev : devices) {
		const auto host_dev = (const host_device*)dev.get();
		prog_map.insert_or_assign(host_dev,
								  create_host_program(*host_dev, toolchain::compile_program_file(*dev, file_name, options)));
	}
	return add_program(std::move(prog_map));
}

std::shared_ptr<device_program> host_context::add_program_source(const std::string& source_code,
															 const std::string additional_options) {
	if (!has_host_device_support()) {
		return std::make_shared<host_program>(*fastest_device, host_program::program_map_type {});
	}
	
	compile_options options { .cli = additional_options };
	return add_program_source(source_code, options);
}

std::shared_ptr<device_program> host_context::add_program_source(const std::string& source_code,
															 compile_options options) {
	if (!has_host_device_support()) {
		return std::make_shared<host_program>(*fastest_device, host_program::program_map_type {});
	}
	
	// compile the source code for all devices in the context
	host_program::program_map_type prog_map;
	options.target = toolchain::TARGET::HOST_COMPUTE_CPU;
	for(const auto& dev : devices) {
		const auto host_dev = (const host_device*)dev.get();
		prog_map.insert_or_assign(host_dev,
								  create_host_program(*host_dev, toolchain::compile_program(*dev, source_code, options)));
	}
	return add_program(std::move(prog_map));
}

host_program::host_program_entry host_context::create_host_program(const host_device& dev,
																   toolchain::program_data program) {
	if(!program.valid) {
		return {};
	}
	return create_host_program_internal(dev, program.data_or_filename.data(), nullptr, 0,
										program.function_info, program.options.silence_debug_output);
}

host_program::host_program_entry host_context::create_host_program_internal(const host_device& dev floor_unused /* TODO: use device */,
																			const std::optional<std::string> elf_bin_file_name,
																			const uint8_t* elf_bin_data,
																			const size_t elf_bin_size,
																			const std::vector<toolchain::function_info>& function_info,
																			const bool& silence_debug_output) {
	host_program::host_program_entry ret;
	ret.functions = function_info;
	
	std::unique_ptr<elf_binary> bin;
	if (elf_bin_file_name && !elf_bin_file_name->empty()) {
		bin = std::make_unique<elf_binary>(*elf_bin_file_name);
	} else if (elf_bin_data != nullptr && elf_bin_size > 0) {
		bin = std::make_unique<elf_binary>(elf_bin_data, elf_bin_size);
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
	floor_return_no_nrvo(ret);
}

std::shared_ptr<device_program> host_context::add_precompiled_program_file(const std::string& file_name floor_unused,
																	   const std::vector<toolchain::function_info>& functions floor_unused) {
	log_error("not supported by Host-Compute!");
	return {};
}

std::shared_ptr<device_program::program_entry> host_context::create_program_entry(const device& dev floor_unused,
																			  toolchain::program_data program,
																			  const toolchain::TARGET) {
	return std::make_shared<device_program::program_entry>(device_program::program_entry { {}, program.function_info, true });
}

bool host_context::has_host_device_support() const {
	return floor::get_host_run_on_device();
}

device_context::memory_usage_t host_context::get_memory_usage(const device& dev) const {
	const auto& host_dev = (const host_device&)dev;
	
	const auto total_mem = host_dev.global_mem_size;
	const auto free_mem = core::get_free_system_memory();
	
	memory_usage_t ret {
		.global_mem_used = (free_mem <= total_mem ? total_mem - free_mem : total_mem),
		.global_mem_total = total_mem,
		.heap_used = 0u,
		.heap_total = 0u,
	};
	return ret;
}

} // namespace fl

#endif
