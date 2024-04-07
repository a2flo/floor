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

#pragma once

#include <floor/compute/compute_memory.hpp>

FLOOR_PUSH_WARNINGS()
FLOOR_IGNORE_WARNING(weak-vtables)

class vulkan_buffer;
class metal_buffer;
class vulkan_queue;
class metal_queue;

class compute_buffer : public compute_memory {
public:
	~compute_buffer() override = default;
	
	//! reads "size" bytes (or the complete buffer if 0) from "offset" onwards
	//! back to the previously specified host pointer
	virtual void read(const compute_queue& cqueue, const size_t size = 0, const size_t offset = 0) = 0;
	//! reads "size" bytes (or the complete buffer if 0) from "offset" onwards
	//! back to the specified "dst" pointer
	virtual void read(const compute_queue& cqueue, void* dst, const size_t size = 0, const size_t offset = 0) = 0;
	
	//! reads sizeof("dst") bytes from "offset" onwards to "dst"
	template <typename T> floor_inline_always void read_to(T& dst, const compute_queue& cqueue, const size_t offset_ = 0) {
		read(cqueue, &dst, sizeof(T), offset_);
	}
	
	//! writes "size" bytes (or the complete buffer if 0) from "offset" onwards
	//! from the previously specified host pointer to this buffer
	virtual void write(const compute_queue& cqueue, const size_t size = 0, const size_t offset = 0) = 0;
	//! writes "size" bytes (or the complete buffer if 0) from "offset" onwards
	//! from the specified "src" pointer to this buffer
	virtual void write(const compute_queue& cqueue, const void* src, const size_t size = 0, const size_t offset = 0) = 0;
	//! writes all of "src" to this buffer, from "offset" on onwards
	template <typename data_type>
	void write(const compute_queue& cqueue, const vector<data_type>& src, const size_t offset_ = 0) {
		write(cqueue, (const void*)&src[0], sizeof(data_type) * src.size(), offset_);
	}
	//! writes all of "src" to this buffer, from "offset" on onwards
	template <typename data_type, size_t n>
	void write(const compute_queue& cqueue, const array<data_type, n>& src, const size_t offset_ = 0) {
		write(cqueue, (const void*)&src[0], sizeof(data_type) * n, offset_);
	}
	
	//! writes sizeof("src") bytes from "src" to "offset" onwards to this buffer
	template <typename T> floor_inline_always void write_from(const T& src, const compute_queue& cqueue, const size_t offset_ = 0) {
		write(cqueue, &src, sizeof(T), offset_);
	}
	
	//! copies data from the specified "src" buffer to this buffer, of the specified "size" (complete buffer if size == 0),
	//! from "src_offset" in the "src" buffer to "dst_offset" in this buffer
	virtual void copy(const compute_queue& cqueue, const compute_buffer& src,
					  const size_t size = 0, const size_t src_offset = 0, const size_t dst_offset = 0) = 0;
	
	//! clones this buffer, optionally copying its contents as well
	virtual shared_ptr<compute_buffer> clone(const compute_queue& cqueue, const bool copy_contents = false,
											 const COMPUTE_MEMORY_FLAG flags_override = COMPUTE_MEMORY_FLAG::NONE);
	
	//! fills this buffer with the provided "pattern" of size "pattern_size" (in bytes),
	//! returns true on success
	//! NOTE: filling the buffer with patterns that are 1 byte, 2 bytes, 4 bytes, 8 bytes or 16 bytes in size might be faster than other sizes
	virtual bool fill(const compute_queue& cqueue,
					  const void* pattern, const size_t& pattern_size,
					  const size_t size = 0, const size_t offset = 0) = 0;
	
