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

#include <floor/compute/cuda/cuda_compute.hpp>

#if !defined(FLOOR_NO_CUDA)

#include <floor/compute/llvm_compute.hpp>
#include <floor/floor/floor.hpp>

cuda_compute::cuda_compute(const unordered_set<string> whitelist) : compute_context() {
	platform_vendor = COMPUTE_VENDOR::NVIDIA;
	
	// init cuda api functions
	if(!cuda_api_init()) {
		log_error("failed to initialize CUDA API functions");
		return;
	}
	
	// init cuda itself
	CU_CALL_RET(cu_init(0), "failed to initialize CUDA")
	
	// need at least 7.5 right now
	const auto to_driver_major = [](const int& version) { return version / 1000; };
	const auto to_driver_minor = [](const int& version) { return (version % 100) / 10; };
	int driver_version = 0;
	cu_driver_get_version(&driver_version);
	if(driver_version < FLOOR_CUDA_API_VERSION_MIN) {
		log_error("at least CUDA %u.%u is required, got CUDA %u.%u!",
				  to_driver_major(FLOOR_CUDA_API_VERSION_MIN), to_driver_minor(FLOOR_CUDA_API_VERSION_MIN),
				  to_driver_major(driver_version), to_driver_minor(driver_version));
		return;
	}
	
	//
	int device_count = 0;
	CU_CALL_RET(cu_device_get_count(&device_count), "cu_device_get_count failed")
	if(device_count == 0) {
		log_error("there is no device that supports CUDA!");
		return;
	}
	
	// sm force debug info
	if(!floor::get_cuda_force_driver_sm().empty()) {
		log_debug("forced driver sm: sm_%s", floor::get_cuda_force_driver_sm());
	}
	if(!floor::get_cuda_force_compile_sm().empty()) {
		log_debug("forced compile sm: sm_%s", floor::get_cuda_force_compile_sm());
	}

	// go through all available devices and check if we can use them
	devices.clear();
	unsigned int gpu_counter = (unsigned int)compute_device::TYPE::GPU0;
	unsigned int fastest_gpu_score = 0;
	for(int cur_device = 0; cur_device < device_count; ++cur_device) {
		// get and create device
		cu_device cuda_dev;
		CU_CALL_CONT(cu_device_get(&cuda_dev, cur_device),
					 "failed to get device #" + to_string(cur_device))
		
		//
		char dev_name[256];
		memset(dev_name, 0, 256);
		CU_CALL_IGNORE(cu_device_get_name(dev_name, 255, cuda_dev));
		
		// check whitelist
		if(!whitelist.empty()) {
			const auto lc_dev_name = core::str_to_lower(dev_name);
			bool found = false;
			for(const auto& entry : whitelist) {
				if(lc_dev_name.find(entry) != string::npos) {
					found = true;
					break;
				}
			}
			if(!found) continue;
		}
		
		// need at least sm_20 capability (fermi)
		int2 cc;
		CU_CALL_IGNORE(cu_device_compute_capability(&cc.x, &cc.y, cuda_dev));
		if(cc.x < 2) {
			log_error("unsupported cuda device \"%s\": at least compute capability 2.0 is required (has %u.%u)!",
					  dev_name, cc.x, cc.y);
			continue;
		}
		
		// create the context for this device
		cu_context ctx;
		CU_CALL_CONT(cu_ctx_create(&ctx, CU_CONTEXT_FLAGS::SCHEDULE_AUTO, cuda_dev),
					 "failed to create context for device");
		CU_CALL_IGNORE(cu_ctx_set_current(ctx));
		
		//
		devices.emplace_back(make_shared<cuda_device>());
		auto device_sptr = devices.back();
		auto& device = *(cuda_device*)device_sptr.get();
		
		// set initial/fixed attributes
		device.ctx = ctx;
		device.device_id = cuda_dev;
		device.sm = uint2(cc);
		device.type = (compute_device::TYPE)gpu_counter++;
		device.vendor = COMPUTE_VENDOR::NVIDIA;
		device.platform_vendor = COMPUTE_VENDOR::NVIDIA;
		device.name = dev_name;
		device.vendor_name = "NVIDIA";
		device.version_str = to_string(cc.x) + "." + to_string(cc.y);
		device.driver_version_str = to_string(to_driver_major(driver_version)) + "." + to_string(to_driver_minor(driver_version));
		device.image_support = true;
		device.double_support = true; // true for all gpus since fermi
#if defined(PLATFORM_X32)
		device.bitness = 32;
#elif defined(PLATFORM_X64)
		device.bitness = 64;
#endif
		device.simd_width = 32;
		
		// get all the attributes!
		size_t global_mem_size = 0;
		CU_CALL_IGNORE(cu_device_total_mem(&global_mem_size, cuda_dev));
		device.global_mem_size = (uint64_t)global_mem_size;
		
		int const_mem, local_mem, l2_cache_size;
		CU_CALL_IGNORE(cu_device_get_attribute((int*)&device.vendor_id, CU_DEVICE_ATTRIBUTE::PCI_DEVICE_ID, cuda_dev));
		CU_CALL_IGNORE(cu_device_get_attribute((int*)&device.units, CU_DEVICE_ATTRIBUTE::MULTIPROCESSOR_COUNT, cuda_dev));
		CU_CALL_IGNORE(cu_device_get_attribute(&const_mem, CU_DEVICE_ATTRIBUTE::TOTAL_CONSTANT_MEMORY, cuda_dev));
		CU_CALL_IGNORE(cu_device_get_attribute(&local_mem, CU_DEVICE_ATTRIBUTE::MAX_SHARED_MEMORY_PER_BLOCK, cuda_dev));
		CU_CALL_IGNORE(cu_device_get_attribute((int*)&device.max_registers_per_block, CU_DEVICE_ATTRIBUTE::MAX_REGISTERS_PER_BLOCK, cuda_dev));
		CU_CALL_IGNORE(cu_device_get_attribute(&l2_cache_size, CU_DEVICE_ATTRIBUTE::L2_CACHE_SIZE, cuda_dev));
		device.constant_mem_size = (const_mem < 0 ? 0ull : uint64_t(const_mem));
		device.local_mem_size = (local_mem < 0 ? 0ull : uint64_t(local_mem));
		device.local_mem_dedicated = true;
		device.l2_cache_size = (l2_cache_size < 0 ? 0u : uint32_t(l2_cache_size));
		
		int max_work_group_size;
		int3 max_block_dim, max_grid_dim;
		CU_CALL_IGNORE(cu_device_get_attribute((int*)&device.warp_size, CU_DEVICE_ATTRIBUTE::WARP_SIZE, cuda_dev));
		CU_CALL_IGNORE(cu_device_get_attribute(&max_work_group_size, CU_DEVICE_ATTRIBUTE::MAX_THREADS_PER_BLOCK, cuda_dev));
		CU_CALL_IGNORE(cu_device_get_attribute(&max_block_dim.x, CU_DEVICE_ATTRIBUTE::MAX_BLOCK_DIM_X, cuda_dev));
		CU_CALL_IGNORE(cu_device_get_attribute(&max_block_dim.y, CU_DEVICE_ATTRIBUTE::MAX_BLOCK_DIM_Y, cuda_dev));
		CU_CALL_IGNORE(cu_device_get_attribute(&max_block_dim.z, CU_DEVICE_ATTRIBUTE::MAX_BLOCK_DIM_Z, cuda_dev));
		CU_CALL_IGNORE(cu_device_get_attribute(&max_grid_dim.x, CU_DEVICE_ATTRIBUTE::MAX_GRID_DIM_X, cuda_dev));
		CU_CALL_IGNORE(cu_device_get_attribute(&max_grid_dim.y, CU_DEVICE_ATTRIBUTE::MAX_GRID_DIM_Y, cuda_dev));
		CU_CALL_IGNORE(cu_device_get_attribute(&max_grid_dim.z, CU_DEVICE_ATTRIBUTE::MAX_GRID_DIM_Z, cuda_dev));
		device.max_work_group_size = uint32_t(max_work_group_size);
		device.max_work_item_sizes = ulong3(max_block_dim) * ulong3(max_grid_dim);
		device.max_work_group_item_sizes = uint3(max_block_dim);
		
		int max_image_1d, max_image_1d_buffer;
		int2 max_image_2d;
		int3 max_image_3d;
		CU_CALL_IGNORE(cu_device_get_attribute(&max_image_1d_buffer, CU_DEVICE_ATTRIBUTE::MAXIMUM_TEXTURE1D_LINEAR_WIDTH, cuda_dev));
		CU_CALL_IGNORE(cu_device_get_attribute(&max_image_1d, CU_DEVICE_ATTRIBUTE::MAXIMUM_TEXTURE1D_WIDTH, cuda_dev));
		CU_CALL_IGNORE(cu_device_get_attribute(&max_image_2d.x, CU_DEVICE_ATTRIBUTE::MAXIMUM_TEXTURE2D_WIDTH, cuda_dev));
		CU_CALL_IGNORE(cu_device_get_attribute(&max_image_2d.y, CU_DEVICE_ATTRIBUTE::MAXIMUM_TEXTURE2D_HEIGHT, cuda_dev));
		CU_CALL_IGNORE(cu_device_get_attribute(&max_image_3d.x, CU_DEVICE_ATTRIBUTE::MAXIMUM_TEXTURE3D_WIDTH, cuda_dev));
		CU_CALL_IGNORE(cu_device_get_attribute(&max_image_3d.y, CU_DEVICE_ATTRIBUTE::MAXIMUM_TEXTURE3D_HEIGHT, cuda_dev));
		CU_CALL_IGNORE(cu_device_get_attribute(&max_image_3d.z, CU_DEVICE_ATTRIBUTE::MAXIMUM_TEXTURE3D_DEPTH, cuda_dev));
		device.max_image_1d_dim = size_t(max_image_1d);
		device.max_image_1d_buffer_dim = size_t(max_image_1d_buffer);
		device.max_image_2d_dim = size2(max_image_2d);
		device.max_image_3d_dim = size3(max_image_3d);
		
		CU_CALL_IGNORE(cu_device_get_attribute((int*)&device.clock, CU_DEVICE_ATTRIBUTE::CLOCK_RATE, cuda_dev));
		CU_CALL_IGNORE(cu_device_get_attribute((int*)&device.mem_clock, CU_DEVICE_ATTRIBUTE::MEMORY_CLOCK_RATE, cuda_dev));
		CU_CALL_IGNORE(cu_device_get_attribute((int*)&device.mem_bus_width, CU_DEVICE_ATTRIBUTE::GLOBAL_MEMORY_BUS_WIDTH, cuda_dev));
		CU_CALL_IGNORE(cu_device_get_attribute((int*)&device.async_engine_count, CU_DEVICE_ATTRIBUTE::ASYNC_ENGINE_COUNT, cuda_dev));
		device.clock /= 1000; // to MHz
		device.mem_clock /= 1000;
		
		int exec_timeout, overlap, map_host_memory, integrated, concurrent, ecc, tcc, unified_memory;
		CU_CALL_IGNORE(cu_device_get_attribute(&exec_timeout, CU_DEVICE_ATTRIBUTE::KERNEL_EXEC_TIMEOUT, cuda_dev));
		CU_CALL_IGNORE(cu_device_get_attribute(&overlap, CU_DEVICE_ATTRIBUTE::GPU_OVERLAP, cuda_dev));
		CU_CALL_IGNORE(cu_device_get_attribute(&map_host_memory, CU_DEVICE_ATTRIBUTE::CAN_MAP_HOST_MEMORY, cuda_dev));
		CU_CALL_IGNORE(cu_device_get_attribute(&integrated, CU_DEVICE_ATTRIBUTE::INTEGRATED, cuda_dev));
		CU_CALL_IGNORE(cu_device_get_attribute(&concurrent, CU_DEVICE_ATTRIBUTE::CONCURRENT_KERNELS, cuda_dev));
		CU_CALL_IGNORE(cu_device_get_attribute(&ecc, CU_DEVICE_ATTRIBUTE::ECC_ENABLED, cuda_dev));
		CU_CALL_IGNORE(cu_device_get_attribute(&tcc, CU_DEVICE_ATTRIBUTE::TCC_DRIVER, cuda_dev));
		CU_CALL_IGNORE(cu_device_get_attribute(&unified_memory, CU_DEVICE_ATTRIBUTE::UNIFIED_ADDRESSING, cuda_dev));
		device.unified_memory = (unified_memory != 0);
		
		device.basic_64_bit_atomics_support = true; // always true since sm_20
		device.extended_64_bit_atomics_support = (device.sm.x > 3 || (device.sm.x == 3 && device.sm.y >= 2)); // supported since sm_32
		
		// compute score and try to figure out which device is the fastest
		const auto compute_gpu_score = [](const cuda_device& dev) -> unsigned int {
			unsigned int multiplier = 1;
			switch(dev.sm.x) {
				case 2:
					multiplier = (dev.sm.y == 0 ? 32 : 48);
					break;
				case 3:
					multiplier = 192;
					break;
				case 5:
				default:
					multiplier = 128;
					break;
			}
			return multiplier * (dev.units * dev.clock);
		};
		
		if(fastest_gpu_device == nullptr) {
			fastest_gpu_device = device_sptr;
			fastest_gpu_score = compute_gpu_score(device);
		}
		else {
			const auto gpu_score = compute_gpu_score(device);
			if(gpu_score > fastest_gpu_score) {
				fastest_gpu_device = device_sptr;
				fastest_gpu_score = gpu_score;
			}
		}
		
		// additional info
		log_msg("mem size: %u MB (global), %u KB (local), %u KB (constant)",
				device.global_mem_size / 1024ULL / 1024ULL,
				device.local_mem_size / 1024ULL,
				device.constant_mem_size / 1024ULL);
		log_msg("host unified memory: %u", device.unified_memory);
		log_msg("max work-group size: %u", device.max_work_group_size);
		log_msg("max work-group item sizes: %v", device.max_work_group_item_sizes);
		log_msg("max cuda grid-dim: %u", max_grid_dim);
		log_msg("max work-item sizes: %v", device.max_work_item_sizes);
		
		size_t printf_buffer_size = 0;
		cu_ctx_get_limit(&printf_buffer_size, CU_LIMIT::PRINTF_FIFO_SIZE);
		log_msg("printf buffer size: %u bytes / %u MB",
				printf_buffer_size,
				printf_buffer_size / 1024ULL / 1024ULL);
		
		//
		log_debug("GPU (Units: %u, Clock: %u MHz, Memory: %u MB): %s %s, SM %s / CUDA %s",
				  device.units,
				  device.clock,
				  (unsigned int)(device.global_mem_size / 1024ull / 1024ull),
				  device.vendor_name,
				  device.name,
				  device.version_str,
				  device.driver_version_str);
	}
	
	// if absolutely no devices are supported, return (supported is still false)
	if(devices.empty()) {
		return;
	}
	// else: init successful, set supported to true
	supported = true;
	
	//
	if(fastest_gpu_device != nullptr) {
		log_debug("fastest GPU device: %s %s (score: %u)",
				  fastest_gpu_device->vendor_name, fastest_gpu_device->name, fastest_gpu_score);
		
		fastest_device = fastest_gpu_device;
		
		// make context of fastest device current
		CU_CALL_IGNORE(cu_ctx_set_current(((cuda_device*)fastest_gpu_device.get())->ctx));
	}
	
	// init shaders in cuda_image
	cuda_image::init_internal();
}

