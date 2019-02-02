/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2019 Florian Ziesche
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

#include <floor/compute/universal_binary.hpp>
#include <floor/compute/compute_context.hpp>
#include <floor/compute/opencl/opencl_device.hpp>
#include <floor/compute/opencl/opencl_common.hpp>
#include <floor/compute/cuda/cuda_device.hpp>
#include <floor/compute/metal/metal_device.hpp>
#include <floor/compute/host/host_device.hpp>
#include <floor/compute/vulkan/vulkan_device.hpp>
#include <floor/core/file_io.hpp>
#include <floor/core/core.hpp>
#include <floor/threading/task.hpp>
#include <floor/floor/floor.hpp>

namespace universal_binary {
	static constexpr const uint32_t min_required_toolchain_version_v1 { 40000u };
	
	unique_ptr<archive> load_archive(const string& file_name) {
		string data;
		if (!file_io::file_to_string(file_name, data)) {
			return {};
		}
		const auto data_size = data.size();
		auto cur_size = (decltype(data_size))0;
		const uint8_t* data_ptr = (const uint8_t*)data.data();
		
		auto ar = make_unique<archive>();
		
		// parse header
		cur_size += sizeof(header_v1);
		if (cur_size > data_size) {
			log_error("universal binary %s: invalid header size, expected %u, got %u",
					  file_name, cur_size, data_size);
			return {};
		}
		const header_v1& header = *(const header_v1*)data_ptr;
		data_ptr += sizeof(header_v1);
		
		if (memcmp(header.magic, "FUBA", 4) != 0) {
			log_error("universal binary %s: invalid header magic", file_name);
			return {};
		}
		if (header.binary_format_version != binary_format_version) {
			log_error("universal binary %s: unsupported binary version %u", file_name, header.binary_format_version);
			return {};
		}
		memcpy(&ar->header.static_header, &header, sizeof(header_v1));
		
		const auto& bin_count = ar->header.static_header.binary_count;
		if (bin_count == 0) {
			// no binaries -> return early
			return ar;
		}
		
		// parse dynamic header
		ar->header.targets.resize(bin_count);
		ar->header.offsets.resize(bin_count);
		ar->header.toolchain_versions.resize(bin_count);
		ar->header.hashes.resize(bin_count);
		
		const auto targets_size = sizeof(target_v1) * bin_count;
		const auto offsets_size = sizeof(typename decltype(header_dynamic_v1::offsets)::value_type) * bin_count;
		const auto toolchain_versions_size = sizeof(typename decltype(header_dynamic_v1::toolchain_versions)::value_type) * bin_count;
		const auto hashes_size = sizeof(typename decltype(header_dynamic_v1::hashes)::value_type) * bin_count;
		const auto dyn_header_size = targets_size + offsets_size + toolchain_versions_size + hashes_size;
		cur_size += dyn_header_size;
		if (cur_size > data_size) {
			log_error("universal binary %s: invalid dynamic header size, expected %u, got %u",
					  file_name, cur_size, data_size);
			return {};
		}
		
		memcpy(ar->header.targets.data(), data_ptr, targets_size);
		data_ptr += targets_size;
		
		memcpy(ar->header.offsets.data(), data_ptr, offsets_size);
		data_ptr += offsets_size;
		
		memcpy(ar->header.toolchain_versions.data(), data_ptr, toolchain_versions_size);
		data_ptr += toolchain_versions_size;
		
		memcpy(ar->header.hashes.data(), data_ptr, hashes_size);
		data_ptr += hashes_size;
		
		// verify targets
		for (const auto& target : ar->header.targets) {
			if (target.version != target_format_version) {
				log_error("universal binary %s: unsupported target version, expected %u, got %u",
						  file_name, target_format_version, target.version);
				return {};
			}
		}
		
		// verify toolchain versions
		for (const auto& toolchain_version : ar->header.toolchain_versions) {
			if (toolchain_version < min_required_toolchain_version_v1) {
				log_error("universal binary %s: unsupported toolchain version, expected %u, got %u",
						  file_name, min_required_toolchain_version_v1, toolchain_version);
				return {};
			}
		}
		
		// parse binaries
		for (uint32_t bin_idx = 0; bin_idx < bin_count; ++bin_idx) {
			binary_dynamic_v1 bin;
			
			// verify binary offset
			if (cur_size != ar->header.offsets[bin_idx]) {
				log_error("universal binary %s: invalid binary offset, expected %u, got %u",
						  file_name, ar->header.offsets[bin_idx], cur_size);
				return {};
			}
			
			// static binary header
			cur_size += sizeof(binary_v1);
			if (cur_size > data_size) {
				log_error("universal binary %s: invalid static binary header size, expected %u, got %u",
						  file_name, cur_size, data_size);
				return {};
			}
			memcpy(&bin.static_binary_header, data_ptr, sizeof(binary_v1));
			data_ptr += sizeof(binary_v1);
			
			// pre-check sizes (we're still going to do on-the-fly checks while parsing the actual data)
			if (cur_size + bin.static_binary_header.function_info_size > data_size) {
				log_error("universal binary %s: invalid binary function info size (pre-check), expected %u, got %u",
						  file_name, cur_size + bin.static_binary_header.function_info_size, data_size);
				return {};
			}
			if (cur_size + bin.static_binary_header.function_info_size + bin.static_binary_header.binary_size > data_size) {
				log_error("universal binary %s: invalid binary size (pre-check), expected %u, got %u",
						  file_name,
						  cur_size + bin.static_binary_header.function_info_size + bin.static_binary_header.binary_size,
						  data_size);
				return {};
			}
			
			// dynamic binary header
			
			// function info
			const auto func_info_start_size = cur_size;
			for (uint32_t func_idx = 0; func_idx < bin.static_binary_header.function_count; ++func_idx) {
				function_info_dynamic_v1 func_info;
				
				// static function info
				cur_size += sizeof(function_info_v1);
				if (cur_size > data_size) {
					log_error("universal binary %s: invalid static function info size, expected %u, got %u",
							  file_name, cur_size, data_size);
					return {};
				}
				memcpy(&func_info.static_function_info, data_ptr, sizeof(function_info_v1));
				data_ptr += sizeof(function_info_v1);
				
				if (func_info.static_function_info.function_info_version != function_info_version) {
					log_error("universal binary %s: unsupported function info version %u",
							  file_name, func_info.static_function_info.function_info_version);
					return {};
				}
				
				// dynamic function info
				for (;;) {
					// name (\0 terminated)
					++cur_size;
					if (cur_size > data_size) {
						log_error("universal binary %s: invalid function info name size, expected %u, got %u",
								  file_name, cur_size, data_size);
						return {};
					}
					
					const auto ch = *data_ptr++;
					if (ch == 0) {
						break;
					}
					func_info.name += *(const char*)&ch;
				}
				
				for (uint32_t arg_idx = 0; arg_idx < func_info.static_function_info.arg_count; ++arg_idx) {
					function_info_dynamic_v1::arg_info arg;
					
					cur_size += sizeof(function_info_dynamic_v1::arg_info);
					if (cur_size > data_size) {
						log_error("universal binary %s: invalid function info arg size, expected %u, got %u",
								  file_name, cur_size, data_size);
						return {};
					}
					memcpy(&arg, data_ptr, sizeof(function_info_dynamic_v1::arg_info));
					data_ptr += sizeof(function_info_dynamic_v1::arg_info);
					
					func_info.args.emplace_back(arg);
				}
				
				bin.functions.emplace_back(func_info);
			}
			const auto func_info_end_size = cur_size;
			const auto func_info_size = func_info_end_size - func_info_start_size;
			if (func_info_size != size_t(bin.static_binary_header.function_info_size)) {
				log_error("universal binary %s: invalid binary function info size, expected %u, got %u",
						  file_name, bin.static_binary_header.function_info_size, func_info_size);
				return {};
			}
			
			// binary data
			cur_size += bin.static_binary_header.binary_size;
			if (cur_size > data_size) {
				log_error("universal binary %s: invalid binary size, expected %u, got %u",
						  file_name, cur_size, data_size);
				return {};
			}
			bin.data.resize(bin.static_binary_header.binary_size);
			memcpy(bin.data.data(), data_ptr, bin.static_binary_header.binary_size);
			data_ptr += bin.static_binary_header.binary_size;
			
			// verify binary
			const auto hash = sha_256::compute_hash(bin.data.data(), bin.data.size());
			if (hash != ar->header.hashes[bin_idx]) {
				log_error("universal binary %s: invalid binary (hash mismatch)", file_name);
				return {};
			}
			
			// binary done
			ar->binaries.emplace_back(bin);
		}
		
		return ar;
	}
	
