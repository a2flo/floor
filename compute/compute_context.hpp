/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2023 Florian Ziesche
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

#ifndef __FLOOR_COMPUTE_CONTEXT_HPP__
#define __FLOOR_COMPUTE_CONTEXT_HPP__

#include <unordered_set>

#include <floor/core/logger.hpp>
#include <floor/compute/llvm_toolchain.hpp>
#include <floor/compute/compute_common.hpp>
#include <floor/compute/compute_buffer.hpp>
#include <floor/compute/compute_image.hpp>
#include <floor/compute/compute_device.hpp>
#include <floor/compute/compute_queue.hpp>
#include <floor/compute/compute_program.hpp>
#include <floor/compute/hdr_metadata.hpp>
#include <floor/compute/compute_fence.hpp>

// necessary here, because there are no out-of-line virtual method definitions
FLOOR_PUSH_WARNINGS()
FLOOR_IGNORE_WARNING(weak-vtables)

class cuda_compute;
class host_compute;
class metal_compute;
class opengl_compute;
class vulkan_compute;

class vulkan_buffer;
class vulkan_image;
class metal_buffer;
class metal_image;
class compute_fence;

class indirect_command_pipeline;
struct indirect_command_description;

class graphics_pipeline;
struct render_pipeline_description;
class graphics_pass;
struct render_pass_description;
class graphics_renderer;

class vr_context;

//! pure abstract base class that provides the interface for all compute implementations (opencl, cuda, ...)
class compute_context {
public:
	//////////////////////////////////////////
	// init / context creation
	
	//! unfortunately necessary
	virtual ~compute_context() = default;
	
	//! returns true if this is a valid context (i.e. a compute context could be created and available compute devices exist)
	virtual bool is_supported() const = 0;
	
	//! returns true if there is graphics support (i.e. the context is able to perform graphics rendering)
	//! NOTE: must still call "is_supported()" to check if this context is actually valid
	virtual bool is_graphics_supported() const = 0;

	//! returns true if VR rendering is supported (implies that is_supported() and is_graphics_supported() return true)
	virtual bool is_vr_supported() const { return false; }
	
	//! returns the underlying compute implementation type
	virtual COMPUTE_TYPE get_compute_type() const = 0;
	
	//////////////////////////////////////////
	// device functions
	
	//! returns the array of all valid devices in this context
	vector<const compute_device*> get_devices() const;
	
	//! tries to return the device matching the specified "type"
	//! NOTE: will return any valid device if none matches "type" or nullptr if no device exists
	const compute_device* get_device(const compute_device::TYPE type) const;
	
	//! returns the device in this context corresponding to the specified "external_dev" device in a different context,
	//! if no match is found, returns nullptr
	const compute_device* get_corresponding_device(const compute_device& external_dev) const;
	
	//! creates and returns a compute_queue (aka command queue or stream) for the specified device
	virtual shared_ptr<compute_queue> create_queue(const compute_device& dev) const = 0;
	
	//! returns the internal default compute_queue for the specified device
	virtual const compute_queue* get_device_default_queue(const compute_device& dev) const = 0;
	
	//! creates and returns a fence for the specified queue
	virtual unique_ptr<compute_fence> create_fence(const compute_queue& cqueue) const = 0;
	
	//////////////////////////////////////////
	// buffer creation
	
	//! constructs an uninitialized buffer of the specified size on the specified device
	virtual shared_ptr<compute_buffer> create_buffer(const compute_queue& cqueue,
													 const size_t& size,
													 const COMPUTE_MEMORY_FLAG flags = (COMPUTE_MEMORY_FLAG::READ_WRITE |
																						COMPUTE_MEMORY_FLAG::HOST_READ_WRITE),
													 const uint32_t opengl_type = 0) const = 0;
	
	//! constructs a buffer of the specified size, using the host pointer as specified by the flags on the specified device
	virtual shared_ptr<compute_buffer> create_buffer(const compute_queue& cqueue,
													 const size_t& size,
													 void* data,
													 const COMPUTE_MEMORY_FLAG flags = (COMPUTE_MEMORY_FLAG::READ_WRITE |
																						COMPUTE_MEMORY_FLAG::HOST_READ_WRITE),
													 const uint32_t opengl_type = 0) const = 0;
	
	//! constructs a buffer of the specified data (under consideration of the specified flags) on the specified device
	template <typename data_type>
	shared_ptr<compute_buffer> create_buffer(const compute_queue& cqueue,
											 const vector<data_type>& data,
											 const COMPUTE_MEMORY_FLAG flags = (COMPUTE_MEMORY_FLAG::READ_WRITE |
																				COMPUTE_MEMORY_FLAG::HOST_READ_WRITE),
											 const uint32_t opengl_type = 0) const {
		return create_buffer(cqueue, sizeof(data_type) * data.size(), (void*)&data[0], flags, opengl_type);
	}
	
