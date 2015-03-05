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
	// TODO: sub-buffer support
	
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
	
	//!
	virtual void copy(shared_ptr<compute_queue> cqueue,
					  shared_ptr<compute_buffer> src,
					  const size_t size = 0, const size_t src_offset = 0, const size_t dst_offset = 0) = 0;
	
	//!
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
	
	//!
	virtual void* __attribute__((aligned(128))) map(shared_ptr<compute_queue> cqueue,
													const COMPUTE_BUFFER_MAP_FLAG flags =
													(COMPUTE_BUFFER_MAP_FLAG::READ_WRITE |
													 COMPUTE_BUFFER_MAP_FLAG::BLOCK),
													const size_t size = 0, const size_t offset = 0) = 0;
	
	//!
	template <typename data_type>
	vector<data_type>* map(shared_ptr<compute_queue> cqueue,
						   const COMPUTE_BUFFER_MAP_FLAG flags_ =
						   (COMPUTE_BUFFER_MAP_FLAG::READ_WRITE |
							COMPUTE_BUFFER_MAP_FLAG::BLOCK),
						   const size_t size_ = 0, const size_t offset = 0) {
		return (vector<data_type>*)map(cqueue, flags_, size_, offset);
	}
	
	//!
	template <typename data_type, size_t n>
	array<data_type, n>* map(shared_ptr<compute_queue> cqueue,
							 const COMPUTE_BUFFER_MAP_FLAG flags_ =
							 (COMPUTE_BUFFER_MAP_FLAG::READ_WRITE |
							  COMPUTE_BUFFER_MAP_FLAG::BLOCK),
							 const size_t size_ = 0, const size_t offset = 0) {
		return (array<data_type, n>*)map(cqueue, flags_, size_, offset);
	}
	
	//!
	virtual void unmap(shared_ptr<compute_queue> cqueue, void* __attribute__((aligned(128))) mapped_ptr) = 0;
	
	//!
	const size_t& get_size() const { return size; }
	
	//!
	void* get_host_ptr() const { return host_ptr; }
	
	//!
	const COMPUTE_BUFFER_FLAG& get_flags() const { return flags; }
	
	//!
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
	
};

#if defined(__clang__)
#pragma clang diagnostic pop
#endif

#endif
