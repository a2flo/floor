/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2026 Florian Ziesche
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

#include <floor/device/metal/metal_common.hpp>

#if !defined(FLOOR_NO_METAL)

#include <floor/device/device_context.hpp>
#include <floor/threading/thread_safety.hpp>
#include <floor/threading/atomic_spin_lock.hpp>
#include <floor/threading/safe_resource_container.hpp>

#if defined(__OBJC__)
@class floor_metal_view;
@protocol CAMetalDrawable;
#import <Metal/Metal.h>
#endif

namespace fl {

class metal_program;
class metal_device;
class vr_context;

class metal_context final : public device_context {
public:
	//////////////////////////////////////////
	// init / context creation
	
	metal_context(const DEVICE_CONTEXT_FLAGS ctx_flags,
				  const bool has_toolchain_ = false,
				  const bool enable_renderer = false,
				  vr_context* vr_ctx_ = nullptr,
				  const std::vector<std::string> whitelist = {});
	
	~metal_context() override = default;
	
	bool is_supported() const override { return supported; }
	
	bool is_graphics_supported() const override { return true; }
	
	bool is_vr_supported() const override;
	
	PLATFORM_TYPE get_platform_type() const override { return PLATFORM_TYPE::METAL; }
	
	//////////////////////////////////////////
	// device functions
	
	std::shared_ptr<device_queue> create_queue(const device& dev) const override;
	
	const device_queue* get_device_default_queue(const device& dev) const override;
	
	std::unique_ptr<device_fence> create_fence(const device_queue& cqueue) const override;
	
	memory_usage_t get_memory_usage(const device& dev) const override;
	
	//////////////////////////////////////////
	// buffer creation
	
	std::shared_ptr<device_buffer> create_buffer(const device_queue& cqueue,
												 const size_t size,
												 const MEMORY_FLAG flags = (MEMORY_FLAG::READ_WRITE |
																			MEMORY_FLAG::HOST_READ_WRITE)) const override;
	
	std::shared_ptr<device_buffer> create_buffer(const device_queue& cqueue,
												 std::span<uint8_t> data,
												 const MEMORY_FLAG flags = (MEMORY_FLAG::READ_WRITE |
																			MEMORY_FLAG::HOST_READ_WRITE)) const override;
	
	//////////////////////////////////////////
	// image creation
	
	std::shared_ptr<device_image> create_image(const device_queue& cqueue,
										   const uint4 image_dim,
										   const IMAGE_TYPE image_type,
										   std::span<uint8_t> data,
										   const MEMORY_FLAG flags = (MEMORY_FLAG::HOST_READ_WRITE),
										   const uint32_t mip_level_limit = 0u) const override;
	
	//////////////////////////////////////////
	// program/function functionality
	
	std::shared_ptr<device_program> add_universal_binary(const std::string& file_name) override REQUIRES(!programs_lock);
	
	std::shared_ptr<device_program> add_universal_binary(const std::span<const uint8_t> data) override REQUIRES(!programs_lock);
	
	std::shared_ptr<device_program> add_program_file(const std::string& file_name,
												 const std::string additional_options) override REQUIRES(!programs_lock);
	
	std::shared_ptr<device_program> add_program_file(const std::string& file_name,
												 compile_options options = {}) override REQUIRES(!programs_lock);
	
	std::shared_ptr<device_program> add_program_source(const std::string& source_code,
												   const std::string additional_options) override REQUIRES(!programs_lock);
	
	std::shared_ptr<device_program> add_program_source(const std::string& source_code,
												   compile_options options = {}) override REQUIRES(!programs_lock);
	
	std::shared_ptr<device_program> add_precompiled_program_file(const std::string& file_name,
															 const std::vector<toolchain::function_info>& functions) override REQUIRES(!programs_lock);
	
	std::shared_ptr<device_program::program_entry> create_program_entry(const device& dev,
																	toolchain::program_data program,
																	const toolchain::TARGET target) override REQUIRES(!programs_lock);
	
	//////////////////////////////////////////
	// execution functionality
	
	std::unique_ptr<indirect_command_pipeline> create_indirect_command_pipeline(const indirect_command_description& desc) const override;
	
	//////////////////////////////////////////
	// graphics functionality
	
	std::unique_ptr<graphics_pipeline> create_graphics_pipeline(const render_pipeline_description& pipeline_desc,
														   const bool with_multi_view_support = true) const override;
	
