/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2024 Florian Ziesche
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

#if defined(__WINDOWS__) // we don't want to globally include vulkan_win32.h (and windows.h), so forward declare this instead
struct VkMemoryGetWin32HandleInfoKHR;
struct VkSemaphoreGetWin32HandleInfoKHR;
typedef void* HANDLE;
typedef VkResult (VKAPI_PTR *PFN_vkGetMemoryWin32HandleKHR)(VkDevice device, const VkMemoryGetWin32HandleInfoKHR* pGetWin32HandleInfo, HANDLE* pHandle);
typedef VkResult (VKAPI_PTR *PFN_vkGetSemaphoreWin32HandleKHR)(VkDevice device, const VkSemaphoreGetWin32HandleInfoKHR* pGetWin32HandleInfo, HANDLE* pHandle);
#endif

class vr_context;

class vulkan_compute final : public compute_context {
public:
	//////////////////////////////////////////
	// init / context creation
	
	explicit vulkan_compute(const COMPUTE_CONTEXT_FLAGS ctx_flags = COMPUTE_CONTEXT_FLAGS::NONE,
							const bool enable_renderer = false,
							vr_context* vr_ctx_ = nullptr,
							const vector<string> whitelist = {});
	
	~vulkan_compute() override;
	
	bool is_supported() const override { return supported; }
	
	bool is_graphics_supported() const override { return true; }

	bool is_vr_supported() const override;
	
	COMPUTE_TYPE get_compute_type() const override { return COMPUTE_TYPE::VULKAN; }
	
	//////////////////////////////////////////
	// device functions
	
	shared_ptr<compute_queue> create_queue(const compute_device& dev) const override;
	
	const compute_queue* get_device_default_queue(const compute_device& dev) const override;
	
	shared_ptr<compute_queue> create_compute_queue(const compute_device& dev) const override;
	
	const compute_queue* get_device_default_compute_queue(const compute_device& dev) const override;
	
	unique_ptr<compute_fence> create_fence(const compute_queue& cqueue) const override;
	
	//////////////////////////////////////////
	// buffer creation
	
	shared_ptr<compute_buffer> create_buffer(const compute_queue& cqueue,
											 const size_t& size,
											 const COMPUTE_MEMORY_FLAG flags = (COMPUTE_MEMORY_FLAG::READ_WRITE |
																				COMPUTE_MEMORY_FLAG::HOST_READ_WRITE),
											 const uint32_t opengl_type = 0) const override;
	
