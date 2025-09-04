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

#include <floor/device/toolchain.hpp>
#include <floor/floor.hpp>
#include <floor/core/cpp_ext.hpp>
#include <regex>
#include <climits>
#include <filesystem>
#include <cassert>

#include <floor/device/opencl/opencl_device.hpp>
#include <floor/device/cuda/cuda_device.hpp>
#include <floor/device/metal/metal_device.hpp>
#include <floor/device/vulkan/vulkan_device.hpp>
#include <floor/device/host/host_device.hpp>
#include <floor/core/file_io.hpp>
#include <floor/core/core.hpp>

#if defined(__APPLE__)
#include <floor/darwin/darwin_helper.hpp>
#endif

namespace fl::toolchain {
using namespace std::literals;

bool create_floor_function_info(const std::string& ffi_file_name,
								std::vector<function_info>& functions,
								const uint32_t toolchain_version floor_unused) {
	std::string ffi;
	if (!file_io::file_to_string(ffi_file_name, ffi)) {
		log_error("failed to retrieve floor function info from \"$\"", ffi_file_name);
		return false;
	}
	
	const auto lines = core::tokenize(ffi, '\n');
	functions.reserve(std::max(lines.size(), size_t(1)) - 1);
	for (const auto& line : lines) {
		if (line.empty()) continue;
		const auto tokens = core::tokenize(line, ',');
		
		// at least 8 w/o any args:
		// functions : <version>,<func_name>,<type>,<flags>,<local_size_x>,<local_size_y>,<local_size_z>,<simd-width>,<args...>
		// arg-buffer: <version>,<func_name>,<type>,<flags>,<arg # in func/arg-buffer>,0,0,0,<args...>
		// NOTE: arg-buffer entry must come after the function entry that uses it
		// NOTE: for an argument buffer struct local_size_* is 0
		static constexpr const uint32_t function_token_count { 8u };
		if (tokens.size() < function_token_count) {
			log_error("invalid function info entry: $", line);
			return false;
		}
		const auto& token_version = tokens[0];
		const auto& token_name = tokens[1];
		const auto& token_type = tokens[2];
		const auto& token_flags = tokens[3];
		// -> functions
		const auto& token_req_local_size_x = tokens[4];
		const auto& token_req_local_size_y = tokens[5];
		const auto& token_req_local_size_z = tokens[6];
		const auto& token_req_simd_width = tokens[7];
		// -> argument buffer
		const auto& token_arg_num = tokens[4];
		
		//
		static constexpr const char floor_functions_version[] { "7" };
		if (token_version != floor_functions_version) {
			log_error("invalid floor function info version, expected $, got $!",
					  floor_functions_version, token_version);
			return false;
		}
		
		FUNCTION_TYPE func_type = FUNCTION_TYPE::NONE;
		if (token_type == "1") func_type = FUNCTION_TYPE::KERNEL;
		else if (token_type == "2") func_type = FUNCTION_TYPE::VERTEX;
		else if (token_type == "3") func_type = FUNCTION_TYPE::FRAGMENT;
		else if (token_type == "4") func_type = FUNCTION_TYPE::TESSELLATION_CONTROL;
		else if (token_type == "5") func_type = FUNCTION_TYPE::TESSELLATION_EVALUATION;
		else if (token_type == "100") func_type = FUNCTION_TYPE::ARGUMENT_BUFFER_STRUCT;
		
		if (func_type != FUNCTION_TYPE::KERNEL &&
			func_type != FUNCTION_TYPE::VERTEX &&
			func_type != FUNCTION_TYPE::FRAGMENT &&
			func_type != FUNCTION_TYPE::TESSELLATION_CONTROL &&
			func_type != FUNCTION_TYPE::TESSELLATION_EVALUATION &&
			func_type != FUNCTION_TYPE::ARGUMENT_BUFFER_STRUCT) {
			log_error("unsupported function type: $", token_type);
			return false;
		}
		
		const auto func_flags = (FUNCTION_FLAGS)strtoull(token_flags.c_str(), nullptr, 10);
		
		uint3 required_local_size {};
		uint32_t required_simd_width { 0u };
		if (func_type != FUNCTION_TYPE::ARGUMENT_BUFFER_STRUCT) {
			required_local_size = {
				(uint32_t)strtoull(token_req_local_size_x.c_str(), nullptr, 10),
				(uint32_t)strtoull(token_req_local_size_y.c_str(), nullptr, 10),
				(uint32_t)strtoull(token_req_local_size_z.c_str(), nullptr, 10),
			};
			required_simd_width = (uint32_t)strtoull(token_req_simd_width.c_str(), nullptr, 10);
		}
		
		function_info info {
			.name = token_name,
			.required_local_size = required_local_size,
			.required_simd_width = required_simd_width,
			.type = func_type,
			.flags = func_flags,
		};
		if (info.type == FUNCTION_TYPE::ARGUMENT_BUFFER_STRUCT && !info.required_local_size.is_null()) {
			log_error("required local size must be 0 for argument buffer struct info");
			return false;
		}
		if (info.type == FUNCTION_TYPE::ARGUMENT_BUFFER_STRUCT && info.required_simd_width != 0) {
			log_error("required SIMD width must be 0 for argument buffer struct info");
			return false;
		}
		
		// each arg: size,array_extent,address_space,access,image_type,flags
		static constexpr const uint32_t arg_token_count { 6u };
		assert(((tokens.size() - function_token_count) % arg_token_count) == 0u && "invalid args token count");
		for (size_t i = function_token_count, count = tokens.size(); i < count; i += arg_token_count) {
			uint64_t arg_tokens[arg_token_count];
			for (uint32_t j = 0; j < arg_token_count; ++j) {
				assert(!tokens[i + j].empty());
				arg_tokens[j] = strtoull(tokens[i + j].c_str(), nullptr, 10);
			}
			
			info.args.emplace_back(arg_info {
				.size = arg_tokens[0],
				.array_extent = arg_tokens[1],
				.address_space = (ARG_ADDRESS_SPACE)arg_tokens[2],
				.access = (ARG_ACCESS)arg_tokens[3],
				.image_type = (ARG_IMAGE_TYPE)arg_tokens[4],
				.flags = (ARG_FLAG)arg_tokens[5],
			});
		}
		
		if (info.type == FUNCTION_TYPE::ARGUMENT_BUFFER_STRUCT) {
			const auto arg_idx = (uint32_t)strtoull(token_arg_num.c_str(), nullptr, 10);
			
			bool found_func = false;
			for (auto riter = functions.rbegin(); riter != functions.rend(); ++riter) {
				if (riter->name == info.name) {
					found_func = true;
					
					if (arg_idx >= riter->args.size()) {
						log_error("argument index $ is out-of-bounds for function $ with $ args", arg_idx, info.name, riter->args.size());
						return false;
					}
					
					auto& arg = riter->args[arg_idx];
					if (!has_flag<ARG_FLAG::ARGUMENT_BUFFER>(arg.flags)) {
						log_error("argument index $ in function $ is not an argument buffer", arg_idx, info.name);
						return false;
					}
					
					arg.argument_buffer_info = std::move(info);
					break;
				}
			}
			if (!found_func) {
				log_error("didn't find function $ for argument buffer", info.name);
				return false;
			}
		} else {
			functions.emplace_back(info);
		}
	}
	
	// check if all argument buffer info has been set for all functions
	for (const auto& func : functions) {
		for (size_t i = 0, count = func.args.size(); i < count; ++i) {
			const auto& arg = func.args[i];
			if (has_flag<ARG_FLAG::ARGUMENT_BUFFER>(arg.flags) && !arg.argument_buffer_info) {
				log_error("missing argument buffer info for argument #$ in function $", i, func.name);
				return false;
			}
		}
	}

	return true;
}

//! compiles a program from the specified input file/handle and prefixes the compiler call with "cmd_prefix"
static program_data compile_input(const std::string& input,
								  const std::string& cmd_prefix,
								  const device& dev,
								  const compile_options options,
								  const bool build_pch);

program_data compile_program(const device& dev,
							 const std::string& code,
							 const compile_options options) {
	const std::string printable_code { "printf \"" + core::str_hex_escape(code) + "\" | " };
	return compile_input("-", printable_code, dev, options, false);
}

program_data compile_program_file(const device& dev,
								  const std::string& filename,
								  const compile_options options) {
	return compile_input("\"" + filename + "\"", "", dev, options, false);
}

program_data compile_precompiled_header(const std::string& pch_output_file_name,
										const device& dev,
										const compile_options options_) {
	auto options = options_;
	options.pch = pch_output_file_name;
	return compile_input("", "", dev, options, true);
}

program_data compile_input(const std::string& input,
						   const std::string& cmd_prefix,
						   const device& dev,
						   const compile_options options,
						   const bool build_pch) {
	// create the initial clang compilation command
	std::string clang_cmd = cmd_prefix;
	std::string libcxx_path, clang_path, floor_include_path;
	std::string sm_version = "50"; // handle CUDA sm version (default to Maxwell/sm_50)
	std::string sm_aa_enabled = "0";
	uint32_t ptx_version = std::max(80u, options.cuda.ptx_version); // handle CUDA PTX version (default to at least ptx 8.0)
	std::string output_file_type = "bc"; // can be overwritten by target
	uint32_t toolchain_version = 0;
	bool disable_sub_groups = false; // in case something needs to override device capabilities
	std::string metal_emit_format;
	const bool metal_preprocess = (options.target == TARGET::AIR && options.debug.preprocess_condense);
	bool primitive_id_support = dev.primitive_id_support;
	bool barycentric_coord_support = dev.barycentric_coord_support;
	switch (options.target) {
		case TARGET::SPIR:
			if (!floor::has_opencl_toolchain()) {
				log_error("can not compile OpenCL source code without a toolchain");
				return {};
			}
			
			toolchain_version = floor::get_opencl_toolchain_version();
			clang_cmd += {
				"\"" + floor::get_opencl_compiler() + "\"" +
				" -x " + (!build_pch ? "cl" : "cl-header") +
				(!build_pch ? " -Xclang -llvm-bc-32 -cl-no-stdinc" : "") +
				" -Xclang -cl-std=CL1.2" +
				" -target spir64-unknown-unknown" \
				" -Xclang -cl-sampler-type -Xclang i32" \
				" -Xclang -cl-kernel-arg-info" \
				" -Xclang -cl-mad-enable" \
				" -Xclang -cl-fast-relaxed-math" \
				" -Xclang -cl-unsafe-math-optimizations" \
				" -Xclang -cl-finite-math-only" \
				" -DFLOOR_DEVICE_OPENCL" \
				" -DFLOOR_DEVICE_SPIR" \
				" -DFLOOR_DEVICE_OPENCL_MAJOR=1" \
				" -DFLOOR_DEVICE_OPENCL_MINOR=2" +
				(!dev.double_support ? " -DFLOOR_DEVICE_NO_DOUBLE" : "") +
				(floor::get_opencl_verify_spir() ? " -Xclang -cl-verify-spir" : "") +
				(dev.platform_vendor == VENDOR::INTEL &&
				 dev.vendor == VENDOR::INTEL ? " -Xclang -cl-spir-intel-workarounds" : "") +
				" -mllvm -slp-vectorize-hor=false"
			};
			libcxx_path += floor::get_opencl_base_path() + "libcxx";
			clang_path += floor::get_opencl_base_path() + "clang";
			floor_include_path += floor::get_opencl_base_path() + "include";
			break;
		case TARGET::AIR: {
			if (!floor::has_metal_toolchain()) {
				log_error("can not compile Metal source code without a toolchain");
				return {};
			}
			
			toolchain_version = floor::get_metal_toolchain_version();
			output_file_type = "metallib";
			
			const auto& mtl_dev = (const metal_device&)dev;
			auto metal_version = mtl_dev.metal_language_version;
			const auto metal_force_version = (!options.ignore_runtime_info ? floor::get_metal_force_version() : 0);
			if (metal_force_version != 0) {
				switch (metal_force_version) {
					case 30:
						metal_version = METAL_VERSION::METAL_3_0;
						break;
					case 31:
						metal_version = METAL_VERSION::METAL_3_1;
						break;
					case 32:
						metal_version = METAL_VERSION::METAL_3_2;
						break;
					case 40:
						metal_version = METAL_VERSION::METAL_4_0;
						break;
					default:
						log_error("invalid force_version: $", metal_force_version);
						break;
				}
			}
			if (metal_version > METAL_VERSION::METAL_4_0) {
				log_error("unsupported Metal language version: $", metal_version_to_string(metal_version));
				return {};
			}
			
			std::string os_target;
			if (mtl_dev.platform_type == metal_device::PLATFORM_TYPE::IOS ||
				mtl_dev.platform_type == metal_device::PLATFORM_TYPE::IOS_SIMULATOR) {
				// -> iOS 16.0+
				switch (metal_version) {
					default:
					case METAL_VERSION::METAL_3_0:
						os_target = "ios16.0.0";
						break;
					case METAL_VERSION::METAL_3_1:
						os_target = "ios17.0.0";
						break;
					case METAL_VERSION::METAL_3_2:
						os_target = "ios18.0.0";
						break;
					case METAL_VERSION::METAL_4_0:
						os_target = "ios26.0.0";
						break;
				}
			} else if (mtl_dev.platform_type == metal_device::PLATFORM_TYPE::VISIONOS ||
					   mtl_dev.platform_type == metal_device::PLATFORM_TYPE::VISIONOS_SIMULATOR) {
				// -> visionOS/XROS 2.0+
				switch (metal_version) {
					default:
					case METAL_VERSION::METAL_3_2:
						os_target = "xros2.0.0";
						break;
					case METAL_VERSION::METAL_4_0:
						os_target = "xros26.0.0";
						break;
				}
			} else if (mtl_dev.platform_type == metal_device::PLATFORM_TYPE::MACOS) {
				// -> macOS 13.0+
				switch (metal_version) {
					default:
					case METAL_VERSION::METAL_3_0:
						os_target = "macosx13.0.0";
						break;
					case METAL_VERSION::METAL_3_1:
						os_target = "macosx14.0.0";
						break;
					case METAL_VERSION::METAL_3_2:
						os_target = "macosx15.0.0";
						break;
					case METAL_VERSION::METAL_4_0:
						os_target = "macosx16.0.0"; // NOTE: for now, we're still on 16.0 ...
						break;
				}
			} else {
				log_error("unsupported Metal device family type: $", uint32_t(mtl_dev.family_type));
				return {};
			}
			if (mtl_dev.platform_type == metal_device::PLATFORM_TYPE::IOS_SIMULATOR ||
				mtl_dev.platform_type == metal_device::PLATFORM_TYPE::VISIONOS_SIMULATOR) {
				os_target += "-simulator";
			}
			
			std::string metal_std = "metal3.0";
			std::string air_target = "air64";
			switch (metal_version) {
				case METAL_VERSION::METAL_3_0:
					metal_std = "metal3.0";
					break;
				case METAL_VERSION::METAL_3_1:
					metal_std = "metal3.1";
					break;
				case METAL_VERSION::METAL_3_2:
					metal_std = "metal3.2";
					break;
				case METAL_VERSION::METAL_4_0:
					metal_std = "metal4.0";
					air_target = "air64_v28";
					break;
				default: break;
			}
			
			bool soft_printf = false;
			if (options.metal.soft_printf) {
				soft_printf = *options.metal.soft_printf;
			} else {
				soft_printf = floor::get_metal_soft_printf();
			}
			
			metal_emit_format = (!build_pch ? " -Xclang -emit-metallib" : "");
			clang_cmd += {
				"\"" + floor::get_metal_compiler() + "\"" +
				" -x " + (!build_pch ? "metal" : "metal-header") +
				" -std=" + metal_std + " -target " + air_target + "-apple-" + os_target +
#if defined(__APPLE__)
				// always enable intel workarounds (conversion problems)
				(dev.vendor == VENDOR::INTEL ? " -Xclang -metal-intel-workarounds" : "") +
#endif
				(soft_printf ? " -Xclang -metal-soft-printf -DFLOOR_DEVICE_HAS_SOFT_PRINTF=1" : "") +
				" -Xclang -cl-mad-enable" \
				" -Xclang -cl-fast-relaxed-math" \
				" -Xclang -cl-unsafe-math-optimizations" \
				" -Xclang -cl-finite-math-only" +
				" -DFLOOR_DEVICE_NO_DOUBLE" \
				" -DFLOOR_DEVICE_METAL" +
				" -DFLOOR_DEVICE_METAL_MAJOR=" + metal_major_version_to_string(metal_version) +
				" -DFLOOR_DEVICE_METAL_MINOR=" + metal_minor_version_to_string(metal_version) +
				" -mllvm -slp-vectorize-hor=false"
			};
			libcxx_path += floor::get_metal_base_path() + "libcxx";
			clang_path += floor::get_metal_base_path() + "clang";
			floor_include_path += floor::get_metal_base_path() + "include";
			
			// base SIMD reduction support on device support flag
			const auto has_simd_reduction_str = std::to_string(mtl_dev.simd_reduction);
			clang_cmd += " -DFLOOR_DEVICE_METAL_HAS_SIMD_REDUCTION="s + has_simd_reduction_str;
			clang_cmd += " -DFLOOR_DEVICE_METAL_HAS_SIMD_REDUCTION_"s + has_simd_reduction_str;
		} break;
		case TARGET::PTX: {
			if (!floor::has_cuda_toolchain()) {
				log_error("can not compile CUDA source code without a toolchain");
				return {};
			}
			
			// handle sm version
			const auto& cuda_dev = (const cuda_device&)dev;
			const auto& force_sm = floor::get_cuda_force_compile_sm();
			const auto& sm = cuda_dev.sm;
			sm_version = (force_sm.empty() || options.ignore_runtime_info ? std::to_string(sm.x * 10 + sm.y) : force_sm);
			// enable architecture-accelerated codegen if specifically enabled (external or FUBAR compilation) or for run-time devices/compilation
			const auto is_sm_aa = (cuda_dev.sm.x >= 9 && (cuda_dev.sm_aa || !options.ignore_runtime_info));
			if (is_sm_aa) {
				sm_aa_enabled = "1";
			}
			
			output_file_type = "ptx";

			// handle ptx version:
			// * 8.0 is the minimum requirement for floor (sm_50 - sm_90)
			// * 8.6 for sm_100/sm_101
			// * 8.7 for sm_120
			// * 8.8 for sm_103/sm_121
			// * 9.0 for sm_110/sm_88
			// * 9.0 for anything else
			switch (cuda_dev.sm.x) {
				case 5:
				case 6:
				case 7:
				case 8:
					// already 80
					if (cuda_dev.sm.y == 8) {
						// sm_88 needs PTX 9.0 now
						ptx_version = std::max(90u, ptx_version);
					}
					break;
				case 9:
					ptx_version = std::max(cuda_dev.sm.y == 0 ? 80u : 86u, ptx_version);
					break;
				case 10:
					ptx_version = std::max(cuda_dev.sm.y <= 1 ? 86u : 88u, ptx_version);
					break;
				case 11:
					ptx_version = std::max(90u, ptx_version);
					break;
				case 12:
					ptx_version = std::max(cuda_dev.sm.y == 0 ? 87u : 88u, ptx_version);
					break;
				default:
					ptx_version = std::max(90u, ptx_version);
					break;
			}
			if (!floor::get_cuda_force_ptx().empty() && !options.ignore_runtime_info) {
				const auto forced_version = stou(floor::get_cuda_force_ptx());
				if (forced_version >= 80 && forced_version < std::numeric_limits<uint32_t>::max()) {
					ptx_version = forced_version;
				}
			}
			
			toolchain_version = floor::get_cuda_toolchain_version();
			clang_cmd += {
				"\"" + floor::get_cuda_compiler() + "\"" +
				" -x " + (!build_pch ? "cuda" : "cuda-header") +
				" -std=cuda" \
				" -target x86_64--" +
				" -nocudalib -nocudainc --cuda-device-only --cuda-gpu-arch=sm_" + sm_version + (is_sm_aa ? "a" : "") +
				" -Xclang -target-feature -Xclang +ptx" + std::to_string(ptx_version) +
				" -Xclang -fcuda-is-device" \
				" -DFLOOR_DEVICE_CUDA"
			};
			if (options.cuda.short_ptr) {
				clang_cmd += " -fcuda-short-ptr -mllvm --nvptx-short-ptr";
			}
			libcxx_path += floor::get_cuda_base_path() + "libcxx";
			clang_path += floor::get_cuda_base_path() + "clang";
			floor_include_path += floor::get_cuda_base_path() + "include";
		} break;
		case TARGET::SPIRV_VULKAN: {
			if (!floor::has_vulkan_toolchain()) {
				log_error("can not compile Vulkan source code without a toolchain");
				return {};
			}
			
			toolchain_version = floor::get_vulkan_toolchain_version();
			output_file_type = "spvc";
			
			const auto& vk_device = (const vulkan_device&)dev;
			std::string vulkan_std = "vulkan1.3";
			switch(vk_device.vulkan_version) {
				case VULKAN_VERSION::VULKAN_1_3:
					vulkan_std = "vulkan1.3";
					break;
				case VULKAN_VERSION::VULKAN_1_4:
					vulkan_std = "vulkan1.4";
					break;
				default: break;
			}
			
			bool soft_printf = false;
			if (options.vulkan.soft_printf) {
				soft_printf = *options.vulkan.soft_printf;
			} else {
				soft_printf = floor::get_vulkan_soft_printf();
			}
			
			// still compiling this as OpenCL for now
			clang_cmd += {
				"\"" + floor::get_vulkan_compiler() + "\"" +
				" -x " + (!build_pch ? "vulkan" : "vulkan-header") +
				(!build_pch ? " -Xclang -emit-spirv-container" : "") +
				" -std=" + vulkan_std +
				" -target spir64-unknown-unknown-vulkan" +
				" -Xclang -cl-sampler-type -Xclang i32" \
				" -Xclang -cl-kernel-arg-info" \
				" -Xclang -cl-mad-enable" \
				" -Xclang -cl-fast-relaxed-math" \
				" -Xclang -cl-unsafe-math-optimizations" \
				" -Xclang -cl-finite-math-only" +
				" -Xclang -vulkan-iub-size=" + std::to_string(vk_device.max_inline_uniform_block_size) +
				" -Xclang -vulkan-iub-count=" + std::to_string(vk_device.max_inline_uniform_block_count) +
				(soft_printf ? " -Xclang -vulkan-soft-printf -DFLOOR_DEVICE_HAS_SOFT_PRINTF=1" : "") +
				(options.vulkan.pre_structurization_pass ? " -Xclang -vulkan-llvm-pre-structurization-pass" : "") +
				(vk_device.subgroup_uniform_cf_support ? " -Xclang -vulkan-subgroup-uniform-cf" : "") +
				(vk_device.max_descriptor_set_count < vulkan_device::min_required_high_bound_descriptor_sets ?
				 " -Xclang -vulkan-low-descriptor-set-count" : "") +
				" -DFLOOR_DEVICE_VULKAN" \
				" -DFLOOR_DEVICE_SPIRV" \
				" -DFLOOR_DEVICE_NO_DOUBLE"
				// TODO: fix Vulkan double support
				//(!dev.double_support ? " -DFLOOR_DEVICE_NO_DOUBLE" : "")
				" -mllvm -slp-vectorize-hor=false"
			};
			libcxx_path += floor::get_vulkan_base_path() + "libcxx";
			clang_path += floor::get_vulkan_base_path() + "clang";
			floor_include_path += floor::get_vulkan_base_path() + "include";
		} break;
		case TARGET::SPIRV_OPENCL: {
			if (!floor::has_opencl_toolchain()) {
				log_error("can not compile OpenCL source code without a toolchain");
				return {};
			}
			
			toolchain_version = floor::get_opencl_toolchain_version();
			output_file_type = "spv";
			const auto& cl_device = (const opencl_device&)dev;
			if(cl_device.spirv_version == SPIRV_VERSION::NONE) {
				log_error("SPIR-V is not supported by this device!");
				return {};
			}
			
			clang_cmd += {
				"\"" + floor::get_opencl_compiler() + "\"" +
				// compile to the max OpenCL standard that is supported by the device
				" -x " + (!build_pch ? "cl" : "cl-header") +
				(!build_pch ? " -Xclang -emit-spirv -cl-no-stdinc" : "") +
				" -Xclang -cl-std=CL" + cl_version_to_string(cl_device.cl_version) +
				" -target spir64-unknown-unknown" \
				" -Xclang -cl-sampler-type -Xclang i32" \
				" -Xclang -cl-kernel-arg-info" \
				" -Xclang -cl-mad-enable" \
				" -Xclang -cl-fast-relaxed-math" \
				" -Xclang -cl-unsafe-math-optimizations" \
				" -Xclang -cl-finite-math-only" \
				" -DFLOOR_DEVICE_OPENCL" \
				" -DFLOOR_DEVICE_SPIRV" \
				" -DFLOOR_DEVICE_OPENCL_MAJOR=" + cl_major_version_to_string(cl_device.cl_version) +
				" -DFLOOR_DEVICE_OPENCL_MINOR=" + cl_minor_version_to_string(cl_device.cl_version) +
				(!dev.double_support ? " -DFLOOR_DEVICE_NO_DOUBLE" : "") +
				" -mllvm -slp-vectorize-hor=false"
			};
			libcxx_path += floor::get_opencl_base_path() + "libcxx";
			clang_path += floor::get_opencl_base_path() + "clang";
			floor_include_path += floor::get_opencl_base_path() + "include";
		} break;
		case TARGET::HOST_COMPUTE_CPU: {
			if (!floor::has_host_toolchain()) {
				log_error("can not compile Host-Compute source code without a toolchain");
				return {};
			}
			
			toolchain_version = floor::get_host_toolchain_version();
			output_file_type = "bin";
			const auto& cpu_device = (const host_device&)dev;
			
			std::string arch, tune, prefer_vec_width;
			switch (cpu_device.cpu_tier) {
#if defined(__x86_64__)
				default:
#endif
				case HOST_CPU_TIER::X86_TIER_1:
					arch = "corei7";
					break;
				case HOST_CPU_TIER::X86_TIER_2:
					arch = "corei7-avx";
					break;
				case HOST_CPU_TIER::X86_TIER_3:
					arch = "core-avx2";
					break;
				case HOST_CPU_TIER::X86_TIER_4:
					arch = "skylake-avx512";
					// override default behavior of preferring 256-bit
					prefer_vec_width = " -mprefer-vector-width=512";
					break;
				case HOST_CPU_TIER::X86_TIER_5:
					arch = "x86-64-v4";
					arch += " -mavx512ifma -mavx512vbmi -mavx512vbmi2 -mvaes -mavx512bitalg -mvpclmulqdq -mgfni -mavx512vnni -mavx512vpopcntdq -mavx512bf16";
					tune = "znver3";
					// override default behavior of preferring 256-bit
					prefer_vec_width = " -mprefer-vector-width=512";
					break;
#if defined(__aarch64__)
				default:
#endif
				case HOST_CPU_TIER::ARM_TIER_1:
					arch = "armv8-a";
					break;
				case HOST_CPU_TIER::ARM_TIER_2:
					arch = "armv8.1-a+fp16";
					break;
				case HOST_CPU_TIER::ARM_TIER_3:
					arch = "armv8.2-a+fp16";
					break;
				case HOST_CPU_TIER::ARM_TIER_4:
					arch = "armv8.3-a+fp16";
					break;
				case HOST_CPU_TIER::ARM_TIER_5:
					arch = "armv8.4-a+fp16";
					break;
				case HOST_CPU_TIER::ARM_TIER_6:
					arch = "armv8.5-a+fp16+fp16fml";
					break;
				case HOST_CPU_TIER::ARM_TIER_7:
					arch = "armv8.6-a+fp16+fp16fml";
					break;
			}
			
			std::string target;
			if (cpu_device.cpu_tier >= HOST_CPU_TIER::__X86_OFFSET && cpu_device.cpu_tier <= HOST_CPU_TIER::__X86_RANGE) {
				target = "x86_64";
			} else if (cpu_device.cpu_tier >= HOST_CPU_TIER::__ARM_OFFSET && cpu_device.cpu_tier <= HOST_CPU_TIER::__ARM_RANGE) {
				target = "aarch64";
			} else {
				log_error("unhandled CPU tier/arch");
				return {};
			}
			
			clang_cmd += {
				"\"" + floor::get_host_compiler() + "\"" +
				" -x " + (!build_pch ? "c++" : "c++-header") +
				" -std=gnu++2b" +
				" -target " + target + "-pc-none-floor_host_compute" \
				" -nostdinc -fbuiltin -fno-math-errno" \
				" -fPIC" /* must be relocatable */ \
				" -march=" + arch +
				(!tune.empty() ? " -mtune=" + tune : "") +
				prefer_vec_width +
				" -mcmodel=large" +
				" -DFLOOR_DEVICE_HOST_COMPUTE_IS_DEVICE -DFLOOR_DEVICE_HOST_COMPUTE" +
				// TODO/NOTE: for now, doubles are not supported
				//(!dev.double_support ? " -DFLOOR_DEVICE_NO_DOUBLE" : "")
				" -DFLOOR_DEVICE_NO_DOUBLE"
				" -fno-stack-protector"
			};
			libcxx_path += floor::get_host_base_path() + "libcxx";
			clang_path += floor::get_host_base_path() + "clang";
			floor_include_path += floor::get_host_base_path() + "include";
		} break;
	}
	
	// handle pch
	std::string pch_include;
	if (build_pch) {
		output_file_type = "pch";
	} else {
		if (options.pch) {
			pch_include = " -include-pch " + *options.pch;
		}
	}
	
	// set toolchain version define
	clang_cmd += " -DFLOOR_TOOLCHAIN_VERSION=" + std::to_string(toolchain_version) + "u";
	
	// add device information
	// -> this adds both a "=" value definition (that is used for enums in device_info.hpp) and a non-valued "_" definition (used for #ifdef's)
	const auto vendor_str = vendor_to_string(dev.vendor);
	const auto platform_vendor_str = vendor_to_string(dev.platform_vendor);
	const auto type_str = (dev.is_gpu() ? "GPU" : (dev.is_cpu() ? "CPU" : "UNKNOWN"));
	std::string os_str;
	if (!options.ignore_runtime_info) {
		os_str = (options.target != TARGET::AIR ?
				  // non Metal/AIR targets (aka the usual/proper handling)
#if defined(__APPLE__)
#if defined(FLOOR_IOS)
				  "IOS"
#elif defined(FLOOR_VISIONOS)
				  "VISIONOS"
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
				  // Metal/AIR specific handling, target OS is dependent on feature set
				  : (((const metal_device&)dev).family_type == metal_device::FAMILY_TYPE::APPLE ? "IOS" : "OSX"));
	} else {
		os_str = "UNKNOWN";
	}
	
	clang_cmd += " -DFLOOR_DEVICE_INFO_VENDOR="s + vendor_str;
	clang_cmd += " -DFLOOR_DEVICE_INFO_VENDOR_"s + vendor_str;
	clang_cmd += " -DFLOOR_DEVICE_INFO_PLATFORM_VENDOR="s + platform_vendor_str;
	clang_cmd += " -DFLOOR_DEVICE_INFO_PLATFORM_VENDOR_"s + platform_vendor_str;
	clang_cmd += " -DFLOOR_DEVICE_INFO_TYPE="s + type_str;
	clang_cmd += " -DFLOOR_DEVICE_INFO_TYPE_"s + type_str;
	clang_cmd += " -DFLOOR_DEVICE_INFO_OS="s + os_str;
	clang_cmd += " -DFLOOR_DEVICE_INFO_OS_"s + os_str;
	
	std::string os_version_str = " -DFLOOR_DEVICE_INFO_OS_VERSION=0";
	os_version_str += " -DFLOOR_DEVICE_INFO_OS_VERSION_0";
#if defined(__APPLE__) // TODO: do this on linux as well? if so, how/what?
	if (!options.ignore_runtime_info) {
		const auto os_version = std::to_string(darwin_helper::get_system_version());
		os_version_str = " -DFLOOR_DEVICE_INFO_OS_VERSION="s + os_version;
		os_version_str += " -DFLOOR_DEVICE_INFO_OS_VERSION_"s + os_version;
	}
#elif defined(__WINDOWS__)
	if (!options.ignore_runtime_info) {
		const auto win_version = core::get_windows_version();
		// set as: 100 (Win 10), 110 (Win 11)
		const auto win_version_str = std::to_string(win_version.x * 10 + win_version.y);
		os_version_str = " -DFLOOR_DEVICE_INFO_OS_VERSION="s + win_version_str;
		os_version_str += " -DFLOOR_DEVICE_INFO_OS_VERSION_"s + win_version_str;
	}
#endif
	clang_cmd += os_version_str;
	
	// assume all GPUs have fma support
	bool has_fma = dev.is_gpu();
	if(dev.is_cpu() && !options.ignore_runtime_info) {
		// if device is a CPU, need to check cpuid on x86, or check if CPU is armv8
		has_fma = core::cpu_has_fma();
	}
	const auto has_fma_str = std::to_string(has_fma);
	clang_cmd += " -DFLOOR_DEVICE_INFO_HAS_FMA="s + has_fma_str;
	clang_cmd += " -DFLOOR_DEVICE_INFO_HAS_FMA_"s + has_fma_str;
	
	// base and extended 64-bit atomics support
	const auto has_base_64_bit_atomics_str = std::to_string(dev.basic_64_bit_atomics_support);
	const auto has_extended_64_bit_atomics_str = std::to_string(dev.extended_64_bit_atomics_support);
	const auto has_base_32_bit_float_atomics_str = std::to_string(dev.basic_32_bit_float_atomics_support);
	clang_cmd += " -DFLOOR_DEVICE_INFO_HAS_64_BIT_ATOMICS="s + has_base_64_bit_atomics_str;
	clang_cmd += " -DFLOOR_DEVICE_INFO_HAS_64_BIT_ATOMICS_"s + has_base_64_bit_atomics_str;
	clang_cmd += " -DFLOOR_DEVICE_INFO_HAS_NATIVE_EXTENDED_64_BIT_ATOMICS="s + has_extended_64_bit_atomics_str;
	clang_cmd += " -DFLOOR_DEVICE_INFO_HAS_NATIVE_EXTENDED_64_BIT_ATOMICS_"s + has_extended_64_bit_atomics_str;
	clang_cmd += " -DFLOOR_DEVICE_INFO_HAS_32_BIT_FLOAT_ATOMICS="s + has_base_32_bit_float_atomics_str;
	clang_cmd += " -DFLOOR_DEVICE_INFO_HAS_32_BIT_FLOAT_ATOMICS_"s + has_base_32_bit_float_atomics_str;
	
	// has device actual dedicated local memory
	const auto has_dedicated_local_memory_str = std::to_string(dev.local_mem_dedicated);
	clang_cmd += " -DFLOOR_DEVICE_INFO_HAS_DEDICATED_LOCAL_MEMORY="s + has_dedicated_local_memory_str;
	clang_cmd += " -DFLOOR_DEVICE_INFO_HAS_DEDICATED_LOCAL_MEMORY_"s + has_dedicated_local_memory_str;
	clang_cmd += " -DFLOOR_DEVICE_INFO_DEDICATED_LOCAL_MEMORY=" + std::to_string(dev.local_mem_size) + "u";
	
	// primitive ID / barycentric coord support
	const auto has_primitive_id_support_str = std::to_string(primitive_id_support);
	const auto has_barycentric_coord_support_str = std::to_string(barycentric_coord_support);
	clang_cmd += " -DFLOOR_DEVICE_INFO_HAS_PRIMITIVE_ID="s + has_primitive_id_support_str;
	clang_cmd += " -DFLOOR_DEVICE_INFO_HAS_PRIMITIVE_ID_"s + has_primitive_id_support_str;
	clang_cmd += " -DFLOOR_DEVICE_INFO_HAS_BARYCENTRIC_COORD="s + has_barycentric_coord_support_str;
	clang_cmd += " -DFLOOR_DEVICE_INFO_HAS_BARYCENTRIC_COORD_"s + has_barycentric_coord_support_str;
	if (primitive_id_support) {
		clang_cmd += " -Xclang -graphics-primitive-id";
	}
	if (barycentric_coord_support) {
		clang_cmd += " -Xclang -graphics-barycentric-coord";
	}
	
	// id/size ranges
	// NOTE: ranges are specified as [min, max), i.e. min is inclusive, max is exclusive
	uint2 global_id_range { 0u, ~0u };
	uint2 global_size_range { 1u, ~0u };
	uint2 group_id_range { 0u, ~0u };
	uint2 group_size_range { 1u, ~0u };
	
	// if the device has actual info about this, use that instead of the defaults
	const auto max_global_size = dev.max_global_size.max_element();
	if (max_global_size > 0) {
		global_id_range.y = (max_global_size >= 0xFFFFFFFFull ? ~0u : uint32_t(max_global_size));
		global_size_range.y = (max_global_size >= 0xFFFFFFFFull ? ~0u : uint32_t(max_global_size + 1));
	}
	
	// default to the common max of 1024
	uint2 local_id_range { 0u, 1024u };
	uint2 local_size_range { 1u, 1025u };
	if (dev.max_total_local_size != 0) {
		local_id_range.y = dev.max_total_local_size;
		local_size_range.y = dev.max_total_local_size + 1;
	}
	
	const auto max_group_size = dev.max_group_size.max_element();
	if(max_group_size > 0) {
		group_id_range.y = max_group_size;
		group_size_range.y = (max_group_size != ~0u ? max_group_size + 1 : ~0u);
	}
	
	clang_cmd += " -DFLOOR_DEVICE_INFO_GLOBAL_ID_RANGE_MIN=" + std::to_string(global_id_range.x) + "u";
	clang_cmd += " -DFLOOR_DEVICE_INFO_GLOBAL_ID_RANGE_MAX=" + std::to_string(global_id_range.y) + "u";
	clang_cmd += " -DFLOOR_DEVICE_INFO_GLOBAL_SIZE_RANGE_MIN=" + std::to_string(global_size_range.x) + "u";
	clang_cmd += " -DFLOOR_DEVICE_INFO_GLOBAL_SIZE_RANGE_MAX=" + std::to_string(global_size_range.y) + "u";
	clang_cmd += " -DFLOOR_DEVICE_INFO_LOCAL_ID_RANGE_MIN=" + std::to_string(local_id_range.x) + "u";
	clang_cmd += " -DFLOOR_DEVICE_INFO_LOCAL_ID_RANGE_MAX=" + std::to_string(local_id_range.y) + "u";
	clang_cmd += " -DFLOOR_DEVICE_INFO_LOCAL_SIZE_RANGE_MIN=" + std::to_string(local_size_range.x) + "u";
	clang_cmd += " -DFLOOR_DEVICE_INFO_LOCAL_SIZE_RANGE_MAX=" + std::to_string(local_size_range.y) + "u";
	clang_cmd += " -DFLOOR_DEVICE_INFO_GROUP_ID_RANGE_MIN=" + std::to_string(group_id_range.x) + "u";
	clang_cmd += " -DFLOOR_DEVICE_INFO_GROUP_ID_RANGE_MAX=" + std::to_string(group_id_range.y) + "u";
	clang_cmd += " -DFLOOR_DEVICE_INFO_GROUP_SIZE_RANGE_MIN=" + std::to_string(group_size_range.x) + "u";
	clang_cmd += " -DFLOOR_DEVICE_INFO_GROUP_SIZE_RANGE_MAX=" + std::to_string(group_size_range.y) + "u";
	
	// handle device simd width
	uint32_t simd_width = dev.simd_width;
	uint2 simd_range = dev.simd_range;
	if(simd_width == 0 && !options.ignore_runtime_info) {
		// try to figure out the simd width of the device if it's 0
		if(dev.is_gpu()) {
			switch(dev.vendor) {
				case VENDOR::NVIDIA: simd_width = 32; simd_range = { simd_width, simd_width }; break;
				case VENDOR::INTEL: simd_width = 16; simd_range = { 8, 32 }; break;
				case VENDOR::APPLE: simd_width = 32; simd_range = { simd_width, simd_width }; break;
				// NOTE: can't handle AMD any more, since we got have either fixed SIMD32 or SIMD64
				// else: unknown GPU
				default: break;
			}
		}
		else if(dev.is_cpu()) {
			// always at least 4 (SSE, newer NEON), 8-wide if AVX/AVX2, 16-wide if AVX-512
			simd_width = (core::cpu_has_avx() ? (core::cpu_has_avx512() ? 16 : 8) : 4);
			simd_range = { 1, simd_width };
		}
	}
	const auto simd_width_str = std::to_string(simd_width);
	clang_cmd += " -DFLOOR_DEVICE_INFO_SIMD_WIDTH="s + simd_width_str + "u";
	clang_cmd += " -DFLOOR_DEVICE_INFO_SIMD_WIDTH_MIN="s + std::to_string(simd_range.x) + "u";
	clang_cmd += " -DFLOOR_DEVICE_INFO_SIMD_WIDTH_MAX="s + std::to_string(simd_range.y) + "u";
	clang_cmd += " -DFLOOR_DEVICE_INFO_SIMD_WIDTH_"s + simd_width_str;
	
	if (dev.sub_group_support && !disable_sub_groups) {
		// sub-group range handling, now that we know local sizes and SIMD-width
		// NOTE: no known device currently has a SIMD-width larger than 64
		// NOTE: correspondence if a sub-group was a work-group:
		//       sub-group local id ^= local id
		//       sub-group size     ^= local size
		//       sub-group id       ^= group id
		//       num sub-groups     ^= group size
		uint2 sub_group_local_id_range { 0u, 64u };
		uint2 sub_group_size_range { 1u, local_size_range.y /* not larger than this */ };
		uint2 sub_group_id_range { 0u, local_id_range.y /* not larger than this */ };
		uint2 num_sub_groups_range { 1u, local_size_range.y /* not larger than this */ };
		
		if (dev.simd_width > 1u) {
			sub_group_local_id_range.y = dev.simd_range.y; // [0, SIMD-width)
			sub_group_size_range = { dev.simd_range.x, dev.simd_range.y + 1 }; // [min SIMD-width, max SIMD-width]
			if (dev.simd_range.x == dev.simd_range.y) {
				// device with constant SIMD-width
				sub_group_id_range.y = (local_id_range.y / dev.simd_range.y); // [0, max-local-size / max-SIMD-width)
				num_sub_groups_range.y = local_id_range.y / dev.simd_range.y + 1; // [1, max-local-size / max-SIMD-width + 1)
			} else {
				// device with variable SIMD-width
				sub_group_id_range.y = (local_id_range.y / dev.simd_range.x); // [0, max-local-size / min-SIMD-width)
				num_sub_groups_range.y = local_id_range.y / dev.simd_range.x + 1; // [1, max-local-size / min-SIMD-width + 1)
			}
		}
		
		clang_cmd += " -DFLOOR_DEVICE_INFO_SUB_GROUP_ID_RANGE_MIN=" + std::to_string(sub_group_id_range.x) + "u";
		clang_cmd += " -DFLOOR_DEVICE_INFO_SUB_GROUP_ID_RANGE_MAX=" + std::to_string(sub_group_id_range.y) + "u";
		clang_cmd += " -DFLOOR_DEVICE_INFO_SUB_GROUP_LOCAL_ID_RANGE_MIN=" + std::to_string(sub_group_local_id_range.x) + "u";
		clang_cmd += " -DFLOOR_DEVICE_INFO_SUB_GROUP_LOCAL_ID_RANGE_MAX=" + std::to_string(sub_group_local_id_range.y) + "u";
		clang_cmd += " -DFLOOR_DEVICE_INFO_SUB_GROUP_SIZE_RANGE_MIN=" + std::to_string(sub_group_size_range.x) + "u";
		clang_cmd += " -DFLOOR_DEVICE_INFO_SUB_GROUP_SIZE_RANGE_MAX=" + std::to_string(sub_group_size_range.y) + "u";
		clang_cmd += " -DFLOOR_DEVICE_INFO_NUM_SUB_GROUPS_RANGE_MIN=" + std::to_string(num_sub_groups_range.x) + "u";
		clang_cmd += " -DFLOOR_DEVICE_INFO_NUM_SUB_GROUPS_RANGE_MAX=" + std::to_string(num_sub_groups_range.y) + "u";
	}
	
	
	// handle sub-group support
	if(dev.sub_group_support && !disable_sub_groups) {
		clang_cmd += " -DFLOOR_DEVICE_INFO_HAS_SUB_GROUPS=1 -DFLOOR_DEVICE_INFO_HAS_SUB_GROUPS_1";
	} else {
		clang_cmd += " -DFLOOR_DEVICE_INFO_HAS_SUB_GROUPS=0 -DFLOOR_DEVICE_INFO_HAS_SUB_GROUPS_0";
	}
	
	// handle sub-group shuffle support
	if(dev.sub_group_shuffle_support && !disable_sub_groups) {
		clang_cmd += " -DFLOOR_DEVICE_INFO_HAS_SUB_GROUP_SHUFFLE=1 -DFLOOR_DEVICE_INFO_HAS_SUB_GROUP_SHUFFLE_1";
	} else {
		clang_cmd += " -DFLOOR_DEVICE_INFO_HAS_SUB_GROUP_SHUFFLE=0 -DFLOOR_DEVICE_INFO_HAS_SUB_GROUP_SHUFFLE_0";
	}
	
	// handle cooperative kernel support
	if(dev.cooperative_kernel_support) {
		clang_cmd += " -DFLOOR_DEVICE_INFO_HAS_COOPERATIVE_KERNEL=1 -DFLOOR_DEVICE_INFO_HAS_COOPERATIVE_KERNEL_1";
	} else {
		clang_cmd += " -DFLOOR_DEVICE_INFO_HAS_COOPERATIVE_KERNEL=0 -DFLOOR_DEVICE_INFO_HAS_COOPERATIVE_KERNEL_0";
	}
	
	// handle image support
	const auto has_image_support = std::to_string(dev.image_support);
	const auto has_image_depth_support = std::to_string(dev.image_depth_support);
	const auto has_image_depth_write_support = std::to_string(dev.image_depth_write_support);
	const auto has_image_msaa_support = std::to_string(dev.image_msaa_support);
	const auto has_image_msaa_write_support = std::to_string(dev.image_msaa_write_support);
	const auto has_image_msaa_array_support = std::to_string(dev.image_msaa_array_support);
	const auto has_image_msaa_array_write_support = std::to_string(dev.image_msaa_array_write_support);
	const auto has_image_cube_support = std::to_string(dev.image_cube_support);
	const auto has_image_cube_write_support = std::to_string(dev.image_cube_write_support);
	const auto has_image_cube_array_support = std::to_string(dev.image_cube_array_support);
	const auto has_image_cube_array_write_support = std::to_string(dev.image_cube_array_write_support);
	const auto has_image_mipmap_support = std::to_string(dev.image_mipmap_support);
	const auto has_image_mipmap_write_support = std::to_string(dev.image_mipmap_write_support);
	const auto has_image_offset_read_support = std::to_string(dev.image_offset_read_support);
	const auto has_image_offset_write_support = std::to_string(dev.image_offset_write_support);
	const auto has_image_depth_compare_support = std::to_string(dev.image_depth_compare_support);
	const auto has_image_gather_support = std::to_string(dev.image_gather_support);
	const auto has_image_read_write_support = std::to_string(dev.image_read_write_support);
	clang_cmd += " -DFLOOR_DEVICE_INFO_HAS_IMAGE_SUPPORT="s + has_image_support;
	clang_cmd += " -DFLOOR_DEVICE_INFO_HAS_IMAGE_SUPPORT_"s + has_image_support;
	clang_cmd += " -DFLOOR_DEVICE_INFO_HAS_IMAGE_DEPTH_SUPPORT="s + has_image_depth_support;
	clang_cmd += " -DFLOOR_DEVICE_INFO_HAS_IMAGE_DEPTH_SUPPORT_"s + has_image_depth_support;
	clang_cmd += " -DFLOOR_DEVICE_INFO_HAS_IMAGE_DEPTH_WRITE_SUPPORT="s + has_image_depth_write_support;
	clang_cmd += " -DFLOOR_DEVICE_INFO_HAS_IMAGE_DEPTH_WRITE_SUPPORT_"s + has_image_depth_write_support;
	clang_cmd += " -DFLOOR_DEVICE_INFO_HAS_IMAGE_MSAA_SUPPORT="s + has_image_msaa_support;
	clang_cmd += " -DFLOOR_DEVICE_INFO_HAS_IMAGE_MSAA_SUPPORT_"s + has_image_msaa_support;
	clang_cmd += " -DFLOOR_DEVICE_INFO_HAS_IMAGE_MSAA_WRITE_SUPPORT="s + has_image_msaa_write_support;
	clang_cmd += " -DFLOOR_DEVICE_INFO_HAS_IMAGE_MSAA_WRITE_SUPPORT_"s + has_image_msaa_write_support;
	clang_cmd += " -DFLOOR_DEVICE_INFO_HAS_IMAGE_MSAA_ARRAY_SUPPORT="s + has_image_msaa_array_support;
	clang_cmd += " -DFLOOR_DEVICE_INFO_HAS_IMAGE_MSAA_ARRAY_SUPPORT_"s + has_image_msaa_array_support;
	clang_cmd += " -DFLOOR_DEVICE_INFO_HAS_IMAGE_MSAA_ARRAY_WRITE_SUPPORT="s + has_image_msaa_array_write_support;
	clang_cmd += " -DFLOOR_DEVICE_INFO_HAS_IMAGE_MSAA_ARRAY_WRITE_SUPPORT_"s + has_image_msaa_array_write_support;
	clang_cmd += " -DFLOOR_DEVICE_INFO_HAS_IMAGE_CUBE_SUPPORT="s + has_image_cube_support;
	clang_cmd += " -DFLOOR_DEVICE_INFO_HAS_IMAGE_CUBE_SUPPORT_"s + has_image_cube_support;
	clang_cmd += " -DFLOOR_DEVICE_INFO_HAS_IMAGE_CUBE_WRITE_SUPPORT="s + has_image_cube_write_support;
	clang_cmd += " -DFLOOR_DEVICE_INFO_HAS_IMAGE_CUBE_WRITE_SUPPORT_"s + has_image_cube_write_support;
	clang_cmd += " -DFLOOR_DEVICE_INFO_HAS_IMAGE_CUBE_ARRAY_SUPPORT="s + has_image_cube_array_support;
	clang_cmd += " -DFLOOR_DEVICE_INFO_HAS_IMAGE_CUBE_ARRAY_SUPPORT_"s + has_image_cube_array_support;
	clang_cmd += " -DFLOOR_DEVICE_INFO_HAS_IMAGE_CUBE_ARRAY_WRITE_SUPPORT="s + has_image_cube_array_write_support;
	clang_cmd += " -DFLOOR_DEVICE_INFO_HAS_IMAGE_CUBE_ARRAY_WRITE_SUPPORT_"s + has_image_cube_array_write_support;
	clang_cmd += " -DFLOOR_DEVICE_INFO_HAS_IMAGE_MIPMAP_SUPPORT="s + has_image_mipmap_support;
	clang_cmd += " -DFLOOR_DEVICE_INFO_HAS_IMAGE_MIPMAP_SUPPORT_"s + has_image_mipmap_support;
	clang_cmd += " -DFLOOR_DEVICE_INFO_HAS_IMAGE_MIPMAP_WRITE_SUPPORT="s + has_image_mipmap_write_support;
	clang_cmd += " -DFLOOR_DEVICE_INFO_HAS_IMAGE_MIPMAP_WRITE_SUPPORT_"s + has_image_mipmap_write_support;
	clang_cmd += " -DFLOOR_DEVICE_INFO_HAS_IMAGE_OFFSET_READ_SUPPORT="s + has_image_mipmap_support;
	clang_cmd += " -DFLOOR_DEVICE_INFO_HAS_IMAGE_OFFSET_READ_SUPPORT_"s + has_image_mipmap_support;
	clang_cmd += " -DFLOOR_DEVICE_INFO_HAS_IMAGE_OFFSET_WRITE_SUPPORT="s + has_image_mipmap_write_support;
	clang_cmd += " -DFLOOR_DEVICE_INFO_HAS_IMAGE_OFFSET_WRITE_SUPPORT_"s + has_image_mipmap_write_support;
	clang_cmd += " -DFLOOR_DEVICE_INFO_HAS_IMAGE_DEPTH_COMPARE_SUPPORT="s + has_image_depth_compare_support;
	clang_cmd += " -DFLOOR_DEVICE_INFO_HAS_IMAGE_DEPTH_COMPARE_SUPPORT_"s + has_image_depth_compare_support;
	clang_cmd += " -DFLOOR_DEVICE_INFO_HAS_IMAGE_GATHER_SUPPORT="s + has_image_gather_support;
	clang_cmd += " -DFLOOR_DEVICE_INFO_HAS_IMAGE_GATHER_SUPPORT_"s + has_image_gather_support;
	clang_cmd += " -DFLOOR_DEVICE_INFO_HAS_IMAGE_READ_WRITE_SUPPORT="s + has_image_read_write_support;
	clang_cmd += " -DFLOOR_DEVICE_INFO_HAS_IMAGE_READ_WRITE_SUPPORT_"s + has_image_read_write_support;
	
	IMAGE_CAPABILITY img_caps { IMAGE_CAPABILITY::NONE };
	if(dev.image_support) img_caps |= IMAGE_CAPABILITY::BASIC;
	if(dev.image_depth_support) img_caps |= IMAGE_CAPABILITY::DEPTH_READ;
	if(dev.image_depth_write_support) img_caps |= IMAGE_CAPABILITY::DEPTH_WRITE;
	if(dev.image_msaa_support) img_caps |= IMAGE_CAPABILITY::MSAA_READ;
	if(dev.image_msaa_write_support) img_caps |= IMAGE_CAPABILITY::MSAA_WRITE;
	if(dev.image_msaa_array_support) img_caps |= IMAGE_CAPABILITY::MSAA_ARRAY_READ;
	if(dev.image_msaa_array_write_support) img_caps |= IMAGE_CAPABILITY::MSAA_ARRAY_WRITE;
	if(dev.image_cube_support) img_caps |= IMAGE_CAPABILITY::CUBE_READ;
	if(dev.image_cube_write_support) img_caps |= IMAGE_CAPABILITY::CUBE_WRITE;
	if(dev.image_cube_array_support) img_caps |= IMAGE_CAPABILITY::CUBE_ARRAY_READ;
	if(dev.image_cube_array_write_support) img_caps |= IMAGE_CAPABILITY::CUBE_ARRAY_WRITE;
	if(dev.image_mipmap_support) img_caps |= IMAGE_CAPABILITY::MIPMAP_READ;
	if(dev.image_mipmap_write_support) img_caps |= IMAGE_CAPABILITY::MIPMAP_WRITE;
	if(dev.image_offset_read_support) img_caps |= IMAGE_CAPABILITY::OFFSET_READ;
	if(dev.image_offset_write_support) img_caps |= IMAGE_CAPABILITY::OFFSET_WRITE;
	if(dev.image_depth_compare_support) img_caps |= IMAGE_CAPABILITY::DEPTH_COMPARE;
	if(dev.image_gather_support) img_caps |= IMAGE_CAPABILITY::GATHER;
	if(dev.image_read_write_support) img_caps |= IMAGE_CAPABILITY::READ_WRITE;
	clang_cmd += " -Xclang -floor-image-capabilities=" + std::to_string((std::underlying_type_t<IMAGE_CAPABILITY>)img_caps);
	
	clang_cmd += " -DFLOOR_DEVICE_INFO_MAX_MIP_LEVELS="s + std::to_string(dev.max_mip_levels) + "u";
	
	// indirect command support
	const auto has_indirect_cmd_str = std::to_string(dev.indirect_command_support);
	const auto has_indirect_compute_cmd_str = std::to_string(dev.indirect_compute_command_support);
	const auto has_indirect_render_cmd_str = std::to_string(dev.indirect_render_command_support);
	clang_cmd += " -DFLOOR_DEVICE_INFO_INDIRECT_COMMAND_SUPPORT="s + has_indirect_cmd_str;
	clang_cmd += " -DFLOOR_DEVICE_INFO_INDIRECT_COMMAND_SUPPORT_"s + has_indirect_cmd_str;
	clang_cmd += " -DFLOOR_DEVICE_INFO_INDIRECT_COMPUTE_COMMAND_SUPPORT="s + has_indirect_compute_cmd_str;
	clang_cmd += " -DFLOOR_DEVICE_INFO_INDIRECT_COMPUTE_COMMAND_SUPPORT_"s + has_indirect_compute_cmd_str;
	clang_cmd += " -DFLOOR_DEVICE_INFO_INDIRECT_RENDER_COMMAND_SUPPORT="s + has_indirect_render_cmd_str;
	clang_cmd += " -DFLOOR_DEVICE_INFO_INDIRECT_RENDER_COMMAND_SUPPORT_"s + has_indirect_render_cmd_str;
	
	// tessellation support
	const auto has_tessellation_str = std::to_string(dev.tessellation_support);
	clang_cmd += " -DFLOOR_DEVICE_INFO_TESSELLATION_SUPPORT="s + has_tessellation_str;
	clang_cmd += " -DFLOOR_DEVICE_INFO_TESSELLATION_SUPPORT_"s + has_tessellation_str;
	clang_cmd += " -DFLOOR_DEVICE_INFO_MAX_TESSELLATION_FACTOR="s + std::to_string(dev.max_tessellation_factor) + "u";
	
	// argument buffer support
	const auto has_arg_buffer_str = std::to_string(dev.argument_buffer_support);
	const auto has_arg_buffer_image_str = std::to_string(dev.argument_buffer_image_support);
	clang_cmd += " -DFLOOR_DEVICE_INFO_HAS_ARGUMENT_BUFFER_SUPPORT="s + has_arg_buffer_str;
	clang_cmd += " -DFLOOR_DEVICE_INFO_HAS_ARGUMENT_BUFFER_SUPPORT_"s + has_arg_buffer_str;
	clang_cmd += " -DFLOOR_DEVICE_INFO_HAS_ARGUMENT_BUFFER_IMAGE_SUPPORT="s + has_arg_buffer_image_str;
	clang_cmd += " -DFLOOR_DEVICE_INFO_HAS_ARGUMENT_BUFFER_IMAGE_SUPPORT_"s + has_arg_buffer_image_str;
	
	// set param workaround define
	if(dev.param_workaround) {
		clang_cmd += " -DFLOOR_DEVICE_PARAM_WORKAROUND=1";
	}
	
	// handle assert support
	const auto assert_str = std::to_string(options.enable_assert);
	clang_cmd += " -DFLOOR_ASSERT=" + assert_str;
	clang_cmd += " -DFLOOR_ASSERT_" + assert_str;
	
	// floor function info
	std::string function_info_file_name;
	if (!build_pch) {
		function_info_file_name = core::create_tmp_file_name("ffi", ".txt");
		clang_cmd += " -Xclang -floor-function-info=" + function_info_file_name;
	}
	
	// target specific compute info
	switch(options.target) {
		case TARGET::PTX:
			// set CUDA sm and PTX version
			clang_cmd += " -DFLOOR_DEVICE_INFO_CUDA_SM=" + sm_version;
			clang_cmd += " -DFLOOR_DEVICE_INFO_CUDA_SM_AA=" + sm_aa_enabled;
			clang_cmd += " -DFLOOR_DEVICE_INFO_CUDA_PTX=" + std::to_string(ptx_version);
			break;
		case TARGET::SPIRV_VULKAN: {
			const auto& vk_device = (const vulkan_device&)dev;
			const auto has_float16_support = std::to_string(vk_device.float16_support);
			clang_cmd += " -DFLOOR_DEVICE_INFO_VULKAN_HAS_FLOAT16_SUPPORT="s + has_float16_support;
			clang_cmd += " -DFLOOR_DEVICE_INFO_VULKAN_HAS_FLOAT16_SUPPORT_"s + has_float16_support;
			break;
		}
		default: break;
	}

	// emit line info if debug mode is enabled (unless this is spir where we'd better not emit this)
	std::string metal_debug_preprocess, metal_final_output_file_type;
	if (((floor::get_toolchain_debug() && !options.ignore_runtime_info) || options.debug.emit_debug_info) &&
		options.target != TARGET::SPIR) {
		clang_cmd += " -gline-tables-only";
		
		if (options.target == TARGET::AIR) {
			clang_cmd += " -gstrict-dwarf -gcolumn-info -frecord-command-line";
			if (options.debug.preprocess_condense && !build_pch) {
				metal_debug_preprocess = " -E -P";
				if (options.debug.preprocess_preserve_comments) {
					metal_debug_preprocess += " -C";
				}
				
				metal_final_output_file_type = output_file_type;
				output_file_type = "cpp";
			}
		}
	}
	
	// default disabled warning flags
	static constexpr const char disabled_warning_flags[] {
		// let's start with everything
		" -Weverything"
		// remove compat warnings
		" -Wno-c++98-compat -Wno-c++98-compat-pedantic"
		" -Wno-c++11-compat -Wno-c++11-compat-pedantic"
		" -Wno-c++14-compat -Wno-c++14-compat-pedantic"
		" -Wno-c++17-compat -Wno-c++17-compat-pedantic"
		" -Wno-c++20-compat -Wno-c++20-compat-pedantic -Wno-c++20-extensions"
		" -Wno-c++2b-compat -Wno-c++2b-compat-pedantic -Wno-c++2b-extensions"
		" -Wno-c99-extensions -Wno-c11-extensions"
		" -Wno-gcc-compat -Wno-gnu"
		// in case we're using warning options that aren't supported by other clang versions
		" -Wno-unknown-warning-option"
		// really don't want to be too pedantic
		" -Wno-old-style-cast -Wno-date-time -Wno-system-headers -Wno-header-hygiene"
		// again: not too pedantic, also useful language features
		" -Wno-nested-anon-types -Wno-global-constructors -Wno-exit-time-destructors"
		// usually conflicting with the other switch/case warning, so disable it
		" -Wno-switch-enum"
		// don't warn when using macros prefixed with "__" or "_"
		" -Wno-reserved-id-macro -Wno-reserved-identifier"
		// ignore "explicit move to avoid copying on older compilers" warning
		" -Wno-return-std-move-in-c++11"
		// ignore documentation warnings
		" -Wno-documentation -Wno-documentation-unknown-command"
		// don't complain "#pragma once" when compiling an .hpp as a main file
		" -Wno-pragma-once-outside-header"
		// end
		" "
	};
	
	// add generic flags/options that are always used
	std::string compiled_file_or_code;
	std::string include_flags {
		" -isystem \"" + libcxx_path + "\"" +
		" -isystem \"" + clang_path + "\"" +
		" -isystem \"" + floor_include_path + "\"" +
		" -include floor/device/backend/common.hpp" +
		pch_include
	};
	clang_cmd += {
#if defined(FLOOR_DEBUG)
		" -DFLOOR_DEBUG"
#endif
		" -DFLOOR_DEVICE"
		" -DFLOOR_NO_MATH_STR"s +
		(!metal_preprocess ? include_flags : "") +
		(options.target != TARGET::HOST_COMPUTE_CPU ? " -fno-pic" : "") +
		" -fno-exceptions -fno-unwind-tables -fno-asynchronous-unwind-tables -fno-addrsig"
		" -fno-rtti -fstrict-aliasing -ffast-math -funroll-loops -Ofast -ffp-contract=fast"
		" -Xclang -menable-unsafe-fp-math"
		// increase limit from 16 to 64, this "fixes" some forced unrolling
		" -mllvm -rotation-max-header-size=64" +
		// use integrated cc1 on latest toolchain
		" -fintegrated-cc1" +
		// use legacy pass manager on latest toolchain for now (TODO: use new pass manager!)
		" -flegacy-pass-manager" +
		// disable argument round-trip on latest toolchain
		" -Xclang -no-round-trip-args" +
		// force enable half/fp16 support on all targets
		" -Xclang -fnative-half-type -Xclang -fnative-half-arguments-and-returns -Xclang -fallow-half-arguments-and-returns" +
		// note that enabling warnings costs a significant amount of compilation time
		(options.enable_warnings ? " -Weverything" : " ") +
		disabled_warning_flags +
		options.cli +
		" -m64"
	};
	if (!build_pch) {
		compiled_file_or_code = core::create_tmp_file_name("", '.' + output_file_type);
		if (options.target != TARGET::HOST_COMPUTE_CPU && options.target != TARGET::PTX) {
			clang_cmd += " -emit-llvm";
		}
		if (!metal_preprocess) {
			if (options.target == TARGET::PTX) {
				clang_cmd += " -S";
			} else {
				clang_cmd += " -c";
			}
			// else: none
			clang_cmd += " -o " + compiled_file_or_code + " " + input;
		} else {
			metal_debug_preprocess += " -o " + compiled_file_or_code + " " + input;
		}
	} else {
		compiled_file_or_code = *options.pch;
		clang_cmd += " \"" + floor_include_path + "/floor/device/backend/common.hpp\"";
		clang_cmd += " -o " + compiled_file_or_code;
	}
	
	// Metal: when we do a two-step compilation (.cpp -> .ii -> .metallib),
	// build the final preprocess and compile commands here
	std::string metal_pp_compile_cmd, metal_final_output_file;
	if (metal_preprocess) {
		metal_final_output_file = core::create_tmp_file_name("", '.' + metal_final_output_file_type);
		metal_pp_compile_cmd = clang_cmd + metal_emit_format + " -Wno-everything";
		metal_pp_compile_cmd += " -emit-llvm -c -o " + metal_final_output_file + " " + compiled_file_or_code;
#if !defined(_MSC_VER)
		metal_pp_compile_cmd += " 2>&1";
#endif
		clang_cmd += include_flags + metal_debug_preprocess;
	} else if (options.target == TARGET::AIR) {
		clang_cmd += metal_emit_format;
	}
	
	// on sane systems, redirect errors to stdout so that we can grab them
#if !defined(_MSC_VER)
	clang_cmd += " 2>&1";
#endif
	
	// compile
	if(floor::get_toolchain_log_commands() &&
	   !options.silence_debug_output) {
		log_debug("clang cmd: $", clang_cmd);
		if (metal_preprocess) {
			log_debug("Metal final cmd: $", metal_pp_compile_cmd);
		}
	}
	std::string compilation_output;
	core::system(clang_cmd, compilation_output);
	// check if the output contains an error string (yes, a bit ugly, but it works for now - can't actually check the return code)
	if(compilation_output.find(" error: ") != std::string::npos ||
	   compilation_output.find(" errors:") != std::string::npos) {
		log_error("compilation failed! failed cmd was:\n$", clang_cmd);
		log_error("compilation errors:\n$", compilation_output);
		return {};
	}
	// also print the output if it is non-empty
	if(compilation_output != "" &&
	   !options.silence_debug_output) {
		log_debug("compilation output:\n$", compilation_output);
	}
	
	// Metal: final build step when pre-processing is enabled
	if (metal_preprocess) {
		// compile pre-processed file into final .metallib
		compilation_output = "";
		core::system(metal_pp_compile_cmd, compilation_output);
		if (compilation_output.find(" error: ") != std::string::npos ||
			compilation_output.find(" errors:") != std::string::npos) {
			log_error("final Metal compilation failed! failed cmd was:\n$", metal_pp_compile_cmd);
			log_error("final Metal compilation errors:\n$", compilation_output);
			return {};
		}
		if (compilation_output != "" &&
			!options.silence_debug_output) {
			log_debug("final Metal compilation output:\n$", compilation_output);
		}
		// switch out output file (.ii -> .metallib)
		compiled_file_or_code = metal_final_output_file;
	}
	
	// grab floor function info and create the internal per-function info
	std::vector<function_info> functions;
	if (!build_pch) {
		if (!create_floor_function_info(function_info_file_name, functions, toolchain_version)) {
			log_error("failed to create internal floor function info");
			return {};
		}
		if (!floor::get_toolchain_keep_temp()) {
			std::error_code ec {};
			(void)std::filesystem::remove(function_info_file_name, ec);
		}
	}
	
	// final target specific processing/compilation
	if (!build_pch) {
		if (options.target == TARGET::SPIR) {
			std::string spir_bc_data;
			if (!file_io::file_to_string(compiled_file_or_code, spir_bc_data)) {
				log_error("failed to read SPIR 1.2 .bc file");
				return {};
			}
			
			// cleanup
			if (!floor::get_toolchain_keep_temp()) {
				std::error_code ec {};
				(void)std::filesystem::remove(compiled_file_or_code, ec);
			}
			
			// move spir data
			compiled_file_or_code.swap(spir_bc_data);
		} else if (options.target == TARGET::AIR) {
			// nop, final processing will be done in metal_context
		} else if (options.target == TARGET::PTX) {
			// check if we have sane output
			std::string ptx_code;
			if (!file_io::file_to_string(compiled_file_or_code, ptx_code)) {
				log_error("PTX compilation failed!");
				return {};
			}
			// add an explicit zero terminator so that we can later on use this when loading the module (where we can't specify a size)
			ptx_code += '\0';
			
			// cleanup
			if (!floor::get_toolchain_keep_temp()) {
				std::error_code ec {};
				(void)std::filesystem::remove(compiled_file_or_code, ec);
			}
			
			if (ptx_code == "" || ptx_code.find("Generated by LLVM NVPTX Back-End") == std::string::npos) {
				log_error("PTX compilation failed!\n$", ptx_code);
				return {};
			}
			compiled_file_or_code.swap(ptx_code);
		} else if (options.target == TARGET::SPIRV_VULKAN ||
				   options.target == TARGET::SPIRV_OPENCL) {
			const auto validate = (options.target == TARGET::SPIRV_VULKAN ?
								   floor::get_vulkan_validate_spirv() : floor::get_opencl_validate_spirv());
			const auto validator = (options.target == TARGET::SPIRV_VULKAN ?
									floor::get_vulkan_spirv_validator() : floor::get_opencl_spirv_validator());
			
			// run spirv-val if specified
			const auto run_validation = [&dev, &validate, &validator, &options, &compiled_file_or_code]() {
				if (!validate) {
					return true;
				}
				std::string validator_opts;
				if (options.target == TARGET::SPIRV_VULKAN) {
					validator_opts = "--target-env vulkan1.3 --uniform-buffer-standard-layout --scalar-block-layout --workgroup-scalar-block-layout";
				} else {
					assert(options.target == TARGET::SPIRV_OPENCL);
					const auto& cl_device = (const opencl_device&)dev;
					switch (cl_device.cl_version) {
						case OPENCL_VERSION::NONE:
						case OPENCL_VERSION::OPENCL_1_0:
						case OPENCL_VERSION::OPENCL_1_1:
							log_error("unsupported OpenCL version");
							return false;
						case OPENCL_VERSION::OPENCL_1_2:
							validator_opts = "--target-env opencl1.2";
							break;
						case OPENCL_VERSION::OPENCL_2_0:
							validator_opts = "--target-env opencl2.0";
							break;
						case OPENCL_VERSION::OPENCL_2_1:
							validator_opts = "--target-env opencl2.1";
							break;
						case OPENCL_VERSION::OPENCL_2_2:
						case OPENCL_VERSION::OPENCL_3_0: // TODO: no 3.0 target env yet
							validator_opts = "--target-env opencl2.2";
							break;
					}
				}
				const std::string spirv_validator_cmd {
					"\"" + validator + "\" " + validator_opts + " " + compiled_file_or_code
#if !defined(_MSC_VER)
					+ " 2>&1"
#endif
				};
				std::string spirv_validator_output;
				core::system(spirv_validator_cmd, spirv_validator_output);
				if (!spirv_validator_output.empty() && spirv_validator_output[spirv_validator_output.size() - 1] == '\n') {
					spirv_validator_output.pop_back(); // trim last newline
				}
				if (!options.silence_debug_output) {
					if (spirv_validator_output == "") {
						log_msg("SPIR-V validator ($): valid", (options.target == TARGET::SPIRV_VULKAN ? "Vulkan" : "OpenCL"));
					} else {
						log_error("SPIR-V validator ($):\n$", (options.target == TARGET::SPIRV_VULKAN ? "Vulkan" : "OpenCL"), spirv_validator_output);
					}
				}
				return true;
			};
			if (!run_validation()) {
				return {};
			}
			
			// run spirv-opt if specified
			if (options.target == TARGET::SPIRV_VULKAN && options.vulkan.run_opt) {
				const auto vk_spirv_opt = floor::get_vulkan_spirv_opt();
				
				const std::string opt_options = (options.vulkan.opt_overrides ? *options.vulkan.opt_overrides :
												 "-O --unify-const --compact-ids --trim-capabilities --preserve-bindings --preserve-interface --preserve-spec-constants");
				const std::string spirv_opt_cmd {
					"\"" + vk_spirv_opt + "\" " + opt_options + " " + compiled_file_or_code + " -o " + compiled_file_or_code
#if !defined(_MSC_VER)
					+ " 2>&1"
#endif
				};
				std::string spirv_opt_output;
				core::system(spirv_opt_cmd, spirv_opt_output);
				if (!spirv_opt_output.empty() && spirv_opt_output[spirv_opt_output.size() - 1] == '\n') {
					spirv_opt_output.pop_back(); // trim last newline
				}
				if (!options.silence_debug_output) {
					if (spirv_opt_output == "") {
						log_msg("SPIR-V opt ($): success", (options.target == TARGET::SPIRV_VULKAN ? "Vulkan" : "OpenCL"));
					} else {
						log_error("SPIR-V opt ($): failure\n$", (options.target == TARGET::SPIRV_VULKAN ? "Vulkan" : "OpenCL"), spirv_opt_output);
					}
				}

				// run validation once more
				if (!run_validation()) {
					return {};
				}
			}
			
			// NOTE: will cleanup the binary in opencl_context/vulkan_context
		} else if (options.target == TARGET::HOST_COMPUTE_CPU) {
			// nop, already a binary
		}
	}
	
	return { true, compiled_file_or_code, functions, options };
}

} // namespace fl::toolchain
