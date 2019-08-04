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

#ifndef __FLOOR_VULKAN_COMPUTE_HPP__
#define __FLOOR_VULKAN_COMPUTE_HPP__

#include <floor/compute/vulkan/vulkan_common.hpp>

#if !defined(FLOOR_NO_VULKAN)

#include <floor/compute/compute_context.hpp>
#include <floor/compute/vulkan/vulkan_buffer.hpp>
#include <floor/compute/vulkan/vulkan_image.hpp>
#include <floor/compute/vulkan/vulkan_device.hpp>
#include <floor/compute/vulkan/vulkan_program.hpp>
#include <floor/compute/vulkan/vulkan_queue.hpp>
#include <floor/compute/spirv_handler.hpp>
#include <floor/threading/atomic_spin_lock.hpp>

class vulkan_compute final : public compute_context {
public:
	//////////////////////////////////////////
	// init / context creation
	
	vulkan_compute(const bool enable_renderer = false,
				   const vector<string> whitelist = {});
	
	~vulkan_compute() override;
	
	bool is_supported() const override { return supported; }
	
	bool is_graphics_supported() const override { return supported; /* identical to is_supported */ }
	
	COMPUTE_TYPE get_compute_type() const override { return COMPUTE_TYPE::VULKAN; }
	
	//////////////////////////////////////////
	// device functions
	
	shared_ptr<compute_queue> create_queue(const compute_device& dev) const override;
	
	//////////////////////////////////////////
	// buffer creation
	
	shared_ptr<compute_buffer> create_buffer(const compute_queue& cqueue,
											 const size_t& size,
											 const COMPUTE_MEMORY_FLAG flags = (COMPUTE_MEMORY_FLAG::READ_WRITE |
																				COMPUTE_MEMORY_FLAG::HOST_READ_WRITE),
											 const uint32_t opengl_type = 0) const override;
	
	shared_ptr<compute_buffer> create_buffer(const compute_queue& cqueue,
											 const size_t& size,
											 void* data,
											 const COMPUTE_MEMORY_FLAG flags = (COMPUTE_MEMORY_FLAG::READ_WRITE |
																				COMPUTE_MEMORY_FLAG::HOST_READ_WRITE),
											 const uint32_t opengl_type = 0) const override;
	
	shared_ptr<compute_buffer> wrap_buffer(const compute_queue& cqueue,
										   const uint32_t opengl_buffer,
										   const uint32_t opengl_type,
										   const COMPUTE_MEMORY_FLAG flags = (COMPUTE_MEMORY_FLAG::READ_WRITE |
																			  COMPUTE_MEMORY_FLAG::HOST_READ_WRITE)) const override;
	
	shared_ptr<compute_buffer> wrap_buffer(const compute_queue& cqueue,
										   const uint32_t opengl_buffer,
										   const uint32_t opengl_type,
										   void* data,
										   const COMPUTE_MEMORY_FLAG flags = (COMPUTE_MEMORY_FLAG::READ_WRITE |
																			  COMPUTE_MEMORY_FLAG::HOST_READ_WRITE)) const override;
	
	//////////////////////////////////////////
	// image creation
	
	shared_ptr<compute_image> create_image(const compute_queue& cqueue,
										   const uint4 image_dim,
										   const COMPUTE_IMAGE_TYPE image_type,
										   const COMPUTE_MEMORY_FLAG flags = (COMPUTE_MEMORY_FLAG::HOST_READ_WRITE),
										   const uint32_t opengl_type = 0) const override;
	
	shared_ptr<compute_image> create_image(const compute_queue& cqueue,
										   const uint4 image_dim,
										   const COMPUTE_IMAGE_TYPE image_type,
										   void* data,
										   const COMPUTE_MEMORY_FLAG flags = (COMPUTE_MEMORY_FLAG::HOST_READ_WRITE),
										   const uint32_t opengl_type = 0) const override;
	
	shared_ptr<compute_image> wrap_image(const compute_queue& cqueue,
										 const uint32_t opengl_image,
										 const uint32_t opengl_target,
										 const COMPUTE_MEMORY_FLAG flags = (COMPUTE_MEMORY_FLAG::READ_WRITE |
																			COMPUTE_MEMORY_FLAG::HOST_READ_WRITE)) const override;
	
	shared_ptr<compute_image> wrap_image(const compute_queue& cqueue,
										 const uint32_t opengl_image,
										 const uint32_t opengl_target,
										 void* data,
										 const COMPUTE_MEMORY_FLAG flags = (COMPUTE_MEMORY_FLAG::READ_WRITE |
																			COMPUTE_MEMORY_FLAG::HOST_READ_WRITE)) const override;
	
	//////////////////////////////////////////
	// program/kernel functionality
	
	shared_ptr<compute_program> add_universal_binary(const string& file_name) override REQUIRES(!programs_lock);
	
	shared_ptr<compute_program> add_program_file(const string& file_name,
												 const string additional_options) override REQUIRES(!programs_lock);
	
	shared_ptr<compute_program> add_program_file(const string& file_name,
												 compile_options options = {}) override REQUIRES(!programs_lock);
	
