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

#include <floor/device/universal_binary.hpp>
#include <floor/device/device_context.hpp>
#include <floor/device/opencl/opencl_device.hpp>
#include <floor/device/opencl/opencl_common.hpp>
#include <floor/device/cuda/cuda_device.hpp>
#include <floor/device/metal/metal_device.hpp>
#include <floor/device/host/host_device.hpp>
#include <floor/device/vulkan/vulkan_device.hpp>
#include <floor/core/file_io.hpp>
#include <floor/threading/task.hpp>
#include <floor/threading/thread_helpers.hpp>
#include <floor/core/bcm.hpp>
#include <floor/floor.hpp>
#include <chrono>
#include <deque>

namespace std {
	template <> struct hash<fl::universal_binary::target_v6> : public hash<uint64_t> {
		size_t operator()(fl::universal_binary::target_v6 target) const noexcept {
			return std::hash<uint64_t>::operator()(target.value);
		}
	};
}

namespace fl::universal_binary {
	static constexpr const uint32_t min_required_toolchain_version_v6 { 140000u };
	
	std::unique_ptr<archive> load_archive(const std::string& file_name) {
		auto [data, data_size] = file_io::file_to_buffer(file_name);
		if (!data || data_size == 0) {
			return {};
		}
		return load_archive(std::span<const uint8_t> { data.get(), data.get() + data_size }, file_name);
	}
		
	std::unique_ptr<archive> load_archive(std::span<const uint8_t> data, const std::string_view filename_hint_) {
		const auto filename_hint = (filename_hint_.empty() ? "<no-file-name>"sv : filename_hint_);
		auto data_size = data.size_bytes();
		auto cur_size = 0uz;
		
		auto ar = std::make_unique<archive>();
		
		// parse header
		cur_size += sizeof(header_v6);
		if (cur_size > data_size) {
			log_error("universal binary $: invalid header size, expected $, got $",
					  filename_hint, cur_size, data_size);
			return {};
		}
		const header_v6& header = *(const header_v6*)data.data();
		data = data.subspan(sizeof(header_v6));
		
		if (memcmp(header.magic, "FUBA", 4) != 0) {
			log_error("universal binary $: invalid header magic", filename_hint);
			return {};
		}
		if (header.binary_format_version != binary_format_version) {
			log_error("universal binary $: unsupported binary version $", filename_hint, header.binary_format_version);
			return {};
		}
		memcpy(&ar->header.static_header, &header, sizeof(header_v6));
		
		const auto& bin_count = ar->header.static_header.binary_count;
		if (bin_count == 0) {
			// no binaries -> return early
			floor_return_no_nrvo(ar);
		}
		
		// parse dynamic header
		ar->header.targets.resize(bin_count);
		ar->header.offsets.resize(bin_count);
		ar->header.toolchain_versions.resize(bin_count);
		ar->header.hashes.resize(bin_count);
		
		const auto targets_size = sizeof(target_v6) * bin_count;
		const auto offsets_size = sizeof(typename decltype(header_dynamic_v6::offsets)::value_type) * bin_count;
		const auto toolchain_versions_size = sizeof(typename decltype(header_dynamic_v6::toolchain_versions)::value_type) * bin_count;
		const auto hashes_size = sizeof(typename decltype(header_dynamic_v6::hashes)::value_type) * bin_count;
		const auto dyn_header_size = targets_size + offsets_size + toolchain_versions_size + hashes_size;
		cur_size += dyn_header_size;
		if (cur_size > data_size) {
			log_error("universal binary $: invalid dynamic header size, expected $, got $",
					  filename_hint, cur_size, data_size);
			return {};
		}
		
		memcpy(ar->header.targets.data(), data.data(), targets_size);
		data = data.subspan(targets_size);
		
		memcpy(ar->header.offsets.data(), data.data(), offsets_size);
		data = data.subspan(offsets_size);
		
		memcpy(ar->header.toolchain_versions.data(), data.data(), toolchain_versions_size);
		data = data.subspan(toolchain_versions_size);
		
		memcpy(ar->header.hashes.data(), data.data(), hashes_size);
		data = data.subspan(hashes_size);
		
		// verify targets
		for (const auto& target : ar->header.targets) {
			if (target.common.version != target_format_version) {
				log_error("universal binary $: unsupported target version, expected $, got $",
						  filename_hint, target_format_version, target.common.version);
				return {};
			}
		}
		
		// verify toolchain versions
		for (const auto& toolchain_version : ar->header.toolchain_versions) {
			if (toolchain_version < min_required_toolchain_version_v6) {
				log_error("universal binary $: unsupported toolchain version, expected $, got $",
						  filename_hint, min_required_toolchain_version_v6, toolchain_version);
				return {};
			}
		}
		
		// decompress data if specific in the header flags
		std::vector<uint8_t> decompressed_data;
		if (ar->header.static_header.flags.is_compressed) {
			decompressed_data = bcm::bcm_decompress(data);
			if (decompressed_data.empty()) {
				log_error("universal binary $: failed to decompress binaries data", filename_hint);
				return {};
			}
			
			// use decompressed data for the remainder of this function
			data_size -= data.size_bytes();
			data_size += decompressed_data.size();
			data = decompressed_data;
		}
		
		// parse binaries
		for (uint32_t bin_idx = 0; bin_idx < bin_count; ++bin_idx) {
			binary_dynamic_v6 bin;
			
			// verify binary offset
			if (cur_size != ar->header.offsets[bin_idx]) {
				log_error("universal binary $: invalid binary offset, expected $, got $",
						  filename_hint, ar->header.offsets[bin_idx], cur_size);
				return {};
			}
			
			// static binary header
			cur_size += sizeof(binary_v6);
			if (cur_size > data_size) {
				log_error("universal binary $: invalid static binary header size, expected $, got $",
						  filename_hint, cur_size, data_size);
				return {};
			}
			memcpy(&bin.static_binary_header, data.data(), sizeof(binary_v6));
			data = data.subspan(sizeof(binary_v6));
			
			// pre-check sizes (we're still going to do on-the-fly checks while parsing the actual data)
			if (cur_size + bin.static_binary_header.function_info_size > data_size) {
				log_error("universal binary $: invalid binary function info size (pre-check), expected $, got $",
						  filename_hint, cur_size + bin.static_binary_header.function_info_size, data_size);
				return {};
			}
			if (cur_size + bin.static_binary_header.function_info_size + bin.static_binary_header.binary_size > data_size) {
				log_error("universal binary $: invalid binary size (pre-check), expected $, got $",
						  filename_hint,
						  cur_size + bin.static_binary_header.function_info_size + bin.static_binary_header.binary_size,
						  data_size);
				return {};
			}
			
			// dynamic binary header
			
			// function info
			const auto func_info_start_size = cur_size;
			for (uint32_t func_idx = 0; func_idx < bin.static_binary_header.function_count; ++func_idx) {
				function_info_dynamic_v6 func_info;
				
				// static function info
				cur_size += sizeof(function_info_v6);
				if (cur_size > data_size) {
					log_error("universal binary $: invalid static function info size, expected $, got $",
							  filename_hint, cur_size, data_size);
					return {};
				}
				memcpy(&func_info.static_function_info, data.data(), sizeof(function_info_v6));
				data = data.subspan(sizeof(function_info_v6));
				
				if (func_info.static_function_info.function_info_version != function_info_version) {
					log_error("universal binary $: unsupported function info version $",
							  filename_hint, func_info.static_function_info.function_info_version);
					return {};
				}
				
				// dynamic function info
				for (;;) {
					// name (\0 terminated)
					++cur_size;
					if (cur_size > data_size) {
						log_error("universal binary $: invalid function info name size, expected $, got $",
								  filename_hint, cur_size, data_size);
						return {};
					}
					
					const auto ch = data.front();
					data = data.subspan(1);
					if (ch == 0) {
						break;
					}
					func_info.name += *(const char*)&ch;
				}
				
				for (uint32_t arg_idx = 0; arg_idx < func_info.static_function_info.arg_count; ++arg_idx) {
					function_info_dynamic_v6::arg_info arg;
					
					cur_size += sizeof(function_info_dynamic_v6::arg_info);
					if (cur_size > data_size) {
						log_error("universal binary $: invalid function info arg size, expected $, got $",
								  filename_hint, cur_size, data_size);
						return {};
					}
					memcpy(&arg, data.data(), sizeof(function_info_dynamic_v6::arg_info));
					data = data.subspan(sizeof(function_info_dynamic_v6::arg_info));
					
					func_info.args.emplace_back(arg);
				}
				
				bin.function_info.emplace_back(func_info);
			}
			const auto func_info_end_size = cur_size;
			const auto func_info_size = func_info_end_size - func_info_start_size;
			if (func_info_size != size_t(bin.static_binary_header.function_info_size)) {
				log_error("universal binary $: invalid binary function info size, expected $, got $",
						  filename_hint, bin.static_binary_header.function_info_size, func_info_size);
				return {};
			}
			
			// binary data
			cur_size += bin.static_binary_header.binary_size;
			if (cur_size > data_size) {
				log_error("universal binary $: invalid binary size, expected $, got $",
						  filename_hint, cur_size, data_size);
				return {};
			}
			bin.data.resize(bin.static_binary_header.binary_size);
			memcpy(bin.data.data(), data.data(), bin.static_binary_header.binary_size);
			data = data.subspan(bin.static_binary_header.binary_size);
			
			// verify binary
			const auto hash = sha_256::compute_hash(bin.data.data(), bin.data.size());
			if (hash != ar->header.hashes[bin_idx]) {
				log_error("universal binary $: invalid binary (hash mismatch)", filename_hint);
				return {};
			}
			
			// binary done
			ar->binaries.emplace_back(bin);
		}
		
		floor_return_no_nrvo(ar);
	}
	
