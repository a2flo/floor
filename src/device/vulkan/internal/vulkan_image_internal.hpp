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

#include <floor/core/essentials.hpp>

#if !defined(FLOOR_NO_VULKAN)
#include <floor/device/vulkan/vulkan_image.hpp>
#include "vulkan_headers.hpp"

namespace fl {

//! image info used for wrapping an existing Vulkan image
//! NOTE: since Vulkan has no image query functionality, this needs to be specified manually
struct external_vulkan_image_info {
	VkImage image { nullptr };
	VkImageView image_view { nullptr };
	VkFormat format { VK_FORMAT_UNDEFINED };
	VkAccessFlags2 access_mask { 0 };
	VkImageLayout layout { VK_IMAGE_LAYOUT_UNDEFINED };
	//! any of IMAGE_1D, IMAGE_2D, IMAGE_3D, ...
	IMAGE_TYPE image_base_type { IMAGE_TYPE::IMAGE_2D };
	uint4 dim;
};

//! computes the IMAGE_TYPE for the specified external Vulkan image info and flags
IMAGE_TYPE compute_vulkan_image_type(const external_vulkan_image_info& info, const MEMORY_FLAG flags);

class vulkan_image_internal : public vulkan_image {
public:
	vulkan_image_internal(const device_queue& cqueue,
						  const uint4 image_dim,
						  const IMAGE_TYPE image_type,
						  std::span<uint8_t> host_data_ = {},
						  const MEMORY_FLAG flags_ = (MEMORY_FLAG::HOST_READ_WRITE),
						  const uint32_t mip_level_limit = 0u);
	
	//! wraps an already existing Vulkan image, with the specified flags and backed by the specified host pointer
	vulkan_image_internal(const device_queue& cqueue,
						  const external_vulkan_image_info& external_image,
						  std::span<uint8_t> host_data_ = {},
						  const MEMORY_FLAG flags_ = (MEMORY_FLAG::READ_WRITE |
													  MEMORY_FLAG::HOST_READ_WRITE));
	
	~vulkan_image_internal() override;
	
	//! returns the Vulkan image format that is used for this image
	VkFormat get_vulkan_format() const {
		return vk_format;
	}
	
	//! returns the image descriptor info of this image
	const VkDescriptorImageInfo* get_vulkan_image_info() const {
		return &image_info;
	}
	
	//! returns the mip-map image descriptor info array of this image
	const std::vector<VkDescriptorImageInfo>& get_vulkan_mip_map_image_info() const {
		return mip_map_image_info;
	}
	
	//! updates the Vulkan image layout and current access mask with the specified ones
	//! NOTE: this is useful when the Vulkan image/state is changed externally and we want to keep this in sync
	void update_with_external_vulkan_state(const VkImageLayout& layout, const VkAccessFlags2& access);
	
	//! transitions this image into a new 'layout', with specified 'access', at src/dst stage
	//! if "cmd_buffer" is nullptr, a new one will be created and enqueued/submitted in the end (unless "soft_transition" is also set)
	//! if "soft_transition" is set, this won't encode a pipeline barrier in the specified cmd buffer (must manually use returned VkImageMemoryBarrier)
	std::pair<bool, VkImageMemoryBarrier2> transition(const device_queue* cqueue,
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
	std::pair<bool, VkImageMemoryBarrier2> transition_read(const device_queue* cqueue, VkCommandBuffer cmd_buffer,
														   const bool allow_general_layout = false,
														   const bool soft_transition = false);
	//! transition for shader or attachment write (if not already in this mode)
	//! returns true if a transition was performed, false if none was necessary
	//! if "read_write" is set, will also make the image readable
	//! if "is_rt_direct_write" is set and the image is a render-target, transition to "general" layout instead of "attachment" layout
	//! if "allow_general_layout" is set and the current layout is "general", the transition will be skipped
	//! if "soft_transition" is set, this won't encode a pipeline barrier in the specified cmd buffer (must manually use returned VkImageMemoryBarrier)
	std::pair<bool, VkImageMemoryBarrier2> transition_write(const device_queue* cqueue, VkCommandBuffer cmd_buffer,
															const bool read_write = false,
															const bool is_rt_direct_write = false,
															const bool allow_general_layout = false,
															const bool soft_transition = false);
	
protected:
	friend vulkan_image;
	
	VkFormat vk_format { VK_FORMAT_UNDEFINED };
	VkDescriptorImageInfo image_info { nullptr, nullptr, VK_IMAGE_LAYOUT_UNDEFINED };
	
	std::vector<VkDescriptorImageInfo> mip_map_image_info;
	void update_mip_map_info();
	
	//! separate create buffer function, b/c it's called by the constructor and resize
	bool create_internal(const bool copy_host_data, const device_queue& cqueue, const VkImageUsageFlags& usage);
	
};

} // namespace fl

#endif // FLOOR_NO_VULKAN
