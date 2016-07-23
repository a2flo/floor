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

#include <floor/compute/vulkan/vulkan_buffer.hpp>

#if !defined(FLOOR_NO_VULKAN)

#include <floor/core/logger.hpp>
#include <floor/compute/vulkan/vulkan_queue.hpp>
#include <floor/compute/vulkan/vulkan_device.hpp>
#include <floor/compute/vulkan/vulkan_compute.hpp>

// TODO: proper error (return) value handling everywhere

vulkan_buffer::vulkan_buffer(const vulkan_device* device,
							 const size_t& size_,
							 void* host_ptr_,
							 const COMPUTE_MEMORY_FLAG flags_,
							 const uint32_t opengl_type_,
							 const uint32_t external_gl_object_) :
compute_buffer(device, size_, host_ptr_, flags_, opengl_type_, external_gl_object_) {
	if(size < min_multiple()) return;
	
	// TODO: handle the remaining flags + host ptr
	if(host_ptr_ != nullptr && !has_flag<COMPUTE_MEMORY_FLAG::NO_INITIAL_COPY>(flags)) {
		// TODO: flag?
	}
	
	// actually create the buffer
	if(!create_internal(true, nullptr)) {
		return; // can't do much else
	}
}

bool vulkan_buffer::create_internal(const bool copy_host_data, shared_ptr<compute_queue> cqueue) {
	auto vulkan_dev = ((const vulkan_device*)dev)->device;
	
	// create the buffer
	const VkBufferCreateInfo buffer_create_info {
		.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
		.pNext = nullptr,
		.flags = 0, // no sparse backing
		.size = size,
		// TODO: will want to use other things here as well, later on (ubo, vbo, ...)
		.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
		// TODO: probably want a concurrent option later on
		.sharingMode = VK_SHARING_MODE_EXCLUSIVE,
		.queueFamilyIndexCount = 0,
		.pQueueFamilyIndices = nullptr,
	};
	VK_CALL_RET(vkCreateBuffer(vulkan_dev, &buffer_create_info, nullptr, &buffer), "buffer creation failed", false);
	
	// allocate / back it up
	VkMemoryRequirements mem_req;
	vkGetBufferMemoryRequirements(vulkan_dev, buffer, &mem_req);
	
	// TODO: handle host-memory backing
	const VkMemoryAllocateInfo alloc_info {
		.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
		.pNext = nullptr,
		.allocationSize = mem_req.size,
		// TODO/NOTE: always use device memory for now
		.memoryTypeIndex = ((const vulkan_device*)dev)->device_mem_index,
	};
	VK_CALL_RET(vkAllocateMemory(vulkan_dev, &alloc_info, nullptr, &mem), "buffer allocation failed", false);
	VK_CALL_RET(vkBindBufferMemory(vulkan_dev, buffer, mem, 0), "buffer allocation binding failed", false);
	
	// update buffer desc info
	buffer_info.buffer = buffer;
	buffer_info.offset = 0;
	buffer_info.range = size;
	
	// buffer init from host data pointer
	if(copy_host_data &&
	   host_ptr != nullptr &&
	   !has_flag<COMPUTE_MEMORY_FLAG::NO_INITIAL_COPY>(flags)) {
		// we definitively need a queue for this (use specified one if possible, otherwise use the default queue)
		auto map_queue = (cqueue != nullptr ? cqueue : ((const vulkan_compute*)dev->context)->get_device_default_queue(dev));
		auto mapped_ptr = map(map_queue, COMPUTE_MEMORY_MAP_FLAG::WRITE_INVALIDATE | COMPUTE_MEMORY_MAP_FLAG::BLOCK, size, 0);
		if(mapped_ptr != nullptr) {
			memcpy(mapped_ptr, host_ptr, size);
			unmap(map_queue, mapped_ptr);
		}
		else {
			log_error("failed to initialize buffer with host data (map failed)");
			return false;
		}
	}
	
	return true;
}

vulkan_buffer::~vulkan_buffer() {
	if(buffer != nullptr) {
		vkDestroyBuffer(((const vulkan_device*)dev)->device, buffer, nullptr);
	}
	// TODO: destroy all the rest
}

void vulkan_buffer::read(shared_ptr<compute_queue> cqueue, const size_t size_, const size_t offset) {
	read(cqueue, host_ptr, size_, offset);
}

void vulkan_buffer::read(shared_ptr<compute_queue> cqueue, void* dst, const size_t size_, const size_t offset) {
	if(buffer == nullptr) return;
	
	// TODO: implement this
}

