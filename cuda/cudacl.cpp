/*
 *  Flexible OpenCL Rasterizer (oclraster)
 *  Copyright (C) 2012 - 2013 Florian Ziesche
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

#if defined(OCLRASTER_CUDA_CL)

#include "opencl.h"
#include "cudacl_translator.h"
#include "cudacl_compiler.h"
#include "pipeline/image.h"
#include "oclraster.h"

#if defined(__APPLE__)
#if !defined(OCLRASTER_IOS)
#include "osx/osx_helper.h"
#endif
#endif

#if (CUDA_VERSION < 5050)
#error "oclraster must be compiled with at least CUDA 5.5!"
#endif

//
struct cuda_kernel_object {
	CUmodule* module = nullptr;
	CUfunction* function = nullptr;
	const cudacl_kernel_info info;
	
	// <arg#, <arg size, arg ptr>>
	struct kernel_arg {
		size_t size = 0;
		void* ptr = nullptr;
		bool free_ptr = true;
	};
	unordered_map<cl_uint, kernel_arg> arguments;
	
	cuda_kernel_object(const cudacl_kernel_info& info_) : info(info_) {}
	~cuda_kernel_object() {
		for(const auto& arg : arguments) {
			if(arg.second.free_ptr &&
			   arg.second.ptr != nullptr) {
				free(arg.second.ptr);
			}
		}
		arguments.clear();
		
		if(function != nullptr) {
			delete function;
		}
		if(module != nullptr) {
			// no CU, since we shouldn't throw here and it doesn't really matter if the unload fails
			cuModuleUnload(*module);
			delete module;
		}
	}
};

//
#define __ERROR_CODE_INFO(F) \
F(CUDA_SUCCESS) \
F(CUDA_ERROR_INVALID_VALUE) \
F(CUDA_ERROR_NOT_INITIALIZED) \
F(CUDA_ERROR_DEINITIALIZED) \
F(CUDA_ERROR_PROFILER_DISABLED) \
F(CUDA_ERROR_PROFILER_NOT_INITIALIZED) \
F(CUDA_ERROR_PROFILER_ALREADY_STARTED) \
F(CUDA_ERROR_PROFILER_ALREADY_STOPPED) \
F(CUDA_ERROR_NO_DEVICE) \
F(CUDA_ERROR_INVALID_DEVICE) \
F(CUDA_ERROR_INVALID_IMAGE) \
F(CUDA_ERROR_INVALID_CONTEXT) \
F(CUDA_ERROR_CONTEXT_ALREADY_CURRENT) \
F(CUDA_ERROR_MAP_FAILED) \
F(CUDA_ERROR_UNMAP_FAILED) \
F(CUDA_ERROR_ARRAY_IS_MAPPED) \
F(CUDA_ERROR_ALREADY_MAPPED) \
F(CUDA_ERROR_NO_BINARY_FOR_GPU) \
F(CUDA_ERROR_ALREADY_ACQUIRED) \
F(CUDA_ERROR_NOT_MAPPED) \
F(CUDA_ERROR_NOT_MAPPED_AS_ARRAY) \
F(CUDA_ERROR_NOT_MAPPED_AS_POINTER) \
F(CUDA_ERROR_ECC_UNCORRECTABLE) \
F(CUDA_ERROR_UNSUPPORTED_LIMIT) \
F(CUDA_ERROR_CONTEXT_ALREADY_IN_USE) \
F(CUDA_ERROR_PEER_ACCESS_UNSUPPORTED) \
F(CUDA_ERROR_INVALID_SOURCE) \
F(CUDA_ERROR_FILE_NOT_FOUND) \
F(CUDA_ERROR_SHARED_OBJECT_SYMBOL_NOT_FOUND) \
F(CUDA_ERROR_SHARED_OBJECT_INIT_FAILED) \
F(CUDA_ERROR_OPERATING_SYSTEM) \
F(CUDA_ERROR_INVALID_HANDLE) \
F(CUDA_ERROR_NOT_FOUND) \
F(CUDA_ERROR_NOT_READY) \
F(CUDA_ERROR_LAUNCH_FAILED) \
F(CUDA_ERROR_LAUNCH_OUT_OF_RESOURCES) \
F(CUDA_ERROR_LAUNCH_TIMEOUT) \
F(CUDA_ERROR_LAUNCH_INCOMPATIBLE_TEXTURING) \
F(CUDA_ERROR_PEER_ACCESS_ALREADY_ENABLED) \
F(CUDA_ERROR_PEER_ACCESS_NOT_ENABLED) \
F(CUDA_ERROR_PRIMARY_CONTEXT_ACTIVE) \
F(CUDA_ERROR_CONTEXT_IS_DESTROYED) \
F(CUDA_ERROR_ASSERT) \
F(CUDA_ERROR_TOO_MANY_PEERS) \
F(CUDA_ERROR_HOST_MEMORY_ALREADY_REGISTERED) \
F(CUDA_ERROR_HOST_MEMORY_NOT_REGISTERED) \
F(CUDA_ERROR_UNKNOWN)

#define __DECLARE_ERROR_CODE_TO_STRING(code) case code: return #code;

string cudacl::error_code_to_string(cl_int error_code) const {
	switch(error_code) {
		__ERROR_CODE_INFO(__DECLARE_ERROR_CODE_TO_STRING);
		case CUDA_ERROR_OUT_OF_MEMORY: {
			size_t free_mem = 0, total_mem = 0;
			cuMemGetInfo(&free_mem, &total_mem);
			return "CUDA_ERROR_OUT_OF_MEMORY (" + size_t2string(free_mem) + "/" + size_t2string(total_mem) + ")";
		}
		default:
			return "UNKNOWN CUDA ERROR";
	}
}

class cudacl_exception : public exception {
protected:
	const int error_code;
	const string error_str;
public:
	cudacl_exception(const int& err_code) : error_code(err_code), error_str("") {}
	cudacl_exception(const string& err_str) : error_code(~0), error_str(err_str) {}
	cudacl_exception(const int& err_code, const string& err_str) : error_code(err_code), error_str(err_str) {}
	~cudacl_exception() throw() {}
	virtual const char* what() const throw ();
	virtual const int& code() const throw ();
};
const char* cudacl_exception::what() const throw () {
	return error_str.c_str();
}
const int& cudacl_exception::code() const throw () {
	return error_code;
}

#define __HANDLE_CL_EXCEPTION_START(func_str) __HANDLE_CL_EXCEPTION_START_EXT(func_str, "")
#define __HANDLE_CL_EXCEPTION_START_EXT(func_str, additional_info)				\
catch(cudacl_exception err) {													\
	oclr_error("line #%s, " func_str "(): %s (%d)%s!",							\
			  __LINE__, err.what(), err.code(), additional_info);
#define __HANDLE_CL_EXCEPTION_END }
#define __HANDLE_CL_EXCEPTION(func_str) __HANDLE_CL_EXCEPTION_START(func_str) __HANDLE_CL_EXCEPTION_END
#define __HANDLE_CL_EXCEPTION_EXT(func_str, additional_info) __HANDLE_CL_EXCEPTION_START_EXT(func_str, additional_info) __HANDLE_CL_EXCEPTION_END

//
#define CU(_CUDA_CALL) {														\
	CUresult _cu_err = _CUDA_CALL;												\
	/* check if call was successful, or if cuda is already shutting down, */	\
	/* in which case we just pretend nothing happened and continue ...    */	\
	if(_cu_err != CUDA_SUCCESS && _cu_err != CUDA_ERROR_DEINITIALIZED) {		\
		oclr_error("cuda driver error #%i: %s (%s)",							\
				  _cu_err, error_code_to_string(_cu_err), #_CUDA_CALL);			\
		throw cudacl_exception(CL_OUT_OF_RESOURCES, "cuda driver error");		\
	}																			\
}

cudacl::cudacl(const char* kernel_path_, SDL_Window* wnd_, const bool clear_cache_) :
opencl_base(), cc_target(CU_TARGET_COMPUTE_10) {
	opencl_base::sdl_wnd = wnd_;
	opencl_base::kernel_path_str = kernel_path_;

	context = nullptr;
	cur_kernel = nullptr;
	active_device = nullptr;
	
	fastest_cpu = nullptr;
	fastest_gpu = nullptr;
	
	build_options = "-I" + kernel_path_str;
	build_options += " -I" + kernel_path_str + "cuda";
	build_options += " -DOCLRASTER_CUDA_CL";
	
#if defined(__APPLE__)
	// add defines for the compile-time and run-time os x versions
	build_options += " -DOS_X_VERSION_COMPILED=" + size_t2string(osx_helper::get_compiled_system_version());
	build_options += " -DOS_X_VERSION=" + size_t2string(osx_helper::get_system_version());
#endif
	
	// clear cuda cache
	if(clear_cache_) {
		// TODO (ignore this for now)
	}
	
	// init cuda
	CUresult cu_err = CUDA_SUCCESS;
	cu_err = cuInit(0);
	if(cu_err != CUDA_SUCCESS) {
		oclr_error("failed to initialize CUDA: %i", cu_err);
		valid = false;
	}
	
	//
	int driver_version = 0;
	cuDriverGetVersion(&driver_version);
	if(driver_version < 5050) {
		oclr_error("oclraster requires at least CUDA 5.5!");
		valid = false;
	}
	
	//
	int device_count = 0;
	if(cuDeviceGetCount(&device_count) != CUDA_SUCCESS) {
		oclr_error("cuDeviceGetCount failed!");
		valid = false;
	}
	if(device_count == 0) {
		oclr_error("there is no device that supports CUDA!");
		valid = false;
	}
	if(!valid) {
		supported = false;
	}
	
	// get the cache file list (if this is actually being used is decided when compiling/adding a kernel)
	cache_path = kernel_path_str.substr(0, kernel_path_str.rfind('/', kernel_path_str.length()-2)) + "/cache/";
	const auto cache_list = core::get_file_list(cache_path);
	for(const auto& cache_file : cache_list) {
		// ignore directories
		if(cache_file.second == file_io::FILE_TYPE::DIR) continue;
		// ignore all files containing a '.'
		if(cache_file.first.find(".") != string::npos) continue;
		// all remaining files don't have a file extension -> must be cache files
		if(cache_file.first.size() != 32) {
			oclr_error("invalid cache filename: %s", cache_file.first);
			continue;
		}
		
		// note: kernel identifier is unknown at this point (-> empty string)
		cuda_cache_hashes.emplace(uint128 {
			strtoull(cache_file.first.substr(0, 16).c_str(), nullptr, 16),
			strtoull(cache_file.first.substr(16, 16).c_str(), nullptr, 16)
		}, "");
	}
}

