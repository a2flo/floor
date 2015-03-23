/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2015 Florian Ziesche
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

#ifndef __FLOOR_COMPUTE_BUFFER_HPP__
#define __FLOOR_COMPUTE_BUFFER_HPP__

#include <vector>
#include <array>
#include <floor/math/vector_lib.hpp>
#include <floor/core/util.hpp>
#include <floor/threading/thread_safety.hpp>

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wweak-vtables"
#endif

class compute_queue;

//! buffer flags
enum class COMPUTE_BUFFER_FLAG : uint32_t {
	//! invalid/uninitialized flag
	NONE				= (0u),
	
	//! read only buffer (kernel point of view)
	READ				= (1u << 0u),
	//! write only buffer (kernel point of view)
	WRITE				= (1u << 1u),
	//! read and write buffer (kernel point of view)
	READ_WRITE			= (READ | WRITE),
	
	//! read only buffer (host point of view)
	HOST_READ			= (1u << 2u),
	//! write only buffer (host point of view)
	HOST_WRITE			= (1u << 3u),
	//! read and write buffer (host point of view)
	HOST_READ_WRITE		= (HOST_READ | HOST_WRITE),
	//! if neither HOST_READ or HOST_WRITE is set, the host will not have access to the buffer
	//! -> can use this mask to AND with flags
	HOST_NO_ACCESS_MASK = ~(HOST_READ_WRITE),
	
	//! the buffer will use/store the specified host pointer,
	//! but won't initialize the compute buffer with that data
	NO_INITIAL_COPY		= (1u << 4u),
	
	//! the specified (host pointer) data will be copied back to the
	//! compute buffer each time it is used by a kernel
	//! -> copy before kernel execution
	//! NOTE: the user must make sure that this is thread-safe!
	COPY_ON_USE			= (1u << 5u),
	
	//! every time a kernel using this buffer has finished execution,
	//! the buffer data will be copied back to the specified host pointer
	//! -> copy after kernel execution
	//! NOTE: the user must make sure that this is thread-safe!
	READ_BACK_RESULT	= (1u << 6u),
	
	//! buffer memory is allocated in host memory, i.e. the specified host pointer
	//! will be used for all buffer operations
	USE_HOST_MEMORY		= (1u << 7u),
	
};
enum_class_bitwise_and_global(COMPUTE_BUFFER_FLAG)
enum_class_bitwise_or_global(COMPUTE_BUFFER_FLAG)

//! buffer mapping flags
enum class COMPUTE_BUFFER_MAP_FLAG : uint32_t {
	NONE				= (0u),
	READ				= (1u << 0u),
	WRITE				= (1u << 1u),
	WRITE_INVALIDATE	= (1u << 2u),
	READ_WRITE			= (READ | WRITE),
	BLOCK				= (1u << 3u),
};
enum_class_bitwise_and_global(COMPUTE_BUFFER_MAP_FLAG)
enum_class_bitwise_or_global(COMPUTE_BUFFER_MAP_FLAG)

class compute_buffer {
public:
	// TODO: flag handling
	// TODO: buffer migration: copy / move
	// TODO: opengl handling / OPENGL_BUFFER
	
	//! constructs a buffer of the specified size, using the host pointer as specified by the flags
	compute_buffer(const void* device,
				   const size_t& size_,
				   void* host_ptr,
				   const COMPUTE_BUFFER_FLAG flags_ = (COMPUTE_BUFFER_FLAG::READ_WRITE |
													   COMPUTE_BUFFER_FLAG::HOST_READ_WRITE));
	
	//! constructs an uninitialized buffer of the specified size
	compute_buffer(const void* device,
				   const size_t& size_,
				   const COMPUTE_BUFFER_FLAG flags_ = (COMPUTE_BUFFER_FLAG::READ_WRITE |
													   COMPUTE_BUFFER_FLAG::HOST_READ_WRITE)) :
	compute_buffer(device, size_, nullptr, flags_) {}
	
	//! constructs a buffer of the specified data (under consideration of the specified flags)
	template <typename data_type>
	compute_buffer(const void* device,
				   const vector<data_type>& data,
				   const COMPUTE_BUFFER_FLAG flags_ = (COMPUTE_BUFFER_FLAG::READ_WRITE |
													   COMPUTE_BUFFER_FLAG::HOST_READ_WRITE)) :
	compute_buffer(device, sizeof(data_type) * data.size(), (void*)&data[0], flags_) {}
	
