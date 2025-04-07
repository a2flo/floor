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

#include <floor/compute/metal/metal_common.hpp>

#if !defined(FLOOR_NO_METAL)

#include <floor/threading/atomic_spin_lock.hpp>
#include <floor/compute/compute_image.hpp>
#include <floor/core/aligned_ptr.hpp>
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
				std::span<uint8_t> host_data_ = {},
				const COMPUTE_MEMORY_FLAG flags_ = (COMPUTE_MEMORY_FLAG::HOST_READ_WRITE));

#if defined(__OBJC__)
	//! wraps an already existing metal image, with the specified flags and backed by the specified host pointer
	metal_image(const compute_queue& cqueue,
				id <MTLTexture> floor_nonnull external_image,
				std::span<uint8_t> host_data_ = {},
				const COMPUTE_MEMORY_FLAG flags_ = (COMPUTE_MEMORY_FLAG::READ_WRITE |
													COMPUTE_MEMORY_FLAG::HOST_READ_WRITE));
#endif
	
	~metal_image() override;
	
	bool blit(const compute_queue& cqueue, compute_image& src) override;
	bool blit_async(const compute_queue& cqueue, compute_image& src,
					vector<const compute_fence*>&& wait_fences,
					vector<compute_fence*>&& signal_fences) override;
	
	bool zero(const compute_queue& cqueue) override;
	
	void* floor_nullable __attribute__((aligned(128))) map(const compute_queue& cqueue,
														   const COMPUTE_MEMORY_MAP_FLAG flags =
														   (COMPUTE_MEMORY_MAP_FLAG::READ_WRITE |
															COMPUTE_MEMORY_MAP_FLAG::BLOCK)) override;
	
	bool unmap(const compute_queue& cqueue,
			   void* floor_nullable __attribute__((aligned(128))) mapped_ptr) override;
	
	void set_debug_label(const string& label) override;
	
	bool is_heap_allocated() const override {
		return is_heap_image;
	}
	
	//! returns the metal specific image object
#if defined(__OBJC__)
	id <MTLTexture> floor_nonnull get_metal_image() const { return image; }
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
	bool is_heap_image { false };

#if defined(__OBJC__)
	MTLResourceOptions options { MTLCPUCacheModeDefaultCache };
	MTLTextureUsage usage_options { MTLTextureUsageUnknown };
	MTLStorageMode storage_options { MTLStorageModeShared };
#endif
	
	struct metal_mapping {
		aligned_ptr<uint8_t> ptr;
		const COMPUTE_MEMORY_MAP_FLAG flags;
		const bool write_only;
	};
	// stores all mapped pointers and the mapped buffer
	unordered_map<void*, metal_mapping> mappings;
	
	// separate create buffer function, b/c it's called by the constructor and resize
	bool create_internal(const bool copy_host_data, const compute_queue& cqueue);
	
	bool blit_internal(const bool is_async, const compute_queue& cqueue, compute_image& src,
					   const vector<const compute_fence*>& wait_fences,
					   const vector<compute_fence*>& signal_fences);
	
};

#endif
