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

#include <floor/compute/vulkan/vulkan_memory.hpp>

#if !defined(FLOOR_NO_VULKAN)

#include <floor/compute/vulkan/vulkan_compute.hpp>
#include <floor/compute/vulkan/vulkan_device.hpp>
#include <floor/compute/vulkan/vulkan_queue.hpp>
#include <floor/compute/vulkan/vulkan_image.hpp>

vulkan_memory::vulkan_memory(const vulkan_device& device_, const uint64_t* object_, const bool is_image_, const COMPUTE_MEMORY_FLAG memory_flags_) noexcept :
device(device_), object(object_), is_image(is_image_), memory_flags(memory_flags_) {
}

vulkan_memory::~vulkan_memory() noexcept {
	if(mem != nullptr) {
		vkFreeMemory(device.device, mem, nullptr);
		mem = nullptr;
	}
}

bool vulkan_memory::write_memory_data(const compute_queue& cqueue, const void* data, const size_t& size, const size_t& offset,
									  const size_t non_shim_input_size, const char* error_msg_on_failure) {
	// we definitively need a queue for this (use specified one if possible, otherwise use the default queue)
	auto mapped_ptr = map(cqueue, COMPUTE_MEMORY_MAP_FLAG::WRITE_INVALIDATE | COMPUTE_MEMORY_MAP_FLAG::BLOCK, size, offset);
	if(mapped_ptr != nullptr) {
		memcpy(mapped_ptr, data, (non_shim_input_size == 0 ? size : non_shim_input_size));
		if (!unmap(cqueue, mapped_ptr)) {
			return false;
		}
	}
	else {
		if(error_msg_on_failure == nullptr) {
			log_error("failed to write vulkan memory data (map failed)");
		}
		else log_error("$", error_msg_on_failure);
		return false;
	}
	return true;
}

bool vulkan_memory::read_memory_data(const compute_queue& cqueue, void* data, const size_t& size, const size_t& offset,
									 const size_t non_shim_input_size, const char* error_msg_on_failure) {
	auto mapped_ptr = map(cqueue, COMPUTE_MEMORY_MAP_FLAG::READ | COMPUTE_MEMORY_MAP_FLAG::BLOCK, size, offset);
	if(mapped_ptr != nullptr) {
		memcpy(data, mapped_ptr, (non_shim_input_size == 0 ? size : non_shim_input_size));
		if (!unmap(cqueue, mapped_ptr)) {
			return false;
		}
	}
	else {
		if(error_msg_on_failure == nullptr) {
			log_error("failed to read vulkan memory data (map failed)");
		}
		else log_error("$", error_msg_on_failure);
		return false;
	}
	return true;
}

