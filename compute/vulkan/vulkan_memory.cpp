/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2019 Florian Ziesche
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

vulkan_memory::vulkan_memory(const vulkan_device* device_, const uint64_t* object_, const bool is_image_) noexcept :
device(device_), object(object_), is_image(is_image_) {
}

vulkan_memory::~vulkan_memory() noexcept {
	if(mem != nullptr) {
		vkFreeMemory(device->device, mem, nullptr);
		mem = nullptr;
	}
}

shared_ptr<compute_queue> vulkan_memory::queue_or_default_compute_queue(shared_ptr<compute_queue> cqueue) const {
	if(cqueue != nullptr) return cqueue;
	return ((vulkan_compute*)device->context)->get_device_default_queue(device);
}

bool vulkan_memory::write_memory_data(shared_ptr<compute_queue> cqueue, const void* data, const size_t& size, const size_t& offset,
									  const size_t non_shim_input_size, const char* error_msg_on_failure) {
	// we definitively need a queue for this (use specified one if possible, otherwise use the default queue)
	auto map_queue = queue_or_default_compute_queue(cqueue);
	auto mapped_ptr = map(map_queue, COMPUTE_MEMORY_MAP_FLAG::WRITE_INVALIDATE | COMPUTE_MEMORY_MAP_FLAG::BLOCK, size, offset);
	if(mapped_ptr != nullptr) {
		memcpy(mapped_ptr, data, (non_shim_input_size == 0 ? size : non_shim_input_size));
		unmap(map_queue, mapped_ptr);
	}
	else {
		if(error_msg_on_failure == nullptr) {
			log_error("failed to write vulkan memory data (map failed)");
		}
		else log_error("%s", error_msg_on_failure);
		return false;
	}
	return true;
}

bool vulkan_memory::read_memory_data(shared_ptr<compute_queue> cqueue, void* data, const size_t& size, const size_t& offset,
									 const size_t non_shim_input_size, const char* error_msg_on_failure) {
	auto map_queue = queue_or_default_compute_queue(cqueue);
	auto mapped_ptr = map(map_queue, COMPUTE_MEMORY_MAP_FLAG::READ | COMPUTE_MEMORY_MAP_FLAG::BLOCK, size, offset);
	if(mapped_ptr != nullptr) {
		memcpy(data, mapped_ptr, (non_shim_input_size == 0 ? size : non_shim_input_size));
		unmap(map_queue, mapped_ptr);
	}
	else {
		if(error_msg_on_failure == nullptr) {
			log_error("failed to read vulkan memory data (map failed)");
		}
		else log_error("%s", error_msg_on_failure);
		return false;
	}
	return true;
}

