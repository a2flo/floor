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

#include "opencl.h"
#include "pipeline/image.h"
#include "oclraster.h"

#if defined(__APPLE__)
#if defined(OCLRASTER_IOS)
#include "ios/ios_helper.h"
#else
#include "osx/osx_helper.h"
#endif
#endif

#if defined(__FreeBSD__)
#include <sys/types.h>
#include <sys/sysctl.h>
#endif

#define CLINFO_STR_SIZE (65536*2)

shared_ptr<opencl::kernel_object> opencl_base::null_kernel_object { nullptr };

// 2d array: [IMAGE_TYPE][IMAGE_CHANNEL] -> cl::ImageFormat (-> will be (0, 0) if not supported)
static array<array<cl::ImageFormat, (size_t)IMAGE_CHANNEL::__MAX_CHANNEL>, (size_t)IMAGE_TYPE::__MAX_TYPE> internal_image_format_mapping;

// these functions are not part of the FreeBSD libOpenCL.so, so define them here
#if defined(__FreeBSD__)
CL_API_ENTRY cl_int CL_API_CALL
clEnqueueAcquireGLObjects(cl_command_queue      /* command_queue */,
                          cl_uint               /* num_objects */,
                          const cl_mem *        /* mem_objects */,
                          cl_uint               /* num_events_in_wait_list */,
                          const cl_event *      /* event_wait_list */,
                          cl_event *            /* event */) {
	oclr_error("don't use gl sharing functions on FreeBSD!");
	throw runtime_error("don't use gl sharing functions on FreeBSD!");
}

CL_API_ENTRY cl_int CL_API_CALL
clEnqueueReleaseGLObjects(cl_command_queue      /* command_queue */,
                          cl_uint               /* num_objects */,
                          const cl_mem *        /* mem_objects */,
                          cl_uint               /* num_events_in_wait_list */,
                          const cl_event *      /* event_wait_list */,
                          cl_event *            /* event */) {
	oclr_error("don't use gl sharing functions on FreeBSD!");
	throw runtime_error("don't use gl sharing functions on FreeBSD!");
}
CL_API_ENTRY cl_mem CL_API_CALL
clCreateFromGLTexture2D(cl_context      /* context */,
                        cl_mem_flags    /* flags */,
                        cl_GLenum       /* target */,
                        cl_GLint        /* miplevel */,
                        cl_GLuint       /* texture */,
                        cl_int *        /* errcode_ret */) {
	oclr_error("don't use gl sharing functions on FreeBSD!");
	throw runtime_error("don't use gl sharing functions on FreeBSD!");
}
CL_API_ENTRY cl_mem CL_API_CALL
clCreateFromGLRenderbuffer(cl_context   /* context */,
                           cl_mem_flags /* flags */,
                           cl_GLuint    /* renderbuffer */,
                           cl_int *     /* errcode_ret */) {
	oclr_error("don't use gl sharing functions on FreeBSD!");
	throw runtime_error("don't use gl sharing functions on FreeBSD!");
}

CL_API_ENTRY cl_mem CL_API_CALL
clCreateFromGLBuffer(cl_context     /* context */,
                     cl_mem_flags   /* flags */,
                     cl_GLuint      /* bufobj */,
                     int *          /* errcode_ret */) {
	oclr_error("don't use gl sharing functions on FreeBSD!");
	throw runtime_error("don't use gl sharing functions on FreeBSD!");
}
#endif

///////////////////////////////////////////////////////////////////////////////////////////////////
// common in all opencl implementations:

opencl_base::opencl_base() {
}

opencl_base::~opencl_base() {
}

vector<pair<opencl_base::PLATFORM_VENDOR, string>> opencl_base::get_platforms() {
	vector<pair<PLATFORM_VENDOR, string>> available_platforms;
#if !defined(__APPLE__)
	vector<cl::Platform> available_cl_platforms;
	cl::Platform::get(&available_cl_platforms);
	size_t platform_index = 0;
	for(const auto& pl : available_cl_platforms) {
		const string platform_str = pl.getInfo<CL_PLATFORM_NAME>();
		const string platform_vendor_str = core::str_to_lower(platform_str);
		PLATFORM_VENDOR vendor = PLATFORM_VENDOR::UNKNOWN;
		if(platform_vendor_str.find("nvidia") != string::npos) {
			vendor = PLATFORM_VENDOR::NVIDIA;
		}
		else if(platform_vendor_str.find("amd") != string::npos) {
			vendor = PLATFORM_VENDOR::AMD;
		}
		else if(platform_vendor_str.find("intel") != string::npos) {
			vendor = PLATFORM_VENDOR::INTEL;
		}
		else if(platform_vendor_str.find("apple") != string::npos) {
			vendor = PLATFORM_VENDOR::APPLE;
		}
		available_platforms.push_back({ vendor, size_t2string(platform_index) });
		platform_index++;
	}
#else
	available_platforms.push_back({ PLATFORM_VENDOR::APPLE, "0" });
#endif
	
#if defined(OCLRASTER_CUDA_CL)
	available_platforms.push_back({ PLATFORM_VENDOR::CUDA, "cuda" });
#endif
	
	return available_platforms;
}

string opencl_base::platform_vendor_to_str(const opencl_base::PLATFORM_VENDOR& pvendor) {
	switch(pvendor) {
		case PLATFORM_VENDOR::NVIDIA: return "NVIDIA";
		case PLATFORM_VENDOR::INTEL: return "INTEL";
		case PLATFORM_VENDOR::AMD: return "AMD";
		case PLATFORM_VENDOR::APPLE: return "APPLE";
		case PLATFORM_VENDOR::FREEOCL: return "FREEOCL";
		case PLATFORM_VENDOR::POCL: return "POCL";
		case PLATFORM_VENDOR::CUDA: return "CUDA";
		case PLATFORM_VENDOR::UNKNOWN: break;
	}
	return "UNKNOWN";
}

opencl_base::PLATFORM_VENDOR opencl_base::get_platform_vendor() const {
	return platform_vendor;
}

opencl_base::CL_VERSION opencl_base::get_platform_cl_version() const {
	return platform_cl_version;
}

void opencl_base::destroy_kernels() {
	cur_kernel = nullptr;
	kernels_lock.lock();
	for(auto& k : kernels) {
		kernel_object::unassociate_buffers(k.second);
		if(k.second.use_count() > 1) {
			oclr_error("kernel object (%X) use count > 1 (%u) - kernel object is still used somewhere!",
					   k.second.get(), k.second.use_count());
		}
		k.second = nullptr; // implicit delete
	}
	kernels.clear();
	kernels_lock.unlock();
}

bool opencl_base::is_cpu_support() const {
	// if a fastest cpu exists, we do have cpu support
	return (fastest_cpu != nullptr);
}

bool opencl_base::is_gpu_support() const {
	// if a fastest gpu exists, we do have gpu support
	return (fastest_gpu != nullptr);
}

weak_ptr<opencl_base::kernel_object> opencl_base::add_kernel_file(const string& identifier, const string& file_name, const string& func_name, const string additional_options) {
	kernels_lock.lock();
	const auto existing_kernel = kernels.find(identifier);
	if(existing_kernel != kernels.end()) {
		oclr_error("kernel \"%s\" already exists!", identifier);
		auto ret_kernel = existing_kernel->second;
		kernels_lock.unlock();
		return ret_kernel;
	}
	kernels_lock.unlock();
	
	//
	string kernel_data;
	if(!file_io::file_to_string(file_name, kernel_data)) {
		return null_kernel_object;
	}
	
	// work around caching bug and modify source on each load, TODO: check if this still exists (still present in 10.8.3 ...)
	if(platform_vendor == PLATFORM_VENDOR::APPLE) {
		kernel_data.insert(0, "#define __" + core::str_to_upper(func_name) +  "_BUILD_TIME__ " + uint2string((unsigned int)time(nullptr)) + "\n");
	}
	
	return add_kernel_src(identifier, kernel_data, func_name, additional_options);
}

void opencl_base::check_compilation(const bool ret, const string& filename) {
	if(!ret) {
		oclr_error("internal kernel \"%s\" didn't compile successfully!", filename.c_str());
		successful_internal_compilation = false;
	}
}

void opencl_base::reload_kernels() {
	kernels_lock.lock();
	destroy_kernels();
	
	successful_internal_compilation = true;
	
	for(const auto& int_kernel : internal_kernels) {
		check_compilation(add_kernel_file(get<0>(int_kernel),
										  make_kernel_path(get<1>(int_kernel)),
										  get<2>(int_kernel),
										  get<3>(int_kernel)).use_count() > 0,
						  get<1>(int_kernel));
	}
	kernels_lock.unlock();
	
	if(successful_internal_compilation) oclr_debug("internal kernels loaded successfully!");
	else {
		// one or more kernels didn't compile
		oclr_error("there were problems loading/compiling the internal kernels!");
	}
	
	// emit kernel reload event
	oclraster::get_event()->add_event(EVENT_TYPE::KERNEL_RELOAD, make_shared<kernel_reload_event>(SDL_GetTicks()));
}

void opencl_base::load_internal_kernels() {
	reload_kernels();
	
	if(is_gpu_support()) set_active_device(DEVICE_TYPE::FASTEST_GPU);
	else if(is_cpu_support()) set_active_device(DEVICE_TYPE::FASTEST_CPU);
}

void opencl_base::use_kernel(const string& identifier) {
	lock_guard<recursive_mutex> lock(kernels_lock);
	const auto kernel_iter = kernels.find(identifier);
	if(kernel_iter == kernels.end()) {
		oclr_error("kernel \"%s\" doesn't exist!", identifier.c_str());
		cur_kernel = nullptr;
		return;
	}
	cur_kernel = kernel_iter->second;
}

void opencl_base::use_kernel(weak_ptr<opencl_base::kernel_object> kernel_obj) {
	cur_kernel = kernel_obj.lock();
}

void opencl_base::run_kernel() {
	run_kernel(cur_kernel);
}

void opencl_base::run_kernel(const string& identifier) {
	kernels_lock.lock();
	const auto iter = kernels.find(identifier);
	if(iter != kernels.end()) {
		auto kernel_ptr = iter->second;
		kernels_lock.unlock();
		run_kernel(kernel_ptr);
	}
	kernels_lock.unlock();
	oclr_error("kernel \"%s\" doesn't exist!", identifier);
}

void opencl_base::delete_kernel(const string& identifier) {
	kernels_lock.lock();
	const auto iter = kernels.find(identifier);
	if(iter != kernels.end()) {
		auto kernel_ptr = iter->second;
		kernels_lock.unlock();
		delete_kernel(kernel_ptr);
	}
	kernels_lock.unlock();
	oclr_error("kernel \"%s\" doesn't exist!", identifier);
}

opencl_base::device_object* opencl_base::get_device(const opencl_base::DEVICE_TYPE& device) {
	if(device == DEVICE_TYPE::FASTEST_GPU) return fastest_gpu;
	else if(device == DEVICE_TYPE::FASTEST_CPU) return fastest_cpu;
	else {
		if((device >= DEVICE_TYPE::GPU0 && device <= DEVICE_TYPE::GPU255) ||
		   (device >= DEVICE_TYPE::CPU0 && device <= DEVICE_TYPE::CPU255)) {
			for(const auto& dev : devices) {
				if(dev->type == device) {
					return dev;
				}
			}
		}
	}
	return nullptr;
}

opencl_base::device_object* opencl_base::get_active_device() {
	return active_device;
}

const vector<opencl_base::device_object*>& opencl_base::get_devices() const {
	return devices;
}

bool opencl_base::has_vendor_device(opencl_base::VENDOR vendor_type) {
	for(const auto& device : devices) {
		if(device->vendor_type == vendor_type) return true;
	}
	return false;
}

void opencl_base::set_kernel_range(const pair<cl::NDRange, cl::NDRange> range) {
	if(cur_kernel == nullptr) return;
	cur_kernel->global = range.first;
	cur_kernel->local = range.second;
}

static size_t next_divisible_number(const size_t& num, const size_t& div) {
	if((num % div) == 0) return num;
	return (num / div) * div + div;
}

