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

// instantiated in here
cuda_api_ptrs cuda_api;

#if !defined(__WINDOWS__)
#include <dlfcn.h>
#else
// TODO: !
#endif

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
	
#if !defined(__WINDOWS__)
	void* cuda_lib_ptr = dlopen(cuda_lib_name, RTLD_NOW);
	
	(void*&)cuda_api.array_3d_create = dlsym(cuda_lib_ptr, "cuArray3DCreate_v2");
	if(cuda_api.array_3d_create == nullptr) log_error("failed to retrieve function pointer for \"cuArray3DCreate_v2\"");
	
	(void*&)cuda_api.array_destroy = dlsym(cuda_lib_ptr, "cuArrayDestroy");
	if(cuda_api.array_destroy == nullptr) log_error("failed to retrieve function pointer for \"cuArrayDestroy\"");
	
	(void*&)cuda_api.ctx_create = dlsym(cuda_lib_ptr, "cuCtxCreate_v2");
	if(cuda_api.ctx_create == nullptr) log_error("failed to retrieve function pointer for \"cuCtxCreate_v2\"");
	
	(void*&)cuda_api.ctx_get_limit = dlsym(cuda_lib_ptr, "cuCtxGetLimit");
	if(cuda_api.ctx_get_limit == nullptr) log_error("failed to retrieve function pointer for \"cuCtxGetLimit\"");
	
	(void*&)cuda_api.ctx_set_current = dlsym(cuda_lib_ptr, "cuCtxSetCurrent");
	if(cuda_api.ctx_set_current == nullptr) log_error("failed to retrieve function pointer for \"cuCtxSetCurrent\"");
	
	(void*&)cuda_api.device_compute_capability = dlsym(cuda_lib_ptr, "cuDeviceComputeCapability");
	if(cuda_api.device_compute_capability == nullptr) log_error("failed to retrieve function pointer for \"cuDeviceComputeCapability\"");
	
	(void*&)cuda_api.device_get = dlsym(cuda_lib_ptr, "cuDeviceGet");
	if(cuda_api.device_get == nullptr) log_error("failed to retrieve function pointer for \"cuDeviceGet\"");
	
	(void*&)cuda_api.device_get_attribute = dlsym(cuda_lib_ptr, "cuDeviceGetAttribute");
	if(cuda_api.device_get_attribute == nullptr) log_error("failed to retrieve function pointer for \"cuDeviceGetAttribute\"");
	
	(void*&)cuda_api.device_get_count = dlsym(cuda_lib_ptr, "cuDeviceGetCount");
	if(cuda_api.device_get_count == nullptr) log_error("failed to retrieve function pointer for \"cuDeviceGetCount\"");
	
	(void*&)cuda_api.device_get_name = dlsym(cuda_lib_ptr, "cuDeviceGetName");
	if(cuda_api.device_get_name == nullptr) log_error("failed to retrieve function pointer for \"cuDeviceGetName\"");
	
	(void*&)cuda_api.device_total_mem = dlsym(cuda_lib_ptr, "cuDeviceTotalMem_v2");
	if(cuda_api.device_total_mem == nullptr) log_error("failed to retrieve function pointer for \"cuDeviceTotalMem_v2\"");
	
	(void*&)cuda_api.driver_get_version = dlsym(cuda_lib_ptr, "cuDriverGetVersion");
	if(cuda_api.driver_get_version == nullptr) log_error("failed to retrieve function pointer for \"cuDriverGetVersion\"");
	
	(void*&)cuda_api.get_error_name = dlsym(cuda_lib_ptr, "cuGetErrorName");
	if(cuda_api.get_error_name == nullptr) log_error("failed to retrieve function pointer for \"cuGetErrorName\"");
	
	(void*&)cuda_api.get_error_string = dlsym(cuda_lib_ptr, "cuGetErrorString");
	if(cuda_api.get_error_string == nullptr) log_error("failed to retrieve function pointer for \"cuGetErrorString\"");
	
	(void*&)cuda_api.graphics_gl_register_buffer = dlsym(cuda_lib_ptr, "cuGraphicsGLRegisterBuffer");
	if(cuda_api.graphics_gl_register_buffer == nullptr) log_error("failed to retrieve function pointer for \"cuGraphicsGLRegisterBuffer\"");
	
	(void*&)cuda_api.graphics_gl_register_image = dlsym(cuda_lib_ptr, "cuGraphicsGLRegisterImage");
	if(cuda_api.graphics_gl_register_image == nullptr) log_error("failed to retrieve function pointer for \"cuGraphicsGLRegisterImage\"");
	
	(void*&)cuda_api.graphics_map_resources = dlsym(cuda_lib_ptr, "cuGraphicsMapResources");
	if(cuda_api.graphics_map_resources == nullptr) log_error("failed to retrieve function pointer for \"cuGraphicsMapResources\"");
	
	(void*&)cuda_api.graphics_resource_get_mapped_pointer = dlsym(cuda_lib_ptr, "cuGraphicsResourceGetMappedPointer_v2");
	if(cuda_api.graphics_resource_get_mapped_pointer == nullptr) log_error("failed to retrieve function pointer for \"cuGraphicsResourceGetMappedPointer_v2\"");
	
	(void*&)cuda_api.graphics_sub_resource_get_mapped_array = dlsym(cuda_lib_ptr, "cuGraphicsSubResourceGetMappedArray");
	if(cuda_api.graphics_sub_resource_get_mapped_array == nullptr) log_error("failed to retrieve function pointer for \"cuGraphicsSubResourceGetMappedArray\"");
	
	(void*&)cuda_api.graphics_unmap_resources = dlsym(cuda_lib_ptr, "cuGraphicsUnmapResources");
	if(cuda_api.graphics_unmap_resources == nullptr) log_error("failed to retrieve function pointer for \"cuGraphicsUnmapResources\"");
	
	(void*&)cuda_api.init = dlsym(cuda_lib_ptr, "cuInit");
	if(cuda_api.init == nullptr) log_error("failed to retrieve function pointer for \"cuInit\"");
	
	(void*&)cuda_api.launch_kernel = dlsym(cuda_lib_ptr, "cuLaunchKernel");
	if(cuda_api.launch_kernel == nullptr) log_error("failed to retrieve function pointer for \"cuLaunchKernel\"");
	
	(void*&)cuda_api.link_add_data = dlsym(cuda_lib_ptr, "cuLinkAddData_v2");
	if(cuda_api.link_add_data == nullptr) log_error("failed to retrieve function pointer for \"cuLinkAddData_v2\"");
	
	(void*&)cuda_api.link_complete = dlsym(cuda_lib_ptr, "cuLinkComplete");
	if(cuda_api.link_complete == nullptr) log_error("failed to retrieve function pointer for \"cuLinkComplete\"");
	
	(void*&)cuda_api.link_create = dlsym(cuda_lib_ptr, "cuLinkCreate_v2");
	if(cuda_api.link_create == nullptr) log_error("failed to retrieve function pointer for \"cuLinkCreate_v2\"");
	
	(void*&)cuda_api.link_destroy = dlsym(cuda_lib_ptr, "cuLinkDestroy");
	if(cuda_api.link_destroy == nullptr) log_error("failed to retrieve function pointer for \"cuLinkDestroy\"");
	
	(void*&)cuda_api.mem_alloc = dlsym(cuda_lib_ptr, "cuMemAlloc_v2");
	if(cuda_api.mem_alloc == nullptr) log_error("failed to retrieve function pointer for \"cuMemAlloc_v2\"");
	
	(void*&)cuda_api.mem_free = dlsym(cuda_lib_ptr, "cuMemFree_v2");
	if(cuda_api.mem_free == nullptr) log_error("failed to retrieve function pointer for \"cuMemFree_v2\"");
	
	(void*&)cuda_api.mem_host_get_device_pointer = dlsym(cuda_lib_ptr, "cuMemHostGetDevicePointer_v2");
	if(cuda_api.mem_host_get_device_pointer == nullptr) log_error("failed to retrieve function pointer for \"cuMemHostGetDevicePointer_v2\"");
	
	(void*&)cuda_api.mem_host_register = dlsym(cuda_lib_ptr, "cuMemHostRegister_v2");
	if(cuda_api.mem_host_register == nullptr) log_error("failed to retrieve function pointer for \"cuMemHostRegister_v2\"");
	
	(void*&)cuda_api.mem_host_unregister = dlsym(cuda_lib_ptr, "cuMemHostUnregister");
	if(cuda_api.mem_host_unregister == nullptr) log_error("failed to retrieve function pointer for \"cuMemHostUnregister\"");
	
	(void*&)cuda_api.memcpy_3d = dlsym(cuda_lib_ptr, "cuMemcpy3D_v2");
	if(cuda_api.memcpy_3d == nullptr) log_error("failed to retrieve function pointer for \"cuMemcpy3D_v2\"");
	
	(void*&)cuda_api.memcpy_3d_async = dlsym(cuda_lib_ptr, "cuMemcpy3DAsync_v2");
	if(cuda_api.memcpy_3d_async == nullptr) log_error("failed to retrieve function pointer for \"cuMemcpy3DAsync_v2\"");
	
	(void*&)cuda_api.memcpy_dtod = dlsym(cuda_lib_ptr, "cuMemcpyDtoD_v2");
	if(cuda_api.memcpy_dtod == nullptr) log_error("failed to retrieve function pointer for \"cuMemcpyDtoD_v2\"");
	
	(void*&)cuda_api.memcpy_dtod_async = dlsym(cuda_lib_ptr, "cuMemcpyDtoDAsync_v2");
	if(cuda_api.memcpy_dtod_async == nullptr) log_error("failed to retrieve function pointer for \"cuMemcpyDtoDAsync_v2\"");
	
	(void*&)cuda_api.memcpy_dtoh = dlsym(cuda_lib_ptr, "cuMemcpyDtoH_v2");
	if(cuda_api.memcpy_dtoh == nullptr) log_error("failed to retrieve function pointer for \"cuMemcpyDtoH_v2\"");
	
	(void*&)cuda_api.memcpy_dtoh_async = dlsym(cuda_lib_ptr, "cuMemcpyDtoHAsync_v2");
	if(cuda_api.memcpy_dtoh_async == nullptr) log_error("failed to retrieve function pointer for \"cuMemcpyDtoHAsync_v2\"");
	
	(void*&)cuda_api.memcpy_htod = dlsym(cuda_lib_ptr, "cuMemcpyHtoD_v2");
	if(cuda_api.memcpy_htod == nullptr) log_error("failed to retrieve function pointer for \"cuMemcpyHtoD_v2\"");
	
	(void*&)cuda_api.memcpy_htod_async = dlsym(cuda_lib_ptr, "cuMemcpyHtoDAsync_v2");
	if(cuda_api.memcpy_htod_async == nullptr) log_error("failed to retrieve function pointer for \"cuMemcpyHtoDAsync_v2\"");
	
	(void*&)cuda_api.memset_d16_async = dlsym(cuda_lib_ptr, "cuMemsetD16Async");
	if(cuda_api.memset_d16_async == nullptr) log_error("failed to retrieve function pointer for \"cuMemsetD16Async\"");
	
	(void*&)cuda_api.memset_d32_async = dlsym(cuda_lib_ptr, "cuMemsetD32Async");
	if(cuda_api.memset_d32_async == nullptr) log_error("failed to retrieve function pointer for \"cuMemsetD32Async\"");
	
	(void*&)cuda_api.memset_d8_async = dlsym(cuda_lib_ptr, "cuMemsetD8Async");
	if(cuda_api.memset_d8_async == nullptr) log_error("failed to retrieve function pointer for \"cuMemsetD8Async\"");
	
	(void*&)cuda_api.module_get_function = dlsym(cuda_lib_ptr, "cuModuleGetFunction");
	if(cuda_api.module_get_function == nullptr) log_error("failed to retrieve function pointer for \"cuModuleGetFunction\"");
	
	(void*&)cuda_api.module_load_data = dlsym(cuda_lib_ptr, "cuModuleLoadData");
	if(cuda_api.module_load_data == nullptr) log_error("failed to retrieve function pointer for \"cuModuleLoadData\"");
	
	(void*&)cuda_api.module_load_data_ex = dlsym(cuda_lib_ptr, "cuModuleLoadDataEx");
	if(cuda_api.module_load_data_ex == nullptr) log_error("failed to retrieve function pointer for \"cuModuleLoadDataEx\"");
	
	(void*&)cuda_api.occupancy_max_potential_block_size = dlsym(cuda_lib_ptr, "cuOccupancyMaxPotentialBlockSize");
	if(cuda_api.occupancy_max_potential_block_size == nullptr) log_error("failed to retrieve function pointer for \"cuOccupancyMaxPotentialBlockSize\"");
	
	(void*&)cuda_api.stream_create = dlsym(cuda_lib_ptr, "cuStreamCreate");
	if(cuda_api.stream_create == nullptr) log_error("failed to retrieve function pointer for \"cuStreamCreate\"");
	
	(void*&)cuda_api.stream_synchronize = dlsym(cuda_lib_ptr, "cuStreamSynchronize");
	if(cuda_api.stream_synchronize == nullptr) log_error("failed to retrieve function pointer for \"cuStreamSynchronize\"");
	
	(void*&)cuda_api.surf_object_create = dlsym(cuda_lib_ptr, "cuSurfObjectCreate");
	if(cuda_api.surf_object_create == nullptr) log_error("failed to retrieve function pointer for \"cuSurfObjectCreate\"");
	
	(void*&)cuda_api.surf_object_destroy = dlsym(cuda_lib_ptr, "cuSurfObjectDestroy");
	if(cuda_api.surf_object_destroy == nullptr) log_error("failed to retrieve function pointer for \"cuSurfObjectDestroy\"");
	
	(void*&)cuda_api.tex_object_create = dlsym(cuda_lib_ptr, "cuTexObjectCreate");
	if(cuda_api.tex_object_create == nullptr) log_error("failed to retrieve function pointer for \"cuTexObjectCreate\"");
	
	(void*&)cuda_api.tex_object_destroy = dlsym(cuda_lib_ptr, "cuTexObjectDestroy");
	if(cuda_api.tex_object_destroy == nullptr) log_error("failed to retrieve function pointer for \"cuTexObjectDestroy\"");
#else
	// TODO: !
#endif
	logger::flush();
	
	// done
	init_success = true;
	return init_success;
}