	//! maps device memory into host accessible memory, of the specified "size" (0 = complete buffer) and buffer "offset"
	//! NOTE: this might require a complete buffer copy on map and/or unmap (use READ, WRITE and WRITE_INVALIDATE appropriately)
	//! NOTE: this call might block regardless of if the BLOCK flag is set or not
	virtual void* __attribute__((aligned(128))) map(const compute_queue& cqueue,
													const COMPUTE_MEMORY_MAP_FLAG flags = (COMPUTE_MEMORY_MAP_FLAG::READ_WRITE | COMPUTE_MEMORY_MAP_FLAG::BLOCK),
													const size_t size = 0, const size_t offset = 0) = 0;
	
	//! maps device memory into host accessible memory, of the specified "size" (0 = complete buffer) and buffer "offset",
	//! returning the mapped pointer as an array<> of "data_type" with "n" elements
	//! NOTE: this might require a complete buffer copy on map and/or unmap (use READ, WRITE and WRITE_INVALIDATE appropriately)
	//! NOTE: this call might block regardless of if the BLOCK flag is set or not
	template <typename data_type, size_t n>
	array<data_type, n>* map(const compute_queue& cqueue,
							 const COMPUTE_MEMORY_MAP_FLAG flags_ = (COMPUTE_MEMORY_MAP_FLAG::READ_WRITE | COMPUTE_MEMORY_MAP_FLAG::BLOCK),
							 const size_t size_ = 0, const size_t offset_ = 0) {
		return (array<data_type, n>*)map(cqueue, flags_, size_, offset_);
	}
	
	//! unmaps a previously mapped memory pointer, returns true on success
	//! NOTE: this might require a complete buffer copy on map and/or unmap (use READ, WRITE and WRITE_INVALIDATE appropriately)
	//! NOTE: this call might block regardless of if the BLOCK flag is set or not
	virtual bool unmap(const compute_queue& cqueue, void* __attribute__((aligned(128))) mapped_ptr) = 0;
	
	//! returns the size of this buffer (in bytes)
	const size_t& get_size() const { return size; }
	
	//! returns the internal shared Metal buffer if there is one, returns nullptr otherwise
	const metal_buffer* get_shared_metal_buffer() const {
		return shared_mtl_buffer;
	}
	
	//! acquires the associated Metal buffer for use with compute (-> release from Metal use)
	//! NOTE: "cqueue" must be a compute_queue of the compute context (or nullptr), "mtl_queue" must be a compute_queue of the Metal context (or nullptr)
	virtual bool acquire_metal_buffer(const compute_queue* cqueue floor_unused, const metal_queue* mtl_queue floor_unused) const {
		return false;
	}
	//! releases the associated Metal buffer from use with compute (-> acquire for Metal use)
	//! NOTE: "cqueue" must be a compute_queue of the compute context (or nullptr), "mtl_queue" must be a compute_queue of the Metal context (or nullptr)
	virtual bool release_metal_buffer(const compute_queue* cqueue floor_unused, const metal_queue* mtl_queue floor_unused) const {
		return false;
	}
	//! synchronizes the contents of this buffer with the shared Metal buffer
	//! NOTE: "cqueue" must be a compute_queue of the compute context (or nullptr), "mtl_queue" must be a compute_queue of the Metal context (or nullptr)
	virtual bool sync_metal_buffer(const compute_queue* cqueue floor_unused, const metal_queue* mtl_queue floor_unused) const {
		return false;
	}
	
	//! returns the underlying Metal buffer that should be used on the device (i.e. this or a shared buffer)
	//! NOTE: when synchronization flags are set, this may synchronize buffer contents
	const metal_buffer* get_underlying_metal_buffer_safe() const;
	
	//! returns the internal shared Vulkan buffer if there is one, returns nullptr otherwise
	const vulkan_buffer* get_shared_vulkan_buffer() const {
		return shared_vk_buffer;
	}
	