shared_ptr<compute_queue> cuda_compute::create_queue(shared_ptr<compute_device> dev) {
	if(dev == nullptr) {
		log_error("nullptr is not a valid device!");
		return {};
	}
	
	cu_stream stream;
	CU_CALL_RET(cu_stream_create(&stream, CU_STREAM_FLAGS::NON_BLOCKING),
				"failed to create cuda stream", {});
	
	auto ret = make_shared<cuda_queue>(dev, stream);
	queues.push_back(ret);
	return ret;
}

shared_ptr<compute_buffer> cuda_compute::create_buffer(const size_t& size, const COMPUTE_MEMORY_FLAG flags,
													   const uint32_t opengl_type) {
	return make_shared<cuda_buffer>((cuda_device*)fastest_device.get(), size, flags, opengl_type);
}

shared_ptr<compute_buffer> cuda_compute::create_buffer(const size_t& size, void* data,
													   const COMPUTE_MEMORY_FLAG flags,
													   const uint32_t opengl_type) {
	return make_shared<cuda_buffer>((cuda_device*)fastest_device.get(), size, data, flags, opengl_type);
}

shared_ptr<compute_buffer> cuda_compute::create_buffer(shared_ptr<compute_device> device,
													   const size_t& size, const COMPUTE_MEMORY_FLAG flags,
													   const uint32_t opengl_type) {
	return make_shared<cuda_buffer>((cuda_device*)device.get(), size, flags, opengl_type);
}