cudacl::~cudacl() {
	oclr_debug("deleting cudacl object");
	
	// delete cl/cuda buffers
	while(!buffers.empty()) {
		delete_buffer(buffers.back());
	}
	
	// delete kernels
	while(!cuda_kernels.empty()) {
		const auto iter = cuda_kernels.begin();
		delete iter->second;
		cuda_kernels.erase(iter);
	}
	destroy_kernels();
	
	// delete the rest (devices, contexts, streams)
	for(const auto& device : cuda_devices) {
		CUcontext* ctx = cuda_contexts[device];
		CUstream* stream = cuda_queues[device];
		CU(cuCtxSetCurrent(*ctx));
		CU(cuStreamDestroy(*stream));
		CU(cuCtxDestroy(*ctx));
		delete stream;
		delete device;
		delete ctx;
	}
	cuda_contexts.clear();
	cuda_queues.clear();
	cuda_devices.clear();
	
	for(const auto& cl_device : devices) {
		delete cl_device;
	}
	devices.clear();
	
	oclr_debug("cudacl object deleted");
}

void cudacl::init(bool use_platform_devices oclr_unused, const size_t platform_index oclr_unused,
				  const set<string> device_restriction oclr_unused, const bool gl_sharing oclr_unused) {
	//
	if(!supported) return;
	
	platform_vendor = PLATFORM_VENDOR::CUDA;
	platform_cl_version = CL_VERSION::CL_1_2;
	
	//
	try {
		int device_count = 0;
		unsigned int fastest_gpu_score = 0;
		unsigned int gpu_score = 0;
		cuDeviceGetCount(&device_count);
		for(int cur_device = 0; cur_device < device_count; cur_device++) {
			// get and create device
			CUdevice* cuda_dev = new CUdevice();
			CUdevice& cuda_device = *cuda_dev;
			CUresult cu_err = cuDeviceGet(cuda_dev, cur_device);
			if(cu_err != CUDA_SUCCESS) {
				oclr_error("failed to get device #%i: %i", cur_device, cu_err);
				delete cuda_dev;
				continue;
			}
			
			char dev_name[256];
			memset(dev_name, 0, 256);
			CU(cuDeviceGetName(dev_name, 255, cuda_device));
			
			pair<int, int> cc;
			CU(cuDeviceComputeCapability(&cc.first, &cc.second, cuda_device));
			if(cc.first < 2) {
				oclr_error("unsupported cuda device \"%s\": at least compute capability 2.0 is required (has %u.%u)!",
						   dev_name, cc.first, cc.second);
				delete cuda_dev;
				continue;
			}
			cuda_devices.emplace_back(cuda_dev);
			
			// get all attributes
			switch(cc.first) {
				case 0:
					oclr_error("invalid compute capability: %u.%u", cc.first, cc.second);
					break;
				case 1:
					switch(cc.second) {
						case 0: // g80
							cc_target_str = "10";
							cc_target = CU_TARGET_COMPUTE_10;
							break;
						case 1: // g8x, g9x
							cc_target_str = "11";
							cc_target = CU_TARGET_COMPUTE_11;
							break;
						case 2: // gt20x (low/mid end, <= gt 240)
							cc_target_str = "12";
							cc_target = CU_TARGET_COMPUTE_12;
							break;
						case 3: // gt20x (high end, >= gtx 260)
						default: // ignore invalid ones ...
							cc_target_str = "13";
							cc_target = CU_TARGET_COMPUTE_13;
							break;
					}
					break;
				case 2:
					switch(cc.second) {
						case 0: // gf100/gf110
							cc_target_str = "20";
							cc_target = CU_TARGET_COMPUTE_20;
							break;
						case 1: // gf10x/gf11x (x != 0)
						default: // ignore invalid ones / default to 2.1
							cc_target_str = "21";
							cc_target = CU_TARGET_COMPUTE_21;
							break;
					}
					break;
				case 3:
					switch(cc.second) {
						case 0: // gk10x
							cc_target_str = "30";
							cc_target = CU_TARGET_COMPUTE_30;
							break;
						case 2: // unknown?
							// this is inofficial, but support it anyways ...
							cc_target_str = "30";
							cc_target = CU_TARGET_COMPUTE_30;
							break;
						case 5: // gk110
						default: // ignore invalid ones / default to 3.5
							cc_target_str = "35";
							cc_target = CU_TARGET_COMPUTE_35;
							break;
					}
					break;
				default: // default higher ones to current highest 3.5 (drivers already mention sm_40 and sm_50)
					cc_target_str = "35";
					cc_target = CU_TARGET_COMPUTE_35;
					break;
			}
			
			bool fp64 = cc.first > 1 || (cc.first == 1 && cc.second >= 3);
			string extensions = "cl_APPLE_gl_sharing cl_khr_byte_addressable_store cl_khr_global_int32_base_atomics cl_khr_global_int32_extended_atomics cl_khr_local_int32_base_atomics cl_khr_local_int32_extended_atomics cl_khr_fp16 cl_nv_device_attribute_query cl_nv_pragma_unroll";
			if(fp64) extensions += " cl_khr_fp64";
			extensions += " "; // add additional space char, since some applications get confused otherwise
			
			size_t global_mem;
			CU(cuDeviceTotalMem(&global_mem, cuda_device));
			
			int vendor_id, proc_count, const_mem, local_mem, priv_mem, cache_size;
			CU(cuDeviceGetAttribute(&vendor_id, CU_DEVICE_ATTRIBUTE_PCI_DEVICE_ID, cuda_device));
			CU(cuDeviceGetAttribute(&proc_count, CU_DEVICE_ATTRIBUTE_MULTIPROCESSOR_COUNT, cuda_device));
			CU(cuDeviceGetAttribute(&const_mem, CU_DEVICE_ATTRIBUTE_TOTAL_CONSTANT_MEMORY, cuda_device));
			CU(cuDeviceGetAttribute(&local_mem, CU_DEVICE_ATTRIBUTE_MAX_SHARED_MEMORY_PER_BLOCK, cuda_device));
			CU(cuDeviceGetAttribute(&priv_mem, CU_DEVICE_ATTRIBUTE_MAX_REGISTERS_PER_BLOCK, cuda_device));
			CU(cuDeviceGetAttribute(&cache_size, CU_DEVICE_ATTRIBUTE_L2_CACHE_SIZE, cuda_device));
			cache_size = (cache_size < 0 ? 0 : cache_size);
			
			int warp_size, max_work_group_size, memory_pitch;
			tuple<int, int, int> max_work_item_size;
			tuple<int, int, int> max_grid_dim;
			CU(cuDeviceGetAttribute(&warp_size, CU_DEVICE_ATTRIBUTE_WARP_SIZE, cuda_device));
			CU(cuDeviceGetAttribute(&max_work_group_size, CU_DEVICE_ATTRIBUTE_MAX_THREADS_PER_BLOCK, cuda_device));
			CU(cuDeviceGetAttribute(&memory_pitch, CU_DEVICE_ATTRIBUTE_MAX_PITCH, cuda_device));
			CU(cuDeviceGetAttribute(&get<0>(max_work_item_size), CU_DEVICE_ATTRIBUTE_MAX_BLOCK_DIM_X, cuda_device));
			CU(cuDeviceGetAttribute(&get<1>(max_work_item_size), CU_DEVICE_ATTRIBUTE_MAX_BLOCK_DIM_Y, cuda_device));
			CU(cuDeviceGetAttribute(&get<2>(max_work_item_size), CU_DEVICE_ATTRIBUTE_MAX_BLOCK_DIM_Z, cuda_device));
			CU(cuDeviceGetAttribute(&get<0>(max_grid_dim), CU_DEVICE_ATTRIBUTE_MAX_GRID_DIM_X, cuda_device));
			CU(cuDeviceGetAttribute(&get<1>(max_grid_dim), CU_DEVICE_ATTRIBUTE_MAX_GRID_DIM_Y, cuda_device));
			CU(cuDeviceGetAttribute(&get<2>(max_grid_dim), CU_DEVICE_ATTRIBUTE_MAX_GRID_DIM_Z, cuda_device));
			
			tuple<int, int> max_image_2d;
			tuple<int, int, int> max_image_3d;
			CU(cuDeviceGetAttribute(&get<0>(max_image_2d), CU_DEVICE_ATTRIBUTE_MAXIMUM_TEXTURE2D_WIDTH, cuda_device));
			CU(cuDeviceGetAttribute(&get<1>(max_image_2d), CU_DEVICE_ATTRIBUTE_MAXIMUM_TEXTURE2D_HEIGHT, cuda_device));
			CU(cuDeviceGetAttribute(&get<0>(max_image_3d), CU_DEVICE_ATTRIBUTE_MAXIMUM_TEXTURE3D_WIDTH, cuda_device));
			CU(cuDeviceGetAttribute(&get<1>(max_image_3d), CU_DEVICE_ATTRIBUTE_MAXIMUM_TEXTURE3D_HEIGHT, cuda_device));
			CU(cuDeviceGetAttribute(&get<2>(max_image_3d), CU_DEVICE_ATTRIBUTE_MAXIMUM_TEXTURE3D_DEPTH, cuda_device));
			
			int clock_rate, mem_clock_rate, mem_bus_width, async_engine_count, tex_align;
			CU(cuDeviceGetAttribute(&clock_rate, CU_DEVICE_ATTRIBUTE_CLOCK_RATE, cuda_device));
			CU(cuDeviceGetAttribute(&mem_clock_rate, CU_DEVICE_ATTRIBUTE_MEMORY_CLOCK_RATE, cuda_device));
			CU(cuDeviceGetAttribute(&mem_bus_width, CU_DEVICE_ATTRIBUTE_GLOBAL_MEMORY_BUS_WIDTH, cuda_device));
			CU(cuDeviceGetAttribute(&async_engine_count, CU_DEVICE_ATTRIBUTE_ASYNC_ENGINE_COUNT, cuda_device));
			CU(cuDeviceGetAttribute(&tex_align, CU_DEVICE_ATTRIBUTE_TEXTURE_ALIGNMENT, cuda_device));
			
			int exec_timeout, overlap, map_host_memory, integrated, concurrent, ecc, tcc, unified_memory;
			CU(cuDeviceGetAttribute(&exec_timeout, CU_DEVICE_ATTRIBUTE_KERNEL_EXEC_TIMEOUT, cuda_device));
			CU(cuDeviceGetAttribute(&overlap, CU_DEVICE_ATTRIBUTE_GPU_OVERLAP, cuda_device));
			CU(cuDeviceGetAttribute(&map_host_memory, CU_DEVICE_ATTRIBUTE_CAN_MAP_HOST_MEMORY, cuda_device));
			CU(cuDeviceGetAttribute(&integrated, CU_DEVICE_ATTRIBUTE_INTEGRATED, cuda_device));
			CU(cuDeviceGetAttribute(&concurrent, CU_DEVICE_ATTRIBUTE_CONCURRENT_KERNELS, cuda_device));
			CU(cuDeviceGetAttribute(&ecc, CU_DEVICE_ATTRIBUTE_ECC_ENABLED, cuda_device));
			CU(cuDeviceGetAttribute(&tcc, CU_DEVICE_ATTRIBUTE_TCC_DRIVER, cuda_device));
			CU(cuDeviceGetAttribute(&unified_memory, CU_DEVICE_ATTRIBUTE_UNIFIED_ADDRESSING, cuda_device));
			
			//
			opencl_base::device_object* device = new opencl_base::device_object();
			device_map[device] = cuda_dev;
			device->device = nullptr;
			device->internal_type = CL_DEVICE_TYPE_GPU;
			device->units = proc_count;
			device->clock = clock_rate / 1000;
			device->mem_size = global_mem;
			device->local_mem_size = local_mem;
			device->constant_mem_size = const_mem;
			device->name = dev_name;
			device->vendor = "NVIDIA";
			device->version = "OpenCL 1.2";
			device->driver_version = "CUDACL 1.2";
			device->cl_c_version = CL_VERSION::CL_1_2;
			device->extensions = extensions;
			device->vendor_type = VENDOR::NVIDIA;
			device->type = (opencl_base::DEVICE_TYPE)cur_device;
			device->max_alloc = global_mem;
			device->max_wi_sizes.set(get<0>(max_work_item_size), get<1>(max_work_item_size), get<2>(max_work_item_size));
			device->max_wg_size = max_work_group_size;
			device->img_support = true;
			device->max_img_2d.set(get<0>(max_image_2d), get<1>(max_image_2d));
			device->max_img_3d.set(get<0>(max_image_3d), get<1>(max_image_3d), get<2>(max_image_3d));
			
			if(fastest_gpu == nullptr) {
				fastest_gpu = device;
				fastest_gpu_score = device->units * device->clock;
			}
			else {
				gpu_score = device->units * device->clock;
				if(gpu_score > fastest_gpu_score) {
					fastest_gpu = device;
				}
			}
			
			devices.push_back(device);
			
			// additional info
			oclr_msg("mem size: %u MB (global), %u KB (local), %u KB (constant)",
					 device->mem_size / 1024ULL / 1024ULL,
					 device->local_mem_size / 1024ULL,
					 device->constant_mem_size / 1024ULL);
			oclr_msg("host unified memory: %u", unified_memory);
			oclr_msg("max_wi_sizes: %v", device->max_wi_sizes);
			oclr_msg("max_wg_size: %u", device->max_wg_size);
			oclr_msg("double support: %b", device->double_support);
			size_t printf_buffer_size = 0;
			cuCtxGetLimit(&printf_buffer_size, CU_LIMIT_PRINTF_FIFO_SIZE);
			oclr_msg("printf buffer size: %u bytes / %u MB",
					 printf_buffer_size,
					 printf_buffer_size / 1024ULL / 1024ULL);
			
			// TYPE (Units: %, Clock: %): Name, Vendor, Version, Driver Version
			const string dev_type_str = "GPU ";
			oclr_debug("%s(Units: %u, Clock: %u MHz, Memory: %u MB): %s %s, %s / %s",
					  dev_type_str,
					  device->units,
					  device->clock,
					  (unsigned int)round(double(device->mem_size) / 1048576.0),
					  device->vendor,
					  device->name,
					  device->version,
					  device->driver_version);
		}
		
		// no supported devices found -> disable opencl/cudacl support
		if(devices.empty()) {
			oclr_error("no supported device found for this platform!");
			supported = false;
			return;
		}
		
		// create a (single) command queue (-> cuda context and stream) for each device
		for(const auto& device : cuda_devices) {
			CUcontext* ctx = new CUcontext();
			cuda_contexts[device] = ctx;
			CU(cuCtxCreate(ctx, CU_CTX_SCHED_AUTO, *device));
			
			CUstream* cuda_stream = new CUstream();
			CU(cuStreamCreate(cuda_stream, 0));
			cuda_queues[device] = cuda_stream;
		}
		CU(cuCtxSetCurrent(*cuda_contexts[cuda_devices[0]]));
		
		if(fastest_gpu != nullptr) oclr_debug("fastest GPU device: %s %s (score: %u)", fastest_gpu->vendor.c_str(), fastest_gpu->name.c_str(), fastest_gpu_score);
		
		// compile internal kernels
		//size_t local_size_limit = std::max((size_t)512, devices[0]->max_wg_size); // default to 512
		//const string lsl_str = " -DLOCAL_SIZE_LIMIT="+size_t2string(local_size_limit);
		
		internal_kernels = { // first time init:
			make_tuple("BIN_RASTERIZE", "bin_rasterize.cl", "oclraster_bin",
					   " -DBIN_SIZE="+uint2string(OCLRASTER_BIN_SIZE)+
					   " -DBATCH_SIZE="+uint2string(OCLRASTER_BATCH_SIZE)),
			
			make_tuple("PROCESSING.PERSPECTIVE", "processing.cl", "oclraster_processing",
					   " -DBIN_SIZE="+uint2string(OCLRASTER_BIN_SIZE)+
					   " -DBATCH_SIZE="+uint2string(OCLRASTER_BATCH_SIZE)+
					   " -DOCLRASTER_PROJECTION_PERSPECTIVE"),
			
			make_tuple("PROCESSING.ORTHOGRAPHIC", "processing.cl", "oclraster_processing",
					   " -DBIN_SIZE="+uint2string(OCLRASTER_BIN_SIZE)+
					   " -DBATCH_SIZE="+uint2string(OCLRASTER_BATCH_SIZE)+
					   " -DOCLRASTER_PROJECTION_ORTHOGRAPHIC"),
			
#if defined(OCLRASTER_FXAA)
			make_tuple("FXAA.LUMA", "luma_pass.cl", "framebuffer_luma", ""),
			make_tuple("FXAA", "fxaa_pass.cl", "framebuffer_fxaa", ""),
#endif
		};
		
		load_internal_kernels();
	}
	catch(cudacl_exception& exc) {
		oclr_error("failed to initialize cuda: %X: %s!", exc.code(), exc.what());
		supported = false;
		valid = false;
	}
}

