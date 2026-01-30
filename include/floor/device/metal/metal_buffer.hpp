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
#include <floor/device/device_buffer.hpp>
#include <floor/core/aligned_ptr.hpp>
#if defined(__OBJC__)
#include <Metal/Metal.h>
#endif

namespace fl {

class metal_device;
class device;
class metal_buffer final : public device_buffer {
public:
	metal_buffer(const device_queue& cqueue,
				 const size_t& size_,
				 std::span<uint8_t> host_data_,
				 const MEMORY_FLAG flags_ = (MEMORY_FLAG::READ_WRITE |
													 MEMORY_FLAG::HOST_READ_WRITE)) :
	metal_buffer(false, cqueue, size_, host_data_, flags_) {}
	
	metal_buffer(const device_queue& cqueue,
				 const size_t& size_,
				 const MEMORY_FLAG flags_ = (MEMORY_FLAG::READ_WRITE |
													 MEMORY_FLAG::HOST_READ_WRITE)) :
	metal_buffer(false, cqueue, size_, {}, flags_) {}
	
#if defined(__OBJC__)
	//! wraps an already existing Metal buffer, with the specified flags and backed by the specified host pointer
	metal_buffer(const device_queue& cqueue,
				 id <MTLBuffer> floor_nonnull external_buffer,
				 std::span<uint8_t> host_data_ = {},
				 const MEMORY_FLAG flags_ = (MEMORY_FLAG::READ_WRITE |
													 MEMORY_FLAG::HOST_READ_WRITE));
#endif
	
	~metal_buffer() override;
	
	void read(const device_queue& cqueue, const size_t size = 0, const size_t offset = 0) const override;
	void read(const device_queue& cqueue, void* floor_nullable dst, const size_t size = 0, const size_t offset = 0) const override;
	
	void write(const device_queue& cqueue, const size_t size = 0, const size_t offset = 0) override;
	void write(const device_queue& cqueue, const void* floor_nonnull src, const size_t size = 0, const size_t offset = 0) override;
	
	void copy(const device_queue& cqueue, const device_buffer& src,
			  const size_t size = 0, const size_t src_offset = 0, const size_t dst_offset = 0) override;
	
	bool fill(const device_queue& cqueue,
			  const void* floor_nonnull pattern, const size_t& pattern_size,
			  const size_t size = 0, const size_t offset = 0) override;
	
	bool zero(const device_queue& cqueue) override;
	
	void* floor_nullable __attribute__((aligned(128))) map(const device_queue& cqueue,
														   const MEMORY_MAP_FLAG flags = (MEMORY_MAP_FLAG::READ_WRITE | MEMORY_MAP_FLAG::BLOCK),
														   const size_t size = 0,
														   const size_t offset = 0) override;
	
	bool unmap(const device_queue& cqueue, void* floor_nonnull __attribute__((aligned(128))) mapped_ptr) override;
	
	void set_debug_label(const std::string& label) override;
	
	bool is_heap_allocated() const override {
		return is_heap_buffer;
	}
	
#if defined(__OBJC__)
	//! returns the Metal specific buffer object
	id <MTLBuffer> floor_nonnull get_metal_buffer() const { return buffer; }
	
	//! returns the MTLResourceOptions of this buffer
	MTLResourceOptions get_metal_resource_options() const { return options; }
	
	//! returns true if the specified resource type/options requires CPU/GPU sync
	static bool metal_resource_type_needs_sync(const MTLResourceOptions& opts) {
#if !defined(FLOOR_IOS) && !defined(FLOOR_VISIONOS)
		return ((opts & MTLResourceStorageModeMask) == MTLResourceStorageModeManaged ||
				(opts & MTLResourceStorageModeMask) == MTLResourceStorageModeShared);
#else
		return ((opts & MTLResourceStorageModeMask) == MTLResourceStorageModeShared);
#endif
	}
	
	//! helper function for MTLResourceStorageModeManaged buffers/images (need to sync before read on CPU)
	static void sync_metal_resource(const device_queue& cqueue, id <MTLResource> __unsafe_unretained floor_nonnull rsrc);
#endif
	
	//! returns the null-buffer for the specified device
	static const device_buffer* floor_nullable get_null_buffer(const device& dev);
	
	//! potential staging constructor so that we can decide whether a staging buffer is created
	metal_buffer(const bool is_staging_buffer_,
				 const device_queue& cqueue,
				 const size_t& size_,
				 std::span<uint8_t> host_data_,
				 const MEMORY_FLAG flags_);
	
protected:
#if defined(__OBJC__)
	id <MTLBuffer> floor_null_unspecified buffer { nil };
#else
	void* floor_null_unspecified buffer { nullptr };
#endif
	std::unique_ptr<metal_buffer> staging_buffer;
	bool is_external { false };
	bool is_staging_buffer { false };
	bool is_heap_buffer { false };
	
#if defined(__OBJC__)
	MTLResourceOptions options { MTLCPUCacheModeDefaultCache };
#endif
	
	struct metal_mapping {
		aligned_ptr<uint8_t> ptr;
		const size_t size;
		const size_t offset;
		const MEMORY_MAP_FLAG flags;
		const bool write_only;
		const bool read_only;
	};
	// stores all mapped pointers and the mapped buffer
	std::unordered_map<void*, metal_mapping> mappings;
	
	// separate create buffer function, b/c it's called by the constructor and resize
	bool create_internal(const bool copy_host_data, const device_queue& cqueue);
	
};

} // namespace fl

#endif
