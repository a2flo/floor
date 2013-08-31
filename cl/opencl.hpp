/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2013 Florian Ziesche
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

#ifndef __FLOOR_OPENCL_HPP__
#define __FLOOR_OPENCL_HPP__

#include "floor/floor.hpp"
#include "core/file_io.hpp"
#include "core/core.hpp"
#include "core/vector2.hpp"
#include "core/gl_support.hpp"
#include "hash/city.hpp"

// necessary for now (when compiling with opencl 1.2+ headers)
#define CL_USE_DEPRECATED_OPENCL_1_1_APIS 1

#if defined(__APPLE__)
#include <OpenCL/OpenCL.h>
#include <OpenCL/cl.h>
#include <OpenCL/cl_platform.h>
#include <OpenCL/cl_ext.h>
#include <OpenCL/cl_gl.h>
#if !defined(FLOOR_IOS)
#include <OpenGL/CGLContext.h>
#include <OpenGL/CGLCurrent.h>
#include <OpenGL/CGLDevice.h>
#endif
#else
#include <CL/cl.h>
#include <CL/cl_platform.h>
#include <CL/cl_ext.h>
#include <CL/cl_gl.h>
#endif

#define __CL_ENABLE_EXCEPTIONS
#include "cl/cl.hpp"

#if !defined(CL_MAP_WRITE_INVALIDATE_REGION)
#define CL_MAP_WRITE_INVALIDATE_REGION (1 << 2)
#endif

//
#if defined(FLOOR_CUDA_CL)
#if defined(__APPLE__)
#include <CUDA/cuda.h>
#include <CUDA/cudaGL.h>
#else
#include <cuda.h>
#include <cudaGL.h>
#endif
#endif

enum class IMAGE_TYPE : unsigned short int;
enum class IMAGE_CHANNEL : unsigned short int;

// opencl base interface
class FLOOR_API opencl_base {
public:
	struct kernel_object;
	struct buffer_object;
	struct device_object;
	
	opencl_base& operator=(const opencl_base&) = delete;
	opencl_base(const opencl_base&) = delete;
	opencl_base();
	virtual ~opencl_base();
	
	bool is_supported() const { return supported; }
	bool is_cpu_support() const;
	bool is_gpu_support() const;
	bool is_full_double_support() const;
	
	enum class DEVICE_TYPE : unsigned int {
		NONE,
		FASTEST_GPU,
		FASTEST_CPU,
		ALL_GPU,
		ALL_CPU,
		ALL_DEVICES,
		GPU0,
		GPU1,
		GPU2,
		GPU4,
		GPU5,
		GPU6,
		GPU7,
		GPU255 = GPU0+255,
		CPU0,
		CPU1,
		CPU2,
		CPU3,
		CPU4,
		CPU5,
		CPU6,
		CPU7,
		CPU255 = CPU0+255
	};
	device_object* get_device(const DEVICE_TYPE& device);
	device_object* get_active_device();
	const vector<device_object*>& get_devices() const;
	
	enum class PLATFORM_VENDOR {
		NVIDIA,
		INTEL,
		AMD,
		APPLE,
		FREEOCL,
		POCL,
		CUDA,
		UNKNOWN
	};
	enum class CL_VERSION {
		CL_1_0,
		CL_1_1,
		CL_1_2,
		CL_2_0,
	};
	// <vendor, index/identifier for use in floor config>
	static vector<pair<PLATFORM_VENDOR, string>> get_platforms();
	static string platform_vendor_to_str(const PLATFORM_VENDOR& pvendor);
	PLATFORM_VENDOR get_platform_vendor() const;
	CL_VERSION get_platform_cl_version() const;
	
	enum class VENDOR {
		NVIDIA,
		INTEL,
		AMD,
		APPLE,
		FREEOCL,
		POCL,
		UNKNOWN
	};
	
	//! buffer flags
	enum class BUFFER_FLAG : unsigned int {
		NONE				= (0u),
		READ				= (1u << 0u),			//!< enum read only buffer (kernel POV)
		WRITE				= (1u << 1u),			//!< enum write only buffer (kernel POV)
		READ_WRITE			= (READ | WRITE),		//!< enum read and write buffer (kernel POV)
		INITIAL_COPY		= (1u << 2u),			//!< enum the specified data will be copied to the buffer at creation time
		COPY_ON_USE			= (1u << 3u),			//!< enum the specified data will be copied to the buffer each time an associated kernel is being used (that is right before kernel execution)
		USE_HOST_MEMORY		= (1u << 4u),			//!< enum buffer memory will be allocated in host memory
		READ_BACK_RESULT	= (1u << 5u),			//!< enum every time an associated kernel has been executed, the result buffer data will be read back/copied to the specified pointer location
		DELETE_AFTER_USE	= (1u << 6u),			//!< enum the buffer will be deleted after its first use (after an associated kernel has been executed)
		BLOCK_ON_READ		= (1u << 7u),			//!< enum the read command is blocking, all data will be read/copied before program continuation
		BLOCK_ON_WRITE		= (1u << 8u),			//!< enum the write command is blocking, all data will be written before program continuation
		OPENGL_BUFFER		= (1u << 9u),			//!< enum determines if a buffer is a shared opengl buffer/image/memory object
	};
	enum_class_bitwise_or(BUFFER_FLAG)
	enum_class_bitwise_and(BUFFER_FLAG)
	
