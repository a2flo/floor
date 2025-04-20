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

#include <floor/device/cuda/cuda_common.hpp>

#if !defined(FLOOR_NO_CUDA)

#include <floor/device/device_image.hpp>
#include <floor/device/backend/cuda_sampler.hpp>
#include <floor/core/aligned_ptr.hpp>

namespace fl {

class cuda_device;
class cuda_context;
class cuda_buffer;
class vulkan_image;
class vulkan_semaphore;
class cuda_image final : public device_image {
public:
	cuda_image(const device_queue& cqueue,
			   const uint4 image_dim,
			   const IMAGE_TYPE image_type,
			   std::span<uint8_t> host_data_ = {},
			   const MEMORY_FLAG flags_ = (MEMORY_FLAG::HOST_READ_WRITE),
			   device_image* shared_image_ = nullptr,
			   const uint32_t mip_level_limit = 0u);
	
	~cuda_image() override;
	
	bool acquire_vulkan_image(const device_queue* cqueue, const vulkan_queue* vk_queue) const override;
	bool release_vulkan_image(const device_queue* cqueue, const vulkan_queue* vk_queue) const override;
	bool sync_vulkan_image(const device_queue*, const vulkan_queue*) const override {
		// nop, since it's backed by the same memory
		return true;
	}
	
	bool zero(const device_queue& cqueue) override;
	
	void* __attribute__((aligned(128))) map(const device_queue& cqueue,
											const MEMORY_MAP_FLAG flags =
											(MEMORY_MAP_FLAG::READ_WRITE |
											 MEMORY_MAP_FLAG::BLOCK)) override;
	
	bool unmap(const device_queue& cqueue, void* __attribute__((aligned(128))) mapped_ptr) override;
	
	//! returns the CUDA specific image pointer (array or mip-mapped array)
	const void* get_cuda_image() const { return image; }
	
	//! returns the CUDA surface objects
	const auto& get_cuda_surfaces() const {
		return surfaces;
	}
	
	//! returns the CUDA buffer containing all lod surface objects (on the device)
	const cuda_buffer* get_cuda_surfaces_lod_buffer() const {
		return (surfaces_lod_buffer != nullptr ? surfaces_lod_buffer.get() : nullptr);
	}
	
	//! returns the CUDA texture objects
	const auto& get_cuda_textures() const {
		return textures;
	}
	
	//! internal function - initialized once by cuda_context
	static void init_internal(cuda_context* ctx);
	
	//! when the internal CUDA API is used, this function will be called by the CUDA driver when
	//! creating a texture object to initialize/create the sampler state of the texture
	static CU_API CU_RESULT internal_device_sampler_init(cu_texture_ref tex_ref);
	
protected:
	// generic image pointer (identical to either image_array or image_mipmap_array)
	void* image { nullptr };
	cu_array image_array { nullptr };
	cu_mip_mapped_array image_mipmap_array { nullptr };
	cu_graphics_resource rsrc { nullptr };
	
	// contains the cu_array for each mip-level
	std::vector<cu_array> image_mipmap_arrays;
	
	// only need one surface object per mip-level (only needs to point to a cu_array)
	std::vector<cu_surf_object> surfaces;
	std::shared_ptr<cuda_buffer> surfaces_lod_buffer;
	
	// the way CUDA reads/samples images must be specified in the host API, which will basically
	// create a combined texture+sampler object -> need to create these for all possible types
	std::array<cu_tex_only_object, cuda_sampler::max_sampler_count> textures;
	
	struct cuda_mapping {
		aligned_ptr<uint8_t> ptr;
		const MEMORY_MAP_FLAG flags;
	};
	// stores all mapped pointers and the mapped buffer
	std::unordered_map<void*, cuda_mapping> mappings;
	
	//! separate create image function, b/c it's called by the constructor and resize
	bool create_internal(const bool copy_host_data, const device_queue& cqueue);
	
#if !defined(FLOOR_NO_VULKAN)
	// external (Vulkan) memory
	cu_external_memory ext_memory { nullptr };
	// internal Vulkan image when using Vulkan memory sharing (and not wrapping an existing image)
	std::shared_ptr<device_image> cuda_vk_image;
	// external (Vulkan) semaphore
	cu_external_semaphore ext_sema { nullptr };
	// internal Vulkan semaphore when using Vulkan memory sharing, used to sync buffer access
	std::unique_ptr<vulkan_semaphore> cuda_vk_sema;
	// creates the internal Vulkan buffer, or deals with the wrapped external one
	bool create_shared_vulkan_image(const bool copy_host_data);
#endif
	// external/Vulkan images are always imported as mip-mapped arrays -> add an easy to check flag to handle both cases
	const bool is_mip_mapped_or_vulkan { false };
	
};

} // namespace fl

#endif