	//! constructs a buffer of the specified data (under consideration of the specified flags) on the specified device
	template <typename data_type, size_t n>
	shared_ptr<compute_buffer> create_buffer(const compute_queue& cqueue,
											 const array<data_type, n>& data,
											 const COMPUTE_MEMORY_FLAG flags = (COMPUTE_MEMORY_FLAG::READ_WRITE |
																				COMPUTE_MEMORY_FLAG::HOST_READ_WRITE),
											 const uint32_t opengl_type = 0) const {
		return create_buffer(cqueue, sizeof(data_type) * n, (void*)&data[0], flags, opengl_type);
	}
	
	//! wraps an already existing opengl buffer, with the specified flags
	//! NOTE: OPENGL_SHARING flag is always implied
	virtual shared_ptr<compute_buffer> wrap_buffer(const compute_queue& cqueue,
												   const uint32_t opengl_buffer,
												   const uint32_t opengl_type,
												   const COMPUTE_MEMORY_FLAG flags = (COMPUTE_MEMORY_FLAG::READ_WRITE |
																					  COMPUTE_MEMORY_FLAG::HOST_READ_WRITE)) const = 0;
	
	//! wraps an already existing opengl buffer, with the specified flags and backed by the specified host pointer
	//! NOTE: OPENGL_SHARING flag is always implied
	virtual shared_ptr<compute_buffer> wrap_buffer(const compute_queue& cqueue,
												   const uint32_t opengl_buffer,
												   const uint32_t opengl_type,
												   void* data,
												   const COMPUTE_MEMORY_FLAG flags = (COMPUTE_MEMORY_FLAG::READ_WRITE |
																					  COMPUTE_MEMORY_FLAG::HOST_READ_WRITE)) const = 0;
	
	//! wraps an already existing Vulkan buffer, with the specified flags
	//! NOTE: VULKAN_SHARING flag is always implied
	virtual shared_ptr<compute_buffer> wrap_buffer(const compute_queue& cqueue,
												   vulkan_buffer& vk_buffer,
												   const COMPUTE_MEMORY_FLAG flags = (COMPUTE_MEMORY_FLAG::READ_WRITE |
																					  COMPUTE_MEMORY_FLAG::HOST_READ_WRITE)) const;
	
	//! wraps an already existing Metal buffer, with the specified flags
	//! NOTE: METAL_SHARING flag is always implied
	virtual shared_ptr<compute_buffer> wrap_buffer(const compute_queue& cqueue,
												   metal_buffer& mtl_buffer,
												   const COMPUTE_MEMORY_FLAG flags = (COMPUTE_MEMORY_FLAG::READ_WRITE |
																					  COMPUTE_MEMORY_FLAG::HOST_READ_WRITE)) const;
	
	//////////////////////////////////////////
	// image creation
	
	//! constructs an uninitialized image of the specified dimensions, types and channel count on the specified device
	virtual shared_ptr<compute_image> create_image(const compute_queue& cqueue,
												   const uint4 image_dim,
												   const COMPUTE_IMAGE_TYPE image_type,
												   const COMPUTE_MEMORY_FLAG flags = (COMPUTE_MEMORY_FLAG::HOST_READ_WRITE),
												   const uint32_t opengl_type = 0) const = 0;
	
	//! constructs an image of the specified dimensions, types and channel count on the specified device
	virtual shared_ptr<compute_image> create_image(const compute_queue& cqueue,
												   const uint4 image_dim,
												   const COMPUTE_IMAGE_TYPE image_type,
												   void* data,
												   const COMPUTE_MEMORY_FLAG flags = (COMPUTE_MEMORY_FLAG::HOST_READ_WRITE),
												   const uint32_t opengl_type = 0) const = 0;
	
	//! constructs an image of the specified dimensions, types and channel count, with the specified data on the specified device
	template <typename data_type>
	shared_ptr<compute_image> create_image(const compute_queue& cqueue,
										   const uint4 image_dim,
										   const COMPUTE_IMAGE_TYPE image_type,
										   const vector<data_type>& data,
										   const COMPUTE_MEMORY_FLAG flags = (COMPUTE_MEMORY_FLAG::HOST_READ_WRITE),
										   const uint32_t opengl_type = 0) const {
		return create_image(cqueue, image_dim, image_type, (void*)&data[0], flags, opengl_type);
	}
	
	//! constructs an image of the specified dimensions, types and channel count, with the specified data on the specified device
	template <typename data_type, size_t n>
	shared_ptr<compute_image> create_image(const compute_queue& cqueue,
										   const uint4 image_dim,
										   const COMPUTE_IMAGE_TYPE image_type,
										   const array<data_type, n>& data,
										   const COMPUTE_MEMORY_FLAG flags = (COMPUTE_MEMORY_FLAG::HOST_READ_WRITE),
										   const uint32_t opengl_type = 0) const {
		return create_image(cqueue, image_dim, image_type, (void*)&data[0], flags, opengl_type);
	}
	
