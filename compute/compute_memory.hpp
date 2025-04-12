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

#include <floor/math/vector_lib.hpp>
#include <floor/core/util.hpp>
#include <floor/core/logger.hpp>
#include <floor/threading/thread_safety.hpp>
#include <floor/compute/compute_memory_flags.hpp>

class compute_queue;
class compute_device;

FLOOR_PUSH_WARNINGS()
FLOOR_IGNORE_WARNING(weak-vtables)

class compute_memory {
public:
	// TODO: flag handling
	// TODO: memory migration: copy / move
	
	virtual ~compute_memory();
	
	//! memory size must always be a multiple of this
	static constexpr size_t min_multiple() { return 4u; }
	
	//! aligns the specified size to the minimal multiple memory size (always upwards!)
	static constexpr size_t align_size(const size_t& size_) {
		return ((size_ % min_multiple()) == 0u ? size_ : (((size_ / min_multiple()) + 1u) * min_multiple()));
	}
	
	//! returns the associated host memory span/range
	std::span<uint8_t> get_host_data() const {
		return host_data;
	}
	
	//! returns the flags that were used to create this memory object
	const COMPUTE_MEMORY_FLAG& get_flags() const { return flags; }
	
	//! returns the associated device
	const compute_device& get_device() const { return dev; }
	
	//! returns true if the shared Metal buffer/image is currently acquired for use with compute
	bool is_shared_metal_object_acquired() const {
		return mtl_object_state;
	}
	
	//! returns true if the shared Vulkan buffer/image is currently acquired for use with compute
	bool is_shared_vulkan_object_acquired() const {
		return vk_object_state;
	}
	
	//! zeros/clears the complete memory object, returns true on success
	virtual bool zero(const compute_queue& cqueue) = 0;
	//! zeros/clears the complete memory object, returns true on success
	bool clear(const compute_queue& cqueue) { return zero(cqueue); }
	
	//! sets the debug label for this memory object (e.g. for display in a debugger)
	virtual void set_debug_label(const string& label);
	//! returns the current debug label
	virtual const string& get_debug_label() const;
	
	//! NOTE: for debugging/development purposes only
	void _lock() const ACQUIRE(lock) REQUIRES(!lock);
	void _unlock() const RELEASE(lock);
	
	//! returns true if this buffer has been allocated from the internal heap
	virtual bool is_heap_allocated() const {
		return false;
	}
	
	//! for debugging purposes: dumps the content of the specified memory object into a file using the specified "value_type" operator<<
	//! NOTE: each value will printed in one line (terminated by \n)
	template <typename value_type, typename memory_object_type>
	static bool dump_to_file(memory_object_type& mem, const size_t size, const compute_queue& cqueue, const string& file_name) {
		ofstream dump_file(file_name, ios::out);
		if (!dump_file.is_open()) {
			return false;
		}
		
		auto dump_mem = &mem;
		
		// clone if we can't read this buffer directly
		shared_ptr<memory_object_type> tmp_mem;
		if (!has_flag<COMPUTE_MEMORY_FLAG::HOST_READ>(mem.get_flags())) {
			tmp_mem = mem.clone(cqueue, true, mem.get_flags() | COMPUTE_MEMORY_FLAG::HOST_READ);
			dump_mem = tmp_mem.get();
		}
		
		auto mapped_ptr = dump_mem->map(cqueue, COMPUTE_MEMORY_MAP_FLAG::READ | COMPUTE_MEMORY_MAP_FLAG::BLOCK);
		if (mapped_ptr == nullptr) {
			return false;
		}
		
		auto typed_ptr = (const value_type*)mapped_ptr;
		const auto value_count = size / sizeof(value_type);
		for (size_t value_idx = 0; value_idx < value_count; ++value_idx) {
			dump_file << *typed_ptr++ << '\n';
		}
		
		dump_mem->unmap(cqueue, mapped_ptr);
		
		return true;
	}
	
	//! for debugging purposes: dumps the binary content of the specified memory object into a file
	template <typename memory_object_type>
	static bool dump_binary_to_file(memory_object_type& mem, const size_t size, const compute_queue& cqueue, const string& file_name) {
		ofstream dump_file(file_name, ios::out | ios::binary);
		if (!dump_file.is_open()) {
			return false;
		}
		
		auto dump_mem = &mem;
		
		// clone if we can't read this buffer directly
		shared_ptr<memory_object_type> tmp_mem;
		if (!has_flag<COMPUTE_MEMORY_FLAG::HOST_READ>(mem.get_flags())) {
			tmp_mem = mem.clone(cqueue, true, mem.get_flags() | COMPUTE_MEMORY_FLAG::HOST_READ);
			dump_mem = tmp_mem.get();
		}
		
		auto mapped_ptr = dump_mem->map(cqueue, COMPUTE_MEMORY_MAP_FLAG::READ | COMPUTE_MEMORY_MAP_FLAG::BLOCK);
		if (mapped_ptr == nullptr) {
			return false;
		}
		
		dump_file.write((char*)mapped_ptr, (std::streamsize)size);
		
		dump_mem->unmap(cqueue, mapped_ptr);
		
		return true;
	}
	
	//! returns the default compute_queue of the device backing the specified memory object
	static const compute_queue* get_default_queue_for_memory(const compute_memory& mem);
	
protected:
	//! constructs an incomplete memory object
	compute_memory(const compute_queue& cqueue,
				   std::span<uint8_t> host_data_,
				   const COMPUTE_MEMORY_FLAG flags_ = (COMPUTE_MEMORY_FLAG::READ_WRITE |
													   COMPUTE_MEMORY_FLAG::HOST_READ_WRITE));
	
	//! constructs an incomplete memory object
	compute_memory(const compute_queue& cqueue,
				   const COMPUTE_MEMORY_FLAG flags_ = (COMPUTE_MEMORY_FLAG::READ_WRITE |
													   COMPUTE_MEMORY_FLAG::HOST_READ_WRITE)) :
	compute_memory(cqueue, {}, flags_) {}
	
	const compute_device& dev;
	std::span<uint8_t> host_data {};
	const COMPUTE_MEMORY_FLAG flags { COMPUTE_MEMORY_FLAG::NONE };
	
	//! false: compute use, true: Metal use
	mutable bool mtl_object_state { true };
	
	//! false: compute use, true: Vulkan use
	mutable bool vk_object_state { true };
	
	mutable safe_recursive_mutex lock;
	
	string debug_label;
	
	//! computes the shared memory (buffer/image) flags that should be used when creating shared Vulkan/Metal memory for Host-Compute
	static COMPUTE_MEMORY_FLAG make_host_shared_memory_flags(const COMPUTE_MEMORY_FLAG& flags,
															 const compute_device& shared_dev,
															 const bool copy_host_data);
	
};

FLOOR_POP_WARNINGS()
