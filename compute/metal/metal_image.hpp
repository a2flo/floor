/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2020 Florian Ziesche
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

#ifndef __FLOOR_METAL_IMAGE_HPP__
#define __FLOOR_METAL_IMAGE_HPP__

#include <floor/compute/metal/metal_common.hpp>

#if !defined(FLOOR_NO_METAL)

#include <floor/threading/atomic_spin_lock.hpp>
#include <floor/compute/compute_image.hpp>
#if defined(__OBJC__)
#include <Metal/Metal.h>
#endif

class metal_device;
class compute_device;
class metal_image final : public compute_image {
public:
	metal_image(const compute_queue& cqueue,
				const uint4 image_dim,
				const COMPUTE_IMAGE_TYPE image_type,
				void* floor_nullable host_ptr = nullptr,
				const COMPUTE_MEMORY_FLAG flags_ = (COMPUTE_MEMORY_FLAG::HOST_READ_WRITE),
				const uint32_t opengl_type = 0,
				const uint32_t external_gl_object_ = 0,
				const opengl_image_info* floor_nullable gl_image_info = nullptr);

#if defined(__OBJC__)
	//! wraps an already existing metal image, with the specified flags and backed by the specified host pointer
	metal_image(const compute_queue& cqueue,
				id <MTLTexture> floor_nonnull external_image,
				void* floor_nullable host_ptr = nullptr,
				const COMPUTE_MEMORY_FLAG flags_ = (COMPUTE_MEMORY_FLAG::READ_WRITE |
													COMPUTE_MEMORY_FLAG::HOST_READ_WRITE));
#endif
	
	~metal_image() override;
	
	bool blit(const compute_queue& cqueue, const compute_image& src) override;
	
	bool acquire_opengl_object(const compute_queue* floor_nullable cqueue) override;
	bool release_opengl_object(const compute_queue* floor_nullable cqueue) override;
	
	void zero(const compute_queue& cqueue) override;
	
	void* floor_nullable __attribute__((aligned(128))) map(const compute_queue& cqueue,
														   const COMPUTE_MEMORY_MAP_FLAG flags =
														   (COMPUTE_MEMORY_MAP_FLAG::READ_WRITE |
															COMPUTE_MEMORY_MAP_FLAG::BLOCK)) override;
	
	void unmap(const compute_queue& cqueue,
			   void* floor_nullable __attribute__((aligned(128))) mapped_ptr) override;
	
	//! returns the metal specific image object
#if defined(__OBJC__)
	id <MTLTexture> floor_nullable get_metal_image() const { return image; }
#endif
	
	//! returns the metal specific image object as a void*
	void* floor_nullable get_metal_image_void_ptr() const;
	
	//! creates the mip-map chain for this metal image
	void generate_mip_map_chain(const compute_queue& cqueue) override;
	
	//! returns the corresponding MTLPixelFormat for the specified COMPUTE_IMAGE_TYPE,
	//! or nothing if there is no matching pixel format
#if defined(__OBJC__)
	static optional<MTLPixelFormat> metal_pixel_format_from_image_type(const COMPUTE_IMAGE_TYPE& image_type_);
#endif
	
protected:
#if defined(__OBJC__)
	id <MTLTexture> floor_null_unspecified image { nil };
	MTLTextureDescriptor* floor_null_unspecified desc { nil };
#else
	void* floor_null_unspecified image { nullptr };
	void* floor_null_unspecified desc { nullptr };
#endif
	bool is_external { false };

#if defined(__OBJC__)
	MTLResourceOptions options { MTLCPUCacheModeDefaultCache };
	MTLTextureUsage usage_options { MTLTextureUsageUnknown };
	MTLStorageMode storage_options { MTLStorageModeShared };
#endif
	
	struct metal_mapping {
		const COMPUTE_MEMORY_MAP_FLAG flags;
		const bool write_only;
	};
	// stores all mapped pointers and the mapped buffer
	unordered_map<void*, metal_mapping> mappings;
	
	// separate create buffer function, b/c it's called by the constructor and resize
	bool create_internal(const bool copy_host_data, const compute_queue& cqueue);
	
};

#endif

#endif