weak_ptr<opencl_base::kernel_object> cudacl::add_kernel_src(const string& identifier, const string& src, const string& func_name, const string additional_options) {
	oclr_debug("compiling \"%s\" kernel!", identifier);
	
	//
	string options = build_options;
	
	// just define this everywhere to make using image support
	// easier without having to specify this every time
	options += " -DOCLRASTER_IMAGE_HEADER_SIZE="+size_t2string(image::header_size());
	
	// the same goes for the general struct alignment
	options += " -DOCLRASTER_STRUCT_ALIGNMENT="+uint2string(OCLRASTER_STRUCT_ALIGNMENT);
	
	// user options
	options += additional_options;
	
	string error_log = "", build_cmd = "";
	try {
		if(kernels.count(identifier) != 0) {
			oclr_error("kernel \"%s\" already exists!", identifier);
			return kernels[identifier];
		}
		
		// add kernel
		auto kernel = make_shared<opencl::kernel_object>();
		kernels[identifier] = kernel;
		kernel->name = identifier;
		kernel->kernel = nullptr;
		const cudacl_kernel_info* kernel_info = nullptr;
		vector<cudacl_kernel_info> kernels_info;
		
		//
		string ptx_code = "", cuda_source = "";
		
		const bool use_cache = oclraster::get_cuda_use_cache();
		const bool keep_binaries = oclraster::get_cuda_keep_binaries();
		bool found_in_cache = false;
		uint128 kernel_hash { 0, 0 };
		cudacl_translate(src, options, cuda_source, kernels_info, use_cache, found_in_cache, kernel_hash,
						 [this](const uint128& hash) {
			const auto iter = cuda_cache_hashes.find(hash);
			if(iter == cuda_cache_hashes.end()) return false;
			if(iter->first == hash) return true;
			// else: ouch, collision
			oclr_error("cuda cache hash collision");
			cuda_cache_hashes.reserve(cuda_cache_hashes.size() * 4);
			return false;
		});
		
		// uint128 hash -> string conversion (+fill up with 0s if necessary)
		stringstream sstr_upper; sstr_upper << hex << kernel_hash.first;
		stringstream sstr_lower; sstr_lower << hex << kernel_hash.second;
		string upper_hash { sstr_upper.str() };
		string lower_hash { sstr_lower.str() };
		if(upper_hash.size() < 16) upper_hash.insert(0, 16 - upper_hash.size(), '0');
		if(lower_hash.size() < 16) lower_hash.insert(0, 16 - lower_hash.size(), '0');
		const string hash_filename { upper_hash + lower_hash };
		
		//
		if(found_in_cache) {
			// a cache file exists for this hash:
			// check if the cache file has already been read (or generated at runtime)
			const auto cache_iter = cuda_cache_binaries.find(kernel_hash);
			if(cache_iter != cuda_cache_binaries.end()) {
				ptx_code = cache_iter->second;
				oclr_debug("using cached binary for \"%s\"!", identifier);
				
				// add entry for this identifier if there isn't one already
				bool found_entry = false;
				const auto range = cuda_cache_hashes.equal_range(kernel_hash);
				for(auto iter = range.first; iter != range.second; iter++) {
					if(iter->second == identifier) {
						found_entry = true;
						break;
					}
				}
				if(!found_entry) {
					cuda_cache_hashes.emplace(kernel_hash, identifier);
					rev_cuda_cache.emplace(identifier, kernel_hash);
				}
			}
			// if not, read the file
			else {
				if(!file_io::file_to_string(cache_path + hash_filename, ptx_code)) {
					oclr_error("couldn't read cached binary \"%s\" for \"%s\"!", hash_filename, identifier);
					found_in_cache = false; // compile the code
				}
				else {
					oclr_debug("using cached binary for \"%s\"!", identifier);
					cuda_cache_binaries.emplace(kernel_hash, ptx_code);
				}
			}
		}
		
		// not cached, cache read failed or caching is disabled -> compile
		if(!found_in_cache) {
			// use internal compile chain (instead of nvcc)
			string info_log = "";
			build_cmd = {
				options +
				" -DNVIDIA"+
				" -DGPU"+
				" -DPLATFORM_"+platform_vendor_to_str(platform_vendor)+
				" -DLOCAL_MEM_SIZE=49152", // TODO: always set to 48k for now?
			};
			ptx_code = cudacl_compiler::compile(cuda_source,
												identifier,
												cc_target_str,
												build_cmd,
												cache_path + hash_filename,
												error_log, info_log);
			// TODO: cleanup if keep flag is not set!
			
			if(!info_log.empty()) {
				oclr_debug("%s info log:\n%s", identifier, info_log);
			}
			if(!error_log.empty()) {
				throw cudacl_exception("error during kernel compilation!");
			}
			
			// if compiled binaries should be cached
			if(keep_binaries) {
				if(!file_io::string_to_file(cache_path + hash_filename, ptx_code)) {
					oclr_error("couldn't cache binary \"%s\" for \"%s\"!", hash_filename, identifier);
				}
			}
			cuda_cache_hashes.emplace(kernel_hash, identifier);
			rev_cuda_cache.emplace(identifier, kernel_hash);
			cuda_cache_binaries.emplace(kernel_hash, ptx_code);
		}
		
		// get kernel info for function
		bool found = false;
		for(const auto& info : kernels_info) {
			if(info.name == func_name) {
				kernel_info = &info;
				kernel->arg_count = (unsigned int)info.parameters.size();
				found = true;
				break;
			}
		}
		if(!found) throw cudacl_exception("kernel function \""+func_name+"\" does not exist in source file!");
		
		//
		kernel->args_passed.insert(kernel->args_passed.begin(), kernel->arg_count, false);
		kernel->buffer_args.insert(kernel->buffer_args.begin(), kernel->arg_count, nullptr);
		
		// create cuda module (== opencl program)
		const CUjit_option jit_options[] {
			CU_JIT_TARGET,
			CU_JIT_GENERATE_LINE_INFO,
			CU_JIT_GENERATE_DEBUG_INFO,
			CU_JIT_MAX_REGISTERS
		};
		const unsigned int option_count = sizeof(jit_options) / sizeof(CUjit_option);
		const struct alignas(void*) {
			union {
				unsigned int ui;
				float f;
				char* cptr;
			};
		} jit_option_values[] {
			{ .ui = cc_target },
			{ .ui = (oclraster::get_cuda_profiling() || oclraster::get_cuda_debug()) ? 1u : 0u },
			{ .ui = oclraster::get_cuda_debug() ? 1u : 0u },
			{ .ui = 32u }
		};
		static_assert(option_count == (sizeof(jit_option_values) / sizeof(void*)), "mismatching option count");
		
		// use the binary/ptx of the first device for now
		CUmodule* module = new CUmodule();
		CU(cuModuleLoadDataEx(module,
							  ptx_code.c_str(),
							  option_count,
							  (CUjit_option*)&jit_options[0],
							  (void**)&jit_option_values[0]));
		
		// create cuda function (== opencl kernel)
		CUfunction* cuda_func = new CUfunction();
		CU(cuModuleGetFunction(cuda_func, *module, func_name.c_str()));
		
		cuda_kernel_object* cuda_kernel = new cuda_kernel_object(*kernel_info);
		cuda_kernel->module = module;
		cuda_kernel->function = cuda_func;
		cuda_kernels.insert(make_pair(kernel, cuda_kernel));
	}
	__HANDLE_CL_EXCEPTION_START("add_kernel")
		// print out build log and build options
		oclr_error("error log (%s): %s", identifier, error_log);
		oclr_error("build command (%s): %s", identifier, build_cmd);
	__HANDLE_CL_EXCEPTION_END
	
	return kernels[identifier];
}