	struct compile_return_t {
		bool success { false };
		uint32_t toolchain_version { 0 };
		toolchain::program_data prog_data;
	};
	static compile_return_t compile_target(const std::string& src_input,
										   const bool is_file_input,
										   const toolchain::compile_options& user_options,
										   const target& build_target,
										   const bool use_precompiled_header) {
		auto options = user_options;
		// always ignore run-time info, we want a reproducible and specific build
		options.ignore_runtime_info = true;
		
		uint32_t toolchain_version = 0;
		std::shared_ptr<device> dev;
		switch (build_target.common.type) {
			case PLATFORM_TYPE::OPENCL: {
				dev = std::make_shared<opencl_device>();
				const auto& cl_target = build_target.opencl;
				auto& cl_dev = (opencl_device&)*dev;
				
				toolchain_version = floor::get_opencl_toolchain_version();
				options.target = (cl_target.is_spir ? toolchain::TARGET::SPIR : toolchain::TARGET::SPIRV_OPENCL);
				if (options.target == toolchain::TARGET::SPIRV_OPENCL) {
					cl_dev.param_workaround = true;
				}
				
				cl_dev.cl_version = cl_version_from_uint(cl_target.major, cl_target.minor);
				cl_dev.c_version = cl_dev.cl_version;
				cl_dev.spirv_version = (cl_target.is_spir ? SPIRV_VERSION::NONE : SPIRV_VERSION::SPIRV_1_0);
				
				cl_dev.image_depth_support = cl_target.image_depth_support;
				cl_dev.image_msaa_support = cl_target.image_msaa_support;
				cl_dev.image_mipmap_support = cl_target.image_mipmap_support;
				cl_dev.image_mipmap_write_support = cl_target.image_mipmap_write_support;
				cl_dev.image_read_write_support = cl_target.image_read_write_support;
				cl_dev.double_support = cl_target.double_support;
				cl_dev.basic_64_bit_atomics_support = cl_target.basic_64_bit_atomics_support;
				cl_dev.extended_64_bit_atomics_support = cl_target.extended_64_bit_atomics_support;
				cl_dev.sub_group_support = cl_target.sub_group_support;
				
				if (cl_target.simd_width > 0) {
					cl_dev.simd_width = cl_target.simd_width;
					cl_dev.simd_range = { cl_dev.simd_width, cl_dev.simd_width };
				}
				
				// device type
				switch (cl_target.device_target) {
					case decltype(cl_target.device_target)::GENERIC:
						cl_dev.type = device::TYPE::NONE;
						break;
					case decltype(cl_target.device_target)::GENERIC_CPU:
					case decltype(cl_target.device_target)::INTEL_CPU:
					case decltype(cl_target.device_target)::AMD_CPU:
						cl_dev.type = device::TYPE::CPU0;
						break;
					case decltype(cl_target.device_target)::GENERIC_GPU:
					case decltype(cl_target.device_target)::INTEL_GPU:
					case decltype(cl_target.device_target)::AMD_GPU:
						cl_dev.type = device::TYPE::GPU0;
						break;
				}
				
				// special vendor workarounds/settings
				switch (cl_target.device_target) {
					default:
						cl_dev.vendor = VENDOR::UNKNOWN;
						cl_dev.platform_vendor = VENDOR::UNKNOWN;
						break;
					case decltype(cl_target.device_target)::INTEL_CPU:
					case decltype(cl_target.device_target)::INTEL_GPU:
						if (options.target == toolchain::TARGET::SPIR) {
							options.cli += " -Xclang -cl-spir-intel-workarounds";
						}
						cl_dev.vendor = VENDOR::INTEL;
						cl_dev.platform_vendor = VENDOR::INTEL;
						break;
					case decltype(cl_target.device_target)::AMD_CPU:
					case decltype(cl_target.device_target)::AMD_GPU:
						cl_dev.vendor = VENDOR::AMD;
						cl_dev.platform_vendor = VENDOR::AMD;
						break;
				}
				
				// assume SIMD-width if none is specified, but a specific hardware target is set
				if (cl_target.simd_width == 0) {
					switch (cl_target.device_target) {
						default: break;
						case decltype(cl_target.device_target)::INTEL_CPU:
						case decltype(cl_target.device_target)::AMD_CPU:
							cl_dev.simd_width = 4; // assume lowest
							cl_dev.simd_range = { cl_dev.simd_width, cl_dev.simd_width };
							break;
						case decltype(cl_target.device_target)::INTEL_GPU:
							cl_dev.simd_width = 16;
							cl_dev.simd_range = { cl_dev.simd_width, cl_dev.simd_width };
							break;
						case decltype(cl_target.device_target)::AMD_GPU:
							// NOTE: can't be assumed any more, can be 32 or 64 -> must be set explictly
							break;
					}
				}
				
				break;
			}
			case PLATFORM_TYPE::CUDA: {
				dev = std::make_shared<cuda_device>();
				const auto& cuda_target = build_target.cuda;
				auto& cuda_dev = (cuda_device&)*dev;
				
				toolchain_version = floor::get_cuda_toolchain_version();
				options.target = toolchain::TARGET::PTX;
				cuda_dev.sm = { cuda_target.sm_major, cuda_target.sm_minor };
				cuda_dev.sm_aa = (cuda_target.sm_aa != 0u);
				
				// handle PTX ISA version
				if ((cuda_dev.sm.x < 9 && cuda_target.ptx_isa_major < 8) ||
					(cuda_dev.sm.x == 9 && cuda_dev.sm.y == 0 && cuda_target.ptx_isa_major < 8) ||
					((cuda_dev.sm.x > 9 || (cuda_dev.sm.x == 9 && cuda_dev.sm.y > 0)) &&
					 (cuda_target.ptx_isa_major < 8 || (cuda_target.ptx_isa_major == 8 && cuda_target.ptx_isa_minor < 4)))) {
					log_error("invalid PTX version $.$ for target $",
							  cuda_target.ptx_isa_major, cuda_target.ptx_isa_minor, cuda_dev.sm);
					return {};
				}
				cuda_dev.ptx = { cuda_target.ptx_isa_major, cuda_target.ptx_isa_minor };
				options.cuda.ptx_version = cuda_target.ptx_isa_major * 10 + cuda_target.ptx_isa_minor;
				
				options.cuda.max_registers = cuda_target.max_registers;
				
				cuda_dev.image_depth_compare_support = cuda_target.image_depth_compare_support;
				
				// NOTE: other fixed device info is already set in the cuda_device constructor
				
				// TODO: handle CUBIN output
				if (!cuda_target.is_ptx) {
					log_error("CUBIN building not supported yet");
				}
				break;
			}
			case PLATFORM_TYPE::METAL: {
				dev = std::make_shared<metal_device>();
				const auto& mtl_target = build_target.metal;
				auto& mtl_dev = (metal_device&)*dev;
				
				toolchain_version = floor::get_metal_toolchain_version();
				options.target = toolchain::TARGET::AIR;
				if (mtl_target.soft_printf) {
					options.metal.soft_printf = true;
				}
				mtl_dev.metal_language_version = metal_version_from_uint(mtl_target.major, mtl_target.minor);
				mtl_dev.family_type = (mtl_target.platform_target == decltype(mtl_target.platform_target)::IOS ||
									   mtl_target.platform_target == decltype(mtl_target.platform_target)::VISIONOS ||
									   mtl_target.platform_target == decltype(mtl_target.platform_target)::IOS_SIMULATOR ||
									   mtl_target.platform_target == decltype(mtl_target.platform_target)::VISIONOS_SIMULATOR ?
									   metal_device::FAMILY_TYPE::APPLE : metal_device::FAMILY_TYPE::MAC);
				mtl_dev.platform_vendor = VENDOR::APPLE;
				mtl_dev.double_support = false; // always disabled for now
				mtl_dev.barycentric_coord_support = mtl_target.barycentric_coord_support;
				
				// overwrite device/metal_device defaults
				if (mtl_target.platform_target == decltype(mtl_target.platform_target)::IOS ||
					mtl_target.platform_target == decltype(mtl_target.platform_target)::IOS_SIMULATOR) {
					mtl_dev.platform_type = (mtl_target.platform_target == decltype(mtl_target.platform_target)::IOS_SIMULATOR ?
											 metal_device::PLATFORM_TYPE::IOS_SIMULATOR : metal_device::PLATFORM_TYPE::IOS);
					mtl_dev.family_tier = 7; // can't be overwritten right now
					mtl_dev.vendor = VENDOR::APPLE;
					mtl_dev.unified_memory = true;
					mtl_dev.simd_width = 32;
					mtl_dev.simd_range = { mtl_dev.simd_width, mtl_dev.simd_width };
				} else if (mtl_target.platform_target == decltype(mtl_target.platform_target)::VISIONOS ||
						   mtl_target.platform_target == decltype(mtl_target.platform_target)::VISIONOS_SIMULATOR) {
					mtl_dev.platform_type = (mtl_target.platform_target == decltype(mtl_target.platform_target)::VISIONOS_SIMULATOR ?
											 metal_device::PLATFORM_TYPE::VISIONOS_SIMULATOR : metal_device::PLATFORM_TYPE::VISIONOS);
					mtl_dev.family_tier = 8; // can't be overwritten right now
					mtl_dev.vendor = VENDOR::APPLE;
					mtl_dev.unified_memory = true;
					mtl_dev.simd_width = 32;
					mtl_dev.simd_range = { mtl_dev.simd_width, mtl_dev.simd_width };
				} else {
					mtl_dev.platform_type = metal_device::PLATFORM_TYPE::MACOS;
					mtl_dev.family_tier = 2; // can't be overwritten right now
					
					// special vendor workarounds/settings + SIMD handling
					switch (mtl_target.device_target) {
						default:
							mtl_dev.simd_width = mtl_target.simd_width;
							break;
						case decltype(mtl_target.device_target)::INTEL:
							options.cli += " -Xclang -metal-intel-workarounds";
							mtl_dev.vendor = VENDOR::INTEL;
							mtl_dev.simd_width = 32;
							break;
						case decltype(mtl_target.device_target)::AMD:
							// no AMD workarounds yet
							mtl_dev.vendor = VENDOR::AMD;
							if (mtl_target.simd_width != 32 && mtl_target.simd_width != 64) {
								log_error("SIMD width must be 32 or 64 when targeting AMD");
								return {};
							}
							mtl_dev.simd_width = mtl_target.simd_width;
							break;
						case decltype(mtl_target.device_target)::APPLE:
							mtl_dev.vendor = VENDOR::APPLE;
							mtl_dev.simd_width = 32;
							break;
					}
					if (mtl_target.device_target != decltype(mtl_target.device_target)::GENERIC) {
						// fixed SIMD width must match requested one
						if (mtl_dev.simd_width != mtl_target.simd_width &&
							mtl_target.simd_width > 0) {
							log_error("invalid required SIMD width: $", mtl_target.simd_width);
							return {};
						}
					}
					mtl_dev.simd_range = { mtl_dev.simd_width, mtl_dev.simd_width };
				}
				break;
			}
			case PLATFORM_TYPE::HOST: {
				dev = std::make_shared<host_device>();
				const auto& host_target = build_target.host;
				auto& host_dev = (host_device&)*dev;
				
				toolchain_version = floor::get_host_toolchain_version();
				options.target = toolchain::TARGET::HOST_COMPUTE_CPU;
				host_dev.cpu_tier = host_target.cpu_tier;
				host_dev.platform_vendor = VENDOR::HOST;
				host_dev.type = device::TYPE::CPU0;
				
				// overwrite SIMD defaults based on target
				switch (host_target.cpu_tier) {
					case HOST_CPU_TIER::X86_TIER_1:
						host_dev.simd_width = 4u; // SSE
						break;
					case HOST_CPU_TIER::X86_TIER_2:
					case HOST_CPU_TIER::X86_TIER_3:
						host_dev.simd_width = 8u; // AVX
						break;
					case HOST_CPU_TIER::X86_TIER_4:
					case HOST_CPU_TIER::X86_TIER_5:
						host_dev.simd_width = 16u; // AVX-512
						break;
					case HOST_CPU_TIER::ARM_TIER_1:
					case HOST_CPU_TIER::ARM_TIER_2:
					case HOST_CPU_TIER::ARM_TIER_3:
					case HOST_CPU_TIER::ARM_TIER_4:
					case HOST_CPU_TIER::ARM_TIER_5:
					case HOST_CPU_TIER::ARM_TIER_6:
					case HOST_CPU_TIER::ARM_TIER_7:
						host_dev.simd_width = 4u; // NEON
						break;
					default:
						log_error("unknown/unhandled CPU tier");
						return {};
				}
				host_dev.simd_range = { 1, host_dev.simd_width };
				
				// no double support for now
				host_dev.double_support = false;
				
				break;
			}
			case PLATFORM_TYPE::VULKAN: {
				dev = std::make_shared<vulkan_device>();
				const auto& vlk_target = build_target.vulkan;
				auto& vlk_dev = (vulkan_device&)*dev;
				
				toolchain_version = floor::get_vulkan_toolchain_version();
				options.target = toolchain::TARGET::SPIRV_VULKAN;
				if (vlk_target.soft_printf) {
					options.vulkan.soft_printf = true;
				}
				vlk_dev.vulkan_version = vulkan_version_from_uint(vlk_target.vulkan_major, vlk_target.vulkan_minor);
				vlk_dev.spirv_version = spirv_version_from_uint(vlk_target.spirv_major, vlk_target.spirv_minor);
				vlk_dev.platform_vendor = VENDOR::KHRONOS;
				vlk_dev.type = device::TYPE::GPU0;
				
				vlk_dev.double_support = vlk_target.double_support;
				vlk_dev.basic_64_bit_atomics_support = vlk_target.basic_64_bit_atomics_support;
				vlk_dev.extended_64_bit_atomics_support = vlk_target.extended_64_bit_atomics_support;
				vlk_dev.basic_32_bit_float_atomics_support = vlk_target.basic_32_bit_float_atomics_support;
				vlk_dev.primitive_id_support = vlk_target.primitive_id_support;
				vlk_dev.barycentric_coord_support = vlk_target.barycentric_coord_support;
				vlk_dev.tessellation_support = vlk_target.tessellation_support;
				vlk_dev.max_tessellation_factor = (vlk_dev.tessellation_support ? 64u : 0u);
				vlk_dev.max_mip_levels = vlk_dev.max_mip_levels;
				vlk_dev.argument_buffer_support = true;
				vlk_dev.argument_buffer_image_support = true;
				vlk_dev.indirect_command_support = true;
				vlk_dev.indirect_render_command_support = true;
				vlk_dev.indirect_compute_command_support = true;
				
				// assume minimum required support for now
				vlk_dev.max_inline_uniform_block_size = vulkan_device::min_required_inline_uniform_block_size;
				vlk_dev.max_inline_uniform_block_count = vulkan_device::min_required_inline_uniform_block_count;
				
				// special vendor workarounds/settings + SIMD handling
				switch (vlk_target.device_target) {
					default:
						vlk_dev.simd_width = vlk_target.simd_width;
						break;
					case decltype(vlk_target.device_target)::NVIDIA:
						vlk_dev.vendor = VENDOR::NVIDIA;
						vlk_dev.simd_width = 32;
						break;
					case decltype(vlk_target.device_target)::AMD:
						vlk_dev.vendor = VENDOR::AMD;
						if (vlk_target.simd_width != 32 && vlk_target.simd_width != 64) {
							log_error("SIMD width must be 32 or 64 when targeting AMD");
							return {};
						}
						break;
					case decltype(vlk_target.device_target)::INTEL:
						vlk_dev.vendor = VENDOR::INTEL;
						vlk_dev.simd_width = 32;
						break;
				}
				if (vlk_target.device_target != decltype(vlk_target.device_target)::GENERIC) {
					// fixed SIMD width must match requested one
					if (vlk_dev.simd_width != vlk_target.simd_width &&
						vlk_target.simd_width > 0) {
						log_error("invalid required SIMD width: $", vlk_target.simd_width);
						return {};
					}
				}
				vlk_dev.simd_range = { vlk_dev.simd_width, vlk_dev.simd_width };
				break;
			}
			case PLATFORM_TYPE::NONE:
				return {};
		}
		
		// build the pre-compiled header
		if (use_precompiled_header) {
			std::stringstream pch_path_sstr;
			switch (build_target.common.type) {
				case PLATFORM_TYPE::OPENCL:
					pch_path_sstr << floor::get_opencl_base_path();
					break;
				case PLATFORM_TYPE::CUDA:
					pch_path_sstr << floor::get_cuda_base_path();
					break;
				case PLATFORM_TYPE::HOST:
					pch_path_sstr << floor::get_host_base_path();
					break;
				case PLATFORM_TYPE::METAL:
					pch_path_sstr << floor::get_metal_base_path();
					break;
				case PLATFORM_TYPE::VULKAN:
					pch_path_sstr << floor::get_vulkan_base_path();
					break;
				case PLATFORM_TYPE::NONE:
					floor_unreachable(); // already exited above
			}
			
			pch_path_sstr << "/pch/";
			if (!file_io::is_directory(pch_path_sstr.str())) {
				file_io::create_directory(pch_path_sstr.str());
			}
			
			pch_path_sstr << std::hex << std::uppercase << std::setw(8u) << std::setfill('0');
			pch_path_sstr << build_target.value;
			pch_path_sstr << std::setfill(' ') << std::setw(0) << std::nouppercase << std::dec;
			pch_path_sstr << ".pch";
			const auto pch_path = pch_path_sstr.str();
			bool has_pch = file_io::is_file(pch_path);
			if (!has_pch) {
				// pch doesn't exist yet, build it
				auto pch = toolchain::compile_precompiled_header(pch_path, *dev, options);
				if (pch.valid) {
					has_pch = true;
				}
			}
			if (has_pch) {
				// pch exists, set it
				options.pch = pch_path;
			}
		}
		
		// build the program
		toolchain::program_data program;
		if (is_file_input) {
			program = toolchain::compile_program_file(*dev, src_input, options);
		} else {
			program = toolchain::compile_program(*dev, src_input, options);
		}
		if (!program.valid) {
			// TODO: build failed, proper error msg
			return {};
		}
		return { true, toolchain_version, program };
	}
	