shared_ptr<compute_buffer> cuda_compute::create_buffer(shared_ptr<compute_device> device,
													   const size_t& size, void* data,
													   const COMPUTE_MEMORY_FLAG flags,
													   const uint32_t opengl_type) {
	return make_shared<cuda_buffer>((cuda_device*)device.get(), size, data, flags, opengl_type);
}


shared_ptr<compute_buffer> cuda_compute::wrap_buffer(shared_ptr<compute_device> device,
													 const uint32_t opengl_buffer,
													 const uint32_t opengl_type,
													 const COMPUTE_MEMORY_FLAG flags) {
	const auto info = compute_buffer::get_opengl_buffer_info(opengl_buffer, opengl_type, flags);
	if(!info.valid) return {};
	return make_shared<cuda_buffer>((cuda_device*)device.get(), info.size, nullptr,
									flags | COMPUTE_MEMORY_FLAG::OPENGL_SHARING,
									opengl_type, opengl_buffer);
}

shared_ptr<compute_buffer> cuda_compute::wrap_buffer(shared_ptr<compute_device> device,
													 const uint32_t opengl_buffer,
													 const uint32_t opengl_type,
													 void* data,
													 const COMPUTE_MEMORY_FLAG flags) {
	const auto info = compute_buffer::get_opengl_buffer_info(opengl_buffer, opengl_type, flags);
	if(!info.valid) return {};
	return make_shared<cuda_buffer>((cuda_device*)device.get(), info.size, data,
									flags | COMPUTE_MEMORY_FLAG::OPENGL_SHARING,
									opengl_type, opengl_buffer);
}