void cudacl::delete_kernel(weak_ptr<opencl::kernel_object> kernel_obj) {
	auto kernel_ptr = kernel_obj.lock();
	if(kernel_ptr == nullptr) {
		// already deleted
		return;
	}
	
	if(cur_kernel == kernel_ptr) {
		// if the currently active kernel is being deleted, flush+finish the queue
		flush();
		finish();
		cur_kernel = nullptr;
	}
	
	const auto iter = cuda_kernels.find(kernel_ptr);
	if(iter == cuda_kernels.end()) {
		oclr_error("couldn't find cuda kernel object!");
		return;
	}
	delete iter->second;
	cuda_kernels.erase(iter);
	
	for(const auto& kernel : kernels) {
		if(kernel.second == kernel_ptr) {
			kernel_object::unassociate_buffers(kernel_ptr);
			kernels.erase(kernel.first);
			if(kernel_ptr.use_count() > 1) {
				oclr_error("kernel object (%X) use count > 1 (%u) - kernel object is still used somewhere!",
						   kernel_ptr.get(), kernel_ptr.use_count());
			}
			return; // implicit delete of kernel_ptr and the kernel_object
		}
	}
	
	oclr_error("couldn't find kernel object!");
}

void cudacl::log_program_binary(const shared_ptr<opencl::kernel_object> kernel) {
	if(kernel == nullptr) return;
}