	shared_ptr<compute_buffer> create_buffer(const compute_queue& cqueue,
											 std::span<uint8_t> data,
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
										   std::span<uint8_t> data,
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
	// execution functionality
	
	unique_ptr<indirect_command_pipeline> create_indirect_command_pipeline(const indirect_command_description& desc) const override;
	
	//////////////////////////////////////////
	// graphics functionality
	
	unique_ptr<graphics_pipeline> create_graphics_pipeline(const render_pipeline_description& pipeline_desc,
														   const bool with_multi_view_support = true) const override;
	
	unique_ptr<graphics_pass> create_graphics_pass(const render_pass_description& pass_desc,
												   const bool with_multi_view_support = true) const override;
	
	unique_ptr<graphics_renderer> create_graphics_renderer(const compute_queue& cqueue,
														   const graphics_pass& pass,
														   const graphics_pipeline& pipeline,
														   const bool create_multi_view_renderer = false) const override;
	
	COMPUTE_IMAGE_TYPE get_renderer_image_type() const override;

	uint4 get_renderer_image_dim() const override;

	vr_context* get_renderer_vr_context() const override;
	
	void set_hdr_metadata(const hdr_metadata_t& hdr_metadata_) override;
	
	//////////////////////////////////////////
	// vulkan specific functions
	
	const VkInstance& get_vulkan_context() const {
		return ctx;
	}

	const vr_context* get_vulkan_vr_context() const {
		return vr_ctx;
	}
	
	//! returns the used screen image format
	VkFormat get_screen_format() const {
		return screen.format;
	}
	
	//! returns true if the screen image has wide-gamut support
	bool is_screen_wide_gamut() const {
		return screen.has_wide_gamut;
	}
	
	//! returns true if the screen image has HDR support and HDR metadata has been set
	bool is_screen_hdr() const {
		return screen.hdr_metadata.has_value();
	}
	
	//! returns the allocated swapchain image count
	//! TODO: remove this
	uint32_t get_swapchain_image_count() const {
		return screen.image_count;
	}
	
	//! returns the swapchain image-view at the specified index
	//! TODO: remove this
	const VkImageView& get_swapchain_image_view(const uint32_t& idx) const {
		return screen.swapchain_image_views[idx];
	}
	
	struct drawable_image_info {
		uint32_t index { ~0u };
		uint2 image_size;
		uint32_t layer_count { 1 };
		VkImage image { nullptr };
		VkImageView image_view { nullptr };
		VkFormat format { VK_FORMAT_UNDEFINED };
		VkAccessFlags2 access_mask { 0 };
		VkImageLayout layout { VK_IMAGE_LAYOUT_UNDEFINED };
		VkImageLayout present_layout { VK_IMAGE_LAYOUT_PRESENT_SRC_KHR };
		COMPUTE_IMAGE_TYPE base_type { COMPUTE_IMAGE_TYPE::NONE };
	};
	
	//! acquires the next drawable image
	//! NOTE: will block for now
	pair<bool, const drawable_image_info> acquire_next_image(const compute_queue& dev_queue, const bool get_multi_view_drawable = false);
	
	//! presents the drawable image that has previously been acquired
	//! NOTE: will block for now
	bool present_image(const compute_queue& dev_queue, const drawable_image_info& drawable);
	//! final queue image present only
	//! NOTE: "present_image" calls this
	bool queue_present(const compute_queue& dev_queue, const drawable_image_info& drawable);
	
#if defined(__WINDOWS__)
	//! calls vkGetMemoryWin32HandleKHR
	VkResult vulkan_get_memory_win32_handle(VkDevice device_, const VkMemoryGetWin32HandleInfoKHR* pGetWin32HandleInfo_, HANDLE* pHandle_) const {
		return (*get_memory_win32_handle)(device_, pGetWin32HandleInfo_, pHandle_);
	}
	//! calls vkGetSemaphoreWin32HandleKHR
	VkResult vulkan_get_semaphore_win32_handle(VkDevice device_, const VkSemaphoreGetWin32HandleInfoKHR* pGetWin32HandleInfo_, HANDLE* pHandle_) const {
		return (*get_semaphore_win32_handle)(device_, pGetWin32HandleInfo_, pHandle_);
	}
#else
	//! calls vkGetMemoryFdKHR
	VkResult vulkan_get_memory_fd(VkDevice device_, const VkMemoryGetFdInfoKHR* pGetFdInfo_, int* pFd_) const {
		return (*get_memory_fd)(device_, pGetFdInfo_, pFd_);
	}
	//! calls vkGetSemaphoreFdKHR
	VkResult vulkan_get_semaphore_fd(VkDevice device_, const VkSemaphoreGetFdInfoKHR* pGetFdInfo_, int* pFd_) const {
		return (*get_semaphore_fd)(device_, pGetFdInfo_, pFd_);
	}
#endif

	//! calls vkSetHdrMetadataEXT
	void vulkan_set_hdr_metadata(VkDevice device_, uint32_t swapchainCount_, const VkSwapchainKHR* pSwapchains_, const VkHdrMetadataEXT* pMetadata_) {
		if (!vk_set_hdr_metadata) {
			return;
		}
		(*vk_set_hdr_metadata)(device_, swapchainCount_, pSwapchains_, pMetadata_);
	}
	
	//! calls vkGetDescriptorSetLayoutBindingOffsetEXT
	void vulkan_get_descriptor_set_layout_binding_offset(VkDevice device_, VkDescriptorSetLayout layout_,
														 uint32_t binding_, VkDeviceSize* pOffset_) const {
		(*get_descriptor_set_layout_binding_offset)(device_, layout_, binding_, pOffset_);
	}
	//! calls vkGetDescriptorSetLayoutSizeEXT
	void vulkan_get_descriptor_set_layout_size(VkDevice device_, VkDescriptorSetLayout layout_,
											   VkDeviceSize* pLayoutSizeInBytes_) const {
		(*get_descriptor_set_layout_size)(device_, layout_, pLayoutSizeInBytes_);
	}
	//! calls vkGetDescriptorEXT
	void vulkan_get_descriptor(VkDevice device_, const VkDescriptorGetInfoEXT* pDescriptorInfo_,
							   size_t dataSize_, void* pDescriptor_) const {
		(*get_descriptor)(device_, pDescriptorInfo_, dataSize_, pDescriptor_);
	}
	//! calls vkCmdBindDescriptorBuffersEXT
	void vulkan_cmd_bind_descriptor_buffers(VkCommandBuffer commandBuffer_, uint32_t bufferCount_,
											const VkDescriptorBufferBindingInfoEXT* pBindingInfos_) const {
		(*cmd_bind_descriptor_buffers)(commandBuffer_, bufferCount_, pBindingInfos_);
	}
	//! calls vkCmdBindDescriptorBufferEmbeddedSamplersEXT
	void vulkan_cmd_bind_descriptor_buffer_embedded_samplers(VkCommandBuffer commandBuffer_, VkPipelineBindPoint pipelineBindPoint_,
														 VkPipelineLayout layout_, uint32_t set_) const {
		(*cmd_bind_descriptor_buffer_embedded_samplers)(commandBuffer_, pipelineBindPoint_, layout_, set_);
	}
	//! calls vkCmdSetDescriptorBufferOffsetsEXT
	void vulkan_cmd_set_descriptor_buffer_offsets(VkCommandBuffer commandBuffer_, VkPipelineBindPoint pipelineBindPoint_,
												  VkPipelineLayout layout_, uint32_t firstSet_, uint32_t setCount_,
												  const uint32_t* pBufferIndices_, const VkDeviceSize* pOffsets_) const {
		(*cmd_set_descriptor_buffer_offsets)(commandBuffer_, pipelineBindPoint_, layout_, firstSet_,
											 setCount_, pBufferIndices_, pOffsets_);
	}

	//! returns true if validation layer error printing is currently enabled
	bool is_vulkan_validation_ignored() const {
		return ignore_validation.load();
	}

	//! sets the state of whether validation layer errors should be printed/logged
	void set_vulkan_validation_ignored(const bool& state) {
		ignore_validation = state;
	}
	
	//! sets a Vulkan debug label on the specified object/handle, on the specified device
	void set_vulkan_debug_label(const vulkan_device& dev [[maybe_unused]],
								const VkObjectType type [[maybe_unused]],
								const uint64_t& handle [[maybe_unused]],
								const string& name [[maybe_unused]]) const
#if defined(FLOOR_DEBUG)
	;
#else
	{}
#endif
	
	//! begins a Vulkan command buffer debug label block
	void vulkan_begin_cmd_debug_label(const VkCommandBuffer& cmd_buffer [[maybe_unused]],
									  const string& label [[maybe_unused]]) const
#if defined(FLOOR_DEBUG)
	;
#else
	{}
#endif
	
	//! ends a Vulkan command buffer debug label block
	void vulkan_end_cmd_debug_label(const VkCommandBuffer& cmd_buffer [[maybe_unused]]) const
#if defined(FLOOR_DEBUG)
	;
#else
	{}
#endif
	
	//! amount of fixed/embedded samplers: 6 bits (used 75%) -> 48 combinations
	static constexpr const uint32_t max_sampler_combinations { 48 };
	
protected:
	VkInstance ctx { nullptr };
	vr_context* vr_ctx { nullptr };
	
	bool enable_renderer { false };
	bool hdr_supported { false };
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
		uint32_t next_fence_index { 0 };
		vector<unique_ptr<compute_fence>> render_fences;
		const vulkan_device* render_device { nullptr };
		bool x11_forwarding { false };
		shared_ptr<vulkan_image> x11_screen;
		optional<VkHdrMetadataEXT> hdr_metadata;
		bool has_wide_gamut { false };
	} screen;
	struct vr_screen_data {
		uint2 size;
		uint32_t layer_count { 2 };
		uint32_t image_count { 2 };
		uint32_t image_index { 0 };
		VkFormat format { VK_FORMAT_UNDEFINED };
		VkColorSpaceKHR color_space { VK_COLOR_SPACE_SRGB_NONLINEAR_KHR };
		COMPUTE_IMAGE_TYPE image_type { COMPUTE_IMAGE_TYPE::NONE };
		vector<shared_ptr<compute_image>> images;
		vector<compute_image*> external_swapchain_images;
		vector<atomic_spin_lock> image_locks;
		const vulkan_device* render_device { nullptr };
		bool has_wide_gamut { false };
	} vr_screen;
	safe_mutex acquisition_lock;
	bool init_renderer();
	bool init_vr_renderer();
	
