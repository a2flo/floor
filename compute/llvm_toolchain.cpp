/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2018 Florian Ziesche
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

#include <floor/compute/llvm_toolchain.hpp>
#include <floor/floor/floor.hpp>
#include <regex>
#include <climits>

#include <floor/compute/opencl/opencl_device.hpp>
#include <floor/compute/cuda/cuda_device.hpp>
#include <floor/compute/metal/metal_device.hpp>
#include <floor/compute/vulkan/vulkan_device.hpp>

#if defined(__APPLE__)
#include <floor/darwin/darwin_helper.hpp>
#endif

bool llvm_toolchain::create_floor_function_info(const string& ffi_file_name,
												vector<llvm_toolchain::function_info>& functions,
												const uint32_t toolchain_version floor_unused) {
	string ffi = "";
	if(!file_io::file_to_string(ffi_file_name, ffi)) {
		log_error("failed to retrieve floor function info from \"%s\"", ffi_file_name);
		return false;
	}
	
	const auto lines = core::tokenize(ffi, '\n');
	functions.reserve(max(lines.size(), size_t(1)) - 1);
	for(const auto& line : lines) {
		if(line.empty()) continue;
		const auto tokens = core::tokenize(line, ',');
		
		// at least 7 w/o any args:
		// <version>,<func_name>,<type>,<local_size_x>,<local_size_y>,<local_size_z>,<args...>
		if(tokens.size() < 7) {
			log_error("invalid function info entry: %s", line);
			return false;
		}
		
		static constexpr const char floor_functions_version[] { "3" };
		if(tokens[0] != floor_functions_version) {
			log_error("invalid floor function info version, expected %u, got %u!",
					  floor_functions_version, tokens[0]);
			return false;
		}
		
		function_info::FUNCTION_TYPE func_type = function_info::FUNCTION_TYPE::NONE;
		if(tokens[2] == "1") func_type = function_info::FUNCTION_TYPE::KERNEL;
		else if(tokens[2] == "2") func_type = function_info::FUNCTION_TYPE::VERTEX;
		else if(tokens[2] == "3") func_type = function_info::FUNCTION_TYPE::FRAGMENT;
		else if(tokens[2] == "4") func_type = function_info::FUNCTION_TYPE::GEOMETRY;
		else if(tokens[2] == "5") func_type = function_info::FUNCTION_TYPE::TESSELLATION_CONTROL;
		else if(tokens[2] == "6") func_type = function_info::FUNCTION_TYPE::TESSELLATION_EVALUATION;
		
		if(func_type != function_info::FUNCTION_TYPE::KERNEL &&
		   func_type != function_info::FUNCTION_TYPE::VERTEX &&
		   func_type != function_info::FUNCTION_TYPE::FRAGMENT) {
			log_error("unsupported function type: %s", tokens[2]);
			return false;
		}
		
		function_info info {
			.name = tokens[1],
			.type = func_type,
			.local_size = {
				(uint32_t)strtoull(tokens[3].c_str(), nullptr, 10),
				(uint32_t)strtoull(tokens[4].c_str(), nullptr, 10),
				(uint32_t)strtoull(tokens[5].c_str(), nullptr, 10),
			}
		};
		
		for(size_t i = 6, count = tokens.size(); i < count; ++i) {
			if(tokens[i].empty()) continue;
			// function arg info: #elem_idx size, address space, image type, image access
			const auto data = strtoull(tokens[i].c_str(), nullptr, 10);
			
			if(data == ULLONG_MAX || data == 0) {
				log_error("invalid arg info (in %s): %s", ffi_file_name, tokens[i]);
			}
			
			info.args.emplace_back(llvm_toolchain::function_info::arg_info {
				.size			= (uint32_t)
				((data & uint64_t(llvm_toolchain::FLOOR_METADATA::ARG_SIZE_MASK)) >>
				 uint64_t(llvm_toolchain::FLOOR_METADATA::ARG_SIZE_SHIFT)),
				.address_space	= (llvm_toolchain::function_info::ARG_ADDRESS_SPACE)
				((data & uint64_t(llvm_toolchain::FLOOR_METADATA::ADDRESS_SPACE_MASK)) >>
				 uint64_t(llvm_toolchain::FLOOR_METADATA::ADDRESS_SPACE_SHIFT)),
				.image_type		= (llvm_toolchain::function_info::ARG_IMAGE_TYPE)
				((data & uint64_t(llvm_toolchain::FLOOR_METADATA::IMAGE_TYPE_MASK)) >>
				 uint64_t(llvm_toolchain::FLOOR_METADATA::IMAGE_TYPE_SHIFT)),
				.image_access	= (llvm_toolchain::function_info::ARG_IMAGE_ACCESS)
				((data & uint64_t(llvm_toolchain::FLOOR_METADATA::IMAGE_ACCESS_MASK)) >>
				 uint64_t(llvm_toolchain::FLOOR_METADATA::IMAGE_ACCESS_SHIFT)),
				.special_type	= (llvm_toolchain::function_info::SPECIAL_TYPE)
				((data & uint64_t(llvm_toolchain::FLOOR_METADATA::SPECIAL_TYPE_MASK)) >>
				 uint64_t(llvm_toolchain::FLOOR_METADATA::SPECIAL_TYPE_SHIFT)),
			});
		}
		
		functions.emplace_back(info);
	}

	return true;
}

llvm_toolchain::program_data llvm_toolchain::compile_program(shared_ptr<compute_device> device,
															 const string& code,
															 const compile_options options) {
	const string printable_code { "printf \"" + core::str_hex_escape(code) + "\" | " };
	return compile_input("-", printable_code, device, options);
}

llvm_toolchain::program_data llvm_toolchain::compile_program_file(shared_ptr<compute_device> device,
																  const string& filename,
																  const compile_options options) {
	return compile_input("\"" + filename + "\"", "", device, options);
}