pair<cl::NDRange, cl::NDRange> opencl_base::compute_kernel_ranges(const size_t& work_items) const {
	if(cur_kernel == nullptr || active_device == nullptr) {
		return { cl::NDRange(work_items), cl::NDRange(1) };
	}
	
	// NOTE: local range will use the kernels max local work group size
	// and the global range will be made divisible by this local range
	// -> #actual work items >= work_items
	const size_t wg_size = get_kernel_work_group_size();
	const size_t local_range = (wg_size > active_device->max_wi_sizes.x ?
								active_device->max_wi_sizes.x : wg_size);
	const size_t global_range = next_divisible_number(work_items, local_range);
	/*oclr_msg("%s (%v -> %v; %v) overlap: %f%%",
			 cur_kernel->name, work_items, global_range, local_range,
			 ((float(global_range) / float(work_items)) - 1.0f) * 100.0f);*/
	return { cl::NDRange(global_range), cl::NDRange(local_range) };
}

pair<cl::NDRange, cl::NDRange> opencl_base::compute_kernel_ranges(const size_t& work_items_x, const size_t& work_items_y) const {
	if(cur_kernel == nullptr || active_device == nullptr) {
		return { cl::NDRange(work_items_x, work_items_y), cl::NDRange(1, 1) };
	}
	
	// NOTE: TODO
	const size_t wg_size = get_kernel_work_group_size();
	const size_t max_wg_size = (wg_size > active_device->max_wi_sizes.x ?
								active_device->max_wi_sizes.x : wg_size);
	// try to make this as even as possible and divisible by 2
	size_t local_x_size = max_wg_size;
	size_t local_y_size = 1;
	for(; local_x_size > 1;) {
		if(((local_x_size >> 1) * (local_y_size << 1)) != max_wg_size ||
		   (local_y_size << 1) > active_device->max_wi_sizes.y ||
		   local_x_size == local_y_size) {
			break;
		}
		local_x_size >>= 1;
		local_y_size <<= 1;
	}
	const size2 global_range {
		next_divisible_number(work_items_x, local_x_size),
		next_divisible_number(work_items_y, local_y_size)
	};
	/*oclr_msg("%s (%v -> %v; %v) overlap: %f%% %f%%, #%f%%",
			 cur_kernel->name, size2(work_items_x, work_items_y), global_range, size2(local_x_size, local_y_size),
			 ((float(global_range.x) / float(work_items_x)) - 1.0f) * 100.0f,
			 ((float(global_range.y) / float(work_items_y)) - 1.0f) * 100.0f,
			 ((float(global_range.x * global_range.y) / float(work_items_x * work_items_y)) - 1.0f) * 100.0f
			 );*/
	return { cl::NDRange(global_range.x, global_range.y), cl::NDRange(local_x_size, local_y_size) };
}

pair<cl::NDRange, cl::NDRange> opencl_base::compute_kernel_ranges(const size_t& work_items_x, const size_t& work_items_y, const size_t& work_items_z) const {
	if(cur_kernel == nullptr || active_device == nullptr) {
		return { cl::NDRange(work_items_x, work_items_y, work_items_z), cl::NDRange(1, 1, 1) };
	}
	
	// NOTE: same as 1D
	// TODO: write this
	const size_t wg_size = get_kernel_work_group_size();
	const size_t max_wg_size = (wg_size > active_device->max_wi_sizes.x ?
								active_device->max_wi_sizes.x : wg_size);
	const size_t global_range = next_divisible_number(work_items_x, max_wg_size);
	return { cl::NDRange(global_range, work_items_y, work_items_z), cl::NDRange(max_wg_size, 1, 1) };
}

void opencl_base::set_manual_gl_sharing(buffer_object* gl_buffer_obj, const bool state) {
	if((gl_buffer_obj->type & BUFFER_FLAG::OPENGL_BUFFER) == BUFFER_FLAG::NONE ||
	   gl_buffer_obj->ogl_buffer == 0) {
		oclr_error("this is not a gl object!");
		return;
	}
	
	gl_buffer_obj->manual_gl_sharing = state;
}

const vector<cl::ImageFormat>& opencl_base::get_image_formats() const {
	return img_formats;
}

cl::ImageFormat opencl_base::get_image_format(const IMAGE_TYPE& data_type, const IMAGE_CHANNEL channel_type) const {
	const auto data_idx = (typename underlying_type<IMAGE_TYPE>::type)data_type;
	const auto channel_idx = (typename underlying_type<IMAGE_CHANNEL>::type)channel_type;
#if defined(OCLRASTER_DEBUG)
	if(data_idx >= internal_image_format_mapping.size()) {
		oclr_error("invalid data_type: %u!", data_idx);
		return cl::ImageFormat(0, 0);
	}
	if(channel_idx >= internal_image_format_mapping[data_idx].size()) {
		oclr_error("invalid channel_type: %u!", channel_idx);
		return cl::ImageFormat(0, 0);
	}
#endif
	return internal_image_format_mapping[data_idx][channel_idx];
}

bool opencl_base::check_image_origin_and_size(const opencl_base::buffer_object* image_obj, cl::size_t<3>& origin, cl::size_t<3>& region) const {
	unsigned int image_dim = 0;
	switch(image_obj->image_type) {
		case buffer_object::IMAGE_TYPE::IMAGE_1D:
			origin[1] = 0;
			origin[2] = 0;
			region[1] = 1;
			region[2] = 1;
			image_dim = 1;
			break;
		case buffer_object::IMAGE_TYPE::IMAGE_2D:
			region[2] = 0;
			region[2] = 1;
			image_dim = 2;
			break;
		case buffer_object::IMAGE_TYPE::IMAGE_3D:
			image_dim = 3;
			break;
		case buffer_object::IMAGE_TYPE::IMAGE_NONE:
			oclr_error("this is not an image object!");
			return false;
	}
	
	for(unsigned int dim = 0; dim < image_dim; dim++) {
		const char* dim_str = (dim == 0 ? "x" : (dim == 1 ? "y" : "z"));
		if(region[dim] == 0) {
			region[dim] = image_obj->image_size[dim];
		}
		if(origin[dim] >= image_obj->image_size[dim]) {
			oclr_error("image %s-origin (%u) out of bound!",
					   dim_str, origin[dim]);
			return false;
		}
		if(region[dim] > image_obj->image_size[dim]) {
			oclr_error("image %s-region (%u) out of bound!",
					   dim_str, region[dim]);
			return false;
		}
		if((origin[dim]+region[dim]) > image_obj->image_size[dim]) {
			oclr_error("combined image %s-region (%u) and image %s-origin (%u) are out of bound!",
					   dim_str, dim_str, region[dim], origin[dim]);
			return false;
		}
	}
	return true;
}

void opencl_base::lock() {
	execution_lock.lock();
}

void opencl_base::unlock() {
	execution_lock.unlock();
}

bool opencl_base::try_lock() {
	return execution_lock.try_lock();
}

bool opencl_base::is_full_double_support() const {
	return full_double_support;
}

