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

#ifndef __FLOOR_COMPUTE_MEMORY_HPP__
#define __FLOOR_COMPUTE_MEMORY_HPP__

#include <floor/math/vector_lib.hpp>
#include <floor/core/util.hpp>
#include <floor/core/logger.hpp>
#include <floor/threading/thread_safety.hpp>
#include <floor/core/gl_support.hpp>

class compute_queue;

//! memory flags
enum class COMPUTE_MEMORY_FLAG : uint32_t {
	//! invalid/uninitialized flag
	NONE				= (0u),
	
	//! read only memory (kernel point of view)
	READ				= (1u << 0u),
	//! write only memory (kernel point of view)
	WRITE				= (1u << 1u),
	//! read and write memory (kernel point of view)
	READ_WRITE			= (READ | WRITE),
	
	//! read only memory (host point of view)
	HOST_READ			= (1u << 2u),
	//! write only memory (host point of view)
	HOST_WRITE			= (1u << 3u),
	//! read and write memory (host point of view)
	HOST_READ_WRITE		= (HOST_READ | HOST_WRITE),
	//! if neither HOST_READ or HOST_WRITE is set, the host will not have access to the memory
	//! -> can use this mask to AND with flags
	__HOST_NO_ACCESS_MASK = ~(HOST_READ_WRITE),
	
	//! the memory will use/store the specified host pointer,
	//! but won't initialize the compute memory with that data
	NO_INITIAL_COPY		= (1u << 4u),
	
	//! the specified (host pointer) data will be copied back to the
	//! compute memory each time it is used by a kernel
	//! -> copy before kernel execution
	//! NOTE: the user must make sure that this is thread-safe!
	COPY_ON_USE			= (1u << 5u),
	
	//! every time a kernel using this memory has finished execution,
	//! the memory data will be copied back to the specified host pointer
	//! -> copy after kernel execution
	//! NOTE: the user must make sure that this is thread-safe!
	READ_BACK_RESULT	= (1u << 6u),
	
	//! memory is allocated in host memory, i.e. the specified host pointer
	//! will be used for all memory operations
	USE_HOST_MEMORY		= (1u << 7u),
	
	//! creates the memory with opengl sharing enabled
	//! NOTE: the opengl object can be retrieved via get_opengl_object()
	//! NOTE: OPENGL_SHARING and USE_HOST_MEMORY are mutually exclusive (for obvious reasons)
	//! NOTE: if no OPENGL_READ or OPENGL_WRITE is specified, OPENGL_READ_WRITE is assumed
	OPENGL_SHARING		= (1u << 8u),
	//! the compute implementation has only read access to the opengl object
	OPENGL_READ			= (1u << 9u),
	//! the compute implementation has only write (discard) access to the opengl object
	OPENGL_WRITE		= (1u << 10u),
	//! the compute implementation has read and write access to the opengl object (default)
	OPENGL_READ_WRITE	= (OPENGL_READ | OPENGL_WRITE),
	
};
floor_global_enum_ext(COMPUTE_MEMORY_FLAG)

//! memory mapping flags
enum class COMPUTE_MEMORY_MAP_FLAG : uint32_t {
	NONE				= (0u),
	READ				= (1u << 0u),
	WRITE				= (1u << 1u),
	WRITE_INVALIDATE	= (1u << 2u),
	READ_WRITE			= (READ | WRITE),
	BLOCK				= (1u << 3u),
};
floor_global_enum_ext(COMPUTE_MEMORY_MAP_FLAG)

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wweak-vtables"
#endif

class compute_memory {
public:
	// TODO: flag handling
	// TODO: memory migration: copy / move
	
	//! constructs an incomplete memory object
	compute_memory(const void* device,
				   void* host_ptr,
				   const COMPUTE_MEMORY_FLAG flags_ = (COMPUTE_MEMORY_FLAG::READ_WRITE |
													   COMPUTE_MEMORY_FLAG::HOST_READ_WRITE),
				   const uint32_t opengl_type_ = 0,
				   const uint32_t external_gl_object_ = 0);
	
	//! constructs an incomplete memory object
	compute_memory(const void* device,
				   const COMPUTE_MEMORY_FLAG flags_ = (COMPUTE_MEMORY_FLAG::READ_WRITE |
													   COMPUTE_MEMORY_FLAG::HOST_READ_WRITE),
				   const uint32_t opengl_type_ = 0,
				   const uint32_t external_gl_object_ = 0) :
	compute_memory(device, nullptr, flags_, opengl_type_, external_gl_object_) {}
	
	virtual ~compute_memory() = 0;
	
	//! memory size must always be a multiple of this
	static constexpr size_t min_multiple() { return 4u; }
	
	//! aligns the specified size to the minimal multiple memory size (always upwards!)
	static constexpr size_t align_size(const size_t& size_) {
		return ((size_ % min_multiple()) == 0u ? size_ : (((size_ / min_multiple()) + 1u) * min_multiple()));
	}
	
	//! returns the associated host memory pointer
	void* get_host_ptr() const { return host_ptr; }
	
	//! returns the flags that were used to create this memory object
	const COMPUTE_MEMORY_FLAG& get_flags() const { return flags; }
	
	//! returns the associated device
	const void* get_device() const { return dev; }
	
	//! returns the associated opengl buffer/image object (if this memory object was created with OPENGL_SHARING)
	const uint32_t& get_opengl_object() const {
		return gl_object;
	}
	
	//! acquires the associated opengl object for use with opengl (-> release from compute use)
	virtual bool acquire_opengl_object(shared_ptr<compute_queue> cqueue) = 0;
	//! releases the associated opengl object from use with opengl (-> acquire for compute use)
	virtual bool release_opengl_object(shared_ptr<compute_queue> cqueue) = 0;
	
	//! NOTE: for debugging/development purposes only
	void _lock() ACQUIRE(lock) REQUIRES(!lock);
	void _unlock() RELEASE(lock);
	
protected:
	const void* dev { nullptr };
	void* host_ptr { nullptr };
	const COMPUTE_MEMORY_FLAG flags { COMPUTE_MEMORY_FLAG::NONE };
	
	const bool has_external_gl_object { false };
	const uint32_t opengl_type { 0u };
	uint32_t gl_object { 0u };
	bool gl_object_state { true }; // false: compute use, true: opengl use
	
	safe_mutex lock;
	
};

#if defined(__clang__)
#pragma clang diagnostic pop
#endif

#endif
