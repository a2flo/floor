/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2026 Florian Ziesche
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

#include <floor/device/metal/metal_common.hpp>

#if !defined(FLOOR_NO_METAL)

#include <floor/threading/atomic_spin_lock.hpp>
#include <floor/device/device_image.hpp>
#include <floor/core/aligned_ptr.hpp>
#if defined(__OBJC__)
#include <Metal/Metal.h>
#endif

namespace fl {

class metal_device;
class device;
class metal4_image final : public device_image {
public:
	metal4_image(const device_queue& cqueue,
				 const uint4 image_dim,
				 const IMAGE_TYPE image_type,
				 std::span<uint8_t> host_data_ = {},
				 const MEMORY_FLAG flags_ = (MEMORY_FLAG::HOST_READ_WRITE),
				 const uint32_t mip_level_limit = 0u,
				 const char* floor_nullable debug_label_ = nullptr);
	
#if defined(__OBJC__)
	//! wraps an already existing Metal image, with the specified flags and backed by the specified host pointer
	metal4_image(const device_queue& cqueue,
				 id <MTLTexture> floor_nonnull external_image,
				 std::span<uint8_t> host_data_ = {},
				 const MEMORY_FLAG flags_ = (MEMORY_FLAG::READ_WRITE |
											 MEMORY_FLAG::HOST_READ_WRITE),
				 const char* floor_nullable debug_label_ = nullptr);
#endif
	
	~metal4_image() override;
	
	bool blit(const device_queue& cqueue, device_image& src) override;
	bool blit_async(const device_queue& cqueue, device_image& src,
					std::vector<const device_fence*>&& wait_fences,
					std::vector<device_fence*>&& signal_fences) override;
	
	bool write(const device_queue& cqueue, const void* floor_nullable src, const size_t src_size,
			   const uint3 offset, const uint3 extent, const uint2 mip_level_range, const uint2 layer_range) override;
	
	bool zero(const device_queue& cqueue) override;
	
	void* floor_nullable __attribute__((aligned(128))) map(const device_queue& cqueue,
														   const MEMORY_MAP_FLAG flags =
														   (MEMORY_MAP_FLAG::READ_WRITE |
															MEMORY_MAP_FLAG::BLOCK)) override;
	
	bool unmap(const device_queue& cqueue, void* floor_nullable __attribute__((aligned(128))) mapped_ptr, const bool discard = false) override;
	
	void set_debug_label(const std::string& label) override;
	
	bool is_heap_allocated() const override {
		return is_heap_image;
	}
	
	//! returns the Metal specific image object
#if defined(__OBJC__)
	id <MTLTexture> floor_nonnull get_metal_image() const { return image; }
#endif
	
	//! returns the Metal specific image object as a void*
	void* floor_nullable get_metal_image_void_ptr() const;
	
	//! creates the mip-map chain for this Metal image
	void generate_mip_map_chain(const device_queue& cqueue) override;
	
protected:
#if defined(__OBJC__)
	id <MTLTexture> floor_null_unspecified image { nil };
	MTLTextureDescriptor* floor_null_unspecified desc { nil };
	MTLResourceOptions options { MTLCPUCacheModeDefaultCache };
	MTLTextureUsage usage_options { MTLTextureUsageUnknown };
#else
	void* floor_null_unspecified image { nullptr };
	void* floor_null_unspecified desc { nullptr };
	uint64_t options { 0u };
	uint64_t usage_options { 0u };
#endif
	
	bool is_external { false };
	bool is_heap_image { false };
	
	struct metal_mapping {
		aligned_ptr<uint8_t> ptr;
		const MEMORY_MAP_FLAG flags;
		const bool write_only;
	};
	// stores all mapped pointers and the mapped buffer
	std::unordered_map<void*, metal_mapping> mappings;
	
	// separate create buffer function, b/c it's called by the constructor and resize
	bool create_internal(const bool copy_host_data, const device_queue& cqueue);
	
	bool blit_internal(const bool is_async, const device_queue& cqueue, device_image& src,
					   const std::vector<const device_fence*>& wait_fences,
					   const std::vector<device_fence*>& signal_fences);
	
};

} // namespace fl

#endif