	// sets screen.hdr_metadata from current compute_context::hdr_metadata if screen.hdr_metadata is not empty
	void set_vk_screen_hdr_metadata();
	
	// NOTE: these match up 1:1
	vector<VkPhysicalDevice> physical_devices;
	vector<VkDevice> logical_devices;
	
	floor_core::flat_map<const compute_device&, shared_ptr<compute_queue>> default_queues;
	floor_core::flat_map<const compute_device&, shared_ptr<compute_queue>> default_compute_queues;
	
	VULKAN_VERSION platform_version { VULKAN_VERSION::VULKAN_1_3 };
	
	atomic_spin_lock programs_lock;
	vector<shared_ptr<vulkan_program>> programs GUARDED_BY(programs_lock);
	
	vulkan_program::vulkan_program_entry
	create_vulkan_program_internal(const vulkan_device& device,
								   const spirv_handler::container& container,
								   const vector<llvm_toolchain::function_info>& functions,
								   const string& identifier);
	
#if defined(FLOOR_DEBUG)
	PFN_vkCreateDebugUtilsMessengerEXT create_debug_utils_messenger { nullptr };
	PFN_vkDestroyDebugUtilsMessengerEXT destroy_debug_utils_messenger { nullptr };
	VkDebugUtilsMessengerEXT debug_utils_messenger { nullptr };
	PFN_vkSetDebugUtilsObjectNameEXT set_debug_utils_object_name { nullptr };
	PFN_vkCmdBeginDebugUtilsLabelEXT cmd_begin_debug_utils_label { nullptr };
	PFN_vkCmdEndDebugUtilsLabelEXT cmd_end_debug_utils_label { nullptr };
#endif
	
#if defined(__WINDOWS__)
	PFN_vkGetMemoryWin32HandleKHR get_memory_win32_handle { nullptr };
	PFN_vkGetSemaphoreWin32HandleKHR get_semaphore_win32_handle { nullptr };
#else
	PFN_vkGetMemoryFdKHR get_memory_fd { nullptr };
	PFN_vkGetSemaphoreFdKHR get_semaphore_fd { nullptr };
#endif

