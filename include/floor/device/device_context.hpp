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

#pragma once

#include <unordered_set>
#include <atomic>
#include <floor/core/logger.hpp>
#include <floor/device/toolchain.hpp>
#include <floor/device/device_common.hpp>
#include <floor/device/device_buffer.hpp>
#include <floor/device/device_image.hpp>
#include <floor/device/device.hpp>
#include <floor/device/device_queue.hpp>
#include <floor/device/device_program.hpp>
#include <floor/core/hdr_metadata.hpp>
#include <floor/device/device_fence.hpp>

namespace fl {

// necessary here, because there are no out-of-line virtual method definitions
FLOOR_PUSH_WARNINGS()
FLOOR_IGNORE_WARNING(weak-vtables)

class cuda_context;
class host_context;
class metal_context;
class opencl_context;
class vulkan_context;

class vulkan_buffer;
class vulkan_image;
class metal_buffer;
class metal_image;
class device_fence;

class indirect_command_pipeline;
struct indirect_command_description;

class graphics_pipeline;
struct render_pipeline_description;
class graphics_pass;
struct render_pass_description;
class graphics_renderer;

class vr_context;

//! global context flags that can be specified during context creation
enum class DEVICE_CONTEXT_FLAGS : uint32_t {
	NONE = 0u,
	
	//! Metal-only (right now): disables any automatic resource tracking on the allocated Metal object
	//! NOTE: this is achieved by automatically adding MEMORY_FLAG::NO_RESOURCE_TRACKING for all buffers/images that are created
	NO_RESOURCE_TRACKING = (1u << 0u),
	
	//! Vulkan-only: flag that disables blocking queue submission
	VULKAN_NO_BLOCKING = (1u << 1u),
	
	//! Metal/Vulkan-only: experimental option to allocate and use an internal heap for supported memory allocations
	//! NOTE: this enables the use of MEMORY_FLAG::__EXP_HEAP_ALLOC
	__EXP_INTERNAL_HEAP = (1u << 2u),
	
	//! Vulkan-only: experimental option to automatically add MEMORY_FLAG::__EXP_HEAP_ALLOC to all allocations
	//! NOTE: requires __EXP_INTERNAL_HEAP
	__EXP_VULKAN_ALWAYS_HEAP = (1u << 3u),
};
floor_global_enum_ext(DEVICE_CONTEXT_FLAGS)

//! pure abstract base class that provides the interface for all device implementations (CUDA/Host-Compute/Metal/OpenCL/Vulkan)
class device_context {
public:
	//////////////////////////////////////////
	// init / context creation
	
	//! unfortunately necessary
	virtual ~device_context() = default;
	
	//! returns true if this is a valid context (i.e. a device context could be created and available devices exist)
	virtual bool is_supported() const = 0;
	
	//! returns true if there is graphics support (i.e. the context is able to perform graphics rendering)
	//! NOTE: must still call "is_supported()" to check if this context is actually valid
	virtual bool is_graphics_supported() const = 0;
	
	//! returns true if VR rendering is supported (implies that is_supported() and is_graphics_supported() return true)
	virtual bool is_vr_supported() const { return false; }
	
	//! returns the underlying platform type
	virtual PLATFORM_TYPE get_platform_type() const = 0;
	
	//! returns the context flags that were specified during context creation
	DEVICE_CONTEXT_FLAGS get_context_flags() const {
		return context_flags;
	}
	
	//! returns true if this context can compile programs from source code at run-time
	virtual bool can_compile_programs() const {
		return has_toolchain;
	}
	
	//////////////////////////////////////////
	// device functions
	
	//! returns the array of all valid devices in this context
	std::vector<const device*> get_devices() const;
	
	//! tries to return the device matching the specified "type"
	//! NOTE: will return any valid device if none matches "type" or nullptr if no device exists
	const device* get_device(const device::TYPE type) const;
	
	//! returns the device in this context corresponding to the specified "external_dev" device in a different context,
	//! if no match is found, returns nullptr
	const device* get_corresponding_device(const device& external_dev) const;
	
	//! creates and returns a device_queue (aka command queue or stream) for the specified device
	virtual std::shared_ptr<device_queue> create_queue(const device& dev) const = 0;
	
	//! returns the internal default device_queue for the specified device
	virtual const device_queue* get_device_default_queue(const device& dev) const = 0;
	
