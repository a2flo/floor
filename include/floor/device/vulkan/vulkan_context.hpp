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

#include <floor/device/vulkan/vulkan_common.hpp>

#if !defined(FLOOR_NO_VULKAN)

#include <floor/device/device_context.hpp>
#include <floor/device/vulkan/vulkan_buffer.hpp>
#include <floor/device/vulkan/vulkan_image.hpp>
#include <floor/device/vulkan/vulkan_device.hpp>
#include <floor/device/vulkan/vulkan_program.hpp>
#include <floor/device/vulkan/vulkan_queue.hpp>
#include <floor/device/spirv_handler.hpp>
#include <floor/threading/atomic_spin_lock.hpp>
#include <floor/core/event_objects.hpp>

namespace fl {

class vr_context;

struct vulkan_context_internal;
struct vulkan_drawable_image_info;

class vulkan_context final : public device_context {
public:
	//////////////////////////////////////////
	// init / context creation
	
	explicit vulkan_context(const DEVICE_CONTEXT_FLAGS ctx_flags = DEVICE_CONTEXT_FLAGS::NONE,
							const bool has_toolchain_ = false,
							const bool enable_renderer = false,
							vr_context* vr_ctx_ = nullptr,
							const std::vector<std::string> whitelist = {});
	
	~vulkan_context() override;
	
	bool is_supported() const override { return supported; }
	
	bool is_graphics_supported() const override { return true; }
	
	bool is_vr_supported() const override;
	
	PLATFORM_TYPE get_platform_type() const override { return PLATFORM_TYPE::VULKAN; }
	
	//////////////////////////////////////////
	// device functions
	
	std::shared_ptr<device_queue> create_queue(const device& dev) const override;
	
	const device_queue* get_device_default_queue(const device& dev) const override;
	
	std::shared_ptr<device_queue> create_compute_queue(const device& dev) const override;
	
	const device_queue* get_device_default_compute_queue(const device& dev) const override;
	
	std::optional<uint32_t> get_max_distinct_queue_count(const device& dev) const override;
	
	std::optional<uint32_t> get_max_distinct_compute_queue_count(const device& dev) const override;
	
	std::vector<std::shared_ptr<device_queue>> create_distinct_queues(const device& dev, const uint32_t wanted_count) const override;
	
	std::vector<std::shared_ptr<device_queue>> create_distinct_compute_queues(const device& dev, const uint32_t wanted_count) const override;
	
	std::unique_ptr<device_fence> create_fence(const device_queue& cqueue) const override;
	
	memory_usage_t get_memory_usage(const device& dev) const override;
	
	//////////////////////////////////////////
	// buffer creation
	
