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

#include <floor/device/vulkan/vulkan_memory.hpp>

#if !defined(FLOOR_NO_VULKAN)

#include "internal/vulkan_headers.hpp"
#include <floor/device/vulkan/vulkan_context.hpp>
#include <floor/device/vulkan/vulkan_device.hpp>
#include <floor/device/vulkan/vulkan_queue.hpp>
#include <floor/device/vulkan/vulkan_image.hpp>
#include "internal/vulkan_heap.hpp"

namespace fl {

vulkan_memory::vulkan_memory(const vulkan_device& vk_dev_, const uint64_t* object_, const bool is_image_, const MEMORY_FLAG memory_flags_) noexcept :
vk_dev(vk_dev_), object(object_), is_image(is_image_), memory_flags(memory_flags_) {
}

vulkan_memory::~vulkan_memory() noexcept {
	if (mem != nullptr) {
		if (!is_heap_allocation) {
			vkFreeMemory(vk_dev.device, mem, nullptr);
		}
		// else: heap allocation destroyed in vulkan_buffer/vulkan_image destructor
		mem = nullptr;
	}
}

bool vulkan_memory::write_memory_data(const device_queue& cqueue, const std::span<const uint8_t> data, const size_t& offset,
									  const size_t shim_input_size, const char* error_msg_on_failure) {
	// use VMA copy if this is just a simple memcpy (no shim conversion necessary)
	if (is_heap_allocation && is_heap_allocation_host_visible && shim_input_size == 0u &&
		// TODO: properly support this if host-coherent image memory is supported (currently leads to invalid data)
		!is_image) {
		if (!vk_dev.heap->host_to_device_copy(data.data(), heap_allocation, offset, data.size_bytes())) {
			if (error_msg_on_failure == nullptr) {
				log_error("failed to write Vulkan memory data");
			} else {
				log_error("$", error_msg_on_failure);
			}
			return false;
		}
		return true;
	}
	
	const auto mapping_size = (shim_input_size != 0 ? shim_input_size : data.size_bytes());
	auto mapped_ptr = map(cqueue, MEMORY_MAP_FLAG::WRITE_INVALIDATE | MEMORY_MAP_FLAG::BLOCK, mapping_size, offset);
	if (mapped_ptr != nullptr) {
		assert(mapping_size >= data.size_bytes());
		memcpy(mapped_ptr, data.data(), data.size_bytes());
		if (!unmap(cqueue, mapped_ptr)) {
			return false;
		}
	} else {
		if (error_msg_on_failure == nullptr) {
			log_error("failed to write Vulkan memory data (map failed)");
		} else {
			log_error("$", error_msg_on_failure);
		}
		return false;
	}
	return true;
}

bool vulkan_memory::read_memory_data(const device_queue& cqueue, std::span<uint8_t> data, const size_t& offset,
									 const size_t shim_input_size, const char* error_msg_on_failure) {
	// use VMA copy if this is just a simple memcpy (no shim conversion necessary)
	// NOTE: while VMA recommends VK_MEMORY_PROPERTY_HOST_CACHED_BIT, it is not required
	if (is_heap_allocation && is_heap_allocation_host_visible && shim_input_size == 0u &&
		// TODO: properly support this if host-coherent image memory is supported (currently leads to invalid data)
		!is_image) {
		if (!vk_dev.heap->device_to_host_copy(heap_allocation, data.data(), offset, data.size_bytes())) {
			if (error_msg_on_failure == nullptr) {
				log_error("failed to read Vulkan memory data");
			} else {
				log_error("$", error_msg_on_failure);
			}
			return false;
		}
		return true;
	}
	
	const auto mapping_size = (shim_input_size != 0 ? shim_input_size : data.size_bytes());
	auto mapped_ptr = map(cqueue, MEMORY_MAP_FLAG::READ | MEMORY_MAP_FLAG::BLOCK, mapping_size, offset);
	if (mapped_ptr != nullptr) {
		assert(mapping_size >= data.size_bytes());
		memcpy(data.data(), mapped_ptr, data.size_bytes());
		if (!unmap(cqueue, mapped_ptr)) {
			return false;
		}
	} else {
		if (error_msg_on_failure == nullptr) {
			log_error("failed to read Vulkan memory data (map failed)");
		} else {
			log_error("$", error_msg_on_failure);
		}
		return false;
	}
	return true;
}

void* __attribute__((aligned(128))) vulkan_memory::map(const device_queue& cqueue,
													   const MEMORY_MAP_FLAG flags,
													   const size_t size, const size_t offset) {
	if (*object == 0) {
		return nullptr;
	}
	
	const bool blocking_map = has_flag<MEMORY_MAP_FLAG::BLOCK>(flags);
	bool does_read = false, does_write = false;
	if (has_flag<MEMORY_MAP_FLAG::WRITE_INVALIDATE>(flags)) {
		does_write = true;
	} else {
		switch (flags & MEMORY_MAP_FLAG::READ_WRITE) {
			case MEMORY_MAP_FLAG::READ:
				does_read = true;
				break;
			case MEMORY_MAP_FLAG::WRITE:
				does_write = true;
				break;
			case MEMORY_MAP_FLAG::READ_WRITE:
				does_read = true;
				does_write = true;
				break;
			case MEMORY_MAP_FLAG::NONE:
			default:
				log_error("neither read nor write flag set for buffer mapping!");
				return nullptr;
		}
	}
	const bool write_only = (!does_read && does_write);
	
	// here is the deal with Vulkan device memory:
	//  * we always allocate device local memory, regardless of any host-visibility
	//  * if the device local memory is not host-visible, we need to allocate an appropriately sized
	//    host-visible buffer, then: a) for read: create a device -> host memory copy (in here)
	//                               b) for write: create a host buffer -> device memory copy (in unmap)
	//  * if the device local memory is host-visible/host-coherent, we can just call Vulkan map/unmap functions
	const auto is_host_coherent = has_flag<MEMORY_FLAG::VULKAN_HOST_COHERENT>(memory_flags);
	
	// create the host-visible buffer if necessary
	vulkan_mapping mapping {
		.size = size,
		.offset = offset,
		.flags = flags,
	};
	size_t host_buffer_offset = offset;
	auto vulkan_dev = vk_dev.device;
	// TODO: make sure this is cleaned up properly on failure
	if (!is_host_coherent || is_image) { // TODO: or already created host-visible (-> create_internal)
		// we're only creating a buffer that is large enough + start from the offset
		host_buffer_offset = 0;
		
		// create the host-visible buffer
		const auto is_concurrent_sharing = (vk_dev.all_queue_family_index != vk_dev.compute_queue_family_index);
		const VkBufferUsageFlags2CreateInfo buffer_usage_flags_info {
			.sType = VK_STRUCTURE_TYPE_BUFFER_USAGE_FLAGS_2_CREATE_INFO,
			.pNext = nullptr,
			.usage = VkBufferUsageFlags2((does_write ? VK_BUFFER_USAGE_2_TRANSFER_SRC_BIT : VkBufferUsageFlagBits2(0u)) |
										 (does_read ? VK_BUFFER_USAGE_2_TRANSFER_DST_BIT : VkBufferUsageFlagBits2(0u))),
		};
		const VkBufferCreateInfo buffer_create_info {
			.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
			.pNext = &buffer_usage_flags_info,
			.flags = 0,
			.size = size,
			.usage = 0,
			.sharingMode = (is_concurrent_sharing ? VK_SHARING_MODE_CONCURRENT : VK_SHARING_MODE_EXCLUSIVE),
			.queueFamilyIndexCount = (is_concurrent_sharing ? uint32_t(vk_dev.queue_families.size()) : 0),
			.pQueueFamilyIndices = (is_concurrent_sharing ? vk_dev.queue_families.data() : nullptr),
		};
		
		if (!is_heap_allocation) {
			VK_CALL_RET(vkCreateBuffer(vulkan_dev, &buffer_create_info, nullptr, &mapping.buffer), "map buffer creation failed", nullptr)
			
			// allocate / back it up
			VkMemoryDedicatedRequirements ded_req {
				.sType = VK_STRUCTURE_TYPE_MEMORY_DEDICATED_REQUIREMENTS,
				.pNext = nullptr,
				.prefersDedicatedAllocation = false,
				.requiresDedicatedAllocation = false,
			};
			VkMemoryRequirements2 mem_req2 {
				.sType = VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2,
				.pNext = &ded_req,
				.memoryRequirements = {},
			};
			const VkBufferMemoryRequirementsInfo2 mem_req_info {
				.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_REQUIREMENTS_INFO_2,
				.pNext = nullptr,
				.buffer = mapping.buffer,
			};
			vkGetBufferMemoryRequirements2(vulkan_dev, &mem_req_info, &mem_req2);
			const auto is_dedicated = (ded_req.prefersDedicatedAllocation || ded_req.requiresDedicatedAllocation);
			const auto& mem_req = mem_req2.memoryRequirements;
			
			const VkMemoryDedicatedAllocateInfo ded_alloc_info {
				.sType = VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO,
				.pNext = nullptr,
				.image = VK_NULL_HANDLE,
				.buffer = mapping.buffer,
			};
			const VkMemoryAllocateInfo alloc_info {
				.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
				.pNext = (is_dedicated ? &ded_alloc_info : nullptr),
				.allocationSize = mem_req.size,
				.memoryTypeIndex = vk_dev.host_mem_cached_index,
			};
			VK_CALL_RET(vkAllocateMemory(vulkan_dev, &alloc_info, nullptr, &mapping.mem), "map buffer allocation failed", nullptr)
			const VkBindBufferMemoryInfo bind_info {
				.sType = VK_STRUCTURE_TYPE_BIND_BUFFER_MEMORY_INFO,
				.pNext = nullptr,
				.buffer = mapping.buffer,
				.memory = mapping.mem,
				.memoryOffset = 0,
			};
			VK_CALL_RET(vkBindBufferMemory2(vulkan_dev, 1, &bind_info), "map buffer allocation binding failed", nullptr)
		} else {
			// if this memory object is a heap-allocation, also make the staging buffer a heap-allocation
			auto alloc = vk_dev.heap->create_buffer(buffer_create_info,
													write_only ? MEMORY_FLAG::HOST_WRITE : MEMORY_FLAG::HOST_READ_WRITE);
			if (!alloc.is_valid()) {
				log_error("map heap buffer creation failed");
				return nullptr;
			}
			mapping.buffer = alloc.buffer;
			mapping.staging_allocation = alloc.allocation;
			mapping.mem = alloc.memory;
		}
	} else {
		mapping.buffer = (VkBuffer)*object;
		mapping.mem = mem;
	}
	
	// check if we need to copy the buffer from the device (in case READ was specified)
	const auto& vk_queue = (const vulkan_queue&)cqueue;
	if (!write_only) {
		if (blocking_map) {
			// must finish up all current work before we can properly read from the current buffer
			cqueue.finish();
		}
		
		// device -> host buffer copy
		if (!is_host_coherent || is_image) {
			VK_CMD_BLOCK(vk_queue, "dev -> host memory copy", ({
				if (!is_image) {
					const VkBufferCopy2 region {
						.sType = VK_STRUCTURE_TYPE_BUFFER_COPY_2,
						.pNext = nullptr,
						.srcOffset = mapping.offset,
						.dstOffset = 0,
						.size = mapping.size,
					};
					const VkCopyBufferInfo2 info {
						.sType = VK_STRUCTURE_TYPE_COPY_BUFFER_INFO_2,
						.pNext = nullptr,
						.srcBuffer = (VkBuffer)*object,
						.dstBuffer = mapping.buffer,
						.regionCount = 1,
						.pRegions = &region,
					};
					vkCmdCopyBuffer2(block_cmd_buffer.cmd_buffer, &info);
				} else {
					image_copy_dev_to_host(cqueue, block_cmd_buffer.cmd_buffer, mapping.buffer);
				}
			}), blocking_map);
		} else {
			VK_CMD_BLOCK(vk_queue, "dev -> host memory barrier", ({
				const VkBufferMemoryBarrier2 buffer_barrier {
					.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2,
					.pNext = nullptr,
					.srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
					.srcAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT,
					.dstStageMask = VK_PIPELINE_STAGE_2_HOST_BIT,
					.dstAccessMask = VkAccessFlags2(VK_ACCESS_2_HOST_READ_BIT |
													(does_write ? VK_ACCESS_2_HOST_WRITE_BIT : VkAccessFlagBits2(0u))),
					.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
					.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
					.buffer = mapping.buffer,
					.offset = mapping.offset,
					.size = mapping.size,
				};
				const VkDependencyInfo dep_info {
					.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
					.pNext = nullptr,
					.dependencyFlags = 0,
					.memoryBarrierCount = 0,
					.pMemoryBarriers = nullptr,
					.bufferMemoryBarrierCount = 1,
					.pBufferMemoryBarriers = &buffer_barrier,
					.imageMemoryBarrierCount = 0,
					.pImageMemoryBarriers = nullptr,
				};
				vkCmdPipelineBarrier2(block_cmd_buffer.cmd_buffer, &dep_info);
			}), blocking_map);
		}
	}
	
	// TODO: use vkFlushMappedMemoryRanges/vkInvalidateMappedMemoryRanges if necessary (non-coherent)
	
	// map the host buffer
	void* __attribute__((aligned(128))) host_ptr { nullptr };
	if (!is_heap_allocation) {
		const VkMemoryMapInfo map_info {
			.sType = VK_STRUCTURE_TYPE_MEMORY_MAP_INFO,
			.pNext = nullptr,
			.flags = 0,
			.memory = mapping.mem,
			.offset = host_buffer_offset,
			.size = size,
		};
		if (vk_dev.vulkan_version >= VULKAN_VERSION::VULKAN_1_4) {
			VK_CALL_RET(vkMapMemory2(vulkan_dev, &map_info, &host_ptr), "failed to map host buffer", nullptr)
		} else {
			VK_CALL_RET(vkMapMemory2KHR(vulkan_dev, &map_info, &host_ptr), "failed to map host buffer", nullptr)
		}
		mapping.base_address = host_ptr;
	} else {
		// use VMA if heap-allocated
		host_ptr = ((const vulkan_device&)cqueue.get_device()).heap->map_memory(mapping.staging_allocation ?
																				mapping.staging_allocation : heap_allocation);
		if (!host_ptr) {
			return nullptr; // error already reported in map_memory()
		}
		
		// VMA currently does not support partial buffer mappings, so we need to take care of this
		mapping.base_address = host_ptr;
		host_ptr = (uint8_t*)host_ptr + host_buffer_offset;
	}
	
	// need to remember how much we mapped and where (so the host -> device write-back copies the right amount of bytes)
	mappings.emplace(host_ptr, mapping);
	
	return host_ptr;
}

bool vulkan_memory::unmap(const device_queue& cqueue, void* __attribute__((aligned(128))) mapped_ptr) {
	if (*object == 0 || mapped_ptr == nullptr) {
		return false;
	}
	
	const auto& vk_queue = (const vulkan_queue&)cqueue;
	auto vulkan_dev = vk_dev.device;
	
	// check if this is actually a mapped pointer (+get the mapped size)
	const auto iter = mappings.find(mapped_ptr);
	if (iter == mappings.end()) {
		log_error("invalid mapped pointer: $X", mapped_ptr);
		return false;
	}
	const auto& mapping = iter->second;
	
	const auto is_host_coherent = has_flag<MEMORY_FLAG::VULKAN_HOST_COHERENT>(memory_flags);
	
	// check if we need to actually copy data back to the device (not the case if read-only mapping)
	if (has_flag<MEMORY_MAP_FLAG::WRITE>(mapping.flags) ||
		has_flag<MEMORY_MAP_FLAG::WRITE_INVALIDATE>(mapping.flags)) {
		if (!is_host_coherent || is_image) {
			do {
				// host -> device copy
				VK_CMD_BLOCK(vk_queue, "host -> dev memory copy", ({
					if (!is_image) {
						const VkBufferCopy2 region {
							.sType = VK_STRUCTURE_TYPE_BUFFER_COPY_2,
							.pNext = nullptr,
							.srcOffset = 0,
							.dstOffset = mapping.offset,
							.size = mapping.size,
						};
						const VkCopyBufferInfo2 info {
							.sType = VK_STRUCTURE_TYPE_COPY_BUFFER_INFO_2,
							.pNext = nullptr,
							.srcBuffer = mapping.buffer,
							.dstBuffer = (VkBuffer)*object,
							.regionCount = 1,
							.pRegions = &region,
						};
						vkCmdCopyBuffer2(block_cmd_buffer.cmd_buffer, &info);
					} else {
						std::span<uint8_t> host_buffer { (uint8_t*)mapped_ptr, mapping.size };
						image_copy_host_to_dev(cqueue, block_cmd_buffer.cmd_buffer, mapping.buffer, host_buffer);
					}
				}), has_flag<MEMORY_MAP_FLAG::BLOCK>(mapping.flags));
			} while(false);
		}
	}
	
	// unmap
	// TODO/NOTE: we can only unmap the whole buffer with Vulkan, not individual mappings ...
	// -> can only unmap if this is the last mapping (if is_host_coherent, if the buffer was just created/allocated for this, then it doesn't matter)
	// also: TODO: SYNC!
	if (!is_heap_allocation) {
		const VkMemoryUnmapInfo unmap_info {
			.sType = VK_STRUCTURE_TYPE_MEMORY_UNMAP_INFO,
			.pNext = nullptr,
			.flags = 0,
			.memory = mapping.mem,
		};
		// NOTE: can't do much more than ignore errors at this point
		if (vk_dev.vulkan_version >= VULKAN_VERSION::VULKAN_1_4) {
			VK_CALL_IGNORE(vkUnmapMemory2(vulkan_dev, &unmap_info), "failed to unmap Vulkan memory")
		} else {
			VK_CALL_IGNORE(vkUnmapMemory2KHR(vulkan_dev, &unmap_info), "failed to unmap Vulkan memory")
		}
	} else {
		// use VMA if heap-allocated
		vk_dev.heap->unmap_memory(mapping.staging_allocation ? mapping.staging_allocation : heap_allocation);
	}
	
	// barrier after unmap when using unified memory
	if (has_flag<MEMORY_MAP_FLAG::WRITE>(mapping.flags) ||
		has_flag<MEMORY_MAP_FLAG::WRITE_INVALIDATE>(mapping.flags)) {
		if (is_host_coherent && !is_image) {
			VK_CMD_BLOCK(vk_queue, "host -> dev memory barrier", ({
				const VkBufferMemoryBarrier2 buffer_barrier {
					.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2,
					.pNext = nullptr,
					.srcStageMask = VK_PIPELINE_STAGE_2_HOST_BIT,
					.srcAccessMask = VkAccessFlags2(VK_ACCESS_2_HOST_WRITE_BIT |
													(has_flag<MEMORY_MAP_FLAG::READ>(mapping.flags) ?
													 VK_ACCESS_2_HOST_READ_BIT : VkAccessFlagBits2(0u))),
					.dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
					.dstAccessMask = (VK_ACCESS_2_MEMORY_READ_BIT |
									  VK_ACCESS_2_MEMORY_WRITE_BIT |
									  VK_ACCESS_2_SHADER_READ_BIT |
									  VK_ACCESS_2_SHADER_WRITE_BIT),
					.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
					.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
					.buffer = mapping.buffer,
					.offset = mapping.offset,
					.size = mapping.size,
				};
				const VkDependencyInfo dep_info {
					.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
					.pNext = nullptr,
					.dependencyFlags = 0,
					.memoryBarrierCount = 0,
					.pMemoryBarriers = nullptr,
					.bufferMemoryBarrierCount = 1,
					.pBufferMemoryBarriers = &buffer_barrier,
					.imageMemoryBarrierCount = 0,
					.pImageMemoryBarriers = nullptr,
				};
				vkCmdPipelineBarrier2(block_cmd_buffer.cmd_buffer, &dep_info);
			}), has_flag<MEMORY_MAP_FLAG::BLOCK>(mapping.flags));
		}
	}
	
	// delete host buffer
	if (!is_host_coherent || is_image) {
		if (!mapping.staging_allocation) {
			if (mapping.buffer != nullptr) {
				vkDestroyBuffer(vulkan_dev, mapping.buffer, nullptr);
			}
			if (mapping.mem != nullptr) {
				vkFreeMemory(vulkan_dev, mapping.mem, nullptr);
			}
		} else {
			vk_dev.heap->destroy_allocation(mapping.staging_allocation, mapping.buffer);
		}
	}
	
	// remove the mapping
	mappings.erase(mapped_ptr);
	
	return true;
}

uint32_t vulkan_memory::find_memory_type_index(const uint32_t memory_type_bits,
											   const bool want_device_memory,
											   const bool requires_device_memory,
											   const bool requires_host_coherent) const {
	const auto find_index = [](const std::vector<uint32_t>& indices,
							   const uint32_t& preferred_index,
							   const uint32_t& type_bits) -> std::pair<bool, uint32_t> {
		// check if preferred index is possible
		const uint32_t preferred_mask = 1u << preferred_index;
		if((type_bits & preferred_mask) == preferred_mask) {
			return { true, preferred_index };
		}
		
		// check all other supported indices
		for(const auto& idx : indices) {
			const uint32_t idx_mask = 1u << idx;
			if((type_bits & idx_mask) == idx_mask) {
				return { true, idx };
			}
		}
		
		// no match
		return { false, 0 };
	};
	
	std::pair<bool, uint32_t> ret { false, 0 };
	
	// if device memory is wanted or required, try this first
	if(want_device_memory || requires_device_memory) {
		// select between device-only and device+host-coherent memory
		ret = (!requires_host_coherent ?
			   find_index(vk_dev.device_mem_indices, vk_dev.device_mem_index, memory_type_bits) :
			   find_index(vk_dev.device_mem_host_coherent_indices, vk_dev.device_mem_host_coherent_index, memory_type_bits));
		if(ret.first) {
			return ret.second;
		}
		if (requires_device_memory) {
			log_error("could not find device-local memory");
			return 0;
		}
	}
	
	if (requires_host_coherent) {
		ret = find_index(vk_dev.device_mem_host_coherent_indices, vk_dev.device_mem_host_coherent_index, memory_type_bits);
		if(ret.first) return ret.second;
	} else {
		// check cached first, then coherent
		ret = find_index(vk_dev.host_mem_cached_indices, vk_dev.host_mem_cached_index, memory_type_bits);
		if(ret.first) return ret.second;
		ret = find_index(vk_dev.device_mem_host_coherent_indices, vk_dev.device_mem_host_coherent_index, memory_type_bits);
		if(ret.first) return ret.second;
	}
	
	// no memory found for this
	log_error("could not find a memory type index for the request memory type bits $X (device memory wanted: $)",
			  memory_type_bits, want_device_memory);
	return 0;
}

} // namespace fl

#endif