	static bool build_archive(const std::string& src_input,
							  const bool is_file_input,
							  const std::string& dst_archive_file_name,
							  const toolchain::compile_options& options,
							  const std::vector<target>& targets_in,
							  const bool use_precompiled_header) {
		// make sure we can open the output file before we start doing anything else
		file_io archive(dst_archive_file_name, file_io::OPEN_TYPE::WRITE_BINARY);
		if (!archive.is_open()) {
			log_error("can't write archive to $", dst_archive_file_name);
			return false;
		}
		
		// ensure targets are unique
		std::unordered_set<target> unique_targets_in;
		unique_targets_in.reserve(targets_in.size());
		for (const auto& target : targets_in) {
			unique_targets_in.emplace(target);
		}
		
		// create a thread pool of #logical-CPUs threads that build all targets
		const auto target_count = unique_targets_in.size();
		const auto compile_job_count = uint32_t(std::min(size_t(get_logical_core_count()), target_count));
		
		// enqueue + sanitize targets
		safe_mutex targets_lock;
		std::vector<target_v6> targets;
		std::deque<std::pair<size_t, target>> remaining_targets;
		auto unique_target_iter = unique_targets_in.begin();
		for (size_t i = 0; i < target_count; ++i, ++unique_target_iter) {
			auto target = *unique_target_iter;
			switch (target.common.type) {
				case PLATFORM_TYPE::NONE:
					log_error("invalid target type");
					return false;
				case PLATFORM_TYPE::OPENCL:
					target.opencl._unused = 0;
					break;
				case PLATFORM_TYPE::CUDA:
					target.cuda._unused = 0;
					break;
				case PLATFORM_TYPE::METAL:
					target.metal._unused = 0;
					break;
				case PLATFORM_TYPE::HOST:
					target.host._unused = 0;
					break;
				case PLATFORM_TYPE::VULKAN:
					target.vulkan._unused = 0;
					break;
			}
			
			targets.emplace_back(target);
			remaining_targets.emplace_back(i, target);
		}
		
		safe_mutex prog_data_lock;
		std::vector<std::unique_ptr<toolchain::program_data>> targets_prog_data(target_count);
		std::vector<uint32_t> targets_toolchain_version(target_count);
		std::vector<sha_256::hash_t> targets_hashes(target_count);
		
		std::atomic<uint32_t> remaining_compile_jobs { compile_job_count };
		std::atomic<bool> compilation_successful { true };
		for (uint32_t i = 0; i < compile_job_count; ++i) {
			task::spawn([&src_input, &is_file_input, &options, &use_precompiled_header,
						 &targets_lock, &remaining_targets,
						 &prog_data_lock, &targets_prog_data, &targets_toolchain_version, &targets_hashes,
						 &remaining_compile_jobs,
						 &compilation_successful]() {
				while (compilation_successful) {
					// get a target
					std::pair<size_t, target> build_target;
					{
						GUARD(targets_lock);
						if (remaining_targets.empty()) {
							break;
						}
						build_target = remaining_targets[0];
						remaining_targets.pop_front();
					}
					
					// compile the target
					auto compile_ret = compile_target(src_input, is_file_input, options, build_target.second, use_precompiled_header);
					if (!compile_ret.success || !compile_ret.prog_data.valid) {
						compilation_successful = false;
						break;
					}
					
					// TODO: cleanup binary as in opencl_context/vulkan_context + in general for other backends?
					
					// for SPIR-V, AIR and Host-Compute, the binary data is written as a file -> read it so we have it in memory
					if (compile_ret.prog_data.options.target == toolchain::TARGET::SPIRV_OPENCL ||
						compile_ret.prog_data.options.target == toolchain::TARGET::SPIRV_VULKAN ||
						compile_ret.prog_data.options.target == toolchain::TARGET::AIR ||
						compile_ret.prog_data.options.target == toolchain::TARGET::HOST_COMPUTE_CPU) {
						std::string bin_data;
						if (!file_io::file_to_string(compile_ret.prog_data.data_or_filename, bin_data)) {
							compilation_successful = false;
							break;
						}
						compile_ret.prog_data.data_or_filename = std::move(bin_data);
					}
					
					// compute binary hash
					const auto binary_hash = sha_256::compute_hash((const uint8_t*)compile_ret.prog_data.data_or_filename.c_str(),
																   compile_ret.prog_data.data_or_filename.size());
					
					// add to program data array
					{
						auto prog_data = std::make_unique<toolchain::program_data>();
						*prog_data = std::move(compile_ret.prog_data);
						
						GUARD(prog_data_lock);
						targets_prog_data[build_target.first] = std::move(prog_data);
						targets_toolchain_version[build_target.first] = compile_ret.toolchain_version;
						targets_hashes[build_target.first] = binary_hash;
					}
				}
				--remaining_compile_jobs;
			}, "build_job_" + std::to_string(i));
		}
		
		while (remaining_compile_jobs > 0) {
			std::this_thread::sleep_for(250ms);
			std::this_thread::yield();
		}
		
		// check success and output validity
		if (!compilation_successful) {
			return false;
		}
		for (const auto& prog_data : targets_prog_data) {
			if (prog_data == nullptr) {
				return false;
			}
		}
		
		// write binary
		header_dynamic_v6 header {
			.static_header = {
				.binary_format_version = binary_format_version,
				.binary_count = uint32_t(targets_prog_data.size()),
				.flags = {
					.is_compressed = options.compress_binaries ? 1u : 0u,
					._unused_flags = 0u,
				},
			},
			.targets = targets,
			.toolchain_versions = std::move(targets_toolchain_version),
			.hashes = std::move(targets_hashes),
		};
		// NOTE: proper offsets are written later on
		header.offsets.resize(header.static_header.binary_count);
		
		// header
		auto& ar_stream = *archive.get_filestream();
		archive.write_block(&header.static_header, sizeof(header_v6));
		archive.write_block(header.targets.data(), target_count * sizeof(typename decltype(header.targets)::value_type));
		const auto header_offsets_pos = ar_stream.tellp();
		archive.write_block(header.offsets.data(), header.offsets.size() * sizeof(typename decltype(header.offsets)::value_type));
		archive.write_block(header.toolchain_versions.data(),
							header.toolchain_versions.size() * sizeof(typename decltype(header.toolchain_versions)::value_type));
		archive.write_block(header.hashes.data(), header.hashes.size() * sizeof(typename decltype(header.hashes)::value_type));
		
		// binaries
		const auto binary_base_offset = uint64_t(ar_stream.tellp());
		std::vector<uint8_t> binaries_data;
		binaries_data.reserve(128u * 1024u);
		for (size_t i = 0; i < target_count; ++i) {
			const auto& bin = *targets_prog_data[i];
			
			// remember offset
			header.offsets[i] = binary_base_offset + binaries_data.size();
			
			// static header
			binary_dynamic_v6 bin_data {
				.static_binary_header = {
					.function_count = 0u, // -> will be incremented below
					.function_info_size = 0, // N/A yet
					.binary_size = uint32_t(bin.data_or_filename.size()),
					.flags = {
						._unused_flags = 0u,
					},
				},
			};
			// NOTE: bin_data.data must not even be written/copied here
			
			// convert function info
			bin_data.function_info.reserve(bin.function_info.size());
			const std::function<bool(const toolchain::function_info&, const uint32_t)> create_bin_function_info =
			[&bin_data, &create_bin_function_info](const toolchain::function_info& func, const uint32_t argument_buffer_index) {
				function_info_dynamic_v6 finfo {
					.static_function_info = {
						.function_info_version = function_info_version,
						.type = func.type,
						.flags = func.flags,
						.arg_count = uint32_t(func.args.size()),
						.local_size = func.required_local_size,
						.simd_width = func.required_simd_width,
						.argument_buffer_index = (func.type == toolchain::FUNCTION_TYPE::ARGUMENT_BUFFER_STRUCT ? argument_buffer_index : 0u),
					},
					.name = func.name,
					.args = {}, // need proper conversion
				};
				bin_data.static_binary_header.function_info_size += sizeof(finfo.static_function_info);
				bin_data.static_binary_header.function_info_size += finfo.name.size() + 1 /* \0 */;
				
				// convert/create args
				finfo.args.reserve(func.args.size());
				std::vector<std::pair<const toolchain::function_info*, uint32_t>> arg_buffers;
				for (uint32_t arg_idx = 0, arg_count = (uint32_t)func.args.size(); arg_idx < arg_count; ++arg_idx) {
					const auto& arg = func.args[arg_idx];
					finfo.args.emplace_back(function_info_dynamic_v6::arg_info {
						.size = arg.size,
						.array_extent = arg.array_extent,
						.address_space = arg.address_space,
						.access = arg.access,
						.image_type = arg.image_type,
						.flags = arg.flags,
					});
					if (has_flag<toolchain::ARG_FLAG::ARGUMENT_BUFFER>(arg.flags)) {
						if (!arg.argument_buffer_info) {
							log_error("missing argument buffer info for function $", finfo.name);
							return false;
						}
						// delay argument buffer function info creation until after we have written the info for this function
						arg_buffers.emplace_back(&*arg.argument_buffer_info, arg_idx);
					}
				}
				++bin_data.static_binary_header.function_count;
				bin_data.static_binary_header.function_info_size += sizeof(function_info_dynamic_v6::arg_info) * finfo.args.size();
				bin_data.function_info.emplace_back(std::move(finfo));
				
				// write argument buffer info
				for (const auto& arg_buffer_info : arg_buffers) {
					create_bin_function_info(*arg_buffer_info.first, arg_buffer_info.second);
				}
				
				return true;
			};
			for (const auto& func : bin.function_info) {
				if (!create_bin_function_info(func, 0u)) {
					return false;
				}
			}
			
			// write static header
			const std::span static_binary_header_data { (const uint8_t*)&bin_data.static_binary_header, sizeof(bin_data.static_binary_header) };
			binaries_data.insert(binaries_data.end(), static_binary_header_data.begin(), static_binary_header_data.end());
			
			// write dynamic binary part
			for (const auto& finfo : bin_data.function_info) {
				const std::span static_function_info_data { (const uint8_t*)&finfo.static_function_info, sizeof(finfo.static_function_info) };
				binaries_data.insert(binaries_data.end(), static_function_info_data.begin(), static_function_info_data.end());
				
				binaries_data.insert(binaries_data.end(), finfo.name.begin(), finfo.name.end());
				binaries_data.emplace_back(0 /* string zero terminator */);
				
				const std::span finfo_args_data {
					(const uint8_t*)finfo.args.data(),
					finfo.args.size() * sizeof(typename decltype(finfo.args)::value_type)
				};
				binaries_data.insert(binaries_data.end(), finfo_args_data.begin(), finfo_args_data.end());
			}
			binaries_data.insert(binaries_data.end(), bin.data_or_filename.begin(), bin.data_or_filename.end());
		}
		
		// write compressed binary data or raw binary data?
		if (header.static_header.flags.is_compressed) {
			auto compressed_data = bcm::bcm_compress(binaries_data);
			archive.write_block(compressed_data.data(), compressed_data.size());
		} else {
			archive.write_block(binaries_data.data(), binaries_data.size());
		}
		
		// update binary offsets now that we know them all
		ar_stream.seekp(header_offsets_pos);
		archive.write_block(header.offsets.data(), header.offsets.size() * sizeof(typename decltype(header.offsets)::value_type));
		
		return true;
	}
	
