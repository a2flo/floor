/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2021 Florian Ziesche
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

#ifndef __FLOOR_VULKAN_IMAGE_HPP__
#define __FLOOR_VULKAN_IMAGE_HPP__

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
				 void* host_ptr = nullptr,
				 const COMPUTE_MEMORY_FLAG flags_ = (COMPUTE_MEMORY_FLAG::HOST_READ_WRITE),
				 const uint32_t opengl_type = 0,
				 const uint32_t external_gl_object_ = 0,
				 const opengl_image_info* gl_image_info = nullptr);
	
	//! image info used for wrapping an existing Vulkan image
	//! NOTE: since Vulkan has no image query functionality, this needs to be specified manually
	struct external_vulkan_image_info {
		VkImage image { nullptr };
		VkImageView image_view { nullptr };
		VkFormat format { VK_FORMAT_UNDEFINED };
		VkAccessFlags access_mask { 0 };
		VkImageLayout layout { VK_IMAGE_LAYOUT_UNDEFINED };
		//! any of IMAGE_1D, IMAGE_2D, IMAGE_3D, ...
		COMPUTE_IMAGE_TYPE image_base_type { COMPUTE_IMAGE_TYPE::IMAGE_2D };
		uint4 dim;
	};
	
	//! wraps an already existing Vulkan image, with the specified flags and backed by the specified host pointer
	vulkan_image(const compute_queue& cqueue,
				 const external_vulkan_image_info& external_image,
				 void* host_ptr = nullptr,
				 const COMPUTE_MEMORY_FLAG flags_ = (COMPUTE_MEMORY_FLAG::READ_WRITE |
													 COMPUTE_MEMORY_FLAG::HOST_READ_WRITE));
	
	~vulkan_image() override;
	
	bool acquire_opengl_object(const compute_queue* cqueue) override;
	bool release_opengl_object(const compute_queue* cqueue) override;
	
	bool zero(const compute_queue& cqueue) override;
	
	void* __attribute__((aligned(128))) map(const compute_queue& cqueue,
											const COMPUTE_MEMORY_MAP_FLAG flags = (COMPUTE_MEMORY_MAP_FLAG::READ_WRITE | COMPUTE_MEMORY_MAP_FLAG::BLOCK)) override;
	
	bool unmap(const compute_queue& cqueue, void* __attribute__((aligned(128))) mapped_ptr) override;
	
	//! transitions this image into a new 'layout', with specified 'access', at src/dst stage
	//! NOTE: if cmd_buffer is nullptr, a new one will be created and enqueued/submitted in the end
	bool transition(const compute_queue& cqueue,
					VkCommandBuffer cmd_buffer,
					const VkAccessFlags dst_access,
					const VkImageLayout new_layout,
					const VkPipelineStageFlags src_stage_mask,
					const VkPipelineStageFlags dst_stage_mask,
					const uint32_t dst_queue_idx = VK_QUEUE_FAMILY_IGNORED);
	
	//! transition for shader or attachment read (if not already in this mode)
	//! if "allow_general_layout" is set and the current layout is "general", the transition will be skipped
	void transition_read(const compute_queue& cqueue, VkCommandBuffer cmd_buffer,
						 const bool allow_general_layout = false);
	//! transition for shader or attachment write (if not already in this mode)
	//! if "read_write" is set, will also make the image readable
	//! if "is_rt_direct_write" is set and the image is a render-target, transition to "general" layout instead of "attachment" layout
	//! if "allow_general_layout" is set and the current layout is "general", the transition will be skipped
	void transition_write(const compute_queue& cqueue, VkCommandBuffer cmd_buffer,
						  const bool read_write = false,
						  const bool is_rt_direct_write = false,
						  const bool allow_general_layout = false);
	
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
	const VkAccessFlags& get_vulkan_access_mask() const {
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
	void update_with_external_vulkan_state(const VkImageLayout& layout, const VkAccessFlags& access);
	
	void set_debug_label(const string& label) override;
	
protected:
	VkImage image { nullptr };
	VkImageView image_view { nullptr };
	VkDescriptorImageInfo image_info { nullptr, nullptr, VK_IMAGE_LAYOUT_UNDEFINED };
	VkAccessFlags cur_access_mask { 0 };
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
								VkCommandBuffer cmd_buffer, VkBuffer host_buffer, void* data) override;
	
};

#endif

#endif
