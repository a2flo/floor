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

#include <floor/compute/cuda/cuda_compute.hpp>

#if !defined(FLOOR_NO_CUDA)

#include <floor/compute/llvm_toolchain.hpp>
#include <floor/floor/floor.hpp>
#include <floor/compute/universal_binary.hpp>

#if !defined(FLOOR_NO_VULKAN)
#include <floor/compute/vulkan/vulkan_buffer.hpp>
#include <floor/compute/vulkan/vulkan_image.hpp>
#endif

cuda_compute::cuda_compute(const vector<string> whitelist) : compute_context() {
	platform_vendor = COMPUTE_VENDOR::NVIDIA;
	
	// init cuda api functions
	if(!cuda_api_init(floor::get_cuda_use_internal_api())) {
		log_error("failed to initialize CUDA API functions");
		return;
	}
	
	// init cuda itself
	CU_CALL_RET(cu_init(0), "failed to initialize CUDA")
	
	// need at least 7.5 right now
	const auto to_driver_major = [](const uint32_t& version) { return version / 1000; };
	const auto to_driver_minor = [](const uint32_t& version) { return (version % 100) / 10; };
	cu_driver_get_version((int*)&driver_version);
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
	
	has_external_memory_support = cuda_can_use_external_memory();
	log_msg("CUDA external memory support: %v", has_external_memory_support ? "yes" : "no");
	
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
		memset(dev_name, 0, size(dev_name));
		CU_CALL_IGNORE(cu_device_get_name(dev_name, size(dev_name) - 1, cuda_dev))
		
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
		CU_CALL_IGNORE(cu_device_compute_capability(&cc.x, &cc.y, cuda_dev))
		if(cc.x < 2) {
			log_error("unsupported cuda device \"%s\": at least compute capability 2.0 is required (has %u.%u)!",
					  dev_name, cc.x, cc.y);
			continue;
		}
		
		// create the context for this device
		cu_context ctx;
		CU_CALL_CONT(cu_ctx_create(&ctx, CU_CONTEXT_FLAGS::SCHEDULE_AUTO, cuda_dev),
					 "failed to create context for device")
		CU_CALL_IGNORE(cu_ctx_set_current(ctx))
		
		//
		devices.emplace_back(make_unique<cuda_device>());
		auto& device = (cuda_device&)*devices.back();
		
		// set initial/fixed attributes
		device.ctx = ctx;
		device.context = this;
		device.device_id = cuda_dev;
		device.sm = uint2(cc);
		device.type = (compute_device::TYPE)gpu_counter++;
		device.name = dev_name;
		device.version_str = to_string(cc.x) + "." + to_string(cc.y);
		device.driver_version_str = to_string(to_driver_major(driver_version)) + "." + to_string(to_driver_minor(driver_version));

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
		
		device.sub_group_shuffle_support = (device.sm.x >= 3); // supported with sm_30+
		device.extended_64_bit_atomics_support = (device.sm.x > 3 || (device.sm.x == 3 && device.sm.y >= 2)); // supported since sm_32
		
		// get UUID if CUDA 9.2+
		if (driver_version >= 9020 && cu_device_get_uuid != nullptr) {
			do {
				cu_uuid uuid;
				CU_CALL_CONT(cu_device_get_uuid(&uuid, cuda_dev), "failed to retrieve device UUID")
				copy_n(begin(uuid.bytes), device.uuid.size(), begin(device.uuid));
				device.has_uuid = true;
			} while (false);
		}
		
		// enable h/w depth compare when using the internal api and everything is alright
		if(cuda_can_use_internal_api()) {
			log_msg("using internal api");
			device.image_depth_compare_support = true;
			
			// exchange the device sampler init function with our own + store the driver function in the device for later use
			auto device_ptr = *(void**)(uintptr_t(device.ctx) + cuda_device_in_ctx_offset);
			auto sampler_func_ptr = (void**)(uintptr_t(device_ptr) + cuda_device_sampler_func_offset);
			(void*&)device.sampler_init_func_ptr = *sampler_func_ptr;
			*sampler_func_ptr = (void*)&cuda_image::internal_device_sampler_init;
		}
		
		// set max supported PTX version and min required PTX version
		if (driver_version >= 7050 && driver_version < 8000) {
			device.ptx = { 4, 3 };
		} else if (driver_version < 9000) {
			device.ptx = { 5, 0 };
		} else if (driver_version < 9010) {
			device.ptx = { 6, 0 };
		} else if (driver_version < 9020) {
			device.ptx = { 6, 1 };
		} else if (driver_version < 10000) {
			device.ptx = { 6, 2 };
		} else if (driver_version < 10010) {
			device.ptx = { 6, 3 };
		} else {
			device.ptx = { 6, 4 };
		}
		
		device.min_req_ptx = { 4, 3 };
		if (device.sm.x == 6) {
			device.min_req_ptx = { 5, 0 };
		} else if (device.sm.x == 7) {
			if (device.sm.y < 5) {
				device.min_req_ptx = { 6, 0 };
			} else {
				device.min_req_ptx = { 6, 3 };
			}
		} else if (device.sm.x == 8) {
			device.min_req_ptx = { 6, 4 };
		} else {
			device.min_req_ptx = { 6, 4 };
		}
		
		// additional info
		log_msg("mem size: %u MB (global), %u KB (local), %u KB (constant)",
				device.global_mem_size / 1024ULL / 1024ULL,
				device.local_mem_size / 1024ULL,
				device.constant_mem_size / 1024ULL);
		log_msg("host unified memory: %u", device.unified_memory);
		log_msg("coop kernels: %u", device.cooperative_kernel_support);
		log_msg("max total local size: %u", device.max_total_local_size);
		log_msg("max local size: %v", device.max_local_size);
		log_msg("max global size: %v", device.max_global_size);
		log_msg("max cuda grid-dim: %u", max_grid_dim);
		log_msg("max mip-levels: %u", device.max_mip_levels);
		
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
	
	// figure out the fastest device
	for (const auto& device : devices) {
		// compute score and try to figure out which device is the fastest
		const auto compute_gpu_score = [](const cuda_device& dev) -> uint32_t {
			uint32_t multiplier = 1;
			switch(dev.sm.x) {
				case 2:
					// sm_20: 32 cores/sm, sm_21: 48 cores/sm
					multiplier = (dev.sm.y == 0 ? 32 : 48);
					break;
				case 3:
					// sm_3x: 192 cores/sm
					multiplier = 192;
					break;
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
				default:
					// sm_82/sm_8x: 64 cores/sm (TODO)?
					multiplier = 64;
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
		log_debug("fastest GPU device: %s %s (score: %u)",
				  fastest_gpu_device->vendor_name, fastest_gpu_device->name, fastest_gpu_score);
		
		fastest_device = fastest_gpu_device;
		
		// make context of fastest device current
		CU_CALL_IGNORE(cu_ctx_set_current(((const cuda_device*)fastest_gpu_device)->ctx))
	}
	
	// create a default queue for each device
	for(const auto& dev : devices) {
		default_queues.insert(*dev, create_queue(*dev));
	}
	
	// init shaders in cuda_image
	cuda_image::init_internal(this);
}

shared_ptr<compute_queue> cuda_compute::create_queue(const compute_device& dev) const {
	cu_stream stream;
	CU_CALL_RET(cu_stream_create(&stream, CU_STREAM_FLAGS::NON_BLOCKING),
				"failed to create cuda stream", {})
	
	auto ret = make_shared<cuda_queue>(dev, stream);
	queues.push_back(ret);
	return ret;
}

const compute_queue* cuda_compute::get_device_default_queue(const compute_device& dev) const {
	const auto def_queue = default_queues.get(dev);
	if(def_queue.first) {
		return def_queue.second->second.get();
	}
	// only happens if the context is invalid (the default queues haven't been created)
	log_error("no default queue for this device exists yet!");
	return nullptr;
}

shared_ptr<compute_buffer> cuda_compute::create_buffer(const compute_queue& cqueue,
													   const size_t& size, const COMPUTE_MEMORY_FLAG flags,
													   const uint32_t opengl_type) const {
	return make_shared<cuda_buffer>(cqueue, size, flags, opengl_type);
}

shared_ptr<compute_buffer> cuda_compute::create_buffer(const compute_queue& cqueue,
													   const size_t& size, void* data,
													   const COMPUTE_MEMORY_FLAG flags,
													   const uint32_t opengl_type) const {
	return make_shared<cuda_buffer>(cqueue, size, data, flags, opengl_type);
}

shared_ptr<compute_buffer> cuda_compute::wrap_buffer(const compute_queue& cqueue,
													 const uint32_t opengl_buffer,
													 const uint32_t opengl_type,
													 const COMPUTE_MEMORY_FLAG flags) const {
	const auto info = compute_buffer::get_opengl_buffer_info(opengl_buffer, opengl_type, flags);
	if(!info.valid) return {};
	return make_shared<cuda_buffer>(cqueue, info.size, nullptr,
									flags | COMPUTE_MEMORY_FLAG::OPENGL_SHARING,
									opengl_type, opengl_buffer);
}

shared_ptr<compute_buffer> cuda_compute::wrap_buffer(const compute_queue& cqueue,
													 const uint32_t opengl_buffer,
													 const uint32_t opengl_type,
													 void* data,
													 const COMPUTE_MEMORY_FLAG flags) const {
	const auto info = compute_buffer::get_opengl_buffer_info(opengl_buffer, opengl_type, flags);
	if(!info.valid) return {};
	return make_shared<cuda_buffer>(cqueue, info.size, data,
									flags | COMPUTE_MEMORY_FLAG::OPENGL_SHARING,
									opengl_type, opengl_buffer);
}

shared_ptr<compute_buffer> cuda_compute::wrap_buffer(const compute_queue& cqueue,
													 const compute_buffer& vk_buffer,
													 const COMPUTE_MEMORY_FLAG flags) const {
#if !defined(FLOOR_NO_VULKAN)
	const auto vk_buffer_obj = dynamic_cast<const vulkan_buffer*>(&vk_buffer);
	if (vk_buffer_obj == nullptr) {
		log_error("specified buffer is not a Vulkan buffer");
		return {};
	}
	
	return make_shared<cuda_buffer>(cqueue, vk_buffer.get_size(), nullptr, flags | COMPUTE_MEMORY_FLAG::VULKAN_SHARING, 0, 0, vk_buffer_obj);
#else
	return compute_context::wrap_buffer(cqueue, vk_buffer, flags);
#endif
}

shared_ptr<compute_image> cuda_compute::create_image(const compute_queue& cqueue,
													 const uint4 image_dim,
													 const COMPUTE_IMAGE_TYPE image_type,
													 const COMPUTE_MEMORY_FLAG flags,
													 const uint32_t opengl_type) const {
	return make_shared<cuda_image>(cqueue, image_dim, image_type, nullptr, flags, opengl_type);
}

shared_ptr<compute_image> cuda_compute::create_image(const compute_queue& cqueue,
													 const uint4 image_dim,
													 const COMPUTE_IMAGE_TYPE image_type,
													 void* data,
													 const COMPUTE_MEMORY_FLAG flags,
													 const uint32_t opengl_type) const {
	return make_shared<cuda_image>(cqueue, image_dim, image_type, data, flags, opengl_type);
}

shared_ptr<compute_image> cuda_compute::wrap_image(const compute_queue& cqueue,
												   const uint32_t opengl_image,
												   const uint32_t opengl_target,
												   const COMPUTE_MEMORY_FLAG flags) const {
	const auto info = compute_image::get_opengl_image_info(opengl_image, opengl_target, flags);
	if(!info.valid) return {};
	return make_shared<cuda_image>(cqueue, info.image_dim, info.image_type, nullptr,
								   flags | COMPUTE_MEMORY_FLAG::OPENGL_SHARING,
								   opengl_target, opengl_image, &info);
}

shared_ptr<compute_image> cuda_compute::wrap_image(const compute_queue& cqueue,
												   const uint32_t opengl_image,
												   const uint32_t opengl_target,
												   void* data,
												   const COMPUTE_MEMORY_FLAG flags) const {
	const auto info = compute_image::get_opengl_image_info(opengl_image, opengl_target, flags);
	if(!info.valid) return {};
	return make_shared<cuda_image>(cqueue, info.image_dim, info.image_type, data,
								   flags | COMPUTE_MEMORY_FLAG::OPENGL_SHARING,
								   opengl_target, opengl_image, &info);
}

shared_ptr<compute_image> cuda_compute::wrap_image(const compute_queue& cqueue,
												   const compute_image& vk_image,
												   const COMPUTE_MEMORY_FLAG flags) const {
#if !defined(FLOOR_NO_VULKAN)
	const auto vk_image_obj = dynamic_cast<const vulkan_image*>(&vk_image);
	if (vk_image_obj == nullptr) {
		log_error("specified buffer is not a Vulkan image");
		return {};
	}
	
	return make_shared<cuda_image>(cqueue, vk_image.get_image_dim(), vk_image.get_image_type(), nullptr,
								   flags | COMPUTE_MEMORY_FLAG::VULKAN_SHARING, 0, 0, nullptr, vk_image_obj);
#else
	return compute_context::wrap_image(cqueue, vk_image, flags);
#endif
}

shared_ptr<compute_program> cuda_compute::add_universal_binary(const string& file_name) {
	auto bins = universal_binary::load_dev_binaries_from_archive(file_name, *this);
	if (bins.ar == nullptr || bins.dev_binaries.empty()) {
		log_error("failed to load universal binary: %s", file_name);
		return {};
	}
	
	// create the program
	cuda_program::program_map_type prog_map;
	prog_map.reserve(devices.size());
	for (size_t i = 0, dev_count = devices.size(); i < dev_count; ++i) {
		const auto& cuda_dev = (const cuda_device&)*devices[i];
		const auto& dev_best_bin = bins.dev_binaries[i];
		const auto func_info = universal_binary::translate_function_info(dev_best_bin.first->functions);
		// TODO: handle CUBIN
		prog_map.insert_or_assign(cuda_dev,
								  create_cuda_program_internal(cuda_dev,
															   dev_best_bin.first->data.data(),
															   dev_best_bin.first->data.size(),
															   func_info,
															   dev_best_bin.second.cuda.max_registers,
															   false /* TODO: true? */));
	}
	
	return add_program(move(prog_map));
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
	compile_options options { .cli = additional_options };
	return add_program_file(file_name, options);
}

shared_ptr<compute_program> cuda_compute::add_program_file(const string& file_name,
														   compile_options options) {
	// compile the source file for all devices in the context
	cuda_program::program_map_type prog_map;
	prog_map.reserve(devices.size());
	options.target = llvm_toolchain::TARGET::PTX;
	for(const auto& dev : devices) {
		const auto& cuda_dev = (const cuda_device&)*dev;
		prog_map.insert_or_assign(cuda_dev,
								  create_cuda_program(cuda_dev, llvm_toolchain::compile_program_file(*dev, file_name, options)));
	}
	return add_program(move(prog_map));
}

shared_ptr<compute_program> cuda_compute::add_program_source(const string& source_code,
															 const string additional_options) {
	compile_options options { .cli = additional_options };
	return add_program_source(source_code, options);
}

shared_ptr<compute_program> cuda_compute::add_program_source(const string& source_code,
															 compile_options options) {
	// compile the source code for all devices in the context
	cuda_program::program_map_type prog_map;
	prog_map.reserve(devices.size());
	options.target = llvm_toolchain::TARGET::PTX;
	for(const auto& dev : devices) {
		const auto& cuda_dev = (const cuda_device&)*dev;
		prog_map.insert_or_assign(cuda_dev,
								  create_cuda_program(cuda_dev, llvm_toolchain::compile_program(*dev, source_code, options)));
	}
	return add_program(move(prog_map));
}

cuda_program::cuda_program_entry cuda_compute::create_cuda_program(const cuda_device& device,
																   llvm_toolchain::program_data program) {
	if(!program.valid) {
		return {};
	}
	return create_cuda_program_internal(device,
										program.data_or_filename.data(), program.data_or_filename.size(),
										program.functions, program.options.cuda.max_registers,
										program.options.silence_debug_output);
}

cuda_program::cuda_program_entry cuda_compute::create_cuda_program_internal(const cuda_device& device,
																			const void* program_data,
																			const size_t& program_size,
																			const vector<llvm_toolchain::function_info>& functions,
																			const uint32_t& max_registers,
																			const bool& silence_debug_output) {
	const auto& force_sm = floor::get_cuda_force_driver_sm();
	const auto& sm = device.sm;
	const uint32_t sm_version = (force_sm.empty() ? sm.x * 10 + sm.y : stou(force_sm));
	cuda_program::cuda_program_entry ret;
	ret.functions = functions;
	
	// must make the device ctx current for this thread (if it isn't already)
	CU_CALL_RET(cu_ctx_set_current(device.ctx),
				"failed to make cuda context current", {})
	
	if(!floor::get_cuda_jit_verbose() && !floor::get_toolchain_debug()) {
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
			{ .ui = floor::get_toolchain_profiling() ? 1u : 0u },
			{ .ui = 0u },
			{ .ui = max_registers != 0 ? max_registers : floor::get_cuda_max_registers() },
			{ .ui = floor::get_cuda_jit_opt_level() },
		};
		static_assert(option_count == size(jit_option_values), "mismatching option count");
		
		CU_CALL_RET(cu_module_load_data_ex(&ret.program,
										   program_data,
										   option_count,
										   (const CU_JIT_OPTION*)&jit_options[0],
										   (const void* const*)&jit_option_values[0]),
					"failed to load/jit cuda module", {})
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
			{ .ui = (floor::get_toolchain_profiling() || floor::get_toolchain_debug()) ? 1u : 0u },
			{ .ui = floor::get_toolchain_debug() ? 1u : 0u },
			{ .ui = max_registers != 0 ? max_registers : floor::get_cuda_max_registers() },
			// opt level must be 0 when debug info is generated
			{ .ui = (floor::get_toolchain_debug() ? 0u : floor::get_cuda_jit_opt_level()) },
			{ .ui = 1u },
			{ .ptr = error_log },
			{ .ptr = info_log },
			{ .ui = log_size - 1u },
			{ .ui = log_size - 1u },
		};
		static_assert(option_count == size(jit_option_values), "mismatching option count");
		
		// TODO: print out llvm_toolchain log
		cu_link_state link_state;
		void* cubin_ptr = nullptr;
		size_t cubin_size = 0;
		CU_CALL_RET(cu_link_create(option_count,
								   (const CU_JIT_OPTION*)&jit_options[0],
								   (const void* const*)&jit_option_values[0],
								   &link_state),
					"failed to create link state", {})
		CU_CALL_ERROR_EXEC(cu_link_add_data(link_state, CU_JIT_INPUT_TYPE::PTX,
											program_data, program_size,
											nullptr, 0, nullptr, nullptr),
						   "failed to add ptx data to link state", {
							   print_error_log();
							   cu_link_destroy(link_state);
							   return ret;
						   })
		CU_CALL_ERROR_EXEC(cu_link_complete(link_state, &cubin_ptr, &cubin_size),
						   "failed to link ptx data", {
							   print_error_log();
							   cu_link_destroy(link_state);
							   return ret;
						   })
		CU_CALL_ERROR_EXEC(cu_module_load_data(&ret.program, cubin_ptr),
						   "failed to load cuda module", {
							   print_error_log();
							   if(info_log[0] != 0) {
								   info_log[log_size - 1] = 0;
								   log_debug("ptx build info: %s", info_log);
							   }
							   cu_link_destroy(link_state);
							   return ret;
						   })
		CU_CALL_NO_ACTION(cu_link_destroy(link_state),
						  "failed to destroy link state")
		
		if(info_log[0] != 0 && !silence_debug_output) {
			info_log[log_size - 1] = 0;
			log_debug("ptx build info: %s", info_log);
		}
	
		if(floor::get_toolchain_log_binaries()) {
			// for testing purposes: dump the compiled binaries again
			file_io::buffer_to_file("binary_" + to_string(sm_version) + ".cubin", (const char*)cubin_ptr, cubin_size);
		}
	}
	
	if(!silence_debug_output) {
		log_debug("successfully created cuda program!");
	}
	
	ret.valid = true;
	return ret;
}

shared_ptr<compute_program> cuda_compute::add_precompiled_program_file(const string& file_name floor_unused,
																	   const vector<llvm_toolchain::function_info>& functions floor_unused) {
	// TODO: !
	log_error("not yet supported by cuda_compute!");
	return {};
}

shared_ptr<compute_program::program_entry> cuda_compute::create_program_entry(const compute_device& device,
																			  llvm_toolchain::program_data program,
																			  const llvm_toolchain::TARGET) {
	return make_shared<cuda_program::cuda_program_entry>(create_cuda_program((const cuda_device&)device, program));
}

#endif