shared_ptr<compute_image> cuda_compute::create_image(shared_ptr<compute_device> device,
													 const uint4 image_dim,
													 const COMPUTE_IMAGE_TYPE image_type,
													 const COMPUTE_MEMORY_FLAG flags,
													 const uint32_t opengl_type) {
	return make_shared<cuda_image>((cuda_device*)device.get(), image_dim, image_type, nullptr, flags, opengl_type);
}

shared_ptr<compute_image> cuda_compute::create_image(shared_ptr<compute_device> device,
													 const uint4 image_dim,
													 const COMPUTE_IMAGE_TYPE image_type,
													 void* data,
													 const COMPUTE_MEMORY_FLAG flags,
													 const uint32_t opengl_type) {
	return make_shared<cuda_image>((cuda_device*)device.get(), image_dim, image_type, data, flags, opengl_type);
}

shared_ptr<compute_image> cuda_compute::wrap_image(shared_ptr<compute_device> device,
												   const uint32_t opengl_image,
												   const uint32_t opengl_target,
												   const COMPUTE_MEMORY_FLAG flags) {
	const auto info = compute_image::get_opengl_image_info(opengl_image, opengl_target, flags);
	if(!info.valid) return {};
	return make_shared<cuda_image>((cuda_device*)device.get(), info.image_dim, info.image_type, nullptr,
								   flags | COMPUTE_MEMORY_FLAG::OPENGL_SHARING,
								   opengl_target, opengl_image, &info);
}