	enum class MAP_BUFFER_FLAG : unsigned int {
		NONE				= (0u),
		READ				= (1u << 0u),
		WRITE				= (1u << 1u),
		WRITE_INVALIDATE	= (1u << 2u), //!< CL_MAP_WRITE_INVALIDATE_REGION
		READ_WRITE			= (READ | WRITE),
		BLOCK				= (1u << 3u),
	};
	enum_class_bitwise_or(MAP_BUFFER_FLAG)
	enum_class_bitwise_and(MAP_BUFFER_FLAG)

	virtual void init(bool use_platform_devices = false, const size_t platform_index = 0,
					  const set<string> device_restriction = set<string> {},
					  const bool gl_sharing = true) = 0;
	void reload_kernels();
	
	// kernel execution
	void use_kernel(const string& identifier);
	void use_kernel(weak_ptr<kernel_object> kernel_obj);
	void run_kernel();
	void run_kernel(const string& identifier);
	virtual void run_kernel(weak_ptr<kernel_object> kernel_obj) = 0;
	weak_ptr<kernel_object> get_cur_kernel() { return cur_kernel; }
	
	// helper functions (for external synchronization, not used internally!)
	// note: kernel execution and kernel parameter setting is not thread-safe!
	void lock();
	void unlock();
	bool try_lock();
	
	//
	virtual void finish() = 0;
	virtual void flush() = 0;
	virtual void barrier() = 0;
	virtual void activate_context() = 0;
	virtual void deactivate_context() = 0;
	
	//
	weak_ptr<kernel_object> add_kernel_file(const string& identifier, const string& file_name, const string& func_name, const string additional_options = "");
	virtual weak_ptr<kernel_object> add_kernel_src(const string& identifier, const string& src, const string& func_name, const string additional_options = "") = 0;
	void delete_kernel(const string& identifier);
	virtual void delete_kernel(weak_ptr<kernel_object> kernel_obj) = 0;
	
	// create
	virtual buffer_object* create_buffer(const BUFFER_FLAG type,
										 const size_t size,
										 const void* data = nullptr) = 0;
	virtual buffer_object* create_sub_buffer(const buffer_object* parent_buffer,
											 const BUFFER_FLAG type,
											 const size_t offset,
											 const size_t size) = 0;
	virtual buffer_object* create_image2d_buffer(const BUFFER_FLAG type,
												 const cl_channel_order channel_order,
												 const cl_channel_type channel_type,
												 const size_t width, const size_t height,
												 const void* data = nullptr) = 0;
	virtual buffer_object* create_image3d_buffer(const BUFFER_FLAG type,
												 const cl_channel_order channel_order,
												 const cl_channel_type channel_type,
												 const size_t width, const size_t height, const size_t depth,
												 const void* data = nullptr) = 0;
	virtual buffer_object* create_ogl_buffer(const BUFFER_FLAG type, const GLuint ogl_buffer) = 0;
	virtual buffer_object* create_ogl_image2d_buffer(const BUFFER_FLAG type,
													 const GLuint texture,
													 const GLenum target = GL_TEXTURE_2D) = 0;
	virtual buffer_object* create_ogl_image2d_renderbuffer(const BUFFER_FLAG type, const GLuint renderbuffer) = 0;
	
	virtual void delete_buffer(buffer_object* buffer_obj) = 0;
	
	// write
	virtual void write_buffer(buffer_object* buffer_obj, const void* src,
							  const size_t offset = 0, const size_t size = 0) = 0;
	virtual void write_buffer_rect(buffer_object* buffer_obj, const void* src,
								   const size3 buffer_origin,
								   const size3 host_origin,
								   const size3 region,
								   const size_t buffer_row_pitch = 0, const size_t buffer_slice_pitch = 0,
								   const size_t host_row_pitch = 0, const size_t host_slice_pitch = 0) = 0;
	virtual void write_image(buffer_object* buffer_obj, const void* src,
							 const size3 origin = { 0, 0, 0 },
							 const size3 region = { 0, 0, 0 }) = 0;
	