	std::unique_ptr<graphics_pass> create_graphics_pass(const render_pass_description& pass_desc,
												   const bool with_multi_view_support = true) const override;
	
	std::unique_ptr<graphics_renderer> create_graphics_renderer(const device_queue& cqueue,
														   const graphics_pass& pass,
														   const graphics_pipeline& pipeline,
														   const bool create_multi_view_renderer = false) const override;
	
	IMAGE_TYPE get_renderer_image_type() const override;
	
	uint4 get_renderer_image_dim() const override;
	
	vr_context* get_renderer_vr_context() const override;
	
	void set_hdr_metadata(const hdr_metadata_t& hdr_metadata_) override;
	
	float get_hdr_range_max() const override;
	
	float get_hdr_display_max_nits() const override;
	
	//////////////////////////////////////////
	// Metal specific functions
	
	//! for debugging/testing purposes only (circumvents the internal program handling)
	std::shared_ptr<device_program> create_metal_test_program(std::shared_ptr<device_program::program_entry> entry);
	
	// Metal functions only available in Objective-C/C++ mode
#if defined(__OBJC__)
	//! if this context was created with renderer support, this returns the underlying pixel format of the Metal view
	MTLPixelFormat get_metal_renderer_pixel_format() const;
	
	//! if this context was created with renderer support, return the next drawable of the Metal view
	id <CAMetalDrawable> get_metal_next_drawable(id <MTLCommandBuffer> cmd_buffer) const;
	
	//! if this context was created with renderer and VR support, return the next drawable VR Metal image
	std::shared_ptr<device_image> get_metal_next_vr_drawable() const;
	
	//! presents the specified VR drawable
	void present_metal_vr_drawable(const device_queue& cqueue, const device_image& img) const;
#endif
	
	//! starts capturing on the specified device, dumping it to "file_name" (extension must be .gputrace)
	bool start_metal_capture(const device& dev, const std::string& file_name) const;
	//! stops the capturing again
	bool stop_metal_capture() const;
	
	//! returns the null-buffer for the specified device
	//! NOTE: the null buffer is one page in size (x86: 4KiB, ARM: 16KiB)
	const metal_buffer* get_null_buffer(const device& dev) const;
	
	//! acquire an internal soft-printf buffer
	std::pair<device_buffer*, uint32_t> acquire_soft_printf_buffer(const device& dev) const;
	//! release an internal soft-printf buffer
	void release_soft_printf_buffer(const device& dev, const std::pair<device_buffer*, uint32_t>& buf) const;
	
protected:
	void* ctx { nullptr };
	vr_context* vr_ctx { nullptr };
	
	bool enable_renderer { false };
#if defined(__OBJC__)
	floor_metal_view* view { nullptr };
#else
	void* view { nullptr };
#endif
	const metal_device* render_device { nullptr };
	
	fl::flat_map<const device*, std::shared_ptr<device_queue>> internal_queues;
	fl::flat_map<const device*, std::shared_ptr<device_buffer>> internal_null_buffers;
	
	atomic_spin_lock programs_lock;
	std::vector<std::shared_ptr<metal_program>> programs GUARDED_BY(programs_lock);
	
	std::shared_ptr<device_program> create_program_from_archive_binaries(universal_binary::archive_binaries& bins) REQUIRES(!programs_lock);
	
	// VR handling
	struct vr_image_t {
		std::shared_ptr<device_image> image;
		atomic_spin_lock image_lock;
	};
	static constexpr const uint32_t vr_image_count { 2 };
	mutable std::array<vr_image_t, vr_image_count> vr_images;
	mutable uint32_t vr_image_index { 0 };
	bool init_vr_renderer();
	
	// soft-printf buffer cache
	static constexpr const uint32_t soft_printf_buffer_count { 32u };
	using soft_printf_buffer_rsrc_container_type = safe_resource_container<std::shared_ptr<device_buffer>, soft_printf_buffer_count, ~0u>;
	fl::flat_map<const device*, std::unique_ptr<soft_printf_buffer_rsrc_container_type>> soft_printf_buffers;
	
};

} // namespace fl

// for internal debugging only
#define FLOOR_METAL_INTERNAL_MEM_TRACKING_DEBUGGING 0
#if FLOOR_METAL_INTERNAL_MEM_TRACKING_DEBUGGING
extern safe_mutex metal_mem_tracking_lock;
extern uint64_t metal_mem_tracking_leak_total;
#endif

#endif
