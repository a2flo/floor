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

#include <floor/core/essentials.hpp>

#if !defined(FLOOR_NO_VULKAN)
#include <floor/threading/safe_resource_container.hpp>
#include <floor/core/flat_map.hpp>

namespace fl {

struct descriptor_buffer_instance_t;
class device_buffer;

//! a thread-safe container of multiple descriptor buffers of the same type (enabling multi-threaded descriptor buffer usage)
class vulkan_descriptor_buffer_container {
public:
	//! amount of contained descriptor buffer
	static constexpr const uint32_t descriptor_count { 16u };
	
	//! { Vulkan buffer, mapped host memory }
	using resource_type = std::pair<std::shared_ptr<device_buffer>, std::span<uint8_t>>;
	
	vulkan_descriptor_buffer_container(std::array<resource_type, descriptor_count>&& desc_bufs_) : descriptor_buffers(std::move(desc_bufs_)) {}
	
	//! acquire a descriptor buffer instance
	//! NOTE: the returned object is a RAII object that will automatically call release_descriptor_buffer on destruction
	descriptor_buffer_instance_t acquire_descriptor_buffer();
	
	//! release a descriptor buffer instance again
	//! NOTE: this generally doesn't have to be called manually (see acquire_descriptor_buffer())
	void release_descriptor_buffer(descriptor_buffer_instance_t& instance);
	
protected:
	safe_resource_container<resource_type, descriptor_count> descriptor_buffers;
	
};

//! a descriptor buffer instance that can be used in a single thread for a single execution
//! NOTE: will auto-release on destruction
struct descriptor_buffer_instance_t {
	friend vulkan_descriptor_buffer_container;
	
	const device_buffer* desc_buffer { nullptr };
	std::span<uint8_t> mapped_host_memory;
	
	constexpr descriptor_buffer_instance_t() noexcept {}
	
	descriptor_buffer_instance_t(const device_buffer* desc_buffer_, std::span<uint8_t> mapped_host_memory_,
								 const uint32_t& index_, vulkan_descriptor_buffer_container* container_) :
	desc_buffer(desc_buffer_), mapped_host_memory(mapped_host_memory_), index(index_), container(container_) {}
	
	descriptor_buffer_instance_t(descriptor_buffer_instance_t&& instance) : desc_buffer(instance.desc_buffer), mapped_host_memory(instance.mapped_host_memory), index(instance.index), container(instance.container) {
		instance.desc_buffer = nullptr;
		instance.mapped_host_memory = {};
		instance.index = 0;
		instance.container = nullptr;
	}
	descriptor_buffer_instance_t& operator=(descriptor_buffer_instance_t&& instance) {
		assert(desc_buffer == nullptr && mapped_host_memory.size() > 0 && index == ~0u && container == nullptr);
		std::swap(desc_buffer, instance.desc_buffer);
		std::swap(mapped_host_memory, instance.mapped_host_memory);
		std::swap(index, instance.index);
		std::swap(container, instance.container);
		return *this;
	}
	
	~descriptor_buffer_instance_t() {
		if (desc_buffer != nullptr && container != nullptr) {
			assert(index != ~0u);
			container->release_descriptor_buffer(*this);
		}
	}
	
	descriptor_buffer_instance_t(const descriptor_buffer_instance_t&) = delete;
	descriptor_buffer_instance_t& operator=(const descriptor_buffer_instance_t&) = delete;
	
protected:
	//! index of this resource in the parent container (needed for auto-release)
	uint32_t index { ~0u };
	//! pointer to the parent container (needed for auto-release)
	vulkan_descriptor_buffer_container* container { nullptr };
};

//! used in descriptor sets for parameters that don't fit IUBs
struct vulkan_constant_buffer_info_t {
	uint32_t offset;
	uint32_t size;
};

//! descriptor set layout definition
struct vulkan_descriptor_set_layout_t {
	VkDescriptorSetLayout desc_set_layout { nullptr };
	//! reported (and potentially unaligned) layout size in bytes
	VkDeviceSize layout_size { 0u };
	
	uint32_t ssbo_desc { 0u };
	uint32_t iub_desc { 0u };
	uint32_t read_image_desc { 0u };
	uint32_t write_image_desc { 0u };
	uint32_t max_iub_size { 0u };
	uint32_t constant_arg_count { 0u };
	uint32_t constant_buffer_size { 0u };
	
	std::vector<VkDescriptorSetLayoutBinding> bindings;
	fl::flat_map<uint32_t, vulkan_constant_buffer_info_t> constant_buffer_info;
};

} // namespace fl

#endif // FLOOR_NO_VULKAN