	struct compile_return_t {
		bool success { false };
		uint32_t toolchain_version { 0 };
		llvm_toolchain::program_data prog_data;
	};
	static compile_return_t compile_target(const string& src_input,
										   const bool is_file_input,
										   const llvm_toolchain::compile_options& user_options,
										   const target& build_target) {
		auto options = user_options;
		// always ignore run-time info, we want a reproducible and specific build
		options.ignore_runtime_info = true;
		
		uint32_t toolchain_version = 0;
		shared_ptr<compute_device> dev;
		switch (build_target.type) {
			case COMPUTE_TYPE::OPENCL: {
				dev = make_shared<opencl_device>();
				const auto& cl_target = build_target.opencl;
				auto& cl_dev = (opencl_device&)*dev;
				
				toolchain_version = floor::get_opencl_toolchain_version();
				options.target = (cl_target.is_spir ? llvm_toolchain::TARGET::SPIR : llvm_toolchain::TARGET::SPIRV_OPENCL);
				if (options.target == llvm_toolchain::TARGET::SPIRV_OPENCL) {
					cl_dev.param_workaround = true;
				}
				
				cl_dev.cl_version = cl_version_from_uint(cl_target.major, cl_target.minor);
				cl_dev.c_version = cl_dev.cl_version;
				
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
				
				cl_dev.bitness = 64; // TODO: add option for this?
				
				// device type
				switch (cl_target.device_target) {
					case decltype(cl_target.device_target)::GENERIC:
						cl_dev.type = compute_device::TYPE::NONE;
						break;
					case decltype(cl_target.device_target)::GENERIC_CPU:
					case decltype(cl_target.device_target)::INTEL_CPU:
					case decltype(cl_target.device_target)::AMD_CPU:
						cl_dev.type = compute_device::TYPE::CPU0;
						break;
					case decltype(cl_target.device_target)::GENERIC_GPU:
					case decltype(cl_target.device_target)::INTEL_GPU:
					case decltype(cl_target.device_target)::AMD_GPU:
						cl_dev.type = compute_device::TYPE::GPU0;
						break;
				}
				
				// special vendor workarounds/settings
				switch (cl_target.device_target) {
					default:
						cl_dev.vendor = COMPUTE_VENDOR::UNKNOWN;
						cl_dev.platform_vendor = COMPUTE_VENDOR::UNKNOWN;
						break;
					case decltype(cl_target.device_target)::INTEL_CPU:
					case decltype(cl_target.device_target)::INTEL_GPU:
						if (options.target == llvm_toolchain::TARGET::SPIR) {
							options.cli += " -Xclang -cl-spir-intel-workarounds";
						}
						cl_dev.vendor = COMPUTE_VENDOR::INTEL;
						cl_dev.platform_vendor = COMPUTE_VENDOR::INTEL;
						break;
					case decltype(cl_target.device_target)::AMD_CPU:
					case decltype(cl_target.device_target)::AMD_GPU:
						cl_dev.vendor = COMPUTE_VENDOR::AMD;
						cl_dev.platform_vendor = COMPUTE_VENDOR::AMD;
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
							cl_dev.simd_width = 64;
							cl_dev.simd_range = { cl_dev.simd_width, cl_dev.simd_width };
							break;
					}
				}
				
				break;
			}
			case COMPUTE_TYPE::CUDA: {
				dev = make_shared<cuda_device>();
				const auto& cuda_target = build_target.cuda;
				auto& cuda_dev = (cuda_device&)*dev;
				
				toolchain_version = floor::get_cuda_toolchain_version();
				options.target = llvm_toolchain::TARGET::PTX;
				cuda_dev.sm = { cuda_target.sm_major, cuda_target.sm_minor };
				
				// handle PTX ISA version
				if ((cuda_dev.sm.x == 6 && cuda_target.ptx_isa_major < 5) ||
					(cuda_dev.sm.x >= 7 && cuda_target.ptx_isa_major < 6)) {
					log_error("invalid PTX version %u.%u for target %u",
							  cuda_target.ptx_isa_major, cuda_target.ptx_isa_minor, cuda_dev.sm);
					return {};
				}
				cuda_dev.ptx = { cuda_target.ptx_isa_major, cuda_target.ptx_isa_minor };
				options.cuda.ptx_version = cuda_target.ptx_isa_major * 10 + cuda_target.ptx_isa_minor;
				
				options.cuda.max_registers = cuda_target.max_registers;
				
				cuda_dev.image_depth_compare_support = cuda_target.image_depth_compare_support;
				cuda_dev.bitness = 64;
				
				// NOTE: other fixed device info is already set in the cuda_device constructor
				
				// TODO: handle CUBIN output
				// TODO: -> need multiple CUDA toolchains (sm_2x requires 7.5 or 8.0, sm_6x requires 8.0+, sm_7x requires 9.0+)
				if (!cuda_target.is_ptx) {
					log_error("CUBIN building not supported yet");
				}
				break;
			}
			case COMPUTE_TYPE::METAL: {
				dev = make_shared<metal_device>();
				const auto& mtl_target = build_target.metal;
				auto& mtl_dev = (metal_device&)*dev;
				
				toolchain_version = floor::get_metal_toolchain_version();
				options.target = llvm_toolchain::TARGET::AIR;
				mtl_dev.metal_version = metal_version_from_uint(mtl_target.major, mtl_target.minor);
				mtl_dev.feature_set = (mtl_target.is_ios ? 0 : 10000);
				mtl_dev.family = 1; // can't be overwritten right now
				mtl_dev.family_version = 1; // can't be overwritten right now
				mtl_dev.platform_vendor = COMPUTE_VENDOR::APPLE;
				mtl_dev.bitness = 64;
				mtl_dev.double_support = false; // always disabled for now
				
				// overwrite compute_device/metal_device defaults
				if (mtl_target.is_ios) {
					mtl_dev.vendor = COMPUTE_VENDOR::APPLE;
					mtl_dev.unified_memory = true;
					mtl_dev.image_cube_write_support = false;
					mtl_dev.simd_width = 32;
					mtl_dev.simd_range = { mtl_dev.simd_width, mtl_dev.simd_width };
					mtl_dev.max_total_local_size = 512; // family 1 - 3 (only 4+ supports 1024)
				} else {
					mtl_dev.image_cube_write_support = true;
					mtl_dev.image_cube_array_support = true;
					mtl_dev.image_cube_array_write_support = true;
					
					// sub-group/shuffle support on Metal 2.0+
					if (mtl_dev.metal_version >= METAL_VERSION::METAL_2_0) {
						mtl_dev.sub_group_support = true;
						mtl_dev.sub_group_shuffle_support = true;
					}
					
					// special vendor workarounds/settings + SIMD handling
					switch (mtl_target.device_target) {
						default:
							mtl_dev.simd_width = mtl_target.simd_width;
							break;
						case decltype(mtl_target.device_target)::NVIDIA:
							options.cli += " -Xclang -metal-nvidia-workarounds";
							mtl_dev.vendor = COMPUTE_VENDOR::NVIDIA;
							mtl_dev.simd_width = 32;
							break;
						case decltype(mtl_target.device_target)::INTEL:
							options.cli += " -Xclang -metal-intel-workarounds";
							mtl_dev.vendor = COMPUTE_VENDOR::INTEL;
							mtl_dev.simd_width = 32;
							break;
						case decltype(mtl_target.device_target)::AMD:
							// no AMD workarounds yet
							mtl_dev.vendor = COMPUTE_VENDOR::AMD;
							mtl_dev.simd_width = 64;
							break;
					}
					if (mtl_target.device_target != decltype(mtl_target.device_target)::GENERIC) {
						// fixed SIMD width must match requested one
						if (mtl_dev.simd_width != mtl_target.simd_width &&
							mtl_target.simd_width > 0) {
							log_error("invalid required SIMD width: %u", mtl_target.simd_width);
							return {};
						}
					}
					mtl_dev.simd_range = { mtl_dev.simd_width, mtl_dev.simd_width };
				}
				break;
			}
			case COMPUTE_TYPE::HOST:
				log_error("host compilation not supported yet");
				return {};
			case COMPUTE_TYPE::VULKAN: {
				dev = make_shared<vulkan_device>();
				const auto& vlk_target = build_target.vulkan;
				auto& vlk_dev = (vulkan_device&)*dev;
				
				toolchain_version = floor::get_vulkan_toolchain_version();
				options.target = llvm_toolchain::TARGET::SPIRV_VULKAN;
				vlk_dev.vulkan_version = vulkan_version_from_uint(vlk_target.vulkan_major, vlk_target.vulkan_minor);
				vlk_dev.spirv_version = spirv_version_from_uint(vlk_target.spirv_major, vlk_target.spirv_minor);
				vlk_dev.bitness = 32; // always 32-bit for now
				vlk_dev.platform_vendor = COMPUTE_VENDOR::KHRONOS;
				vlk_dev.type = compute_device::TYPE::GPU0;
				
				vlk_dev.double_support = vlk_target.double_support;
				vlk_dev.basic_64_bit_atomics_support = vlk_target.basic_64_bit_atomics_support;
				vlk_dev.extended_64_bit_atomics_support = vlk_target.extended_64_bit_atomics_support;
				
				// special vendor workarounds/settings
				switch (vlk_target.device_target) {
					default: break;
					case decltype(vlk_target.device_target)::NVIDIA:
						vlk_dev.vendor = COMPUTE_VENDOR::NVIDIA;
						break;
					case decltype(vlk_target.device_target)::AMD:
						vlk_dev.vendor = COMPUTE_VENDOR::AMD;
						break;
					case decltype(vlk_target.device_target)::INTEL:
						vlk_dev.vendor = COMPUTE_VENDOR::INTEL;
						break;
				}
				break;
			}
			case COMPUTE_TYPE::NONE:
				return {};
		}
		