shared_ptr<compute_image> cuda_compute::wrap_image(shared_ptr<compute_device> device,
												   const uint32_t opengl_image,
												   const uint32_t opengl_target,
												   void* data,
												   const COMPUTE_MEMORY_FLAG flags) {
	const auto info = compute_image::get_opengl_image_info(opengl_image, opengl_target, flags);
	if(!info.valid) return {};
	return make_shared<cuda_image>((cuda_device*)device.get(), info.image_dim, info.image_type, data,
								   flags | COMPUTE_MEMORY_FLAG::OPENGL_SHARING,
								   opengl_target, opengl_image, &info);
}

shared_ptr<cuda_program> cuda_compute::add_program(cuda_program::program_map_type&& prog_map) {
	// create the program object, which in turn will create kernel objects for all kernel functions in the program,
	// for all devices contained in the program map
	auto prog = make_shared<cuda_program>(move(prog_map));
	{
		GUARD(programs_lock);
		programs.push_back(prog);
	}
	return prog;
}

shared_ptr<compute_program> cuda_compute::add_program_file(const string& file_name,
														   const string additional_options) {
	// compile the source file for all devices in the context
	cuda_program::program_map_type prog_map;
	prog_map.reserve(devices.size());
	for(const auto& dev : devices) {
		prog_map.insert_or_assign((cuda_device*)dev.get(),
								  create_cuda_program(llvm_compute::compile_program_file(dev, file_name, additional_options,
																						 llvm_compute::TARGET::PTX)));
	}
	return add_program(move(prog_map));
}