	bool build_archive_from_file(const std::string& src_file_name,
								 const std::string& dst_archive_file_name,
								 const toolchain::compile_options& options,
								 const std::vector<target>& targets,
								 const bool use_precompiled_header) {
		return build_archive(src_file_name, true, dst_archive_file_name, options, targets, use_precompiled_header);
	}
	
	bool build_archive_from_memory(const std::string& src_code,
								   const std::string& dst_archive_file_name,
								   const toolchain::compile_options& options,
								   const std::vector<target>& targets,
								   const bool use_precompiled_header) {
		return build_archive(src_code, false, dst_archive_file_name, options, targets, use_precompiled_header);
	}
	
	std::pair<const binary_dynamic_v6*, const target_v6>
	find_best_match_for_device(const device& dev, const archive& ar) {
		if (dev.context == nullptr) return { nullptr, {} };
		
		const auto type = dev.context->get_platform_type();
		
		// for easier access
		const auto& cl_dev = (const opencl_device&)dev;
		const auto& cuda_dev = (const cuda_device&)dev;
		const auto& mtl_dev = (const metal_device&)dev;
		const auto& host_dev = (const host_device&)dev;
		const auto& vlk_dev = (const vulkan_device&)dev;
		
		size_t best_target_idx = ~size_t(0);
		for (size_t i = 0, count = ar.header.targets.size(); i < count; ++i) {
			const auto& target = ar.header.targets[i];
			if (target.common.type != type) continue;
			if (ar.header.toolchain_versions[i] < min_required_toolchain_version_v6) continue;
			
			switch (target.common.type) {
				case PLATFORM_TYPE::NONE: continue;
				case PLATFORM_TYPE::OPENCL: {
					const auto& cl_target = target.opencl;
					
					// version check
					const auto cl_ver = cl_version_from_uint(cl_target.major, cl_target.minor);
					if (cl_ver > cl_dev.cl_version || cl_ver == OPENCL_VERSION::NONE) {
						// version too high
						continue;
					}
					
					// check SPIR compat
					if (cl_target.is_spir && !cl_dev.has_spir_support) {
						// no SPIR support
						continue;
					}
					
					// check SPIR-V compat
					if (!cl_target.is_spir && cl_dev.spirv_version == SPIRV_VERSION::NONE) {
						// no SPIR-V support
						continue;
					}
					
					// generic device can only match generic target
					if (cl_dev.is_no_cpu_or_gpu() &&
						cl_target.device_target != decltype(cl_target.device_target)::GENERIC) {
						continue;
					}
					
					// check device target
					switch (cl_target.device_target) {
						case decltype(cl_target.device_target)::GENERIC:
							// assume support
							break;
						case decltype(cl_target.device_target)::GENERIC_CPU:
							if (cl_dev.is_gpu()) continue;
							break;
						case decltype(cl_target.device_target)::GENERIC_GPU:
							if (cl_dev.is_cpu()) continue;
							break;
						case decltype(cl_target.device_target)::INTEL_CPU:
							if (cl_dev.is_gpu()) continue;
							if (cl_dev.vendor != VENDOR::INTEL) continue;
							break;
						case decltype(cl_target.device_target)::INTEL_GPU:
							if (cl_dev.is_cpu()) continue;
							if (cl_dev.vendor != VENDOR::INTEL) continue;
							break;
						case decltype(cl_target.device_target)::AMD_CPU:
							if (cl_dev.is_gpu()) continue;
							if (cl_dev.vendor != VENDOR::AMD) continue;
							break;
						case decltype(cl_target.device_target)::AMD_GPU:
							if (cl_dev.is_cpu()) continue;
							if (cl_dev.vendor != VENDOR::AMD) continue;
							break;
					}
					
					// check caps
					if (cl_target.image_depth_support && !dev.image_depth_support) {
						continue;
					}
					if (cl_target.image_msaa_support && !dev.image_msaa_support) {
						continue;
					}
					if (cl_target.image_mipmap_support && !dev.image_mipmap_support) {
						continue;
					}
					if (cl_target.image_mipmap_write_support && !dev.image_mipmap_write_support) {
						continue;
					}
					if (cl_target.image_read_write_support && !dev.image_read_write_support) {
						continue;
					}
					if (cl_target.double_support && !dev.double_support) {
						continue;
					}
					if (cl_target.basic_64_bit_atomics_support && !dev.basic_64_bit_atomics_support) {
						continue;
					}
					if (cl_target.extended_64_bit_atomics_support && !dev.extended_64_bit_atomics_support) {
						continue;
					}
					if (cl_target.sub_group_support && !dev.sub_group_support) {
						continue;
					}
					
					// check SIMD width
					if (cl_target.simd_width > 0 && (cl_target.simd_width < dev.simd_range.x ||
													 cl_target.simd_width > dev.simd_range.y)) {
						continue;
					}
					
					// -> binary is compatible, now check for best match
					if (best_target_idx != ~size_t(0)) {
						// newer version beats old
						const auto& best_cl = ar.header.targets[best_target_idx].opencl;
						const auto best_cl_ver = cl_version_from_uint(best_cl.major, best_cl.minor);
						if (cl_ver > best_cl_ver) {
							best_target_idx = i;
							continue;
						}
						if (cl_ver < best_cl_ver) {
							continue; // ignore lower target
						}
						
						// for OpenCL 2.0+, SPIR-V beats SPIR
						if (cl_ver >= OPENCL_VERSION::OPENCL_2_0) {
							if (!cl_target.is_spir && best_cl.is_spir) {
								best_target_idx = i;
								continue;
							}
							if (cl_target.is_spir && !best_cl.is_spir) {
								continue; // ignore lower target
							}
						}
						
						// vendor / device specific beats generic
						// NOTE: we have already filtered out non-supported device targets
						//       -> enums are ordered so that higher is better / more specific
						if (cl_target.device_target > best_cl.device_target) {
							best_target_idx = i;
							continue;
						}
						if (cl_target.device_target < best_cl.device_target) {
							continue; // ignore lower target
						}
						
						// more used/supported caps beats lower
						const auto cap_sum = (cl_target.image_depth_support +
											  cl_target.image_msaa_support +
											  cl_target.image_mipmap_support +
											  cl_target.image_mipmap_write_support +
											  cl_target.image_read_write_support +
											  cl_target.double_support +
											  cl_target.basic_64_bit_atomics_support +
											  cl_target.extended_64_bit_atomics_support +
											  cl_target.sub_group_support);
						const auto best_cap_sum = (best_cl.image_depth_support +
												   best_cl.image_msaa_support +
												   best_cl.image_mipmap_support +
												   best_cl.image_mipmap_write_support +
												   best_cl.image_read_write_support +
												   best_cl.double_support +
												   best_cl.basic_64_bit_atomics_support +
												   best_cl.extended_64_bit_atomics_support +
												   best_cl.sub_group_support);
						if (cap_sum > best_cap_sum) {
							best_target_idx = i;
							continue;
						}
						if (cap_sum < best_cap_sum) {
							continue; // ignore lower target
						}
						
						// higher SIMD width beats lower
						// NOTE: only relevant for dynamic SIMD width devices and if we have sub-group support
						if (cl_target.sub_group_support) {
							if (cl_target.simd_width > best_cl.simd_width) {
								best_target_idx = i;
								continue;
							}
							if (cl_target.simd_width < best_cl.simd_width) {
								continue; // ignore lower target
							}
						}
					} else {
						// no best binary yet
						best_target_idx = i;
						continue;
					}
					break;
				}
				case PLATFORM_TYPE::CUDA: {
					const auto& cuda_target = target.cuda;
					
					// check SM version
					// NOTE: if the binary is a CUBIN or "architecture-accelerated", the SM version must exactly match
					if (!cuda_target.is_ptx || cuda_target.sm_aa) {
						if (cuda_target.sm_major != cuda_dev.sm.x ||
							cuda_target.sm_minor != cuda_dev.sm.y) {
							continue;
						}
					} else {
						// PTX is also upwards-compatible (but not downwards)
						if (cuda_target.sm_major > cuda_dev.sm.x ||
							(cuda_target.sm_major == cuda_dev.sm.x &&
							 cuda_target.sm_minor > cuda_dev.sm.y)) {
							continue;
						}
					}
					
					// check PTX ISA version
					if (cuda_target.ptx_isa_major < cuda_dev.min_req_ptx.x ||
						(cuda_target.ptx_isa_major == cuda_dev.min_req_ptx.x &&
						 cuda_target.ptx_isa_minor < cuda_dev.min_req_ptx.y)) {
						continue;
					}
					
					// check hardware/software depth compare support
					if (cuda_target.image_depth_compare_support && !dev.image_depth_compare_support) {
						continue;
					}
					
					// -> binary is compatible, now check for best match
					if (best_target_idx != ~size_t(0)) {
						// CUBIN beats PTX, regardless of version
						const auto& best_cuda = ar.header.targets[best_target_idx].cuda;
						if (!cuda_target.is_ptx && best_cuda.is_ptx) {
							best_target_idx = i;
							continue;
						}
						if (cuda_target.is_ptx && !best_cuda.is_ptx) {
							continue; // ignore lower target
						}
						
						// if PTX: higher SM beats lower SM
						if (cuda_target.is_ptx && !cuda_target.sm_aa) {
							if (cuda_target.sm_major > best_cuda.sm_major ||
								(cuda_target.sm_major == best_cuda.sm_major &&
								 cuda_target.sm_minor > best_cuda.sm_minor)) {
								best_target_idx = i;
								continue;
							}
							if (cuda_target.sm_major < best_cuda.sm_major ||
								(cuda_target.sm_major == best_cuda.sm_major &&
								 cuda_target.sm_minor < best_cuda.sm_minor)) {
								continue; // ignore lower target
							}
						}
						
						// higher PTX ISA version beats lower version
						if (cuda_target.ptx_isa_major > best_cuda.ptx_isa_major ||
							(cuda_target.ptx_isa_major == best_cuda.ptx_isa_major &&
							 cuda_target.ptx_isa_minor > best_cuda.ptx_isa_minor)) {
							best_target_idx = i;
							continue;
						}
						if (cuda_target.ptx_isa_major < best_cuda.ptx_isa_major ||
							(cuda_target.ptx_isa_major == best_cuda.ptx_isa_major &&
							 cuda_target.ptx_isa_minor < best_cuda.ptx_isa_minor)) {
							continue; // ignore lower target
						}
						
						// hardware depth compare beats software
						if (cuda_target.image_depth_compare_support && !best_cuda.image_depth_compare_support) {
							best_target_idx = i;
							continue;
						}
						if (!cuda_target.image_depth_compare_support && best_cuda.image_depth_compare_support) {
							continue; // ignore lower target
						}
						
						// NOTE: max_registers is ignored for any comparison
					} else {
						// no best binary yet
						best_target_idx = i;
						continue;
					}
					break;
				}
				case PLATFORM_TYPE::METAL: {
					const auto& mtl_target = target.metal;
					
					// macOS binary, but not macOS device?
					if (mtl_target.platform_target == decltype(mtl_target.platform_target)::MACOS &&
						mtl_dev.platform_type != metal_device::PLATFORM_TYPE::MACOS) {
						continue;
					}
					// iOS binary, but not iOS device?
					if (mtl_target.platform_target == decltype(mtl_target.platform_target)::IOS &&
						mtl_dev.platform_type != metal_device::PLATFORM_TYPE::IOS) {
						continue;
					}
					if (mtl_target.platform_target == decltype(mtl_target.platform_target)::IOS_SIMULATOR &&
						mtl_dev.platform_type != metal_device::PLATFORM_TYPE::IOS_SIMULATOR) {
						continue;
					}
					// visionOS binary, but not visionOS device?
					if (mtl_target.platform_target == decltype(mtl_target.platform_target)::VISIONOS &&
						mtl_dev.platform_type != metal_device::PLATFORM_TYPE::VISIONOS) {
						continue;
					}
					if (mtl_target.platform_target == decltype(mtl_target.platform_target)::VISIONOS_SIMULATOR &&
						mtl_dev.platform_type != metal_device::PLATFORM_TYPE::VISIONOS_SIMULATOR) {
						continue;
					}
					
					// version check
					const auto mtl_ver = metal_version_from_uint(mtl_target.major, mtl_target.minor);
					if (mtl_ver > mtl_dev.metal_language_version) {
						continue;
					}
					
					// check device target
					switch (mtl_target.device_target) {
						case decltype(mtl_target.device_target)::GENERIC:
							// assume support
							break;
						case decltype(mtl_target.device_target)::AMD:
							if (mtl_dev.vendor != VENDOR::AMD) {
								continue;
							}
							break;
						case decltype(mtl_target.device_target)::INTEL:
							if (mtl_dev.vendor != VENDOR::INTEL) {
								continue;
							}
							break;
						case decltype(mtl_target.device_target)::APPLE:
							if (mtl_dev.vendor != VENDOR::APPLE) {
								continue;
							}
							break;
					}
					if ((mtl_target.platform_target == decltype(mtl_target.platform_target)::IOS ||
						 mtl_target.platform_target == decltype(mtl_target.platform_target)::VISIONOS ||
						 mtl_target.platform_target == decltype(mtl_target.platform_target)::IOS_SIMULATOR ||
						 mtl_target.platform_target == decltype(mtl_target.platform_target)::VISIONOS_SIMULATOR) &&
						mtl_target.device_target != decltype(mtl_target.device_target)::APPLE) {
						continue; // iOS/visionOS must use APPLE target
					}
					
					// check SIMD width
					if (mtl_target.simd_width > 0 && (mtl_target.simd_width < dev.simd_range.x ||
													  mtl_target.simd_width > dev.simd_range.y)) {
						continue;
					}
					
					// check caps
					if (mtl_target.barycentric_coord_support && !dev.barycentric_coord_support) {
						continue;
					}
					
					// -> binary is compatible, now check for best match
					if (best_target_idx != ~size_t(0)) {
						// higher version beats lower version
						const auto& best_mtl = ar.header.targets[best_target_idx].metal;
						const auto best_mtl_ver = metal_version_from_uint(best_mtl.major, best_mtl.minor);
						if (mtl_ver > best_mtl_ver) {
							best_target_idx = i;
							continue;
						}
						if (mtl_ver < best_mtl_ver) {
							continue; // ignore lower target
						}
						
						// vendor / device specific beats generic
						// NOTE: we have already filtered out non-supported device targets
						//       -> enums are ordered so that higher is better / more specific
						if (mtl_target.device_target > best_mtl.device_target) {
							best_target_idx = i;
							continue;
						}
						if (mtl_target.device_target < best_mtl.device_target) {
							continue; // ignore lower target
						}
						
						// higher SIMD width beats lower
						// NOTE: only relevant for dynamic SIMD width devices
						if (mtl_target.simd_width > best_mtl.simd_width) {
							best_target_idx = i;
							continue;
						}
						if (mtl_target.simd_width < best_mtl.simd_width) {
							continue; // ignore lower target
						}
						
						// more used/supported caps beats lower
						const auto cap_sum = (mtl_target.barycentric_coord_support);
						const auto best_cap_sum = (best_mtl.barycentric_coord_support);
						if (cap_sum > best_cap_sum) {
							best_target_idx = i;
							continue;
						}
						if (cap_sum < best_cap_sum) {
							continue; // ignore lower target
						}
						
#if defined(FLOOR_DEBUG)
						// in debug mode: soft-printf beats non-soft-printf
						if (mtl_target.soft_printf && !best_mtl.soft_printf) {
							best_target_idx = i;
							continue;
						}
#endif
					} else {
						// no best binary yet
						best_target_idx = i;
						continue;
					}
					break;
				}
				case PLATFORM_TYPE::HOST: {
					const auto& host_target = target.host;
					
					// check for arch match
					const auto is_target_x86 = (host_target.cpu_tier >= HOST_CPU_TIER::__X86_OFFSET && host_target.cpu_tier <= HOST_CPU_TIER::__X86_RANGE);
					const auto is_target_arm = (host_target.cpu_tier >= HOST_CPU_TIER::__ARM_OFFSET && host_target.cpu_tier <= HOST_CPU_TIER::__ARM_RANGE);
					const auto is_dev_x86 = (host_dev.cpu_tier >= HOST_CPU_TIER::__X86_OFFSET && host_dev.cpu_tier <= HOST_CPU_TIER::__X86_RANGE);
					const auto is_dev_arm = (host_dev.cpu_tier >= HOST_CPU_TIER::__ARM_OFFSET && host_dev.cpu_tier <= HOST_CPU_TIER::__ARM_RANGE);
					if (!(is_target_x86 && is_dev_x86) && !(is_target_arm && is_dev_arm)) {
						continue;
					}
					
					// CPU tier is too high for this device
					if (host_target.cpu_tier > host_dev.cpu_tier) {
						continue;
					}
					
					// -> binary is compatible, now check for best match
					if (best_target_idx != ~size_t(0)) {
						const auto& best_host = ar.header.targets[best_target_idx].host;
						
						// use highest supported CPU tier
						if (host_target.cpu_tier > best_host.cpu_tier) {
							best_target_idx = i;
							continue;
						}
					} else {
						// no best binary yet
						best_target_idx = i;
						continue;
					}
					break;
				}
				case PLATFORM_TYPE::VULKAN: {
					const auto& vlk_target = target.vulkan;
					
					// version check
					const auto vlk_version = vulkan_version_from_uint(vlk_target.vulkan_major, vlk_target.vulkan_minor);
					if (vlk_version > vlk_dev.vulkan_version) {
						continue;
					}
					
					const auto spirv_version = spirv_version_from_uint(vlk_target.spirv_major, vlk_target.spirv_minor);
					if (spirv_version > vlk_dev.spirv_version) {
						continue;
					}
					
					// check device target
					switch (vlk_target.device_target) {
						case decltype(vlk_target.device_target)::GENERIC:
							// assume support
							break;
						case decltype(vlk_target.device_target)::NVIDIA:
							if (vlk_dev.vendor != VENDOR::NVIDIA) {
								continue;
							}
							break;
						case decltype(vlk_target.device_target)::AMD:
							if (vlk_dev.vendor != VENDOR::AMD) {
								continue;
							}
							break;
						case decltype(vlk_target.device_target)::INTEL:
							if (vlk_dev.vendor != VENDOR::INTEL) {
								continue;
							}
							break;
					}
					
					// check SIMD width
					if (vlk_target.simd_width > 0 && (vlk_target.simd_width < dev.simd_range.x ||
													  vlk_target.simd_width > dev.simd_range.y)) {
						continue;
					}
					
					// check caps
					if (vlk_target.double_support && !dev.double_support) {
						continue;
					}
					if (vlk_target.basic_64_bit_atomics_support && !dev.basic_64_bit_atomics_support) {
						continue;
					}
					if (vlk_target.extended_64_bit_atomics_support && !dev.extended_64_bit_atomics_support) {
						continue;
					}
					if (vlk_target.basic_32_bit_float_atomics_support && !dev.basic_32_bit_float_atomics_support) {
						continue;
					}
					if (vlk_target.primitive_id_support && !dev.primitive_id_support) {
						continue;
					}
					if (vlk_target.barycentric_coord_support && !dev.barycentric_coord_support) {
						continue;
					}
					if (vlk_target.tessellation_support && (!dev.tessellation_support || dev.max_tessellation_factor < 64u)) {
						continue;
					}
					
					// -> binary is compatible, now check for best match
					if (best_target_idx != ~size_t(0)) {
						// higher version beats lower version
						const auto& best_vlk = ar.header.targets[best_target_idx].vulkan;
						const auto best_vlk_version = vulkan_version_from_uint(best_vlk.vulkan_major, best_vlk.vulkan_minor);
						const auto best_spirv_version = spirv_version_from_uint(best_vlk.spirv_major, best_vlk.spirv_minor);
						if (vlk_version > best_vlk_version) {
							best_target_idx = i;
							continue;
						}
						if (vlk_version < best_vlk_version) {
							continue; // ignore lower target
						}
						
						if (spirv_version > best_spirv_version) {
							best_target_idx = i;
							continue;
						}
						if (spirv_version < best_spirv_version) {
							continue; // ignore lower target
						}
						
						// higher SIMD width beats lower
						// NOTE: only relevant for dynamic SIMD width devices
						if (vlk_target.simd_width > best_vlk.simd_width) {
							best_target_idx = i;
							continue;
						}
						if (vlk_target.simd_width < best_vlk.simd_width) {
							continue; // ignore lower target
						}
						
						// vendor / device specific beats generic
						// NOTE: we have already filtered out non-supported device targets
						//       -> enums are ordered so that higher is better / more specific
						if (vlk_target.device_target > best_vlk.device_target) {
							best_target_idx = i;
							continue;
						}
						if (vlk_target.device_target < best_vlk.device_target) {
							continue; // ignore lower target
						}
						
						// more used/supported caps beats lower
						const auto cap_sum = (vlk_target.double_support +
											  vlk_target.basic_64_bit_atomics_support +
											  vlk_target.extended_64_bit_atomics_support +
											  vlk_target.basic_32_bit_float_atomics_support +
											  vlk_target.primitive_id_support +
											  vlk_target.barycentric_coord_support +
											  vlk_target.tessellation_support +
											  (vlk_target.max_mip_levels >= dev.max_mip_levels ? 1u : 0u));
						const auto best_cap_sum = (best_vlk.double_support +
												   best_vlk.basic_64_bit_atomics_support +
												   best_vlk.extended_64_bit_atomics_support +
												   best_vlk.basic_32_bit_float_atomics_support +
												   best_vlk.primitive_id_support +
												   best_vlk.barycentric_coord_support +
												   best_vlk.tessellation_support +
												   (best_vlk.max_mip_levels >= dev.max_mip_levels ? 1u : 0u));
						if (cap_sum > best_cap_sum) {
							best_target_idx = i;
							continue;
						}
						if (cap_sum < best_cap_sum) {
							continue; // ignore lower target
						}
						
#if defined(FLOOR_DEBUG)
						// in debug mode: soft-printf beats non-soft-printf
						if (vlk_target.soft_printf && !best_vlk.soft_printf) {
							best_target_idx = i;
							continue;
						}
#endif
					} else {
						// no best binary yet
						best_target_idx = i;
						continue;
					}
					break;
				}
			}
		}
		
		if (best_target_idx != ~size_t(0)) {
			return { &ar.binaries[best_target_idx], ar.header.targets[best_target_idx] };
		}
		return { nullptr, {} };
	}
	
