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

#pragma once

#include <floor/compute/vulkan/vulkan_common.hpp>

#if !defined(FLOOR_NO_VULKAN)

#include <floor/compute/compute_image.hpp>
#include <floor/compute/vulkan/vulkan_memory.hpp>

class vulkan_device;
class vulkan_image final : public compute_image, vulkan_memory {
public:
	vulkan_image(const compute_queue& cqueue,
				 const uint4 image_dim,
				 const COMPUTE_IMAGE_TYPE image_type,
				 std::span<uint8_t> host_data_ = {},
				 const COMPUTE_MEMORY_FLAG flags_ = (COMPUTE_MEMORY_FLAG::HOST_READ_WRITE));
	
	//! image info used for wrapping an existing Vulkan image
	//! NOTE: since Vulkan has no image query functionality, this needs to be specified manually
	struct external_vulkan_image_info {
		VkImage image { nullptr };
		VkImageView image_view { nullptr };
		VkFormat format { VK_FORMAT_UNDEFINED };
		VkAccessFlags2 access_mask { 0 };
		VkImageLayout layout { VK_IMAGE_LAYOUT_UNDEFINED };
		//! any of IMAGE_1D, IMAGE_2D, IMAGE_3D, ...
		COMPUTE_IMAGE_TYPE image_base_type { COMPUTE_IMAGE_TYPE::IMAGE_2D };
		uint4 dim;
	};
	
	//! wraps an already existing Vulkan image, with the specified flags and backed by the specified host pointer
	vulkan_image(const compute_queue& cqueue,
				 const external_vulkan_image_info& external_image,
				 std::span<uint8_t> host_data_ = {},
				 const COMPUTE_MEMORY_FLAG flags_ = (COMPUTE_MEMORY_FLAG::READ_WRITE |
													 COMPUTE_MEMORY_FLAG::HOST_READ_WRITE));
	
	~vulkan_image() override;
	
	bool zero(const compute_queue& cqueue) override;
	
	bool blit(const compute_queue& cqueue, compute_image& src) override;
	
	void* __attribute__((aligned(128))) map(const compute_queue& cqueue,
											const COMPUTE_MEMORY_MAP_FLAG flags = (COMPUTE_MEMORY_MAP_FLAG::READ_WRITE | COMPUTE_MEMORY_MAP_FLAG::BLOCK)) override;
	
	bool unmap(const compute_queue& cqueue, void* __attribute__((aligned(128))) mapped_ptr) override;
	
	//! transitions this image into a new 'layout', with specified 'access', at src/dst stage
	//! if "cmd_buffer" is nullptr, a new one will be created and enqueued/submitted in the end (unless "soft_transition" is also set)
	//! if "soft_transition" is set, this won't encode a pipeline barrier in the specified cmd buffer (must manually use returned VkImageMemoryBarrier)
	pair<bool, VkImageMemoryBarrier2> transition(const compute_queue* cqueue,
												 VkCommandBuffer cmd_buffer,
												 const VkAccessFlags2 dst_access,
												 const VkImageLayout new_layout,
												 const VkPipelineStageFlags2 src_stage_mask,
												 const VkPipelineStageFlags2 dst_stage_mask,
												 const bool soft_transition = false);
	
	//! transition for shader or attachment read (if not already in this mode),
	//! returns true if a transition was performed, false if none was necessary
	//! if "allow_general_layout" is set and the current layout is "general", the transition will be skipped
	//! if "soft_transition" is set, this won't encode a pipeline barrier in the specified cmd buffer (must manually use returned VkImageMemoryBarrier)
	pair<bool, VkImageMemoryBarrier2> transition_read(const compute_queue* cqueue, VkCommandBuffer cmd_buffer,
													  const bool allow_general_layout = false,
													  const bool soft_transition = false);
	//! transition for shader or attachment write (if not already in this mode)
	//! returns true if a transition was performed, false if none was necessary
	//! if "read_write" is set, will also make the image readable
	//! if "is_rt_direct_write" is set and the image is a render-target, transition to "general" layout instead of "attachment" layout
	//! if "allow_general_layout" is set and the current layout is "general", the transition will be skipped
	//! if "soft_transition" is set, this won't encode a pipeline barrier in the specified cmd buffer (must manually use returned VkImageMemoryBarrier)
	pair<bool, VkImageMemoryBarrier2> transition_write(const compute_queue* cqueue, VkCommandBuffer cmd_buffer,
													   const bool read_write = false,
													   const bool is_rt_direct_write = false,
													   const bool allow_general_layout = false,
													   const bool soft_transition = false);
	
	//! returns the vulkan specific image object/pointer
	const VkImage& get_vulkan_image() const {
		return image;
	}
	
	//! returns the vulkan specific image view object
	const VkImageView& get_vulkan_image_view() const {
		return image_view;
	}
	