	// copy
	virtual void copy_buffer(const buffer_object* src_buffer, buffer_object* dst_buffer,
							 const size_t src_offset = 0, const size_t dst_offset = 0,
							 const size_t size = 0) = 0;
	virtual void copy_buffer_rect(const buffer_object* src_buffer, buffer_object* dst_buffer,
								  const size3 src_origin, const size3 dst_origin,
								  const size3 region,
								  const size_t src_row_pitch = 0, const size_t src_slice_pitch = 0,
								  const size_t dst_row_pitch = 0, const size_t dst_slice_pitch = 0) = 0;
	virtual void copy_image(const buffer_object* src_buffer, buffer_object* dst_buffer,
							const size3 src_origin, const size3 dst_origin,
							const size3 region) = 0;
	virtual void copy_buffer_to_image(const buffer_object* src_buffer, buffer_object* dst_buffer,
									  const size_t src_offset = 0,
									  const size3 dst_origin = { 0, 0, 0 }, const size3 dst_region = { 0, 0, 0 }) = 0;
	virtual void copy_image_to_buffer(const buffer_object* src_buffer, buffer_object* dst_buffer,
									  const size3 src_origin = { 0, 0, 0 }, const size3 src_region = { 0, 0, 0 },
									  const size_t dst_offset = 0) = 0;
	
	// read
	virtual void read_buffer(void* dst, const buffer_object* buffer_obj,
							 const size_t offset = 0, const size_t size = 0) = 0;
	virtual void read_buffer_rect(void* dst, const buffer_object* buffer_obj,
								  const size3 buffer_origin,
								  const size3 host_origin,
								  const size3 region,
								  const size_t buffer_row_pitch = 0, const size_t buffer_slice_pitch = 0,
								  const size_t host_row_pitch = 0, const size_t host_slice_pitch = 0) = 0;
	virtual void read_image(void* dst, const buffer_object* buffer_obj,
							const size3 origin = { 0, 0, 0 }, const size3 region = { 0, 0, 0 },
							const size_t image_row_pitch = 0,
							const size_t image_slice_pitch = 0) = 0;
	
	// map
	virtual void* __attribute__((aligned(128))) map_buffer(buffer_object* buffer_obj,
														   const MAP_BUFFER_FLAG access_type = (MAP_BUFFER_FLAG::READ_WRITE | MAP_BUFFER_FLAG::BLOCK),
														   const size_t offset = 0,
														   const size_t size = 0) = 0;
	virtual void* __attribute__((aligned(128))) map_image(buffer_object* buffer_obj,
														  const MAP_BUFFER_FLAG access_type = (MAP_BUFFER_FLAG::READ_WRITE | MAP_BUFFER_FLAG::BLOCK),
														  const size3 origin = { 0, 0, 0 },
														  const size3 region = { 0, 0, 0 },
														  size_t* image_row_pitch = nullptr,
														  size_t* image_slice_pitch = nullptr) = 0;
	
	virtual pair<buffer_object*, void*> create_and_map_buffer(const BUFFER_FLAG type,
															  const size_t size,
															  const void* data = nullptr,
															  const MAP_BUFFER_FLAG access_type = (MAP_BUFFER_FLAG::WRITE_INVALIDATE |
																								   MAP_BUFFER_FLAG::BLOCK),
															  const size_t map_offset = 0,
															  const size_t map_size = 0) = 0;
	
	virtual void unmap_buffer(buffer_object* buffer_obj, void* map_ptr) = 0;
	
	//
	void set_manual_gl_sharing(buffer_object* gl_buffer_obj, const bool state);
	
	const vector<cl::ImageFormat>& get_image_formats() const;
	
	//! note: this is only available in opencl 1.2
	virtual void _fill_buffer(buffer_object* buffer_obj,
							  const void* pattern,
							  const size_t& pattern_size,
							  const size_t offset = 0,
							  const size_t size = 0) = 0;
	
	// for debugging purposes
	virtual void dump_buffer(buffer_object* buffer_obj,
							 const string& filename);
	
	//
	virtual void set_active_device(const DEVICE_TYPE& dev) = 0;
	virtual bool set_kernel_argument(const unsigned int& index, buffer_object* arg) = 0;
	virtual bool set_kernel_argument(const unsigned int& index, const buffer_object* arg) = 0;
	virtual bool set_kernel_argument(const unsigned int& index, size_t size, void* arg) = 0;
	template<typename T> bool set_kernel_argument(const unsigned int& index, T&& arg);
	void set_kernel_range(const pair<cl::NDRange, cl::NDRange> range);
	virtual size_t get_kernel_work_group_size() const = 0;
	
