/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2016 Florian Ziesche
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

vulkan_memory::vulkan_memory(const vulkan_device* device_, const void** object_, const bool is_image_) noexcept :
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

bool vulkan_memory::write_memory_data(shared_ptr<compute_queue> cqueue, void* data, const size_t& size, const size_t& offset,
									  const char* error_msg_on_failure) {
	// we definitively need a queue for this (use specified one if possible, otherwise use the default queue)
	auto map_queue = queue_or_default_compute_queue(cqueue);
	auto mapped_ptr = map(map_queue, COMPUTE_MEMORY_MAP_FLAG::WRITE_INVALIDATE | COMPUTE_MEMORY_MAP_FLAG::BLOCK, size, offset);
	if(mapped_ptr != nullptr) {
		memcpy(mapped_ptr, data, size);
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


void* __attribute__((aligned(128))) vulkan_memory::map(shared_ptr<compute_queue> cqueue,
													   const COMPUTE_MEMORY_MAP_FLAG flags,
													   const size_t size, const size_t offset) {
	if(*object == nullptr) return nullptr;
	
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
		VK_CALL_RET(vkCreateBuffer(vulkan_dev, &buffer_create_info, nullptr, &mapping.buffer), "map buffer creation failed", nullptr);
	
		// allocate / back it up
		VkMemoryRequirements mem_req;
		vkGetBufferMemoryRequirements(vulkan_dev, mapping.buffer, &mem_req);
	
		const VkMemoryAllocateInfo alloc_info {
			.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
			.pNext = nullptr,
			.allocationSize = mem_req.size,
			.memoryTypeIndex = device->host_mem_cached_index,
		};
		VK_CALL_RET(vkAllocateMemory(vulkan_dev, &alloc_info, nullptr, &mapping.mem), "map buffer allocation failed", nullptr);
		VK_CALL_RET(vkBindBufferMemory(vulkan_dev, mapping.buffer, mapping.mem, 0), "map buffer allocation binding failed", nullptr);
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
						"failed to begin command buffer", nullptr);
			
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
	
			VK_CALL_RET(vkEndCommandBuffer(cmd_buffer.cmd_buffer), "failed to end command buffer", nullptr);
			((vulkan_queue*)cqueue.get())->submit_command_buffer(cmd_buffer, blocking_map);
		}
		else {
			// TODO
		}
	}
	
	// TODO: use vkFlushMappedMemoryRanges/vkInvalidateMappedMemoryRanges if possible
	
	// map the host buffer
	void* __attribute__((aligned(128))) host_ptr { nullptr };
	VK_CALL_RET(vkMapMemory(vulkan_dev, mapping.mem, host_buffer_offset, size, 0, &host_ptr), "failed to map host buffer", nullptr);
	
	// need to remember how much we mapped and where (so the host -> device write-back copies the right amount of bytes)
	mappings.emplace(host_ptr, mapping);
	
	return host_ptr;
}

void vulkan_memory::unmap(shared_ptr<compute_queue> cqueue, void* __attribute__((aligned(128))) mapped_ptr) {
	if(*object == nullptr) return;
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
							  "failed to begin command buffer");
				
				if(!is_image) {
					const VkBufferCopy region {
						.srcOffset = 0,
						.dstOffset = iter->second.offset,
						.size = iter->second.size,
					};
					vkCmdCopyBuffer(cmd_buffer.cmd_buffer, iter->second.buffer, (VkBuffer)*object, 1, &region);
				}
				else {
					image_copy_host_to_dev(cmd_buffer.cmd_buffer, iter->second.buffer);
				}
				
				VK_CALL_BREAK(vkEndCommandBuffer(cmd_buffer.cmd_buffer), "failed to end command buffer");
				((vulkan_queue*)cqueue.get())->submit_command_buffer(cmd_buffer, has_flag<COMPUTE_MEMORY_MAP_FLAG::BLOCK>(iter->second.flags));
			} while(false);
		}
		else {
			// TODO: flush?
		}
	}
	
	// unmap
	// TODO/NOTE: we can only unmap the whole buffer with vulkan, not individual mappings ... -> can only unmap if this is the last mapping (if unified_memory, if the buffer was just created/allocated for this, then it doesn't matter)
	// also: TODO: SYNC!
	vkUnmapMemory(vulkan_dev, iter->second.mem);
	
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

#endif
