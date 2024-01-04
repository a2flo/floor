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

#ifndef __FLOOR_HOST_IMAGE_HPP__
#define __FLOOR_HOST_IMAGE_HPP__

#include <floor/compute/host/host_common.hpp>

#if !defined(FLOOR_NO_HOST_COMPUTE)

#include <floor/compute/compute_image.hpp>
#include <floor/compute/device/host_limits.hpp>
#include <floor/core/aligned_ptr.hpp>

class host_device;
class host_image final : public compute_image {
public:
	host_image(const compute_queue& cqueue,
			   const uint4 image_dim,
			   const COMPUTE_IMAGE_TYPE image_type,
			   std::span<uint8_t> host_data_ = {},
			   const COMPUTE_MEMORY_FLAG flags_ = (COMPUTE_MEMORY_FLAG::HOST_READ_WRITE),
			   const uint32_t opengl_type = 0,
			   const uint32_t external_gl_object_ = 0,
			   const opengl_image_info* gl_image_info = nullptr,
			   compute_image* shared_image_ = nullptr);
	
	~host_image() override;
	
	bool acquire_opengl_object(const compute_queue* cqueue) override;
	bool release_opengl_object(const compute_queue* cqueue) override;
	
	bool zero(const compute_queue& cqueue) override;
	
	void* __attribute__((aligned(128))) map(const compute_queue& cqueue,
											const COMPUTE_MEMORY_MAP_FLAG flags = (COMPUTE_MEMORY_MAP_FLAG::READ_WRITE | COMPUTE_MEMORY_MAP_FLAG::BLOCK)) override;
	
	bool unmap(const compute_queue& cqueue, void* __attribute__((aligned(128))) mapped_ptr) override;
	
	//! returns a direct pointer to the internal host image buffer
	auto get_host_image_buffer_ptr() const {
		return image.get();
	}
	
	//! returns the internal structure necessary to run a function/program with this image
	void* get_host_image_program_info() const {
		return (void*)&program_info;
	}
	
	bool acquire_metal_image(const compute_queue& cqueue, const metal_queue& mtl_queue) override;
	bool release_metal_image(const compute_queue& cqueue, const metal_queue& mtl_queue) override;
	bool sync_metal_image(const compute_queue* cqueue = nullptr,
						  const metal_queue* mtl_queue = nullptr) const override;
	
	bool acquire_vulkan_image(const compute_queue& cqueue, const vulkan_queue& vk_queue) override;
	bool release_vulkan_image(const compute_queue& cqueue, const vulkan_queue& vk_queue) override;
	bool sync_vulkan_image(const compute_queue* cqueue = nullptr,
						   const vulkan_queue* vk_queue = nullptr) const override;
	
protected:
	aligned_ptr<uint8_t> image;
	
	struct image_program_info {
		uint8_t* __attribute__((aligned(aligned_ptr<uint8_t>::page_size))) buffer;
		COMPUTE_IMAGE_TYPE runtime_image_type;
		alignas(16) struct {
			uint4 dim;
			int4 clamp_dim_int;
			float4 clamp_dim_float;
			uint32_t offset;
			const uint32_t _unused[3] { 0, 0, 0 };
		} level_info[host_limits::max_mip_levels];
		static_assert(sizeof(level_info) == (16 * 4) * host_limits::max_mip_levels,
					  "invalid level_info size");
	} program_info;
	
	//! separate create image function, b/c it's called by the constructor and resize
	bool create_internal(const bool copy_host_data, const compute_queue& cqueue);
	
	// internal Metal/Vulkan image when using Metal/Vulkan memory sharing (and not wrapping an existing image)
	shared_ptr<compute_image> host_shared_image;
	// creates the internal Metal/Vulkan image, or deals with the wrapped external one
	bool create_shared_image(const bool copy_host_data);
	
};

#endif

#endif
