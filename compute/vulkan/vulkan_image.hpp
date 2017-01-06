/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2017 Florian Ziesche
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
	vulkan_image(const vulkan_device* device,
				 const uint4 image_dim,
				 const COMPUTE_IMAGE_TYPE image_type,
				 void* host_ptr = nullptr,
				 const COMPUTE_MEMORY_FLAG flags_ = (COMPUTE_MEMORY_FLAG::HOST_READ_WRITE),
				 const uint32_t opengl_type = 0,
				 const uint32_t external_gl_object_ = 0,
				 const opengl_image_info* gl_image_info = nullptr);
	
	~vulkan_image() override;
	
	bool acquire_opengl_object(shared_ptr<compute_queue> cqueue) override;
	bool release_opengl_object(shared_ptr<compute_queue> cqueue) override;
	
	void zero(shared_ptr<compute_queue> cqueue) override;
	
	void* __attribute__((aligned(128))) map(shared_ptr<compute_queue> cqueue,
											const COMPUTE_MEMORY_MAP_FLAG flags =
											(COMPUTE_MEMORY_MAP_FLAG::READ_WRITE |
											 COMPUTE_MEMORY_MAP_FLAG::BLOCK)) override;
	
	void unmap(shared_ptr<compute_queue> cqueue, void* __attribute__((aligned(128))) mapped_ptr) override;
	
	//! transitions this image into a new 'layout', with specified 'access', at src/dst stage
	//! NOTE: if cmd_buffer is nullptr, a new one will be created and enqueued/submitted in the end
	void transition(VkCommandBuffer cmd_buffer,
					const VkAccessFlags dst_access,
					const VkImageLayout new_layout,
					const VkPipelineStageFlags src_stage_mask,
					const VkPipelineStageFlags dst_stage_mask,
					const uint32_t dst_queue_idx = VK_QUEUE_FAMILY_IGNORED);
	
	//! transition for shader or attachment read (if not already in this mode)
	void transition_read(VkCommandBuffer cmd_buffer);
	//! transition for shader or attachment write (if not already in this mode)
	//! if 'read_write' is set, will also make the image readable
	void transition_write(VkCommandBuffer cmd_buffer, const bool read_write = false);
	
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
	
	//! returns the mip-map image descriptor info array of this image
	const vector<VkDescriptorImageInfo>& get_vulkan_mip_map_image_info() const {
		return mip_map_image_info;
	}
	
	//! returns the vulkan image format that is used for this image
	VkFormat get_vulkan_format() const {
		return vk_format;
	}
	
protected:
	VkImage image { nullptr };
	VkImageView image_view { nullptr };
	VkDescriptorImageInfo image_info { nullptr, nullptr, VK_IMAGE_LAYOUT_UNDEFINED };
	VkAccessFlags cur_access_mask { 0 };
	VkImageUsageFlags usage { 0 };
	VkFormat vk_format { VK_FORMAT_UNDEFINED };
	
	// similar to image_info, but these contain per-level image views and infos
	// NOTE: image views are only created/used when the image is writable
	vector<VkImageView> mip_map_image_view;
	vector<VkDescriptorImageInfo> mip_map_image_info;
	void update_mip_map_info();
	
	//! separate create buffer function, b/c it's called by the constructor and resize
	bool create_internal(const bool copy_host_data, shared_ptr<compute_queue> cqueue);
	
	void image_copy_dev_to_host(VkCommandBuffer cmd_buffer, VkBuffer host_buffer) override;
	void image_copy_host_to_dev(VkCommandBuffer cmd_buffer, VkBuffer host_buffer, void* data) override;
	
};

#endif

#endif