	//! create a compute-only queue for the specified device
	//! NOTE: this is only relevant on backends that a) offer graphics supports and b) offer compute-only queues
	virtual std::shared_ptr<device_queue> create_compute_queue(const device& dev) const {
		return create_queue(dev);
	}
	
	//! returns the internal default compute-only device_queue for the specified device
	virtual const device_queue* get_device_default_compute_queue(const device& dev) const {
		return get_device_default_queue(dev);
	}
	
	//! returns the max amount of distinct queues that can be created by the context for the specified device,
	//! returns empty if there is no particular max amount
	virtual std::optional<uint32_t> get_max_distinct_queue_count(const device&) const {
		return {};
	}
	
	//! returns the max amount of distinct compute-only queues that can be created by the context for the specified device,
	//! returns empty if there is no particular max amount
	virtual std::optional<uint32_t> get_max_distinct_compute_queue_count(const device&) const {
		return {};
	}
	
	//! creates up to "wanted_count" number of device_queues for the specified device "dev",
	//! for backends that only support a certain amount of distinct queues, this will create/return distinct queues from that pool,
	//! with the returned number of created queues limited to min(wanted_count, get_max_distinct_queue_count())
	virtual std::vector<std::shared_ptr<device_queue>> create_distinct_queues(const device& dev, const uint32_t wanted_count) const;
	
	//! creates up to "wanted_count" number of compute-only device_queues for the specified device "dev",
	//! for backends that only support a certain amount of distinct compute-only queues, this will create/return distinct queues from that pool,
	//! with the returned number of created queues limited to min(wanted_count, get_max_distinct_compute_queue_count())
	virtual std::vector<std::shared_ptr<device_queue>> create_distinct_compute_queues(const device& dev, const uint32_t wanted_count) const;
	
	//! creates and returns a fence for the specified queue
	virtual std::unique_ptr<device_fence> create_fence(const device_queue& cqueue) const = 0;
	
	//! memory usage returned by get_memory_usage()
	struct memory_usage_t {
		//! current amount of used global memory in bytes
		uint64_t global_mem_used { 0u };
		//! total available amount of global memory in bytes
		uint64_t global_mem_total { 0u };
		//! current amount of used heap memory in bytes
		uint64_t heap_used { 0u };
		//! total available amount of heap memory in bytes
		uint64_t heap_total { 0u };
		
		//! returns the global memory usage as a percentage
		double global_mem_usage_percentage() const {
			return (global_mem_total > 0 ? (double(global_mem_used) / double(global_mem_total)) * 100.0 : 0.0);
		}
		//! returns the heap memory usage as a percentage
		double heap_usage_percentage() const {
			return (heap_total > 0 ? (double(heap_used) / double(heap_total)) * 100.0 : 0.0);
		}
	};
	
	//! return the current memory usage for the specified device
	virtual memory_usage_t get_memory_usage(const device& dev) const = 0;
	
	//////////////////////////////////////////
	// buffer creation
	
	//! constructs an uninitialized buffer of the specified size on the specified device
	virtual std::shared_ptr<device_buffer> create_buffer(const device_queue& cqueue,
													 const size_t& size,
													 const MEMORY_FLAG flags = (MEMORY_FLAG::READ_WRITE |
																						MEMORY_FLAG::HOST_READ_WRITE)) const = 0;
	
	//! constructs a buffer of the specified size, using the host pointer as specified by the flags on the specified device
	virtual std::shared_ptr<device_buffer> create_buffer(const device_queue& cqueue,
													 std::span<uint8_t> data,
													 const MEMORY_FLAG flags = (MEMORY_FLAG::READ_WRITE |
																						MEMORY_FLAG::HOST_READ_WRITE)) const = 0;
	
	//! constructs a buffer of the specified size, using the host pointer as specified by the flags on the specified device
	template <typename data_type>
	std::shared_ptr<device_buffer> create_buffer(const device_queue& cqueue,
											 std::span<data_type> data,
											 const MEMORY_FLAG flags = (MEMORY_FLAG::READ_WRITE |
																				MEMORY_FLAG::HOST_READ_WRITE)) const {
		return create_buffer(cqueue, { reinterpret_cast<uint8_t*>(const_cast<std::remove_const_t<data_type>*>(data.data())), data.size_bytes() }, flags);
	}
	