		llvm_toolchain::program_data program;
		if (is_file_input) {
			program = llvm_toolchain::compile_program_file(dev, src_input, options);
		} else {
			program = llvm_toolchain::compile_program(dev, src_input, options);
		}
		if (!program.valid) {
			// TODO: build failed, proper error msg
			return {};
		}
		return { true, toolchain_version, program };
	}
	
	static bool build_archive(const string& src_input,
							  const bool is_file_input,
							  const string& dst_archive_file_name,
							  const llvm_toolchain::compile_options& options,
							  const vector<target>& targets_in) {
		// make sure we can open the output file before we start doing anything else
		file_io archive(dst_archive_file_name, file_io::OPEN_TYPE::WRITE_BINARY);
		if (!archive.is_open()) {
			log_error("can't write archive to %s", dst_archive_file_name);
			return false;
		}
		
		// create a thread pool of #logical-cpus threads that build all targets
		const auto target_count = targets_in.size();
		const auto compile_job_count = uint32_t(min(size_t(core::get_hw_thread_count()), target_count));
		
		// enqueue + sanitize targets
		safe_mutex targets_lock;
		vector<target_v1> targets;
		deque<pair<size_t, target>> remaining_targets;
		for (size_t i = 0; i < target_count; ++i) {
			auto target = targets_in[i];
			switch (target.type) {
				case COMPUTE_TYPE::NONE:
					log_error("invalid target type");
					return false;
				case COMPUTE_TYPE::OPENCL:
					target.opencl._unused = 0;
					break;
				case COMPUTE_TYPE::CUDA:
					target.cuda._unused = 0;
					break;
				case COMPUTE_TYPE::METAL:
					target.metal._unused = 0;
					break;
				case COMPUTE_TYPE::HOST:
					target.host._unused = 0;
					break;
				case COMPUTE_TYPE::VULKAN:
					target.vulkan._unused = 0;
					break;
			}
			
			targets.emplace_back(target);
			remaining_targets.emplace_back(i, target);
		}
		
		safe_mutex prog_data_lock;
		vector<unique_ptr<llvm_toolchain::program_data>> targets_prog_data(target_count);
		vector<uint32_t> targets_toolchain_version(target_count);
		vector<sha_256::hash_t> targets_hashes(target_count);
		
		atomic<uint32_t> remaining_compile_jobs { compile_job_count };
		atomic<bool> compilation_successful { true };
		for (uint32_t i = 0; i < compile_job_count; ++i) {
			task::spawn([&src_input, &is_file_input, &options,
						 &targets_lock, &remaining_targets,
						 &prog_data_lock, &targets_prog_data, &targets_toolchain_version, &targets_hashes,
						 &remaining_compile_jobs,
						 &compilation_successful]() {
				while (compilation_successful) {
					// get a target
					pair<size_t, target> build_target;
					{
						GUARD(targets_lock);
						if (remaining_targets.empty()) {
							break;
						}
						build_target = remaining_targets[0];
						remaining_targets.pop_front();
					}
					
					// compile the target
					auto compile_ret = compile_target(src_input, is_file_input, options, build_target.second);
					if (!compile_ret.success || !compile_ret.prog_data.valid) {
						compilation_successful = false;
						break;
					}
					
					// TODO: cleanup binary as in opencl_compute/vulkan_compute + in general for other backends?
					
					// for SPIR-V and AIR, the binary data is written as a file -> read it so we have it in memory
					if (compile_ret.prog_data.options.target == llvm_toolchain::TARGET::SPIRV_OPENCL ||
						compile_ret.prog_data.options.target == llvm_toolchain::TARGET::SPIRV_VULKAN ||
						compile_ret.prog_data.options.target == llvm_toolchain::TARGET::AIR) {
						string bin_data;
						if (!file_io::file_to_string(compile_ret.prog_data.data_or_filename, bin_data)) {
							compilation_successful = false;
							break;
						}
						compile_ret.prog_data.data_or_filename = move(bin_data);
					}
					
					// compute binary hash
					const auto binary_hash = sha_256::compute_hash((const uint8_t*)compile_ret.prog_data.data_or_filename.c_str(),
																   compile_ret.prog_data.data_or_filename.size());
					
					// add to program data array
					{
						auto prog_data = make_unique<llvm_toolchain::program_data>();
						*prog_data = move(compile_ret.prog_data);
						
						GUARD(prog_data_lock);
						targets_prog_data[build_target.first] = move(prog_data);
						targets_toolchain_version[build_target.first] = compile_ret.toolchain_version;
						targets_hashes[build_target.first] = binary_hash;
					}
				}
				--remaining_compile_jobs;
			}, "build_job_" + to_string(i));
		}
		
		while (remaining_compile_jobs > 0) {
			this_thread::sleep_for(250ms);
			this_thread::yield();
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
		header_dynamic_v1 header {
			.static_header = {
				.binary_format_version = binary_format_version,
				.binary_count = uint32_t(targets_prog_data.size()),
			},
			.targets = targets,
			.toolchain_versions = move(targets_toolchain_version),
			.hashes = move(targets_hashes),
		};
		// NOTE: proper offsets are written later on
		header.offsets.resize(header.static_header.binary_count);
		
		// header
		auto& ar_stream = *archive.get_filestream();
		archive.write_block(&header.static_header, sizeof(header_v1));
		archive.write_block(header.targets.data(), target_count * sizeof(typename decltype(header.targets)::value_type));
		const auto header_offsets_pos = ar_stream.tellp();
		archive.write_block(header.offsets.data(), header.offsets.size() * sizeof(typename decltype(header.offsets)::value_type));
		archive.write_block(header.toolchain_versions.data(),
							header.toolchain_versions.size() * sizeof(typename decltype(header.toolchain_versions)::value_type));
		archive.write_block(header.hashes.data(), header.hashes.size() * sizeof(typename decltype(header.hashes)::value_type));
		
		// binaries
		for (size_t i = 0; i < target_count; ++i) {
			const auto& bin = *targets_prog_data[i];
			
			// remember offset
			header.offsets[i] = uint64_t(ar_stream.tellp());
			
			// static header
			binary_dynamic_v1 bin_data {
				.static_binary_header = {
					.function_count = uint32_t(bin.functions.size()),
					.function_info_size = 0, // N/A yet
					.binary_size = uint32_t(bin.data_or_filename.size()),
				},
			};
			// NOTE: bin_data.data must not even be written/copied here
			
			// convert function info
			bin_data.functions.reserve(bin.functions.size());
			for (const auto& func : bin.functions) {
				function_info_dynamic_v1 finfo {
					.static_function_info = {
						.function_info_version = function_info_version,
						.type = func.type,
						.arg_count = uint32_t(func.args.size()),
						.local_size = func.local_size,
					},
					.name = func.name,
					.args = {}, // need proper conversion
				};
				bin_data.static_binary_header.function_info_size += sizeof(finfo.static_function_info);
				bin_data.static_binary_header.function_info_size += finfo.name.size() + 1 /* \0 */;
				
				// convert/create args
				finfo.args.reserve(func.args.size());
				for (const auto& arg : func.args) {
					finfo.args.emplace_back(function_info_dynamic_v1::arg_info {
						.argument_size = arg.size,
						.address_space = arg.address_space,
						._unused_0 = 0,
						.image_type = arg.image_type,
						.image_access = arg.image_access,
						._unused_1 = 0,
						.special_type = arg.special_type,
					});
				}
				bin_data.static_binary_header.function_info_size += sizeof(function_info_dynamic_v1::arg_info) * finfo.args.size();
				
				bin_data.functions.emplace_back(move(finfo));
			}
			
			// write static header
			archive.write_block(&bin_data.static_binary_header, sizeof(bin_data.static_binary_header));
			
			// write dynamic binary part
			for (const auto& finfo : bin_data.functions) {
				archive.write_block(&finfo.static_function_info, sizeof(finfo.static_function_info));
				archive.write_terminated_block(finfo.name, 0);
				archive.write_block(finfo.args.data(), finfo.args.size() * sizeof(typename decltype(finfo.args)::value_type));
			}
			archive.write_block(bin.data_or_filename.data(), bin.data_or_filename.size());
		}
		
		// update binary offsets now that we know them all
		ar_stream.seekp(header_offsets_pos);
		archive.write_block(header.offsets.data(), header.offsets.size() * sizeof(typename decltype(header.offsets)::value_type));
		
		return true;
	}
	
	bool build_archive_from_file(const string& src_file_name,
								 const string& dst_archive_file_name,
								 const llvm_toolchain::compile_options& options,
								 const vector<target>& targets) {
		return build_archive(src_file_name, true, dst_archive_file_name, options, targets);
	}
	
	bool build_archive_from_memory(const string& src_code,
								   const string& dst_archive_file_name,
								   const llvm_toolchain::compile_options& options,
								   const vector<target>& targets) {
		return build_archive(src_code, false, dst_archive_file_name, options, targets);
	}
	
	pair<const binary_dynamic_v1*, const target_v1>
	find_best_match_for_device(const compute_device& dev, const archive& ar) {
		if (dev.context == nullptr) return { nullptr, {} };
		
		const auto type = dev.context->get_compute_type();
		if (type == COMPUTE_TYPE::HOST) return { nullptr, {} }; // not implemented yet
		
		// for easier access
		const auto& cl_dev = (const opencl_device&)dev;
		const auto& cuda_dev = (const cuda_device&)dev;
		const auto& mtl_dev = (const metal_device&)dev;
		//const auto& host_dev = (const host_device&)dev;
		const auto& vlk_dev = (const vulkan_device&)dev;
		
		size_t best_target_idx = ~size_t(0);
		for (size_t i = 0, count = ar.header.targets.size(); i < count; ++i) {
			const auto& target = ar.header.targets[i];
			if (target.type != type) continue;
			if (ar.header.toolchain_versions[i] < min_required_toolchain_version_v1) continue;
			
			switch (target.type) {
				case COMPUTE_TYPE::NONE: continue;
				case COMPUTE_TYPE::OPENCL: {
					const auto& cl_target = target.opencl;
					
					// version check
					const auto cl_ver = cl_version_from_uint(cl_target.major, cl_target.minor);
					if (cl_ver > cl_dev.cl_version || cl_ver == OPENCL_VERSION::NONE) {
						// version too high
						continue;
					}
					
					// check SPIR-V compat (SPIR compat is always implied)
					if (!cl_target.is_spir) {
						if (cl_dev.spirv_version == SPIRV_VERSION::NONE) {
							// no SPIR-V support
							continue;
						}
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
							if (cl_dev.vendor != COMPUTE_VENDOR::INTEL) continue;
							break;
						case decltype(cl_target.device_target)::INTEL_GPU:
							if (cl_dev.is_cpu()) continue;
							if (cl_dev.vendor != COMPUTE_VENDOR::INTEL) continue;
							break;
						case decltype(cl_target.device_target)::AMD_CPU:
							if (cl_dev.is_gpu()) continue;
							if (cl_dev.vendor != COMPUTE_VENDOR::AMD) continue;
							break;
						case decltype(cl_target.device_target)::AMD_GPU:
							if (cl_dev.is_cpu()) continue;
							if (cl_dev.vendor != COMPUTE_VENDOR::AMD) continue;
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
				case COMPUTE_TYPE::CUDA: {
					const auto& cuda_target = target.cuda;
					
					// check sm
					// NOTE: if the binary is a CUBIN, the sm must exactly match
					if (!cuda_target.is_ptx) {
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
						
						// if PTX: higher sm beats lower sm
						if (cuda_target.is_ptx) {
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
				case COMPUTE_TYPE::METAL: {
					const auto& mtl_target = target.metal;
					
					// iOS binary, macOS device?
					if (mtl_target.is_ios && mtl_dev.feature_set >= 10000u) {
						continue;
					}
					// macOS binary, iOS device?
					if (!mtl_target.is_ios && mtl_dev.feature_set < 10000u) {
						continue;
					}
					
					// version check
					const auto mtl_ver = metal_version_from_uint(mtl_target.major, mtl_target.minor);
					if (mtl_ver > mtl_dev.metal_version) {
						continue;
					}
					
					// check device target
					switch (mtl_target.device_target) {
						case decltype(mtl_target.device_target)::GENERIC:
							// assume support
							break;
						case decltype(mtl_target.device_target)::NVIDIA:
							if (mtl_dev.vendor != COMPUTE_VENDOR::NVIDIA) {
								continue;
							}
							break;
						case decltype(mtl_target.device_target)::AMD:
							if (mtl_dev.vendor != COMPUTE_VENDOR::AMD) {
								continue;
							}
							break;
						case decltype(mtl_target.device_target)::INTEL:
							if (mtl_dev.vendor != COMPUTE_VENDOR::INTEL) {
								continue;
							}
							break;
					}
					if (mtl_target.is_ios && mtl_target.device_target != decltype(mtl_target.device_target)::GENERIC) {
						continue; // iOS must use GENERIC target
					}
					
					// check SIMD width
					if (mtl_target.simd_width > 0 && (mtl_target.simd_width < dev.simd_range.x ||
													  mtl_target.simd_width > dev.simd_range.y)) {
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
					} else {
						// no best binary yet
						best_target_idx = i;
						continue;
					}
					break;
				}
				case COMPUTE_TYPE::HOST: {
					// TODO: implement this
					break;
				}
				case COMPUTE_TYPE::VULKAN: {
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
							if (vlk_dev.vendor != COMPUTE_VENDOR::NVIDIA) {
								continue;
							}
							break;
						case decltype(vlk_target.device_target)::AMD:
							if (vlk_dev.vendor != COMPUTE_VENDOR::AMD) {
								continue;
							}
							break;
						case decltype(vlk_target.device_target)::INTEL:
							if (vlk_dev.vendor != COMPUTE_VENDOR::INTEL) {
								continue;
							}
							break;
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
											  vlk_target.extended_64_bit_atomics_support);
						const auto best_cap_sum = (best_vlk.double_support +
												   best_vlk.basic_64_bit_atomics_support +
												   best_vlk.extended_64_bit_atomics_support);
						if (cap_sum > best_cap_sum) {
							best_target_idx = i;
							continue;
						}
						if (cap_sum < best_cap_sum) {
							continue; // ignore lower target
						}
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
	
	vector<llvm_toolchain::function_info> translate_function_info(const vector<function_info_dynamic_v1>& functions) {
		vector<llvm_toolchain::function_info> ret;
		
		for (const auto& func : functions) {
			llvm_toolchain::function_info entry;
			entry.type = func.static_function_info.type;
			entry.local_size = func.static_function_info.local_size;
			entry.name = func.name;
			
			for (const auto& arg : func.args) {
				llvm_toolchain::function_info::arg_info arg_entry;
				arg_entry.size = arg.argument_size;
				arg_entry.address_space = arg.address_space;
				arg_entry.image_type = arg.image_type;
				arg_entry.image_access = arg.image_access;
				arg_entry.special_type = arg.special_type;
				entry.args.emplace_back(arg_entry);
			}
			
			ret.emplace_back(entry);
		}
		
		return ret;
	}
	
	archive_binaries load_dev_binaries_from_archive(const string& file_name,
													const vector<shared_ptr<compute_device>>& devices) {
		auto ar = universal_binary::load_archive(file_name);
		if (ar == nullptr) {
			log_error("failed to load universal binary: %s", file_name);
			return {};
		}
		
		// find the best matching binary for each device
		vector<pair<const universal_binary::binary_dynamic_v1*, const universal_binary::target_v1>> dev_binaries;
		dev_binaries.reserve(devices.size());
		for (const auto& dev : devices) {
			const auto best_bin = universal_binary::find_best_match_for_device(*dev, *ar);
			if (best_bin.first == nullptr) {
				log_error("no matching binary found for device %s", dev->name);
				return {};
			}
			dev_binaries.emplace_back(best_bin);
		}
		
		return { move(ar), dev_binaries };
	}
	
}