opencl_base::buffer_object* cudacl::create_buffer_object(const opencl_base::BUFFER_FLAG type, const void* data) {
	try {
		opencl_base::buffer_object* buffer = new opencl_base::buffer_object();
		buffers.push_back(buffer);
		
		// type/flag validity check
		BUFFER_FLAG vtype = BUFFER_FLAG::NONE;
		if((type & BUFFER_FLAG::USE_HOST_MEMORY) != BUFFER_FLAG::NONE) vtype |= BUFFER_FLAG::USE_HOST_MEMORY;
		if((type & BUFFER_FLAG::DELETE_AFTER_USE) != BUFFER_FLAG::NONE) vtype |= BUFFER_FLAG::DELETE_AFTER_USE;
		if((type & BUFFER_FLAG::BLOCK_ON_READ) != BUFFER_FLAG::NONE) vtype |= BUFFER_FLAG::BLOCK_ON_READ;
		if((type & BUFFER_FLAG::BLOCK_ON_WRITE) != BUFFER_FLAG::NONE) vtype |= BUFFER_FLAG::BLOCK_ON_WRITE;
		if(data != nullptr &&
		   (type & BUFFER_FLAG::INITIAL_COPY) != BUFFER_FLAG::NONE &&
		   (type & BUFFER_FLAG::USE_HOST_MEMORY) == BUFFER_FLAG::NONE) {
			vtype |= BUFFER_FLAG::INITIAL_COPY;
		}
		if(data != nullptr &&
		   (type & BUFFER_FLAG::COPY_ON_USE) != BUFFER_FLAG::NONE) {
			vtype |= BUFFER_FLAG::COPY_ON_USE;
		}
		if(data != nullptr &&
		   (type & BUFFER_FLAG::READ_BACK_RESULT) != BUFFER_FLAG::NONE) {
			vtype |= BUFFER_FLAG::READ_BACK_RESULT;
		}
		
		cl_mem_flags flags = 0;
		switch(type & BUFFER_FLAG::READ_WRITE) {
			case BUFFER_FLAG::READ_WRITE:
				vtype |= BUFFER_FLAG::READ_WRITE;
				flags |= CL_MEM_READ_WRITE;
				break;
			case BUFFER_FLAG::READ:
				vtype |= BUFFER_FLAG::READ;
				flags |= CL_MEM_READ_ONLY;
				break;
			case BUFFER_FLAG::WRITE:
				vtype |= BUFFER_FLAG::WRITE;
				flags |= CL_MEM_WRITE_ONLY;
				break;
			default:
				break;
		}
		if((vtype & BUFFER_FLAG::INITIAL_COPY) != BUFFER_FLAG::NONE &&
		   (vtype & BUFFER_FLAG::USE_HOST_MEMORY) == BUFFER_FLAG::NONE) {
			flags |= CL_MEM_COPY_HOST_PTR;
		}
		if(data != nullptr &&
		   (vtype & BUFFER_FLAG::USE_HOST_MEMORY) != BUFFER_FLAG::NONE) {
			flags |= CL_MEM_USE_HOST_PTR;
		}
		if(data == nullptr &&
		   (vtype & BUFFER_FLAG::USE_HOST_MEMORY) != BUFFER_FLAG::NONE) {
			flags |= CL_MEM_ALLOC_HOST_PTR;
		}
		
		buffer->type = vtype;
		buffer->flags = flags;
		buffer->data = (void*)data;
		return buffer;
	}
	__HANDLE_CL_EXCEPTION("create_buffer_object")
	return nullptr;
}

opencl_base::buffer_object* cudacl::create_buffer(const opencl_base::BUFFER_FLAG type, const size_t size, const void* data) {
	if(size == 0) {
		return nullptr;
	}
	
	try {
		buffer_object* buffer_obj = create_buffer_object(type, data);
		if(buffer_obj == nullptr) return nullptr;
		
		buffer_obj->size = size;
		buffer_obj->buffer = nullptr;
		
		CUdeviceptr* cuda_mem = new CUdeviceptr();
		
		unsigned int cu_flags = 0;
		if(buffer_obj->flags & CL_MEM_USE_HOST_PTR) {
			cu_flags |= CU_MEMHOSTALLOC_DEVICEMAP;
			CU(cuMemHostRegister(buffer_obj->data, size, cu_flags));
			CU(cuMemHostGetDevicePointer(cuda_mem, buffer_obj->data, 0));
		}
		else if(buffer_obj->flags & CL_MEM_ALLOC_HOST_PTR) {
			if(buffer_obj->flags & CL_MEM_READ_ONLY) cu_flags |= CU_MEMHOSTALLOC_WRITECOMBINED;
			cu_flags |= CU_MEMHOSTALLOC_DEVICEMAP;
			CU(cuMemHostAlloc(&buffer_obj->data, size, cu_flags));
			CU(cuMemHostGetDevicePointer(cuda_mem, buffer_obj->data, 0));
		}
		else if(buffer_obj->flags & CL_MEM_COPY_HOST_PTR) {
			CU(cuMemAlloc(cuda_mem, size));
			CU(cuMemcpyHtoD(*cuda_mem, buffer_obj->data, size));
		}
		else {
			CU(cuMemAlloc(cuda_mem, size));
		}
		cuda_buffers[buffer_obj] = cuda_mem;
		return buffer_obj;
	}
	__HANDLE_CL_EXCEPTION("create_buffer")
	return nullptr;
}


opencl_base::buffer_object* cudacl::create_sub_buffer(const buffer_object* parent_buffer,
													  const BUFFER_FLAG type,
													  const size_t offset,
													  const size_t size) {
	if(parent_buffer == nullptr || parent_buffer->image_type != buffer_object::IMAGE_TYPE::IMAGE_NONE) {
		oclr_error("invalid buffer object!");
		return nullptr;
	}
	if(size == 0 || size > parent_buffer->size) {
		oclr_error("invalid size (%u) - must be > 0 and <= buffer size (%u)!", size, parent_buffer->size);
		return nullptr;
	}
	if(offset >= parent_buffer->size || (size+offset) > parent_buffer->size) {
		oclr_error("invalid offset (%u) - offset must be < buffer size (%u) and offset+size (%u) must be <= buffer size (%u)!",
				   size, parent_buffer->size, size+offset, parent_buffer->size);
		return nullptr;
	}
	
	try {
		CUdeviceptr* parent_cuda_mem = cuda_buffers.at((buffer_object*)parent_buffer);
		
		buffer_object* sub_buffer = create_buffer_object(type, nullptr);
		if(sub_buffer == nullptr) return nullptr;
		
		sub_buffer->size = size;
		sub_buffer->buffer = nullptr;
		sub_buffer->parent_buffer = parent_buffer;
		
		CUdeviceptr* cuda_mem = new CUdeviceptr();
		*cuda_mem = *parent_cuda_mem + offset;
		cuda_buffers[sub_buffer] = cuda_mem;
		return sub_buffer;
	}
	__HANDLE_CL_EXCEPTION("create_sub_buffer")
	return nullptr;
}

opencl_base::buffer_object* cudacl::create_image2d_buffer(const opencl_base::BUFFER_FLAG type oclr_unused,
														  const cl_channel_order channel_order oclr_unused, const cl_channel_type channel_type oclr_unused,
														  const size_t width oclr_unused, const size_t height oclr_unused, const void* data oclr_unused) {
	assert(false && "create_image2d_buffer not implemented yet!");
	// TODO
	/*try {
	}
	__HANDLE_CL_EXCEPTION("create_image2d_buffer")*/
	return nullptr;
}

opencl_base::buffer_object* cudacl::create_image3d_buffer(const opencl_base::BUFFER_FLAG type oclr_unused,
														  const cl_channel_order channel_order oclr_unused, const cl_channel_type channel_type oclr_unused,
														  const size_t width oclr_unused, const size_t height oclr_unused, const size_t depth oclr_unused, const void* data oclr_unused) {
	assert(false && "create_image3d_buffer not implemented yet!");
	// TODO
	/*try {
	}
	__HANDLE_CL_EXCEPTION("create_image3d_buffer")*/
	return nullptr;
}

opencl_base::buffer_object* cudacl::create_ogl_buffer(const opencl_base::BUFFER_FLAG type, const GLuint ogl_buffer) {
	try {
		opencl_base::buffer_object* buffer = new opencl_base::buffer_object();
		buffers.push_back(buffer);
		
		// type/flag validity check
		BUFFER_FLAG vtype = BUFFER_FLAG::NONE;
		if((type & BUFFER_FLAG::DELETE_AFTER_USE) != BUFFER_FLAG::NONE) vtype |= BUFFER_FLAG::DELETE_AFTER_USE;
		if((type & BUFFER_FLAG::BLOCK_ON_READ) != BUFFER_FLAG::NONE) vtype |= BUFFER_FLAG::BLOCK_ON_READ;
		if((type & BUFFER_FLAG::BLOCK_ON_WRITE) != BUFFER_FLAG::NONE) vtype |= BUFFER_FLAG::BLOCK_ON_WRITE;
		
		unsigned int cuda_flags = 0;
		switch(type & BUFFER_FLAG::READ_WRITE) {
			case BUFFER_FLAG::READ_WRITE:
				vtype |= BUFFER_FLAG::READ_WRITE;
				cuda_flags = CU_GRAPHICS_REGISTER_FLAGS_NONE;
				break;
			case BUFFER_FLAG::READ:
				vtype |= BUFFER_FLAG::READ;
				cuda_flags = CU_GRAPHICS_REGISTER_FLAGS_READ_ONLY;
				break;
			case BUFFER_FLAG::WRITE:
				vtype |= BUFFER_FLAG::WRITE;
				cuda_flags = CU_GRAPHICS_REGISTER_FLAGS_WRITE_DISCARD;
				break;
			default:
				break;
		}
		
		vtype |= BUFFER_FLAG::OPENGL_BUFFER;
		
		buffer->type = vtype;
		buffer->ogl_buffer = ogl_buffer;
		buffer->data = nullptr;
		buffer->size = 0;
		buffer->buffer = nullptr;
		
		CUgraphicsResource* cuda_gl_buffer = new CUgraphicsResource();
		CU(cuGraphicsGLRegisterBuffer(cuda_gl_buffer, ogl_buffer, cuda_flags));
		cuda_gl_buffers[buffer] = cuda_gl_buffer;
		
		return buffer;
	}
	__HANDLE_CL_EXCEPTION("create_ogl_buffer")
	return nullptr;
}

