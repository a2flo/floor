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

#include <floor/device/device_image.hpp>
#include <floor/device/vulkan/vulkan_memory.hpp>

namespace fl {

struct external_vulkan_image_info;
class vulkan_device;

class vulkan_image : public device_image, public vulkan_memory {
public:
	bool zero(const device_queue& cqueue) override;
	
	bool blit(const device_queue& cqueue, device_image& src) override;
	bool blit_async(const device_queue& cqueue, device_image& src,
					std::vector<const device_fence*>&& wait_fences,
					std::vector<device_fence*>&& signal_fences) override;
	
	void* __attribute__((aligned(128))) map(const device_queue& cqueue,
											const MEMORY_MAP_FLAG flags = (MEMORY_MAP_FLAG::READ_WRITE | MEMORY_MAP_FLAG::BLOCK)) override;
	
	bool unmap(const device_queue& cqueue, void* __attribute__((aligned(128))) mapped_ptr) override;
	
	//! returns the Vulkan specific image object/pointer
	const VkImage& get_vulkan_image() const {
		return image;
	}
	
	//! returns the Vulkan specific image view object
	const VkImageView& get_vulkan_image_view() const {
		return image_view;
	}
	
	//! returns the current Vulkan specific access mask
	const VkAccessFlags2& get_vulkan_access_mask() const {
		return cur_access_mask;
	}
	
	//! returns the Vulkan shared memory handle (nullptr/0 if !shared)
	const auto& get_vulkan_shared_handle() const {
		return shared_handle;
	}
	
	//! returns the actual allocation size in bytes this image has been created with
	//! NOTE: allocation size has been computed via vkGetImageMemoryRequirements2 and can differ from "size"
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
	
	void set_debug_label(const std::string& label) override;
	
	bool is_heap_allocated() const override {
		return is_heap_allocation;
	}
	
	//! returns the descriptor data for this buffer (for use in descriptor buffers)
	std::span<const uint8_t> get_vulkan_descriptor_data_sampled() const {
		return { descriptor_data_sampled.get(), descriptor_sampled_size };
	}
	
	//! returns the descriptor data for this buffer (for use in descriptor buffers)
	std::span<const uint8_t> get_vulkan_descriptor_data_storage() const {
		return { descriptor_data_storage.get(), descriptor_storage_size };
	}
	
protected:
	VkImage image { nullptr };
	VkImageView image_view { nullptr };
	VkAccessFlags2 cur_access_mask { 0 };
	VkDeviceSize allocation_size { 0 };
	bool is_external { false };
	
	// contains each individual layer of an image array that has been created with aliasing support
	std::vector<VkImage> image_aliased_layers;
	
	// similar to image_info, but these contain per-level image views and infos
	// NOTE: image views are only created/used when the image is writable
	std::vector<VkImageView> mip_map_image_view;
	
	//! when using descriptor buffers, this contains the descriptor data (sampled and storage descriptor)
	size_t descriptor_sampled_size = 0, descriptor_storage_size = 0;
	std::unique_ptr<uint8_t[]> descriptor_data_sampled;
	std::unique_ptr<uint8_t[]> descriptor_data_storage;
	
	// shared memory handle when the image has been created with VULKAN_SHARING
#if defined(__WINDOWS__)
	void* /* HANDLE */ shared_handle { nullptr };
#else
	int shared_handle { 0 };
#endif
	
	void image_copy_dev_to_host(const device_queue& cqueue,
								VkCommandBuffer cmd_buffer, VkBuffer host_buffer) override;
	void image_copy_host_to_dev(const device_queue& cqueue,
								VkCommandBuffer cmd_buffer, VkBuffer host_buffer, std::span<uint8_t> data) override;
	
	bool blit_internal(const bool is_async, const device_queue& cqueue, device_image& src,
					   const std::vector<const device_fence*>& wait_fences,
					   const std::vector<device_fence*>& signal_fences);
	
	std::vector<VkBufferImageCopy2> build_image_buffer_copy_regions(const bool with_shim_type) const;
	
	vulkan_image(const device_queue& cqueue,
				 const uint4 image_dim,
				 const IMAGE_TYPE image_type,
				 std::span<uint8_t> host_data_ = {},
				 const MEMORY_FLAG flags_ = (MEMORY_FLAG::HOST_READ_WRITE),
				 const uint32_t mip_level_limit = 0u);
	
	~vulkan_image() override;
	
	//! wraps an already existing Vulkan image, with the specified flags and backed by the specified host pointer
	vulkan_image(const device_queue& cqueue,
				 const external_vulkan_image_info& external_image,
				 std::span<uint8_t> host_data_,
				 const MEMORY_FLAG flags_);
	
};

} // namespace fl

#endif
