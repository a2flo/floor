/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2022 Florian Ziesche
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

#ifndef __FLOOR_GRAPHICS_VULKAN_VULKAN_DESCRIPTOR_SET_HPP__
#define __FLOOR_GRAPHICS_VULKAN_VULKAN_DESCRIPTOR_SET_HPP__

#include <floor/compute/vulkan/vulkan_common.hpp>

#if !defined(FLOOR_NO_VULKAN)
#include <floor/threading/safe_resource_container.hpp>

struct descriptor_set_instance_t;
struct descriptor_buffer_instance_t;
class compute_buffer;

//! a thread-safe container of multiple descriptor sets of the same type (enabling multi-threaded descriptor set usage)
class vulkan_descriptor_set_container {
public:
	//! amount of contained descriptor sets
	static constexpr const uint32_t descriptor_count { 16u };
	
	vulkan_descriptor_set_container(array<VkDescriptorSet, descriptor_count>&& desc_sets_) : descriptor_sets(move(desc_sets_)) {}
	
	//! acquire a descriptor set instance
	//! NOTE: the returned object is a RAII object that will automatically call release_descriptor_set on destruction
	descriptor_set_instance_t acquire_descriptor_set();
	
	//! release a descriptor set instance again
	//! NOTE: this generally doesn't have to be called manually (see acquire_descriptor_set())
	void release_descriptor_set(descriptor_set_instance_t& instance);
	
protected:
	safe_resource_container<VkDescriptorSet, descriptor_count> descriptor_sets;
	
};

//! a descriptor set instance that can be used in a single thread for a single execution
//! NOTE: will auto-release on destruction
struct descriptor_set_instance_t {
	friend vulkan_descriptor_set_container;
	
	VkDescriptorSet desc_set { nullptr };

	constexpr descriptor_set_instance_t() noexcept {}
	
	descriptor_set_instance_t(VkDescriptorSet desc_set_, const uint32_t& index_, vulkan_descriptor_set_container& container_) :
	desc_set(desc_set_), index(index_), container(&container_) {}
	
	descriptor_set_instance_t(descriptor_set_instance_t&& instance) : desc_set(instance.desc_set), index(instance.index), container(instance.container) {
		instance.desc_set = nullptr;
		instance.index = 0;
		instance.container = nullptr;
	}
	descriptor_set_instance_t& operator=(descriptor_set_instance_t&& instance) {
		assert(desc_set == nullptr && index == ~0u && container == nullptr);
		swap(desc_set, instance.desc_set);
		swap(index, instance.index);
		swap(container, instance.container);
		return *this;
	}
	
	~descriptor_set_instance_t() {
		if (desc_set != nullptr) {
			assert(container != nullptr);
			container->release_descriptor_set(*this);
		}
	}
	
	descriptor_set_instance_t(const descriptor_set_instance_t&) = delete;
	descriptor_set_instance_t& operator=(const descriptor_set_instance_t&) = delete;
	
protected:
	//! index of this resource in the parent container (needed for auto-release)
	uint32_t index { ~0u };
	//! pointer to the parent container (needed for auto-release)
	vulkan_descriptor_set_container* container { nullptr };
};

//! a thread-safe container of multiple descriptor buffers of the same type (enabling multi-threaded descriptor buffer usage)
class vulkan_descriptor_buffer_container {
public:
	//! amount of contained descriptor buffer
	static constexpr const uint32_t descriptor_count { 16u };

	//! { Vulkan buffer, mapped host memory }
	using resource_type = pair<shared_ptr<compute_buffer>, span<uint8_t>>;
	
	vulkan_descriptor_buffer_container(array<resource_type, descriptor_count>&& desc_bufs_) : descriptor_buffers(move(desc_bufs_)) {}
	
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
	
	compute_buffer* desc_buffer;
	span<uint8_t> mapped_host_memory;

	constexpr descriptor_buffer_instance_t() noexcept {}
	
	descriptor_buffer_instance_t(compute_buffer* desc_buffer_, span<uint8_t> mapped_host_memory_, const uint32_t& index_, vulkan_descriptor_buffer_container& container_) :
	desc_buffer(desc_buffer_), mapped_host_memory(mapped_host_memory_), index(index_), container(&container_) {}
	
	descriptor_buffer_instance_t(descriptor_buffer_instance_t&& instance) : desc_buffer(instance.desc_buffer), mapped_host_memory(instance.mapped_host_memory), index(instance.index), container(instance.container) {
		instance.desc_buffer = nullptr;
		instance.mapped_host_memory = {};
		instance.index = 0;
		instance.container = nullptr;
	}
	descriptor_buffer_instance_t& operator=(descriptor_buffer_instance_t&& instance) {
		assert(desc_buffer == nullptr && mapped_host_memory.size() > 0 && index == ~0u && container == nullptr);
		swap(desc_buffer, instance.desc_buffer);
		swap(mapped_host_memory, instance.mapped_host_memory);
		swap(index, instance.index);
		swap(container, instance.container);
		return *this;
	}
	
	~descriptor_buffer_instance_t() {
		if (desc_buffer != nullptr) {
			assert(container != nullptr);
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

#endif

#endif