void* __attribute__((aligned(128))) vulkan_memory::map(shared_ptr<compute_queue> cqueue,
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
	//  * if the device local memory is host-visible, we can just call vulkan map/unmap functions
	
	// create the host-visible buffer if necessary
	vulkan_mapping mapping {
		.buffer = nullptr,
		.mem = nullptr,
		.size = size,
		.offset = offset,
		.flags = flags,
	};
	size_t host_buffer_offset = offset;
	auto vulkan_dev = device->device;
	// TODO: make sure this is cleaned up properly on failure
	if(!device->unified_memory || is_image) { // TODO: or already created host-visible (-> create_internal)
		// we're only creating a buffer that is large enough + start from the offset
		host_buffer_offset = 0;
		
		// create the host-visible buffer
		const VkBufferCreateInfo buffer_create_info {
			.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
			.pNext = nullptr,
			.flags = 0,
			.size = size,
			.usage = ((does_write ? VK_BUFFER_USAGE_TRANSFER_SRC_BIT : 0) |
					  (does_read ? VK_BUFFER_USAGE_TRANSFER_DST_BIT : 0)),
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
			.memoryTypeIndex = device->host_mem_cached_index,
		};
		VK_CALL_RET(vkAllocateMemory(vulkan_dev, &alloc_info, nullptr, &mapping.mem), "map buffer allocation failed", nullptr)
		VK_CALL_RET(vkBindBufferMemory(vulkan_dev, mapping.buffer, mapping.mem, 0), "map buffer allocation binding failed", nullptr)
	}
	else {
		mapping.buffer = (VkBuffer)*object;
		mapping.mem = mem;
	}
	
	// check if we need to copy the buffer from the device (in case READ was specified)
	if(!write_only) {
		if(blocking_map) {
			// must finish up all current work before we can properly read from the current buffer
			cqueue->finish();
		}
		
		// device -> host buffer copy
		if(!device->unified_memory || is_image) {
			auto cmd_buffer = ((vulkan_queue*)cqueue.get())->make_command_buffer("dev -> host memory copy"); // TODO: should probably abstract this a little
			const VkCommandBufferBeginInfo begin_info {
				.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
				.pNext = nullptr,
				.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
				.pInheritanceInfo = nullptr,
			};
			VK_CALL_RET(vkBeginCommandBuffer(cmd_buffer.cmd_buffer, &begin_info),
						"failed to begin command buffer", nullptr)
			
			if(!is_image) {
				const VkBufferCopy region {
					.srcOffset = mapping.offset,
					.dstOffset = 0,
					.size = mapping.size,
				};
				vkCmdCopyBuffer(cmd_buffer.cmd_buffer, (VkBuffer)*object, mapping.buffer, 1, &region);
			}
			else {
				image_copy_dev_to_host(cmd_buffer.cmd_buffer, mapping.buffer);
			}
	
			VK_CALL_RET(vkEndCommandBuffer(cmd_buffer.cmd_buffer), "failed to end command buffer", nullptr)
			((vulkan_queue*)cqueue.get())->submit_command_buffer(cmd_buffer, blocking_map);
		}
		else {
			// TODO: make this actually work
			auto cmd_buffer = ((vulkan_queue*)cqueue.get())->make_command_buffer("dev -> host memory barrier");
			const VkCommandBufferBeginInfo begin_info {
				.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
				.pNext = nullptr,
				.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
				.pInheritanceInfo = nullptr,
			};
			VK_CALL_RET(vkBeginCommandBuffer(cmd_buffer.cmd_buffer, &begin_info),
						"failed to begin command buffer", nullptr)
			
			const VkBufferMemoryBarrier buffer_barrier {
				.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
				.pNext = nullptr,
				.srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT,
				.dstAccessMask = VK_ACCESS_HOST_READ_BIT | (does_write ? VK_ACCESS_HOST_WRITE_BIT : 0),
				.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
				.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
				.buffer = mapping.buffer,
				.offset = mapping.offset,
				.size = mapping.size,
			};
			
			vkCmdPipelineBarrier(cmd_buffer.cmd_buffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_HOST_BIT,
								 0, 0, nullptr, 1, &buffer_barrier, 0, nullptr);
			
			VK_CALL_RET(vkEndCommandBuffer(cmd_buffer.cmd_buffer), "failed to end command buffer", nullptr)
			((vulkan_queue*)cqueue.get())->submit_command_buffer(cmd_buffer, blocking_map);
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

void vulkan_memory::unmap(shared_ptr<compute_queue> cqueue, void* __attribute__((aligned(128))) mapped_ptr) {
	if(*object == 0) return;
	if(mapped_ptr == nullptr) return;
	
	auto vulkan_dev = device->device;
	
	// check if this is actually a mapped pointer (+get the mapped size)
	const auto iter = mappings.find(mapped_ptr);
	if(iter == mappings.end()) {
		log_error("invalid mapped pointer: %X", mapped_ptr);
		return;
	}
	
	// check if we need to actually copy data back to the device (not the case if read-only mapping)
	if(has_flag<COMPUTE_MEMORY_MAP_FLAG::WRITE>(iter->second.flags) ||
	   has_flag<COMPUTE_MEMORY_MAP_FLAG::WRITE_INVALIDATE>(iter->second.flags)) {
		if(!device->unified_memory || is_image) {
			do {
				// host -> device copy
				// TODO: sync ...
				auto cmd_buffer = ((vulkan_queue*)cqueue.get())->make_command_buffer("host -> dev memory copy"); // TODO: should probably abstract this a little
				const VkCommandBufferBeginInfo begin_info {
					.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
					.pNext = nullptr,
					.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
					.pInheritanceInfo = nullptr,
				};
				VK_CALL_BREAK(vkBeginCommandBuffer(cmd_buffer.cmd_buffer, &begin_info),
							  "failed to begin command buffer")
				
				if(!is_image) {
					const VkBufferCopy region {
						.srcOffset = 0,
						.dstOffset = iter->second.offset,
						.size = iter->second.size,
					};
					vkCmdCopyBuffer(cmd_buffer.cmd_buffer, iter->second.buffer, (VkBuffer)*object, 1, &region);
				}
				else {
					image_copy_host_to_dev(cmd_buffer.cmd_buffer, iter->second.buffer, mapped_ptr);
				}
				
				VK_CALL_BREAK(vkEndCommandBuffer(cmd_buffer.cmd_buffer), "failed to end command buffer")
				((vulkan_queue*)cqueue.get())->submit_command_buffer(cmd_buffer, has_flag<COMPUTE_MEMORY_MAP_FLAG::BLOCK>(iter->second.flags));
			} while(false);
		}
	}
	
	// unmap
	// TODO/NOTE: we can only unmap the whole buffer with vulkan, not individual mappings ... -> can only unmap if this is the last mapping (if unified_memory, if the buffer was just created/allocated for this, then it doesn't matter)
	// also: TODO: SYNC!
	vkUnmapMemory(vulkan_dev, iter->second.mem);
	
	// barrier after unmap when using unified memory
	// TODO: make this actually work
	if(has_flag<COMPUTE_MEMORY_MAP_FLAG::WRITE>(iter->second.flags) ||
	   has_flag<COMPUTE_MEMORY_MAP_FLAG::WRITE_INVALIDATE>(iter->second.flags)) {
		if(device->unified_memory && !is_image) {
			auto cmd_buffer = ((vulkan_queue*)cqueue.get())->make_command_buffer("host -> dev memory barrier");
			const VkCommandBufferBeginInfo begin_info {
				.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
				.pNext = nullptr,
				.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
				.pInheritanceInfo = nullptr,
			};
			VK_CALL_RET(vkBeginCommandBuffer(cmd_buffer.cmd_buffer, &begin_info),
						"failed to begin command buffer")
			
			const VkBufferMemoryBarrier buffer_barrier {
				.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
				.pNext = nullptr,
				.srcAccessMask = (VK_ACCESS_HOST_WRITE_BIT |
								  (has_flag<COMPUTE_MEMORY_MAP_FLAG::READ>(iter->second.flags) ? VK_ACCESS_HOST_READ_BIT : 0)),
				.dstAccessMask = (VK_ACCESS_MEMORY_READ_BIT |
								  VK_ACCESS_MEMORY_WRITE_BIT |
								  VK_ACCESS_SHADER_READ_BIT |
								  VK_ACCESS_SHADER_WRITE_BIT),
				.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
				.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
				.buffer = iter->second.buffer,
				.offset = iter->second.offset,
				.size = iter->second.size,
			};
			
			vkCmdPipelineBarrier(cmd_buffer.cmd_buffer, VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
								 0, 0, nullptr, 1, &buffer_barrier, 0, nullptr);
			
			VK_CALL_RET(vkEndCommandBuffer(cmd_buffer.cmd_buffer), "failed to end command buffer")
			((vulkan_queue*)cqueue.get())->submit_command_buffer(cmd_buffer, has_flag<COMPUTE_MEMORY_MAP_FLAG::BLOCK>(iter->second.flags));
		}
	}
	
	// delete host buffer
	if(!device->unified_memory || is_image) {
		if(iter->second.buffer != nullptr) {
			vkDestroyBuffer(vulkan_dev, iter->second.buffer, nullptr);
		}
		if(iter->second.mem != nullptr) {
			vkFreeMemory(vulkan_dev, iter->second.mem, nullptr);
		}
	}
	
	// remove the mapping
	mappings.erase(mapped_ptr);
}

uint32_t vulkan_memory::find_memory_type_index(const uint32_t memory_type_bits,
											   const bool want_device_memory) const {
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
	
	// if device memory is wanted, try this first
	if(want_device_memory) {
		const auto ret = find_index(device->device_mem_indices, device->device_mem_index, memory_type_bits);
		if(ret.first) return ret.second;
	}
	
	// check cached first, then uncached
	auto ret = find_index(device->host_mem_cached_indices, device->host_mem_cached_index, memory_type_bits);
	if(ret.first) return ret.second;
	
	// check cached first, then uncached
	ret = find_index(device->host_mem_uncached_indices, device->host_mem_uncached_index, memory_type_bits);
	if(ret.first) return ret.second;
	
	// no memory found for this
	log_error("could not find a memory type index for the request memory type bits %X (device memory wanted: %v)",
			  memory_type_bits, want_device_memory);
	return 0;
}

#endif