llvm_toolchain::program_data llvm_toolchain::compile_input(const string& input,
														   const string& cmd_prefix,
														   shared_ptr<compute_device> device,
														   const compile_options options) {
	// create the initial clang compilation command
	string clang_cmd = cmd_prefix;
	string libcxx_path = " -isystem \"", clang_path = " -isystem \"", floor_path = " -isystem \"";
	string sm_version = "20"; // handle cuda sm version (default to fermi/sm_20)
	uint32_t ptx_version = max(43u, options.cuda.ptx_version); // handle cuda ptx version (default to at least ptx 4.3)
	uint32_t bitness = device->bitness; // can be overwritten by target
	string output_file_type = "bc"; // can be overwritten by target
	uint32_t toolchain_version = 0;
	switch(options.target) {
		case TARGET::SPIR:
			toolchain_version = floor::get_opencl_toolchain_version();
			clang_cmd += {
				"\"" + floor::get_opencl_compiler() + "\"" +
				" -x cl -Xclang -cl-std=CL1.2" \
				" -target " + (bitness == 32 ? "spir-unknown-unknown" : "spir64-unknown-unknown") +
				" -llvm-bc-32" \
				" -Xclang -cl-sampler-type -Xclang i32" \
				" -Xclang -cl-kernel-arg-info" \
				" -Xclang -cl-mad-enable" \
				" -Xclang -cl-fast-relaxed-math" \
				" -Xclang -cl-unsafe-math-optimizations" \
				" -Xclang -cl-finite-math-only" \
				" -DFLOOR_COMPUTE_OPENCL" \
				" -DFLOOR_COMPUTE_SPIR" \
				" -DFLOOR_COMPUTE_OPENCL_MAJOR=1" \
				" -DFLOOR_COMPUTE_OPENCL_MINOR=2" +
				(!device->double_support ? " -DFLOOR_COMPUTE_NO_DOUBLE" : "") +
				(floor::get_opencl_verify_spir() ? " -Xclang -cl-verify-spir" : "") +
				(device->platform_vendor == COMPUTE_VENDOR::INTEL &&
				 device->vendor == COMPUTE_VENDOR::INTEL ? " -Xclang -cl-spir-intel-workarounds" : "")
			};
			libcxx_path += floor::get_opencl_base_path() + "libcxx";
			clang_path += floor::get_opencl_base_path() + "clang";
			floor_path += floor::get_opencl_base_path() + "floor";
			break;
		case TARGET::AIR: {
			toolchain_version = floor::get_metal_toolchain_version();
			output_file_type = "metallib";
			
			const auto mtl_dev = (metal_device*)device.get();
			auto metal_version = mtl_dev->metal_version;
			const auto metal_force_version = floor::get_metal_force_version();
			if (metal_force_version != 0) {
				switch (metal_force_version) {
					case 11:
						metal_version = METAL_VERSION::METAL_1_1;
						break;
					case 12:
						metal_version = METAL_VERSION::METAL_1_2;
						break;
					case 20:
						metal_version = METAL_VERSION::METAL_2_0;
						break;
					default:
						log_error("invalid force_version: %u", metal_force_version);
						break;
				}
			}
			
			string os_target;
			if(mtl_dev->family < 10000) {
				// -> iOS 9.0+
				switch(metal_version) {
					default:
						os_target = "ios9.0.0";
						break;
					case METAL_VERSION::METAL_1_2:
						os_target = "ios10.0.0";
						break;
					case METAL_VERSION::METAL_2_0:
						os_target = "ios11.0.0";
						break;
				}
			}
			else {
				// -> OS X 10.11+
				switch(metal_version) {
					default:
						os_target = "macosx10.11.0";
						break;
					case METAL_VERSION::METAL_1_2:
						os_target = "macosx10.12.0";
						break;
					case METAL_VERSION::METAL_2_0:
						os_target = "macosx10.13.0";
						break;
				}
			}
			
			string metal_std = "metal1.1";
			switch(metal_version) {
				case METAL_VERSION::METAL_1_2:
					metal_std = "metal1.2";
					break;
				case METAL_VERSION::METAL_2_0:
					metal_std = "metal2.0";
					break;
				default: break;
			}
			
			clang_cmd += {
				"\"" + floor::get_metal_compiler() + "\"" +
				// NOTE: always compiling to 64-bit here, because 32-bit never existed
				" -x metal -std=" + metal_std + " -target air64-apple-" + os_target +
#if defined(__APPLE__)
				// always enable intel workarounds (conversion problems)
				(device->vendor == COMPUTE_VENDOR::INTEL ? " -Xclang -metal-intel-workarounds" : "") +
				// enable nvidia workarounds on osx 10.12+ (array load/store problems)
				(device->vendor == COMPUTE_VENDOR::NVIDIA && darwin_helper::get_system_version() >= 101200 ?
				 " -Xclang -metal-nvidia-workarounds" : "") +
#endif
				" -Xclang -cl-mad-enable" \
				" -Xclang -cl-fast-relaxed-math" \
				" -Xclang -cl-unsafe-math-optimizations" \
				" -Xclang -cl-finite-math-only" \
				" -DFLOOR_COMPUTE_NO_DOUBLE" \
				" -DFLOOR_COMPUTE_METAL" \
				" -llvm-metallib" +
				" -DFLOOR_COMPUTE_METAL_MAJOR=" + metal_major_version_to_string(metal_version) +
				" -DFLOOR_COMPUTE_METAL_MINOR=" + metal_minor_version_to_string(metal_version)
			};
			libcxx_path += floor::get_metal_base_path() + "libcxx";
			clang_path += floor::get_metal_base_path() + "clang";
			floor_path += floor::get_metal_base_path() + "floor";
		} break;
		case TARGET::PTX: {
			// handle sm version
			const auto& force_sm = floor::get_cuda_force_compile_sm();
#if !defined(FLOOR_NO_CUDA)
			const auto& sm = ((cuda_device*)device.get())->sm;
			sm_version = (force_sm.empty() ? to_string(sm.x * 10 + sm.y) : force_sm);
#else
			if(!force_sm.empty()) sm_version = force_sm;
#endif

			// handle ptx version (note that 4.3 is the minimum requirement for floor, 5.0 for sm_6x, 6.0 for sm_70+)
			switch(((cuda_device*)device.get())->sm.x) {
				case 2:
				case 3:
				case 5:
					// already 43
					break;
				case 6:
					ptx_version = max(50u, ptx_version);
					break;
				case 7:
				default:
					ptx_version = max(60u, ptx_version);
					break;
			}
			if(!floor::get_cuda_force_ptx().empty()) {
				const auto forced_version = stou(floor::get_cuda_force_ptx());
				if(forced_version >= 43 && forced_version < numeric_limits<uint32_t>::max()) {
					ptx_version = forced_version;
				}
			}
			
			toolchain_version = floor::get_cuda_toolchain_version();
			clang_cmd += {
				"\"" + floor::get_cuda_compiler() + "\"" +
				" -x cuda -std=cuda" \
				" -target " + (bitness == 32 ? "i386--" : "x86_64--") +
				" -nocudalib -nocudainc --cuda-device-only --cuda-gpu-arch=sm_" + sm_version +
				" -Xclang -fcuda-is-device" \
				" -DFLOOR_COMPUTE_CUDA"
			};
			libcxx_path += floor::get_cuda_base_path() + "libcxx";
			clang_path += floor::get_cuda_base_path() + "clang";
			floor_path += floor::get_cuda_base_path() + "floor";
		} break;
		case TARGET::SPIRV_VULKAN:
			toolchain_version = floor::get_vulkan_toolchain_version();
			bitness = 32; // always 32-bit for now
			output_file_type = "spvc";
			
			// still compiling this as opencl for now
			clang_cmd += {
				"\"" + floor::get_vulkan_compiler() + "\"" +
				" -x vulkan -std=vulkan1.0" \
				" -llvm-spirv-container" \
				" -target spir-unknown-unknown-vulkan" +
				" -Xclang -cl-sampler-type -Xclang i32" \
				" -Xclang -cl-kernel-arg-info" \
				" -Xclang -cl-mad-enable" \
				" -Xclang -cl-fast-relaxed-math" \
				" -Xclang -cl-unsafe-math-optimizations" \
				" -Xclang -cl-finite-math-only" \
				" -DFLOOR_COMPUTE_VULKAN" \
				" -DFLOOR_COMPUTE_SPIRV" \
				" -DFLOOR_COMPUTE_NO_DOUBLE"
				// TODO: fix Vulkan double support
				//(!device->double_support ? " -DFLOOR_COMPUTE_NO_DOUBLE" : "")
			};
			libcxx_path += floor::get_vulkan_base_path() + "libcxx";
			clang_path += floor::get_vulkan_base_path() + "clang";
			floor_path += floor::get_vulkan_base_path() + "floor";
			break;
		case TARGET::SPIRV_OPENCL:
			toolchain_version = floor::get_opencl_toolchain_version();
			output_file_type = "spv";
			const auto cl_device = (const opencl_device*)device.get();
			if(cl_device->spirv_version == SPIRV_VERSION::NONE) {
				log_error("SPIR-V is not supported by this device!");
				return {};
			}
			
			clang_cmd += {
				"\"" + floor::get_opencl_compiler() + "\"" +
				// compile to the max opencl standard that is supported by the device
				" -x cl -Xclang -cl-std=CL" + cl_version_to_string(cl_device->cl_version) +
				" -target " + (bitness == 32 ? "spir-unknown-unknown" : "spir64-unknown-unknown") +
				" -llvm-spirv" \
				" -Xclang -cl-sampler-type -Xclang i32" \
				" -Xclang -cl-kernel-arg-info" \
				" -Xclang -cl-mad-enable" \
				" -Xclang -cl-fast-relaxed-math" \
				" -Xclang -cl-unsafe-math-optimizations" \
				" -Xclang -cl-finite-math-only" \
				" -DFLOOR_COMPUTE_OPENCL" \
				" -DFLOOR_COMPUTE_SPIRV" \
				" -DFLOOR_COMPUTE_OPENCL_MAJOR=" + cl_major_version_to_string(cl_device->cl_version) +
				" -DFLOOR_COMPUTE_OPENCL_MINOR=" + cl_minor_version_to_string(cl_device->cl_version) +
				(!device->double_support ? " -DFLOOR_COMPUTE_NO_DOUBLE" : "")
			};
			libcxx_path += floor::get_opencl_base_path() + "libcxx";
			clang_path += floor::get_opencl_base_path() + "clang";
			floor_path += floor::get_opencl_base_path() + "floor";
			break;
	}
	libcxx_path += "\"";
	clang_path += "\"";
	floor_path += "\"";
	
	// set toolchain version define
	clang_cmd += " -DFLOOR_TOOLCHAIN_VERSION=" + to_string(toolchain_version) + "u";
	
	// add device information
	// -> this adds both a "=" value definiton (that is used for enums in device_info.hpp) and a non-valued "_" defintion (used for #ifdef's)
	const auto vendor_str = compute_vendor_to_string(device->vendor);
	const auto platform_vendor_str = compute_vendor_to_string(device->platform_vendor);
	const auto type_str = (compute_device::has_flag<compute_device::TYPE::GPU>(device->type) ? "GPU" :
						   compute_device::has_flag<compute_device::TYPE::CPU>(device->type) ? "CPU" : "UNKNOWN");
	const auto os_str = (options.target != TARGET::AIR ?
						 // non metal/air targets (aka the usual/proper handling)
#if defined(__APPLE__)
#if defined(FLOOR_IOS)
						 "IOS"
#else
						 "OSX"
#endif
#elif defined(__WINDOWS__)
						 "WINDOWS"
#elif defined(__LINUX__)
						 "LINUX"
#elif defined(__FreeBSD__)
						 "FREEBSD"
#elif defined(__OpenBSD__)
						 "OPENBSD"
#else
						 "UNKNOWN"
#endif
						 // metal/air specific handling, target os is dependent on device family
						 : (((metal_device*)device.get())->family < 10000 ? "IOS" : "OSX"));
	
	clang_cmd += " -DFLOOR_COMPUTE_INFO_VENDOR="s + vendor_str;
	clang_cmd += " -DFLOOR_COMPUTE_INFO_VENDOR_"s + vendor_str;
	clang_cmd += " -DFLOOR_COMPUTE_INFO_PLATFORM_VENDOR="s + platform_vendor_str;
	clang_cmd += " -DFLOOR_COMPUTE_INFO_PLATFORM_VENDOR_"s + platform_vendor_str;
	clang_cmd += " -DFLOOR_COMPUTE_INFO_TYPE="s + type_str;
	clang_cmd += " -DFLOOR_COMPUTE_INFO_TYPE_"s + type_str;
	clang_cmd += " -DFLOOR_COMPUTE_INFO_OS="s + os_str;
	clang_cmd += " -DFLOOR_COMPUTE_INFO_OS_"s + os_str;
	
#if defined(__APPLE__)
	const auto os_version_str = to_string(darwin_helper::get_system_version());
	clang_cmd += " -DFLOOR_COMPUTE_INFO_OS_VERSION="s + os_version_str;
	clang_cmd += " -DFLOOR_COMPUTE_INFO_OS_VERSION_"s + os_version_str;
#else
	// TODO: do this on windows and linux as well? if so, how/what?
	clang_cmd += " -DFLOOR_COMPUTE_INFO_OS_VERSION=0";
	clang_cmd += " -DFLOOR_COMPUTE_INFO_OS_VERSION_0";
#endif
	
	// assume all gpus have fma support
	bool has_fma = compute_device::has_flag<compute_device::TYPE::GPU>(device->type);
	if(compute_device::has_flag<compute_device::TYPE::CPU>(device->type)) {
		// if device is a cpu, need to check cpuid on x86, or check if cpu is armv8
		has_fma = core::cpu_has_fma();
	}
	const auto has_fma_str = to_string(has_fma);
	clang_cmd += " -DFLOOR_COMPUTE_INFO_HAS_FMA="s + has_fma_str;
	clang_cmd += " -DFLOOR_COMPUTE_INFO_HAS_FMA_"s + has_fma_str;
	
	// base and extended 64-bit atomics support
	const auto has_base_64_bit_atomics_str = to_string(device->basic_64_bit_atomics_support);
	const auto has_extended_64_bit_atomics_str = to_string(device->extended_64_bit_atomics_support);
	clang_cmd += " -DFLOOR_COMPUTE_INFO_HAS_64_BIT_ATOMICS="s + has_base_64_bit_atomics_str;
	clang_cmd += " -DFLOOR_COMPUTE_INFO_HAS_64_BIT_ATOMICS_"s + has_base_64_bit_atomics_str;
	clang_cmd += " -DFLOOR_COMPUTE_INFO_HAS_NATIVE_EXTENDED_64_BIT_ATOMICS="s + has_extended_64_bit_atomics_str;
	clang_cmd += " -DFLOOR_COMPUTE_INFO_HAS_NATIVE_EXTENDED_64_BIT_ATOMICS_"s + has_extended_64_bit_atomics_str;
	
	// has device actual dedicated local memory
	const auto has_dedicated_local_memory_str = to_string(device->local_mem_dedicated);
	clang_cmd += " -DFLOOR_COMPUTE_INFO_HAS_DEDICATED_LOCAL_MEMORY="s + has_dedicated_local_memory_str;
	clang_cmd += " -DFLOOR_COMPUTE_INFO_HAS_DEDICATED_LOCAL_MEMORY_"s + has_dedicated_local_memory_str;
	
	// id/size ranges
	uint2 global_id_range { 0u, 0xFFFFFFFFu };
	uint2 global_size_range { 1u, 0xFFFFFFFFu };
	// NOTE: nobody supports a local size > 2048 (or 1536) right now, if that changes, this needs to be updated
	uint2 local_id_range { 0u, 2048u };
	uint2 local_size_range { 1u, 2048u };
	uint2 group_id_range { 0u, 0xFFFFFFFFu };
	uint2 group_size_range { 1u, 0xFFFFFFFFu };
	
	// if the device has actual info about this, use that instead of the defaults
	const auto max_global_size = device->max_global_size.max_element();
	if(max_global_size > 0) {
		const uint32_t max_global_size_u32 = (max_global_size > 0xFFFFFFFFull ? 0xFFFFFFFFu : uint32_t(max_global_size));
		global_id_range.y = max_global_size_u32;
		global_size_range.y = max_global_size_u32;
	}
	
	if(device->max_total_local_size != 0) {
		local_id_range.y = device->max_total_local_size;
		local_size_range.y = device->max_total_local_size;
	}
	
	const auto max_group_size = device->max_group_size.max_element();
	if(max_group_size > 0) {
		group_id_range.y = max_group_size;
		group_size_range.y = max_group_size;
	}
	
	clang_cmd += " -DFLOOR_COMPUTE_INFO_GLOBAL_ID_RANGE_MIN=" + to_string(global_id_range.x) + "u";
	clang_cmd += " -DFLOOR_COMPUTE_INFO_GLOBAL_ID_RANGE_MAX=" + to_string(global_id_range.y) + "u";
	clang_cmd += " -DFLOOR_COMPUTE_INFO_GLOBAL_SIZE_RANGE_MIN=" + to_string(global_size_range.x) + "u";
	clang_cmd += " -DFLOOR_COMPUTE_INFO_GLOBAL_SIZE_RANGE_MAX=" + to_string(global_size_range.y) + "u";
	clang_cmd += " -DFLOOR_COMPUTE_INFO_LOCAL_ID_RANGE_MIN=" + to_string(local_id_range.x) + "u";
	clang_cmd += " -DFLOOR_COMPUTE_INFO_LOCAL_ID_RANGE_MAX=" + to_string(local_id_range.y) + "u";
	clang_cmd += " -DFLOOR_COMPUTE_INFO_LOCAL_SIZE_RANGE_MIN=" + to_string(local_size_range.x) + "u";
	clang_cmd += " -DFLOOR_COMPUTE_INFO_LOCAL_SIZE_RANGE_MAX=" + to_string(local_size_range.y) + "u";
	clang_cmd += " -DFLOOR_COMPUTE_INFO_GROUP_ID_RANGE_MIN=" + to_string(group_id_range.x) + "u";
	clang_cmd += " -DFLOOR_COMPUTE_INFO_GROUP_ID_RANGE_MAX=" + to_string(group_id_range.y) + "u";
	clang_cmd += " -DFLOOR_COMPUTE_INFO_GROUP_SIZE_RANGE_MIN=" + to_string(group_size_range.x) + "u";
	clang_cmd += " -DFLOOR_COMPUTE_INFO_GROUP_SIZE_RANGE_MAX=" + to_string(group_size_range.y) + "u";
	
	// handle device simd width
	uint32_t simd_width = device->simd_width;
	uint2 simd_range = device->simd_range;
	if(simd_width == 0) {
		// try to figure out the simd width of the device if it's 0
		if(compute_device::has_flag<compute_device::TYPE::GPU>(device->type)) {
			switch(device->vendor) {
				case COMPUTE_VENDOR::NVIDIA: simd_width = 32; simd_range = { simd_width, simd_width }; break;
				case COMPUTE_VENDOR::AMD: simd_width = 64; simd_range = { simd_width, simd_width }; break;
				case COMPUTE_VENDOR::INTEL: simd_width = 16; simd_range = { 8, 32 }; break;
				case COMPUTE_VENDOR::APPLE: simd_width = 32; simd_range = { simd_width, simd_width }; break;
				// else: unknown gpu
				default: break;
			}
		}
		else if(compute_device::has_flag<compute_device::TYPE::CPU>(device->type)) {
			// always at least 4 (SSE, newer NEON), 8-wide if avx/avx, 16-wide if avx-512
			simd_width = (core::cpu_has_avx() ? (core::cpu_has_avx512() ? 16 : 8) : 4);
			simd_range = { 1, simd_width };
		}
	}
	const auto simd_width_str = to_string(simd_width);
	clang_cmd += " -DFLOOR_COMPUTE_INFO_SIMD_WIDTH="s + simd_width_str + "u";
	clang_cmd += " -DFLOOR_COMPUTE_INFO_SIMD_WIDTH_MIN="s + to_string(simd_range.x) + "u";
	clang_cmd += " -DFLOOR_COMPUTE_INFO_SIMD_WIDTH_MAX="s + to_string(simd_range.y) + "u";
	clang_cmd += " -DFLOOR_COMPUTE_INFO_SIMD_WIDTH_"s + simd_width_str;
	
	// handle sub-group support
	if(device->sub_group_support) {
		clang_cmd += " -DFLOOR_COMPUTE_INFO_HAS_SUB_GROUPS=1 -DFLOOR_COMPUTE_INFO_HAS_SUB_GROUPS_1";
	} else {
		clang_cmd += " -DFLOOR_COMPUTE_INFO_HAS_SUB_GROUPS=0 -DFLOOR_COMPUTE_INFO_HAS_SUB_GROUPS_0";
	}
	
	// handle sub-group shuffle support
	if(device->sub_group_shuffle_support) {
		clang_cmd += " -DFLOOR_COMPUTE_INFO_HAS_SUB_GROUP_SHUFFLE=1 -DFLOOR_COMPUTE_INFO_HAS_SUB_GROUP_SHUFFLE_1";
	} else {
		clang_cmd += " -DFLOOR_COMPUTE_INFO_HAS_SUB_GROUP_SHUFFLE=0 -DFLOOR_COMPUTE_INFO_HAS_SUB_GROUP_SHUFFLE_0";
	}
	
	// handle cooperative kernel support
	if(device->cooperative_kernel_support) {
		clang_cmd += " -DFLOOR_COMPUTE_INFO_HAS_COOPERATIVE_KERNEL=1 -DFLOOR_COMPUTE_INFO_HAS_COOPERATIVE_KERNEL_1";
	} else {
		clang_cmd += " -DFLOOR_COMPUTE_INFO_HAS_COOPERATIVE_KERNEL=0 -DFLOOR_COMPUTE_INFO_HAS_COOPERATIVE_KERNEL_0";
	}
	
	// handle image support
	const auto has_image_support = to_string(device->image_support);
	const auto has_image_depth_support = to_string(device->image_depth_support);
	const auto has_image_depth_write_support = to_string(device->image_depth_write_support);
	const auto has_image_msaa_support = to_string(device->image_msaa_support);
	const auto has_image_msaa_write_support = to_string(device->image_msaa_write_support);
	const auto has_image_msaa_array_support = to_string(device->image_msaa_array_support);
	const auto has_image_msaa_array_write_support = to_string(device->image_msaa_array_write_support);
	const auto has_image_cube_support = to_string(device->image_cube_support);
	const auto has_image_cube_write_support = to_string(device->image_cube_write_support);
	const auto has_image_cube_array_support = to_string(device->image_cube_array_support);
	const auto has_image_cube_array_write_support = to_string(device->image_cube_array_write_support);
	const auto has_image_mipmap_support = to_string(device->image_mipmap_support);
	const auto has_image_mipmap_write_support = to_string(device->image_mipmap_write_support);
	const auto has_image_offset_read_support = to_string(device->image_offset_read_support);
	const auto has_image_offset_write_support = to_string(device->image_offset_write_support);
	const auto has_image_depth_compare_support = to_string(device->image_depth_compare_support);
	const auto has_image_gather_support = to_string(device->image_gather_support);
	const auto has_image_read_write_support = to_string(device->image_read_write_support);
	clang_cmd += " -DFLOOR_COMPUTE_INFO_HAS_IMAGE_SUPPORT="s + has_image_support;
	clang_cmd += " -DFLOOR_COMPUTE_INFO_HAS_IMAGE_SUPPORT_"s + has_image_support;
	clang_cmd += " -DFLOOR_COMPUTE_INFO_HAS_IMAGE_DEPTH_SUPPORT="s + has_image_depth_support;
	clang_cmd += " -DFLOOR_COMPUTE_INFO_HAS_IMAGE_DEPTH_SUPPORT_"s + has_image_depth_support;
	clang_cmd += " -DFLOOR_COMPUTE_INFO_HAS_IMAGE_DEPTH_WRITE_SUPPORT="s + has_image_depth_write_support;
	clang_cmd += " -DFLOOR_COMPUTE_INFO_HAS_IMAGE_DEPTH_WRITE_SUPPORT_"s + has_image_depth_write_support;
	clang_cmd += " -DFLOOR_COMPUTE_INFO_HAS_IMAGE_MSAA_SUPPORT="s + has_image_msaa_support;
	clang_cmd += " -DFLOOR_COMPUTE_INFO_HAS_IMAGE_MSAA_SUPPORT_"s + has_image_msaa_support;
	clang_cmd += " -DFLOOR_COMPUTE_INFO_HAS_IMAGE_MSAA_WRITE_SUPPORT="s + has_image_msaa_write_support;
	clang_cmd += " -DFLOOR_COMPUTE_INFO_HAS_IMAGE_MSAA_WRITE_SUPPORT_"s + has_image_msaa_write_support;
	clang_cmd += " -DFLOOR_COMPUTE_INFO_HAS_IMAGE_MSAA_ARRAY_SUPPORT="s + has_image_msaa_array_support;
	clang_cmd += " -DFLOOR_COMPUTE_INFO_HAS_IMAGE_MSAA_ARRAY_SUPPORT_"s + has_image_msaa_array_support;
	clang_cmd += " -DFLOOR_COMPUTE_INFO_HAS_IMAGE_MSAA_ARRAY_WRITE_SUPPORT="s + has_image_msaa_array_write_support;
	clang_cmd += " -DFLOOR_COMPUTE_INFO_HAS_IMAGE_MSAA_ARRAY_WRITE_SUPPORT_"s + has_image_msaa_array_write_support;
	clang_cmd += " -DFLOOR_COMPUTE_INFO_HAS_IMAGE_CUBE_SUPPORT="s + has_image_cube_support;
	clang_cmd += " -DFLOOR_COMPUTE_INFO_HAS_IMAGE_CUBE_SUPPORT_"s + has_image_cube_support;
	clang_cmd += " -DFLOOR_COMPUTE_INFO_HAS_IMAGE_CUBE_WRITE_SUPPORT="s + has_image_cube_write_support;
	clang_cmd += " -DFLOOR_COMPUTE_INFO_HAS_IMAGE_CUBE_WRITE_SUPPORT_"s + has_image_cube_write_support;
	clang_cmd += " -DFLOOR_COMPUTE_INFO_HAS_IMAGE_CUBE_ARRAY_SUPPORT="s + has_image_cube_array_support;
	clang_cmd += " -DFLOOR_COMPUTE_INFO_HAS_IMAGE_CUBE_ARRAY_SUPPORT_"s + has_image_cube_array_support;
	clang_cmd += " -DFLOOR_COMPUTE_INFO_HAS_IMAGE_CUBE_ARRAY_WRITE_SUPPORT="s + has_image_cube_array_write_support;
	clang_cmd += " -DFLOOR_COMPUTE_INFO_HAS_IMAGE_CUBE_ARRAY_WRITE_SUPPORT_"s + has_image_cube_array_write_support;
	clang_cmd += " -DFLOOR_COMPUTE_INFO_HAS_IMAGE_MIPMAP_SUPPORT="s + has_image_mipmap_support;
	clang_cmd += " -DFLOOR_COMPUTE_INFO_HAS_IMAGE_MIPMAP_SUPPORT_"s + has_image_mipmap_support;
	clang_cmd += " -DFLOOR_COMPUTE_INFO_HAS_IMAGE_MIPMAP_WRITE_SUPPORT="s + has_image_mipmap_write_support;
	clang_cmd += " -DFLOOR_COMPUTE_INFO_HAS_IMAGE_MIPMAP_WRITE_SUPPORT_"s + has_image_mipmap_write_support;
	clang_cmd += " -DFLOOR_COMPUTE_INFO_HAS_IMAGE_OFFSET_READ_SUPPORT="s + has_image_mipmap_support;
	clang_cmd += " -DFLOOR_COMPUTE_INFO_HAS_IMAGE_OFFSET_READ_SUPPORT_"s + has_image_mipmap_support;
	clang_cmd += " -DFLOOR_COMPUTE_INFO_HAS_IMAGE_OFFSET_WRITE_SUPPORT="s + has_image_mipmap_write_support;
	clang_cmd += " -DFLOOR_COMPUTE_INFO_HAS_IMAGE_OFFSET_WRITE_SUPPORT_"s + has_image_mipmap_write_support;
	clang_cmd += " -DFLOOR_COMPUTE_INFO_HAS_IMAGE_DEPTH_COMPARE_SUPPORT="s + has_image_depth_compare_support;
	clang_cmd += " -DFLOOR_COMPUTE_INFO_HAS_IMAGE_DEPTH_COMPARE_SUPPORT_"s + has_image_depth_compare_support;
	clang_cmd += " -DFLOOR_COMPUTE_INFO_HAS_IMAGE_GATHER_SUPPORT="s + has_image_gather_support;
	clang_cmd += " -DFLOOR_COMPUTE_INFO_HAS_IMAGE_GATHER_SUPPORT_"s + has_image_gather_support;
	clang_cmd += " -DFLOOR_COMPUTE_INFO_HAS_IMAGE_READ_WRITE_SUPPORT="s + has_image_read_write_support;
	clang_cmd += " -DFLOOR_COMPUTE_INFO_HAS_IMAGE_READ_WRITE_SUPPORT_"s + has_image_read_write_support;
	
	IMAGE_CAPABILITY img_caps { IMAGE_CAPABILITY::NONE };
	if(device->image_support) img_caps |= IMAGE_CAPABILITY::BASIC;
	if(device->image_depth_support) img_caps |= IMAGE_CAPABILITY::DEPTH_READ;
	if(device->image_depth_write_support) img_caps |= IMAGE_CAPABILITY::DEPTH_WRITE;
	if(device->image_msaa_support) img_caps |= IMAGE_CAPABILITY::MSAA_READ;
	if(device->image_msaa_write_support) img_caps |= IMAGE_CAPABILITY::MSAA_WRITE;
	if(device->image_msaa_array_support) img_caps |= IMAGE_CAPABILITY::MSAA_ARRAY_READ;
	if(device->image_msaa_array_write_support) img_caps |= IMAGE_CAPABILITY::MSAA_ARRAY_WRITE;
	if(device->image_cube_support) img_caps |= IMAGE_CAPABILITY::CUBE_READ;
	if(device->image_cube_write_support) img_caps |= IMAGE_CAPABILITY::CUBE_WRITE;
	if(device->image_cube_array_support) img_caps |= IMAGE_CAPABILITY::CUBE_ARRAY_READ;
	if(device->image_cube_array_write_support) img_caps |= IMAGE_CAPABILITY::CUBE_ARRAY_WRITE;
	if(device->image_mipmap_support) img_caps |= IMAGE_CAPABILITY::MIPMAP_READ;
	if(device->image_mipmap_write_support) img_caps |= IMAGE_CAPABILITY::MIPMAP_WRITE;
	if(device->image_offset_read_support) img_caps |= IMAGE_CAPABILITY::OFFSET_READ;
	if(device->image_offset_write_support) img_caps |= IMAGE_CAPABILITY::OFFSET_WRITE;
	if(device->image_depth_compare_support) img_caps |= IMAGE_CAPABILITY::DEPTH_COMPARE;
	if(device->image_gather_support) img_caps |= IMAGE_CAPABILITY::GATHER;
	if(device->image_read_write_support) img_caps |= IMAGE_CAPABILITY::READ_WRITE;
	clang_cmd += " -Xclang -floor-image-capabilities=" + to_string((underlying_type_t<IMAGE_CAPABILITY>)img_caps);
	
	clang_cmd += " -DFLOOR_COMPUTE_INFO_MAX_MIP_LEVELS="s + to_string(device->max_mip_levels) + "u";
	
	// set param workaround define
	if(device->param_workaround) {
		clang_cmd += " -DFLOOR_COMPUTE_PARAM_WORKAROUND=1";
	}
	
	// floor function info
	const auto function_info_file_name = core::create_tmp_file_name("ffi", ".txt");
	clang_cmd += " -Xclang -floor-function-info=" + function_info_file_name;
	
	// target specific compute info
	switch(options.target) {
		case TARGET::PTX:
			// set cuda sm and ptx version
			clang_cmd += " -DFLOOR_COMPUTE_INFO_CUDA_SM=" + sm_version;
			clang_cmd += " -DFLOOR_COMPUTE_INFO_CUDA_PTX=" + to_string(ptx_version);
			break;
		case TARGET::SPIRV_VULKAN: {
			const auto vk_device = (const vulkan_device*)device.get();
			const auto has_int16_support = to_string(vk_device->int16_support);
			const auto has_int64_support = to_string(vk_device->int64_support);
			const auto has_float16_support = to_string(vk_device->float16_support);
			clang_cmd += " -DFLOOR_COMPUTE_INFO_VULKAN_HAS_INT16_SUPPORT="s + has_int16_support;
			clang_cmd += " -DFLOOR_COMPUTE_INFO_VULKAN_HAS_INT16_SUPPORT_"s + has_int16_support;
			clang_cmd += " -DFLOOR_COMPUTE_INFO_VULKAN_HAS_INT64_SUPPORT="s + has_int64_support;
			clang_cmd += " -DFLOOR_COMPUTE_INFO_VULKAN_HAS_INT64_SUPPORT_"s + has_int64_support;
			clang_cmd += " -DFLOOR_COMPUTE_INFO_VULKAN_HAS_FLOAT16_SUPPORT="s + has_float16_support;
			clang_cmd += " -DFLOOR_COMPUTE_INFO_VULKAN_HAS_FLOAT16_SUPPORT_"s + has_float16_support;

			if(!vk_device->int64_support) {
				clang_cmd += " -DFLOOR_NO_INT64_SUPPORT";
			}
			break;
		}
		default: break;
	}

	// emit line info if debug mode is enabled (unless this is spir where we'd better not emit this)
	if((floor::get_toolchain_debug() || options.emit_debug_line_info) &&
		options.target != TARGET::SPIR) {
		clang_cmd += " -gline-tables-only";
	}
	
	// default warning flags (note that these cost a significant amount of compilation time)
	const char* warning_flags {
		// let's start with everything
		" -Weverything"
		// remove compat warnings
		" -Wno-c++98-compat -Wno-c++98-compat-pedantic"
		" -Wno-c++11-compat -Wno-c++11-compat-pedantic"
		" -Wno-c++14-compat -Wno-c++14-compat-pedantic"
		" -Wno-c99-extensions -Wno-c11-extensions"
		" -Wno-gcc-compat -Wno-gnu"
		// in case we're using warning options that aren't supported by other clang versions
		" -Wno-unknown-warning-option"
		// really don't want to be too pedantic
		" -Wno-old-style-cast -Wno-date-time -Wno-system-headers -Wno-header-hygiene -Wno-documentation"
		// again: not too pedantic, also useful language features
		" -Wno-nested-anon-types -Wno-global-constructors -Wno-exit-time-destructors"
		// usually conflicting with the other switch/case warning, so disable it
		" -Wno-switch-enum"
		// don't warn when using macros prefixed with "__" or "_"
		" -Wno-reserved-id-macro"
		// end
		" "
	};
	
	// add generic flags/options that are always used
	auto compiled_file_or_code = core::create_tmp_file_name("", '.' + output_file_type);
	clang_cmd += {
#if defined(FLOOR_DEBUG)
		" -DFLOOR_DEBUG"
#endif
		" -DFLOOR_COMPUTE"
		" -DFLOOR_NO_MATH_STR" +
		clang_path + libcxx_path + floor_path +
		" -include floor/compute/device/common.hpp"
		" -fno-exceptions -fno-rtti -fno-pic -fstrict-aliasing -ffast-math -funroll-loops -Ofast -ffp-contract=fast"
		// increase limit from 16 to 64, this "fixes" some forced unrolling
		" -mllvm -rotation-max-header-size=64" +
		(options.enable_warnings ? warning_flags : " ") +
		options.cli +
		// compile to the right device bitness
		(bitness == 32 ? " -m32 -DPLATFORM_X32" : " -m64 -DPLATFORM_X64") +
		" -emit-llvm -c -o " + compiled_file_or_code + " " + input
	};
	
	// on sane systems, redirect errors to stdout so that we can grab them
#if !defined(_MSC_VER)
	clang_cmd += " 2>&1";
#endif
	
	// compile
	string compilation_output = "";
	core::system(clang_cmd, compilation_output);
	// check if the output contains an error string (yes, a bit ugly, but it works for now - can't actually check the return code)
	if(compilation_output.find(" error: ") != string::npos ||
	   compilation_output.find(" errors:") != string::npos) {
		log_error("compilation failed! failed cmd was:\n%s", clang_cmd);
		log_error("compilation errors:\n%s", compilation_output);
		return {};
	}
	// also print the output if it is non-empty
	if(compilation_output != "" &&
	   !options.silence_debug_output) {
		log_debug("compilation output:\n%s", compilation_output);
	}
	if(floor::get_toolchain_log_commands() &&
	   !options.silence_debug_output) {
		log_debug("clang cmd: %s", clang_cmd);
	}
	
	// grab floor function info and create the internal per-function info
	vector<function_info> functions;
	if(!create_floor_function_info(function_info_file_name, functions, toolchain_version)) {
		log_error("failed to create internal floor function info");
		return {};
	}
	if(!floor::get_toolchain_keep_temp()) {
		core::system("rm " + function_info_file_name);
	}
	
	// final target specific processing/compilation
	if(options.target == TARGET::SPIR) {
		string spir_bc_data = "";
		if(!file_io::file_to_string(compiled_file_or_code, spir_bc_data)) {
			log_error("failed to read SPIR 1.2 .bc file");
			return {};
		}
		
		// cleanup
		if(!floor::get_toolchain_keep_temp()) {
			core::system("rm " + compiled_file_or_code);
		}
		
		// move spir data
		compiled_file_or_code.swap(spir_bc_data);
	}
	else if(options.target == TARGET::AIR) {
		// nop, final processing will be done in metal_compute
	}
	else if(options.target == TARGET::PTX) {
		// compile llvm ir to ptx
		const string llc_cmd {
			"\"" + floor::get_cuda_llc() + "\"" +
			" -nvptx-fma-level=2 -nvptx-sched4reg -enable-unsafe-fp-math" \
			" -mcpu=sm_" + sm_version + " -mattr=ptx" + to_string(ptx_version) +
			" -o - " + compiled_file_or_code
#if !defined(_MSC_VER)
			+ " 2>&1"
#endif
		};
		if(floor::get_toolchain_log_commands() &&
		   !options.silence_debug_output) {
			log_debug("llc cmd: %s", llc_cmd);
		}
		string ptx_code = "";
		core::system(llc_cmd, ptx_code);
		
		// only output the compiled ptx code if this was specified in the config
		// NOTE: explicitly create this in the working directory (not in tmp)
		if(floor::get_toolchain_keep_temp()) {
			file_io::string_to_file("cuda.ptx", ptx_code);
		}
		
		// check if we have sane output
		if(ptx_code == "" || ptx_code.find("Generated by LLVM NVPTX Back-End") == string::npos) {
			log_error("llc/ptx compilation failed!\n%s", ptx_code);
			return {};
		}
		
		// cleanup
		if(!floor::get_toolchain_keep_temp()) {
			core::system("rm " + compiled_file_or_code);
		}
		
		// move ptx code
		compiled_file_or_code.swap(ptx_code);
	}
	else if(options.target == TARGET::SPIRV_VULKAN ||
			options.target == TARGET::SPIRV_OPENCL) {
		const auto validate = (options.target == TARGET::SPIRV_VULKAN ?
							   floor::get_vulkan_validate_spirv() : floor::get_opencl_validate_spirv());
		const auto validator = (options.target == TARGET::SPIRV_VULKAN ?
								floor::get_vulkan_spirv_validator() : floor::get_opencl_spirv_validator());
		
		// run spirv-val if specified
		if(validate) {
			const string spirv_validator_cmd {
				"\"" + validator + "\" " + compiled_file_or_code
#if !defined(_MSC_VER)
				+ " 2>&1"
#endif
			};
			string spirv_validator_output = "";
			core::system(spirv_validator_cmd, spirv_validator_output);
			if(!spirv_validator_output.empty() && spirv_validator_output[spirv_validator_output.size() - 1] == '\n') {
				spirv_validator_output.pop_back(); // trim last newline
			}
			if(!options.silence_debug_output) {
				if(spirv_validator_output == "") {
					log_msg("spir-v validator: valid");
				}
				else {
					log_error("spir-v validator: %s", spirv_validator_output);
				}
			}
		}
		
		// NOTE: will cleanup the binary in opencl_compute/vulkan_compute
	}
	
	return { true, compiled_file_or_code, functions, options };
}