	//! constructs a buffer of the specified data (under consideration of the specified flags) on the specified device
	template <typename data_type>
	std::shared_ptr<device_buffer> create_buffer(const device_queue& cqueue,
											 const std::vector<data_type>& data,
											 const MEMORY_FLAG flags = (MEMORY_FLAG::READ_WRITE |
																				MEMORY_FLAG::HOST_READ_WRITE)) const {
		return create_buffer(cqueue, { (uint8_t*)const_cast<data_type*>(data.data()), sizeof(data_type) * data.size() }, flags);
	}
	
	//! constructs a buffer of the specified data (under consideration of the specified flags) on the specified device
	template <typename data_type, size_t n>
	std::shared_ptr<device_buffer> create_buffer(const device_queue& cqueue,
											 const std::array<data_type, n>& data,
											 const MEMORY_FLAG flags = (MEMORY_FLAG::READ_WRITE |
																				MEMORY_FLAG::HOST_READ_WRITE)) const {
		return create_buffer(cqueue, { (uint8_t*)const_cast<data_type*>(data.data()), sizeof(data_type) * n }, flags);
	}
	
	//! wraps an already existing Vulkan buffer, with the specified flags
	//! NOTE: VULKAN_SHARING flag is always implied
	virtual std::shared_ptr<device_buffer> wrap_buffer(const device_queue& cqueue,
												   vulkan_buffer& vk_buffer,
												   const MEMORY_FLAG flags = (MEMORY_FLAG::READ_WRITE |
																					  MEMORY_FLAG::HOST_READ_WRITE)) const;
	
	//! wraps an already existing Metal buffer, with the specified flags
	//! NOTE: METAL_SHARING flag is always implied
	virtual std::shared_ptr<device_buffer> wrap_buffer(const device_queue& cqueue,
												   metal_buffer& mtl_buffer,
												   const MEMORY_FLAG flags = (MEMORY_FLAG::READ_WRITE |
																					  MEMORY_FLAG::HOST_READ_WRITE)) const;
	
	//////////////////////////////////////////
	// image creation
	
	//! constructs an image of the specified dimensions, types and channel count, with the specified data on the specified device
	virtual std::shared_ptr<device_image> create_image(const device_queue& cqueue,
												   const uint4 image_dim,
												   const IMAGE_TYPE image_type,
												   std::span<uint8_t> data,
												   const MEMORY_FLAG flags = (MEMORY_FLAG::HOST_READ_WRITE),
												   const uint32_t mip_level_limit = 0u) const = 0;
	
	//! constructs an uninitialized image of the specified dimensions, types and channel count on the specified device
	std::shared_ptr<device_image> create_image(const device_queue& cqueue,
										   const uint4 image_dim,
										   const IMAGE_TYPE image_type,
										   const MEMORY_FLAG flags = (MEMORY_FLAG::HOST_READ_WRITE),
										   const uint32_t mip_level_limit = 0u) const {
		return create_image(cqueue, image_dim, image_type, std::span<uint8_t> {}, flags, mip_level_limit);
	}
	
	//! constructs an image of the specified dimensions, types and channel count, with the specified data on the specified device
	template <typename data_type>
	std::shared_ptr<device_image> create_image(const device_queue& cqueue,
										   const uint4 image_dim,
										   const IMAGE_TYPE image_type,
										   std::span<data_type> data,
										   const MEMORY_FLAG flags = (MEMORY_FLAG::HOST_READ_WRITE),
										   const uint32_t mip_level_limit = 0u) const {
		return create_image(cqueue, image_dim, image_type,
							{ reinterpret_cast<uint8_t*>(const_cast<std::remove_const_t<data_type>*>(data.data())), data.size_bytes() },
							flags, mip_level_limit);
	}
	
	//! constructs an image of the specified dimensions, types and channel count, with the specified data on the specified device
	template <typename data_type>
	std::shared_ptr<device_image> create_image(const device_queue& cqueue,
										   const uint4 image_dim,
										   const IMAGE_TYPE image_type,
										   const std::vector<data_type>& data,
										   const MEMORY_FLAG flags = (MEMORY_FLAG::HOST_READ_WRITE),
										   const uint32_t mip_level_limit = 0u) const {
		return create_image(cqueue, image_dim, image_type, { (uint8_t*)const_cast<data_type*>(data.data()), data.size() * sizeof(data_type) },
							flags, mip_level_limit);
	}
	