	//! wraps an already existing opengl image, with the specified flags
	//! NOTE: OPENGL_SHARING flag is always implied
	virtual shared_ptr<compute_image> wrap_image(const compute_queue& cqueue,
												 const uint32_t opengl_image,
												 const uint32_t opengl_target,
												 const COMPUTE_MEMORY_FLAG flags = (COMPUTE_MEMORY_FLAG::READ_WRITE |
																					COMPUTE_MEMORY_FLAG::HOST_READ_WRITE)) const = 0;
	
	//! wraps an already existing opengl image, with the specified flags and backed by the specified host pointer
	//! NOTE: OPENGL_SHARING flag is always implied
	virtual shared_ptr<compute_image> wrap_image(const compute_queue& cqueue,
												 const uint32_t opengl_image,
												 const uint32_t opengl_target,
												 void* data,
												 const COMPUTE_MEMORY_FLAG flags = (COMPUTE_MEMORY_FLAG::READ_WRITE |
																					COMPUTE_MEMORY_FLAG::HOST_READ_WRITE)) const = 0;
	
	//! wraps an already existing Vulkan image, with the specified flags
	//! NOTE: VULKAN_SHARING flag is always implied
	virtual shared_ptr<compute_image> wrap_image(const compute_queue& cqueue,
												 vulkan_image& vk_image,
												 const COMPUTE_MEMORY_FLAG flags = (COMPUTE_MEMORY_FLAG::READ_WRITE |
																					COMPUTE_MEMORY_FLAG::HOST_READ_WRITE)) const;
	
	//! wraps an already existing Metal image, with the specified flags
	//! NOTE: METAL_SHARING flag is always implied
	virtual shared_ptr<compute_image> wrap_image(const compute_queue& cqueue,
												 metal_image& mtl_image,
												 const COMPUTE_MEMORY_FLAG flags = (COMPUTE_MEMORY_FLAG::READ_WRITE |
																					COMPUTE_MEMORY_FLAG::HOST_READ_WRITE)) const;
	
	// TODO: add is_image_format_supported(...) function
	
	//////////////////////////////////////////
	// program/function functionality
	
	//! adds a pre-compiled universal binary (loaded from a file)
	virtual shared_ptr<compute_program> add_universal_binary(const string& file_name) = 0;
	
	//! alias the llvm_toolchain compile_options (for now)
	using compile_options = llvm_toolchain::compile_options;
	
	//! adds and compiles a program and its functions from a file
	virtual shared_ptr<compute_program> add_program_file(const string& file_name,
														 const string additional_options) = 0;
	
	//! adds and compiles a program and its functions from a file
	virtual shared_ptr<compute_program> add_program_file(const string& file_name,
														 compile_options options = {}) = 0;
	
	//! adds and compiles a program and its functions from the provided source code
	virtual shared_ptr<compute_program> add_program_source(const string& source_code,
														   const string additional_options = "") = 0;
	
	//! adds and compiles a program and its functions from the provided source code
	virtual shared_ptr<compute_program> add_program_source(const string& source_code,
														   compile_options options = {}) = 0;
	
	//! adds a precompiled program and its functions, using the provided file name and function infos
	virtual shared_ptr<compute_program> add_precompiled_program_file(const string& file_name,
																	 const vector<llvm_toolchain::function_info>& functions) = 0;
	
	//! creates a program entry from pre-existing program data and function information on the specified device
	//! NOTE: this is intended for rolling custom or semi-custom compilation, for normal code use the add_program_* functions
	//! NOTE: this usually leads to final program compilation on most compute platforms (but not all!)
	virtual shared_ptr<compute_program::program_entry> create_program_entry(const compute_device& device,
																			llvm_toolchain::program_data program,
																			const llvm_toolchain::TARGET target) = 0;
	
	//////////////////////////////////////////
	// execution functionality
	
	//! creates an indirect compute/render command pipeline from the specified description
	//! NOTE: only supported when the context has any devices with support for either indirect compute or rendering
	virtual unique_ptr<indirect_command_pipeline> create_indirect_command_pipeline(const indirect_command_description& desc) const = 0;
	
	//////////////////////////////////////////
	// graphics functionality
	
	//! creates a graphics render pipeline with the specified description
	//! if "with_multi_view_support" is false, neither manual nor automatic multi-view support will be enabled
	//! NOTE: only available on backends with graphics support
	virtual unique_ptr<graphics_pipeline> create_graphics_pipeline(const render_pipeline_description& pipeline_desc,
																   const bool with_multi_view_support = true) const;
	