void vulkan_buffer::write(shared_ptr<compute_queue> cqueue, const size_t size_, const size_t offset) {
	write(cqueue, host_ptr, size_, offset);
}

void vulkan_buffer::write(shared_ptr<compute_queue> cqueue, const void* src, const size_t size_, const size_t offset) {
	if(buffer == nullptr) return;
	
	// TODO: implement this
}

void vulkan_buffer::copy(shared_ptr<compute_queue> cqueue,
						 shared_ptr<compute_buffer> src,
						 const size_t size_, const size_t src_offset, const size_t dst_offset) {
	if(buffer == nullptr) return;
	
	// TODO: implement this
}

void vulkan_buffer::fill(shared_ptr<compute_queue> cqueue,
						 const void* pattern, const size_t& pattern_size,
						 const size_t size_, const size_t offset) {
	if(buffer == nullptr) return;
	
	// TODO: implement this
}

void vulkan_buffer::zero(shared_ptr<compute_queue> cqueue) {
	if(buffer == nullptr) return;
	
	// TODO: implement this
}

bool vulkan_buffer::resize(shared_ptr<compute_queue> cqueue, const size_t& new_size_,
						   const bool copy_old_data, const bool copy_host_data,
						   void* new_host_ptr) {
	// TODO: implement this
	return false;
}