	//! constructs an image of the specified dimensions, types and channel count, with the specified data on the specified device
	template <typename data_type, size_t n>
	std::shared_ptr<device_image> create_image(const device_queue& cqueue,
										   const uint4 image_dim,
										   const IMAGE_TYPE image_type,
										   const std::array<data_type, n>& data,
										   const MEMORY_FLAG flags = (MEMORY_FLAG::HOST_READ_WRITE),
										   const uint32_t mip_level_limit = 0u) const {
		return create_image(cqueue, image_dim, image_type, { (uint8_t*)const_cast<data_type*>(data.data()), n * sizeof(data_type) },
							flags, mip_level_limit);
	}
	
	//! wraps an already existing Vulkan image, with the specified flags
	//! NOTE: VULKAN_SHARING flag is always implied
	virtual std::shared_ptr<device_image> wrap_image(const device_queue& cqueue,
												 vulkan_image& vk_image,
												 const MEMORY_FLAG flags = (MEMORY_FLAG::READ_WRITE |
																					MEMORY_FLAG::HOST_READ_WRITE)) const;
	
	//! wraps an already existing Metal image, with the specified flags
	//! NOTE: METAL_SHARING flag is always implied
	virtual std::shared_ptr<device_image> wrap_image(const device_queue& cqueue,
												 metal_image& mtl_image,
												 const MEMORY_FLAG flags = (MEMORY_FLAG::READ_WRITE |
																					MEMORY_FLAG::HOST_READ_WRITE)) const;
	
	// TODO: add is_image_format_supported(...) function
	
	//////////////////////////////////////////
	// program/function functionality
	
	//! adds a pre-compiled universal binary (loaded from a file)
	virtual std::shared_ptr<device_program> add_universal_binary(const std::string& file_name) = 0;
	
	//! adds a pre-compiled universal binary (provided as in-memory data)
	virtual std::shared_ptr<device_program> add_universal_binary(const std::span<const uint8_t> data) = 0;
	
	//! alias the toolchain compile_options (for now)
	using compile_options = toolchain::compile_options;
	
	//! adds and compiles a program and its functions from a file
	virtual std::shared_ptr<device_program> add_program_file(const std::string& file_name,
														 const std::string additional_options) = 0;
	
	//! adds and compiles a program and its functions from a file
	virtual std::shared_ptr<device_program> add_program_file(const std::string& file_name,
														 compile_options options = {}) = 0;
	
	//! adds and compiles a program and its functions from the provided source code
	virtual std::shared_ptr<device_program> add_program_source(const std::string& source_code,
														   const std::string additional_options = "") = 0;
	
	//! adds and compiles a program and its functions from the provided source code
	virtual std::shared_ptr<device_program> add_program_source(const std::string& source_code,
														   compile_options options = {}) = 0;
	
	//! adds a precompiled program and its functions, using the provided file name and function infos
	virtual std::shared_ptr<device_program> add_precompiled_program_file(const std::string& file_name,
																	 const std::vector<toolchain::function_info>& functions) = 0;
	
	//! creates a program entry from pre-existing program data and function information on the specified device
	//! NOTE: this is intended for rolling custom or semi-custom compilation, for normal code use the add_program_* functions
	//! NOTE: this usually leads to final program compilation on most platforms (but not all!)
	virtual std::shared_ptr<device_program::program_entry> create_program_entry(const device& dev,
																			toolchain::program_data program,
																			const toolchain::TARGET target) = 0;
	
	//////////////////////////////////////////
	// execution functionality
	
	//! creates an indirect compute/render command pipeline from the specified description
	//! NOTE: only supported when the context has any devices with support for either indirect compute or rendering
	virtual std::unique_ptr<indirect_command_pipeline> create_indirect_command_pipeline(const indirect_command_description& desc) const;
	
	//////////////////////////////////////////
	// graphics functionality
	
	//! creates a graphics render pipeline with the specified description
	//! if "with_multi_view_support" is false, neither manual nor automatic multi-view support will be enabled
	//! NOTE: only available on backends with graphics support
	virtual std::unique_ptr<graphics_pipeline> create_graphics_pipeline(const render_pipeline_description& pipeline_desc,
																   const bool with_multi_view_support = true) const;
	
