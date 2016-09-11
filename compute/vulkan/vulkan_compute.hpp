/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2016 Florian Ziesche
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
#include <floor/compute/vulkan/vulkan_kernel.hpp>
#include <floor/compute/vulkan/vulkan_program.hpp>
#include <floor/compute/vulkan/vulkan_queue.hpp>

class vulkan_compute final : public compute_context {
public:
	//////////////////////////////////////////
	// init / context creation
	
	vulkan_compute(const bool enable_renderer = false,
				   const vector<string> whitelist = {});
	
	~vulkan_compute() override;
	
	bool is_supported() const override { return supported; }
	
	COMPUTE_TYPE get_compute_type() const override { return COMPUTE_TYPE::VULKAN; }
	
	//////////////////////////////////////////
	// device functions
	
	shared_ptr<compute_queue> create_queue(shared_ptr<compute_device> dev) override;
	
	//////////////////////////////////////////
	// buffer creation
	
	shared_ptr<compute_buffer> create_buffer(shared_ptr<compute_device> device,
											 const size_t& size,
											 const COMPUTE_MEMORY_FLAG flags = (COMPUTE_MEMORY_FLAG::READ_WRITE |
																				COMPUTE_MEMORY_FLAG::HOST_READ_WRITE),
											 const uint32_t opengl_type = 0) override;
	
	shared_ptr<compute_buffer> create_buffer(shared_ptr<compute_device> device,
											 const size_t& size,
											 void* data,
											 const COMPUTE_MEMORY_FLAG flags = (COMPUTE_MEMORY_FLAG::READ_WRITE |
																				COMPUTE_MEMORY_FLAG::HOST_READ_WRITE),
											 const uint32_t opengl_type = 0) override;
	
	shared_ptr<compute_buffer> wrap_buffer(shared_ptr<compute_device> device,
										   const uint32_t opengl_buffer,
										   const uint32_t opengl_type,
										   const COMPUTE_MEMORY_FLAG flags = (COMPUTE_MEMORY_FLAG::READ_WRITE |
																			  COMPUTE_MEMORY_FLAG::HOST_READ_WRITE)) override;
	
	shared_ptr<compute_buffer> wrap_buffer(shared_ptr<compute_device> device,
										   const uint32_t opengl_buffer,
										   const uint32_t opengl_type,
										   void* data,
										   const COMPUTE_MEMORY_FLAG flags = (COMPUTE_MEMORY_FLAG::READ_WRITE |
																			  COMPUTE_MEMORY_FLAG::HOST_READ_WRITE)) override;
	
	//////////////////////////////////////////
	// image creation
	
	shared_ptr<compute_image> create_image(shared_ptr<compute_device> device,
										   const uint4 image_dim,
										   const COMPUTE_IMAGE_TYPE image_type,
										   const COMPUTE_MEMORY_FLAG flags = (COMPUTE_MEMORY_FLAG::HOST_READ_WRITE),
										   const uint32_t opengl_type = 0) override;
	
	shared_ptr<compute_image> create_image(shared_ptr<compute_device> device,
										   const uint4 image_dim,
										   const COMPUTE_IMAGE_TYPE image_type,
										   void* data,
										   const COMPUTE_MEMORY_FLAG flags = (COMPUTE_MEMORY_FLAG::HOST_READ_WRITE),
										   const uint32_t opengl_type = 0) override;
	
	shared_ptr<compute_image> wrap_image(shared_ptr<compute_device> device,
										 const uint32_t opengl_image,
										 const uint32_t opengl_target,
										 const COMPUTE_MEMORY_FLAG flags = (COMPUTE_MEMORY_FLAG::READ_WRITE |
																			COMPUTE_MEMORY_FLAG::HOST_READ_WRITE)) override;
	
	shared_ptr<compute_image> wrap_image(shared_ptr<compute_device> device,
										 const uint32_t opengl_image,
										 const uint32_t opengl_target,
										 void* data,
										 const COMPUTE_MEMORY_FLAG flags = (COMPUTE_MEMORY_FLAG::READ_WRITE |
																			COMPUTE_MEMORY_FLAG::HOST_READ_WRITE)) override;
	
	//////////////////////////////////////////
	// program/kernel functionality
	
	shared_ptr<compute_program> add_program_file(const string& file_name,
												 const string additional_options) override REQUIRES(!programs_lock);
	
	shared_ptr<compute_program> add_program_file(const string& file_name,
												 compile_options options = {}) override REQUIRES(!programs_lock);
	
	shared_ptr<compute_program> add_program_source(const string& source_code,
												   const string additional_options) override REQUIRES(!programs_lock);
	
	shared_ptr<compute_program> add_program_source(const string& source_code,
												   compile_options options = {}) override REQUIRES(!programs_lock);
	
	shared_ptr<compute_program> add_precompiled_program_file(const string& file_name,
															 const vector<llvm_compute::function_info>& functions) override REQUIRES(!programs_lock);
	
	//! NOTE: for internal purposes (not exposed by other backends)
	vulkan_program::vulkan_program_entry create_vulkan_program(shared_ptr<compute_device> device,
															   llvm_compute::program_data program);
	
	//! NOTE: for internal purposes (not exposed by other backends)
	shared_ptr<vulkan_program> add_program(vulkan_program::program_map_type&& prog_map) REQUIRES(!programs_lock);
	
	shared_ptr<compute_program::program_entry> create_program_entry(shared_ptr<compute_device> device,
																	llvm_compute::program_data program,
																	const llvm_compute::TARGET target) override REQUIRES(!programs_lock);
	
	//////////////////////////////////////////
	// vulkan specific functions
	
	const VkInstance& get_vulkan_context() const {
		return ctx;
	}
	
	shared_ptr<compute_queue> get_device_default_queue(shared_ptr<compute_device> dev) const;
	shared_ptr<compute_queue> get_device_default_queue(const compute_device* dev) const;
	
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
		const uint32_t index;
		const uint2 image_size;
		VkImage image;
		VkSemaphore sema;
	};
	
	//! acquires the next drawable image
	//! NOTE: will block for now
	pair<bool, drawable_image_info> acquire_next_image();
	
	//! presents the drawable image that has previously been acquired
	//! NOTE: will block for now
	bool present_image(const drawable_image_info& drawable);
	
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
		VkColorSpaceKHR color_space { VK_COLOR_SPACE_SRGB_NONLINEAR_KHR };
		vector<VkImage> swapchain_images;
		vector<VkImageView> swapchain_image_views;
		vector<VkSemaphore> render_semas;
		vulkan_device* render_device { nullptr };
	} screen;
	bool init_renderer();
	
	// NOTE: these match up 1:1
	vector<VkPhysicalDevice> physical_devices;
	vector<VkDevice> logical_devices;
	
	vector<pair<shared_ptr<compute_device>, shared_ptr<compute_queue>>> default_queues;
	
	VULKAN_VERSION platform_version { VULKAN_VERSION::VULKAN_1_0 };
	
	atomic_spin_lock programs_lock;
	vector<shared_ptr<vulkan_program>> programs GUARDED_BY(programs_lock);
	
#if defined(FLOOR_DEBUG)
	PFN_vkCreateDebugReportCallbackEXT create_debug_report_callback;
	PFN_vkDestroyDebugReportCallbackEXT destroy_debug_report_callback;
	VkDebugReportCallbackEXT debug_callback;
#endif
	
};

#endif

#endif