	PFN_vkSetHdrMetadataEXT vk_set_hdr_metadata { nullptr };
	
	// VK_EXT_descriptor_buffer
	PFN_vkGetDescriptorSetLayoutBindingOffsetEXT get_descriptor_set_layout_binding_offset { nullptr };
	PFN_vkGetDescriptorSetLayoutSizeEXT get_descriptor_set_layout_size { nullptr };
	PFN_vkGetDescriptorEXT get_descriptor { nullptr };
	PFN_vkCmdBindDescriptorBuffersEXT cmd_bind_descriptor_buffers { nullptr };
	PFN_vkCmdBindDescriptorBufferEmbeddedSamplersEXT cmd_bind_descriptor_buffer_embedded_samplers { nullptr };
	PFN_vkCmdSetDescriptorBufferOffsetsEXT cmd_set_descriptor_buffer_offsets { nullptr };
	
	// creates the fixed sampler set for all devices
	void create_fixed_sampler_set() const;

	// if true, won't log/print validation layer messages
	atomic<bool> ignore_validation { false };
	
	//! internal device queue creation handler
	shared_ptr<compute_queue> create_queue_internal(const compute_device& dev, const uint32_t family_index,
													const compute_queue::QUEUE_TYPE queue_type,
													uint32_t& queue_index) const;
	
};

#endif

#endif
