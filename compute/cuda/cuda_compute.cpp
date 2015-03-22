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

// NOTE: all of these options are ignored in cuda mode (they make no sense there)
void cuda_compute::init(const bool use_platform_devices floor_unused,
						const uint32_t platform_index floor_unused,
						const bool gl_sharing floor_unused,
						const unordered_set<string> device_restriction floor_unused) {
	// init cuda itself
	CU_CALL_RET(cuInit(0), "failed to initialize CUDA")
	
	// need at least 6.5 right now
	const auto to_driver_major = [](const int& version) { return version / 1000; };
	const auto to_driver_minor = [](const int& version) { return (version % 100) / 10; };
	int driver_version = 0;
	cuDriverGetVersion(&driver_version);
	if(driver_version < FLOOR_CUDA_API_VERSION_MIN) {
		log_error("at least CUDA %u.%u is required, got CUDA %u.%u!",
				  to_driver_major(FLOOR_CUDA_API_VERSION_MIN), to_driver_minor(FLOOR_CUDA_API_VERSION_MIN),
				  to_driver_major(driver_version), to_driver_minor(driver_version));
		return;
	}
	
	//
	int device_count = 0;
	CU_CALL_RET(cuDeviceGetCount(&device_count), "cuDeviceGetCount failed")
	if(device_count == 0) {
		log_error("there is no device that supports CUDA!");
		return;
	}

	// go through all available devices and check if we can use them
	devices.clear();
	unsigned int gpu_counter = (unsigned int)compute_device::TYPE::GPU0;
	unsigned int fastest_gpu_score = 0;
	for(int cur_device = 0; cur_device < device_count; ++cur_device) {
		// get and create device
		CUdevice cuda_dev;
		CU_CALL_CONT(cuDeviceGet(&cuda_dev, cur_device),
					 "failed to get device #" + to_string(cur_device))
		
		//
		char dev_name[256];
		memset(dev_name, 0, 256);
		CU_CALL_IGNORE(cuDeviceGetName(dev_name, 255, cuda_dev));
		
		// need at least sm_20 capability (fermi)
		int2 cc;
		CU_CALL_IGNORE(cuDeviceComputeCapability(&cc.x, &cc.y, cuda_dev));
		if(cc.x < 2) {
			log_error("unsupported cuda device \"%s\": at least compute capability 2.0 is required (has %u.%u)!",
					  dev_name, cc.x, cc.y);
			continue;
		}
		
		// create the context for this device
		CUcontext ctx;
		CU_CALL_CONT(cuCtxCreate(&ctx, CU_CTX_SCHED_AUTO, cuda_dev),
					 "failed to create context for device");
		CU_CALL_IGNORE(cuCtxSetCurrent(ctx));
		
		//
		devices.emplace_back(make_shared<cuda_device>());
		auto device_sptr = devices.back();
		auto& device = *(cuda_device*)device_sptr.get();
		
		// set initial/fixed attributes
		device.ctx = ctx;
		device.device_id = cuda_dev;
		device.sm = uint2(cc);
		device.type = (compute_device::TYPE)gpu_counter++;
		device.vendor = compute_device::VENDOR::NVIDIA;
		device.name = dev_name;
		device.vendor_name = "NVIDIA";
		device.version_str = to_string(cc.x) + "." + to_string(cc.y);
		device.driver_version_str = to_string(to_driver_major(driver_version)) + "." + to_string(to_driver_minor(driver_version));
		device.image_support = true;
		device.double_support = true; // true for all gpus since fermi
		
		// get all the attributes!
		size_t global_mem_size = 0;
		CU_CALL_IGNORE(cuDeviceTotalMem(&global_mem_size, cuda_dev));
		device.global_mem_size = (uint64_t)global_mem_size;
		
		int const_mem, local_mem, l2_cache_size;
		CU_CALL_IGNORE(cuDeviceGetAttribute((int*)&device.vendor_id, CU_DEVICE_ATTRIBUTE_PCI_DEVICE_ID, cuda_dev));
		CU_CALL_IGNORE(cuDeviceGetAttribute((int*)&device.units, CU_DEVICE_ATTRIBUTE_MULTIPROCESSOR_COUNT, cuda_dev));
		CU_CALL_IGNORE(cuDeviceGetAttribute(&const_mem, CU_DEVICE_ATTRIBUTE_TOTAL_CONSTANT_MEMORY, cuda_dev));
		CU_CALL_IGNORE(cuDeviceGetAttribute(&local_mem, CU_DEVICE_ATTRIBUTE_MAX_SHARED_MEMORY_PER_BLOCK, cuda_dev));
		CU_CALL_IGNORE(cuDeviceGetAttribute((int*)&device.max_registers_per_block, CU_DEVICE_ATTRIBUTE_MAX_REGISTERS_PER_BLOCK, cuda_dev));
		CU_CALL_IGNORE(cuDeviceGetAttribute(&l2_cache_size, CU_DEVICE_ATTRIBUTE_L2_CACHE_SIZE, cuda_dev));
		device.constant_mem_size = (const_mem < 0 ? 0ull : uint64_t(const_mem));
		device.local_mem_size = (local_mem < 0 ? 0ull : uint64_t(local_mem));
		device.l2_cache_size = (l2_cache_size < 0 ? 0u : uint32_t(l2_cache_size));
		
		int max_work_group_size;
		int3 max_block_dim, max_grid_dim;
		CU_CALL_IGNORE(cuDeviceGetAttribute((int*)&device.warp_size, CU_DEVICE_ATTRIBUTE_WARP_SIZE, cuda_dev));
		CU_CALL_IGNORE(cuDeviceGetAttribute(&max_work_group_size, CU_DEVICE_ATTRIBUTE_MAX_THREADS_PER_BLOCK, cuda_dev));
		CU_CALL_IGNORE(cuDeviceGetAttribute(&max_block_dim.x, CU_DEVICE_ATTRIBUTE_MAX_BLOCK_DIM_X, cuda_dev));
		CU_CALL_IGNORE(cuDeviceGetAttribute(&max_block_dim.y, CU_DEVICE_ATTRIBUTE_MAX_BLOCK_DIM_Y, cuda_dev));
		CU_CALL_IGNORE(cuDeviceGetAttribute(&max_block_dim.z, CU_DEVICE_ATTRIBUTE_MAX_BLOCK_DIM_Z, cuda_dev));
		CU_CALL_IGNORE(cuDeviceGetAttribute(&max_grid_dim.x, CU_DEVICE_ATTRIBUTE_MAX_GRID_DIM_X, cuda_dev));
		CU_CALL_IGNORE(cuDeviceGetAttribute(&max_grid_dim.y, CU_DEVICE_ATTRIBUTE_MAX_GRID_DIM_Y, cuda_dev));
		CU_CALL_IGNORE(cuDeviceGetAttribute(&max_grid_dim.z, CU_DEVICE_ATTRIBUTE_MAX_GRID_DIM_Z, cuda_dev));
		device.max_work_group_size = (size_t)max_work_group_size;
		device.max_work_item_sizes = ulong3(max_block_dim) * ulong3(max_grid_dim);
		device.max_work_group_item_sizes = size3(max_block_dim);
		
		int max_image_1d;
		int2 max_image_2d;
		int3 max_image_3d;
		CU_CALL_IGNORE(cuDeviceGetAttribute(&max_image_1d, CU_DEVICE_ATTRIBUTE_MAXIMUM_TEXTURE1D_LINEAR_WIDTH, cuda_dev));
		CU_CALL_IGNORE(cuDeviceGetAttribute(&max_image_2d.x, CU_DEVICE_ATTRIBUTE_MAXIMUM_TEXTURE2D_WIDTH, cuda_dev));
		CU_CALL_IGNORE(cuDeviceGetAttribute(&max_image_2d.y, CU_DEVICE_ATTRIBUTE_MAXIMUM_TEXTURE2D_HEIGHT, cuda_dev));
		CU_CALL_IGNORE(cuDeviceGetAttribute(&max_image_3d.x, CU_DEVICE_ATTRIBUTE_MAXIMUM_TEXTURE3D_WIDTH, cuda_dev));
		CU_CALL_IGNORE(cuDeviceGetAttribute(&max_image_3d.y, CU_DEVICE_ATTRIBUTE_MAXIMUM_TEXTURE3D_HEIGHT, cuda_dev));
		CU_CALL_IGNORE(cuDeviceGetAttribute(&max_image_3d.z, CU_DEVICE_ATTRIBUTE_MAXIMUM_TEXTURE3D_DEPTH, cuda_dev));
		device.max_image_1d_dim = size_t(max_image_1d);
		device.max_image_2d_dim = size2(max_image_2d);
		device.max_image_3d_dim = size3(max_image_3d);
		
		CU_CALL_IGNORE(cuDeviceGetAttribute((int*)&device.clock, CU_DEVICE_ATTRIBUTE_CLOCK_RATE, cuda_dev));
		CU_CALL_IGNORE(cuDeviceGetAttribute((int*)&device.mem_clock, CU_DEVICE_ATTRIBUTE_MEMORY_CLOCK_RATE, cuda_dev));
		CU_CALL_IGNORE(cuDeviceGetAttribute((int*)&device.mem_bus_width, CU_DEVICE_ATTRIBUTE_GLOBAL_MEMORY_BUS_WIDTH, cuda_dev));
		CU_CALL_IGNORE(cuDeviceGetAttribute((int*)&device.async_engine_count, CU_DEVICE_ATTRIBUTE_ASYNC_ENGINE_COUNT, cuda_dev));
		device.clock /= 1000; // to MHz
		device.mem_clock /= 1000;
		
		int exec_timeout, overlap, map_host_memory, integrated, concurrent, ecc, tcc, unified_memory;
		CU_CALL_IGNORE(cuDeviceGetAttribute(&exec_timeout, CU_DEVICE_ATTRIBUTE_KERNEL_EXEC_TIMEOUT, cuda_dev));
		CU_CALL_IGNORE(cuDeviceGetAttribute(&overlap, CU_DEVICE_ATTRIBUTE_GPU_OVERLAP, cuda_dev));
		CU_CALL_IGNORE(cuDeviceGetAttribute(&map_host_memory, CU_DEVICE_ATTRIBUTE_CAN_MAP_HOST_MEMORY, cuda_dev));
		CU_CALL_IGNORE(cuDeviceGetAttribute(&integrated, CU_DEVICE_ATTRIBUTE_INTEGRATED, cuda_dev));
		CU_CALL_IGNORE(cuDeviceGetAttribute(&concurrent, CU_DEVICE_ATTRIBUTE_CONCURRENT_KERNELS, cuda_dev));
		CU_CALL_IGNORE(cuDeviceGetAttribute(&ecc, CU_DEVICE_ATTRIBUTE_ECC_ENABLED, cuda_dev));
		CU_CALL_IGNORE(cuDeviceGetAttribute(&tcc, CU_DEVICE_ATTRIBUTE_TCC_DRIVER, cuda_dev));
		CU_CALL_IGNORE(cuDeviceGetAttribute(&unified_memory, CU_DEVICE_ATTRIBUTE_UNIFIED_ADDRESSING, cuda_dev));
		device.unified_memory = (unified_memory != 0);
		
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
		cuCtxGetLimit(&printf_buffer_size, CU_LIMIT_PRINTF_FIFO_SIZE);
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
	platform_vendor = compute_base::PLATFORM_VENDOR::NVIDIA;
	
	//
	if(fastest_gpu_device != nullptr) {
		log_debug("fastest GPU device: %s %s (score: %u)",
				  fastest_gpu_device->vendor_name, fastest_gpu_device->name, fastest_gpu_score);
		
		fastest_device = fastest_gpu_device;
		
		// make context of fastest device current
		CU_CALL_IGNORE(cuCtxSetCurrent(((cuda_device*)fastest_gpu_device.get())->ctx));
	}
}

shared_ptr<compute_queue> cuda_compute::create_queue(shared_ptr<compute_device> dev) {
	if(dev == nullptr) {
		log_error("nullptr is not a valid device!");
		return {};
	}
	
	CUstream stream;
	CU_CALL_RET(cuStreamCreate(&stream, CU_STREAM_NON_BLOCKING),
				"failed to create cuda stream", {})
	
	auto ret = make_shared<cuda_queue>(stream);
	queues.push_back(ret);
	return ret;
}

shared_ptr<compute_buffer> cuda_compute::create_buffer(const size_t& size, const COMPUTE_BUFFER_FLAG flags) {
	return make_shared<cuda_buffer>((cuda_device*)fastest_device.get(), size, flags);
}

shared_ptr<compute_buffer> cuda_compute::create_buffer(const size_t& size, void* data,
													   const COMPUTE_BUFFER_FLAG flags) {
	return make_shared<cuda_buffer>((cuda_device*)fastest_device.get(), size, data, flags);
}

shared_ptr<compute_buffer> cuda_compute::create_buffer(shared_ptr<compute_device> device,
													   const size_t& size, const COMPUTE_BUFFER_FLAG flags) {
	return make_shared<cuda_buffer>((cuda_device*)device.get(), size, flags);
}

shared_ptr<compute_buffer> cuda_compute::create_buffer(shared_ptr<compute_device> device,
													   const size_t& size, void* data,
													   const COMPUTE_BUFFER_FLAG flags) {
	return make_shared<cuda_buffer>((cuda_device*)device.get(), size, data, flags);
}

void cuda_compute::finish() {
	// TODO: !
}

void cuda_compute::flush() {
	// TODO: !
}

void cuda_compute::activate_context() {
	// TODO: !
}

void cuda_compute::deactivate_context() {
	// TODO: !
}

shared_ptr<compute_program> cuda_compute::add_program_file(const string& file_name,
														   const string additional_options) {
	string code;
	if(!file_io::file_to_string(file_name, code)) {
		return {};
	}
	return add_program_source(code, additional_options);
}

shared_ptr<compute_program> cuda_compute::add_program_source(const string& source_code,
															 const string additional_options) {
	// TODO: build for all cuda devices (if needed due to different sm_*)
	
	// compile the source code to cuda ptx
	const auto program_data = llvm_compute::compile_program(devices[0], // TODO: do for all devices
															source_code, additional_options, llvm_compute::TARGET::PTX);
	
	// jit the module / ptx code
	const CUjit_option jit_options[] {
		CU_JIT_TARGET,
		CU_JIT_GENERATE_LINE_INFO,
		CU_JIT_GENERATE_DEBUG_INFO,
		CU_JIT_MAX_REGISTERS
	};
	constexpr const auto option_count = sizeof(jit_options) / sizeof(CUjit_option);
	
	const auto& force_sm = floor::get_cuda_force_driver_sm();
	const auto& sm = ((cuda_device*)devices[0].get())->sm;
	const uint32_t sm_version = (force_sm.empty() ? sm.x * 10 + sm.y : (uint32_t)stoul(force_sm));
	
	const struct alignas(void*) {
		uint32_t ui;
	} jit_option_values[] {
		{ sm_version },
		{ .ui = (floor::get_compute_profiling() || floor::get_compute_debug()) ? 1u : 0u },
		{ .ui = floor::get_compute_debug() ? 1u : 0u },
		{ 32u } // TODO: configurable!
	};
	static_assert(option_count == (sizeof(jit_option_values) / sizeof(void*)), "mismatching option count");
	
	CUmodule program;
	CU_CALL_RET(cuModuleLoadDataEx(&program,
								   program_data.first.c_str(),
								   option_count,
								   (CUjit_option*)&jit_options[0],
								   (void**)&jit_option_values[0]),
				"failed to load/jit cuda module", {});
	log_debug("successfully created cuda program!");
	
	// TODO: print out build log - get it from llvm_compute?
	/*for(const auto& device : ctx_devices) {
		log_debug("build log: %s", cl_get_info<CL_PROGRAM_BUILD_LOG>(program, device));
	}*/
	
#if 0
	// TODO: for testing purposes: retrieve the compiled binaries again
	/*const auto binaries = cl_get_info<CL_PROGRAM_BINARIES>(program);
	for(size_t i = 0; i < binaries.size(); ++i) {
		file_io::string_to_file("binary_" + to_string(i) + ".bin", binaries[i]);
	}*/
#endif
	
	// create the program object, which in turn will create kernel objects for all kernel functions in the program
	auto ret_program = make_shared<cuda_program>(program, program_data.second);
	programs.push_back(ret_program);
	return ret_program;
}

shared_ptr<compute_program> cuda_compute::add_precompiled_program_file(const string& file_name floor_unused,
																	   const vector<llvm_compute::kernel_info>& kernel_infos floor_unused) {
	// TODO: !
	log_error("not yet supported by cuda_compute!");
	return {};
}

#endif