	pair<cl::NDRange, cl::NDRange> compute_kernel_ranges(const size_t& work_items) const;
	pair<cl::NDRange, cl::NDRange> compute_kernel_ranges(const size_t& work_items_x, const size_t& work_items_y) const;
	pair<cl::NDRange, cl::NDRange> compute_kernel_ranges(const size_t& work_items_x, const size_t& work_items_y, const size_t& work_items_z) const;
	
	//! this is for manual handling only
	virtual void acquire_gl_object(buffer_object* gl_buffer_obj) = 0;
	virtual void release_gl_object(buffer_object* gl_buffer_obj) = 0;
	
	struct kernel_object {
		cl::Kernel* kernel = nullptr;
		cl::Program* program = nullptr;
		cl::NDRange global { 1 };
		cl::NDRange local { 1 };
		unsigned int arg_count = 0;
		bool has_ogl_buffers = false;
		vector<bool> args_passed;
		vector<buffer_object*> buffer_args;
		string name = "";
		unordered_map<cl::CommandQueue*, cl::KernelFunctor> functors;
		atomic<bool> valid { false };
		
		kernel_object() {}
		~kernel_object() {
			if(program != nullptr) delete program;
			if(kernel != nullptr) delete kernel;
		}
		static void unassociate_buffers(shared_ptr<kernel_object> kernel_ptr) {
			for(auto& buffer : kernel_ptr->buffer_args) {
				if(buffer == nullptr) continue;
				buffer->associated_kernels.erase(kernel_ptr);
			}
		}
	};
	static shared_ptr<kernel_object> null_kernel_object;
	
	struct buffer_object {
		cl::Buffer* buffer = nullptr;
		cl::Image* image_buffer = nullptr;
		const buffer_object* parent_buffer = nullptr;
		GLuint ogl_buffer = 0;
		bool manual_gl_sharing = false;
		void* data = nullptr;
		size_t size = 0;
		BUFFER_FLAG type = BUFFER_FLAG::NONE;
		cl_mem_flags flags = 0;
		cl::ImageFormat format = cl::ImageFormat(0, 0);
		size3 image_size { 0, 0, 0 };
		// kernels + argument numbers
		unordered_map<shared_ptr<kernel_object>, vector<unsigned int>> associated_kernels;
		
		enum class IMAGE_TYPE : unsigned int {
			IMAGE_NONE,
			IMAGE_1D,
			IMAGE_2D,
			IMAGE_3D,
		};
		IMAGE_TYPE image_type = IMAGE_TYPE::IMAGE_NONE;
		
		buffer_object() {}
		~buffer_object() {}
	};
	
	struct device_object {
		cl::Device device;
		opencl_base::DEVICE_TYPE type = DEVICE_TYPE::NONE;
		opencl_base::VENDOR vendor_type = VENDOR::UNKNOWN;
		opencl_base::CL_VERSION cl_c_version = CL_VERSION::CL_1_0;
		unsigned int units = 0;
		unsigned int clock = 0;
		cl_ulong mem_size = 0;
		cl_ulong local_mem_size = 0;
		cl_ulong constant_mem_size = 0;
		cl_device_type internal_type = 0;
		string name = "";
		string vendor = "";
		string version = "";
		string driver_version = "";
		string extensions = "";
		
		cl_ulong max_alloc = 0;
		size_t max_wg_size = 0;
		size3 max_wi_sizes { 1, 1, 1 };
		size2 max_img_2d { 0, 0 };
		size3 max_img_3d { 0, 0, 0 };
		bool img_support = false;
		bool double_support = false;
		
		device_object() {}
		~device_object() {}
	};

	inline const string make_kernel_path(const string& file_name) const {
		return kernel_path_str + file_name;
	}
	
	// identifier -> { file_name, func_name, options }
	struct internal_kernel_info {
		string identifier;
		string filename;
		string func_name;
		string options;
	};
	void add_internal_kernels(const vector<internal_kernel_info>& internal_kernels);
	void remove_internal_kernels(const vector<string>& identifiers);
	
	//
	void add_global_kernel_defines(const string& defines);
	
protected:
	SDL_Window* sdl_wnd;
	bool supported = true;
	bool full_double_support = false;
	
	string build_options { "" };
	string nv_build_options { "" };
	string global_defines { "" };
	string kernel_path_str { "" };
	