opencl_base::buffer_object* cudacl::create_ogl_image2d_buffer(BUFFER_FLAG type oclr_unused, GLuint texture oclr_unused, GLenum target oclr_unused) {
	assert(false && "create_ogl_image2d_buffer not implemented yet!");
	// TODO
	/*try {
	}
	__HANDLE_CL_EXCEPTION("create_ogl_image2d_buffer")*/
	return nullptr;
}

opencl_base::buffer_object* cudacl::create_ogl_image2d_renderbuffer(BUFFER_FLAG type oclr_unused, GLuint renderbuffer oclr_unused) {
	assert(false && "create_ogl_image2d_renderbuffer not implemented yet!");
	// TODO
	/*try {
	}
	__HANDLE_CL_EXCEPTION("create_ogl_image2d_renderbuffer")*/
	return nullptr;
}

void cudacl::delete_buffer(opencl_base::buffer_object* buffer_obj) {
	// remove buffer from each associated kernel (and unset the kernel argument)
	for(const auto& associated_kernel : buffer_obj->associated_kernels) {
		for(const auto& arg_num : associated_kernel.second) {
			associated_kernel.first->args_passed[arg_num] = false;
			associated_kernel.first->buffer_args[arg_num] = nullptr;
		}
	}
	buffer_obj->associated_kernels.clear();
	
	// normal buffer
	const auto buffer_iter = cuda_buffers.find(buffer_obj);
	if(buffer_iter != cuda_buffers.end()) {
		CUdeviceptr* cuda_mem = buffer_iter->second;
		if(cuda_mem != nullptr) {
			if(// don't delete memory that points to null and
			   *cuda_mem != 0 &&
			   // only delete actual buffers (sub-buffers have a parent buffer pointer)
			   buffer_iter->first->parent_buffer == nullptr) {
				if(buffer_obj->flags & CL_MEM_USE_HOST_PTR) {
					CU(cuMemHostUnregister(buffer_obj->data));
				}
				else if(buffer_obj->flags & CL_MEM_ALLOC_HOST_PTR) {
					CU(cuMemFreeHost(buffer_obj->data));
				}
				else {
					CU(cuMemFree(*cuda_mem));
				}
			}
			delete cuda_mem;
		}
		cuda_buffers.erase(buffer_iter);
	}
	
	// array/image buffer
	const auto image_iter = cuda_images.find(buffer_obj);
	if(image_iter != cuda_images.end()) {
		CUarray* cuda_img = image_iter->second;
		if(cuda_img != nullptr && *cuda_img != 0) {
			CU(cuArrayDestroy(*cuda_img));
			delete cuda_img;
		}
		cuda_images.erase(image_iter);
	}
	
	// unregister resource (+potential unmap)
	const auto gl_buffer_iter = cuda_gl_buffers.find(buffer_obj);
	if(gl_buffer_iter != cuda_gl_buffers.end()) {
		CUgraphicsResource* res = gl_buffer_iter->second;
		if(buffer_obj->ogl_buffer != 0) {
			if(cuda_mapped_gl_buffers.count(res) > 0) {
				// resource is still mapped -> unmap
				release_gl_object(buffer_obj);
			}
			CU(cuGraphicsUnregisterResource(*res));
			delete res;
		}
		cuda_gl_buffers.erase(gl_buffer_iter);
	}
	
	// remove from cl class
	const auto iter = find(begin(buffers), end(buffers), buffer_obj);
	if(iter != end(buffers)) buffers.erase(iter);
	delete buffer_obj;
}

void cudacl::write_buffer(opencl_base::buffer_object* buffer_obj, const void* src, const size_t offset, const size_t size) {
	size_t write_size = size;
	if(write_size == 0) {
		if(buffer_obj->size == 0) {
			oclr_error("can't write 0 bytes (size of 0)!");
			return;
		}
		else write_size = buffer_obj->size;
	}
	
	size_t write_offset = offset;
	if(write_offset >= buffer_obj->size) {
		oclr_error("write offset (%d) out of bound!", write_offset);
		return;
	}
	
	if(write_offset+write_size > buffer_obj->size) {
		oclr_error("write offset (%d) or write size (%d) is too big - using write size of (%d) instead!",
				  write_offset, write_size, (buffer_obj->size - write_offset));
		write_size = buffer_obj->size - write_offset;
	}
	
	//
	CUdeviceptr* cuda_mem = cuda_buffers[buffer_obj];
	CUstream stream = *cuda_queues[device_map[active_device]];
	if((buffer_obj->type & BUFFER_FLAG::BLOCK_ON_WRITE) != BUFFER_FLAG::NONE) {
		// blocking write: wait until everything has completed in the cmdqueue
		finish();
		
		CU(cuMemcpyHtoD(*cuda_mem + offset, src, write_size));
	}
	else {
		CU(cuMemcpyHtoDAsync(*cuda_mem + offset, src, write_size, stream));
	}
}

void cudacl::write_buffer_rect(buffer_object* buffer_obj oclr_unused, const void* src oclr_unused,
							   const size3 buffer_origin oclr_unused,
							   const size3 host_origin oclr_unused,
							   const size3 region oclr_unused,
							   const size_t buffer_row_pitch oclr_unused, const size_t buffer_slice_pitch oclr_unused,
							   const size_t host_row_pitch oclr_unused, const size_t host_slice_pitch oclr_unused) {
	try {
		// TODO
		assert(false && "write_buffer_rect not implemented yet!");
	}
	__HANDLE_CL_EXCEPTION("write_buffer_rect")
}

void cudacl::write_image(opencl::buffer_object* buffer_obj oclr_unused, const void* src oclr_unused, const size3 origin oclr_unused, const size3 region oclr_unused) {
	try {
		// TODO
		assert(false && "write_image not implemented yet!");
	}
	__HANDLE_CL_EXCEPTION("write_image")
}

void cudacl::copy_buffer(const buffer_object* src_buffer oclr_unused, buffer_object* dst_buffer oclr_unused,
						 const size_t src_offset oclr_unused, const size_t dst_offset oclr_unused, const size_t size oclr_unused) {
	try {
		// TODO
		assert(false && "copy_buffer not implemented yet!");
	}
	__HANDLE_CL_EXCEPTION("copy_buffer")
}

void cudacl::copy_buffer_rect(const buffer_object* src_buffer oclr_unused, buffer_object* dst_buffer oclr_unused,
							  const size3 src_origin oclr_unused, const size3 dst_origin oclr_unused,
							  const size3 region oclr_unused,
							  const size_t src_row_pitch oclr_unused, const size_t src_slice_pitch oclr_unused,
							  const size_t dst_row_pitch oclr_unused, const size_t dst_slice_pitch oclr_unused) {
	try {
		// TODO
		assert(false && "copy_buffer_rect not implemented yet!");
	}
	__HANDLE_CL_EXCEPTION("copy_buffer_rect")
}

void cudacl::copy_image(const buffer_object* src_buffer oclr_unused, buffer_object* dst_buffer oclr_unused,
						const size3 src_origin oclr_unused, const size3 dst_origin oclr_unused, const size3 region oclr_unused) {
	try {
		// TODO
		assert(false && "copy_image not implemented yet!");
	}
	__HANDLE_CL_EXCEPTION("copy_image")
}

void cudacl::copy_buffer_to_image(const buffer_object* src_buffer oclr_unused, buffer_object* dst_buffer oclr_unused,
								  const size_t src_offset oclr_unused, const size3 dst_origin oclr_unused, const size3 dst_region oclr_unused) {
	try {
		// TODO
		assert(false && "copy_buffer_to_image not implemented yet!");
	}
	__HANDLE_CL_EXCEPTION("copy_buffer_to_image")
}

void cudacl::copy_image_to_buffer(const buffer_object* src_buffer oclr_unused, buffer_object* dst_buffer oclr_unused,
								  const size3 src_origin oclr_unused, const size3 src_region oclr_unused, const size_t dst_offset oclr_unused) {
	try {
		// TODO
		assert(false && "copy_image_to_buffer not implemented yet!");
	}
	__HANDLE_CL_EXCEPTION("copy_image_to_buffer")
}

void cudacl::read_buffer(void* dst, const opencl_base::buffer_object* buffer_obj, const size_t offset, const size_t size_) {
	try {
		const size_t size = (size_ == 0 ? buffer_obj->size : size_);
		
		//
		CUdeviceptr* cuda_mem = cuda_buffers[(opencl_base::buffer_object*)buffer_obj];
		CUstream stream = *cuda_queues[device_map[active_device]];
		if((buffer_obj->type & BUFFER_FLAG::BLOCK_ON_READ) != BUFFER_FLAG::NONE) {
			// blocking read: wait until everything has completed in the cmdqueue
			finish();
			
			CU(cuMemcpyDtoH(dst, *cuda_mem + offset, size));
		}
		else {
			CU(cuMemcpyDtoHAsync(dst, *cuda_mem + offset, size, stream));
		}
	}
	__HANDLE_CL_EXCEPTION("read_buffer")
}

void cudacl::read_buffer_rect(void* dst oclr_unused, const buffer_object* buffer_obj oclr_unused,
							  const size3 buffer_origin oclr_unused,
							  const size3 host_origin oclr_unused,
							  const size3 region oclr_unused,
							  const size_t buffer_row_pitch oclr_unused, const size_t buffer_slice_pitch oclr_unused,
							  const size_t host_row_pitch oclr_unused, const size_t host_slice_pitch oclr_unused) {
	try {
		// TODO
		assert(false && "read_buffer_rect not implemented yet!");
	}
	__HANDLE_CL_EXCEPTION("read_buffer_rect")
}