	std::vector<toolchain::function_info> translate_function_info(const std::pair<const binary_dynamic_v6*, const target_v6>& bin) {
		std::vector<toolchain::function_info> ret;
		
		const uint32_t max_mip_levels = (bin.second.common.type != PLATFORM_TYPE::VULKAN ? 0u : bin.second.vulkan.max_mip_levels);
		
		for (const auto& func : bin.first->function_info) {
			toolchain::function_info entry {
				.name = func.name,
				.type = func.static_function_info.type,
				.flags = func.static_function_info.flags,
				.is_fubar = true,
				.max_mip_levels = max_mip_levels,
			};
			
			uint32_t arg_idx = 0u;
			if (entry.type != toolchain::FUNCTION_TYPE::ARGUMENT_BUFFER_STRUCT) {
				entry.required_local_size = func.static_function_info.local_size;
				entry.required_simd_width = func.static_function_info.simd_width;
			} else {
				arg_idx = func.static_function_info.argument_buffer_index;
			}
			
			for (const auto& arg : func.args) {
				entry.args.emplace_back(toolchain::arg_info {
					.size = arg.size,
					.array_extent = arg.array_extent,
					.address_space = arg.address_space,
					.access = arg.access,
					.image_type = arg.image_type,
					.flags = arg.flags,
				});
			}
			
			if (entry.type != toolchain::FUNCTION_TYPE::ARGUMENT_BUFFER_STRUCT) {
				ret.emplace_back(entry);
			} else {
				bool found_func = false;
				for (auto riter = ret.rbegin(); riter != ret.rend(); ++riter) {
					if (riter->name == entry.name) {
						found_func = true;
						if (arg_idx >= (uint32_t)riter->args.size()) {
							log_error("argument index $ is out-of-bounds for function $ with $ args", arg_idx, entry.name, riter->args.size());
							return {};
						}
						
						auto& arg = riter->args[arg_idx];
						if (!has_flag<toolchain::ARG_FLAG::ARGUMENT_BUFFER>(arg.flags)) {
							log_error("argument index $ in function $ is not an argument buffer", arg_idx, entry.name);
							return {};
						}
						
						arg.argument_buffer_info = std::move(entry);
						break;
					}
				}
				if (!found_func) {
					log_error("didn't find function $ for argument buffer", entry.name);
					return {};
				}
			}
		}
		
		floor_return_no_nrvo(ret);
	}
	