	//! constructs a buffer of the specified data (under consideration of the specified flags)
	template <typename data_type, size_t n>
	compute_buffer(const void* device,
				   const array<data_type, n>& data,
				   const COMPUTE_BUFFER_FLAG flags_ = (COMPUTE_BUFFER_FLAG::READ_WRITE |
													   COMPUTE_BUFFER_FLAG::HOST_READ_WRITE)) :
	compute_buffer(device, sizeof(data_type) * n, (void*)&data[0], flags_) {}
	
	virtual ~compute_buffer() = 0;
	
	//! buffer size must always be a multiple of this
	static constexpr size_t min_multiple() { return 4u; }
	
	//! aligns the specified size to the minimal multiple buffer size (always upwards!)
	static constexpr size_t align_size(const size_t& size_) {
		return ((size_ % min_multiple()) == 0u ? size_ : (((size_ / min_multiple()) + 1u) * min_multiple()));
	}
	
	//! reads "size" bytes (or the complete buffer if 0) from "offset" onwards
	//! back to the previously specified host pointer
	virtual void read(shared_ptr<compute_queue> cqueue, const size_t size = 0, const size_t offset = 0) = 0;
	//! reads "size" bytes (or the complete buffer if 0) from "offset" onwards
	//! back to the specified "dst" pointer
	virtual void read(shared_ptr<compute_queue> cqueue, void* dst, const size_t size = 0, const size_t offset = 0) = 0;
	
	//! writes "size" bytes (or the complete buffer if 0) from "offset" onwards
	//! from the previously specified host pointer to this buffer
	virtual void write(shared_ptr<compute_queue> cqueue, const size_t size = 0, const size_t offset = 0) = 0;
	//! writes "size" bytes (or the complete buffer if 0) from "offset" onwards
	//! from the specified "src" pointer to this buffer
	virtual void write(shared_ptr<compute_queue> cqueue, const void* src, const size_t size = 0, const size_t offset = 0) = 0;
	
	//! copies data from the specified "src" buffer to this buffer, of the specified "size" (complete buffer if size == 0),
	//! from "src_offset" in the "src" buffer to "dst_offset" in this buffer
	virtual void copy(shared_ptr<compute_queue> cqueue,
					  shared_ptr<compute_buffer> src,
					  const size_t size = 0, const size_t src_offset = 0, const size_t dst_offset = 0) = 0;
	
	//! fills this buffer with the provided "pattern" of size "pattern_size" (in bytes)
	//! NOTE: filling the buffer with patterns that are 1 byte, 2 bytes or 4 bytes in size might be faster than other sizes
	virtual void fill(shared_ptr<compute_queue> cqueue,
					  const void* pattern, const size_t& pattern_size,
					  const size_t size = 0, const size_t offset = 0) = 0;
	
	//! zeros/clears the complete buffer
	virtual void zero(shared_ptr<compute_queue> cqueue) = 0;
	//! zeros/clears the complete buffer
	void clear(shared_ptr<compute_queue> cqueue) { zero(cqueue); }
	
	//! resizes (recreates) the buffer to "size" and either copies the old data from the old buffer if specified,
	//! or copies the data (again) from the previously specified host pointer or the one provided to this call,
	//! and will also update the host memory pointer (if used!) to "new_host_ptr" if set to non-nullptr
	virtual bool resize(shared_ptr<compute_queue> cqueue,
						const size_t& size,
						const bool copy_old_data = false,
						const bool copy_host_data = false,
						void* new_host_ptr = nullptr) = 0;
	
	//! maps device memory into host accessible memory, of the specified "size" (0 = complete buffer) and buffer "offset"
	//! NOTE: this might require a complete buffer copy on map and/or unmap (use READ, WRITE and WRITE_INVALIDATE appropriately)
	//! NOTE: this call might block regardless of if the BLOCK flag is set or not
	virtual void* __attribute__((aligned(128))) map(shared_ptr<compute_queue> cqueue,
													const COMPUTE_BUFFER_MAP_FLAG flags =
													(COMPUTE_BUFFER_MAP_FLAG::READ_WRITE |
													 COMPUTE_BUFFER_MAP_FLAG::BLOCK),
													const size_t size = 0, const size_t offset = 0) = 0;
	
	//! maps device memory into host accessible memory, of the specified "size" (0 = complete buffer) and buffer "offset",
	//! returning the mapped pointer as a vector<> of "data_type"
	//! NOTE: this might require a complete buffer copy on map and/or unmap (use READ, WRITE and WRITE_INVALIDATE appropriately)
	//! NOTE: this call might block regardless of if the BLOCK flag is set or not
	template <typename data_type>
	vector<data_type>* map(shared_ptr<compute_queue> cqueue,
						   const COMPUTE_BUFFER_MAP_FLAG flags_ =
						   (COMPUTE_BUFFER_MAP_FLAG::READ_WRITE |
							COMPUTE_BUFFER_MAP_FLAG::BLOCK),
						   const size_t size_ = 0, const size_t offset = 0) {
		return (vector<data_type>*)map(cqueue, flags_, size_, offset);
	}
	