	//! creates a graphics render pass with the specified description
	//! if "with_multi_view_support" is false, neither manual nor automatic multi-view support will be enabled
	//! NOTE: only available on backends with graphics support
	virtual unique_ptr<graphics_pass> create_graphics_pass(const render_pass_description& pass_desc,
														   const bool with_multi_view_support = true) const;
	
	//! creates a graphics renderer
	//! NOTE: only available on backends with graphics support
	virtual unique_ptr<graphics_renderer> create_graphics_renderer(const compute_queue& cqueue,
																   const graphics_pass& pass,
																   const graphics_pipeline& pipeline,
																   const bool create_multi_view_renderer = false) const;
	
	//! returns the underlying image type (pixel format) of the renderer/screen
	virtual COMPUTE_IMAGE_TYPE get_renderer_image_type() const;
	
	//! returns the image dim of the renderer/screen as (width, height, layers, _unused)
	virtual uint4 get_renderer_image_dim() const;
	
	//! returns the associated VR context of the renderer (if the renderer supports VR and VR is enabled)
	virtual vr_context* get_renderer_vr_context() const;
	
	//! replaces the current HDR metadata with the specified metadata
	virtual void set_hdr_metadata(const hdr_metadata_t& hdr_metadata_);
	
	//! returns the currently active HDR metadata
	virtual const hdr_metadata_t& get_hdr_metadata() const;
	
	//! returns the currently active HDR luminance min/max range
	virtual const float2& get_hdr_luminance_range() const;
	
	//! returns the current max possible/representable value of the renderer (defaults to 1.0)
	virtual float get_hdr_range_max() const;
	
	//! returns the current max nits of the display that is used for rendering (defaults to 80 nits)
	virtual float get_hdr_display_max_nits() const;
	
	//////////////////////////////////////////
	// resource registry functionality
	
	//! enables the resource registry functionality
	//! NOTE: only resources created *after* calling this will be available in the registry
	virtual void enable_resource_registry();
	
	//! retrieves a resource from the registry
	virtual weak_ptr<compute_memory> get_memory_from_resource_registry(const string& label) REQUIRES(!resource_registry_lock);
	
	//! returns a vector of resource labels of all currently registered resources
	virtual vector<string> get_resource_registry_keys() const REQUIRES(!resource_registry_lock);
	
protected:
	//! platform vendor enum (set after initialization)
	COMPUTE_VENDOR platform_vendor { COMPUTE_VENDOR::UNKNOWN };
	
	//! true if compute support (set after initialization)
	bool supported { false };
	
	//! all compute devices of the current compute context
	vector<unique_ptr<compute_device>> devices;
	//! pointer to the fastest (any) compute_device if it exists
	const compute_device* fastest_device { nullptr };
	//! pointer to the fastest cpu compute_device if it exists
	const compute_device* fastest_cpu_device { nullptr };
	//! pointer to the fastest gpu compute_device if it exists
	const compute_device* fastest_gpu_device { nullptr };
	
	//! all compute queues of the current compute context
	mutable vector<shared_ptr<compute_queue>> queues;
	
	//! current HDR metadata
	hdr_metadata_t hdr_metadata {};
	
	//! compute_memory must be able to access resource registry functionality
	friend compute_memory;
	//! access to resource registry objects must be thread-safe
	mutable safe_mutex resource_registry_lock;
	//! "label" -> "memory ptr" resource registry
	unordered_map<string, weak_ptr<compute_memory>> resource_registry GUARDED_BY(resource_registry_lock);
	//! "memory ptr" -> "label" reverse resource registry
	unordered_map<const compute_memory*, string> resource_registry_reverse GUARDED_BY(resource_registry_lock);
	//! "memory ptr" -> weak "memory ptr" lookup table
	mutable unordered_map<const compute_memory*, weak_ptr<compute_memory>> resource_registry_ptr_lut GUARDED_BY(resource_registry_lock);
	//! updates a resource registry entry for the specified "ptr", changing the label from "prev_label" to "label"
	void update_resource_registry(const compute_memory* ptr, const string& prev_label, const string& label) REQUIRES(!resource_registry_lock);
	//! removes a resource from the resource registry
	void remove_from_resource_registry(const compute_memory* ptr) REQUIRES(!resource_registry_lock);
	//! flag whether the resource registry is active
	atomic<bool> resource_registry_enabled { false };
	//! adds a resource to the registry (or nop/pass-through if inactive)
	template <typename resource_type>
	requires (is_base_of_v<compute_memory, resource_type>)
	shared_ptr<resource_type> add_resource(shared_ptr<resource_type> resource) const REQUIRES(!resource_registry_lock) {
		if (resource_registry_enabled) {
			GUARD(resource_registry_lock);
			resource_registry_ptr_lut.emplace(resource.get(), resource);
		}
		return resource;
	}
	
};

FLOOR_POP_WARNINGS()

#endif