	virtual buffer_object* create_buffer_object(const BUFFER_FLAG type, const void* data = nullptr) = 0;
	void load_internal_kernels();
	void destroy_kernels();
	void check_compilation(const bool ret, const string& filename);
	virtual void log_program_binary(const shared_ptr<kernel_object> kernel) = 0;
	
	bool has_vendor_device(VENDOR vendor_type);
	
	virtual string error_code_to_string(cl_int error_code) const = 0;
		
	bool check_image_origin_and_size(const buffer_object* image_obj, cl::size_t<3>& origin, cl::size_t<3>& region) const;
	
	cl::Context* context;
	cl::Platform* platform;
	PLATFORM_VENDOR platform_vendor = PLATFORM_VENDOR::UNKNOWN;
	CL_VERSION platform_cl_version = CL_VERSION::CL_1_0;
	vector<cl::Platform> platforms;
	vector<device_object*> devices;
	device_object* active_device;
	device_object* fastest_cpu;
	device_object* fastest_gpu;
	cl_int ierr;
	bool successful_internal_compilation;
	
	vector<cl::ImageFormat> img_formats;
	
	vector<buffer_object*> buffers;
	
	recursive_mutex execution_lock;
	recursive_mutex kernels_lock;
	unordered_map<string, shared_ptr<kernel_object>> kernels;
	shared_ptr<kernel_object> cur_kernel;
	
	unordered_map<const cl::Device*, cl::CommandQueue*> queues;
	
	vector<internal_kernel_info> internal_kernels;
	
};

template<typename T> bool opencl_base::set_kernel_argument(const unsigned int& index, T&& arg) {
	try {
		set_kernel_argument(index, sizeof(T), (void*)&arg);
	}
	catch(cl::Error err) {
		log_error("%s (%d: %s)!", err.what(), err.err(), error_code_to_string(err.err()));
		return false;
	}
	catch(...) {
		log_error("unknown error!");
		return false;
	}
	
	// remove "references" of the last used buffer for this kernel and argument index (if there is one)
	auto& buffer = cur_kernel->buffer_args[index];
	if(buffer != nullptr) {
		auto& kernel_args = buffer->associated_kernels[cur_kernel];
		kernel_args.erase(remove(begin(kernel_args), end(kernel_args), index),
						  end(kernel_args));
		buffer = nullptr;
	}
	return true;
}
	
// actual opencl implementation
class FLOOR_API opencl : public opencl_base {
public:
	//
	opencl(const char* kernel_path_, SDL_Window* wnd_, const bool clear_cache_);
	virtual ~opencl();
	
	virtual void init(bool use_platform_devices = false, const size_t platform_index = 0,
					  const set<string> device_restriction = set<string> {},
					  const bool gl_sharing = true);
	
	virtual void run_kernel(weak_ptr<kernel_object> kernel_obj);
	
	virtual void finish();
	virtual void flush();
	virtual void barrier();
	virtual void activate_context();
	virtual void deactivate_context();
	
	virtual weak_ptr<kernel_object> add_kernel_src(const string& identifier, const string& src, const string& func_name, const string additional_options = "");
	virtual void delete_kernel(weak_ptr<kernel_object> kernel_obj);
	
	// create
	virtual buffer_object* create_buffer(const BUFFER_FLAG type,
										 const size_t size,
										 const void* data = nullptr);
	virtual buffer_object* create_sub_buffer(const buffer_object* parent_buffer,
											 const BUFFER_FLAG type,
											 const size_t offset,
											 const size_t size);
	virtual buffer_object* create_image2d_buffer(const BUFFER_FLAG type,
												 const cl_channel_order channel_order,
												 const cl_channel_type channel_type,
												 const size_t width, const size_t height,
												 const void* data = nullptr);
	virtual buffer_object* create_image3d_buffer(const BUFFER_FLAG type,
												 const cl_channel_order channel_order,
												 const cl_channel_type channel_type,
												 const size_t width, const size_t height, const size_t depth,
												 const void* data = nullptr);
	virtual buffer_object* create_ogl_buffer(const BUFFER_FLAG type, const GLuint ogl_buffer);
	virtual buffer_object* create_ogl_image2d_buffer(const BUFFER_FLAG type,
													 const GLuint texture,
													 const GLenum target = GL_TEXTURE_2D);
	virtual buffer_object* create_ogl_image2d_renderbuffer(const BUFFER_FLAG type, const GLuint renderbuffer);
	virtual void delete_buffer(buffer_object* buffer_obj);
	