	//! maps device memory into host accessible memory, of the specified "size" (0 = complete buffer) and buffer "offset",
	//! returning the mapped pointer as an array<> of "data_type" with "n" elements
	//! NOTE: this might require a complete buffer copy on map and/or unmap (use READ, WRITE and WRITE_INVALIDATE appropriately)
	//! NOTE: this call might block regardless of if the BLOCK flag is set or not
	template <typename data_type, size_t n>
	array<data_type, n>* map(shared_ptr<compute_queue> cqueue,
							 const COMPUTE_BUFFER_MAP_FLAG flags_ =
							 (COMPUTE_BUFFER_MAP_FLAG::READ_WRITE |
							  COMPUTE_BUFFER_MAP_FLAG::BLOCK),
							 const size_t size_ = 0, const size_t offset = 0) {
		return (array<data_type, n>*)map(cqueue, flags_, size_, offset);
	}
	
	//! unmaps a previously mapped memory pointer
	//! NOTE: this might require a complete buffer copy on map and/or unmap (use READ, WRITE and WRITE_INVALIDATE appropriately)
	//! NOTE: this call might block regardless of if the BLOCK flag is set or not
	virtual void unmap(shared_ptr<compute_queue> cqueue, void* __attribute__((aligned(128))) mapped_ptr) = 0;
	
	//! returns the size of this buffer (in bytes)
	const size_t& get_size() const { return size; }
	
	//! returns the associated host memory pointer
	void* get_host_ptr() const { return host_ptr; }
	
	//! returns the flags that were used to create this buffer
	const COMPUTE_BUFFER_FLAG& get_flags() const { return flags; }
	
	//! returns the associated device
	const void* get_device() const { return dev; }
	
	//! NOTE: for debugging/development purposes only
	void _lock() ACQUIRE(lock) REQUIRES(!lock);
	void _unlock() RELEASE(lock);
	
protected:
	const void* dev { nullptr };
	size_t size { 0u };
	void* host_ptr { nullptr };
	const COMPUTE_BUFFER_FLAG flags { COMPUTE_BUFFER_FLAG::NONE };
	
	safe_mutex lock;
	
	// buffer size/offset checking (used for debugging/development purposes)
	// NOTE: this can also be enabled by simply defining FLOOR_DEBUG_COMPUTE_BUFFER elsewhere
#if defined(FLOOR_DEBUG) || defined(FLOOR_DEBUG_COMPUTE_BUFFER)
	floor_inline_always static bool read_check(const size_t& buffer_size,
											   const size_t& read_size,
											   const size_t& offset) {
		if(read_size == 0) {
			log_warn("read: trying to read 0 bytes!");
		}
		if(offset >= buffer_size) {
			log_error("read: invalid offset (>= size): offset: %X, size: %X", offset, buffer_size);
			return false;
		}
		if(offset + read_size > buffer_size) {
			log_error("read: invalid offset/read size (offset + read size > buffer size): offset: %X, read size: %X, size: %X",
					  offset, read_size, buffer_size);
			return false;
		}
		return true;
	}
	