	//! acquires the associated Vulkan buffer for use with compute (-> release from Vulkan use)
	//! NOTE: "cqueue" must be a compute_queue of the compute context, "vk_queue" must be a compute_queue of the Vulkan context
	virtual bool acquire_vulkan_buffer(const compute_queue* cqueue floor_unused, const vulkan_queue* vk_queue floor_unused) const {
		return false;
	}
	//! releases the associated Vulkan buffer from use with compute (-> acquire for Vulkan use)
	//! NOTE: "cqueue" must be a compute_queue of the compute context, "vk_queue" must be a compute_queue of the Vulkan context
	virtual bool release_vulkan_buffer(const compute_queue* cqueue floor_unused, const vulkan_queue* vk_queue floor_unused) const {
		return false;
	}
	//! synchronizes the contents of this buffer with the shared Vulkan buffer
	//! NOTE: "cqueue" must be a compute_queue of the compute context (or nullptr), "vk_queue" must be a compute_queue of the Vulkan context (or nullptr)
	virtual bool sync_vulkan_buffer(const compute_queue* cqueue floor_unused, const vulkan_queue* vk_queue floor_unused) const {
		return false;
	}
	
	//! returns the underlying Vulkan buffer that should be used on the device (i.e. this or a shared buffer)
	//! NOTE: when synchronization flags are set, this may synchronize buffer contents
	const vulkan_buffer* get_underlying_vulkan_buffer_safe() const;
	
	//! for debugging purposes: dumps the content of this buffer into a file using the specified "value_type" operator<<
	//! NOTE: each value will printed in one line (terminated by \n)
	template <typename value_type>
	bool dump_to_file(const compute_queue& cqueue, const string& file_name) {
		return compute_memory::dump_to_file<value_type>(*this, size, cqueue, file_name);
	}
	
	//! for debugging purposes: dumps the binary content of this buffer into a file
	bool dump_binary_to_file(const compute_queue& cqueue, const string& file_name) {
		return compute_memory::dump_binary_to_file(*this, size, cqueue, file_name);
	}
	
protected:
	//! constructs a buffer of the specified size, using the host pointer as specified by the flags
	compute_buffer(const compute_queue& cqueue,
				   const size_t& size_,
				   std::span<uint8_t> host_data_,
				   const COMPUTE_MEMORY_FLAG flags_ = (COMPUTE_MEMORY_FLAG::READ_WRITE |
													   COMPUTE_MEMORY_FLAG::HOST_READ_WRITE),
				   compute_buffer* shared_buffer_ = nullptr);
	
	//! constructs a buffer of the specified size, using the host pointer as specified by the flags
	compute_buffer(const compute_queue& cqueue,
				   std::span<uint8_t> host_data_,
				   const COMPUTE_MEMORY_FLAG flags_ = (COMPUTE_MEMORY_FLAG::READ_WRITE |
													   COMPUTE_MEMORY_FLAG::HOST_READ_WRITE),
				   compute_buffer* shared_buffer_ = nullptr) :
	compute_buffer(cqueue, host_data_.size_bytes(), host_data_, flags_, shared_buffer_) {}
	
	//! constructs an uninitialized buffer of the specified size
	compute_buffer(const compute_queue& cqueue,
				   const size_t& size_,
				   const COMPUTE_MEMORY_FLAG flags_ = (COMPUTE_MEMORY_FLAG::READ_WRITE |
													   COMPUTE_MEMORY_FLAG::HOST_READ_WRITE),
				   compute_buffer* shared_buffer_ = nullptr) :
	compute_buffer(cqueue, size_, {}, flags_, shared_buffer_) {}
	
	size_t size { 0u };
	
	// NOTE: only one of these can be active at a time
	union {
		compute_buffer* shared_buffer { nullptr };
		// shared Vulkan buffer object when Vulkan sharing is used
		vulkan_buffer* shared_vk_buffer;
		// shared Metal buffer object when Metal sharing is used
		metal_buffer* shared_mtl_buffer;
	};
	