	//! returns the image descriptor info of this image
	const VkDescriptorImageInfo* get_vulkan_image_info() const {
		return &image_info;
	}

	//! returns the current vulkan specific access mask
	const VkAccessFlags2& get_vulkan_access_mask() const {
		return cur_access_mask;
	}
	
	//! returns the mip-map image descriptor info array of this image
	const vector<VkDescriptorImageInfo>& get_vulkan_mip_map_image_info() const {
		return mip_map_image_info;
	}
	
	//! returns the vulkan image format that is used for this image
	VkFormat get_vulkan_format() const {
		return vk_format;
	}
	
	//! returns the corresponding VkFormat for the specified COMPUTE_IMAGE_TYPE,
	//! or nothing if there is no matching format
	static optional<VkFormat> vulkan_format_from_image_type(const COMPUTE_IMAGE_TYPE& image_type_);
	
	//! returns the corresponding COMPUTE_IMAGE_TYPE for the specified VkFormat,
	//! or nothing if there is no matching format
	static optional<COMPUTE_IMAGE_TYPE> image_type_from_vulkan_format(const VkFormat& format_);
	
	//! returns the Vulkan shared memory handle (nullptr/0 if !shared)
	const auto& get_vulkan_shared_handle() const {
		return shared_handle;
	}
	
	//! returns the actual allocation size in bytes this image has been created with
	//! NOTE: allocation size has been computed via vkGetBufferMemoryRequirements and can differ from "size"
	const VkDeviceSize& get_vulkan_allocation_size() const {
		return allocation_size;
	}

	//! if this is an array image that has been created with Vulkan memory aliasing support, returns an individual layer image at the specified index
	VkImage get_vulkan_aliased_layer_image(const uint32_t& layer_index) const {
		if (layer_index < image_aliased_layers.size()) {
			return image_aliased_layers[layer_index];
		}
		return nullptr;
	}

	//! updates the Vulkan image layout and current access mask with the specified ones
	//! NOTE: this is useful when the Vulkan image/state is changed externally and we want to keep this in sync
	void update_with_external_vulkan_state(const VkImageLayout& layout, const VkAccessFlags2& access);
	
	//! converts the specified sample count to the Vulkan sample count enum
	//! NOTE: if "sample_count" doesn't exactly match a supported sample count, the next lower one is returned
	static VkSampleCountFlagBits sample_count_to_vulkan_sample_count(const uint32_t& sample_count);
	
	void set_debug_label(const string& label) override;
	
	//! returns the descriptor data for this buffer (for use in descriptor buffers)
	span<const uint8_t> get_vulkan_descriptor_data_sampled() const {
		return { descriptor_data_sampled.get(), descriptor_sampled_size };
	}
	
	//! returns the descriptor data for this buffer (for use in descriptor buffers)
	span<const uint8_t> get_vulkan_descriptor_data_storage() const {
		return { descriptor_data_storage.get(), descriptor_storage_size };
	}
	
protected:
	VkImage image { nullptr };
	VkImageView image_view { nullptr };
	VkDescriptorImageInfo image_info { nullptr, nullptr, VK_IMAGE_LAYOUT_UNDEFINED };
	VkAccessFlags2 cur_access_mask { 0 };
	VkFormat vk_format { VK_FORMAT_UNDEFINED };
	VkDeviceSize allocation_size { 0 };
	bool is_external { false };

	// contains each individual layer of an image array that has been created with aliasing support
	vector<VkImage> image_aliased_layers;
	
	// similar to image_info, but these contain per-level image views and infos
	// NOTE: image views are only created/used when the image is writable
	vector<VkImageView> mip_map_image_view;
	vector<VkDescriptorImageInfo> mip_map_image_info;
	void update_mip_map_info();
	
	//! when using descriptor buffers, this contains the descriptor data (sampled and storage descriptor)
	size_t descriptor_sampled_size = 0, descriptor_storage_size = 0;
	unique_ptr<uint8_t[]> descriptor_data_sampled;
	unique_ptr<uint8_t[]> descriptor_data_storage;
	
	// shared memory handle when the image has been created with VULKAN_SHARING
#if defined(__WINDOWS__)
	void* /* HANDLE */ shared_handle { nullptr };
#else
	int shared_handle { 0 };
#endif
	
	//! separate create buffer function, b/c it's called by the constructor and resize
	bool create_internal(const bool copy_host_data, const compute_queue& cqueue, const VkImageUsageFlags& usage);
	
	void image_copy_dev_to_host(const compute_queue& cqueue,
								VkCommandBuffer cmd_buffer, VkBuffer host_buffer) override;
	void image_copy_host_to_dev(const compute_queue& cqueue,
								VkCommandBuffer cmd_buffer, VkBuffer host_buffer, std::span<uint8_t> data) override;
	
};

#endif