	// write
	virtual void write_buffer(buffer_object* buffer_obj, const void* src,
							  const size_t offset = 0, const size_t size = 0);
	virtual void write_buffer_rect(buffer_object* buffer_obj, const void* src,
								   const size3 buffer_origin,
								   const size3 host_origin,
								   const size3 region,
								   const size_t buffer_row_pitch = 0, const size_t buffer_slice_pitch = 0,
								   const size_t host_row_pitch = 0, const size_t host_slice_pitch = 0);
	virtual void write_image(buffer_object* buffer_obj, const void* src,
							 const size3 origin = { 0, 0, 0 },
							 const size3 region = { 0, 0, 0 });
	
	// copy
	virtual void copy_buffer(const buffer_object* src_buffer, buffer_object* dst_buffer,
							 const size_t src_offset = 0, const size_t dst_offset = 0,
							 const size_t size = 0);
	virtual void copy_buffer_rect(const buffer_object* src_buffer, buffer_object* dst_buffer,
								  const size3 src_origin, const size3 dst_origin,
								  const size3 region,
								  const size_t src_row_pitch = 0, const size_t src_slice_pitch = 0,
								  const size_t dst_row_pitch = 0, const size_t dst_slice_pitch = 0);
	virtual void copy_image(const buffer_object* src_buffer, buffer_object* dst_buffer,
							const size3 src_origin, const size3 dst_origin,
							const size3 region);
	virtual void copy_buffer_to_image(const buffer_object* src_buffer, buffer_object* dst_buffer,
									  const size_t src_offset = 0,
									  const size3 dst_origin = { 0, 0, 0 }, const size3 dst_region = { 0, 0, 0 });
	virtual void copy_image_to_buffer(const buffer_object* src_buffer, buffer_object* dst_buffer,
									  const size3 src_origin = { 0, 0, 0 }, const size3 src_region = { 0, 0, 0 },
									  const size_t dst_offset = 0);
	
	// read
	virtual void read_buffer(void* dst, const buffer_object* buffer_obj,
							 const size_t offset = 0, const size_t size = 0);
	virtual void read_buffer_rect(void* dst, const buffer_object* buffer_obj,
								  const size3 buffer_origin,
								  const size3 host_origin,
								  const size3 region,
								  const size_t buffer_row_pitch = 0, const size_t buffer_slice_pitch = 0,
								  const size_t host_row_pitch = 0, const size_t host_slice_pitch = 0);
	virtual void read_image(void* dst, const buffer_object* buffer_obj,
							const size3 origin = { 0, 0, 0 }, const size3 region = { 0, 0, 0 },
							const size_t image_row_pitch = 0,
							const size_t image_slice_pitch = 0);
	
	// map
	virtual void* __attribute__((aligned(128))) map_buffer(buffer_object* buffer_obj,
														   const MAP_BUFFER_FLAG access_type = (MAP_BUFFER_FLAG::READ_WRITE | MAP_BUFFER_FLAG::BLOCK),
														   const size_t offset = 0,
														   const size_t size = 0);
	virtual void* __attribute__((aligned(128))) map_image(buffer_object* buffer_obj,
														  const MAP_BUFFER_FLAG access_type = (MAP_BUFFER_FLAG::READ_WRITE | MAP_BUFFER_FLAG::BLOCK),
														  const size3 origin = { 0, 0, 0 },
														  const size3 region = { 0, 0, 0 },
														  size_t* image_row_pitch = nullptr,
														  size_t* image_slice_pitch = nullptr);
	
	virtual pair<buffer_object*, void*> create_and_map_buffer(const BUFFER_FLAG type,
															  const size_t size,
															  const void* data = nullptr,
															  const MAP_BUFFER_FLAG access_type = (MAP_BUFFER_FLAG::WRITE_INVALIDATE |
																								   MAP_BUFFER_FLAG::BLOCK),
															  const size_t map_offset = 0,
															  const size_t map_size = 0);
	
	virtual void unmap_buffer(buffer_object* buffer_obj, void* map_ptr);
	
	virtual void _fill_buffer(buffer_object* buffer_obj,
							  const void* pattern,
							  const size_t& pattern_size,
							  const size_t offset = 0,
							  const size_t size = 0);
	
	virtual void set_active_device(const DEVICE_TYPE& dev);
	virtual bool set_kernel_argument(const unsigned int& index, buffer_object* arg);
	virtual bool set_kernel_argument(const unsigned int& index, const buffer_object* arg);
	virtual bool set_kernel_argument(const unsigned int& index, size_t size, void* arg);
	virtual size_t get_kernel_work_group_size() const;
	
