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
	
	//! returns the vulkan specific image object/pointer
	const VkImage& get_vulkan_image() const {
		return image;
	}
	
protected:
	VkImage image { nullptr };
	VkImageView image_view { nullptr };
	
	//! separate create buffer function, b/c it's called by the constructor and resize
	bool create_internal(const bool copy_host_data, shared_ptr<compute_queue> cqueue);
	
	void image_copy_dev_to_host(VkCommandBuffer cmd_buffer, VkBuffer host_buffer) override;
	void image_copy_host_to_dev(VkCommandBuffer cmd_buffer, VkBuffer host_buffer) override;
	
};

#endif

#endif