	static archive_binaries make_device_binaries_from_archive(std::unique_ptr<archive>&& ar, const std::vector<const device*>& devices) {
		// find the best matching binary for each device
		std::vector<std::pair<const universal_binary::binary_dynamic_v6*, const universal_binary::target_v6>> dev_binaries;
		dev_binaries.reserve(devices.size());
		for (const auto& dev : devices) {
			const auto best_bin = universal_binary::find_best_match_for_device(*dev, *ar);
			if (best_bin.first == nullptr) {
				log_error("no matching binary found for device $", dev->name);
				return {};
			}
			dev_binaries.emplace_back(best_bin);
		}
		
		return { std::move(ar), dev_binaries };
	}
	
	archive_binaries load_dev_binaries_from_archive(const std::string& file_name, const std::vector<const device*>& devices) {
		auto ar = universal_binary::load_archive(file_name);
		if (ar == nullptr) {
			log_error("failed to load universal binary: $", file_name);
			return {};
		}
		return make_device_binaries_from_archive(std::move(ar), devices);
	}
	
	archive_binaries load_dev_binaries_from_archive(const std::string& file_name, const device_context& ctx) {
		return load_dev_binaries_from_archive(file_name, ctx.get_devices());
	}
	
	archive_binaries load_dev_binaries_from_archive(const std::span<const uint8_t> data, const std::vector<const device*>& devices) {
		auto ar = universal_binary::load_archive(data);
		if (ar == nullptr) {
			log_error("failed to load universal binary from in-memory data (#bytes: $')", data.size_bytes());
			return {};
		}
		return make_device_binaries_from_archive(std::move(ar), devices);
	}
	
	archive_binaries load_dev_binaries_from_archive(const std::span<const uint8_t> data, const device_context& ctx) {
		return load_dev_binaries_from_archive(data, ctx.get_devices());
	}
	
} // namespace fl::universal_binary