void cudacl::read_image(void* dst oclr_unused, const opencl::buffer_object* buffer_obj oclr_unused, const size3 origin oclr_unused, const size3 region oclr_unused,
						const size_t image_row_pitch oclr_unused, const size_t image_slice_pitch oclr_unused) {
	try {
		// TODO
		assert(false && "read_image not implemented yet!");
	}
	__HANDLE_CL_EXCEPTION("read_image")
}

void* __attribute__((aligned(128))) cudacl::map_buffer(opencl_base::buffer_object* buffer_obj,
													   const MAP_BUFFER_FLAG access_type,
													   const size_t offset,
													   const size_t size) {
	try {
		const bool blocking { (access_type & MAP_BUFFER_FLAG::BLOCK) != MAP_BUFFER_FLAG::NONE };
		
		if((access_type & MAP_BUFFER_FLAG::READ_WRITE) != MAP_BUFFER_FLAG::NONE &&
		   (access_type & MAP_BUFFER_FLAG::WRITE_INVALIDATE) != MAP_BUFFER_FLAG::NONE) {
			oclr_error("READ or WRITE access and WRITE_INVALIDATE are mutually exclusive!");
			return nullptr;
		}
		
		size_t map_size = size;
		if(map_size == 0) {
			if(buffer_obj->size == 0) {
				oclr_error("can't map 0 bytes (size of 0)!");
				return nullptr;
			}
			else map_size = buffer_obj->size;
		}
		
		size_t map_offset = offset;
		if(map_offset >= buffer_obj->size) {
			oclr_error("map offset (%d) out of bound!", map_offset);
			return nullptr;
		}
		if(map_offset+map_size > buffer_obj->size) {
			oclr_error("map offset (%d) or map size (%d) is too big - using map size of (%d) instead!",
					   map_offset, map_size, (buffer_obj->size - map_offset));
			map_size = buffer_obj->size - map_offset;
		}
		
		cl_map_flags map_flags = ((access_type & (MAP_BUFFER_FLAG::READ_WRITE | MAP_BUFFER_FLAG::WRITE_INVALIDATE)) ==
								  MAP_BUFFER_FLAG::NONE) ? CL_MAP_READ : 0; // if no access type is specified, use read-only
		switch(access_type & MAP_BUFFER_FLAG::READ_WRITE) {
			case MAP_BUFFER_FLAG::READ_WRITE: map_flags = CL_MAP_READ | CL_MAP_WRITE; break;
			case MAP_BUFFER_FLAG::READ: map_flags = CL_MAP_READ; break;
			case MAP_BUFFER_FLAG::WRITE: map_flags = CL_MAP_WRITE; break;
			default: break;
		}
		if((access_type & MAP_BUFFER_FLAG::WRITE_INVALIDATE) != MAP_BUFFER_FLAG::NONE) {
			map_flags |= CL_MAP_WRITE_INVALIDATE_REGION;
		}
		
		alignas(128) void* map_ptr = nullptr;
		if(buffer_obj->image_type == buffer_object::IMAGE_TYPE::IMAGE_NONE) {
			CUdeviceptr* cuda_mem = cuda_buffers[buffer_obj];
			CUdeviceptr device_mem_ptr = *cuda_mem + map_offset;
			const CUdevice* device = device_map[active_device];
			CUstream stream = *cuda_queues[device];
			
			// TODO: handle buffers that already reside in host memory (were host allocated and are page-locked)!
			
			// cuda has no way of mapping device memory -> use a host buffer and write the results back later
			alignas(128) unsigned char* host_buffer = new unsigned char[map_size] alignas(128);
			if((map_flags & CL_MAP_READ) != 0) {
				// read back to host buffer
				if(blocking) {
					CU(cuMemcpyDtoH(host_buffer, device_mem_ptr, map_size));
				}
				else {
					CU(cuMemcpyDtoHAsync(host_buffer, device_mem_ptr, map_size, stream));
				}
			}
			
			// add to active mappings
			cuda_mem_mappings.emplace(host_buffer, cuda_mem_map_data {
				device,
				device_mem_ptr,
				buffer_obj,
				map_flags,
				map_offset,
				map_size,
				blocking
			});
			map_ptr = host_buffer;
		}
		else {
			// image
			oclr_error("use map_image to map an image buffer object!");
			return nullptr;
		}
		return map_ptr;
	}
	__HANDLE_CL_EXCEPTION("map_buffer")
	return nullptr;
}

void* __attribute__((aligned(128))) cudacl::map_image(opencl_base::buffer_object* buffer_obj oclr_unused,
													  const MAP_BUFFER_FLAG access_type oclr_unused,
													  const size3 origin oclr_unused,
													  const size3 region oclr_unused,
													  size_t* image_row_pitch oclr_unused,
													  size_t* image_slice_pitch oclr_unused) {
	// TODO
	assert(false && "map_image not implemented yet!");
	return nullptr;
}

pair<opencl::buffer_object*, void*> cudacl::create_and_map_buffer(const BUFFER_FLAG type,
																  const size_t size,
																  const void* data,
																  const MAP_BUFFER_FLAG access_type,
																  const size_t map_offset,
																  const size_t map_size) {
	// TODO: optimize?
	auto buffer_obj = create_buffer(type, size, data);
	auto mapped_ptr = map_buffer(buffer_obj, access_type, map_offset, map_size);
	return { buffer_obj, mapped_ptr };
}

void cudacl::unmap_buffer(opencl_base::buffer_object* buffer_obj, void* map_ptr) {
	try {
		if(buffer_obj->image_type == buffer_object::IMAGE_TYPE::IMAGE_NONE) {
			// unmap buffer
			const auto map_iter = cuda_mem_mappings.find(map_ptr);
			if(map_iter == cuda_mem_mappings.end()) {
				oclr_error("map_ptr is not a valid memory mapping pointer!");
				return;
			}
			
			//
			const auto& mapping = map_iter->second;
			if((mapping.flags & CL_MAP_WRITE) != 0 ||
			   (mapping.flags & CL_MAP_WRITE_INVALIDATE_REGION) != 0) {
				// always blocking! non-blocking would require the host pointer to be page-locked,
				// which is not desirable in this case (as the buffer might be very large)
				CU(cuMemcpyHtoD(mapping.device_mem_ptr, map_ptr, mapping.size));
			}
			// else: nothing to do for read-only buffers
			
			// delete host memory buffer again + remove from mappings map
			delete [] (unsigned char*)map_iter->first;
			cuda_mem_mappings.erase(map_iter);
		}
		else {
			// TODO: unmap image
			assert(false && "unmap_buffer not implemented for images yet!");
		}
	}
	__HANDLE_CL_EXCEPTION("unmap_buffer")
}

void cudacl::_fill_buffer(buffer_object* buffer_obj,
						  const void* pattern,
						  const size_t& pattern_size,
						  const size_t offset,
						  const size_t size) {
	//
	if((offset % pattern_size) != 0) {
		oclr_error("offset must be a multiple of pattern_size!");
		return;
	}
	if((size % pattern_size) != 0) {
		oclr_error("size must be a multiple of pattern_size!");
		return;
	}
	
	//
	size_t fill_size = size;
	if(fill_size == 0) {
		if(buffer_obj->size == 0) {
			oclr_error("can't fill 0 byte buffer (size of 0)!");
			return;
		}
		else fill_size = buffer_obj->size;
	}
	
	size_t fill_offset = offset;
	if(fill_offset >= buffer_obj->size) {
		oclr_error("fill offset (%d) out of bound!", fill_offset);
		return;
	}
	if(fill_offset+fill_size > buffer_obj->size) {
		oclr_error("fill offset (%d) or fill size (%d) is too big - using fill size of (%d) instead!",
				   fill_offset, fill_size, (buffer_obj->size - fill_offset));
		fill_size = buffer_obj->size - fill_offset;
	}
	
	try {
		CUdeviceptr* cuda_mem = cuda_buffers[buffer_obj];
		CUdeviceptr device_mem_ptr = *cuda_mem + fill_offset;
		const size_t pattern_count = fill_size / pattern_size;
		switch(pattern_size) {
			case 1:
				CU(cuMemsetD8(device_mem_ptr, *(unsigned char*)pattern, pattern_count));
				break;
			case 2:
				CU(cuMemsetD16(device_mem_ptr, *(unsigned short*)pattern, pattern_count));
				break;
			case 4:
				CU(cuMemsetD32(device_mem_ptr, *(unsigned int*)pattern, pattern_count));
				break;
			default:
				// not a pattern size that allows a fast memset
				// -> create a host buffer with the pattern and upload it
				unsigned char* pattern_buffer = new unsigned char[fill_size];
				unsigned char* write_ptr = pattern_buffer;
				for(size_t i = 0; i < pattern_count; i++) {
					memcpy(write_ptr, pattern, pattern_size);
					write_ptr += pattern_size;
				}
				CU(cuMemcpyHtoD(device_mem_ptr, pattern_buffer, fill_size));
				delete [] pattern_buffer;
				break;
		}
	}
	__HANDLE_CL_EXCEPTION("fill_buffer")
}

