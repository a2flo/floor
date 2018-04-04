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

#include <floor/compute/vulkan/vulkan_buffer.hpp>

#if !defined(FLOOR_NO_VULKAN)

#include <floor/core/logger.hpp>
#include <floor/compute/vulkan/vulkan_queue.hpp>
#include <floor/compute/vulkan/vulkan_device.hpp>
#include <floor/compute/vulkan/vulkan_compute.hpp>

// TODO: proper error (return) value handling everywhere

vulkan_buffer::vulkan_buffer(vulkan_device* device,
							 const size_t& size_,
							 void* host_ptr_,
							 const COMPUTE_MEMORY_FLAG flags_,
							 const uint32_t opengl_type_,
							 const uint32_t external_gl_object_) :
compute_buffer(device, size_, host_ptr_, flags_, opengl_type_, external_gl_object_),
vulkan_memory(device, &buffer) {
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
		// set all the bits here, might need some better restrictions later on
		// NOTE: not setting vertex bit here, b/c we're always using SSBOs
		.usage = (VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
				  VK_BUFFER_USAGE_TRANSFER_DST_BIT |
				  VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
				  VK_BUFFER_USAGE_INDEX_BUFFER_BIT |
				  VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT),
		// TODO: probably want a concurrent option later on
		.sharingMode = VK_SHARING_MODE_EXCLUSIVE,
		.queueFamilyIndexCount = 0,
		.pQueueFamilyIndices = nullptr,
	};
	VK_CALL_RET(vkCreateBuffer(vulkan_dev, &buffer_create_info, nullptr, &buffer),
				"buffer creation failed", false);
	
	// allocate / back it up
	VkMemoryRequirements mem_req;
	vkGetBufferMemoryRequirements(vulkan_dev, buffer, &mem_req);
	
	const VkMemoryAllocateInfo alloc_info {
		.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
		.pNext = nullptr,
		.allocationSize = mem_req.size,
		.memoryTypeIndex = find_memory_type_index(mem_req.memoryTypeBits, true /* prefer device memory */),
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
		if(!write_memory_data(cqueue, host_ptr, size, 0, 0,
							  "failed to initialize buffer with host data (map failed)")) {
			return false;
		}
	}
	
	return true;
}

vulkan_buffer::~vulkan_buffer() {
	if(buffer != nullptr) {
		vkDestroyBuffer(((const vulkan_device*)dev)->device, buffer, nullptr);
		buffer = nullptr;
	}
	buffer_info = { nullptr, 0, 0 };
}

void vulkan_buffer::read(shared_ptr<compute_queue> cqueue, const size_t size_, const size_t offset) {
	read(cqueue, host_ptr, size_, offset);
}

void vulkan_buffer::read(shared_ptr<compute_queue> cqueue, void* dst,
						 const size_t size_, const size_t offset) {
	if(buffer == nullptr) return;
	
	const size_t read_size = (size_ == 0 ? size : size_);
	if(!read_check(size, read_size, offset, flags)) return;
	
	GUARD(lock);
	read_memory_data(cqueue, dst, read_size, offset, 0, "failed to read buffer");
}

void vulkan_buffer::write(shared_ptr<compute_queue> cqueue, const size_t size_, const size_t offset) {
	write(cqueue, host_ptr, size_, offset);
}

void vulkan_buffer::write(shared_ptr<compute_queue> cqueue, const void* src,
						  const size_t size_, const size_t offset) {
	if(buffer == nullptr) return;
	
	const size_t write_size = (size_ == 0 ? size : size_);
	if(!write_check(size, write_size, offset, flags)) return;
	
	GUARD(lock);
	write_memory_data(cqueue, src, write_size, offset, 0, "failed to write buffer");
}

void vulkan_buffer::copy(shared_ptr<compute_queue> cqueue floor_unused,
						 shared_ptr<compute_buffer> src floor_unused,
						 const size_t size_ floor_unused, const size_t src_offset floor_unused, const size_t dst_offset floor_unused) {
	if(buffer == nullptr) return;
	
	// TODO: implement this
}

void vulkan_buffer::fill(shared_ptr<compute_queue> cqueue floor_unused,
						 const void* pattern floor_unused, const size_t& pattern_size floor_unused,
						 const size_t size_ floor_unused, const size_t offset floor_unused) {
	if(buffer == nullptr) return;
	
	// TODO: implement this
}

void vulkan_buffer::zero(shared_ptr<compute_queue> cqueue) {
	if(buffer == nullptr) return;
	
	auto cmd_buffer = ((vulkan_queue*)cqueue.get())->make_command_buffer("buffer zero"); // TODO: abstract
	const VkCommandBufferBeginInfo begin_info {
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
		.pNext = nullptr,
		.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
		.pInheritanceInfo = nullptr,
	};
	VK_CALL_RET(vkBeginCommandBuffer(cmd_buffer.cmd_buffer, &begin_info),
				"failed to begin command buffer");
	
	vkCmdFillBuffer(cmd_buffer.cmd_buffer, buffer, 0, size, 0);
	
	VK_CALL_RET(vkEndCommandBuffer(cmd_buffer.cmd_buffer), "failed to end command buffer");
	((vulkan_queue*)cqueue.get())->submit_command_buffer(cmd_buffer);
}

bool vulkan_buffer::resize(shared_ptr<compute_queue> cqueue floor_unused, const size_t& new_size_ floor_unused,
						   const bool copy_old_data floor_unused, const bool copy_host_data floor_unused,
						   void* new_host_ptr floor_unused) {
	// TODO: implement this
	return false;
}

void* __attribute__((aligned(128))) vulkan_buffer::map(shared_ptr<compute_queue> cqueue,
													   const COMPUTE_MEMORY_MAP_FLAG flags_,
													   const size_t size_, const size_t offset) {
	const size_t map_size = (size_ == 0 ? size : size_);
	if(!map_check(size, map_size, flags, flags_, offset)) return nullptr;
	return vulkan_memory::map(cqueue, flags_, map_size, offset);
}

void vulkan_buffer::unmap(shared_ptr<compute_queue> cqueue, void* __attribute__((aligned(128))) mapped_ptr) {
	return vulkan_memory::unmap(cqueue, mapped_ptr);
}

bool vulkan_buffer::acquire_opengl_object(shared_ptr<compute_queue>) {
	log_error("not supported by vulkan");
	return false;
}

bool vulkan_buffer::release_opengl_object(shared_ptr<compute_queue>) {
	log_error("not supported by vulkan");
	return false;
}

#endif
