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

#include <floor/compute/llvm_compute.hpp>
#include <floor/floor/floor.hpp>
#include <regex>

#include <floor/compute/opencl/opencl_device.hpp>
#include <floor/compute/cuda/cuda_device.hpp>
#include <floor/compute/metal/metal_device.hpp>

#if defined(__APPLE__)
#include <floor/darwin/darwin_helper.hpp>
#endif

bool llvm_compute::get_floor_metadata(const string& llvm_ir, vector<llvm_compute::kernel_info>& kernels,
									  const uint32_t toolchain_version) {
	// parses metadata lines of the format: !... = !{!N, !M, !I, !J, ...}
	const auto parse_metadata_line = [toolchain_version](const string& line, const auto& per_elem_func) {
		const auto set_start = line.find('{');
		const auto set_end = line.rfind('}');
		if(set_start != string::npos && set_end != string::npos &&
		   set_start + 1 < set_end) { // not interested in empty sets!
			const auto set_str = core::tokenize(line.substr(set_start + 1, set_end - set_start - 1), ',');
			for(const auto& elem_ws : set_str) {
				auto elem = core::trim(elem_ws); // trim whitespace, just in case
				
				// llvm 3.5
				if(toolchain_version < 360) {
					if(elem[0] == '!') {
						// ref, just forward
						per_elem_func(elem);
						continue;
					}
					
					const auto ws_pos = elem.find(' ');
					if(ws_pos != string::npos) {
						const auto elem_front = elem.substr(0, ws_pos);
						const auto elem_back = elem.substr(ws_pos + 1, elem.size() - ws_pos - 1);
						if(elem_front == "i32" || elem_front == "i64") {
							// int, forward back
							per_elem_func(elem_back);
							continue;
						}
						else if(elem_front == "metadata") {
							// string
							const auto str_front = elem_back.find('\"'), str_back = elem_back.rfind('\"');
							if(str_front != string::npos && str_back != string::npos && str_back > str_front) {
								per_elem_func(elem_back.substr(str_front + 1, str_back - str_front - 1));
								continue;
							}
						}
					}
				}
				// llvm 3.6/3.7/3.8+
				else {
					const auto ws_pos = elem.find(' ');
					if(ws_pos != string::npos) {
						const auto elem_front = elem.substr(0, ws_pos);
						const auto elem_back = elem.substr(ws_pos + 1, elem.size() - ws_pos - 1);
						if(elem_front == "i32" || elem_front == "i64") {
							// int, forward back
							per_elem_func(elem_back);
							continue;
						}
						// else: something else
					}
					else if(elem[0] == '!' && elem.find('\"') != string::npos) {
						// string
						const auto str_front = elem.find('\"'), str_back = elem.rfind('\"');
						if(str_front != string::npos && str_back != string::npos && str_back > str_front) {
							per_elem_func(elem.substr(str_front + 1, str_back - str_front - 1));
							continue;
						}
					}
				}
				
				// no idea what this is, just forward
				per_elem_func(elem);
			}
		}
	};
	
	// go through all lines and process the found metadata lines
	auto lines = core::tokenize(llvm_ir, '\n');
	unordered_map<uint64_t, const string*> metadata_refs;
	vector<uint64_t> kernel_refs;
	for(const auto& line : lines) {
		// check if it's a metadata line
		if(line[0] == '!') {
			const string metadata_type = line.substr(1, line.find(' ') - 1);
			if(metadata_type[0] >= '0' && metadata_type[0] <= '9') {
				// !N metadata
				const auto mid = stoull(metadata_type);
				metadata_refs.emplace(mid, &line);
			}
			else if(metadata_type == "floor.functions") {
				// contains all references to functions metadata
				// format: !floor.functions = !{!N, !M, !I, !J, ...}
				parse_metadata_line(line, [&kernel_refs](const string& elem) {
					if(elem[0] == '!') {
						const auto kernel_ref_idx = stoull(elem.substr(1));
						kernel_refs.emplace_back(kernel_ref_idx);
					}
				});
				
				// now that we know the kernel count, alloc enough memory
				kernels.resize(kernel_refs.size());
			}
		}
	}
	
	// parse the individual kernel metadata lines and put the info into the "kernels" vector
	for(size_t i = 0, count = kernel_refs.size(); i < count; ++i) {
		const auto& metadata = *metadata_refs[kernel_refs[i]];
		auto& kernel = kernels[i];
		uint32_t elem_idx = 0;
		parse_metadata_line(metadata, [&elem_idx, &kernel](const string& elem) {
			if(elem_idx == 0) {
				// version check
				static constexpr const int floor_functions_version { 2 };
				if(stoi(elem) != floor_functions_version) {
					log_warn("invalid floor.functions version, expected %u, got %u!",
							 floor_functions_version, elem);
				}
			}
			else if(elem_idx == 1) {
				// function name
				kernel.name = elem;
			}
			else if(elem_idx == 2) {
				// function type
				kernel.type = (llvm_compute::kernel_info::FUNCTION_TYPE)stou(elem);
			}
			else {
				// kernel arg info: #elem_idx size, address space, image type, image access
				const auto data = stoull(elem);
				kernel.args.emplace_back(llvm_compute::kernel_info::kernel_arg_info {
					.size			= (uint32_t)
						((data & uint64_t(llvm_compute::FLOOR_METADATA::ARG_SIZE_MASK)) >>
						 uint64_t(llvm_compute::FLOOR_METADATA::ARG_SIZE_SHIFT)),
					.address_space	= (llvm_compute::kernel_info::ARG_ADDRESS_SPACE)
						((data & uint64_t(llvm_compute::FLOOR_METADATA::ADDRESS_SPACE_MASK)) >>
						 uint64_t(llvm_compute::FLOOR_METADATA::ADDRESS_SPACE_SHIFT)),
					.image_type		= (llvm_compute::kernel_info::ARG_IMAGE_TYPE)
						((data & uint64_t(llvm_compute::FLOOR_METADATA::IMAGE_TYPE_MASK)) >>
						 uint64_t(llvm_compute::FLOOR_METADATA::IMAGE_TYPE_SHIFT)),
					.image_access	= (llvm_compute::kernel_info::ARG_IMAGE_ACCESS)
						((data & uint64_t(llvm_compute::FLOOR_METADATA::IMAGE_ACCESS_MASK)) >>
						 uint64_t(llvm_compute::FLOOR_METADATA::IMAGE_ACCESS_SHIFT)),
					.special_type	= (llvm_compute::kernel_info::SPECIAL_TYPE)
						((data & uint64_t(llvm_compute::FLOOR_METADATA::SPECIAL_TYPE_MASK)) >>
						 uint64_t(llvm_compute::FLOOR_METADATA::SPECIAL_TYPE_SHIFT)),
				});
			}
			++elem_idx;
		});
	}
	
	return true;
}