	virtual void acquire_gl_object(buffer_object* gl_buffer_obj);
	virtual void release_gl_object(buffer_object* gl_buffer_obj);
	
protected:
	virtual buffer_object* create_buffer_object(const BUFFER_FLAG type, const void* data = nullptr);
	virtual void log_program_binary(const shared_ptr<kernel_object> kernel);
	virtual string error_code_to_string(cl_int error_code) const;
	
};

#if defined(FLOOR_CUDA_CL)
// opencl_base implementation on top of cuda
struct cuda_kernel_object;
class FLOOR_API cudacl : public opencl_base {
public:
	cudacl(const char* kernel_path_, SDL_Window* wnd_, const bool clear_cache_);
	virtual ~cudacl();
	
	virtual void init(bool use_platform_devices = false, const size_t platform_index = 0,
					  const set<string> device_restriction = set<string> {},
					  const bool gl_sharing = true);
	
	virtual void run_kernel(weak_ptr<kernel_object> kernel_obj);
	
	virtual void finish();
	virtual void flush();
	virtual void barrier();
	virtual void activate_context();
	virtual void deactivate_context();
	
	virtual weak_ptr<kernel_object> add_kernel_src(const string& identifier, const string& src, const string& func_name, const string additional_options = "");
	virtual void delete_kernel(weak_ptr<kernel_object> kernel_obj);
	
	// create
	virtual buffer_object* create_buffer(const BUFFER_FLAG type,
										 const size_t size,
										 const void* data = nullptr);
	virtual buffer_object* create_sub_buffer(const buffer_object* parent_buffer,
											 const BUFFER_FLAG type,
											 const size_t offset,
											 const size_t size);
	virtual buffer_object* create_image2d_buffer(const BUFFER_FLAG type,
												 const cl_channel_order channel_order,
												 const cl_channel_type channel_type,
												 const size_t width, const size_t height,
												 const void* data = nullptr);
	virtual buffer_object* create_image3d_buffer(const BUFFER_FLAG type,
												 const cl_channel_order channel_order,
												 const cl_channel_type channel_type,
												 const size_t width, const size_t height, const size_t depth,
												 const void* data = nullptr);
	virtual buffer_object* create_ogl_buffer(const BUFFER_FLAG type, const GLuint ogl_buffer);
	virtual buffer_object* create_ogl_image2d_buffer(const BUFFER_FLAG type,
													 const GLuint texture,
													 const GLenum target = GL_TEXTURE_2D);
	virtual buffer_object* create_ogl_image2d_renderbuffer(const BUFFER_FLAG type, const GLuint renderbuffer);
	virtual void delete_buffer(buffer_object* buffer_obj);
	
	// write
	virtual void write_buffer(buffer_object* buffer_obj, const void* src,
							  const size_t offset = 0, const size_t size = 0);
	virtual void write_buffer_rect(buffer_object* buffer_obj, const void* src,
								   const size3 buffer_origin,
								   const size3 host_origin,
								   const size3 region,
								   const size_t buffer_row_pitch = 0, const size_t buffer_slice_pitch = 0,
								   const size_t host_row_pitch = 0, const size_t host_slice_pitch = 0);
	virtual void write_image(buffer_object* buffer_obj, const void* src,
							 const size3 origin = { 0, 0, 0 },
							 const size3 region = { 0, 0, 0 });
	
	// copy
	virtual void copy_buffer(const buffer_object* src_buffer, buffer_object* dst_buffer,
							 const size_t src_offset = 0, const size_t dst_offset = 0,
							 const size_t size = 0);
	virtual void copy_buffer_rect(const buffer_object* src_buffer, buffer_object* dst_buffer,
								  const size3 src_origin, const size3 dst_origin,
								  const size3 region,
								  const size_t src_row_pitch = 0, const size_t src_slice_pitch = 0,
								  const size_t dst_row_pitch = 0, const size_t dst_slice_pitch = 0);
	virtual void copy_image(const buffer_object* src_buffer, buffer_object* dst_buffer,
							const size3 src_origin, const size3 dst_origin,
							const size3 region);
	virtual void copy_buffer_to_image(const buffer_object* src_buffer, buffer_object* dst_buffer,
									  const size_t src_offset = 0,
									  const size3 dst_origin = { 0, 0, 0 }, const size3 dst_region = { 0, 0, 0 });
	virtual void copy_image_to_buffer(const buffer_object* src_buffer, buffer_object* dst_buffer,
									  const size3 src_origin = { 0, 0, 0 }, const size3 src_region = { 0, 0, 0 },
									  const size_t dst_offset = 0);
	
