/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2018 Florian Ziesche
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

#ifndef __FLOOR_VULKAN_BUFFER_HPP__
#define __FLOOR_VULKAN_BUFFER_HPP__

#include <floor/compute/vulkan/vulkan_common.hpp>

#if !defined(FLOOR_NO_VULKAN)

#include <floor/compute/compute_buffer.hpp>
#include <floor/compute/vulkan/vulkan_memory.hpp>

class vulkan_device;
class vulkan_buffer final : public compute_buffer, vulkan_memory {
public:
	vulkan_buffer(vulkan_device* device,
				  const size_t& size_,
				  void* host_ptr,
				  const COMPUTE_MEMORY_FLAG flags_ = (COMPUTE_MEMORY_FLAG::READ_WRITE |
													  COMPUTE_MEMORY_FLAG::HOST_READ_WRITE),
				  const uint32_t opengl_type_ = 0,
				  const uint32_t external_gl_object_ = 0);
	
FLOOR_PUSH_WARNINGS()
FLOOR_IGNORE_WARNING(cast-qual) // ignored for safe "const void*" -> "void*" cast here
	vulkan_buffer(vulkan_device* device,
				  const size_t& size_,
				  const void* host_ptr_,
				  const COMPUTE_MEMORY_FLAG flags_ = (COMPUTE_MEMORY_FLAG::READ_WRITE |
													  COMPUTE_MEMORY_FLAG::HOST_READ_WRITE),
				  const uint32_t opengl_type_ = 0,
				  const uint32_t external_gl_object_ = 0) :
	vulkan_buffer(device, size_, (void*)host_ptr_, flags_, opengl_type_, external_gl_object_) {}
FLOOR_POP_WARNINGS()
	
	vulkan_buffer(vulkan_device* device,
				  const size_t& size_,
				  const COMPUTE_MEMORY_FLAG flags_ = (COMPUTE_MEMORY_FLAG::READ_WRITE |
													  COMPUTE_MEMORY_FLAG::HOST_READ_WRITE),
				  const uint32_t opengl_type_ = 0) :
	vulkan_buffer(device, size_, (void*)nullptr, flags_, opengl_type_) {}
	
	template <typename data_type>
	vulkan_buffer(vulkan_device* device,
				  const vector<data_type>& data,
				  const COMPUTE_MEMORY_FLAG flags_ = (COMPUTE_MEMORY_FLAG::READ_WRITE |
													  COMPUTE_MEMORY_FLAG::HOST_READ_WRITE),
				  const uint32_t opengl_type_ = 0) :
	vulkan_buffer(device, sizeof(data_type) * data.size(), (const void*)&data[0], flags_, opengl_type_) {}
	
	template <typename data_type, size_t n>
	vulkan_buffer(vulkan_device* device,
				  const array<data_type, n>& data,
				  const COMPUTE_MEMORY_FLAG flags_ = (COMPUTE_MEMORY_FLAG::READ_WRITE |
													  COMPUTE_MEMORY_FLAG::HOST_READ_WRITE),
				  const uint32_t opengl_type_ = 0) :
	vulkan_buffer(device, sizeof(data_type) * n, (const void*)&data[0], flags_, opengl_type_) {}
	
	~vulkan_buffer() override;

	void read(shared_ptr<compute_queue> cqueue, const size_t size = 0, const size_t offset = 0) override;
	void read(shared_ptr<compute_queue> cqueue, void* dst, const size_t size = 0, const size_t offset = 0) override;
	
	void write(shared_ptr<compute_queue> cqueue, const size_t size = 0, const size_t offset = 0) override;
	void write(shared_ptr<compute_queue> cqueue, const void* src, const size_t size = 0, const size_t offset = 0) override;
	
	void copy(shared_ptr<compute_queue> cqueue,
			  shared_ptr<compute_buffer> src,
			  const size_t size = 0, const size_t src_offset = 0, const size_t dst_offset = 0) override;
	
	void fill(shared_ptr<compute_queue> cqueue,
			  const void* pattern, const size_t& pattern_size,
			  const size_t size = 0, const size_t offset = 0) override;
	
	void zero(shared_ptr<compute_queue> cqueue) override;
	
	bool resize(shared_ptr<compute_queue> cqueue,
				const size_t& size,
				const bool copy_old_data = false,
				const bool copy_host_data = false,
				void* new_host_ptr = nullptr) override;
	
	void* __attribute__((aligned(128))) map(shared_ptr<compute_queue> cqueue,
											const COMPUTE_MEMORY_MAP_FLAG flags =
											(COMPUTE_MEMORY_MAP_FLAG::READ_WRITE |
											 COMPUTE_MEMORY_MAP_FLAG::BLOCK),
											const size_t size = 0, const size_t offset = 0) override;
	
	void unmap(shared_ptr<compute_queue> cqueue, void* __attribute__((aligned(128))) mapped_ptr) override;
	
	bool acquire_opengl_object(shared_ptr<compute_queue> cqueue) override;
	bool release_opengl_object(shared_ptr<compute_queue> cqueue) override;
	
	//! returns the vulkan specific buffer object/pointer
	const VkBuffer& get_vulkan_buffer() const { return buffer; }
	const VkDescriptorBufferInfo* get_vulkan_buffer_info() const { return &buffer_info; }
	
protected:
	VkBuffer buffer { nullptr };
	VkDescriptorBufferInfo buffer_info { nullptr, 0, 0 };
	
	//! separate create buffer function, b/c it's called by the constructor and resize
	bool create_internal(const bool copy_host_data, shared_ptr<compute_queue> cqueue);
	
};

#endif

#endif