	//! creates a graphics render pass with the specified description
	//! if "with_multi_view_support" is false, neither manual nor automatic multi-view support will be enabled
	//! NOTE: only available on backends with graphics support
	virtual std::unique_ptr<graphics_pass> create_graphics_pass(const render_pass_description& pass_desc,
														   const bool with_multi_view_support = true) const;
	
	//! creates a graphics renderer
	//! NOTE: only available on backends with graphics support
	virtual std::unique_ptr<graphics_renderer> create_graphics_renderer(const device_queue& cqueue,
																   const graphics_pass& pass,
																   const graphics_pipeline& pipeline,
																   const bool create_multi_view_renderer = false) const;
	
	//! returns the underlying image type (pixel format) of the renderer/screen
	virtual IMAGE_TYPE get_renderer_image_type() const;
	
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
	virtual std::weak_ptr<device_memory> get_memory_from_resource_registry(const std::string& label) REQUIRES(!resource_registry_lock);
	
	//! returns a vector of resource labels of all currently registered resources
	virtual std::vector<std::string> get_resource_registry_keys() const REQUIRES(!resource_registry_lock);
	
	//! returns a vector of weak pointers to all currently registered resources
	virtual std::vector<std::weak_ptr<device_memory>> get_resource_registry_weak_resources() const REQUIRES(!resource_registry_lock);
	
protected:
	device_context(const DEVICE_CONTEXT_FLAGS context_flags_, const bool has_toolchain_) :
	context_flags(context_flags_), has_toolchain(has_toolchain_) {}
	
	//! platform vendor enum (set after initialization)
	VENDOR platform_vendor { VENDOR::UNKNOWN };
	
	//! context flags that were specified during creation
	const DEVICE_CONTEXT_FLAGS context_flags { DEVICE_CONTEXT_FLAGS::NONE };
	
	//! true if compute is supported (set after initialization)
	bool supported { false };
	
	//! true if a toolchain for the specific backend exists
	bool has_toolchain { false };
	
	//! all devices of the current context
	std::vector<std::unique_ptr<device>> devices;
	//! pointer to the fastest (any) device if it exists
	const device* fastest_device { nullptr };
	//! pointer to the fastest CPU device if it exists
	const device* fastest_cpu_device { nullptr };
	//! pointer to the fastest GPU device if it exists
	const device* fastest_gpu_device { nullptr };
	
	//! all device queues of the current context
	mutable std::vector<std::shared_ptr<device_queue>> queues;
	
	//! current HDR metadata
	hdr_metadata_t hdr_metadata {};
	
	//! device_memory must be able to access resource registry functionality
	friend device_memory;
	//! access to resource registry objects must be thread-safe
	mutable safe_mutex resource_registry_lock;
	//! "label" -> "memory ptr" resource registry
	std::unordered_map<std::string, std::weak_ptr<device_memory>> resource_registry GUARDED_BY(resource_registry_lock);
	//! "memory ptr" -> "label" reverse resource registry
	std::unordered_map<const device_memory*, std::string> resource_registry_reverse GUARDED_BY(resource_registry_lock);
	//! "memory ptr" -> weak "memory ptr" lookup table
	mutable std::unordered_map<const device_memory*, std::weak_ptr<device_memory>> resource_registry_ptr_lut GUARDED_BY(resource_registry_lock);
	//! updates a resource registry entry for the specified "ptr", changing the label from "prev_label" to "label"
	void update_resource_registry(const device_memory* ptr, const std::string& prev_label, const std::string& label) REQUIRES(!resource_registry_lock);
	//! removes a resource from the resource registry
	void remove_from_resource_registry(const device_memory* ptr) REQUIRES(!resource_registry_lock);
	//! flag whether the resource registry is active
	std::atomic<bool> resource_registry_enabled { false };
	//! adds a resource to the registry (or nop/pass-through if inactive)
	template <typename resource_type>
	requires (std::is_base_of_v<device_memory, resource_type>)
	std::shared_ptr<resource_type> add_resource(std::shared_ptr<resource_type> resource) const REQUIRES(!resource_registry_lock) {
		if (resource_registry_enabled) {
			GUARD(resource_registry_lock);
			resource_registry_ptr_lut.emplace(resource.get(), resource);
		}
		return resource;
	}
	
};

FLOOR_POP_WARNINGS()

} // namespace fl