shared_ptr<compute_program> cuda_compute::add_program_source(const string& source_code,
															 const string additional_options) {
	// compile the source code for all devices in the context
	cuda_program::program_map_type prog_map;
	prog_map.reserve(devices.size());
	for(const auto& dev : devices) {
		prog_map.insert_or_assign((cuda_device*)dev.get(),
								  create_cuda_program(llvm_compute::compile_program_file(dev, source_code, additional_options,
																						 llvm_compute::TARGET::PTX)));
	}
	return add_program(move(prog_map));
}

cuda_program::cuda_program_entry cuda_compute::create_cuda_program(pair<string, vector<llvm_compute::kernel_info>> program_data) {
	const auto& force_sm = floor::get_cuda_force_driver_sm();
	const auto& sm = ((cuda_device*)devices[0].get())->sm;
	const uint32_t sm_version = (force_sm.empty() ? sm.x * 10 + sm.y : stou(force_sm));
	cuda_program::cuda_program_entry ret;
	ret.kernels_info = program_data.second;
	
	if(!floor::get_cuda_jit_verbose() && !floor::get_compute_debug()) {
		const CU_JIT_OPTION jit_options[] {
			CU_JIT_OPTION::TARGET,
			CU_JIT_OPTION::GENERATE_LINE_INFO,
			CU_JIT_OPTION::GENERATE_DEBUG_INFO,
			CU_JIT_OPTION::MAX_REGISTERS,
			CU_JIT_OPTION::OPTIMIZATION_LEVEL,
		};
		constexpr const size_t option_count { size(jit_options) };
		
		const union alignas(void*) {
			void* ptr;
			size_t ui;
		} jit_option_values[] {
			{ .ui = sm_version },
			{ .ui = floor::get_compute_profiling() ? 1u : 0u },
			{ .ui = 0u },
			{ .ui = floor::get_cuda_max_registers() },
			{ .ui = floor::get_cuda_jit_opt_level() },
		};
		static_assert(option_count == size(jit_option_values), "mismatching option count");
		
		CU_CALL_RET(cu_module_load_data_ex(&ret.program,
										   program_data.first.c_str(),
										   option_count,
										   (CU_JIT_OPTION*)&jit_options[0],
										   (void**)&jit_option_values[0]),
					"failed to load/jit cuda module", {});
	}
	else {
		// jit the module / ptx code
		const CU_JIT_OPTION jit_options[] {
			CU_JIT_OPTION::TARGET,
			CU_JIT_OPTION::GENERATE_LINE_INFO,
			CU_JIT_OPTION::GENERATE_DEBUG_INFO,
			CU_JIT_OPTION::MAX_REGISTERS,
			CU_JIT_OPTION::OPTIMIZATION_LEVEL,
			CU_JIT_OPTION::LOG_VERBOSE,
			CU_JIT_OPTION::ERROR_LOG_BUFFER,
			CU_JIT_OPTION::INFO_LOG_BUFFER,
			CU_JIT_OPTION::ERROR_LOG_BUFFER_SIZE_BYTES,
			CU_JIT_OPTION::INFO_LOG_BUFFER_SIZE_BYTES,
		};
		constexpr const size_t option_count { size(jit_options) };
		constexpr const size_t log_size { 65536 };
		char error_log[log_size], info_log[log_size];
		error_log[0] = 0;
		info_log[0] = 0;
		const auto print_error_log = [&error_log] {
			if(error_log[0] != 0) {
				error_log[log_size - 1] = 0;
				log_error("ptx build errors: %s", error_log);
			}
		};
		
		const union alignas(void*) {
			void* ptr;
			size_t ui;
		} jit_option_values[] {
			{ .ui = sm_version },
			{ .ui = (floor::get_compute_profiling() || floor::get_compute_debug()) ? 1u : 0u },
			{ .ui = floor::get_compute_debug() ? 1u : 0u },
			{ .ui = floor::get_cuda_max_registers() },
			// opt level must be 0 when debug info is generated
			{ .ui = (floor::get_compute_debug() ? 0u : floor::get_cuda_jit_opt_level()) },
			{ .ui = 1u },
			{ .ptr = error_log },
			{ .ptr = info_log },
			{ .ui = log_size - 1u },
			{ .ui = log_size - 1u },
		};
		static_assert(option_count == size(jit_option_values), "mismatching option count");
		
		// TODO: print out llvm_compute log
		cu_link_state link_state;
		void* cubin_ptr = nullptr;
		size_t cubin_size = 0;
		CU_CALL_RET(cu_link_create(option_count,
								   (CU_JIT_OPTION*)&jit_options[0],
								   (void**)&jit_option_values[0],
								   &link_state),
					"failed to create link state", {});
		CU_CALL_ERROR_EXEC(cu_link_add_data(link_state, CU_JIT_INPUT_TYPE::PTX,
											(void*)program_data.first.c_str(), program_data.first.size(),
											nullptr, 0, nullptr, nullptr),
						   "failed to add ptx data to link state", {
							   print_error_log();
							   cu_link_destroy(link_state);
							   return ret;
						   });
		CU_CALL_ERROR_EXEC(cu_link_complete(link_state, &cubin_ptr, &cubin_size),
						   "failed to link ptx data", {
							   print_error_log();
							   cu_link_destroy(link_state);
							   return ret;
						   });
		CU_CALL_ERROR_EXEC(cu_module_load_data(&ret.program, cubin_ptr),
						   "failed to load cuda module", {
							   print_error_log();
							   if(info_log[0] != 0) {
								   info_log[log_size - 1] = 0;
								   log_debug("ptx build info: %s", info_log);
							   }
							   cu_link_destroy(link_state);
							   return ret;
						   });
		CU_CALL_NO_ACTION(cu_link_destroy(link_state),
						  "failed to destroy link state");
		
		if(info_log[0] != 0) {
			info_log[log_size - 1] = 0;
			log_debug("ptx build info: %s", info_log);
		}
	
		if(floor::get_compute_log_binaries()) {
			// for testing purposes: dump the compiled binaries again
			file_io::buffer_to_file("binary_" + to_string(sm_version) + ".cubin", (const char*)cubin_ptr, cubin_size);
		}
	}
	log_debug("successfully created cuda program!");
	
	ret.valid = true;
	return ret;
}

shared_ptr<compute_program> cuda_compute::add_precompiled_program_file(const string& file_name floor_unused,
																	   const vector<llvm_compute::kernel_info>& kernel_infos floor_unused) {
	// TODO: !
	log_error("not yet supported by cuda_compute!");
	return {};
}

shared_ptr<compute_program::program_entry> cuda_compute::create_program_entry(shared_ptr<compute_device> device floor_unused,
																			  pair<string, vector<llvm_compute::kernel_info>> program_data) {
	return make_shared<cuda_program::cuda_program_entry>(create_cuda_program(program_data));
}

#endif
