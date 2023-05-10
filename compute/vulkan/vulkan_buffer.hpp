/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2023 Florian Ziesche
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
	vulkan_buffer(const compute_queue& cqueue,
				  const size_t& size_,
				  std::span<uint8_t> host_data_,
				  const COMPUTE_MEMORY_FLAG flags_ = (COMPUTE_MEMORY_FLAG::READ_WRITE |
													  COMPUTE_MEMORY_FLAG::HOST_READ_WRITE),
				  const uint32_t opengl_type_ = 0,
				  const uint32_t external_gl_object_ = 0);
	
	vulkan_buffer(const compute_queue& cqueue,
				  const size_t& size_,
				  const COMPUTE_MEMORY_FLAG flags_ = (COMPUTE_MEMORY_FLAG::READ_WRITE |
													  COMPUTE_MEMORY_FLAG::HOST_READ_WRITE),
				  const uint32_t opengl_type_ = 0) :
	vulkan_buffer(cqueue, size_, {}, flags_, opengl_type_) {}
	
	~vulkan_buffer() override;

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
	
	bool acquire_opengl_object(const compute_queue* cqueue) override;
	bool release_opengl_object(const compute_queue* cqueue) override;
	
	//! returns the vulkan specific buffer object/pointer
	const VkBuffer& get_vulkan_buffer() const { return buffer; }
	const VkDescriptorBufferInfo* get_vulkan_buffer_info() const { return &buffer_info; }
	const VkDeviceAddress& get_vulkan_buffer_device_address() const { return buffer_device_address; }
	
	//! returns the Vulkan shared memory handle (nullptr/0 if !shared)
	const auto& get_vulkan_shared_handle() const {
		return shared_handle;
	}
	
	//! returns the actual allocation size in bytes this buffer has been created with
	//! NOTE: allocation size has been computed via vkGetBufferMemoryRequirements and can differ from "size"
	const VkDeviceSize& get_vulkan_allocation_size() const {
		return allocation_size;
	}
	
	void set_debug_label(const string& label) override;
	
	//! max size of an SSBO descriptor
	static constexpr const uint32_t max_ssbo_descriptor_size { 16u };
	
	//! returns the descriptor data for this buffer (for use in descriptor buffers)
	const auto& get_vulkan_descriptor_data() const {
		return descriptor_data;
	}
	
	//! returns the usage flags that this Vulkan buffer was created with
	VkBufferUsageFlags get_vulkan_buffer_usage() const {
		return buffer_usage;
	}
	
	//! returns the underlying Vulkan buffer that should be used on the device (i.e. this or a shared buffer)
	const vulkan_buffer* get_underlying_vulkan_buffer_safe() const {
		if (has_flag<COMPUTE_MEMORY_FLAG::VULKAN_SHARING>(flags)) {
			const vulkan_buffer* ret = nullptr;
			if (ret = get_shared_vulkan_buffer(); !ret) {
				ret = this;
			}
#if defined(FLOOR_DEBUG)
			if (auto test_cast_vk_buffer = dynamic_cast<const vulkan_buffer*>(ret); !test_cast_vk_buffer) {
				throw runtime_error("specified buffer is neither a Vulkan buffer nor a shared Vulkan buffer");
			}
#endif
			return ret;
		}
		return this;
	}
	
protected:
	VkBuffer buffer { nullptr };
	VkDescriptorBufferInfo buffer_info { nullptr, 0, 0 };
	VkDeviceSize allocation_size { 0 };
	VkDeviceAddress buffer_device_address { 0 };
	VkBufferUsageFlags buffer_usage { 0 };
	
	//! when using descriptor buffers, this contains the descriptor data (as a SSBO descriptor)
	array<uint8_t, max_ssbo_descriptor_size> descriptor_data {{
		0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u,
		0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u,
	}};
	
	// shared memory handle when the buffer has been created with VULKAN_SHARING
#if defined(__WINDOWS__)
	void* /* HANDLE */ shared_handle { nullptr };
#else
	int shared_handle { 0 };
#endif
	
	//! separate create buffer function, b/c it's called by the constructor and resize
	bool create_internal(const bool copy_host_data, const compute_queue& cqueue);
	
};

#endif

#endif