	std::shared_ptr<device_buffer> create_buffer(const device_queue& cqueue,
											 const size_t& size,
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
	
	//! NOTE: for internal purposes (not exposed by other backends)
	vulkan_program::vulkan_program_entry create_vulkan_program(const device& dev,
															   toolchain::program_data program);
	
	//! NOTE: for internal purposes (not exposed by other backends)
	std::shared_ptr<vulkan_program> add_program(vulkan_program::program_map_type&& prog_map) REQUIRES(!programs_lock);
	
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
	
	//////////////////////////////////////////
	// Vulkan specific functions
	
	const VkInstance& get_vulkan_context() const {
		return ctx;
	}
	
	const vr_context* get_vulkan_vr_context() const {
		return vr_ctx;
	}
	
	//! acquires the next drawable image
	std::pair<bool, std::unique_ptr<vulkan_drawable_image_info>> acquire_next_image(const device_queue& dev_queue,
																					const bool get_multi_view_drawable = false)
	REQUIRES(!acquisition_lock, !screen_sema_lock);
	
	//! presents the drawable image that has previously been acquired
	//! NOTE: will block for now
	bool present_image(const device_queue& dev_queue, const vulkan_drawable_image_info& drawable);
	//! final queue image present only
	//! NOTE: "present_image" calls this
	bool queue_present(const device_queue& dev_queue, const vulkan_drawable_image_info& drawable);

	//! returns true if validation layer error printing is currently enabled
	bool is_vulkan_validation_ignored() const {
		return ignore_validation.load();
	}

	//! sets the state of whether validation layer errors should be printed/logged
	void set_vulkan_validation_ignored(const bool& state) {
		ignore_validation = state;
	}
	
	//! amount of fixed/embedded samplers: 6 bits (used 75%) -> 48 combinations
	static constexpr const uint32_t max_sampler_combinations { 48 };
	
protected:
	std::shared_ptr<vulkan_context_internal> internal;
	VkInstance ctx { nullptr };
	vr_context* vr_ctx { nullptr };
	
	bool enable_renderer { false };
	bool hdr_supported { false };
	safe_mutex acquisition_lock;

	//! max swapchain image count limit
	static constexpr const uint32_t max_swapchain_image_count { 8u };
	//! multiplier against the actual image count (conservative estimate) -> use in acquisition_semas/present_semas
	static constexpr const uint32_t semaphore_multiplier { 2u };
	//! NOTE: semaphores do not map 1:1 to swapchain_images
	safe_mutex screen_sema_lock;
	std::vector<VkFence> present_fences GUARDED_BY(screen_sema_lock);
	uint32_t next_sema_index GUARDED_BY(screen_sema_lock) { 0 };
	std::bitset<32u> semas_in_use GUARDED_BY(screen_sema_lock);
	std::vector<std::unique_ptr<device_fence>> acquisition_semas GUARDED_BY(screen_sema_lock);
	std::vector<std::unique_ptr<device_fence>> present_semas GUARDED_BY(screen_sema_lock);
	
	bool reinit_renderer(const uint2 screen_size) REQUIRES(!screen_sema_lock);
	void destroy_renderer_swapchain(const bool reset_present_fences) REQUIRES(!screen_sema_lock);
	bool init_vr_renderer();
	std::pair<bool, std::unique_ptr<vulkan_drawable_image_info>> acquire_next_vr_image(const device_queue& dev_queue) REQUIRES(acquisition_lock);
	
	std::function<bool(EVENT_TYPE, std::shared_ptr<event_object>)> resize_handler_fnctr;
	bool resize_handler(EVENT_TYPE type, std::shared_ptr<event_object>) REQUIRES(!screen_sema_lock);
	
	// sets screen.hdr_metadata from current device_context::hdr_metadata if screen.hdr_metadata is not empty
	void set_vk_screen_hdr_metadata();
	
	// NOTE: these match up 1:1
	std::vector<VkPhysicalDevice> physical_devices;
	std::vector<VkDevice> logical_devices;
	
	fl::flat_map<const device*, std::shared_ptr<device_queue>> default_queues;
	fl::flat_map<const device*, std::shared_ptr<device_queue>> default_device_queues;
	
	VULKAN_VERSION platform_version { VULKAN_VERSION::VULKAN_1_3 };
	
	atomic_spin_lock programs_lock;
	std::vector<std::shared_ptr<vulkan_program>> programs GUARDED_BY(programs_lock);
	
	vulkan_program::vulkan_program_entry
	create_vulkan_program_internal(const vulkan_device& dev,
								   const spirv_handler::container& container,
								   const std::vector<toolchain::function_info>& functions,
								   const std::string& identifier);
	
	// creates the fixed sampler set for all devices
	void create_fixed_sampler_set() const;

	// if true, won't log/print validation layer messages
	std::atomic<bool> ignore_validation { false };
	
	//! internal device queue creation handler
	std::shared_ptr<device_queue> create_queue_internal(const device& dev, const uint32_t family_index,
													const device_queue::QUEUE_TYPE queue_type,
													uint32_t& queue_index) const;
	
	std::shared_ptr<device_program> create_program_from_archive_binaries(universal_binary::archive_binaries& bins,
																	 const std::string& identifier) REQUIRES(!programs_lock);
	
};

} // namespace fl

#endif
