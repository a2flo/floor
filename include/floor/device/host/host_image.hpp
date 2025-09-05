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

#include <floor/device/host/host_common.hpp>

#if !defined(FLOOR_NO_HOST_COMPUTE)

#include <floor/device/device_image.hpp>
#include <floor/device/backend/host_limits.hpp>
#include <floor/core/aligned_ptr.hpp>

namespace fl {

class host_device;
class host_image final : public device_image {
public:
	host_image(const device_queue& cqueue,
			   const uint4 image_dim,
			   const IMAGE_TYPE image_type,
			   std::span<uint8_t> host_data_ = {},
			   const MEMORY_FLAG flags_ = (MEMORY_FLAG::HOST_READ_WRITE),
			   device_image* shared_image_ = nullptr,
			   const uint32_t mip_level_limit = 0u);
	
	~host_image() override;
	
	bool write(const device_queue& cqueue, const void* src, const size_t src_size,
			   const uint3 offset, const uint3 extent, const uint2 mip_level_range, const uint2 layer_range) override;
	
	bool zero(const device_queue& cqueue) override;
	
	void* __attribute__((aligned(128))) map(const device_queue& cqueue,
											const MEMORY_MAP_FLAG flags = (MEMORY_MAP_FLAG::READ_WRITE | MEMORY_MAP_FLAG::BLOCK)) override;
	
	bool unmap(const device_queue& cqueue, void* __attribute__((aligned(128))) mapped_ptr) override;
	
	//! returns a direct pointer to the internal host image buffer
	auto get_host_image_buffer_ptr() const {
		return image.get();
	}
	
	//! returns the internal structure necessary to run a function/program with this image
	void* get_host_image_program_info() const {
		return (void*)&program_info;
	}
	
	//! returns the internal structure necessary to run a function/program with this image and synchronizes buffer contents if synchronization flags are set
	void* get_host_image_program_info_with_sync() const;
	
	bool acquire_metal_image(const device_queue* cqueue, const metal_queue* mtl_queue) const override;
	bool release_metal_image(const device_queue* cqueue, const metal_queue* mtl_queue) const override;
	bool sync_metal_image(const device_queue* cqueue, const metal_queue* mtl_queue) const override;
	
	bool acquire_vulkan_image(const device_queue* cqueue, const vulkan_queue* vk_queue) const override;
	bool release_vulkan_image(const device_queue* cqueue, const vulkan_queue* vk_queue) const override;
	bool sync_vulkan_image(const device_queue* cqueue, const vulkan_queue* vk_queue) const override;
	
protected:
	mutable aligned_ptr<uint8_t> image;
	
	struct image_program_info {
		uint8_t* __attribute__((aligned(aligned_ptr<uint8_t>::page_size))) buffer;
		IMAGE_TYPE runtime_image_type;
		alignas(16) struct {
			union {
				uint4 dim {};
				struct {
					uint32_t dim_x;
					uint32_t dim_y;
					uint32_t dim_z;
					uint32_t offset;
				};
			};
			int4 clamp_dim_int;
			float4 clamp_dim_float;
			float4 clamp_dim_float_excl;
		} level_info[host_limits::max_mip_levels];
		static_assert(sizeof(level_info) == (16 * 4) * host_limits::max_mip_levels,
					  "invalid level_info size");
	} program_info;
	
	//! separate create image function, b/c it's called by the constructor and resize
	bool create_internal(const bool copy_host_data, const device_queue& cqueue);
	
	// internal Metal/Vulkan image when using Metal/Vulkan memory sharing (and not wrapping an existing image)
	std::shared_ptr<device_image> host_shared_image;
	// creates the internal Metal/Vulkan image, or deals with the wrapped external one
	bool create_shared_image(const bool copy_host_data);
	
};

} // namespace fl

#endif