void opencl_base::dump_buffer(buffer_object* buffer_obj,
							  const string& filename) {
	flush();
	finish();
	
	auto data_ptr = map_buffer(buffer_obj);
	file_io dump_file(filename, file_io::OPEN_TYPE::WRITE_BINARY);
	
	auto data = new char[buffer_obj->size];
	copy_n((const char*)data_ptr, buffer_obj->size, data);
	dump_file.write_block(data, buffer_obj->size);
	
	dump_file.close();
	unmap_buffer(buffer_obj, data_ptr);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// general/actual opencl implementation below
#define __ERROR_CODE_INFO_CL_11(F) \
F(CL_SUCCESS) \
F(CL_DEVICE_NOT_FOUND) \
F(CL_DEVICE_NOT_AVAILABLE) \
F(CL_COMPILER_NOT_AVAILABLE) \
F(CL_MEM_OBJECT_ALLOCATION_FAILURE) \
F(CL_OUT_OF_RESOURCES) \
F(CL_OUT_OF_HOST_MEMORY) \
F(CL_PROFILING_INFO_NOT_AVAILABLE) \
F(CL_MEM_COPY_OVERLAP) \
F(CL_IMAGE_FORMAT_MISMATCH) \
F(CL_IMAGE_FORMAT_NOT_SUPPORTED) \
F(CL_BUILD_PROGRAM_FAILURE) \
F(CL_MAP_FAILURE) \
F(CL_INVALID_VALUE) \
F(CL_INVALID_DEVICE_TYPE) \
F(CL_INVALID_PLATFORM) \
F(CL_INVALID_DEVICE) \
F(CL_INVALID_CONTEXT) \
F(CL_INVALID_QUEUE_PROPERTIES) \
F(CL_INVALID_COMMAND_QUEUE) \
F(CL_INVALID_HOST_PTR) \
F(CL_INVALID_MEM_OBJECT) \
F(CL_INVALID_IMAGE_FORMAT_DESCRIPTOR) \
F(CL_INVALID_IMAGE_SIZE) \
F(CL_INVALID_SAMPLER) \
F(CL_INVALID_BINARY) \
F(CL_INVALID_BUILD_OPTIONS) \
F(CL_INVALID_PROGRAM) \
F(CL_INVALID_PROGRAM_EXECUTABLE) \
F(CL_INVALID_KERNEL_NAME) \
F(CL_INVALID_KERNEL_DEFINITION) \
F(CL_INVALID_KERNEL) \
F(CL_INVALID_ARG_INDEX) \
F(CL_INVALID_ARG_VALUE) \
F(CL_INVALID_ARG_SIZE) \
F(CL_INVALID_KERNEL_ARGS) \
F(CL_INVALID_WORK_DIMENSION) \
F(CL_INVALID_WORK_GROUP_SIZE) \
F(CL_INVALID_WORK_ITEM_SIZE) \
F(CL_INVALID_GLOBAL_OFFSET) \
F(CL_INVALID_EVENT_WAIT_LIST) \
F(CL_INVALID_EVENT) \
F(CL_INVALID_OPERATION) \
F(CL_INVALID_GL_OBJECT) \
F(CL_INVALID_BUFFER_SIZE) \
F(CL_INVALID_MIP_LEVEL)

#if defined(CL_VERSION_1_2)
#define __ERROR_CODE_INFO_CL_12(F) \
F(CL_MISALIGNED_SUB_BUFFER_OFFSET) \
F(CL_EXEC_STATUS_ERROR_FOR_EVENTS_IN_WAIT_LIST) \
F(CL_COMPILE_PROGRAM_FAILURE) \
F(CL_LINKER_NOT_AVAILABLE) \
F(CL_LINK_PROGRAM_FAILURE) \
F(CL_DEVICE_PARTITION_FAILED) \
F(CL_KERNEL_ARG_INFO_NOT_AVAILABLE) \
F(CL_INVALID_GLOBAL_WORK_SIZE) \
F(CL_INVALID_PROPERTY) \
F(CL_INVALID_IMAGE_DESCRIPTOR) \
F(CL_INVALID_COMPILER_OPTIONS) \
F(CL_INVALID_LINKER_OPTIONS) \
F(CL_INVALID_DEVICE_PARTITION_COUNT)
#define __ERROR_CODE_INFO(F) __ERROR_CODE_INFO_CL_11(F) __ERROR_CODE_INFO_CL_12(F)
#else
#define __ERROR_CODE_INFO(F) __ERROR_CODE_INFO_CL_11(F)
#endif

#define __DECLARE_ERROR_CODE_TO_STRING(code) case code: return #code;

string opencl::error_code_to_string(cl_int error_code) const {
	switch(error_code) {
		__ERROR_CODE_INFO(__DECLARE_ERROR_CODE_TO_STRING);
		default:
			return "UNKNOWN CL ERROR";
	}
}

#define __HANDLE_CL_EXCEPTION_START(func_str) __HANDLE_CL_EXCEPTION_START_EXT(func_str, "")
#define __HANDLE_CL_EXCEPTION_START_EXT(func_str, additional_info)									\
catch(cl::Error err) {																				\
	oclr_error("line #%s, " func_str "(): %s (%d: %s)%s!", \
			  __LINE__, err.what(), err.err(), error_code_to_string(err.err()), additional_info);
#define __HANDLE_CL_EXCEPTION_END }
#define __HANDLE_CL_EXCEPTION(func_str) __HANDLE_CL_EXCEPTION_START(func_str) __HANDLE_CL_EXCEPTION_END
#define __HANDLE_CL_EXCEPTION_EXT(func_str, additional_info) __HANDLE_CL_EXCEPTION_START_EXT(func_str, additional_info) __HANDLE_CL_EXCEPTION_END

/*! creates a opencl object
 */
opencl::opencl(const char* kernel_path, SDL_Window* wnd, const bool clear_cache) : opencl_base() {
	opencl::sdl_wnd = wnd;
	opencl::kernel_path_str = kernel_path;
	
	context = nullptr;
	cur_kernel = nullptr;
	active_device = nullptr;
	
	fastest_cpu = nullptr;
	fastest_gpu = nullptr;
	
	// TODO: this currently doesn't work if there are spaces inside the path and surrounding
	// the path by "" doesn't work either, probably a bug in the apple implementation -- or clang?
	build_options = "-I" + kernel_path_str.substr(0, kernel_path_str.length()-1);
	build_options += " -cl-mad-enable";
	build_options += " -cl-no-signed-zeros";
	build_options += " -cl-fast-relaxed-math";
	build_options += " -cl-single-precision-constant";
	build_options += " -cl-denorms-are-zero";
	
#if !defined(OCLRASTER_DEBUG)
	build_options += " -w";
#endif
	
#if !defined(__APPLE__)
	//nv_build_options = " -cl-nv-verbose";
	//nv_build_options = " -check-kernel-functions";
	//nv_build_options += " -nvptx-mad-enable -inline-all";
#else
	build_options += " -cl-auto-vectorize-enable";
	
	// add defines for the compile-time and run-time os x versions
	build_options += " -DOS_X_VERSION_COMPILED=" + size_t2string(osx_helper::get_compiled_system_version());
	build_options += " -DOS_X_VERSION=" + size_t2string(osx_helper::get_system_version());
#endif
	
	// clear opencl cache
	if(clear_cache) {
#if defined(__APPLE__)
		// TODO: delete app specific cache (~/Library/Caches/$identifier/com.apple.opencl)
#elif defined(__WINDOWS__)
		// TODO: find it (/Users/$user/AppData/Roaming/NVIDIA/ComputeCache)
#else
		system("rm -R ~/.nv/ComputeCache > /dev/null 2>&1");
#endif
	}
}

/*! opencl destructor
 */
opencl::~opencl() {
	oclr_debug("deleting opencl object");
	
	for(const auto& buf : buffers) {
		delete buf->buffer;
	}
	buffers.clear();
	
	destroy_kernels();
	
	for(const auto& cl_device : devices) {
		delete cl_device;
	}
	devices.clear();
	
	if(context != nullptr) delete context;
	
	oclr_debug("opencl object deleted");
}

void opencl::init(bool use_platform_devices, const size_t platform_index,
				  const set<string> device_restriction, const bool gl_sharing) {
	try {
		platform = new cl::Platform();
		platform->get(&platforms);
		
		vector<cl::Device> internal_devices;
		if(platforms.size() > platform_index) {
			platforms[platform_index].getDevices(CL_DEVICE_TYPE_ALL, &internal_devices);
		}
		else {
			oclr_error("no opencl platform available!");
			return;
		}
		oclr_debug("%u opencl platform%s found!", platforms.size(), (platforms.size() > 1 ? "s" : ""));
		if(use_platform_devices) {
			oclr_debug("%u opencl device%s found!", internal_devices.size(), (internal_devices.size() > 1 ? "s" : ""));
		}
		
#if defined(__APPLE__)
		platform_vendor = PLATFORM_VENDOR::APPLE;
		
		// if gl sharing is enabled, but a device restriction is specified that doesn't contain "GPU",
		// an opengl sharegroup (gl sharing) may not be used, since this would add gpu devices to the context
		bool apple_gl_sharing = gl_sharing;
		if(!device_restriction.empty() && device_restriction.count("GPU") == 0) {
			oclr_error("opencl device restriction set to disallow GPUs, but gl sharing is enabled - disabling gl sharing!");
			apple_gl_sharing = false;
		}
		
		cl_context_properties cl_properties[] {
			CL_CONTEXT_PLATFORM, (cl_context_properties)platforms[platform_index](),
			apple_gl_sharing ? CL_CONTEXT_PROPERTY_USE_CGL_SHAREGROUP_APPLE : 0,
#if !defined(OCLRASTER_IOS)
			apple_gl_sharing ? (cl_context_properties)CGLGetShareGroup(CGLGetCurrentContext()) : 0,
#else
			apple_gl_sharing ? (cl_context_properties)ios_helper::get_eagl_sharegroup() : 0,
#endif
			0
		};
		
		// from cl_gl_ext.h:
		// "If the <num_devices> and <devices> argument values to clCreateContext are 0 and NULL respectively,
		// all CL compliant devices in the CGL share group will be used to create the context.
		// Additional CL devices can also be specified using the <num_devices> and <devices> arguments.
		// These, however, cannot be GPU devices. On Mac OS X, you can add the CPU to the list of CL devices
		// (in addition to the CL compliant devices in the CGL share group) used to create the CL context.
		// Note that if a CPU device is specified, the CGL share group must also include the GL float renderer;
		// Otherwise CL_INVALID_DEVICE will be returned."
		// -> create a vector of all cpu devices and create the context
		vector<cl::Device> cl_devices;
		if(apple_gl_sharing) {
			if(device_restriction.empty() || device_restriction.count("CPU") > 0) {
				for(const auto& device : internal_devices) {
					if(device.getInfo<CL_DEVICE_TYPE>() == CL_DEVICE_TYPE_CPU) {
						cl_devices.emplace_back(device);
					}
				}
			}
		}
		else {
			cl_devices = internal_devices;
		}
		context = new cl::Context(cl_devices, cl_properties, clLogMessagesToStdoutAPPLE, nullptr, &ierr);
		
#else
		// context with gl share group (cl/gl interop)
#if defined(__WINDOWS__)
		cl_context_properties cl_properties[] {
			CL_CONTEXT_PLATFORM, (cl_context_properties)platforms[platform_index](),
			gl_sharing ? CL_GL_CONTEXT_KHR : 0,
			gl_sharing ? (cl_context_properties)wglGetCurrentContext() : 0,
			gl_sharing ? CL_WGL_HDC_KHR : 0,
			gl_sharing ? (cl_context_properties)wglGetCurrentDC() : 0,
			0
		};
#else // Linux, hopefully *BSD too
		SDL_SysWMinfo wm_info;
		SDL_VERSION(&wm_info.version);
		if(SDL_GetWindowWMInfo(sdl_wnd, &wm_info) != 1) {
			oclr_error("couldn't get window manger info!");
			return;
		}
		
		cl_context_properties cl_properties[] {
			CL_CONTEXT_PLATFORM, (cl_context_properties)platforms[platform_index](),
			gl_sharing ? CL_GL_CONTEXT_KHR : 0,
			gl_sharing ? (cl_context_properties)glXGetCurrentContext() : 0,
			gl_sharing ? CL_GLX_DISPLAY_KHR : 0,
			gl_sharing ? (cl_context_properties)wm_info.info.x11.display : 0,
			0
		};
#endif
		
		if(use_platform_devices) {
			context = new cl::Context(internal_devices, cl_properties, nullptr, nullptr, &ierr);
		}
		else {
			context = new cl::Context(CL_DEVICE_TYPE_ALL, cl_properties, nullptr, nullptr, &ierr);
		}
#endif

#if !defined(__APPLE__)
		// get platform vendor
		const string platform_str = platforms[platform_index].getInfo<CL_PLATFORM_NAME>();
		const string platform_vendor_str = core::str_to_lower(platform_str);
		if(platform_vendor_str.find("nvidia") != string::npos) {
			platform_vendor = PLATFORM_VENDOR::NVIDIA;
		}
		else if(platform_vendor_str.find("amd") != string::npos) {
			platform_vendor = PLATFORM_VENDOR::AMD;
		}
		else if(platform_vendor_str.find("intel") != string::npos) {
			platform_vendor = PLATFORM_VENDOR::INTEL;
		}
		else if(platform_vendor_str.find("freeocl") != string::npos) {
			platform_vendor = PLATFORM_VENDOR::FREEOCL;
		}
#endif
		
		//
		const auto extract_cl_version = [](const string& cl_version_str, const string str_start) -> pair<bool, CL_VERSION> {
			// "OpenCL X.Y" or "OpenCL C X.Y" required by spec (str_start must be either)
			const size_t start_len = str_start.length();
			if(cl_version_str.length() >= (start_len + 3) && cl_version_str.substr(0, start_len) == str_start) {
				const string version_str = cl_version_str.substr(start_len, cl_version_str.find(" ", start_len) - start_len);
				const size_t dot_pos = version_str.find(".");
				if(string2size_t(version_str.substr(0, dot_pos)) > 1) {
					// major version is higher than 1 -> pretend we're running on CL 2.0
					return { true, CL_VERSION::CL_2_0 };
				}
				else {
					switch(string2size_t(version_str.substr(dot_pos+1, version_str.length()-dot_pos-1))) {
						case 0: return { true, CL_VERSION::CL_1_0 };
						case 1: return { true, CL_VERSION::CL_1_1 };
						case 2:
						default: // default to CL 1.2
							return { true, CL_VERSION::CL_1_2 };
					}
				}
			}
			return { false, CL_VERSION::CL_1_0 };
		};

		// get platform cl version
		const string cl_version_str = platforms[platform_index].getInfo<CL_PLATFORM_VERSION>();
		const auto extracted_cl_version = extract_cl_version(cl_version_str, "OpenCL "); // "OpenCL X.Y" required by spec
		if(!extracted_cl_version.first) {
			oclr_error("invalid opencl platform version string: %s", cl_version_str);
		}
		platform_cl_version = extracted_cl_version.second;
		
		// pocl only identifies itself in the platform version string, not the vendor string
		if(cl_version_str.find("pocl") != string::npos) {
			platform_vendor = PLATFORM_VENDOR::POCL;
		}

		//
		oclr_debug("opencl platform #%u vendor: %s (version CL%s)",
				   platform_index, platform_vendor_to_str(platform_vendor),
				   (platform_cl_version == CL_VERSION::CL_1_0 ? "1.0" :
					(platform_cl_version == CL_VERSION::CL_1_1 ? "1.1" :
					 (platform_cl_version == CL_VERSION::CL_1_2 ? "1.2" : "2.0"))));
		
		internal_devices.clear();
		internal_devices = context->getInfo<CL_CONTEXT_DEVICES>();
		oclr_debug("%u opencl device%s found!", internal_devices.size(), (internal_devices.size() > 1 ? "s" : ""));
		
		oclr_debug("opencl context successfully created!");
		
		string dev_type_str;
		unsigned int gpu_counter = (unsigned int)DEVICE_TYPE::GPU0;
		unsigned int cpu_counter = (unsigned int)DEVICE_TYPE::CPU0;
		unsigned int fastest_cpu_score = 0;
		unsigned int fastest_gpu_score = 0;
		unsigned int cpu_score = 0;
		unsigned int gpu_score = 0;
		for(const auto& internal_device : internal_devices) {
			dev_type_str = "";
			
			// device restriction
			if(!device_restriction.empty()) {
				switch(internal_device.getInfo<CL_DEVICE_TYPE>()) {
					case CL_DEVICE_TYPE_CPU:
						if(device_restriction.count("CPU") == 0) continue;
						break;
					case CL_DEVICE_TYPE_GPU:
						if(device_restriction.count("GPU") == 0) continue;
						break;
					case CL_DEVICE_TYPE_ACCELERATOR:
						if(device_restriction.count("ACCELERATOR") == 0) continue;
						break;
					default: break;
				}
			}
			
			opencl::device_object* device = new opencl::device_object();
			device->device = internal_device;
			device->internal_type = internal_device.getInfo<CL_DEVICE_TYPE>();
			device->units = internal_device.getInfo<CL_DEVICE_MAX_COMPUTE_UNITS>();
			device->clock = internal_device.getInfo<CL_DEVICE_MAX_CLOCK_FREQUENCY>();
			device->mem_size = internal_device.getInfo<CL_DEVICE_GLOBAL_MEM_SIZE>();
			device->local_mem_size = internal_device.getInfo<CL_DEVICE_LOCAL_MEM_SIZE>();
			device->constant_mem_size = internal_device.getInfo<CL_DEVICE_MAX_CONSTANT_BUFFER_SIZE>();
			device->name = internal_device.getInfo<CL_DEVICE_NAME>();
			device->vendor = internal_device.getInfo<CL_DEVICE_VENDOR>();
			device->version = internal_device.getInfo<CL_DEVICE_VERSION>();
			device->driver_version = internal_device.getInfo<CL_DRIVER_VERSION>();
			device->extensions = internal_device.getInfo<CL_DEVICE_EXTENSIONS>();
			
			device->max_alloc = internal_device.getInfo<CL_DEVICE_MAX_MEM_ALLOC_SIZE>();
			device->max_wg_size = internal_device.getInfo<CL_DEVICE_MAX_WORK_GROUP_SIZE>();
			const auto max_wi_sizes = internal_device.getInfo<CL_DEVICE_MAX_WORK_ITEM_SIZES>();
			device->max_wi_sizes.set(max_wi_sizes[0], max_wi_sizes[1], max_wi_sizes[2]);
			device->img_support = (internal_device.getInfo<CL_DEVICE_IMAGE_SUPPORT>() == 1);
			device->max_img_2d.set(internal_device.getInfo<CL_DEVICE_IMAGE2D_MAX_WIDTH>(),
								   internal_device.getInfo<CL_DEVICE_IMAGE2D_MAX_HEIGHT>());
			device->max_img_3d.set(internal_device.getInfo<CL_DEVICE_IMAGE3D_MAX_WIDTH>(),
								   internal_device.getInfo<CL_DEVICE_IMAGE3D_MAX_HEIGHT>(),
								   internal_device.getInfo<CL_DEVICE_IMAGE3D_MAX_DEPTH>());
			device->double_support = (internal_device.getInfo<CL_DEVICE_DOUBLE_FP_CONFIG>() != 0);
			
			oclr_msg("address space size: %u", internal_device.getInfo<CL_DEVICE_ADDRESS_BITS>());
			oclr_msg("max mem alloc: %u bytes / %u MB",
					 device->max_alloc,
					 device->max_alloc / 1024ULL / 1024ULL);
			oclr_msg("mem size: %u MB (global), %u KB (local), %u KB (constant)",
					 device->mem_size / 1024ULL / 1024ULL,
					 device->local_mem_size / 1024ULL,
					 device->constant_mem_size / 1024ULL);
			oclr_msg("mem base address alignment: %u", internal_device.getInfo<CL_DEVICE_MEM_BASE_ADDR_ALIGN>());
			oclr_msg("min data type alignment size: %u", internal_device.getInfo<CL_DEVICE_MIN_DATA_TYPE_ALIGN_SIZE>());
			oclr_msg("host unified memory: %u", internal_device.getInfo<CL_DEVICE_HOST_UNIFIED_MEMORY>());
			oclr_msg("max_wi_sizes: %v", device->max_wi_sizes);
			oclr_msg("max_wg_size: %u", device->max_wg_size);
			oclr_msg("max param size: %u", internal_device.getInfo<CL_DEVICE_MAX_PARAMETER_SIZE>());
			oclr_msg("double support: %b", device->double_support);
			oclr_msg("image support: %b", device->img_support);
#if defined(CL_VERSION_1_2)
			if(platform_cl_version >= CL_VERSION::CL_1_2 &&
			   // pocl has no support for this yet
			   platform_vendor != PLATFORM_VENDOR::POCL) {
				const unsigned long long int printf_buffer_size = internal_device.getInfo<CL_DEVICE_PRINTF_BUFFER_SIZE>();
				oclr_msg("printf buffer size: %u bytes / %u MB",
						 printf_buffer_size,
						 printf_buffer_size / 1024ULL / 1024ULL);
				oclr_msg("max sub-devices: %u", internal_device.getInfo<CL_DEVICE_PARTITION_MAX_SUB_DEVICES>());
				if(platform_vendor != PLATFORM_VENDOR::FREEOCL) {
					// this is broken on freeocl
					oclr_msg("built-in kernels: %s", internal_device.getInfo<CL_DEVICE_BUILT_IN_KERNELS>());
				}
			}
#endif

			device->vendor_type = VENDOR::UNKNOWN;
			string vendor_str = core::str_to_lower(device->vendor);
			if(strstr(vendor_str.c_str(), "nvidia") != nullptr) {
				device->vendor_type = VENDOR::NVIDIA;
			}
			else if(strstr(vendor_str.c_str(), "intel") != nullptr) {
				device->vendor_type = VENDOR::INTEL;
			}
			else if(strstr(vendor_str.c_str(), "apple") != nullptr) {
				device->vendor_type = VENDOR::APPLE;
			}
			else if(strstr(vendor_str.c_str(), "amd") != nullptr ||
					// "ati" should be tested last, since it also matches "corporation"
					strstr(vendor_str.c_str(), "ati") != nullptr) {
				device->vendor_type = VENDOR::AMD;
			}
			
			// freeocl and pocl use an empty device name, but "FreeOCL"/"pocl" is contained in the device version
			if(device->version.find("FreeOCL") != string::npos) {
				device->vendor_type = VENDOR::FREEOCL;
			}
			if(device->version.find("pocl") != string::npos) {
				device->vendor_type = VENDOR::POCL;
				
				// device unit count on pocl is 0 -> figure out how many logical cpus actually exist
				if(device->units == 0) {
#if defined(__FreeBSD__)
					int core_count = 0;
					size_t size = sizeof(core_count);
					sysctlbyname("hw.ncpu", &core_count, &size, nullptr, 0);
					device->units = core_count;
#else
					// TODO: other platforms?
					device->units = 1;
#endif
				}
			}
			
			if(device->internal_type & CL_DEVICE_TYPE_CPU) {
				device->type = (opencl::DEVICE_TYPE)cpu_counter;
				cpu_counter++;
				dev_type_str += "CPU ";
				
				if(fastest_cpu == nullptr) {
					fastest_cpu = device;
					fastest_cpu_score = device->units * device->clock;
				}
				else {
					cpu_score = device->units * device->clock;
					if(cpu_score > fastest_cpu_score) {
						fastest_cpu = device;
						fastest_cpu_score = cpu_score;
					}
				}
			}
			if(device->internal_type & CL_DEVICE_TYPE_GPU) {
				device->type = (opencl::DEVICE_TYPE)gpu_counter;
				gpu_counter++;
				dev_type_str += "GPU ";
				const auto compute_gpu_score = [](const device_object& dev) -> unsigned int {
					unsigned int multiplier = 1;
					switch(dev.vendor_type) {
						case VENDOR::NVIDIA:
							// fermi or kepler+ card if wg size is >= 1024
							multiplier = (dev.max_wg_size >= 1024 ? 32 : 8);
							break;
						case VENDOR::AMD:
							multiplier = 16;
							break;
						// none for INTEL
						default: break;
					}
					return multiplier * (dev.units * dev.clock);
				};
				
				if(fastest_gpu == nullptr) {
					fastest_gpu = device;
					fastest_gpu_score = compute_gpu_score(*device);
				}
				else {
					gpu_score = compute_gpu_score(*device);
					if(gpu_score > fastest_gpu_score) {
						fastest_gpu = device;
						fastest_gpu_score = gpu_score;
					}
				}
			}
			if(device->internal_type & CL_DEVICE_TYPE_ACCELERATOR) {
				dev_type_str += "Accelerator ";
			}
			if(device->internal_type & CL_DEVICE_TYPE_DEFAULT) {
				dev_type_str += "Default ";
			}
			
			const string cl_c_version_str = internal_device.getInfo<CL_DEVICE_OPENCL_C_VERSION>();
			const auto extracted_cl_c_version = extract_cl_version(cl_c_version_str, "OpenCL C "); // "OpenCL C X.Y" required by spec
			if(!extracted_cl_c_version.first) {
				oclr_error("invalid opencl c version string: %s", cl_c_version_str);
			}
			device->cl_c_version = extracted_cl_c_version.second;
			
			// cl_khr_byte_addressable_store support is mandatory
			if(device->extensions.find("cl_khr_byte_addressable_store") == string::npos) {
				oclr_msg("opencl device \"%s %s\" does not support \"cl_khr_byte_addressable_store\"!", device->vendor, device->name);
				delete device;
				continue;
			}
			devices.push_back(device);
			
			// TYPE (Units: %, Clock: %): Name, Vendor, Version, Driver Version
			oclr_debug("%s(Units: %u, Clock: %u MHz, Memory: %u MB): %s %s, %s / %s / %s",
					   dev_type_str,
					   device->units,
					   device->clock,
					   (unsigned int)(device->mem_size / 1024ul / 1024ul),
					   device->vendor,
					   device->name,
					   device->version,
					   device->driver_version,
					   cl_c_version_str);
		}
		internal_devices.clear();
		
		// no supported devices found
		if(devices.empty()) {
			throw oclraster_exception("no supported device found for this platform!");
		}
		
		// check if all devices support doubles
		full_double_support = true;
		for(const auto& device : devices) {
			if(!device->double_support) {
				full_double_support = false;
				break;
			}
		}
		
		// create a (single) command queue for each device
		for(const auto& device : devices) {
			queues[&device->device] = new cl::CommandQueue(*context, device->device,
#if !defined(OCLRASTER_PROFILING)
														   0,
#else
														   CL_QUEUE_PROFILING_ENABLE,
#endif
														   &ierr);
		}
		
		if(fastest_cpu != nullptr) oclr_debug("fastest CPU device: %s %s (score: %u)", fastest_cpu->vendor.c_str(), fastest_cpu->name.c_str(), fastest_cpu_score);
		if(fastest_gpu != nullptr) oclr_debug("fastest GPU device: %s %s (score: %u)", fastest_gpu->vendor.c_str(), fastest_gpu->name.c_str(), fastest_gpu_score);
		
		// compile internal kernels
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
	__HANDLE_CL_EXCEPTION_START("init")
		// try another time w/o using the platform devices
		if(platform_index+1 < platforms.size()) {
			oclr_debug("trying next platform ...");
			init(use_platform_devices, platform_index+1);
		}
	__HANDLE_CL_EXCEPTION_END
	catch(oclraster_exception& e) {
		oclr_debug("%s", e.what());
		// try another time w/o using the platform devices
		if(platform_index+1 < platforms.size()) {
			oclr_debug("trying next platform ...");
			init(use_platform_devices, platform_index+1);
		}
	}
	
	// if absolutely no devices on any platform are supported, disable opencl support
	if(devices.empty()) {
		supported = false;
		return;
	}
	
	// context has been created, query image format information
	img_formats.clear();
	if(platform_vendor != PLATFORM_VENDOR::POCL) {
		context->getSupportedImageFormats(CL_MEM_READ_WRITE, CL_MEM_OBJECT_IMAGE2D, &img_formats);
		if(img_formats.empty()) {
			oclr_error("no supported image formats!");
		}
	}
	else {
		// pocl has too many issues and doesn't have full image support
		// -> disable it and don't get any "supported" image formats
		for(const auto& dev : devices) {
			dev->img_support = false;
		}
	}
	
	// un-#if-0 for debug output
#if 0
	const auto channel_order_to_string = [](const cl_channel_order& channel_order) -> string {
		switch(channel_order) {
			case CL_R: return "CL_R";
			case CL_A: return "CL_A";
			case CL_RG: return "CL_RG";
			case CL_RA: return "CL_RA";
			case CL_RGB: return "CL_RGB";
			case CL_RGBA: return "CL_RGBA";
			case CL_BGRA: return "CL_BGRA";
			case CL_ARGB: return "CL_ARGB";
			case CL_INTENSITY: return "CL_INTENSITY";
			case CL_LUMINANCE: return "CL_LUMINANCE";
			case CL_Rx: return "CL_Rx";
			case CL_RGx: return "CL_RGx";
			case CL_RGBx: return "CL_RGBx";
#if defined(CL_DEPTH)
			case CL_DEPTH: return "CL_DEPTH";
#endif
#if defined(CL_DEPTH_STENCIL)
			case CL_DEPTH_STENCIL: return "CL_DEPTH_STENCIL";
#endif
#if defined(CL_1RGB_APPLE)
			case CL_1RGB_APPLE: return "CL_1RGB_APPLE";
#endif
#if defined(CL_BGR1_APPLE)
			case CL_BGR1_APPLE: return "CL_BGR1_APPLE";
#endif
#if defined(CL_YCbYCr_APPLE)
			case CL_YCbYCr_APPLE: return "CL_YCbYCr_APPLE";
#endif
#if defined(CL_CbYCrY_APPLE)
			case CL_CbYCrY_APPLE: return "CL_CbYCrY_APPLE";
#endif
			default: break;
		}
		return uint2string(channel_order);
	};
	const auto channel_type_to_string = [](const cl_channel_type& channel_type) -> string {
		switch(channel_type) {
			case CL_SNORM_INT8: return "CL_SNORM_INT8";
			case CL_SNORM_INT16: return "CL_SNORM_INT16";
			case CL_UNORM_INT8: return "CL_UNORM_INT8";
			case CL_UNORM_INT16: return "CL_UNORM_INT16";
			case CL_UNORM_SHORT_565: return "CL_UNORM_SHORT_565";
			case CL_UNORM_SHORT_555: return "CL_UNORM_SHORT_555";
			case CL_UNORM_INT_101010: return "CL_UNORM_INT_101010";
			case CL_SIGNED_INT8: return "CL_SIGNED_INT8";
			case CL_SIGNED_INT16: return "CL_SIGNED_INT16";
			case CL_SIGNED_INT32: return "CL_SIGNED_INT32";
			case CL_UNSIGNED_INT8: return "CL_UNSIGNED_INT8";
			case CL_UNSIGNED_INT16: return "CL_UNSIGNED_INT16";
			case CL_UNSIGNED_INT32: return "CL_UNSIGNED_INT32";
			case CL_HALF_FLOAT: return "CL_HALF_FLOAT";
			case CL_FLOAT: return "CL_FLOAT";
#if defined(CL_SFIXED14_APPLE)
			case CL_SFIXED14_APPLE: return "CL_SFIXED14_APPLE";
#endif
#if defined(CL_BIASED_HALF_APPLE)
			case CL_BIASED_HALF_APPLE: return "CL_BIASED_HALF_APPLE";
#endif
			default: break;
		}
		return uint2string(channel_type);
	};
	
	for(const auto& format : img_formats) {
		oclr_log("\t%s %s", channel_order_to_string(format.image_channel_order), channel_type_to_string(format.image_channel_data_type));
	}
#endif
	
	// create image type/channel mappings
	static const array<vector<cl_channel_type>, (size_t)IMAGE_TYPE::__MAX_TYPE> type_mapping {
		{
			{}, // NONE
			{ CL_SNORM_INT8, CL_SIGNED_INT8 }, // INT_8
			{ CL_SNORM_INT16, CL_SIGNED_INT16 }, // INT_16
			{ CL_SIGNED_INT32 }, // INT_32
			{  }, // INT_64 (not supported by opencl)
			{ CL_UNORM_INT8, CL_UNSIGNED_INT8 }, // UINT_8
			{ CL_UNORM_INT16, CL_UNSIGNED_INT16 }, // UINT_16
			{ CL_UNSIGNED_INT32 }, // UINT_32
			{  }, // UINT_64 (not supported by opencl)
			{ CL_HALF_FLOAT }, // FLOAT_16
			{ CL_FLOAT }, // FLOAT_32
			{  } // FLOAT_64 (not supported by opencl)
		}
	};
	static const array<vector<cl_channel_order>, (size_t)IMAGE_CHANNEL::__MAX_CHANNEL> channel_mapping {
		{
			{}, // NONE
			{ CL_R, CL_Rx, CL_INTENSITY, CL_LUMINANCE }, // R
			{ CL_RG, CL_RGx }, // RG
			{ CL_RGB, CL_RGBx }, // RGB
			{ CL_RGBA, CL_BGRA, CL_ARGB } // RGBA
		}
	};
	
	// fill internal_image_format_mapping with appropriate info/mappings
	for(size_t data_idx = 0; data_idx < type_mapping.size(); data_idx++) {
		if(type_mapping[data_idx].empty()) continue;
		for(size_t channel_idx = 0; channel_idx < channel_mapping.size(); channel_idx++) {
			if(channel_mapping[channel_idx].empty()) continue;
			bool found = false;
			for(const auto& req_data_type : type_mapping[data_idx]) {
				for(const auto& req_channel_type : channel_mapping[channel_idx]) {
					for(const auto& format : img_formats) {
						if(req_data_type == format.image_channel_data_type &&
						   req_channel_type == format.image_channel_order) {
							internal_image_format_mapping[data_idx][channel_idx] = cl::ImageFormat(req_channel_type, req_data_type);
							found = true;
#if 0
							oclr_log("native image support: %s -> %s %s",
									 image_type { (IMAGE_TYPE)data_idx, (IMAGE_CHANNEL)channel_idx }.to_string(),
									 channel_type_to_string(format.image_channel_data_type),
									 channel_order_to_string(format.image_channel_order));
#endif
							break;
						}
					}
					if(found) break;
				}
				if(found) break;
			}
		}
	}
	
#if defined(__APPLE__)
	if(fastest_gpu != nullptr) {
		// workaround: when a shared cpu/gpu context is used, apple falsely advertises CL_Rx, CL_RGx, CL_RGBx in combination with CL_FLOAT
		// as supported, when they're actually not -> use CL_R, CL_RG, CL_RGB instead, which do work, but are not officially listed
		auto& r_float_mapping = internal_image_format_mapping[(size_t)IMAGE_TYPE::FLOAT_32][(size_t)IMAGE_CHANNEL::R];
		auto& rg_float_mapping = internal_image_format_mapping[(size_t)IMAGE_TYPE::FLOAT_32][(size_t)IMAGE_CHANNEL::RG];
		auto& rgb_float_mapping = internal_image_format_mapping[(size_t)IMAGE_TYPE::FLOAT_32][(size_t)IMAGE_CHANNEL::RGB];
		if(r_float_mapping.image_channel_data_type != 0 && r_float_mapping.image_channel_order != 0) {
			r_float_mapping = cl::ImageFormat(CL_R, CL_FLOAT);
		}
		if(rg_float_mapping.image_channel_data_type != 0 && rg_float_mapping.image_channel_order != 0) {
			rg_float_mapping = cl::ImageFormat(CL_RG, CL_FLOAT);
		}
		if(rgb_float_mapping.image_channel_data_type != 0 && rgb_float_mapping.image_channel_order != 0) {
			rgb_float_mapping = cl::ImageFormat(CL_RGB, CL_FLOAT);
		}
	}
#endif
}

weak_ptr<opencl::kernel_object> opencl::add_kernel_src(const string& identifier, const string& src, const string& func_name, const string additional_options) {
	// lock kernels (for mt safety), check if a kernel with such an identifier
	// already exists (return it, if it does), otherwise create a new kernel
	kernels_lock.lock();
	
	const auto existing_kernel = kernels.find(identifier);
	if(existing_kernel != kernels.end()) {
		oclr_error("kernel \"%s\" already exists!", identifier);
		auto ret_kernel = existing_kernel->second;
		kernels_lock.unlock();
		return ret_kernel;
	}
	
	auto kernel_ptr = make_shared<opencl::kernel_object>();
	if(kernel_ptr == nullptr) {
		// complete failure
		oclr_error("could not create kernel_object for \"%s\"!", identifier);
		return null_kernel_object;
	}
	kernels.emplace(identifier, kernel_ptr);
	
	kernels_lock.unlock();
	
	//
	oclr_debug("compiling \"%s\" kernel!", identifier);
	string options = build_options;
	
	// just define this everywhere to make using image support
	// easier without having to specify this every time
	options += " -DOCLRASTER_IMAGE_HEADER_SIZE="+size_t2string(image::header_size());
	
	// the same goes for the general struct alignment
	options += " -DOCLRASTER_STRUCT_ALIGNMENT="+uint2string(OCLRASTER_STRUCT_ALIGNMENT);
	
	try {
		if(!additional_options.empty()) {
			options += (additional_options[0] != ' ' ? " " : "") + additional_options;
		}
		
#if !defined(__APPLE__)
		// workaround for the nvidia compiler which apparently defines __APPLE__
		options += " -DUNDEF__APPLE__";
#endif
#if defined(__WINDOWS__)
		options += " -D__WINDOWS__";
#endif
		
		// platform specific compile options
		switch(platform_vendor) {
			case PLATFORM_VENDOR::AMD:
				// use the "static c++" compiler
				options += " -x clc++";
				break;
			default:
				break;
		}
		
		// add kernel
		kernel_ptr->name = identifier;
		cl::Program::Sources source(1, make_pair(src.c_str(), src.length()));
		kernel_ptr->program = new cl::Program(*context, source);
		
		// compile for each device independently to add device-specific defines
		for(const auto& device : devices) {
			string device_options = "";
			switch(device->vendor_type) {
				case VENDOR::NVIDIA:
					device_options += nv_build_options;
					device_options += " -DNVIDIA";
					break;
				case VENDOR::INTEL:
					device_options += " -DINTEL";
					break;
				case VENDOR::AMD:
					device_options += " -DAMD";
					break;
				case VENDOR::APPLE:
					device_options += " -DAPPLE_ARM";
					break;
				case VENDOR::FREEOCL:
					device_options += " -DFREEOCL";
					break;
				case VENDOR::POCL:
					device_options += " -DPOCL";
					break;
				case VENDOR::UNKNOWN:
					device_options += " -DUNKNOWN_VENDOR";
					break;
			}
			if((device->internal_type & CL_DEVICE_TYPE_CPU) != 0) device_options += " -DCPU";
			if((device->internal_type & CL_DEVICE_TYPE_GPU) != 0) device_options += " -DGPU";
			if((device->internal_type & CL_DEVICE_TYPE_ACCELERATOR) != 0) device_options += " -DACCELERATOR";
			
			device_options += " -DPLATFORM_"+platform_vendor_to_str(platform_vendor);
			device_options += " -DLOCAL_MEM_SIZE="+ull2string(device->local_mem_size);
			if(device->double_support) device_options += " -DOCLRASTER_DOUBLE_SUPPORT";
			
			kernel_ptr->program->build({ device->device }, (options+device_options).c_str());
		}
		
		kernel_ptr->kernel = new cl::Kernel(*kernel_ptr->program, func_name.c_str(), &ierr);
		
		kernel_ptr->arg_count = kernel_ptr->kernel->getInfo<CL_KERNEL_NUM_ARGS>();
		kernel_ptr->args_passed.insert(kernel_ptr->args_passed.begin(), kernel_ptr->arg_count, false);
		kernel_ptr->buffer_args.insert(kernel_ptr->buffer_args.begin(), kernel_ptr->arg_count, nullptr);

		// print out build log
		/*for(const auto& internal_device : internal_devices) {
			char build_log[CLINFO_STR_SIZE];
			memset(build_log, 0, CLINFO_STR_SIZE);
			kernel_ptr->program->getBuildInfo(internal_device, CL_PROGRAM_BUILD_LOG, &build_log);
			oclr_debug("build log: %s", build_log);
		}*/
		
#if 0
		size_t device_num = 0;
		for(const auto& device : devices) {
			oclr_log("%s (dev #%u): work group size: %u", identifier, device_num,
					 kernel_ptr->kernel->getWorkGroupInfo<CL_KERNEL_WORK_GROUP_SIZE>(device->device));
			oclr_log("%s (dev #%u): kernel preferred wg size multiple: %u", identifier, device_num,
					 kernel_ptr->kernel->getWorkGroupInfo<CL_KERNEL_PREFERRED_WORK_GROUP_SIZE_MULTIPLE>(device->device));
			oclr_log("%s (dev #%u): kernel local memory: %u", identifier, device_num,
					 kernel_ptr->kernel->getWorkGroupInfo<CL_KERNEL_LOCAL_MEM_SIZE>(device->device));
			oclr_log("%s (dev #%u): kernel private memory: %u", identifier, device_num,
					 kernel_ptr->kernel->getWorkGroupInfo<CL_KERNEL_PRIVATE_MEM_SIZE>(device->device));
			device_num++;
		}
#endif
	}
	__HANDLE_CL_EXCEPTION_START("add_kernel")
		// print out build log and build options
		for(const auto& device : devices) {
			char build_log[CLINFO_STR_SIZE];
			memset(build_log, 0, CLINFO_STR_SIZE);
			kernel_ptr->program->getBuildInfo(device->device, CL_PROGRAM_BUILD_LOG, &build_log);
			oclr_error("build log (%s): %s", identifier, build_log);
			
			// print out current build options
			char buildoptions[CLINFO_STR_SIZE];
			memset(buildoptions, 0, CLINFO_STR_SIZE);
			kernel_ptr->program->getBuildInfo(device->device, CL_PROGRAM_BUILD_OPTIONS, &buildoptions);
			oclr_debug("build options: %s", buildoptions);
		}
		
		weak_ptr<opencl::kernel_object> delete_ptr = kernel_ptr;
		kernel_ptr = nullptr;
		delete_kernel(delete_ptr);
		
		//log_program_binary(kernel_ptr, options);
		return null_kernel_object;
	__HANDLE_CL_EXCEPTION_END
	if(oclraster::get_log_binaries()) {
		log_program_binary(kernel_ptr);
	}
	kernel_ptr->valid = true;
	return kernel_ptr;
}

void opencl::delete_kernel(weak_ptr<opencl::kernel_object> kernel_obj) {
	auto kernel_ptr = kernel_obj.lock();
	if(kernel_ptr == nullptr) {
		// already deleted
		return;
	}
	
	// note: when add_kernel_src fails and calls this function => cur_kernel != kernel_ptr
	if(cur_kernel == kernel_ptr) {
		// if the currently active kernel is being deleted, flush+finish the queue
		flush();
		finish();
		cur_kernel = nullptr;
	}
	
	// must iterate over all kernels -> lock
	lock_guard<recursive_mutex> lock(kernels_lock);
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

void opencl::log_program_binary(const shared_ptr<opencl::kernel_object> kernel) {
	if(kernel == nullptr) return;
	
	try {
		//
		size_t device_num = 0;
		vector<size_t> program_sizes = kernel->program->getInfo<CL_PROGRAM_BINARY_SIZES>();
		if(program_sizes.size() == 0) return;
		
		unsigned char** program_binaries = new unsigned char*[program_sizes.size()];
		for(size_t i = 0; i < program_sizes.size(); i++) {
			program_binaries[i] = new unsigned char[program_sizes[i]+1];
		}
		clGetProgramInfo((*kernel->program)(), CL_PROGRAM_BINARIES, program_sizes.size()*sizeof(unsigned char*), &program_binaries[0], nullptr);
		
		string kernel_name = kernel->kernel->getInfo<CL_KERNEL_FUNCTION_NAME>();
		for(const auto& device : devices) {
			if(program_sizes[device_num] > 0) {
				if(device->vendor_type != VENDOR::UNKNOWN) {
					string file_name = kernel_name + string("_") + size_t2string(device_num);
					if(device->vendor_type == VENDOR::NVIDIA) {
						file_name += ".cubin";
					}
					else if(device->vendor_type == VENDOR::INTEL || device->vendor_type == VENDOR::AMD) {
						file_name += ".bin";
					}
					else {
						file_name += ".bin";
					}
					
					fstream bin_file(file_name.c_str(), fstream::out | fstream::binary | fstream::trunc);
					if(!bin_file.is_open()) {
						oclr_error("couldn't save cl-binary file \"%s\"!", file_name.c_str());
						return;
					}
					
					bin_file.write((const char*)program_binaries[device_num], program_sizes[device_num]);
					bin_file.flush();
					bin_file.close();
					
#if defined(__APPLE__)
					// on os x 10.7+, the kernel binary is packed inside a binary plist
					// -> convert to text (xml)
					// -> extract binary base64 string and convert
					core::system("plutil -convert xml1 "+file_name);
					const string binary_xml { file_io::file_to_string(file_name) };
					const char xml_binary_start[] { "<key>clBinaryData</key>\n\t<data>" };
					const char xml_binary_end[] { "</data>" };
					const auto start_pos = binary_xml.find(xml_binary_start);
					const auto end_pos = binary_xml.find(xml_binary_end, start_pos);
					if(start_pos != string::npos && end_pos != string::npos) {
						const auto base64_binary_start = start_pos + sizeof(xml_binary_start);
						const string base64_binary { binary_xml.substr(base64_binary_start,
																	   end_pos - base64_binary_start) };
						fstream base64_file(file_name+".b64", fstream::out | fstream::binary | fstream::trunc);
						base64_file.write(base64_binary.c_str(), base64_binary.size());
						base64_file.flush();
						base64_file.close();
						core::system("base64 -D -i "+file_name+".b64 -o "+file_name);
						core::system("rm "+file_name+".b64");
					}
					
					// for x86: otool -tvVQch outfile
					if((device->vendor_type == VENDOR::INTEL || device->vendor_type == VENDOR::AMD) &&
					   device->type >= DEVICE_TYPE::CPU0 && device->type <= DEVICE_TYPE::CPU255) {
						core::system("otool -tvVQch "+file_name+" > "+file_name+".asm");
					}
					// for nvidia: strip first 12 bytes and cuobjdump -elf -sort -sass outfile
					else if(device->vendor_type == VENDOR::NVIDIA) {
						const string elf_data { file_io::file_to_string(file_name) };
						fstream elf_file(file_name, fstream::out | fstream::binary | fstream::trunc);
						elf_file.write(elf_data.c_str()+12, elf_data.size()-12);
						elf_file.flush();
						elf_file.close();
						core::system("cuobjdump -elf -sort -sass "+file_name+" > "+file_name+".asm");
					}
#endif
				}
			}
			device_num++;
		}
		
		for(size_t i = 0; i < program_sizes.size(); i++) {
			delete [] program_binaries[i];
		}
		delete [] program_binaries;
	}
	__HANDLE_CL_EXCEPTION("log_program_binary")
}

opencl::buffer_object* opencl::create_buffer_object(const opencl::BUFFER_FLAG type, const void* data) {
	try {
		opencl::buffer_object* buffer = new opencl::buffer_object();
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

opencl::buffer_object* opencl::create_buffer(const opencl::BUFFER_FLAG type, const size_t size, const void* data) {
	if(size == 0) {
		return nullptr;
	}
	
	try {
		buffer_object* buffer_obj = create_buffer_object(type, data);
		if(buffer_obj == nullptr) return nullptr;
		
		buffer_obj->size = size;
		buffer_obj->buffer = new cl::Buffer(*context, buffer_obj->flags, size,
											((buffer_obj->type & BUFFER_FLAG::INITIAL_COPY) != BUFFER_FLAG::NONE ||
											 (buffer_obj->type & BUFFER_FLAG::USE_HOST_MEMORY) != BUFFER_FLAG::NONE ? (void*)data : nullptr),
											&ierr);
		return buffer_obj;
	}
	__HANDLE_CL_EXCEPTION("create_buffer")
	return nullptr;
}

opencl::buffer_object* opencl::create_sub_buffer(const buffer_object* parent_buffer,
												 const BUFFER_FLAG type,
												 const size_t offset,
												 const size_t size) {
	if(parent_buffer == nullptr || parent_buffer->buffer == nullptr) {
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
		buffer_object* sub_buffer = create_buffer_object(type, nullptr);
		if(sub_buffer == nullptr) return nullptr;
		
		const auto region = cl_buffer_region { offset, size };
		sub_buffer->size = size;
		sub_buffer->buffer = new cl::Buffer(parent_buffer->buffer->createSubBuffer(sub_buffer->flags, CL_BUFFER_CREATE_TYPE_REGION, &region));
		sub_buffer->parent_buffer = parent_buffer;
		return sub_buffer;
	}
	__HANDLE_CL_EXCEPTION("create_sub_buffer")
	return nullptr;
}

opencl::buffer_object* opencl::create_image2d_buffer(const opencl::BUFFER_FLAG type,
													 const cl_channel_order channel_order, const cl_channel_type channel_type,
													 const size_t width, const size_t height, const void* data) {
	try {
		buffer_object* buffer_obj = create_buffer_object(type, (void*)data);
		if(buffer_obj == nullptr) return nullptr;
		
		buffer_obj->format.image_channel_order = channel_order;
		buffer_obj->format.image_channel_data_type = channel_type;
		buffer_obj->image_size.set(width, height, 1); // depth must be 1 for 2d images
		buffer_obj->image_type = buffer_object::IMAGE_TYPE::IMAGE_2D;
		buffer_obj->image_buffer = new cl::Image2D(*context, buffer_obj->flags, buffer_obj->format, width, height, 0, (void*)data, &ierr);
		return buffer_obj;
	}
	__HANDLE_CL_EXCEPTION("create_image2d_buffer")
	return nullptr;
}

opencl::buffer_object* opencl::create_image3d_buffer(const opencl::BUFFER_FLAG type,
													 const cl_channel_order channel_order, const cl_channel_type channel_type,
													 const size_t width, const size_t height, const size_t depth, const void* data) {
	try {
		buffer_object* buffer_obj = create_buffer_object(type, (void*)data);
		if(buffer_obj == nullptr) return nullptr;
		
		buffer_obj->format.image_channel_order = channel_order;
		buffer_obj->format.image_channel_data_type = channel_type;
		buffer_obj->image_size.set(width, height, depth);
		buffer_obj->image_type = buffer_object::IMAGE_TYPE::IMAGE_3D;
		buffer_obj->image_buffer = new cl::Image3D(*context, buffer_obj->flags, buffer_obj->format, width, height, depth, 0, 0, (void*)data, &ierr);
		return buffer_obj;
	}
	__HANDLE_CL_EXCEPTION("create_image3d_buffer")
	return nullptr;
}

opencl::buffer_object* opencl::create_ogl_buffer(const opencl::BUFFER_FLAG type, const GLuint ogl_buffer) {
	try {
		opencl::buffer_object* buffer = new opencl::buffer_object();
		buffers.push_back(buffer);
		
		// type/flag validity check
		BUFFER_FLAG vtype = BUFFER_FLAG::NONE;
		if((type & BUFFER_FLAG::DELETE_AFTER_USE) != BUFFER_FLAG::NONE) vtype |= BUFFER_FLAG::DELETE_AFTER_USE;
		if((type & BUFFER_FLAG::BLOCK_ON_READ) != BUFFER_FLAG::NONE) vtype |= BUFFER_FLAG::BLOCK_ON_READ;
		if((type & BUFFER_FLAG::BLOCK_ON_WRITE) != BUFFER_FLAG::NONE) vtype |= BUFFER_FLAG::BLOCK_ON_WRITE;
		
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
		
		vtype |= BUFFER_FLAG::OPENGL_BUFFER;
		
		buffer->type = vtype;
		buffer->ogl_buffer = ogl_buffer;
		buffer->data = nullptr;
		buffer->size = 0;
		buffer->buffer = new cl::BufferGL(*context, flags, ogl_buffer, &ierr);
		return buffer;
	}
	__HANDLE_CL_EXCEPTION("create_ogl_buffer")
	return nullptr;
}

opencl::buffer_object* opencl::create_ogl_image2d_buffer(const BUFFER_FLAG type, const GLuint texture, const GLenum target) {
	opencl::buffer_object* buffer = nullptr;
	try {
		buffer = new opencl::buffer_object();
		
		// type/flag validity check
		BUFFER_FLAG vtype = BUFFER_FLAG::NONE;
		if((type & BUFFER_FLAG::DELETE_AFTER_USE) != BUFFER_FLAG::NONE) vtype |= BUFFER_FLAG::DELETE_AFTER_USE;
		if((type & BUFFER_FLAG::BLOCK_ON_READ) != BUFFER_FLAG::NONE) vtype |= BUFFER_FLAG::BLOCK_ON_READ;
		if((type & BUFFER_FLAG::BLOCK_ON_WRITE) != BUFFER_FLAG::NONE) vtype |= BUFFER_FLAG::BLOCK_ON_WRITE;
		
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
		
		vtype |= BUFFER_FLAG::OPENGL_BUFFER;
		
		buffer->type = vtype;
		buffer->ogl_buffer = texture;
		buffer->data = nullptr;
		buffer->size = 0;
		buffer->image_type = buffer_object::IMAGE_TYPE::IMAGE_2D;
		buffer->image_buffer = new cl::Image2DGL(*context, flags, target, 0, texture, &ierr);
		buffer->image_size.set(buffer->image_buffer->getImageInfo<CL_IMAGE_WIDTH>(),
							   buffer->image_buffer->getImageInfo<CL_IMAGE_HEIGHT>(),
							   1);
		buffers.push_back(buffer);
		return buffer;
	}
	__HANDLE_CL_EXCEPTION_START("create_ogl_image2d_buffer")
		if(buffer != nullptr) delete buffer;
	__HANDLE_CL_EXCEPTION_END
	return nullptr;
}

opencl::buffer_object* opencl::create_ogl_image2d_renderbuffer(const BUFFER_FLAG type, const GLuint renderbuffer) {
	try {
		opencl::buffer_object* buffer = new opencl::buffer_object();
		buffers.push_back(buffer);
		
		// type/flag validity check
		BUFFER_FLAG vtype = BUFFER_FLAG::NONE;
		if((type & BUFFER_FLAG::DELETE_AFTER_USE) != BUFFER_FLAG::NONE) vtype |= BUFFER_FLAG::DELETE_AFTER_USE;
		if((type & BUFFER_FLAG::BLOCK_ON_READ) != BUFFER_FLAG::NONE) vtype |= BUFFER_FLAG::BLOCK_ON_READ;
		if((type & BUFFER_FLAG::BLOCK_ON_WRITE) != BUFFER_FLAG::NONE) vtype |= BUFFER_FLAG::BLOCK_ON_WRITE;
		
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
		
		vtype |= BUFFER_FLAG::OPENGL_BUFFER;
		
		buffer->type = vtype;
		buffer->ogl_buffer = renderbuffer;
		buffer->data = nullptr;
		buffer->size = 0;
		buffer->image_type = buffer_object::IMAGE_TYPE::IMAGE_2D;
		buffer->image_buffer = new cl::ImageRenderGL(*context, flags, renderbuffer, &ierr);
		buffer->image_size.set(buffer->image_buffer->getImageInfo<CL_IMAGE_WIDTH>(),
							   buffer->image_buffer->getImageInfo<CL_IMAGE_HEIGHT>(),
							   1);
		return buffer;
	}
	__HANDLE_CL_EXCEPTION("create_ogl_image2d_renderbuffer")
	return nullptr;
}

void opencl::delete_buffer(opencl::buffer_object* buffer_obj) {
	// remove buffer from each associated kernel (and unset the kernel argument)
	for(const auto& associated_kernel : buffer_obj->associated_kernels) {
		for(const auto& arg_num : associated_kernel.second) {
			associated_kernel.first->args_passed[arg_num] = false;
			associated_kernel.first->buffer_args[arg_num] = nullptr;
		}
	}
	buffer_obj->associated_kernels.clear();
	if(buffer_obj->buffer != nullptr) delete buffer_obj->buffer;
	if(buffer_obj->image_buffer != nullptr) delete buffer_obj->image_buffer;
	const auto iter = find(begin(buffers), end(buffers), buffer_obj);
	if(iter != end(buffers)) buffers.erase(iter);
	delete buffer_obj;
}

void opencl::write_buffer(opencl::buffer_object* buffer_obj, const void* src, const size_t offset, const size_t size) {
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
	
	try {
		queues[&active_device->device]->enqueueWriteBuffer(*buffer_obj->buffer,
														   ((buffer_obj->type & BUFFER_FLAG::BLOCK_ON_WRITE) != BUFFER_FLAG::NONE),
														   write_offset, write_size, src);
	}
	__HANDLE_CL_EXCEPTION("write_buffer")
}

void opencl::write_buffer_rect(buffer_object* buffer_obj, const void* src,
							   const size3 buffer_origin,
							   const size3 host_origin,
							   const size3 region,
							   const size_t buffer_row_pitch, const size_t buffer_slice_pitch,
							   const size_t host_row_pitch, const size_t host_slice_pitch) {
	try {
		cl::size_t<3> write_buffer_origin {{ buffer_origin.x, buffer_origin.y, buffer_origin.z }};
		cl::size_t<3> write_host_origin {{ host_origin.x, host_origin.y, host_origin.z }};
		cl::size_t<3> write_region {{ region.x, region.y, region.z }};
		queues[&active_device->device]->enqueueWriteBufferRect(*buffer_obj->buffer,
															   ((buffer_obj->type & BUFFER_FLAG::BLOCK_ON_WRITE) != BUFFER_FLAG::NONE),
															   write_buffer_origin, write_host_origin, write_region,
															   buffer_row_pitch, buffer_slice_pitch, host_row_pitch, host_slice_pitch, (void*)src);
	}
	__HANDLE_CL_EXCEPTION("write_buffer_rect")
}

void opencl::write_image(opencl::buffer_object* buffer_obj, const void* src, const size3 origin, const size3 region) {
	try {
		cl::size_t<3> img_origin {{ origin.x, origin.y, origin.z }};
		cl::size_t<3> img_region {{ region.x, region.y, region.z }};
		if(!check_image_origin_and_size(buffer_obj, img_origin, img_region)) return;
		queues[&active_device->device]->enqueueWriteImage(*buffer_obj->image_buffer,
														  ((buffer_obj->type & BUFFER_FLAG::BLOCK_ON_WRITE) != BUFFER_FLAG::NONE),
														  img_origin, img_region, 0, 0, (void*)src);
	}
	__HANDLE_CL_EXCEPTION("write_image2d")
}

void opencl::copy_buffer(const buffer_object* src_buffer, buffer_object* dst_buffer,
						 const size_t src_offset, const size_t dst_offset, const size_t size) {
	try {
		queues[&active_device->device]->enqueueCopyBuffer(*src_buffer->buffer, *dst_buffer->buffer, src_offset, dst_offset, size);
	}
	__HANDLE_CL_EXCEPTION("copy_buffer")
}

void opencl::copy_buffer_rect(const buffer_object* src_buffer, buffer_object* dst_buffer,
							  const size3 src_origin, const size3 dst_origin,
							  const size3 region,
							  const size_t src_row_pitch, const size_t src_slice_pitch,
							  const size_t dst_row_pitch, const size_t dst_slice_pitch) {
	try {
		cl::size_t<3> copy_src_origin {{ src_origin.x, src_origin.y, src_origin.z }};
		cl::size_t<3> copy_dst_origin {{ dst_origin.x, dst_origin.y, dst_origin.z }};
		cl::size_t<3> copy_region {{ region.x, region.y, region.z }};
		queues[&active_device->device]->enqueueCopyBufferRect(*src_buffer->buffer, *dst_buffer->buffer,
															  copy_src_origin, copy_dst_origin, copy_region,
															  src_row_pitch, src_slice_pitch, dst_row_pitch, dst_slice_pitch);
	}
	__HANDLE_CL_EXCEPTION("copy_buffer_rect")
}

void opencl::copy_image(const buffer_object* src_buffer, buffer_object* dst_buffer,
						const size3 src_origin, const size3 dst_origin, const size3 region) {
	try {
		cl::size_t<3> img_src_origin {{ src_origin.x, src_origin.y, src_origin.z }};
		cl::size_t<3> img_dst_origin {{ dst_origin.x, dst_origin.y, dst_origin.z }};
		cl::size_t<3> img_region {{ region.x, region.y, region.z }};
		if(!check_image_origin_and_size(src_buffer, img_src_origin, img_region)) return; // check src first, so region is set correctly (if default 0)
		if(!check_image_origin_and_size(dst_buffer, img_dst_origin, img_region)) return;
		queues[&active_device->device]->enqueueCopyImage(*src_buffer->image_buffer, *dst_buffer->image_buffer,
														 img_src_origin, img_dst_origin, img_region);
	}
	__HANDLE_CL_EXCEPTION("copy_image")
}

void opencl::copy_buffer_to_image(const buffer_object* src_buffer, buffer_object* dst_buffer,
								  const size_t src_offset, const size3 dst_origin, const size3 dst_region) {
	try {
		cl::size_t<3> img_origin {{ dst_origin.x, dst_origin.y, dst_origin.z }};
		cl::size_t<3> img_region {{ dst_region.x, dst_region.y, dst_region.z }};
		if(!check_image_origin_and_size(dst_buffer, img_origin, img_region)) return;
		queues[&active_device->device]->enqueueCopyBufferToImage(*src_buffer->buffer, *dst_buffer->image_buffer,
																 src_offset, img_origin, img_region);
	}
	__HANDLE_CL_EXCEPTION("copy_buffer_to_image")
}

void opencl::copy_image_to_buffer(const buffer_object* src_buffer, buffer_object* dst_buffer,
								  const size3 src_origin, const size3 src_region, const size_t dst_offset) {
	try {
		cl::size_t<3> img_origin {{ src_origin.x, src_origin.y, src_origin.z }};
		cl::size_t<3> img_region {{ src_region.x, src_region.y, src_region.z }};
		if(!check_image_origin_and_size(src_buffer, img_origin, img_region)) return;
		queues[&active_device->device]->enqueueCopyImageToBuffer(*src_buffer->image_buffer, *dst_buffer->buffer,
																 img_origin, img_region, dst_offset);
	}
	__HANDLE_CL_EXCEPTION("copy_image_to_buffer")
}

void opencl::read_buffer(void* dst, const opencl::buffer_object* buffer_obj, const size_t offset, const size_t size_) {
	try {
		const size_t size = (size_ == 0 ? buffer_obj->size : size_);
		queues[&active_device->device]->enqueueReadBuffer(*buffer_obj->buffer,
														  ((buffer_obj->type & BUFFER_FLAG::BLOCK_ON_READ) != BUFFER_FLAG::NONE),
														  offset, size, dst);
	}
	__HANDLE_CL_EXCEPTION("read_buffer")
}

void opencl::read_buffer_rect(void* dst, const buffer_object* buffer_obj,
							  const size3 buffer_origin,
							  const size3 host_origin,
							  const size3 region,
							  const size_t buffer_row_pitch, const size_t buffer_slice_pitch,
							  const size_t host_row_pitch, const size_t host_slice_pitch) {
	try {
		cl::size_t<3> read_buffer_origin {{ buffer_origin.x, buffer_origin.y, buffer_origin.z }};
		cl::size_t<3> read_host_origin {{ host_origin.x, host_origin.y, host_origin.z }};
		cl::size_t<3> read_region {{ region.x, region.y, region.z }};
		queues[&active_device->device]->enqueueReadBufferRect(*buffer_obj->buffer,
															  ((buffer_obj->type & BUFFER_FLAG::BLOCK_ON_READ) != BUFFER_FLAG::NONE),
															  read_buffer_origin, read_host_origin, read_region,
															  buffer_row_pitch, buffer_slice_pitch, host_row_pitch, host_slice_pitch, dst);
	}
	__HANDLE_CL_EXCEPTION("read_buffer_rect")
}

void opencl::read_image(void* dst, const opencl::buffer_object* buffer_obj, const size3 origin, const size3 region,
						  const size_t image_row_pitch, const size_t image_slice_pitch) {
	try {
		cl::size_t<3> img_origin {{ origin.x, origin.y, origin.z }};
		cl::size_t<3> img_region {{ region.x, region.y, region.z }};
		if(!check_image_origin_and_size(buffer_obj, img_origin, img_region)) return;
		
		queues[&active_device->device]->enqueueReadImage(*buffer_obj->image_buffer,
														 ((buffer_obj->type & BUFFER_FLAG::BLOCK_ON_READ) != BUFFER_FLAG::NONE),
														 img_origin, img_region, image_row_pitch, image_slice_pitch, dst);
	}
	__HANDLE_CL_EXCEPTION("read_image")
}

void opencl::run_kernel(weak_ptr<kernel_object> kernel_obj) {
	auto kernel_ptr = kernel_obj.lock();
	if(kernel_ptr == nullptr) {
		oclr_error("invalid kernel object (nullptr)!");
		return;
	}
	
	try {
		bool all_set = true;
		for(unsigned int i = 0; i < kernel_ptr->args_passed.size(); i++) {
			if(!kernel_ptr->args_passed[i]) {
				oclr_error("kernel %s: argument #%u not set!", kernel_ptr->name, i);
				all_set = false;
			}
		}
		if(!all_set) return;
		
		cl::CommandQueue* cmd_queue = queues[&active_device->device];
		
		vector<cl::Memory> gl_objects;
		for(const auto& buffer_arg : kernel_ptr->buffer_args) {
			if(buffer_arg == nullptr) continue;
			if((buffer_arg->type & BUFFER_FLAG::COPY_ON_USE) != BUFFER_FLAG::NONE) {
				write_buffer(buffer_arg, buffer_arg->data);
			}
			if((buffer_arg->type & BUFFER_FLAG::OPENGL_BUFFER) != BUFFER_FLAG::NONE &&
			   !buffer_arg->manual_gl_sharing) {
				gl_objects.push_back(*(buffer_arg->buffer != nullptr ?
									   (cl::Memory*)buffer_arg->buffer :
									   (cl::Memory*)buffer_arg->image_buffer));
				kernel_ptr->has_ogl_buffers = true;
			}
		}
		if(!gl_objects.empty()) {
			cmd_queue->enqueueAcquireGLObjects(&gl_objects);
		}
		
		// TODO: write my own opencl kernel functor (this is rather ugly right now ...)
		auto functor = kernel_ptr->functors.find(cmd_queue);
		if(functor == kernel_ptr->functors.end()) {
			functor = kernel_ptr->functors.insert({
				cmd_queue,
				kernel_ptr->kernel->bind(*cmd_queue,
										 kernel_ptr->global,
										 kernel_ptr->local)
			}).first;
		}
		else {
			functor->second.global_ = kernel_ptr->global;
			functor->second.local_ = kernel_ptr->local;
		}
		
#if !defined(OCLRASTER_PROFILING)
		functor->second();
		//functor->second().wait();
#else
		auto evt = functor->second();
		evt.wait();
		const auto prof_queued = evt.getProfilingInfo<CL_PROFILING_COMMAND_QUEUED>();
		const auto prof_submit = evt.getProfilingInfo<CL_PROFILING_COMMAND_SUBMIT>();
		const auto prof_start = evt.getProfilingInfo<CL_PROFILING_COMMAND_START>();
		const auto prof_end = evt.getProfilingInfo<CL_PROFILING_COMMAND_END>();
		const auto ns_to_ms = [](const unsigned long long int& t) {
			return ((double)t) / 1000000.0;
		};
		oclr_msg("profiling %s:\n"
				 "\t%u (queued->submit), %u (submit->start), %u (start->end), %u (submit->end)\n"
				 "\t%ums (queued->submit), %ums (submit->start), %ums (start->end), %ums (submit->end)",
				 kernel_ptr->name,
				 prof_submit - prof_queued,
				 prof_start - prof_submit,
				 prof_end - prof_start,
				 prof_end - prof_submit,
				 ns_to_ms(prof_submit - prof_queued),
				 ns_to_ms(prof_start - prof_submit),
				 ns_to_ms(prof_end - prof_start),
				 ns_to_ms(prof_end - prof_submit));
#endif
		
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
			cmd_queue->enqueueReleaseGLObjects(&gl_objects);
		}
	}
	__HANDLE_CL_EXCEPTION_EXT("run_kernel", (" - in kernel: "+kernel_ptr->name).c_str())
}

void opencl::finish() {
	if(active_device == nullptr) return;
	queues[&active_device->device]->finish();
}

void opencl::flush() {
	if(active_device == nullptr) return;
	queues[&active_device->device]->flush();
}

void opencl::barrier() {
	if(active_device == nullptr) return;
	queues[&active_device->device]->enqueueBarrier();
}

void opencl::activate_context() {
	// nothing for opencl
}

void opencl::deactivate_context() {
	// nothing for opencl
}

bool opencl::set_kernel_argument(const unsigned int& index, opencl::buffer_object* arg) {
	if((arg->buffer != nullptr && opencl_base::set_kernel_argument(index, (*arg->buffer)())) ||
	   (arg->image_buffer != nullptr && opencl_base::set_kernel_argument(index, *(cl::Memory*)arg->image_buffer))) {
		cur_kernel->buffer_args[index] = arg;
		arg->associated_kernels[cur_kernel].push_back(index);
		return true;
	}
	return false;
}

bool opencl::set_kernel_argument(const unsigned int& index, const opencl::buffer_object* arg) {
	return set_kernel_argument(index, (opencl::buffer_object*)arg);
}

bool opencl::set_kernel_argument(const unsigned int& index, size_t size, void* arg) {
	try {
		cur_kernel->kernel->setArg(index, size, arg);
		cur_kernel->args_passed[index] = true;
		return true;
	}
	__HANDLE_CL_EXCEPTION_EXT("set_kernel_argument",
							  (" - arg idx: "+size_t2string(index)+
							   ", arg size: "+size_t2string(size)+
							   ", in kernel: "+cur_kernel->name).c_str())
	return false;
}

void* __attribute__((aligned(128))) opencl::map_buffer(opencl::buffer_object* buffer_obj, const MAP_BUFFER_FLAG access_type,
													   const size_t offset, const size_t size) {
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
			if(get_platform_cl_version() >= CL_VERSION::CL_1_2) {
				map_flags |= CL_MAP_WRITE_INVALIDATE_REGION;
			}
			else {
				map_flags |= CL_MAP_WRITE;
			}
		}
		
		void* __attribute__((aligned(128))) map_ptr = nullptr;
		if(buffer_obj->buffer != nullptr) {
			map_ptr = queues[&active_device->device]->enqueueMapBuffer(*buffer_obj->buffer, blocking, map_flags, map_offset, map_size);
		}
		else if(buffer_obj->image_buffer != nullptr) {
			oclr_error("use map_image to map an image buffer object!");
			return nullptr;
		}
		else {
			oclr_error("unknown buffer object!");
			return nullptr;
		}
		return map_ptr;
	}
	__HANDLE_CL_EXCEPTION("map_buffer")
	return nullptr;
}

void* __attribute__((aligned(128))) opencl::map_image(opencl_base::buffer_object* buffer_obj,
													  const MAP_BUFFER_FLAG access_type,
													  const size3 origin,
													  const size3 region,
													  size_t* image_row_pitch,
													  size_t* image_slice_pitch) {
	try {
		const bool blocking { (access_type & MAP_BUFFER_FLAG::BLOCK) != MAP_BUFFER_FLAG::NONE };
		
		if((access_type & MAP_BUFFER_FLAG::READ_WRITE) != MAP_BUFFER_FLAG::NONE &&
		   (access_type & MAP_BUFFER_FLAG::WRITE_INVALIDATE) != MAP_BUFFER_FLAG::NONE) {
			oclr_error("READ or WRITE access and WRITE_INVALIDATE are mutually exclusive!");
			return nullptr;
		}
		
		cl::size_t<3> map_origin {{ origin.x, origin.y, origin.z }};
		cl::size_t<3> map_region {{ region.x, region.y, region.z }};
		if(!check_image_origin_and_size(buffer_obj, map_origin, map_region)) return nullptr;
		
		cl_map_flags map_flags = ((access_type & (MAP_BUFFER_FLAG::READ_WRITE | MAP_BUFFER_FLAG::WRITE_INVALIDATE)) ==
								  MAP_BUFFER_FLAG::NONE) ? CL_MAP_READ : 0; // if no access type is specified, use read-only
		switch(access_type & MAP_BUFFER_FLAG::READ_WRITE) {
			case MAP_BUFFER_FLAG::READ_WRITE: map_flags = CL_MAP_READ | CL_MAP_WRITE; break;
			case MAP_BUFFER_FLAG::READ: map_flags = CL_MAP_READ; break;
			case MAP_BUFFER_FLAG::WRITE: map_flags = CL_MAP_WRITE; break;
			default: break;
		}
		if((access_type & MAP_BUFFER_FLAG::WRITE_INVALIDATE) != MAP_BUFFER_FLAG::NONE) {
			if(get_platform_cl_version() >= CL_VERSION::CL_1_2) {
				map_flags |= CL_MAP_WRITE_INVALIDATE_REGION;
			}
			else {
				map_flags |= CL_MAP_WRITE;
			}
		}
		
		void* __attribute__((aligned(128))) map_ptr = nullptr;
		if(buffer_obj->image_buffer != nullptr) {
			map_ptr = queues[&active_device->device]->enqueueMapImage(*buffer_obj->image_buffer, blocking, map_flags,
																	  map_origin, map_region,
																	  image_row_pitch, image_slice_pitch);
		}
		else if(buffer_obj->buffer != nullptr) {
			oclr_error("use map_buffer to map a buffer object!");
			return nullptr;
		}
		else {
			oclr_error("unknown buffer object!");
			return nullptr;
		}
		return map_ptr;
	}
	__HANDLE_CL_EXCEPTION("map_image")
	return nullptr;
}

pair<opencl::buffer_object*, void*> opencl::create_and_map_buffer(const BUFFER_FLAG type,
																  const size_t size,
																  const void* data,
																  const MAP_BUFFER_FLAG access_type,
																  const size_t map_offset,
																  const size_t map_size) {
	auto buffer_obj = create_buffer(type, size, data);
	auto mapped_ptr = map_buffer(buffer_obj, access_type, map_offset, map_size);
	return { buffer_obj, mapped_ptr };
}

void opencl::unmap_buffer(opencl::buffer_object* buffer_obj, void* map_ptr) {
	try {
		void* buffer_ptr = nullptr;
		if(buffer_obj->buffer != nullptr) buffer_ptr = buffer_obj->buffer;
		else if(buffer_obj->image_buffer != nullptr) buffer_ptr = buffer_obj->image_buffer;
		else {
			oclr_error("unknown buffer object!");
			return;
		}
		queues[&active_device->device]->enqueueUnmapMemObject(*(cl::Memory*)buffer_ptr, map_ptr);
	}
	__HANDLE_CL_EXCEPTION("unmap_buffer")
}

#if defined(CL_VERSION_1_2) && !defined(OCLRASTER_POCL)
void opencl::_fill_buffer(buffer_object* buffer_obj,
						  const void* pattern,
						  const size_t& pattern_size,
						  const size_t offset,
						  const size_t size_) {
	// TODO: on os x: clEnqueueFillBuffer spams the console on every call (unusable as of 10.8.3; semi-fixed in 10.8.4)
	// also: insanely slow (slower than copying a zero buffer)
	try {
		// TODO: get 1.2 cl.hpp
		const size_t size = (size_ == 0 ? (buffer_obj->size / pattern_size) : size_);
		cl::CommandQueue* cmd_queue = queues[&active_device->device];
		const cl_int err = clEnqueueFillBuffer((*cmd_queue)(), (*buffer_obj->buffer)(),
											   pattern, pattern_size, offset, size, 0, nullptr, nullptr);
		if(err != CL_SUCCESS) {
			throw cl::Error(err);
		}
	}
	__HANDLE_CL_EXCEPTION("fill_buffer")
}
#else
void opencl::_fill_buffer(buffer_object* buffer_obj oclr_unused,
						  const void* pattern oclr_unused,
						  const size_t& pattern_size oclr_unused,
						  const size_t offset oclr_unused,
						  const size_t size_ oclr_unused) {
}
#endif

size_t opencl::get_kernel_work_group_size() const {
	if(cur_kernel == nullptr || active_device == nullptr) return 0;
	
	try {
		return cur_kernel->kernel->getWorkGroupInfo<CL_KERNEL_WORK_GROUP_SIZE>(active_device->device);
	}
	__HANDLE_CL_EXCEPTION("get_kernel_work_group_size")
	return 0;
}

void opencl::acquire_gl_object(buffer_object* gl_buffer_obj) {
	vector<cl::Memory> gl_objects;
	gl_objects.push_back(*(gl_buffer_obj->buffer != nullptr ?
						   (cl::Memory*)gl_buffer_obj->buffer :
						   (cl::Memory*)gl_buffer_obj->image_buffer));
	queues[&active_device->device]->enqueueAcquireGLObjects(&gl_objects);
}

void opencl::release_gl_object(buffer_object* gl_buffer_obj) {
	vector<cl::Memory> gl_objects;
	gl_objects.push_back(*(gl_buffer_obj->buffer != nullptr ?
						   (cl::Memory*)gl_buffer_obj->buffer :
						   (cl::Memory*)gl_buffer_obj->image_buffer));
	queues[&active_device->device]->enqueueReleaseGLObjects(&gl_objects);
}

void opencl::set_active_device(const opencl_base::DEVICE_TYPE& dev) {
	switch(dev) {
		case DEVICE_TYPE::FASTEST_GPU:
			if(fastest_gpu != nullptr) {
				active_device = fastest_gpu;
				return;
			}
			break;
		case DEVICE_TYPE::FASTEST_CPU:
			if(fastest_cpu != nullptr) {
				active_device = fastest_cpu;
				return;
			}
			break;
		case DEVICE_TYPE::ALL_GPU:
			// TODO: ...
			break;
		case DEVICE_TYPE::ALL_CPU:
			// TODO: ...
			break;
		case DEVICE_TYPE::ALL_DEVICES:
			// TODO: ...
			break;
		case DEVICE_TYPE::NONE:
		default:
			break;
	}
	
	if((dev >= DEVICE_TYPE::GPU0 && dev <= DEVICE_TYPE::GPU255) ||
	   (dev >= DEVICE_TYPE::CPU0 && dev <= DEVICE_TYPE::CPU255)) {
		for(const auto& device : devices) {
			if(device->type == dev) {
				active_device = device;
				return;
			}
		}
	}
	
	if(active_device != nullptr) {
		oclr_error("can't use device %u - keeping current one (%u)!", dev, active_device->type);
	}
	else {
		// try to use _any_ device if there is at least one available ...
		if(!devices.empty()) {
			active_device = devices[0];
			oclr_error("can't use device %u (doesn't exist or isn't available) - using %s (%u) instead!",
					   dev, active_device->name, active_device->type);
		}
		else {
			oclr_error("can't use device %u and no other device is currently available!", dev);
		}
	}
}
