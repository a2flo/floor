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

#include <floor/compute/cuda/cuda_common.hpp>
#include <floor/compute/cuda/cuda_api.hpp>

#if !defined(FLOOR_NO_CUDA)

// instantiated in here
cuda_api_ptrs cuda_api;

#if !defined(__WINDOWS__)
#include <dlfcn.h>
#define open_dynamic_library(file_name) dlopen(file_name, RTLD_NOW)
#define load_symbol(lib_handle, symbol_name) dlsym(lib_handle, symbol_name)
typedef void* lib_handle_type;
#else
#include <windows.h>
#define open_dynamic_library(file_name) LoadLibrary(file_name)
#define load_symbol(lib_handle, symbol_name) (void*)GetProcAddress(lib_handle, symbol_name)
typedef HMODULE lib_handle_type;
#endif
static lib_handle_type cuda_lib { nullptr };

bool cuda_api_init() {
	// already init check
	static bool did_init = false, init_success = false;
	if(did_init) return init_success;
	did_init = true;
	
	// init function pointers
	static const char* cuda_lib_name {
#if defined(__APPLE__)
		"/Library/Frameworks/CUDA.framework/CUDA"
#elif defined(__WINDOWS__)
		"nvcuda.dll"
#else
		"libcuda.so"
#endif
	};
	
	cuda_lib = open_dynamic_library(cuda_lib_name);
	if(cuda_lib == nullptr) {
		log_error("failed to open cuda library \"%s\"!", cuda_lib_name);
		return false;
	}
	
	(void*&)cuda_api.array_3d_create = load_symbol(cuda_lib, "cuArray3DCreate_v2");
	if(cuda_api.array_3d_create == nullptr) log_error("failed to retrieve function pointer for \"cuArray3DCreate_v2\"");
	
	(void*&)cuda_api.array_3d_get_descriptor = load_symbol(cuda_lib, "cuArray3DGetDescriptor_v2");
	if(cuda_api.array_3d_get_descriptor == nullptr) log_error("failed to retrieve function pointer for \"cuArray3DGetDescriptor_v2\"");
	
	(void*&)cuda_api.array_destroy = load_symbol(cuda_lib, "cuArrayDestroy");
	if(cuda_api.array_destroy == nullptr) log_error("failed to retrieve function pointer for \"cuArrayDestroy\"");
	
	(void*&)cuda_api.ctx_create = load_symbol(cuda_lib, "cuCtxCreate_v2");
	if(cuda_api.ctx_create == nullptr) log_error("failed to retrieve function pointer for \"cuCtxCreate_v2\"");
	
	(void*&)cuda_api.ctx_get_limit = load_symbol(cuda_lib, "cuCtxGetLimit");
	if(cuda_api.ctx_get_limit == nullptr) log_error("failed to retrieve function pointer for \"cuCtxGetLimit\"");
	
	(void*&)cuda_api.ctx_set_current = load_symbol(cuda_lib, "cuCtxSetCurrent");
	if(cuda_api.ctx_set_current == nullptr) log_error("failed to retrieve function pointer for \"cuCtxSetCurrent\"");
	
	(void*&)cuda_api.device_compute_capability = load_symbol(cuda_lib, "cuDeviceComputeCapability");
	if(cuda_api.device_compute_capability == nullptr) log_error("failed to retrieve function pointer for \"cuDeviceComputeCapability\"");
	
	(void*&)cuda_api.device_get = load_symbol(cuda_lib, "cuDeviceGet");
	if(cuda_api.device_get == nullptr) log_error("failed to retrieve function pointer for \"cuDeviceGet\"");
	
	(void*&)cuda_api.device_get_attribute = load_symbol(cuda_lib, "cuDeviceGetAttribute");
	if(cuda_api.device_get_attribute == nullptr) log_error("failed to retrieve function pointer for \"cuDeviceGetAttribute\"");
	
	(void*&)cuda_api.device_get_count = load_symbol(cuda_lib, "cuDeviceGetCount");
	if(cuda_api.device_get_count == nullptr) log_error("failed to retrieve function pointer for \"cuDeviceGetCount\"");
	
	(void*&)cuda_api.device_get_name = load_symbol(cuda_lib, "cuDeviceGetName");
	if(cuda_api.device_get_name == nullptr) log_error("failed to retrieve function pointer for \"cuDeviceGetName\"");
	
	(void*&)cuda_api.device_total_mem = load_symbol(cuda_lib, "cuDeviceTotalMem_v2");
	if(cuda_api.device_total_mem == nullptr) log_error("failed to retrieve function pointer for \"cuDeviceTotalMem_v2\"");
	
	(void*&)cuda_api.driver_get_version = load_symbol(cuda_lib, "cuDriverGetVersion");
	if(cuda_api.driver_get_version == nullptr) log_error("failed to retrieve function pointer for \"cuDriverGetVersion\"");
	
	(void*&)cuda_api.get_error_name = load_symbol(cuda_lib, "cuGetErrorName");
	if(cuda_api.get_error_name == nullptr) log_error("failed to retrieve function pointer for \"cuGetErrorName\"");
	
	(void*&)cuda_api.get_error_string = load_symbol(cuda_lib, "cuGetErrorString");
	if(cuda_api.get_error_string == nullptr) log_error("failed to retrieve function pointer for \"cuGetErrorString\"");
	
	(void*&)cuda_api.graphics_gl_register_buffer = load_symbol(cuda_lib, "cuGraphicsGLRegisterBuffer");
	if(cuda_api.graphics_gl_register_buffer == nullptr) log_error("failed to retrieve function pointer for \"cuGraphicsGLRegisterBuffer\"");
	
	(void*&)cuda_api.graphics_gl_register_image = load_symbol(cuda_lib, "cuGraphicsGLRegisterImage");
	if(cuda_api.graphics_gl_register_image == nullptr) log_error("failed to retrieve function pointer for \"cuGraphicsGLRegisterImage\"");
	
	(void*&)cuda_api.graphics_map_resources = load_symbol(cuda_lib, "cuGraphicsMapResources");
	if(cuda_api.graphics_map_resources == nullptr) log_error("failed to retrieve function pointer for \"cuGraphicsMapResources\"");
	
	(void*&)cuda_api.graphics_resource_get_mapped_pointer = load_symbol(cuda_lib, "cuGraphicsResourceGetMappedPointer_v2");
	if(cuda_api.graphics_resource_get_mapped_pointer == nullptr) log_error("failed to retrieve function pointer for \"cuGraphicsResourceGetMappedPointer_v2\"");
	
	(void*&)cuda_api.graphics_sub_resource_get_mapped_array = load_symbol(cuda_lib, "cuGraphicsSubResourceGetMappedArray");
	if(cuda_api.graphics_sub_resource_get_mapped_array == nullptr) log_error("failed to retrieve function pointer for \"cuGraphicsSubResourceGetMappedArray\"");
	
	(void*&)cuda_api.graphics_unmap_resources = load_symbol(cuda_lib, "cuGraphicsUnmapResources");
	if(cuda_api.graphics_unmap_resources == nullptr) log_error("failed to retrieve function pointer for \"cuGraphicsUnmapResources\"");
	
	(void*&)cuda_api.init = load_symbol(cuda_lib, "cuInit");
	if(cuda_api.init == nullptr) log_error("failed to retrieve function pointer for \"cuInit\"");
	
	(void*&)cuda_api.launch_kernel = load_symbol(cuda_lib, "cuLaunchKernel");
	if(cuda_api.launch_kernel == nullptr) log_error("failed to retrieve function pointer for \"cuLaunchKernel\"");
	
	(void*&)cuda_api.link_add_data = load_symbol(cuda_lib, "cuLinkAddData_v2");
	if(cuda_api.link_add_data == nullptr) log_error("failed to retrieve function pointer for \"cuLinkAddData_v2\"");
	
	(void*&)cuda_api.link_complete = load_symbol(cuda_lib, "cuLinkComplete");
	if(cuda_api.link_complete == nullptr) log_error("failed to retrieve function pointer for \"cuLinkComplete\"");
	
	(void*&)cuda_api.link_create = load_symbol(cuda_lib, "cuLinkCreate_v2");
	if(cuda_api.link_create == nullptr) log_error("failed to retrieve function pointer for \"cuLinkCreate_v2\"");
	
	(void*&)cuda_api.link_destroy = load_symbol(cuda_lib, "cuLinkDestroy");
	if(cuda_api.link_destroy == nullptr) log_error("failed to retrieve function pointer for \"cuLinkDestroy\"");
	
	(void*&)cuda_api.mem_alloc = load_symbol(cuda_lib, "cuMemAlloc_v2");
	if(cuda_api.mem_alloc == nullptr) log_error("failed to retrieve function pointer for \"cuMemAlloc_v2\"");
	
	(void*&)cuda_api.mem_free = load_symbol(cuda_lib, "cuMemFree_v2");
	if(cuda_api.mem_free == nullptr) log_error("failed to retrieve function pointer for \"cuMemFree_v2\"");
	
	(void*&)cuda_api.mem_host_get_device_pointer = load_symbol(cuda_lib, "cuMemHostGetDevicePointer_v2");
	if(cuda_api.mem_host_get_device_pointer == nullptr) log_error("failed to retrieve function pointer for \"cuMemHostGetDevicePointer_v2\"");
	
	(void*&)cuda_api.mem_host_register = load_symbol(cuda_lib, "cuMemHostRegister_v2");
	if(cuda_api.mem_host_register == nullptr) log_error("failed to retrieve function pointer for \"cuMemHostRegister_v2\"");
	
	(void*&)cuda_api.mem_host_unregister = load_symbol(cuda_lib, "cuMemHostUnregister");
	if(cuda_api.mem_host_unregister == nullptr) log_error("failed to retrieve function pointer for \"cuMemHostUnregister\"");
	
	(void*&)cuda_api.memcpy_3d = load_symbol(cuda_lib, "cuMemcpy3D_v2");
	if(cuda_api.memcpy_3d == nullptr) log_error("failed to retrieve function pointer for \"cuMemcpy3D_v2\"");
	
	(void*&)cuda_api.memcpy_3d_async = load_symbol(cuda_lib, "cuMemcpy3DAsync_v2");
	if(cuda_api.memcpy_3d_async == nullptr) log_error("failed to retrieve function pointer for \"cuMemcpy3DAsync_v2\"");
	
	(void*&)cuda_api.memcpy_dtod = load_symbol(cuda_lib, "cuMemcpyDtoD_v2");
	if(cuda_api.memcpy_dtod == nullptr) log_error("failed to retrieve function pointer for \"cuMemcpyDtoD_v2\"");
	
	(void*&)cuda_api.memcpy_dtod_async = load_symbol(cuda_lib, "cuMemcpyDtoDAsync_v2");
	if(cuda_api.memcpy_dtod_async == nullptr) log_error("failed to retrieve function pointer for \"cuMemcpyDtoDAsync_v2\"");
	
	(void*&)cuda_api.memcpy_dtoh = load_symbol(cuda_lib, "cuMemcpyDtoH_v2");
	if(cuda_api.memcpy_dtoh == nullptr) log_error("failed to retrieve function pointer for \"cuMemcpyDtoH_v2\"");
	
	(void*&)cuda_api.memcpy_dtoh_async = load_symbol(cuda_lib, "cuMemcpyDtoHAsync_v2");
	if(cuda_api.memcpy_dtoh_async == nullptr) log_error("failed to retrieve function pointer for \"cuMemcpyDtoHAsync_v2\"");
	
	(void*&)cuda_api.memcpy_htod = load_symbol(cuda_lib, "cuMemcpyHtoD_v2");
	if(cuda_api.memcpy_htod == nullptr) log_error("failed to retrieve function pointer for \"cuMemcpyHtoD_v2\"");
	
	(void*&)cuda_api.memcpy_htod_async = load_symbol(cuda_lib, "cuMemcpyHtoDAsync_v2");
	if(cuda_api.memcpy_htod_async == nullptr) log_error("failed to retrieve function pointer for \"cuMemcpyHtoDAsync_v2\"");
	
	(void*&)cuda_api.memset_d16_async = load_symbol(cuda_lib, "cuMemsetD16Async");
	if(cuda_api.memset_d16_async == nullptr) log_error("failed to retrieve function pointer for \"cuMemsetD16Async\"");
	
	(void*&)cuda_api.memset_d32_async = load_symbol(cuda_lib, "cuMemsetD32Async");
	if(cuda_api.memset_d32_async == nullptr) log_error("failed to retrieve function pointer for \"cuMemsetD32Async\"");
	
	(void*&)cuda_api.memset_d8_async = load_symbol(cuda_lib, "cuMemsetD8Async");
	if(cuda_api.memset_d8_async == nullptr) log_error("failed to retrieve function pointer for \"cuMemsetD8Async\"");
	
	(void*&)cuda_api.module_get_function = load_symbol(cuda_lib, "cuModuleGetFunction");
	if(cuda_api.module_get_function == nullptr) log_error("failed to retrieve function pointer for \"cuModuleGetFunction\"");
	
	(void*&)cuda_api.module_load_data = load_symbol(cuda_lib, "cuModuleLoadData");
	if(cuda_api.module_load_data == nullptr) log_error("failed to retrieve function pointer for \"cuModuleLoadData\"");
	
	(void*&)cuda_api.module_load_data_ex = load_symbol(cuda_lib, "cuModuleLoadDataEx");
	if(cuda_api.module_load_data_ex == nullptr) log_error("failed to retrieve function pointer for \"cuModuleLoadDataEx\"");
	
	(void*&)cuda_api.occupancy_max_potential_block_size = load_symbol(cuda_lib, "cuOccupancyMaxPotentialBlockSize");
	if(cuda_api.occupancy_max_potential_block_size == nullptr) log_error("failed to retrieve function pointer for \"cuOccupancyMaxPotentialBlockSize\"");
	
	(void*&)cuda_api.stream_create = load_symbol(cuda_lib, "cuStreamCreate");
	if(cuda_api.stream_create == nullptr) log_error("failed to retrieve function pointer for \"cuStreamCreate\"");
	
	(void*&)cuda_api.stream_synchronize = load_symbol(cuda_lib, "cuStreamSynchronize");
	if(cuda_api.stream_synchronize == nullptr) log_error("failed to retrieve function pointer for \"cuStreamSynchronize\"");
	
	(void*&)cuda_api.surf_object_create = load_symbol(cuda_lib, "cuSurfObjectCreate");
	if(cuda_api.surf_object_create == nullptr) log_error("failed to retrieve function pointer for \"cuSurfObjectCreate\"");
	
	(void*&)cuda_api.surf_object_destroy = load_symbol(cuda_lib, "cuSurfObjectDestroy");
	if(cuda_api.surf_object_destroy == nullptr) log_error("failed to retrieve function pointer for \"cuSurfObjectDestroy\"");
	
	(void*&)cuda_api.tex_object_create = load_symbol(cuda_lib, "cuTexObjectCreate");
	if(cuda_api.tex_object_create == nullptr) log_error("failed to retrieve function pointer for \"cuTexObjectCreate\"");
	
	(void*&)cuda_api.tex_object_destroy = load_symbol(cuda_lib, "cuTexObjectDestroy");
	if(cuda_api.tex_object_destroy == nullptr) log_error("failed to retrieve function pointer for \"cuTexObjectDestroy\"");

	logger::flush();
	
	// done
	init_success = true;
	return init_success;
}

#endif
