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

#include <floor/compute/cuda/cuda_common.hpp>

#if !defined(FLOOR_NO_CUDA)

#include <floor/compute/compute_buffer.hpp>
#include <floor/core/aligned_ptr.hpp>

class vulkan_buffer;
class vulkan_semaphore;
class cuda_device;
class cuda_buffer final : public compute_buffer {
public:
	cuda_buffer(const compute_queue& cqueue,
				const size_t& size_,
				std::span<uint8_t> host_data_,
				const COMPUTE_MEMORY_FLAG flags_ = (COMPUTE_MEMORY_FLAG::READ_WRITE |
													COMPUTE_MEMORY_FLAG::HOST_READ_WRITE),
				compute_buffer* shared_buffer_ = nullptr);
	
	cuda_buffer(const compute_queue& cqueue,
				const size_t& size_,
				const COMPUTE_MEMORY_FLAG flags_ = (COMPUTE_MEMORY_FLAG::READ_WRITE |
													COMPUTE_MEMORY_FLAG::HOST_READ_WRITE),
				compute_buffer* shared_buffer_ = nullptr) :
	cuda_buffer(cqueue, size_, {}, flags_, shared_buffer_) {}
	
	~cuda_buffer() override;
	
	void read(const compute_queue& cqueue, const size_t size = 0, const size_t offset = 0) override;
	void read(const compute_queue& cqueue, void* dst, const size_t size = 0, const size_t offset = 0) override;
	
	void write(const compute_queue& cqueue, const size_t size = 0, const size_t offset = 0) override;
	void write(const compute_queue& cqueue, const void* src, const size_t size = 0, const size_t offset = 0) override;
	
	void copy(const compute_queue& cqueue, const compute_buffer& src,
			  const size_t size = 0, const size_t src_offset = 0, const size_t dst_offset = 0) override;
	
	bool fill(const compute_queue& cqueue,
			  const void* pattern, const size_t& pattern_size,
			  const size_t size = 0, const size_t offset = 0) override;
	
	bool zero(const compute_queue& cqueue) override;
	
	void* __attribute__((aligned(128))) map(const compute_queue& cqueue,
											const COMPUTE_MEMORY_MAP_FLAG flags = (COMPUTE_MEMORY_MAP_FLAG::READ_WRITE | COMPUTE_MEMORY_MAP_FLAG::BLOCK),
											const size_t size = 0, const size_t offset = 0) override;
	
	bool unmap(const compute_queue& cqueue, void* __attribute__((aligned(128))) mapped_ptr) override;
	
	bool acquire_vulkan_buffer(const compute_queue* cqueue, const vulkan_queue* vk_queue) const override;
	bool release_vulkan_buffer(const compute_queue* cqueue, const vulkan_queue* vk_queue) const override;
	bool sync_vulkan_buffer(const compute_queue* = nullptr, const vulkan_queue* = nullptr) const override {
		// nop, since it's backed by the same memory
		return true;
	}
	
	//! returns the cuda specific buffer pointer (device pointer)
	const cu_device_ptr& get_cuda_buffer() const {
		return buffer;
	}
	
protected:
	cu_device_ptr buffer { 0ull };
	cu_graphics_resource rsrc { nullptr };
	
	struct cuda_mapping {
		aligned_ptr<uint8_t> ptr;
		const size_t size;
		const size_t offset;
		const COMPUTE_MEMORY_MAP_FLAG flags;
	};
	// stores all mapped pointers and the mapped buffer
	unordered_map<void*, cuda_mapping> mappings;
	
	// separate create buffer function, b/c it's called by the constructor and resize
	bool create_internal(const bool copy_host_data, const compute_queue& cqueue);

#if !defined(FLOOR_NO_VULKAN)
	// external (Vulkan) memory
	cu_external_memory ext_memory { nullptr };
	// internal Vulkan buffer when using Vulkan memory sharing (and not wrapping an existing buffer)
	shared_ptr<compute_buffer> cuda_vk_buffer;
	// external (Vulkan) semaphore
	cu_external_semaphore ext_sema { nullptr };
	// internal Vulkan semaphore when using Vulkan memory sharing, used to sync buffer access
	unique_ptr<vulkan_semaphore> cuda_vk_sema;
	// creates the internal Vulkan buffer, or deals with the wrapped external one
	bool create_shared_vulkan_buffer(const bool copy_host_data);
#endif
	
};

#endif