void* __attribute__((aligned(128))) vulkan_buffer::map(shared_ptr<compute_queue> cqueue,
													   const COMPUTE_MEMORY_MAP_FLAG flags_,
													   const size_t size_, const size_t offset) {
	if(buffer == nullptr) return nullptr;
	
	const size_t map_size = (size_ == 0 ? size : size_);
	const bool blocking_map = has_flag<COMPUTE_MEMORY_MAP_FLAG::BLOCK>(flags_);
	if(!map_check(size, map_size, flags, flags_, offset)) return nullptr;
	
	bool write_only = false;
	if(has_flag<COMPUTE_MEMORY_MAP_FLAG::WRITE_INVALIDATE>(flags_)) {
		write_only = true;
	}
	else {
		switch(flags_ & COMPUTE_MEMORY_MAP_FLAG::READ_WRITE) {
			case COMPUTE_MEMORY_MAP_FLAG::READ:
				write_only = false;
				break;
			case COMPUTE_MEMORY_MAP_FLAG::WRITE:
				write_only = true;
				break;
			case COMPUTE_MEMORY_MAP_FLAG::READ_WRITE:
				write_only = false;
				break;
			case COMPUTE_MEMORY_MAP_FLAG::NONE:
			default:
				log_error("neither read nor write flag set for buffer mapping!");
				return nullptr;
		}
	}
	
	// here is the deal with vulkan device memory:
	//  * we always allocate device local memory, regardless of any host-visibility
	//  * if the device local memory is not host-visible, we need to allocate an appropriately sized
	//    host-visible buffer, then: a) for read: create a device -> host buffer copy (in here)
	//                               b) for write: create a host buffer -> device buffer copy (in unmap)
	//  * if the device local memory is host-visible, we can just call vulkan map/unmap functions
	
	// create the host-visible buffer if necessary
	vulkan_mapping mapping {
		.buffer = buffer,
		.mem = mem,
		.size = map_size,
		.offset = offset,
		.flags = flags_,
	};
	size_t host_buffer_offset = offset;
	auto vulkan_dev = ((const vulkan_device*)dev)->device;
	// TODO: make sure this is cleaned up properly on failure
	if(!dev->unified_memory) { // TODO: or already created host-visible (-> create_internal)
		// we're only creating a buffer that is large enough + start from the offset
		host_buffer_offset = 0;
		
		// create the host-visible buffer
		const VkBufferCreateInfo buffer_create_info {
			.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
			.pNext = nullptr,
			.flags = 0,
			.size = size,
			.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
			.sharingMode = VK_SHARING_MODE_EXCLUSIVE,
			.queueFamilyIndexCount = 0,
			.pQueueFamilyIndices = nullptr,
		};
		VK_CALL_RET(vkCreateBuffer(vulkan_dev, &buffer_create_info, nullptr, &mapping.buffer), "map buffer creation failed", nullptr);
	
		// allocate / back it up
		VkMemoryRequirements mem_req;
		vkGetBufferMemoryRequirements(vulkan_dev, buffer, &mem_req);
	
		const VkMemoryAllocateInfo alloc_info {
			.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
			.pNext = nullptr,
			.allocationSize = mem_req.size,
			.memoryTypeIndex = ((const vulkan_device*)dev)->host_mem_cached_index,
		};
		VK_CALL_RET(vkAllocateMemory(vulkan_dev, &alloc_info, nullptr, &mapping.mem), "map buffer allocation failed", nullptr);
		VK_CALL_RET(vkBindBufferMemory(vulkan_dev, mapping.buffer, mapping.mem, 0), "map buffer allocation binding failed", nullptr);
	}
	
	// check if we need to copy the buffer from the device (in case READ was specified)
	if(!write_only) {
		if(blocking_map) {
			// must finish up all current work before we can properly read from the current buffer
			cqueue->finish();
		}
		
		// device -> host buffer copy
		if(!dev->unified_memory) {
			auto cmd_buffer = ((vulkan_queue*)cqueue.get())->make_command_buffer(); // TODO: should probably abstract this a little
			const VkCommandBufferBeginInfo begin_info {
				.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
				.pNext = nullptr,
				.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
				.pInheritanceInfo = nullptr,
			};
			VK_CALL_RET(vkBeginCommandBuffer(cmd_buffer.cmd_buffer, &begin_info),
						"failed to begin command buffer", nullptr);
	
			const VkBufferCopy region {
				.srcOffset = mapping.offset,
				.dstOffset = 0,
				.size = mapping.size,
			};
			vkCmdCopyBuffer(cmd_buffer.cmd_buffer, buffer, mapping.buffer, 1, &region);
	
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
	VK_CALL_RET(vkMapMemory(vulkan_dev, mapping.mem, host_buffer_offset, map_size, 0, &host_ptr), "failed to map host buffer", nullptr);
	
	// need to remember how much we mapped and where (so the host -> device write-back copies the right amount of bytes)
	mappings.emplace(host_ptr, mapping);
	
	return host_ptr;
}

void vulkan_buffer::unmap(shared_ptr<compute_queue> cqueue, void* __attribute__((aligned(128))) mapped_ptr) {
	if(buffer == nullptr) return;
	if(mapped_ptr == nullptr) return;
	
	auto vulkan_dev = ((const vulkan_device*)dev)->device;
	
	// check if this is actually a mapped pointer (+get the mapped size)
	const auto iter = mappings.find(mapped_ptr);
	if(iter == mappings.end()) {
		log_error("invalid mapped pointer: %X", mapped_ptr);
		return;
	}
	
	// TODO: again, make sure this is cleaned up properly on failure
	
	// check if we need to actually copy data back to the device (not the case if read-only mapping)
	if(has_flag<COMPUTE_MEMORY_MAP_FLAG::WRITE>(iter->second.flags) ||
	   has_flag<COMPUTE_MEMORY_MAP_FLAG::WRITE_INVALIDATE>(iter->second.flags)) {
		if(!dev->unified_memory) {
			// host -> device copy
			// TODO: sync ...
			auto cmd_buffer = ((vulkan_queue*)cqueue.get())->make_command_buffer(); // TODO: should probably abstract this a little
			const VkCommandBufferBeginInfo begin_info {
				.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
				.pNext = nullptr,
				.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
				.pInheritanceInfo = nullptr,
			};
			VK_CALL_RET(vkBeginCommandBuffer(cmd_buffer.cmd_buffer, &begin_info),
						"failed to begin command buffer");
		
			const VkBufferCopy region {
				.srcOffset = 0,
				.dstOffset = iter->second.offset,
				.size = iter->second.size,
			};
			vkCmdCopyBuffer(cmd_buffer.cmd_buffer, iter->second.buffer, buffer, 1, &region);
		
			VK_CALL_RET(vkEndCommandBuffer(cmd_buffer.cmd_buffer), "failed to end command buffer");
			((vulkan_queue*)cqueue.get())->submit_command_buffer(cmd_buffer, has_flag<COMPUTE_MEMORY_MAP_FLAG::BLOCK>(iter->second.flags));
		}
		else {
			// TODO: flush?
		}
	}
	
	// unmap
	// TODO/NOTE: we can only unmap the whole buffer with vulkan, not individual mappings ... -> can only unmap if this is the last mapping (if unified_memory, if the buffer was just created/allocated for this, then it doesn't matter)
	// also: TODO: SYNC!
	vkUnmapMemory(vulkan_dev, iter->second.mem);
	
	// TODO: delete host buffer
	
	// remove the mapping
	mappings.erase(mapped_ptr);
}

bool vulkan_buffer::acquire_opengl_object(shared_ptr<compute_queue> cqueue) {
	log_error("not supported by vulkan");
	return false;
}

bool vulkan_buffer::release_opengl_object(shared_ptr<compute_queue> cqueue) {
	log_error("not supported by vulkan");
	return false;
}

#endif