	shared_ptr<compute_program> add_program_source(const string& source_code,
												   const string additional_options) override REQUIRES(!programs_lock);
	
	shared_ptr<compute_program> add_program_source(const string& source_code,
												   compile_options options = {}) override REQUIRES(!programs_lock);
	
	shared_ptr<compute_program> add_precompiled_program_file(const string& file_name,
															 const vector<llvm_toolchain::function_info>& functions) override REQUIRES(!programs_lock);
	
	//! NOTE: for internal purposes (not exposed by other backends)
	vulkan_program::vulkan_program_entry create_vulkan_program(const compute_device& device,
															   llvm_toolchain::program_data program);
	
	//! NOTE: for internal purposes (not exposed by other backends)
	shared_ptr<vulkan_program> add_program(vulkan_program::program_map_type&& prog_map) REQUIRES(!programs_lock);
	
	shared_ptr<compute_program::program_entry> create_program_entry(const compute_device& device,
																	llvm_toolchain::program_data program,
																	const llvm_toolchain::TARGET target) override REQUIRES(!programs_lock);
	
	//////////////////////////////////////////
	// graphics functionality
	
	unique_ptr<graphics_pipeline> create_graphics_pipeline(const render_pipeline_description& pipeline_desc) const override;
	
	unique_ptr<graphics_pass> create_graphics_pass(const render_pass_description& pass_desc) const override;
	
	unique_ptr<graphics_renderer> create_graphics_renderer(const compute_queue& cqueue,
														   const graphics_pass& pass,
														   const graphics_pipeline& pipeline) const override;
	
	COMPUTE_IMAGE_TYPE get_renderer_image_type() const override;
	
	//////////////////////////////////////////
	// vulkan specific functions
	
	const VkInstance& get_vulkan_context() const {
		return ctx;
	}
	
	const compute_queue* get_device_default_queue(const compute_device& dev) const;
	
	//! returns the used screen image format
	VkFormat get_screen_format() const {
		return screen.format;
	}
	
	//! returns the allocated swapchain image count
	uint32_t get_swapchain_image_count() const {
		return screen.image_count;
	}
	
	//! returns the swapchain image-view at the specified index
	const VkImageView& get_swapchain_image_view(const uint32_t& idx) const {
		return screen.swapchain_image_views[idx];
	}
	
	struct drawable_image_info {
		uint32_t index { ~0u };
		uint2 image_size;
		VkImage image { nullptr };
		VkImageView image_view { nullptr };
		VkFormat format { VK_FORMAT_UNDEFINED };
		VkAccessFlags access_mask { 0 };
		VkImageLayout layout { VK_IMAGE_LAYOUT_UNDEFINED };
	};
	
	//! acquires the next drawable image
	//! NOTE: will block for now
	pair<bool, const drawable_image_info> acquire_next_image();
	
	//! presents the drawable image that has previously been acquired
	//! NOTE: will block for now
	bool present_image(const drawable_image_info& drawable);
	//! final queue image present only
	//! NOTE: "present_image" calls this
	bool queue_present(const drawable_image_info& drawable);
	
protected:
	VkInstance ctx { nullptr };
	
	bool enable_renderer { false };
	struct screen_data {
		uint2 size;
		uint32_t image_count { 0 };
		uint32_t image_index { 0 };
		VkSurfaceKHR surface { nullptr };
		VkSwapchainKHR swapchain { nullptr };
		VkFormat format { VK_FORMAT_UNDEFINED };
		COMPUTE_IMAGE_TYPE image_type { COMPUTE_IMAGE_TYPE::NONE };
		VkColorSpaceKHR color_space { VK_COLOR_SPACE_SRGB_NONLINEAR_KHR };
		vector<VkImage> swapchain_images;
		vector<VkImageView> swapchain_image_views;
		vector<VkSemaphore> render_semas;
		const vulkan_device* render_device { nullptr };
		bool x11_forwarding { false };
		shared_ptr<vulkan_image> x11_screen;
	} screen;
	bool init_renderer();
	
	// NOTE: these match up 1:1
	vector<VkPhysicalDevice> physical_devices;
	vector<VkDevice> logical_devices;
	
	flat_map<const compute_device&, shared_ptr<compute_queue>> default_queues;
	
	VULKAN_VERSION platform_version { VULKAN_VERSION::VULKAN_1_0 };
	
	atomic_spin_lock programs_lock;
	vector<shared_ptr<vulkan_program>> programs GUARDED_BY(programs_lock);
	
	vulkan_program::vulkan_program_entry
	create_vulkan_program_internal(const vulkan_device& device,
								   const spirv_handler::container& container,
								   const vector<llvm_toolchain::function_info>& functions,
								   const string& identifier);
	
#if defined(FLOOR_DEBUG)
	PFN_vkCreateDebugReportCallbackEXT create_debug_report_callback { nullptr };
	PFN_vkDestroyDebugReportCallbackEXT destroy_debug_report_callback { nullptr };
	VkDebugReportCallbackEXT debug_callback { nullptr };
#endif
	
	// creates the fixed sampler set for all devices
	void create_fixed_sampler_set() const;
	
};

#endif

#endif