void cudacl::run_kernel(weak_ptr<kernel_object> kernel_obj) {
	auto kernel_ptr = kernel_obj.lock();
	if(kernel_ptr == nullptr) {
		oclr_error("invalid kernel object (nullptr)!");
		return;
	}
	
	try {
		bool all_set = true;
		for(unsigned int i = 0; i < kernel_ptr->args_passed.size(); i++) {
			if(!kernel_ptr->args_passed[i]) {
				oclr_error("argument #%u not set!", i);
				all_set = false;
			}
		}
		if(!all_set) return;
		
		//
		cuda_kernel_object* kernel = cuda_kernels[kernel_ptr];
		CUfunction* cuda_function = kernel->function;
		CUstream* stream = cuda_queues[device_map[active_device]];
		
		// check if all arguments have been set
		if(kernel->arguments.size() != kernel->info.parameters.size()) {
			throw cudacl_exception(CL_INVALID_KERNEL_ARGS);
		}
		
		// pre kernel-launch stuff:
		vector<opencl_base::buffer_object*> gl_objects;
		for(const auto& buffer_arg : kernel_ptr->buffer_args) {
			if(buffer_arg == nullptr) continue;
			if((buffer_arg->type & BUFFER_FLAG::COPY_ON_USE) != BUFFER_FLAG::NONE) {
				write_buffer(buffer_arg, buffer_arg->data);
			}
			if((buffer_arg->type & BUFFER_FLAG::OPENGL_BUFFER) != BUFFER_FLAG::NONE &&
			   !buffer_arg->manual_gl_sharing) {
				gl_objects.push_back(buffer_arg);
				kernel_ptr->has_ogl_buffers = true;
			}
		}
		if(!gl_objects.empty()) {
			for(const auto& obj : gl_objects) {
				acquire_gl_object(obj);
			}
		}
		
		// create kernel arguments block for cuda
		void** kernel_arguments = new void*[kernel->arguments.size()];
		for(cl_uint i = 0; i < kernel->arguments.size(); i++) {
			kernel_arguments[i] = kernel->arguments[i].ptr;
		}
		
		uint3 global_dim((unsigned int)cur_kernel->global[0],
						 (unsigned int)cur_kernel->global[1],
						 (unsigned int)cur_kernel->global[2]);
		uint3 local_dim((unsigned int)cur_kernel->local[0],
						(unsigned int)cur_kernel->local[1],
						(unsigned int)cur_kernel->local[2]);
		global_dim.max(uint3(1)); // dimensions must at least be 1 in cuda (== 0 in opencl)
		local_dim.max(uint3(1));
		
		CU(cuLaunchKernel(*cuda_function,
						  global_dim.x, global_dim.y, global_dim.z,
						  local_dim.x, local_dim.y, local_dim.z,
						  0, // 0 = auto?
						  *stream,
						  kernel_arguments,
						  nullptr));
		//finish();
		
		delete [] kernel_arguments;
		
		// post kernel-run stuff:
		for(const auto& buffer_arg : kernel_ptr->buffer_args) {
			if(buffer_arg == nullptr) continue;
			if((buffer_arg->type & BUFFER_FLAG::READ_BACK_RESULT) != BUFFER_FLAG::NONE) {
				read_buffer(buffer_arg->data, buffer_arg);
			}
		}
		
		for_each(begin(kernel_ptr->buffer_args), end(kernel_ptr->buffer_args),
				 [this](buffer_object* buffer_arg) {
					 if(buffer_arg == nullptr) return;
					 if((buffer_arg->type & BUFFER_FLAG::DELETE_AFTER_USE) != BUFFER_FLAG::NONE) {
						 this->delete_buffer(buffer_arg);
					 }
				 });
		
		if(kernel_ptr->has_ogl_buffers && !gl_objects.empty()) {
			for(const auto& obj : gl_objects) {
				release_gl_object(obj);
			}
		}
	}
	__HANDLE_CL_EXCEPTION_EXT("run_kernel", (" - in kernel: "+kernel_ptr->name).c_str())
}

void cudacl::finish() {
	if(active_device == nullptr) return;
	CU(cuStreamSynchronize(*cuda_queues[device_map[active_device]]));
}

void cudacl::flush() {
	return; // nothing?
}

void cudacl::barrier() {
	if(active_device == nullptr) return;
	CU(cuStreamSynchronize(*cuda_queues[device_map[active_device]]));
}

void cudacl::activate_context() {
	if(active_device == nullptr) return;
	CU(cuCtxSetCurrent(*cuda_contexts.at(device_map.at(active_device))));
}

void cudacl::deactivate_context() {
	CU(cuCtxSetCurrent(nullptr));
}

bool cudacl::set_kernel_argument(const unsigned int& index, opencl_base::buffer_object* arg) {
	if(!set_kernel_argument(index, 0, (void*)arg)) return false;
	cur_kernel->buffer_args[index] = arg;
	arg->associated_kernels[cur_kernel].push_back(index);
	return true;
}

bool cudacl::set_kernel_argument(const unsigned int& index, const opencl_base::buffer_object* arg) {
	return set_kernel_argument(index, (opencl_base::buffer_object*)arg);
}

bool cudacl::set_kernel_argument(const unsigned int& index, size_t size, void* arg) {
	try {
		// alloc memory for new argument and copy data
		cuda_kernel_object* kernel = cuda_kernels[cur_kernel];
		auto& kernel_arg = kernel->arguments[index];
		
		// delete old arg data (if there is any)
		if(kernel_arg.free_ptr && kernel_arg.ptr != nullptr) {
			free(kernel_arg.ptr);
		}
		kernel_arg.size = 0;
		kernel_arg.ptr = nullptr;
		kernel_arg.free_ptr = true;
		
		switch(kernel->info.get_parameter_type(index)) {
			case CUDACL_PARAM_TYPE::BUFFER:
			case CUDACL_PARAM_TYPE::IMAGE_1D:
			case CUDACL_PARAM_TYPE::IMAGE_2D:
			case CUDACL_PARAM_TYPE::IMAGE_3D:
				kernel_arg.size = sizeof(void*);
				if(arg != nullptr) {
					kernel_arg.free_ptr = false;
					opencl_base::buffer_object* buffer = (opencl_base::buffer_object*)arg;
					if(buffer->ogl_buffer == 0) {
						if(buffer->image_type == buffer_object::IMAGE_TYPE::IMAGE_NONE) {
							kernel_arg.ptr = (void*)cuda_buffers[buffer];
						}
						else {
							kernel_arg.ptr = (void*)cuda_images[buffer];
						}
					}
					else {
						CUgraphicsResource* res = cuda_gl_buffers[buffer];
						const auto iter = cuda_mapped_gl_buffers.find(res);
						CUdeviceptr* dev_ptr = nullptr;
						if(iter != cuda_mapped_gl_buffers.end()) {
							dev_ptr = iter->second;
						}
						else {
							dev_ptr = new CUdeviceptr();
							size_t size_ret = 0;
							CU(cuGraphicsResourceGetMappedPointer(dev_ptr, &size_ret, *res));
							cuda_mapped_gl_buffers.insert({res, dev_ptr});
						}
						kernel_arg.ptr = (void*)dev_ptr;
					}
				}
				else {
					kernel_arg.ptr = malloc(kernel_arg.size);
					memset(kernel_arg.ptr, 0, kernel_arg.size);
				}
				break;
			case CUDACL_PARAM_TYPE::SAMPLER:
				kernel_arg.size = sizeof(void*);
				kernel_arg.ptr = malloc(kernel_arg.size);
				memset(kernel_arg.ptr, 0, kernel_arg.size);
				break;
			default:
				kernel_arg.size = size;
				if(kernel_arg.size > 0) {
					kernel_arg.ptr = malloc(kernel_arg.size);
					memcpy(kernel_arg.ptr, arg, kernel_arg.size);
				}
				break;
		}
		cur_kernel->args_passed[index] = true;
		return true;
	}
	__HANDLE_CL_EXCEPTION("set_kernel_argument")
	return false;
}

size_t cudacl::get_kernel_work_group_size() const {
	if(cur_kernel == nullptr || active_device == nullptr) return 0;
	
	try {
		CUfunction* cuda_function = cuda_kernels.at(cur_kernel)->function;
		int ret = 0;
		CU(cuFuncGetAttribute(&ret, CU_FUNC_ATTRIBUTE_MAX_THREADS_PER_BLOCK, *cuda_function));
		return ret;
	}
	__HANDLE_CL_EXCEPTION("get_kernel_work_group_size")
	return 0;
}

void cudacl::acquire_gl_object(buffer_object* gl_buffer_obj) {
	CUstream stream = *cuda_queues[device_map[active_device]];
	CUgraphicsResource* cuda_gl_buffer = cuda_gl_buffers[gl_buffer_obj];
	CU(cuGraphicsMapResources(1, cuda_gl_buffer, stream));
}

void cudacl::release_gl_object(buffer_object* gl_buffer_obj) {
	CUstream stream = *cuda_queues[device_map[active_device]];
	CUgraphicsResource* cuda_gl_buffer = cuda_gl_buffers[gl_buffer_obj];
	const auto iter = cuda_mapped_gl_buffers.find(cuda_gl_buffer);
	if(iter != cuda_mapped_gl_buffers.end()) {
		delete iter->second;
		cuda_mapped_gl_buffers.erase(iter);
	}
	CU(cuGraphicsUnmapResources(1, cuda_gl_buffer, stream));
}

void cudacl::set_active_device(const opencl_base::DEVICE_TYPE& dev oclr_unused) {
	// TODO: select corresponding cuda device
	active_device = fastest_gpu;
}

#endif