void* __attribute__((aligned(128))) vulkan_memory::map(const compute_queue& cqueue,
													   const COMPUTE_MEMORY_MAP_FLAG flags,
													   const size_t size, const size_t offset) {
	if(*object == 0) return nullptr;
	
	const bool blocking_map = has_flag<COMPUTE_MEMORY_MAP_FLAG::BLOCK>(flags);
	bool does_read = false, does_write = false;
	if(has_flag<COMPUTE_MEMORY_MAP_FLAG::WRITE_INVALIDATE>(flags)) {
		does_write = true;
	}
	else {
		switch(flags & COMPUTE_MEMORY_MAP_FLAG::READ_WRITE) {
			case COMPUTE_MEMORY_MAP_FLAG::READ:
				does_read = true;
				break;
			case COMPUTE_MEMORY_MAP_FLAG::WRITE:
				does_write = true;
				break;
			case COMPUTE_MEMORY_MAP_FLAG::READ_WRITE:
				does_read = true;
				does_write = true;
				break;
			case COMPUTE_MEMORY_MAP_FLAG::NONE:
			default:
				log_error("neither read nor write flag set for buffer mapping!");
				return nullptr;
		}
	}
	const bool write_only = (!does_read && does_write);
	
	// here is the deal with vulkan device memory:
	//  * we always allocate device local memory, regardless of any host-visibility
	//  * if the device local memory is not host-visible, we need to allocate an appropriately sized
	//    host-visible buffer, then: a) for read: create a device -> host memory copy (in here)
	//                               b) for write: create a host buffer -> device memory copy (in unmap)
	//  * if the device local memory is host-visible/host-coherent, we can just call vulkan map/unmap functions
	const auto is_host_coherent = has_flag<COMPUTE_MEMORY_FLAG::VULKAN_HOST_COHERENT>(memory_flags);
	
	// create the host-visible buffer if necessary
	vulkan_mapping mapping {
		.buffer = nullptr,
		.mem = nullptr,
		.size = size,
		.offset = offset,
		.flags = flags,
	};
	size_t host_buffer_offset = offset;
	auto vulkan_dev = device.device;
	// TODO: make sure this is cleaned up properly on failure
	if (!is_host_coherent || is_image) { // TODO: or already created host-visible (-> create_internal)
		// we're only creating a buffer that is large enough + start from the offset
		host_buffer_offset = 0;
		
		// create the host-visible buffer
		const VkBufferCreateInfo buffer_create_info {
			.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
			.pNext = nullptr,
			.flags = 0,
			.size = size,
			.usage = VkBufferUsageFlags((does_write ? VK_BUFFER_USAGE_TRANSFER_SRC_BIT : VkBufferUsageFlagBits(0u)) |
										(does_read ? VK_BUFFER_USAGE_TRANSFER_DST_BIT : VkBufferUsageFlagBits(0u))),
			.sharingMode = VK_SHARING_MODE_EXCLUSIVE,
			.queueFamilyIndexCount = 0,
			.pQueueFamilyIndices = nullptr,
		};
		VK_CALL_RET(vkCreateBuffer(vulkan_dev, &buffer_create_info, nullptr, &mapping.buffer), "map buffer creation failed", nullptr)
	
		// allocate / back it up
		VkMemoryRequirements mem_req;
		vkGetBufferMemoryRequirements(vulkan_dev, mapping.buffer, &mem_req);
	
		const VkMemoryAllocateInfo alloc_info {
			.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
			.pNext = nullptr,
			.allocationSize = mem_req.size,
			.memoryTypeIndex = device.host_mem_cached_index,
		};
		VK_CALL_RET(vkAllocateMemory(vulkan_dev, &alloc_info, nullptr, &mapping.mem), "map buffer allocation failed", nullptr)
		VK_CALL_RET(vkBindBufferMemory(vulkan_dev, mapping.buffer, mapping.mem, 0), "map buffer allocation binding failed", nullptr)
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
					const VkBufferCopy region {
						.srcOffset = mapping.offset,
						.dstOffset = 0,
						.size = mapping.size,
					};
					vkCmdCopyBuffer(block_cmd_buffer.cmd_buffer, (VkBuffer)*object, mapping.buffer, 1, &region);
				} else {
					image_copy_dev_to_host(cqueue, block_cmd_buffer.cmd_buffer, mapping.buffer);
				}
			}), blocking_map);
		} else {
			// TODO: make this actually work
			VK_CMD_BLOCK(vk_queue, "dev -> host memory barrier", ({
				const VkBufferMemoryBarrier2 buffer_barrier {
					.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2,
					.pNext = nullptr,
					.srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
					.srcAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT,
					.dstStageMask = VK_PIPELINE_STAGE_2_HOST_BIT,
					.dstAccessMask = VkAccessFlags2(VK_ACCESS_2_HOST_READ_BIT | (does_write ? VK_ACCESS_2_HOST_WRITE_BIT : VkAccessFlagBits2(0u))),
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
	VK_CALL_RET(vkMapMemory(vulkan_dev, mapping.mem, host_buffer_offset, size, 0, &host_ptr), "failed to map host buffer", nullptr)
	
	// need to remember how much we mapped and where (so the host -> device write-back copies the right amount of bytes)
	mappings.emplace(host_ptr, mapping);
	
	return host_ptr;
}

bool vulkan_memory::unmap(const compute_queue& cqueue, void* __attribute__((aligned(128))) mapped_ptr) {
	if(*object == 0) return false;
	if(mapped_ptr == nullptr) return false;
	
	const auto& vk_queue = (const vulkan_queue&)cqueue;
	auto vulkan_dev = device.device;
	
	// check if this is actually a mapped pointer (+get the mapped size)
	const auto iter = mappings.find(mapped_ptr);
	if(iter == mappings.end()) {
		log_error("invalid mapped pointer: $X", mapped_ptr);
		return false;
	}
	
	const auto is_host_coherent = has_flag<COMPUTE_MEMORY_FLAG::VULKAN_HOST_COHERENT>(memory_flags);
	
	// check if we need to actually copy data back to the device (not the case if read-only mapping)
	if(has_flag<COMPUTE_MEMORY_MAP_FLAG::WRITE>(iter->second.flags) ||
	   has_flag<COMPUTE_MEMORY_MAP_FLAG::WRITE_INVALIDATE>(iter->second.flags)) {
		if (!is_host_coherent || is_image) {
			do {
				// host -> device copy
				// TODO: sync ...
				VK_CMD_BLOCK(vk_queue, "host -> dev memory copy", ({
					if(!is_image) {
						const VkBufferCopy region {
							.srcOffset = 0,
							.dstOffset = iter->second.offset,
							.size = iter->second.size,
						};
						vkCmdCopyBuffer(block_cmd_buffer.cmd_buffer, iter->second.buffer, (VkBuffer)*object, 1, &region);
					} else {
						image_copy_host_to_dev(cqueue, block_cmd_buffer.cmd_buffer, iter->second.buffer, mapped_ptr);
					}
				}), has_flag<COMPUTE_MEMORY_MAP_FLAG::BLOCK>(iter->second.flags));
			} while(false);
		}
	}
	
	// unmap
	// TODO/NOTE: we can only unmap the whole buffer with vulkan, not individual mappings ...
	// -> can only unmap if this is the last mapping (if is_host_coherent, if the buffer was just created/allocated for this, then it doesn't matter)
	// also: TODO: SYNC!
	vkUnmapMemory(vulkan_dev, iter->second.mem);
	
	// barrier after unmap when using unified memory
	// TODO: make this actually work
	if (has_flag<COMPUTE_MEMORY_MAP_FLAG::WRITE>(iter->second.flags) ||
		has_flag<COMPUTE_MEMORY_MAP_FLAG::WRITE_INVALIDATE>(iter->second.flags)) {
		if (is_host_coherent && !is_image) {
			VK_CMD_BLOCK(vk_queue, "host -> dev memory barrier", ({
				const VkBufferMemoryBarrier2 buffer_barrier {
					.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2,
					.pNext = nullptr,
					.srcStageMask = VK_PIPELINE_STAGE_2_HOST_BIT,
					.srcAccessMask = VkAccessFlags2(VK_ACCESS_2_HOST_WRITE_BIT |
													(has_flag<COMPUTE_MEMORY_MAP_FLAG::READ>(iter->second.flags) ?
													 VK_ACCESS_2_HOST_READ_BIT : VkAccessFlagBits2(0u))),
					.dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
					.dstAccessMask = (VK_ACCESS_2_MEMORY_READ_BIT |
									  VK_ACCESS_2_MEMORY_WRITE_BIT |
									  VK_ACCESS_2_SHADER_READ_BIT |
									  VK_ACCESS_2_SHADER_WRITE_BIT),
					.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
					.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
					.buffer = iter->second.buffer,
					.offset = iter->second.offset,
					.size = iter->second.size,
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
			}), has_flag<COMPUTE_MEMORY_MAP_FLAG::BLOCK>(iter->second.flags));
		}
	}
	
	// delete host buffer
	if (!is_host_coherent || is_image) {
		if(iter->second.buffer != nullptr) {
			vkDestroyBuffer(vulkan_dev, iter->second.buffer, nullptr);
		}
		if(iter->second.mem != nullptr) {
			vkFreeMemory(vulkan_dev, iter->second.mem, nullptr);
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
	const auto find_index = [](const unordered_set<uint32_t>& indices,
							   const uint32_t& preferred_index,
							   const uint32_t& type_bits) -> pair<bool, uint32_t> {
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
	
	pair<bool, uint32_t> ret { false, 0 };
	
	// if device memory is wanted or required, try this first
	if(want_device_memory || requires_device_memory) {
		// select between device-only and device+host-coherent memory
		ret = (!requires_host_coherent ?
			   find_index(device.device_mem_indices, device.device_mem_index, memory_type_bits) :
			   find_index(device.device_mem_host_coherent_indices, device.device_mem_host_coherent_index, memory_type_bits));
		if(ret.first) {
			return ret.second;
		}
		if (requires_device_memory) {
			log_error("could not find device-local memory");
			return 0;
		}
	}
	
	if (requires_host_coherent) {
		ret = find_index(device.device_mem_host_coherent_indices, device.device_mem_host_coherent_index, memory_type_bits);
		if(ret.first) return ret.second;
	} else {
		// check cached first, then coherent
		ret = find_index(device.host_mem_cached_indices, device.host_mem_cached_index, memory_type_bits);
		if(ret.first) return ret.second;
		ret = find_index(device.device_mem_host_coherent_indices, device.device_mem_host_coherent_index, memory_type_bits);
		if(ret.first) return ret.second;
	}
	
	// no memory found for this
	log_error("could not find a memory type index for the request memory type bits $X (device memory wanted: $)",
			  memory_type_bits, want_device_memory);
	return 0;
}

#endif