	floor_inline_always static bool write_check(const size_t& buffer_size,
												const size_t& write_size,
												const size_t& offset) {
		if(write_size == 0) {
			log_warn("write: trying to write 0 bytes!");
		}
		if(offset >= buffer_size) {
			log_error("write: invalid offset (>= size): offset: %X, size: %X", offset, buffer_size);
			return false;
		}
		if(offset + write_size > buffer_size) {
			log_error("write: invalid offset/write size (offset + write size > buffer size): offset: %X, write size: %X, size: %X",
					  offset, write_size, buffer_size);
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
			log_error("copy: invalid src offset (>= size): offset: %X, size: %X", src_offset, src_size);
			return false;
		}
		if(dst_offset >= buffer_size) {
			log_error("copy: invalid dst offset (>= size): offset: %X, size: %X", dst_offset, buffer_size);
			return false;
		}
		if(src_offset + copy_size > src_size) {
			log_error("copy: invalid src offset/copy size (offset + copy size > buffer size): offset: %X, copy size: %X, size: %X",
					  src_offset, copy_size, src_size);
			return false;
		}
		if(dst_offset + copy_size > buffer_size) {
			log_error("copy: invalid dst offset/copy size (offset + copy size > buffer size): offset: %X, copy size: %X, size: %X",
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
			log_error("fill: fill offset must be a multiple of pattern size: offset: %X, pattern size: %X", offset, pattern_size);
			return false;
		}
		if((fill_size % pattern_size) != 0) {
			log_error("fill: fill size must be a multiple of pattern size: fille size: %X, pattern size: %X", fill_size, pattern_size);
			return false;
		}
		if(offset >= buffer_size) {
			log_error("fill: invalid fill offset (>= size): offset: %X, size: %X", offset, buffer_size);
			return false;
		}
		if(offset + fill_size > buffer_size) {
			log_error("fill: invalid fill offset/fill size (offset + size > buffer size): offset: %X, fill size: %X, size: %X",
					  offset, fill_size, buffer_size);
			return false;
		}
		return true;
	}
	
	floor_inline_always static bool map_check(const size_t& buffer_size,
											  const size_t& map_size,
											  const COMPUTE_BUFFER_FLAG& buffer_flags,
											  const COMPUTE_BUFFER_MAP_FLAG& map_flags,
											  const size_t& offset) {
		if((map_flags & COMPUTE_BUFFER_MAP_FLAG::WRITE_INVALIDATE) != COMPUTE_BUFFER_MAP_FLAG::NONE &&
		   (map_flags & COMPUTE_BUFFER_MAP_FLAG::READ_WRITE) != COMPUTE_BUFFER_MAP_FLAG::NONE) {
			log_error("map: WRITE_INVALIDATE map flag is mutually exclusive with the READ and WRITE flags!");
			return false;
		}
		if((map_flags & COMPUTE_BUFFER_MAP_FLAG::READ_WRITE) == COMPUTE_BUFFER_MAP_FLAG::NONE) {
			log_error("map: neither read nor write flags set for buffer mapping!");
			return false;
		}
		if(map_size == 0) {
			log_error("map: trying to map 0 bytes!");
			return false;
		}
		if(offset >= buffer_size) {
			log_error("map: invalid offset (>= size): offset: %X, size: %X", offset, buffer_size);
			return false;
		}
		if(offset + map_size > buffer_size) {
			log_error("map: invalid offset/map size (offset + map size > buffer size): offset: %X, map size: %X, size: %X",
					  offset, map_size, buffer_size);
			return false;
		}
		// should buffer be accessible at all?
		if((buffer_flags & COMPUTE_BUFFER_FLAG::HOST_READ_WRITE) == COMPUTE_BUFFER_FLAG::NONE) {
			log_error("map: buffer has been created with no host access flags, buffer can not be mapped to host memory!");
			return false;
		}
		// read/write mismatch check (only if either read or write set)
		if((buffer_flags & COMPUTE_BUFFER_FLAG::HOST_READ_WRITE) != COMPUTE_BUFFER_FLAG::HOST_READ_WRITE) {
			if((buffer_flags & COMPUTE_BUFFER_FLAG::HOST_READ) != COMPUTE_BUFFER_FLAG::NONE &&
			   ((map_flags & COMPUTE_BUFFER_MAP_FLAG::WRITE) != COMPUTE_BUFFER_MAP_FLAG::NONE ||
				(map_flags & COMPUTE_BUFFER_MAP_FLAG::WRITE_INVALIDATE) != COMPUTE_BUFFER_MAP_FLAG::NONE)) {
				   log_error("map: buffer has been created with the HOST_READ flag, but map flags specify buffer must be writable!");
				   return false;
			   }
			if((buffer_flags & COMPUTE_BUFFER_FLAG::HOST_WRITE) != COMPUTE_BUFFER_FLAG::NONE &&
			   (map_flags & COMPUTE_BUFFER_MAP_FLAG::READ) != COMPUTE_BUFFER_MAP_FLAG::NONE) {
				log_error("map: buffer has been created with the HOST_WRITE flag, but map flags specify buffer must be readable!");
				return false;
			}
		}
		return true;
	}
#else
	floor_inline_always static constexpr bool read_check(const size_t&, const size_t&, const size_t&) {
		return true;
	}
	floor_inline_always static constexpr bool write_check(const size_t&, const size_t&, const size_t&) {
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
											  const COMPUTE_BUFFER_FLAG&, const COMPUTE_BUFFER_MAP_FLAG&,
											  const size_t&) {
		return true;
	}
#endif
	
};

#if defined(__clang__)
#pragma clang diagnostic pop
#endif

#endif