pair<string, vector<llvm_compute::kernel_info>> llvm_compute::compile_program(shared_ptr<compute_device> device,
																			  const string& code,
																			  const string additional_options,
																			  const TARGET target) {
	const string printable_code { "printf \"" + core::str_hex_escape(code) + "\" | " };
	return compile_input("-", printable_code, device, additional_options, target);
}

pair<string, vector<llvm_compute::kernel_info>> llvm_compute::compile_program_file(shared_ptr<compute_device> device,
																				   const string& filename,
																				   const string additional_options,
																				   const TARGET target) {
	return compile_input("\"" + filename + "\"", "", device, additional_options, target);
}

pair<string, vector<llvm_compute::kernel_info>> llvm_compute::compile_input(const string& input,
																			const string& cmd_prefix,
																			shared_ptr<compute_device> device,
																			const string additional_options,
																			const TARGET target) {
	// TODO/NOTE: additional clang flags:
	//  -vectorize-loops -vectorize-slp -vectorize-slp-aggressive
	//  -menable-unsafe-fp-math
	
	// for now, only enable these in debug mode (note that these cost a not insignificant amount of compilation time)
#if defined(FLOOR_DEBUG) && 0
	const char* warning_flags {
		// let's start with everything
		" -Weverything"
		// remove compat warnings
		" -Wno-c++98-compat -Wno-c++98-compat-pedantic -Wno-c99-extensions -Wno-c11-extensions -Wno-gnu"
		// in case we're using warning options that aren't supported by other clang versions
		" -Wno-unknown-warning-option"
		// really don't want to be too pedantic
		" -Wno-old-style-cast -Wno-date-time -Wno-system-headers -Wno-header-hygiene -Wno-documentation"
		// again: not too pedantic, also useful language features
		" -Wno-nested-anon-types -Wno-global-constructors -Wno-exit-time-destructors"
		// usually conflicting with the other switch/case warning, so disable it
		" -Wno-switch-enum"
		// TODO: also add -Wno-padded -Wno-packed? or make these optional? there are situations were these are useful
		// end
		" "
	};
#else
	const char* warning_flags { "" };
#endif
	
	// create the initial clang compilation command
	string clang_cmd = cmd_prefix;
	string libcxx_path = " -isystem \"", clang_path = " -isystem \"", floor_path = " -isystem \"";
	string sm_version = "20"; // handle cuda sm version (default to fermi/sm_20)
	uint32_t toolchain_version = 0;
	switch(target) {
		case TARGET::SPIR:
			clang_cmd += {
				"\"" + floor::get_opencl_compiler() + "\"" +
				" -x cl -Xclang -cl-std=CL1.2" \
				" -target " + (device->bitness == 32 ? "spir-unknown-unknown" : "spir64-unknown-unknown") +
				" -Xclang -cl-kernel-arg-info" \
				" -Xclang -cl-mad-enable" \
				" -Xclang -cl-fast-relaxed-math" \
				" -Xclang -cl-unsafe-math-optimizations" \
				" -Xclang -cl-finite-math-only" \
				" -DFLOOR_COMPUTE_OPENCL" \
				" -DFLOOR_COMPUTE_SPIR" +
				(!device->double_support ? " -DFLOOR_COMPUTE_NO_DOUBLE " : " ")
			};
			libcxx_path += floor::get_opencl_base_path() + "libcxx";
			clang_path += floor::get_opencl_base_path() + "clang";
			floor_path += floor::get_opencl_base_path() + "floor";
			toolchain_version = floor::get_opencl_toolchain_version();
			break;
		case TARGET::AIR: {
			const auto mtl_dev = (metal_device*)device.get();
			string os_target;
			if(mtl_dev->family < 10000) {
				// -> iOS 9.0+
				os_target = "ios9.0.0 -miphoneos-version-min=9.0";
			}
			else {
				// -> OS X 10.11+
				os_target = "macosx10.11.0 -mmacosx-version-min=10.11";
			}
			
			clang_cmd += {
				"\"" + floor::get_metal_compiler() + "\"" +
				// NOTE: always compiling to 64-bit here, because 32-bit never existed
				" -x metal -std=metal1.1 -target air64-apple-" + os_target +
				(device->vendor == COMPUTE_VENDOR::INTEL ? " -Xclang -metal-intel-workarounds" : "") +
				" -Xclang -cl-mad-enable" \
				" -Xclang -cl-fast-relaxed-math" \
				" -Xclang -cl-unsafe-math-optimizations" \
				" -Xclang -cl-finite-math-only" \
				" -DFLOOR_COMPUTE_NO_DOUBLE" \
				" -DFLOOR_COMPUTE_METAL"
			};
			libcxx_path += floor::get_metal_base_path() + "libcxx";
			clang_path += floor::get_metal_base_path() + "clang";
			floor_path += floor::get_metal_base_path() + "floor";
			toolchain_version = floor::get_metal_toolchain_version();
		} break;
		case TARGET::PTX: {
			const auto& force_sm = floor::get_cuda_force_compile_sm();
#if !defined(FLOOR_NO_CUDA)
			const auto& sm = ((cuda_device*)device.get())->sm;
			sm_version = (force_sm.empty() ? to_string(sm.x * 10 + sm.y) : force_sm);
#else
			if(!force_sm.empty()) sm_version = force_sm;
#endif
			
			toolchain_version = floor::get_cuda_toolchain_version();
			clang_cmd += {
				"\"" + floor::get_cuda_compiler() + "\"" +
				" -x cuda -std=cuda" \
				" -target " + (device->bitness == 32 ? "nvptx-nvidia-cuda" : "nvptx64-nvidia-cuda") +
				(toolchain_version >= 380 ? " --cuda-device-only --cuda-gpu-arch=sm_" + sm_version : "") +
				" -Xclang -fcuda-is-device" \
				" -DFLOOR_COMPUTE_CUDA"
			};
			libcxx_path += floor::get_cuda_base_path() + "libcxx";
			clang_path += floor::get_cuda_base_path() + "clang";
			floor_path += floor::get_cuda_base_path() + "floor";
		} break;
		case TARGET::APPLECL:
			clang_cmd += {
				"\"" + floor::get_opencl_compiler() + "\"" +
				" -x cl -Xclang -cl-std=CL1.2" \
				" -target " + (device->bitness == 32 ? "spir-applecl-unknown" : "spir64-applecl-unknown") +
				" -Xclang -applecl-kernel-info" \
				" -Xclang -cl-mad-enable" \
				" -Xclang -cl-fast-relaxed-math" \
				" -Xclang -cl-unsafe-math-optimizations" \
				" -Xclang -cl-finite-math-only" \
				" -DFLOOR_COMPUTE_OPENCL" \
				" -DFLOOR_COMPUTE_APPLECL" +
				(!device->double_support ? " -DFLOOR_COMPUTE_NO_DOUBLE " : " ")
			};
			libcxx_path += floor::get_opencl_base_path() + "libcxx";
			clang_path += floor::get_opencl_base_path() + "clang";
			floor_path += floor::get_opencl_base_path() + "floor";
			toolchain_version = floor::get_opencl_toolchain_version();
			break;
		case TARGET::SPIRV_VULKAN:
			toolchain_version = floor::get_vulkan_toolchain_version();
			if(toolchain_version < 380) {
				log_error("SPIR-V is not supported by this toolchain!");
				return {};
			}
			
			// still compiling this as opencl for now
			clang_cmd += {
				"\"" + floor::get_vulkan_compiler() + "\"" +
				" -x cl -Xclang -cl-std=CL1.2" \
				" -target " + (device->bitness == 32 ? "spir-unknown-unknown" : "spir64-unknown-unknown") +
				" -Xclang -cl-kernel-arg-info" \
				" -Xclang -cl-mad-enable" \
				" -Xclang -cl-fast-relaxed-math" \
				" -Xclang -cl-unsafe-math-optimizations" \
				" -Xclang -cl-finite-math-only" \
				" -DFLOOR_COMPUTE_VULKAN" \
				" -DFLOOR_COMPUTE_SPIRV" +
				(!device->double_support ? " -DFLOOR_COMPUTE_NO_DOUBLE " : " ")
			};
			libcxx_path += floor::get_vulkan_base_path() + "libcxx";
			clang_path += floor::get_vulkan_base_path() + "clang";
			floor_path += floor::get_vulkan_base_path() + "floor";
			break;
		case TARGET::SPIRV_OPENCL:
			toolchain_version = floor::get_opencl_toolchain_version();
			if(toolchain_version < 380) {
				log_error("SPIR-V is not supported by this toolchain!");
				return {};
			}
			
			// TODO: use CL2.0 or add select option for CL1.2 or CL2.0?
			clang_cmd += {
				"\"" + floor::get_opencl_compiler() + "\"" +
				" -x cl -Xclang -cl-std=CL1.2" \
				" -target " + (device->bitness == 32 ? "spir-unknown-unknown" : "spir64-unknown-unknown") +
				" -Xclang -cl-kernel-arg-info" \
				" -Xclang -cl-mad-enable" \
				" -Xclang -cl-fast-relaxed-math" \
				" -Xclang -cl-unsafe-math-optimizations" \
				" -Xclang -cl-finite-math-only" \
				" -DFLOOR_COMPUTE_OPENCL" \
				" -DFLOOR_COMPUTE_SPIRV" +
				(!device->double_support ? " -DFLOOR_COMPUTE_NO_DOUBLE " : " ")
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
	clang_cmd += " -DFLOOR_COMPUTE_TOOLCHAIN_VERSION=" + to_string(toolchain_version) + "u";
	
	// add device information
	// -> this adds both a "=" value definiton (that is used for enums in device_info.hpp) and a non-valued "_" defintion (used for #ifdef's)
	const auto vendor_str = compute_vendor_to_string(device->vendor);
	const auto platform_vendor_str = compute_vendor_to_string(device->platform_vendor);
	const auto type_str = (compute_device::has_flag<compute_device::TYPE::GPU>(device->type) ? "GPU" :
						   compute_device::has_flag<compute_device::TYPE::CPU>(device->type) ? "CPU" : "UNKNOWN");
	const auto os_str = (target != TARGET::AIR ?
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
		clang_cmd += " -DFLOOR_COMPUTE_INFO_HAS_SUB_GROUPS=1";
	}
	
	// handle image support
	const auto has_image_support = to_string(device->image_support);
	const auto has_image_depth_support = to_string(device->image_depth_support);
	const auto has_image_depth_write_support = to_string(device->image_depth_write_support);
	const auto has_image_msaa_support = to_string(device->image_msaa_support);
	const auto has_image_msaa_write_support = to_string(device->image_msaa_write_support);
	const auto has_image_cube_support = to_string(device->image_cube_support);
	const auto has_image_cube_write_support = to_string(device->image_cube_write_support);
	const auto has_image_mipmap_support = to_string(device->image_mipmap_support);
	const auto has_image_mipmap_write_support = to_string(device->image_mipmap_write_support);
	const auto has_image_offset_read_support = to_string(device->image_offset_read_support);
	const auto has_image_offset_write_support = to_string(device->image_offset_write_support);
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
	clang_cmd += " -DFLOOR_COMPUTE_INFO_HAS_IMAGE_CUBE_SUPPORT="s + has_image_cube_support;
	clang_cmd += " -DFLOOR_COMPUTE_INFO_HAS_IMAGE_CUBE_SUPPORT_"s + has_image_cube_support;
	clang_cmd += " -DFLOOR_COMPUTE_INFO_HAS_IMAGE_CUBE_WRITE_SUPPORT="s + has_image_cube_write_support;
	clang_cmd += " -DFLOOR_COMPUTE_INFO_HAS_IMAGE_CUBE_WRITE_SUPPORT_"s + has_image_cube_write_support;
	clang_cmd += " -DFLOOR_COMPUTE_INFO_HAS_IMAGE_MIPMAP_SUPPORT="s + has_image_mipmap_support;
	clang_cmd += " -DFLOOR_COMPUTE_INFO_HAS_IMAGE_MIPMAP_SUPPORT_"s + has_image_mipmap_support;
	clang_cmd += " -DFLOOR_COMPUTE_INFO_HAS_IMAGE_MIPMAP_WRITE_SUPPORT="s + has_image_mipmap_write_support;
	clang_cmd += " -DFLOOR_COMPUTE_INFO_HAS_IMAGE_MIPMAP_WRITE_SUPPORT_"s + has_image_mipmap_write_support;
	clang_cmd += " -DFLOOR_COMPUTE_INFO_HAS_IMAGE_OFFSET_READ_SUPPORT="s + has_image_mipmap_support;
	clang_cmd += " -DFLOOR_COMPUTE_INFO_HAS_IMAGE_OFFSET_READ_SUPPORT_"s + has_image_mipmap_support;
	clang_cmd += " -DFLOOR_COMPUTE_INFO_HAS_IMAGE_OFFSET_WRITE_SUPPORT="s + has_image_mipmap_write_support;
	clang_cmd += " -DFLOOR_COMPUTE_INFO_HAS_IMAGE_OFFSET_WRITE_SUPPORT_"s + has_image_mipmap_write_support;
	
	IMAGE_CAPABILITY img_caps { IMAGE_CAPABILITY::NONE };
	if(device->image_support) img_caps |= IMAGE_CAPABILITY::BASIC;
	if(device->image_depth_support) img_caps |= IMAGE_CAPABILITY::DEPTH_READ;
	if(device->image_depth_write_support) img_caps |= IMAGE_CAPABILITY::DEPTH_WRITE;
	if(device->image_msaa_support) img_caps |= IMAGE_CAPABILITY::MSAA_READ;
	if(device->image_msaa_write_support) img_caps |= IMAGE_CAPABILITY::MSAA_WRITE;
	if(device->image_cube_support) img_caps |= IMAGE_CAPABILITY::CUBE_READ;
	if(device->image_cube_write_support) img_caps |= IMAGE_CAPABILITY::CUBE_WRITE;
	if(device->image_mipmap_support) img_caps |= IMAGE_CAPABILITY::MIPMAP_READ;
	if(device->image_mipmap_write_support) img_caps |= IMAGE_CAPABILITY::MIPMAP_WRITE;
	if(device->image_offset_read_support) img_caps |= IMAGE_CAPABILITY::OFFSET_READ;
	if(device->image_offset_write_support) img_caps |= IMAGE_CAPABILITY::OFFSET_WRITE;
	clang_cmd += " -Xclang -floor-image-capabilities=" + to_string((underlying_type_t<IMAGE_CAPABILITY>)img_caps);
	
	// set cuda sm version
	if(target == TARGET::PTX) {
		clang_cmd += " -DFLOOR_COMPUTE_INFO_CUDA_SM=" + sm_version;
	}
	
	// emit line info if debug mode is enabled (unless this is spir where we'd better not emit this)
	if(floor::get_compute_debug() && target != TARGET::SPIR) {
		clang_cmd += " -gline-tables-only";
	}
	
	// add generic flags/options that are always used
	clang_cmd += {
#if defined(FLOOR_DEBUG)
		" -DFLOOR_DEBUG"
#endif
		" -DFLOOR_COMPUTE"
		" -DFLOOR_NO_MATH_STR" +
		clang_path + libcxx_path + floor_path +
		" -include floor/compute/device/common.hpp"
		" -fno-exceptions -fno-rtti -fstrict-aliasing -ffast-math -funroll-loops -flto -Ofast -ffp-contract=fast"
		// increase limit from 16 to 64, this "fixes" some forced unrolling
		" -mllvm -rotation-max-header-size=64 " +
		warning_flags +
		additional_options +
		// compile to the right device bitness
		(device->bitness == 32 ? " -m32 -DPLATFORM_X32" : " -m64 -DPLATFORM_X64") +
		" -emit-llvm -S -o - " + input
	};
	
	// on sane systems, redirect errors to stdout so that we can grab them
#if !defined(_MSC_VER)
	clang_cmd += " 2>&1";
#endif
	
	// compile and store the llvm ir in a string (stdout output)
	string ir_output = "";
	core::system(clang_cmd, ir_output);
	// if the output is empty or doesn't start with the llvm "ModuleID", something is probably wrong
	if(ir_output == "" || ir_output.size() < 10 || ir_output.substr(0, 10) != "; ModuleID") {
		log_error("compilation failed! failed cmd was:\n%s", clang_cmd);
		if(ir_output != "") {
			log_error("compilation errors:\n%s", ir_output);
		}
		return {};
	}
	if(floor::get_compute_log_commands()) {
		log_debug("clang cmd: %s", clang_cmd);
	}
	
	// grab floor metadata from compiled ir and create per-kernel info
	vector<kernel_info> kernels;
	get_floor_metadata(ir_output, kernels, toolchain_version);
	
	// final target specific processing/compilation
	string compiled_code = "";
	if(target == TARGET::SPIR) {
		// NOTE: temporary fixes to get this to compile with the intel compiler (readonly fail)
		core::find_and_replace(ir_output, "readonly", "");
		core::find_and_replace(ir_output, "nocapture readnone", "");
		
		// remove "dereferenceable(*)", this is not supported by spir
		static const regex rx_deref("dereferenceable\\(\\d+\\)");
		ir_output = regex_replace(ir_output, rx_deref, "");
		
		// output modified ir back to a file and create a bc file so spir-encoder can consume it
		const auto spir_35_ll = core::create_tmp_file_name("spir_3_5", ".ll");
		if(!file_io::string_to_file(spir_35_ll, ir_output)) {
			log_error("failed to output LLVM IR for SPIR consumption");
			return {};
		}
		const auto spir_35_bc = core::create_tmp_file_name("spir_3_5", ".bc");
		core::system("\"" + floor::get_opencl_as() + "\" -o " + spir_35_bc + " " + spir_35_ll);
		
		// run spir-encoder for 3.5 -> 3.2 conversion
		const auto spir_32_bc = core::create_tmp_file_name("spir_3_2", ".bc");
		const string spir_3_2_encoder_cmd {
			"\"" + floor::get_opencl_spir_encoder() + "\" " + spir_35_bc + " " + spir_32_bc
#if !defined(_MSC_VER)
			+ " 2>&1"
#endif
		};
		string spir_encoder_output = "";
		core::system(spir_3_2_encoder_cmd, spir_encoder_output);
		log_msg("spir encoder: %s", spir_encoder_output);
		
		// run spir-verifier if specified
		if(floor::get_opencl_verify_spir()) {
			const string spir_verifier_cmd {
				"\"" + floor::get_opencl_spir_verifier() + "\" " + spir_32_bc
#if !defined(_MSC_VER)
				+ " 2>&1"
#endif
			};
			string spir_verifier_output = "";
			core::system(spir_verifier_cmd, spir_verifier_output);
			if(!spir_verifier_output.empty() && spir_verifier_output[spir_verifier_output.size() - 1] == '\n') {
				spir_verifier_output.pop_back(); // trim last newline
			}
			log_msg("spir verifier: %s", spir_verifier_output);
		}
		
		// finally, read converted bitcode data back, this is the code that will be compiled by the opencl implementation
		if(!file_io::file_to_string(spir_32_bc, compiled_code)) {
			log_error("failed to read back SPIR 1.2 .bc file");
			return {};
		}
		
		// cleanup
		if(!floor::get_compute_keep_temp()) {
			core::system("rm " + spir_35_ll);
			core::system("rm " + spir_35_bc);
			core::system("rm " + spir_32_bc);
		}
	}
	else if(target == TARGET::AIR) {
		// output final processed ir if this was specified in the config
		// NOTE: explicitly create this in the working directory (not in tmp)
		if(floor::get_compute_keep_temp()) {
			file_io::string_to_file("air_processed.ll", ir_output);
		}
		
		// llvm ir is the final output format
		compiled_code.swap(ir_output);
	}
	else if(target == TARGET::PTX) {
		// llc expects an input file (or stdin, but we can't write there reliably)
		const auto cuda_ll = core::create_tmp_file_name("cuda", ".ll");
		if(!file_io::string_to_file(cuda_ll, ir_output)) {
			log_error("failed to output LLVM IR for llc consumption");
			return {};
		}
		
		// compile llvm ir to ptx
		const string llc_cmd {
			"\"" + floor::get_cuda_llc() + "\"" +
			" -nvptx-fma-level=2 -nvptx-sched4reg -enable-unsafe-fp-math" \
			" -mcpu=sm_" + sm_version + " -mattr=ptx43" +
			" -o - " + cuda_ll
#if !defined(_MSC_VER)
			+ " 2>&1"
#endif
		};
		if(floor::get_compute_log_commands()) {
			log_debug("llc cmd: %s", llc_cmd);
		}
		core::system(llc_cmd, compiled_code);
		
		// only output the compiled ptx code if this was specified in the config
		// NOTE: explicitly create this in the working directory (not in tmp)
		if(floor::get_compute_keep_temp()) {
			file_io::string_to_file("cuda.ptx", compiled_code);
		}
		
		// check if we have sane output
		if(compiled_code == "" || compiled_code.find("Generated by LLVM NVPTX Back-End") == string::npos) {
			log_error("llc/ptx compilation failed!\n%s", compiled_code);
			return {};
		}
		
		// cleanup
		if(floor::get_compute_keep_temp()) {
			core::system("rm " + cuda_ll);
		}
	}
	else if(target == TARGET::APPLECL) {
		// apparently this needs the same fixes as the intel/amd spir compilers
		core::find_and_replace(ir_output, "readonly", "");
		core::find_and_replace(ir_output, "nocapture readnone", "");
		
		// remove "dereferenceable(*)", this is not supported by applecl
		static const regex rx_deref("dereferenceable\\(\\d+\\)");
		ir_output = regex_replace(ir_output, rx_deref, "");
		
		// output (modified) ir back to a file and create a bc file so applecl-encoder can consume it
		const auto applecl_35_ll = core::create_tmp_file_name("applecl_3_5", ".ll");
		if(!file_io::string_to_file(applecl_35_ll, ir_output)) {
			log_error("failed to output LLVM IR for AppleCL consumption");
			return {};
		}
		const auto applecl_35_bc = core::create_tmp_file_name("applecl_3_5", ".bc");
		core::system("\"" + floor::get_opencl_as() + "\" -o " + applecl_35_bc + " " + applecl_35_ll);
		
		// run applecl-encoder for 3.5 -> 3.2 conversion
		const auto applecl_32_bc = core::create_tmp_file_name("applecl_3_2", ".bc");
		const string applecl_3_2_encoder_cmd {
			"\"" + floor::get_opencl_applecl_encoder() + "\"" +
			(compute_device::has_flag<compute_device::TYPE::CPU>(device->type) ? " -encode-cpu" : "") +
			" " + applecl_35_bc + " " + applecl_32_bc
#if !defined(_MSC_VER)
			+ " 2>&1"
#endif
		};
		string applecl_encoder_output = "";
		core::system(applecl_3_2_encoder_cmd, applecl_encoder_output);
		log_msg("applecl encoder: %s", applecl_encoder_output);
		
		// finally, read converted bitcode data back, this is the code that will be compiled by the opencl implementation
		if(!file_io::file_to_string(applecl_32_bc, compiled_code)) {
			log_error("failed to read back AppleCL .bc file");
			return {};
		}
		
		// cleanup
		if(!floor::get_compute_keep_temp()) {
			core::system("rm " + applecl_35_ll);
			core::system("rm " + applecl_35_bc);
			core::system("rm " + applecl_32_bc);
		}
	}
	else if(target == TARGET::SPIRV_VULKAN ||
			target == TARGET::SPIRV_OPENCL) {
		const auto encoder = (target == TARGET::SPIRV_VULKAN ? floor::get_vulkan_spirv_encoder() : floor::get_opencl_spirv_encoder());
		const auto as = (target == TARGET::SPIRV_VULKAN ? floor::get_vulkan_as() : floor::get_opencl_as());
		const auto validate = (target == TARGET::SPIRV_VULKAN ? floor::get_vulkan_validate_spirv() : floor::get_opencl_validate_spirv());
		const auto validator = (target == TARGET::SPIRV_VULKAN ? floor::get_vulkan_spirv_validator() : floor::get_opencl_spirv_validator());
		
		// output modified ir back to a file and create a bc file so llvm-spirv can consume it
		const auto spirv_38_ll = core::create_tmp_file_name("spirv_3_8", ".ll");
		if(!file_io::string_to_file(spirv_38_ll, ir_output)) {
			log_error("failed to output LLVM IR for SPIR-V consumption");
			return {};
		}
		const auto spirv_38_bc = core::create_tmp_file_name("spirv_3_8", ".bc");
		core::system("\"" + as + "\" -o " + spirv_38_bc + " " + spirv_38_ll);
		
		// run llvm-spirv for llvm 3.8 bc -> spir-v binary conversion
		const auto spirv_bin = core::create_tmp_file_name("spirv_3_8", ".spv");
		const string spirv_encoder_cmd {
			"\"" + encoder + "\" -o " + spirv_bin + " " + spirv_38_bc
#if !defined(_MSC_VER)
			+ " 2>&1"
#endif
		};
		string spirv_encoder_output = "";
		core::system(spirv_encoder_cmd, spirv_encoder_output);
		log_msg("spir-v encoder: %s", spirv_encoder_output);
		
		// run spirv-val if specified
		if(validate) {
			const string spirv_validator_cmd {
				"\"" + validator + "\" " + spirv_bin
#if !defined(_MSC_VER)
				+ " 2>&1"
#endif
			};
			string spirv_validator_output = "";
			core::system(spirv_validator_cmd, spirv_validator_output);
			if(!spirv_validator_output.empty() && spirv_validator_output[spirv_validator_output.size() - 1] == '\n') {
				spirv_validator_output.pop_back(); // trim last newline
			}
			log_msg("spir-v validator: %s", spirv_validator_output);
		}
		
		// always directly use the encoded spir-v binary (no read back + write again)
		compiled_code = spirv_bin;
		
		// cleanup
		if(!floor::get_compute_keep_temp()) {
			core::system("rm " + spirv_38_ll);
			core::system("rm " + spirv_38_bc);
		}
	}
	
	return { compiled_code, kernels };
}