	// buffer size/offset checking (used for debugging/development purposes)
	// NOTE: this can also be enabled by simply defining FLOOR_DEBUG_COMPUTE_BUFFER elsewhere
#if defined(FLOOR_DEBUG) || defined(FLOOR_DEBUG_COMPUTE_BUFFER)
	floor_inline_always static bool read_check(const size_t& buffer_size,
											   const size_t& read_size,
											   const size_t& offset,
											   const COMPUTE_MEMORY_FLAG& buffer_flags) {
		if(read_size == 0) {
			log_warn("read: trying to read 0 bytes!");
		}
		if(offset >= buffer_size) {
			log_error("read: invalid offset (>= size): offset: $X, size: $X", offset, buffer_size);
			return false;
		}
		if(offset + read_size > buffer_size) {
			log_error("read: invalid offset/read size (offset + read size > buffer size): offset: $X, read size: $X, size: $X",
					  offset, read_size, buffer_size);
			return false;
		}
		// should buffer be readable from the host?
		if(!has_flag<COMPUTE_MEMORY_FLAG::HOST_READ>(buffer_flags)) {
			log_error("read: buffer is not readable by the host (HOST_READ buffer flag not set)");
			return false;
		}
		return true;
	}
	
	floor_inline_always static bool write_check(const size_t& buffer_size,
												const size_t& write_size,
												const size_t& offset,
												const COMPUTE_MEMORY_FLAG& buffer_flags) {
		if(write_size == 0) {
			log_warn("write: trying to write 0 bytes!");
		}
		if(offset >= buffer_size) {
			log_error("write: invalid offset (>= size): offset: $X, size: $X", offset, buffer_size);
			return false;
		}
		if(offset + write_size > buffer_size) {
			log_error("write: invalid offset/write size (offset + write size > buffer size): offset: $X, write size: $X, size: $X",
					  offset, write_size, buffer_size);
			return false;
		}
		// should buffer be writable from the host?
		if(!has_flag<COMPUTE_MEMORY_FLAG::HOST_WRITE>(buffer_flags)) {
			log_error("write: buffer is not writable by the host (HOST_WRITE buffer flag not set)");
			return false;
		}
		return true;
	}
	
	floor_inline_always static bool copy_check(const size_t& buffer_size,
											   const size_t& src_size,
											   const size_t& copy_size,
											   const size_t& dst_offset,
											   const size_t& src_offset) {
		if(copy_size == 0) {
			log_warn("copy: trying to copy 0 bytes!");
		}
		if(src_offset >= src_size) {
			log_error("copy: invalid src offset (>= size): offset: $X, size: $X", src_offset, src_size);
			return false;
		}
		if(dst_offset >= buffer_size) {
			log_error("copy: invalid dst offset (>= size): offset: $X, size: $X", dst_offset, buffer_size);
			return false;
		}
		if(src_offset + copy_size > src_size) {
			log_error("copy: invalid src offset/copy size (offset + copy size > buffer size): offset: $X, copy size: $X, size: $X",
					  src_offset, copy_size, src_size);
			return false;
		}
		if(dst_offset + copy_size > buffer_size) {
			log_error("copy: invalid dst offset/copy size (offset + copy size > buffer size): offset: $X, copy size: $X, size: $X",
					  dst_offset, copy_size, buffer_size);
			return false;
		}
		return true;
	}
	
	floor_inline_always static bool fill_check(const size_t& buffer_size,
											   const size_t& fill_size,
											   const size_t& pattern_size,
											   const size_t& offset) {
		if(fill_size == 0) {
			log_error("fill: trying to fill 0 bytes!");
			return false;
		}
		if((offset % pattern_size) != 0) {
			log_error("fill: fill offset must be a multiple of pattern size: offset: $X, pattern size: $X", offset, pattern_size);
			return false;
		}
		if((fill_size % pattern_size) != 0) {
			log_error("fill: fill size must be a multiple of pattern size: fille size: $X, pattern size: $X", fill_size, pattern_size);
			return false;
		}
		if(offset >= buffer_size) {
			log_error("fill: invalid fill offset (>= size): offset: $X, size: $X", offset, buffer_size);
			return false;
		}
		if(offset + fill_size > buffer_size) {
			log_error("fill: invalid fill offset/fill size (offset + size > buffer size): offset: $X, fill size: $X, size: $X",
					  offset, fill_size, buffer_size);
			return false;
		}
		return true;
	}
	
