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

#include <floor/device/cuda/cuda_context.hpp>

#if !defined(FLOOR_NO_CUDA)

#include <floor/device/toolchain.hpp>
#include <floor/floor.hpp>
#include <floor/device/universal_binary.hpp>
#include <floor/core/cpp_ext.hpp>
#include <floor/core/core.hpp>

#if !defined(FLOOR_NO_VULKAN)
#include <floor/device/vulkan/vulkan_buffer.hpp>
#include <floor/device/vulkan/vulkan_image.hpp>
#endif

namespace fl {

cuda_context::cuda_context(const DEVICE_CONTEXT_FLAGS ctx_flags, const bool has_toolchain_,
						   const std::vector<std::string> whitelist) : device_context(ctx_flags, has_toolchain_) {
	platform_vendor = VENDOR::NVIDIA;
	
	// init CUDA API functions
	if(!cuda_api_init(floor::get_cuda_use_internal_api())) {
		log_error("failed to initialize CUDA API functions");
		return;
	}
	
	// init CUDA itself
	CU_CALL_RET(cu_init(0), "failed to initialize CUDA")
	
	// need at least 12.0 right now
	const auto to_driver_major = [](const uint32_t& version) { return version / 1000; };
	const auto to_driver_minor = [](const uint32_t& version) { return (version % 100) / 10; };
	cu_driver_get_version((int*)&driver_version);
	if(driver_version < FLOOR_CUDA_API_VERSION_MIN) {
		log_error("at least CUDA $.$ is required, got CUDA $.$!",
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
	
	has_external_memory_support = cuda_can_use_external_memory();
	log_msg("CUDA external memory support: $", has_external_memory_support ? "yes" : "no");
	
	// sm force debug info
	if(!floor::get_cuda_force_driver_sm().empty()) {
		log_debug("forced driver sm: sm_$", floor::get_cuda_force_driver_sm());
	}
	if(!floor::get_cuda_force_compile_sm().empty()) {
		log_debug("forced compile sm: sm_$", floor::get_cuda_force_compile_sm());
	}

	// go through all available devices and check if we can use them
	devices.clear();
	auto gpu_counter = (uint32_t)device::TYPE::GPU0;
	uint32_t fastest_gpu_score = 0;
	for(int cur_device = 0; cur_device < device_count; ++cur_device) {
		// get and create device
		cu_device cuda_dev;
		CU_CALL_CONT(cu_device_get(&cuda_dev, cur_device),
					 "failed to get device #" + std::to_string(cur_device))
		
		//
		char dev_name[256];
		memset(dev_name, 0, std::size(dev_name));
		CU_CALL_IGNORE(cu_device_get_name(dev_name, std::size(dev_name) - 1, cuda_dev))
		
		// check whitelist
		if(!whitelist.empty()) {
			const auto lc_dev_name = core::str_to_lower(dev_name);
			bool found = false;
			for(const auto& entry : whitelist) {
				if(lc_dev_name.find(entry) != std::string::npos) {
					found = true;
					break;
				}
			}
			if(!found) continue;
		}
		
		// need at least sm_50 capability (Maxwell)
		int2 cc;
		CU_CALL_IGNORE(cu_device_compute_capability(&cc.x, &cc.y, cuda_dev))
		if (cc.x < 5) {
			log_error("unsupported CUDA device \"$\": at least compute capability 5.0 is required (has $.$)!",
					  dev_name, cc.x, cc.y);
			continue;
		}
		
		// create the context for this device
		cu_context ctx;
		CU_CALL_CONT(cu_ctx_create(&ctx, nullptr, 0, CU_CONTEXT_FLAGS::SCHEDULE_AUTO, cuda_dev),
					 "failed to create context for device")
		CU_CALL_IGNORE(cu_ctx_set_current(ctx))
		
		//
		devices.emplace_back(std::make_unique<cuda_device>());
		auto& device = (cuda_device&)*devices.back();
		
		// set initial/fixed attributes
		device.ctx = ctx;
		device.context = this;
		device.device_id = cuda_dev;
		device.sm = uint2(cc);
		device.type = (device::TYPE)gpu_counter++;
		device.name = dev_name;
		device.version_str = std::to_string(cc.x) + "." + std::to_string(cc.y);
		device.driver_version_str = std::to_string(to_driver_major(driver_version)) + "." + std::to_string(to_driver_minor(driver_version));

		// get all the attributes!
		size_t global_mem_size = 0;
		CU_CALL_IGNORE(cu_device_total_mem(&global_mem_size, cuda_dev))
		device.global_mem_size = (uint64_t)global_mem_size;
		
		int const_mem, local_mem, l2_cache_size;
		CU_CALL_IGNORE(cu_device_get_attribute((int*)&device.vendor_id, CU_DEVICE_ATTRIBUTE::PCI_DEVICE_ID, cuda_dev))
		CU_CALL_IGNORE(cu_device_get_attribute((int*)&device.units, CU_DEVICE_ATTRIBUTE::MULTIPROCESSOR_COUNT, cuda_dev))
		CU_CALL_IGNORE(cu_device_get_attribute(&const_mem, CU_DEVICE_ATTRIBUTE::TOTAL_CONSTANT_MEMORY, cuda_dev))
		CU_CALL_IGNORE(cu_device_get_attribute(&local_mem, CU_DEVICE_ATTRIBUTE::MAX_SHARED_MEMORY_PER_BLOCK, cuda_dev))
		CU_CALL_IGNORE(cu_device_get_attribute((int*)&device.max_registers_per_block, CU_DEVICE_ATTRIBUTE::MAX_REGISTERS_PER_BLOCK, cuda_dev))
		CU_CALL_IGNORE(cu_device_get_attribute(&l2_cache_size, CU_DEVICE_ATTRIBUTE::L2_CACHE_SIZE, cuda_dev))
		device.constant_mem_size = (const_mem < 0 ? 0ull : uint64_t(const_mem));
		device.local_mem_size = (local_mem < 0 ? 0ull : uint64_t(local_mem));
		device.l2_cache_size = (l2_cache_size < 0 ? 0u : uint32_t(l2_cache_size));
		
		int max_total_local_size = 0, max_coop_total_local_size = 0;
		int3 max_block_dim, max_grid_dim;
		CU_CALL_IGNORE(cu_device_get_attribute((int*)&device.warp_size, CU_DEVICE_ATTRIBUTE::WARP_SIZE, cuda_dev))
		CU_CALL_IGNORE(cu_device_get_attribute(&max_total_local_size, CU_DEVICE_ATTRIBUTE::MAX_THREADS_PER_BLOCK, cuda_dev))
		CU_CALL_IGNORE(cu_device_get_attribute(&max_coop_total_local_size, CU_DEVICE_ATTRIBUTE::MAX_THREADS_PER_MULTIPROCESSOR, cuda_dev))
		CU_CALL_IGNORE(cu_device_get_attribute(&max_block_dim.x, CU_DEVICE_ATTRIBUTE::MAX_BLOCK_DIM_X, cuda_dev))
		CU_CALL_IGNORE(cu_device_get_attribute(&max_block_dim.y, CU_DEVICE_ATTRIBUTE::MAX_BLOCK_DIM_Y, cuda_dev))
		CU_CALL_IGNORE(cu_device_get_attribute(&max_block_dim.z, CU_DEVICE_ATTRIBUTE::MAX_BLOCK_DIM_Z, cuda_dev))
		CU_CALL_IGNORE(cu_device_get_attribute(&max_grid_dim.x, CU_DEVICE_ATTRIBUTE::MAX_GRID_DIM_X, cuda_dev))
		CU_CALL_IGNORE(cu_device_get_attribute(&max_grid_dim.y, CU_DEVICE_ATTRIBUTE::MAX_GRID_DIM_Y, cuda_dev))
		CU_CALL_IGNORE(cu_device_get_attribute(&max_grid_dim.z, CU_DEVICE_ATTRIBUTE::MAX_GRID_DIM_Z, cuda_dev))
		device.max_total_local_size = uint32_t(max_total_local_size);
		device.max_coop_total_local_size = uint32_t(max_coop_total_local_size);
		device.max_resident_local_size = device.max_coop_total_local_size;
		device.max_global_size = ulong3(max_block_dim) * ulong3(max_grid_dim);
		device.max_local_size = uint3(max_block_dim);
		
		int max_image_1d, max_image_1d_buffer, max_image_1d_mip_map;
		int2 max_image_2d, max_image_2d_mip_map;
		int3 max_image_3d;
		CU_CALL_IGNORE(cu_device_get_attribute(&max_image_1d_buffer, CU_DEVICE_ATTRIBUTE::MAXIMUM_TEXTURE1D_LINEAR_WIDTH, cuda_dev))
		CU_CALL_IGNORE(cu_device_get_attribute(&max_image_1d, CU_DEVICE_ATTRIBUTE::MAXIMUM_TEXTURE1D_WIDTH, cuda_dev))
		CU_CALL_IGNORE(cu_device_get_attribute(&max_image_2d.x, CU_DEVICE_ATTRIBUTE::MAXIMUM_TEXTURE2D_WIDTH, cuda_dev))
		CU_CALL_IGNORE(cu_device_get_attribute(&max_image_2d.y, CU_DEVICE_ATTRIBUTE::MAXIMUM_TEXTURE2D_HEIGHT, cuda_dev))
		CU_CALL_IGNORE(cu_device_get_attribute(&max_image_3d.x, CU_DEVICE_ATTRIBUTE::MAXIMUM_TEXTURE3D_WIDTH, cuda_dev))
		CU_CALL_IGNORE(cu_device_get_attribute(&max_image_3d.y, CU_DEVICE_ATTRIBUTE::MAXIMUM_TEXTURE3D_HEIGHT, cuda_dev))
		CU_CALL_IGNORE(cu_device_get_attribute(&max_image_3d.z, CU_DEVICE_ATTRIBUTE::MAXIMUM_TEXTURE3D_DEPTH, cuda_dev))
		CU_CALL_IGNORE(cu_device_get_attribute(&max_image_2d_mip_map.x, CU_DEVICE_ATTRIBUTE::MAXIMUM_TEXTURE2D_MIPMAPPED_WIDTH, cuda_dev))
		CU_CALL_IGNORE(cu_device_get_attribute(&max_image_2d_mip_map.y, CU_DEVICE_ATTRIBUTE::MAXIMUM_TEXTURE2D_MIPMAPPED_HEIGHT, cuda_dev))
		CU_CALL_IGNORE(cu_device_get_attribute(&max_image_1d_mip_map, CU_DEVICE_ATTRIBUTE::MAXIMUM_TEXTURE1D_MIPMAPPED_WIDTH, cuda_dev))
		device.max_image_1d_dim = uint32_t(max_image_1d);
		device.max_image_1d_buffer_dim = size_t(max_image_1d_buffer);
		device.max_image_2d_dim = uint2(max_image_2d);
		device.max_image_3d_dim = uint3(max_image_3d);
		// -> image_mip_level_count
		device.max_mip_levels = image_mip_level_count_from_max_dim(uint32_t(std::max(max_image_2d_mip_map.max_element(),
																					 max_image_1d_mip_map)));
		
		CU_CALL_IGNORE(cu_device_get_attribute((int*)&device.clock, CU_DEVICE_ATTRIBUTE::CLOCK_RATE, cuda_dev))
		CU_CALL_IGNORE(cu_device_get_attribute((int*)&device.mem_clock, CU_DEVICE_ATTRIBUTE::MEMORY_CLOCK_RATE, cuda_dev))
		CU_CALL_IGNORE(cu_device_get_attribute((int*)&device.mem_bus_width, CU_DEVICE_ATTRIBUTE::GLOBAL_MEMORY_BUS_WIDTH, cuda_dev))
		CU_CALL_IGNORE(cu_device_get_attribute((int*)&device.async_engine_count, CU_DEVICE_ATTRIBUTE::ASYNC_ENGINE_COUNT, cuda_dev))
		device.clock /= 1000; // to MHz
		device.mem_clock /= 1000;
		
		int exec_timeout, overlap, map_host_memory, integrated, concurrent, ecc, tcc, unified_memory, coop_launch;
		CU_CALL_IGNORE(cu_device_get_attribute(&exec_timeout, CU_DEVICE_ATTRIBUTE::KERNEL_EXEC_TIMEOUT, cuda_dev))
		CU_CALL_IGNORE(cu_device_get_attribute(&overlap, CU_DEVICE_ATTRIBUTE::GPU_OVERLAP, cuda_dev))
		CU_CALL_IGNORE(cu_device_get_attribute(&map_host_memory, CU_DEVICE_ATTRIBUTE::CAN_MAP_HOST_MEMORY, cuda_dev))
		CU_CALL_IGNORE(cu_device_get_attribute(&integrated, CU_DEVICE_ATTRIBUTE::INTEGRATED, cuda_dev))
		CU_CALL_IGNORE(cu_device_get_attribute(&concurrent, CU_DEVICE_ATTRIBUTE::CONCURRENT_KERNELS, cuda_dev))
		CU_CALL_IGNORE(cu_device_get_attribute(&ecc, CU_DEVICE_ATTRIBUTE::ECC_ENABLED, cuda_dev))
		CU_CALL_IGNORE(cu_device_get_attribute(&tcc, CU_DEVICE_ATTRIBUTE::TCC_DRIVER, cuda_dev))
		CU_CALL_IGNORE(cu_device_get_attribute(&unified_memory, CU_DEVICE_ATTRIBUTE::UNIFIED_ADDRESSING, cuda_dev))
		CU_CALL_IGNORE(cu_device_get_attribute(&coop_launch, CU_DEVICE_ATTRIBUTE::COOPERATIVE_LAUNCH_SUPPORTED, cuda_dev))
		device.unified_memory = (unified_memory != 0);
		device.cooperative_kernel_support = (coop_launch != 0);
		
		// (de)compression support
		int compression_support = 0;
		int decompression_algos = 0;
		int max_decompression_len = 0;
		std::string decompression_algos_str;
		CU_CALL_IGNORE(cu_device_get_attribute(&compression_support, CU_DEVICE_ATTRIBUTE::GENERIC_COMPRESSION_SUPPORTED, cuda_dev))
		if (driver_version >= 12080) {
			CU_CALL_IGNORE(cu_device_get_attribute(&decompression_algos, CU_DEVICE_ATTRIBUTE::MEM_DECOMPRESS_ALGORITHM_MASK, cuda_dev))
			CU_CALL_IGNORE(cu_device_get_attribute(&max_decompression_len, CU_DEVICE_ATTRIBUTE::MEM_DECOMPRESS_MAXIMUM_LENGTH, cuda_dev))
			if (has_flag<CU_MEM_DECOMPRESS_ALGORITHM::DEFLATE>(std::bit_cast<CU_MEM_DECOMPRESS_ALGORITHM>(decompression_algos))) {
				decompression_algos_str += "deflate ";
			}
			if (has_flag<CU_MEM_DECOMPRESS_ALGORITHM::SNAPPY>(std::bit_cast<CU_MEM_DECOMPRESS_ALGORITHM>(decompression_algos))) {
				decompression_algos_str += "snappy ";
			}
			if (has_flag<CU_MEM_DECOMPRESS_ALGORITHM::LZ4>(std::bit_cast<CU_MEM_DECOMPRESS_ALGORITHM>(decompression_algos))) {
				decompression_algos_str += "LZ4 ";
			}
		}
		
		// get UUID
		cu_uuid uuid;
		CU_CALL_IGNORE(cu_device_get_uuid(&uuid, cuda_dev), "failed to retrieve device UUID")
		std::copy_n(std::begin(uuid.bytes), device.uuid.size(), std::begin(device.uuid));
		device.has_uuid = true;
		
		// enable h/w depth compare when using the internal API and everything is alright
		if(cuda_can_use_internal_api()) {
			log_msg("using internal API");
			device.image_depth_compare_support = true;
			
			// exchange the device sampler init function with our own + store the driver function in the device for later use
			auto device_ptr = *(void**)(uintptr_t(device.ctx) + cuda_device_in_ctx_offset);
			auto sampler_func_ptr = (void**)(uintptr_t(device_ptr) + cuda_device_sampler_func_offset);
			(void*&)device.sampler_init_func_ptr = *sampler_func_ptr;
			*sampler_func_ptr = (void*)&cuda_image::internal_device_sampler_init;
		}
		
		// set max supported PTX version and min required PTX version
		if (driver_version < 12010) {
			device.ptx = { 8, 0 };
		} else if (driver_version < 12020) {
			device.ptx = { 8, 1 };
		} else if (driver_version < 12030) {
			device.ptx = { 8, 2 };
		} else if (driver_version < 12040) {
			device.ptx = { 8, 3 };
		} else if (driver_version < 12050) {
			device.ptx = { 8, 4 };
		} else if (driver_version < 12070) {
			// 12.5 and 12.6 both use PTX 8.5
			device.ptx = { 8, 5 };
		} else if (driver_version < 12080) {
			device.ptx = { 8, 6 };
		} else if (driver_version < 12090) {
			device.ptx = { 8, 7 };
		} else {
			device.ptx = { 8, 8 };
		}
		
		if (device.sm.x < 9 || (device.sm.x == 9 && device.sm.y == 0)) {
			device.min_req_ptx = { 8, 0 };
		} else if (device.sm.x < 10 || (device.sm.x == 10 && device.sm.y <= 1)) {
			device.min_req_ptx = { 8, 6 };
		} else if (device.sm.x < 12 || (device.sm.x == 12 && device.sm.y == 0)) {
			device.min_req_ptx = { 8, 7 };
		} else if ((device.sm.x == 10 && device.sm.y <= 3) ||
				   (device.sm.x == 12 && device.sm.y <= 1)) {
			device.min_req_ptx = { 8, 8 };
		} else {
			device.min_req_ptx = { 8, 8 };
		}
		
		// additional info
		log_msg("mem size: $' MB (global), $' KB (local), $' KB (constant)",
				device.global_mem_size / 1024ULL / 1024ULL,
				device.local_mem_size / 1024ULL,
				device.constant_mem_size / 1024ULL);
		log_msg("host unified memory: $'", device.unified_memory);
		log_msg("coop kernels: $", device.cooperative_kernel_support);
		log_msg("max total local size: $' (max resident $')", device.max_total_local_size, device.max_resident_local_size);
		log_msg("max local size: $'", device.max_local_size);
		log_msg("max global size: $'", device.max_global_size);
		log_msg("max grid-dim: $", max_grid_dim);
		log_msg("max mip-levels: $", device.max_mip_levels);
		log_msg("generic compression support: $", compression_support);
		log_msg("decompression: max length $', algorithms $($)", max_decompression_len, decompression_algos_str, decompression_algos);
		
		size_t printf_buffer_size = 0;
		cu_ctx_get_limit(&printf_buffer_size, CU_LIMIT::PRINTF_FIFO_SIZE);
		log_msg("printf buffer size: $ bytes / $ MB",
				printf_buffer_size,
				printf_buffer_size / 1024ULL / 1024ULL);
		
		//
		log_debug("GPU (Units: $', Clock: $' MHz, Memory: $' MB): $ $, SM $ / CUDA $",
				  device.units,
				  device.clock,
				  uint32_t(device.global_mem_size / 1024ull / 1024ull),
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
	
	// figure out the fastest device
	for (const auto& device : devices) {
		// compute score and try to figure out which device is the fastest
		const auto compute_gpu_score = [](const cuda_device& dev) -> uint32_t {
			uint32_t multiplier = 1;
			switch(dev.sm.x) {
				case 5:
					// sm_5x: 128 cores/sm
					multiplier = 128;
					break;
				case 6:
					// sm_60: 64 cores/sm, sm_61/sm_62: 128 cores/sm
					multiplier = (dev.sm.y == 0 ? 64 : 128);
					break;
				case 7:
					// sm_70/sm_72/sm_73/sm_75: 64 cores/sm
					multiplier = 64;
					break;
				case 8:
					// sm_80/sm_82: 64 cores/sm, sm_86+: 128 cores/sm
					multiplier = (dev.sm.y < 6 ? 64 : 128);
					break;
				case 9:
					// sm_90: 128 cores/sm
					multiplier = 128;
					break;
				case 10:
					// sm_100/sm_101: 128 cores/sm
					multiplier = 128;
					break;
				case 12:
					// sm_120: 128 cores/sm
					multiplier = 128;
					break;
				default:
					// default to 128 cores/sm
					multiplier = 128;
					break;
			}
			return multiplier * (dev.units * dev.clock);
		};
		
		if (fastest_gpu_device == nullptr) {
			fastest_gpu_device = device.get();
			fastest_gpu_score = compute_gpu_score((const cuda_device&)*device);
		} else {
			const auto gpu_score = compute_gpu_score((const cuda_device&)*device);
			if (gpu_score > fastest_gpu_score) {
				fastest_gpu_device = device.get();
				fastest_gpu_score = gpu_score;
			}
		}
	}
	
	//
	if(fastest_gpu_device != nullptr) {
		log_debug("fastest GPU device: $ $ (score: $)",
				  fastest_gpu_device->vendor_name, fastest_gpu_device->name, fastest_gpu_score);
		
		fastest_device = fastest_gpu_device;
		
		// make context of fastest device current
		(void)((const cuda_device*)fastest_gpu_device)->make_context_current();
	}
	
	// create a default queue for each device
	for(const auto& dev : devices) {
		default_queues.emplace(dev.get(), create_queue(*dev));
	}
	
	// init shaders in cuda_image
	cuda_image::init_internal(this);
}

std::shared_ptr<device_queue> cuda_context::create_queue(const device& dev) const {
	// ensure context is set for the calling thread
	const auto& cuda_dev = (const cuda_device&)dev;
	if (cuda_dev.ctx != nullptr) {
		if (!cuda_dev.make_context_current()) {
			return {};
		}
	}
	
	cu_stream stream;
	CU_CALL_RET(cu_stream_create(&stream, CU_STREAM_FLAGS::NON_BLOCKING),
				"failed to create CUDA stream", {})
	
	auto ret = std::make_shared<cuda_queue>(dev, stream);
	queues.push_back(ret);
	return ret;
}

const device_queue* cuda_context::get_device_default_queue(const device& dev) const {
	const auto def_queue_iter = default_queues.find(&dev);
	if (def_queue_iter != default_queues.end()) {
		return def_queue_iter->second.get();
	}
	// only happens if the context is invalid (the default queues haven't been created)
	log_error("no default queue for this device exists yet!");
	return nullptr;
}

std::unique_ptr<device_fence> cuda_context::create_fence(const device_queue&) const {
	log_error("fence creation not yet supported by cuda_context!");
	return {};
}

std::shared_ptr<device_buffer> cuda_context::create_buffer(const device_queue& cqueue,
													   const size_t& size, const MEMORY_FLAG flags) const {
	return add_resource(std::make_shared<cuda_buffer>(cqueue, size, flags));
}

std::shared_ptr<device_buffer> cuda_context::create_buffer(const device_queue& cqueue,
													   std::span<uint8_t> data,
													   const MEMORY_FLAG flags) const {
	return add_resource(std::make_shared<cuda_buffer>(cqueue, data.size_bytes(), data, flags));
}

std::shared_ptr<device_buffer> cuda_context::wrap_buffer(const device_queue& cqueue,
													 vulkan_buffer& vk_buffer,
													 const MEMORY_FLAG flags) const {
#if !defined(FLOOR_NO_VULKAN)
	return add_resource(std::make_shared<cuda_buffer>(cqueue, vk_buffer.get_size(), std::span<uint8_t> {},
												 flags | MEMORY_FLAG::VULKAN_SHARING, &vk_buffer));
#else
	return device_context::wrap_buffer(cqueue, vk_buffer, flags);
#endif
}

std::shared_ptr<device_image> cuda_context::create_image(const device_queue& cqueue,
													 const uint4 image_dim,
													 const IMAGE_TYPE image_type,
													 std::span<uint8_t> data,
													 const MEMORY_FLAG flags,
													 const uint32_t mip_level_limit) const {
	return add_resource(std::make_shared<cuda_image>(cqueue, image_dim, image_type, data, flags, nullptr, mip_level_limit));
}

std::shared_ptr<device_image> cuda_context::wrap_image(const device_queue& cqueue,
												   vulkan_image& vk_image,
												   const MEMORY_FLAG flags) const {
#if !defined(FLOOR_NO_VULKAN)
	return add_resource(std::make_shared<cuda_image>(cqueue, vk_image.get_image_dim(), vk_image.get_image_type(), std::span<uint8_t> {},
												flags | MEMORY_FLAG::VULKAN_SHARING, (device_image*)&vk_image));
#else
	return device_context::wrap_image(cqueue, vk_image, flags);
#endif
}

std::shared_ptr<device_program> cuda_context::create_program_from_archive_binaries(universal_binary::archive_binaries& bins) {
	// create the program
	cuda_program::program_map_type prog_map;
	for (size_t i = 0, dev_count = devices.size(); i < dev_count; ++i) {
		const auto cuda_dev = (const cuda_device*)devices[i].get();
		const auto& dev_best_bin = bins.dev_binaries[i];
		const auto func_info = universal_binary::translate_function_info(dev_best_bin);
		// TODO: handle CUBIN
		prog_map.insert_or_assign(cuda_dev,
								  create_cuda_program_internal(*cuda_dev,
															   dev_best_bin.first->data,
															   func_info,
															   dev_best_bin.second.cuda.max_registers,
															   false /* TODO: true? */));
	}
	return add_program(std::move(prog_map));
}

std::shared_ptr<device_program> cuda_context::add_universal_binary(const std::string& file_name) {
	auto bins = universal_binary::load_dev_binaries_from_archive(file_name, *this);
	if (bins.ar == nullptr || bins.dev_binaries.empty()) {
		log_error("failed to load universal binary: $", file_name);
		return {};
	}
	return create_program_from_archive_binaries(bins);
}

std::shared_ptr<device_program> cuda_context::add_universal_binary(const std::span<const uint8_t> data) {
	auto bins = universal_binary::load_dev_binaries_from_archive(data, *this);
	if (bins.ar == nullptr || bins.dev_binaries.empty()) {
		log_error("failed to load universal binary (in-memory data)");
		return {};
	}
	return create_program_from_archive_binaries(bins);
}

std::shared_ptr<cuda_program> cuda_context::add_program(cuda_program::program_map_type&& prog_map) {
	// create the program object, which in turn will create function objects for all function functions in the program,
	// for all devices contained in the program map
	auto prog = std::make_shared<cuda_program>(std::move(prog_map));
	{
		GUARD(programs_lock);
		programs.push_back(prog);
	}
	return prog;
}

std::shared_ptr<device_program> cuda_context::add_program_file(const std::string& file_name,
														   const std::string additional_options) {
	compile_options options { .cli = additional_options };
	return add_program_file(file_name, options);
}

std::shared_ptr<device_program> cuda_context::add_program_file(const std::string& file_name,
														   compile_options options) {
	// compile the source file for all devices in the context
	cuda_program::program_map_type prog_map;
	options.target = toolchain::TARGET::PTX;
	for(const auto& dev : devices) {
		const auto cuda_dev = (const cuda_device*)dev.get();
		prog_map.insert_or_assign(cuda_dev,
								  create_cuda_program(*cuda_dev, toolchain::compile_program_file(*dev, file_name, options)));
	}
	return add_program(std::move(prog_map));
}

std::shared_ptr<device_program> cuda_context::add_program_source(const std::string& source_code,
															 const std::string additional_options) {
	compile_options options { .cli = additional_options };
	return add_program_source(source_code, options);
}

std::shared_ptr<device_program> cuda_context::add_program_source(const std::string& source_code,
															 compile_options options) {
	// compile the source code for all devices in the context
	cuda_program::program_map_type prog_map;
	options.target = toolchain::TARGET::PTX;
	for(const auto& dev : devices) {
		const auto cuda_dev = (const cuda_device*)dev.get();
		prog_map.insert_or_assign(cuda_dev,
								  create_cuda_program(*cuda_dev, toolchain::compile_program(*dev, source_code, options)));
	}
	return add_program(std::move(prog_map));
}

cuda_program::cuda_program_entry cuda_context::create_cuda_program(const cuda_device& dev,
																   toolchain::program_data program) {
	if(!program.valid) {
		return {};
	}
	return create_cuda_program_internal(dev,
										std::span { (const uint8_t*)program.data_or_filename.data(), program.data_or_filename.size() },
										program.function_info, program.options.cuda.max_registers,
										program.options.silence_debug_output);
}

cuda_program::cuda_program_entry cuda_context::create_cuda_program_internal(const cuda_device& dev,
																			const std::span<const uint8_t> program,
																			const std::vector<toolchain::function_info>& functions,
																			const uint32_t max_registers,
																			const bool silence_debug_output) {
	const auto& force_sm = floor::get_cuda_force_driver_sm();
	const auto& sm = dev.sm;
	const uint32_t sm_version = (force_sm.empty() ? sm.x * 10 + sm.y : stou(force_sm));
	cuda_program::cuda_program_entry ret;
	ret.functions = functions;
	
	// must make the device ctx current for this thread (if it isn't already)
	if (!dev.make_context_current()) {
		return {};
	}
	
	if (!floor::get_cuda_jit_verbose() && !floor::get_toolchain_debug() && !floor::get_toolchain_log_binaries()) {
		static constexpr const CU_JIT_OPTION jit_options[] {
			CU_JIT_OPTION::TARGET,
			CU_JIT_OPTION::GENERATE_LINE_INFO,
			CU_JIT_OPTION::GENERATE_DEBUG_INFO,
			CU_JIT_OPTION::MAX_REGISTERS,
			CU_JIT_OPTION::OPTIMIZATION_LEVEL,
		};
		constexpr const size_t option_count { std::size(jit_options) };
		
		const union alignas(void*) {
			void* ptr;
			size_t ui;
		} jit_option_values[] {
			{ .ui = sm_version },
			{ .ui = floor::get_toolchain_profiling() ? 1u : 0u },
			{ .ui = 0u },
			{ .ui = max_registers != 0 ? max_registers : floor::get_cuda_max_registers() },
			{ .ui = floor::get_cuda_jit_opt_level() },
		};
		static_assert(option_count == std::size(jit_option_values), "mismatching option count");
		
		CU_CALL_RET(cu_module_load_data_ex(&ret.program,
										   program.data(),
										   option_count,
										   &jit_options[0],
										   (const void* const*)&jit_option_values[0]),
					"failed to load/JIT compile the CUDA module", {})
	} else {
		// JIT compile the module / PTX code
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
		constexpr const size_t option_count { std::size(jit_options) };
		constexpr const size_t log_size { 65536 };
		std::string error_log(log_size, 0);
		std::string info_log(log_size, 0);
		
		const union alignas(void*) {
			void* ptr;
			size_t ui;
		} jit_option_values[] {
			{ .ui = sm_version },
			{ .ui = (floor::get_toolchain_profiling() || floor::get_toolchain_debug()) ? 1u : 0u },
			{ .ui = floor::get_toolchain_debug() ? 1u : 0u },
			{ .ui = max_registers != 0 ? max_registers : floor::get_cuda_max_registers() },
			// opt level must be 0 when debug info is generated
			{ .ui = (floor::get_toolchain_debug() ? 0u : floor::get_cuda_jit_opt_level()) },
			{ .ui = 1u },
			{ .ptr = error_log.data() },
			{ .ptr = info_log.data() },
			{ .ui = log_size - 1u }, // @8 - do not change position
			{ .ui = log_size - 1u }, // @9 - do not change position
		};
		static_assert(option_count == std::size(jit_option_values), "mismatching option count");
		
		const auto print_error_log = [&error_log, &jit_option_values] {
			if (error_log[0] != 0) {
				const auto log_size_actual = std::min(log_size - 1u, jit_option_values[8].ui);
				error_log.resize(log_size_actual);
				log_error("PTX build errors: $", error_log);
			}
		};
		
		// TODO: print out toolchain log
		cu_link_state link_state;
		void* cubin_ptr = nullptr;
		size_t cubin_size = 0;
		CU_CALL_RET(cu_link_create(option_count,
								   (const CU_JIT_OPTION*)&jit_options[0],
								   (const void* const*)&jit_option_values[0],
								   &link_state),
					"failed to create link state", {})
FLOOR_PUSH_AND_IGNORE_WARNING(nrvo)
		CU_CALL_ERROR_EXEC(cu_link_add_data(link_state, CU_JIT_INPUT_TYPE::PTX,
											program.data(), program.size_bytes(),
											nullptr, 0, nullptr, nullptr),
						   "failed to add PTX data to link state", {
							   print_error_log();
							   cu_link_destroy(link_state);
							   return ret;
						   })
		CU_CALL_ERROR_EXEC(cu_link_complete(link_state, &cubin_ptr, &cubin_size),
						   "failed to link PTX data", {
							   print_error_log();
							   cu_link_destroy(link_state);
							   return ret;
						   })
		CU_CALL_ERROR_EXEC(cu_module_load_data(&ret.program, cubin_ptr),
						   "failed to load CUDA module", {
							   print_error_log();
							   if (info_log[0] != 0) {
								   info_log[log_size - 1] = 0;
								   log_debug("PTX build info: $", info_log);
							   }
							   cu_link_destroy(link_state);
							   return ret;
						   })
FLOOR_POP_WARNINGS()
		if (floor::get_toolchain_log_binaries()) {
			// for testing purposes: dump the compiled binaries again
			auto bin_name = "binary_" + std::to_string(sm_version);
			if (!functions.empty() && !functions[0].name.empty()) {
				bin_name += "_" + functions[0].name;
			}
			bin_name += ".bin";
			file_io::buffer_to_file(bin_name, (const char*)cubin_ptr, cubin_size);
		}
		CU_CALL_NO_ACTION(cu_link_destroy(link_state),
						  "failed to destroy link state")
		
		const auto log_size_actual = std::min(log_size - 1u, jit_option_values[9].ui);
		if (info_log[0] != 0 && !silence_debug_output) {
			info_log.resize(log_size_actual);
			log_debug("PTX build info: $", info_log);
		}
		if (jit_option_values[8].ui > 0) {
			print_error_log();
		}
	}
	
	if (!silence_debug_output) {
		log_debug("successfully created CUDA program!");
	}
	
	ret.valid = true;
	floor_return_no_nrvo(ret);
}

std::shared_ptr<device_program> cuda_context::add_precompiled_program_file(const std::string& file_name floor_unused,
																	   const std::vector<toolchain::function_info>& functions floor_unused) {
	// TODO: !
	log_error("not yet supported by cuda_context!");
	return {};
}

std::shared_ptr<device_program::program_entry> cuda_context::create_program_entry(const device& dev,
																			  toolchain::program_data program,
																			  const toolchain::TARGET) {
	return std::make_shared<cuda_program::cuda_program_entry>(create_cuda_program((const cuda_device&)dev, program));
}

device_context::memory_usage_t cuda_context::get_memory_usage(const device& dev) const {
	const auto& cu_dev = (const cuda_device&)dev;
	if (!cu_dev.make_context_current()) {
		return {};
	}
	size_t free = 0u, total = 0u;
	CU_CALL_RET(cu_mem_get_info(&free, &total), "failed to query device memory info", {})
	memory_usage_t ret {
		.global_mem_used = (free <= total ? total - free : total),
		.global_mem_total = total,
		.heap_used = 0u,
		.heap_total = 0u,
	};
	return ret;
}

} // namespace fl

#endif