	// read
	virtual void read_buffer(void* dst, const buffer_object* buffer_obj,
							 const size_t offset = 0, const size_t size = 0);
	virtual void read_buffer_rect(void* dst, const buffer_object* buffer_obj,
								  const size3 buffer_origin,
								  const size3 host_origin,
								  const size3 region,
								  const size_t buffer_row_pitch = 0, const size_t buffer_slice_pitch = 0,
								  const size_t host_row_pitch = 0, const size_t host_slice_pitch = 0);
	virtual void read_image(void* dst, const buffer_object* buffer_obj,
							const size3 origin = { 0, 0, 0 }, const size3 region = { 0, 0, 0 },
							const size_t image_row_pitch = 0,
							const size_t image_slice_pitch = 0);
	
	// map
	virtual void* __attribute__((aligned(128))) map_buffer(buffer_object* buffer_obj,
														   const MAP_BUFFER_FLAG access_type = (MAP_BUFFER_FLAG::READ_WRITE | MAP_BUFFER_FLAG::BLOCK),
														   const size_t offset = 0,
														   const size_t size = 0);
	virtual void* __attribute__((aligned(128))) map_image(buffer_object* buffer_obj,
														  const MAP_BUFFER_FLAG access_type = (MAP_BUFFER_FLAG::READ_WRITE | MAP_BUFFER_FLAG::BLOCK),
														  const size3 origin = { 0, 0, 0 },
														  const size3 region = { 0, 0, 0 },
														  size_t* image_row_pitch = nullptr,
														  size_t* image_slice_pitch = nullptr);
	
	virtual pair<buffer_object*, void*> create_and_map_buffer(const BUFFER_FLAG type,
															  const size_t size,
															  const void* data = nullptr,
															  const MAP_BUFFER_FLAG access_type = (MAP_BUFFER_FLAG::WRITE_INVALIDATE |
																								   MAP_BUFFER_FLAG::BLOCK),
															  const size_t map_offset = 0,
															  const size_t map_size = 0);
	
	virtual void unmap_buffer(buffer_object* buffer_obj, void* map_ptr);
	
	virtual void _fill_buffer(buffer_object* buffer_obj,
							  const void* pattern,
							  const size_t& pattern_size,
							  const size_t offset = 0,
							  const size_t size = 0);
	
	virtual void set_active_device(const DEVICE_TYPE& dev);
	virtual bool set_kernel_argument(const unsigned int& index, buffer_object* arg);
	virtual bool set_kernel_argument(const unsigned int& index, const buffer_object* arg);
	virtual bool set_kernel_argument(const unsigned int& index, size_t size, void* arg);
	virtual size_t get_kernel_work_group_size() const;
	
	virtual void acquire_gl_object(buffer_object* gl_buffer_obj);
	virtual void release_gl_object(buffer_object* gl_buffer_obj);
	
protected:
	bool valid = true;
	string cache_path = "";
	string cc_target_str = "10";
	unsigned int cc_target = 0;
	vector<CUdevice*> cuda_devices;
	unordered_map<opencl_base::device_object*, const CUdevice*> device_map;
	unordered_map<const CUdevice*, CUcontext*> cuda_contexts;
	unordered_map<const CUdevice*, CUstream*> cuda_queues;
	unordered_map<opencl_base::buffer_object*, CUdeviceptr*> cuda_buffers;
	unordered_map<opencl_base::buffer_object*, CUarray*> cuda_images;
	unordered_map<opencl_base::buffer_object*, CUgraphicsResource*> cuda_gl_buffers;
	unordered_map<CUgraphicsResource*, CUdeviceptr*> cuda_mapped_gl_buffers;
	unordered_map<shared_ptr<opencl_base::kernel_object>, cuda_kernel_object*> cuda_kernels;
	
	// 128-bit kernel hash -> kernel identifier
	// note: this is a multimap, because there might be kernels that are identical
	// and can use the same binary (especially simple passthrough shaders)
	unordered_multimap<uint128, string> cuda_cache_hashes;
	unordered_map<string, uint128> rev_cuda_cache; // for reverse lookup
	unordered_map<uint128, string> cuda_cache_binaries; // hash -> cache file content
	
	// active (host) memory mappings
	struct cuda_mem_map_data {
		const CUdevice* device;
		const CUdeviceptr device_mem_ptr;
		opencl_base::buffer_object* buffer;
		const cl_map_flags flags;
		const size_t offset;
		const size_t size;
		const bool blocking;
	};
	unordered_map<void*, cuda_mem_map_data> cuda_mem_mappings;
	
	//
	virtual buffer_object* create_buffer_object(const BUFFER_FLAG type, const void* data = nullptr);
	virtual void log_program_binary(shared_ptr<kernel_object> kernel);
	virtual string error_code_to_string(cl_int error_code) const;
	
};
#endif

#endif