	floor_inline_always static bool map_check(const size_t& buffer_size,
											  const size_t& map_size,
											  const COMPUTE_MEMORY_FLAG& buffer_flags,
											  const COMPUTE_MEMORY_MAP_FLAG& map_flags,
											  const size_t& offset) {
		if(has_flag<COMPUTE_MEMORY_MAP_FLAG::WRITE_INVALIDATE>(map_flags) &&
		   (map_flags & COMPUTE_MEMORY_MAP_FLAG::READ_WRITE) != COMPUTE_MEMORY_MAP_FLAG::NONE) {
			log_error("map: WRITE_INVALIDATE map flag is mutually exclusive with the READ and WRITE flags!");
			return false;
		}
		if(!has_flag<COMPUTE_MEMORY_MAP_FLAG::WRITE_INVALIDATE>(map_flags) &&
		   (map_flags & COMPUTE_MEMORY_MAP_FLAG::READ_WRITE) == COMPUTE_MEMORY_MAP_FLAG::NONE) {
			log_error("map: neither read nor write flags set for buffer mapping!");
			return false;
		}
		if(map_size == 0) {
			log_error("map: trying to map 0 bytes!");
			return false;
		}
		if(offset >= buffer_size) {
			log_error("map: invalid offset (>= size): offset: $X, size: $X", offset, buffer_size);
			return false;
		}
		if(offset + map_size > buffer_size) {
			log_error("map: invalid offset/map size (offset + map size > buffer size): offset: $X, map size: $X, size: $X",
					  offset, map_size, buffer_size);
			return false;
		}
		// should buffer be accessible at all?
		if((buffer_flags & COMPUTE_MEMORY_FLAG::HOST_READ_WRITE) == COMPUTE_MEMORY_FLAG::NONE) {
			log_error("map: buffer has been created with no host access flags, buffer can not be mapped to host memory!");
			return false;
		}
		// read/write mismatch check (only if either read or write set)
		if((buffer_flags & COMPUTE_MEMORY_FLAG::HOST_READ_WRITE) != COMPUTE_MEMORY_FLAG::HOST_READ_WRITE) {
			if(has_flag<COMPUTE_MEMORY_FLAG::HOST_READ>(buffer_flags) &&
			   (has_flag<COMPUTE_MEMORY_MAP_FLAG::WRITE>(map_flags) || has_flag<COMPUTE_MEMORY_MAP_FLAG::WRITE_INVALIDATE>(map_flags))) {
				   log_error("map: buffer has been created with the HOST_READ flag, but map flags specify buffer must be writable!");
				   return false;
			   }
			if(has_flag<COMPUTE_MEMORY_FLAG::HOST_WRITE>(buffer_flags) &&
			   has_flag<COMPUTE_MEMORY_MAP_FLAG::READ>(map_flags)) {
				log_error("map: buffer has been created with the HOST_WRITE flag, but map flags specify buffer must be readable!");
				return false;
			}
		}
		return true;
	}
#else
	floor_inline_always static constexpr bool read_check(const size_t&, const size_t&, const size_t&,
														 const COMPUTE_MEMORY_FLAG&) {
		return true;
	}
	floor_inline_always static constexpr bool write_check(const size_t&, const size_t&, const size_t&,
														  const COMPUTE_MEMORY_FLAG&) {
		return true;
	}
	floor_inline_always static constexpr bool copy_check(const size_t&, const size_t&, const size_t&,
														 const size_t&, const size_t&) {
		return true;
	}
	floor_inline_always static constexpr bool fill_check(const size_t&, const size_t&, const size_t&,
														 const size_t&) {
		return true;
	}
	floor_inline_always static bool map_check(const size_t&, const size_t&,
											  const COMPUTE_MEMORY_FLAG&, const COMPUTE_MEMORY_MAP_FLAG&,
											  const size_t&) {
		return true;
	}
#endif
	
};

FLOOR_POP_WARNINGS()
