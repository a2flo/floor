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

#include <floor/device/vulkan/vulkan_buffer.hpp>

#if !defined(FLOOR_NO_VULKAN)

#include <floor/core/logger.hpp>
#include "internal/vulkan_headers.hpp"
#include <floor/device/vulkan/vulkan_queue.hpp>
#include <floor/device/vulkan/vulkan_device.hpp>
#include <floor/device/vulkan/vulkan_context.hpp>
#include "internal/vulkan_debug.hpp"

#if defined(__WINDOWS__)
#include <floor/core/platform_windows.hpp>
#include <floor/core/essentials.hpp>
#include <vulkan/vulkan_win32.h>
#endif

namespace fl {

// TODO: proper error (return) value handling everywhere

vulkan_buffer::vulkan_buffer(const device_queue& cqueue,
							 const size_t& size_,
							 std::span<uint8_t> host_data_,
							 const MEMORY_FLAG flags_) :
device_buffer(cqueue, size_, host_data_, flags_),
vulkan_memory((const vulkan_device&)cqueue.get_device(), &buffer, flags) {
	if(size < min_multiple()) return;
	
	// TODO: handle the remaining flags + host ptr
	if (host_data.data() != nullptr && !has_flag<MEMORY_FLAG::NO_INITIAL_COPY>(flags)) {
		// TODO: flag?
	}
	
	// actually create the buffer
	if(!create_internal(true, cqueue)) {
		return; // can't do much else
	}
}

bool vulkan_buffer::create_internal(const bool copy_host_data, const device_queue& cqueue) {
	const auto& vk_dev = (const vulkan_device&)cqueue.get_device();
	const auto& vulkan_dev = vk_dev.device;
	
	// create the buffer
	const auto is_sharing = has_flag<MEMORY_FLAG::VULKAN_SHARING>(flags);
	const auto is_host_coherent = has_flag<MEMORY_FLAG::VULKAN_HOST_COHERENT>(flags);
	const auto is_desc_buffer = has_flag<MEMORY_FLAG::VULKAN_DESCRIPTOR_BUFFER>(flags);
	// set all the bits here, might need some better restrictions later on
	// NOTE: not setting vertex bit here, b/c we're always using SSBOs
	buffer_usage = (VK_BUFFER_USAGE_2_TRANSFER_SRC_BIT |
					VK_BUFFER_USAGE_2_TRANSFER_DST_BIT |
					VK_BUFFER_USAGE_2_STORAGE_BUFFER_BIT |
					VK_BUFFER_USAGE_2_INDEX_BUFFER_BIT |
					VK_BUFFER_USAGE_2_INDIRECT_BUFFER_BIT |
					VK_BUFFER_USAGE_2_SHADER_DEVICE_ADDRESS_BIT);
	VkExternalMemoryBufferCreateInfo ext_create_info;
	if (is_sharing) {
		ext_create_info = {
			.sType = VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_BUFFER_CREATE_INFO,
			.pNext = nullptr,
#if defined(__WINDOWS__)
			.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT,
#else
			.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT,
#endif
		};
	}
	if (is_desc_buffer) {
		buffer_usage |= VK_BUFFER_USAGE_2_RESOURCE_DESCRIPTOR_BUFFER_BIT_EXT;
	}
	const auto is_concurrent_sharing = (vk_dev.all_queue_family_index != vk_dev.compute_queue_family_index);
	const VkBufferUsageFlags2CreateInfo buffer_usage_flags_info {
		.sType = VK_STRUCTURE_TYPE_BUFFER_USAGE_FLAGS_2_CREATE_INFO,
		.pNext = (is_sharing ? &ext_create_info : nullptr),
		.usage = buffer_usage,
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
	VK_CALL_RET(vkCreateBuffer(vulkan_dev, &buffer_create_info, nullptr, &buffer),
				"buffer creation failed", false)
	
	// export memory alloc info (if sharing is enabled)
	VkExportMemoryAllocateInfo export_alloc_info;
#if defined(__WINDOWS__)
	VkExportMemoryWin32HandleInfoKHR export_mem_win32_info;
#endif
	if (is_sharing) {
#if defined(__WINDOWS__)
		// Windows 8+ needs more detailed sharing info
		export_mem_win32_info = {
			.sType = VK_STRUCTURE_TYPE_EXPORT_MEMORY_WIN32_HANDLE_INFO_KHR,
			.pNext = nullptr,
			// NOTE: SECURITY_ATTRIBUTES are only required if we want a child process to inherit this handle
			//       -> we don't need this, so set it to nullptr
			.pAttributes = nullptr,
			.dwAccess = (DXGI_SHARED_RESOURCE_READ | DXGI_SHARED_RESOURCE_WRITE),
			.name = nullptr,
		};
#endif
		
		export_alloc_info = {
			.sType = VK_STRUCTURE_TYPE_EXPORT_MEMORY_ALLOCATE_INFO,
#if defined(__WINDOWS__)
			.pNext = &export_mem_win32_info,
			.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT,
#else
			.pNext = nullptr,
			.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT,
#endif
		};
	}
	
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
		.buffer = buffer,
	};
	vkGetBufferMemoryRequirements2(vulkan_dev, &mem_req_info, &mem_req2);
	const auto is_dedicated = (ded_req.prefersDedicatedAllocation || ded_req.requiresDedicatedAllocation);
	const auto& mem_req = mem_req2.memoryRequirements;
	allocation_size = mem_req.size;
	
	const VkMemoryDedicatedAllocateInfo ded_alloc_info {
		.sType = VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO,
		.pNext = nullptr,
		.image = VK_NULL_HANDLE,
		.buffer = buffer,
	};
	if (is_sharing && is_dedicated) {
		export_alloc_info.pNext = &ded_alloc_info;
	}
	const VkMemoryAllocateFlagsInfo alloc_flags_info {
		.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO,
		.pNext = (is_sharing ? (void*)&export_alloc_info : (is_dedicated ? (void*)&ded_alloc_info : nullptr)),
		.flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT,
		.deviceMask = 0,
	};
	const VkMemoryAllocateInfo alloc_info {
		.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
		.pNext = &alloc_flags_info,
		.allocationSize = allocation_size,
		.memoryTypeIndex = find_memory_type_index(mem_req.memoryTypeBits, true /* prefer device memory */,
												  is_sharing || is_host_coherent /* sharing or host-coherent requires device memory */,
												  is_host_coherent /* require host-coherent if set */),
	};
	VK_CALL_RET(vkAllocateMemory(vulkan_dev, &alloc_info, nullptr, &mem), "buffer allocation failed", false)
	const VkBindBufferMemoryInfo bind_info {
		.sType = VK_STRUCTURE_TYPE_BIND_BUFFER_MEMORY_INFO,
		.pNext = nullptr,
		.buffer = buffer,
		.memory = mem,
		.memoryOffset = 0,
	};
	VK_CALL_RET(vkBindBufferMemory2(vulkan_dev, 1, &bind_info), "buffer allocation binding failed", false)
	
	// query device address
	const VkBufferDeviceAddressInfo dev_addr_info {
		.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
		.pNext = nullptr,
		.buffer = buffer,
	};
	buffer_device_address = vkGetBufferDeviceAddress(vulkan_dev, &dev_addr_info);
	if (buffer_device_address == 0) {
		log_error("failed to query buffer device address");
		return false;
	}
	//log_debug("dev addr: $X", buffer_device_address);
	
	// query descriptor data
	const VkDescriptorAddressInfoEXT addr_info {
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_ADDRESS_INFO_EXT,
		.pNext = nullptr,
		.address = buffer_device_address,
		.range = size,
		.format = VK_FORMAT_UNDEFINED,
	};
	const VkDescriptorGetInfoEXT desc_info {
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_GET_INFO_EXT,
		.pNext = nullptr,
		.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
		.data = {
			.pStorageBuffer = &addr_info,
		},
	};
	vkGetDescriptorEXT(vulkan_dev, &desc_info, vk_dev.desc_buffer_sizes.ssbo, &descriptor_data[0]);
	
	// buffer init from host data pointer
	if (copy_host_data &&
		host_data.data() != nullptr &&
		!has_flag<MEMORY_FLAG::NO_INITIAL_COPY>(flags)) {
		if (!write_memory_data(cqueue, host_data, 0, 0,
							   "failed to initialize buffer with host data (map failed)")) {
			return false;
		}
	}
	
	// get shared memory handle (if sharing is enabled)
	if (is_sharing) {
#if defined(__WINDOWS__)
		VkMemoryGetWin32HandleInfoKHR get_win32_handle {
			.sType = VK_STRUCTURE_TYPE_MEMORY_GET_WIN32_HANDLE_INFO_KHR,
			.pNext = nullptr,
			.memory = mem,
			.handleType = (VkExternalMemoryHandleTypeFlagBits)export_alloc_info.handleTypes,
		};
		VK_CALL_RET(vkGetMemoryWin32HandleKHR(vulkan_dev, &get_win32_handle, &shared_handle),
					"failed to retrieve shared win32 memory handle", false)
#else
		VkMemoryGetFdInfoKHR get_fd_handle {
			.sType = VK_STRUCTURE_TYPE_MEMORY_GET_FD_INFO_KHR,
			.pNext = nullptr,
			.memory = mem,
			.handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT,
		};
		VK_CALL_RET(vkGetMemoryFdKHR(vulkan_dev, &get_fd_handle, &shared_handle),
					"failed to retrieve shared fd memory handle", false)
#endif
	}
	
	return true;
}

vulkan_buffer::~vulkan_buffer() {
	if(buffer != nullptr) {
		vkDestroyBuffer(((const vulkan_device&)dev).device, buffer, nullptr);
		buffer = nullptr;
	}
}

void vulkan_buffer::read(const device_queue& cqueue, const size_t size_, const size_t offset) {
	read(cqueue, host_data.data(), size_, offset);
}

void vulkan_buffer::read(const device_queue& cqueue, void* dst,
						 const size_t size_, const size_t offset) {
	if(buffer == nullptr) return;
	
	const size_t read_size = (size_ == 0 ? size : size_);
	if(!read_check(size, read_size, offset, flags)) return;
	
	GUARD(lock);
	read_memory_data(cqueue, { (uint8_t*)dst, read_size }, offset, 0, "failed to read buffer");
}

void vulkan_buffer::write(const device_queue& cqueue, const size_t size_, const size_t offset) {
	write(cqueue, host_data.data(), size_, offset);
}

void vulkan_buffer::write(const device_queue& cqueue, const void* src,
						  const size_t size_, const size_t offset) {
	if(buffer == nullptr) return;
	
	const size_t write_size = (size_ == 0 ? size : size_);
	if(!write_check(size, write_size, offset, flags)) return;
	
	GUARD(lock);
	write_memory_data(cqueue, { (const uint8_t*)src, write_size }, offset, 0, "failed to write buffer");
}

void vulkan_buffer::copy(const device_queue& cqueue, const device_buffer& src,
						 const size_t size_, const size_t src_offset, const size_t dst_offset) {
	if(buffer == nullptr) return;
	
	const size_t src_size = src.get_size();
	const size_t copy_size = (size_ == 0 ? std::min(src_size, size) : size_);
	if(!copy_check(size, src_size, copy_size, dst_offset, src_offset)) return;
	
	const auto& vk_queue = (const vulkan_queue&)cqueue;
	VK_CMD_BLOCK_RET(vk_queue, "buffer zero", ({
		const VkBufferCopy2 region {
			.sType = VK_STRUCTURE_TYPE_BUFFER_COPY_2,
			.pNext = nullptr,
			.srcOffset = src_offset,
			.dstOffset = dst_offset,
			.size = copy_size,
		};
		const VkCopyBufferInfo2 info {
			.sType = VK_STRUCTURE_TYPE_COPY_BUFFER_INFO_2,
			.pNext = nullptr,
			.srcBuffer = ((const vulkan_buffer&)src).buffer,
			.dstBuffer = buffer,
			.regionCount = 1,
			.pRegions = &region,
		};
		vkCmdCopyBuffer2(block_cmd_buffer.cmd_buffer, &info);
	}), , true /* always blocking */);
}

bool vulkan_buffer::fill(const device_queue& cqueue,
						 const void* pattern, const size_t& pattern_size,
						 const size_t size_, const size_t offset) {
	if (buffer == nullptr) return false;
	
	const size_t fill_size = (size_ == 0 ? size : size_);
	if (!fill_check(size, fill_size, pattern_size, offset)) return false;
	
	// we can only fill the buffer with 32-bit values in Vulkan
	// -> fill size (in bytes) must be evenly divisible by 4 or must cover the whole buffer size
	if ((pattern_size == 1 || pattern_size == 2 || pattern_size == 4) &&
		(fill_size % 4u == 0u || fill_size == size)) {
		uint32_t u32_pattern = 0;
		if (pattern_size == 1) {
			const uint32_t u8_pattern = *(const uint8_t*)pattern;
			u32_pattern = (u8_pattern | (u8_pattern << 8u) | (u8_pattern << 16) | (u8_pattern << 24u));
		} else if (pattern_size == 2) {
			const uint32_t u16_pattern = *(const uint16_t*)pattern;
			u32_pattern = (u16_pattern | (u16_pattern << 16));
		} else {
			u32_pattern = *(const uint32_t*)pattern;
		}
		
		const auto& vk_queue = (const vulkan_queue&)cqueue;
		VK_CMD_BLOCK_RET(vk_queue, "buffer fill", ({
			vkCmdFillBuffer(block_cmd_buffer.cmd_buffer, buffer, offset, fill_size, u32_pattern);
		}), false /* false on error */, true /* always blocking */);
		
		return true;
	}
	
	// not a pattern size that allows a fast memset
	const size_t pattern_count = fill_size / pattern_size;
	const size_t pattern_fill_size = pattern_size * pattern_count;
	if (has_flag<MEMORY_FLAG::VULKAN_HOST_COHERENT>(flags)) {
		// -> host-coherent, map and write directly
		auto mapped_ptr = (uint8_t*)map(cqueue, MEMORY_MAP_FLAG::WRITE | MEMORY_MAP_FLAG::BLOCK);
		if (!mapped_ptr) {
			return false;
		}
		
		uint8_t* write_ptr = mapped_ptr;
		for (size_t i = 0; i < pattern_count; i++) {
			memcpy(write_ptr, pattern, pattern_size);
			write_ptr += pattern_size;
		}
		
		unmap(cqueue, mapped_ptr);
		return true;
	}
	// -> create a host buffer with the pattern and upload it
	auto pattern_buffer = std::make_unique<uint8_t[]>(pattern_fill_size);
	uint8_t* write_ptr = pattern_buffer.get();
	for (size_t i = 0; i < pattern_count; i++) {
		memcpy(write_ptr, pattern, pattern_size);
		write_ptr += pattern_size;
	}
	write(cqueue, pattern_buffer.get(), pattern_fill_size, offset);
	return true;
}

bool vulkan_buffer::zero(const device_queue& cqueue) {
	if(buffer == nullptr) return false;
	
	const auto& vk_queue = (const vulkan_queue&)cqueue;
	VK_CMD_BLOCK(vk_queue, "buffer zero", ({
		vkCmdFillBuffer(block_cmd_buffer.cmd_buffer, buffer, 0, size, 0);
	}), true /* always blocking */);
	return true;
}

void* __attribute__((aligned(128))) vulkan_buffer::map(const device_queue& cqueue,
													   const MEMORY_MAP_FLAG flags_,
													   const size_t size_, const size_t offset) {
	const size_t map_size = (size_ == 0 ? size : size_);
	if(!map_check(size, map_size, flags, flags_, offset)) return nullptr;
	return vulkan_memory::map(cqueue, flags_, map_size, offset);
}

bool vulkan_buffer::unmap(const device_queue& cqueue, void* __attribute__((aligned(128))) mapped_ptr) {
	return vulkan_memory::unmap(cqueue, mapped_ptr);
}

void vulkan_buffer::set_debug_label(const std::string& label) {
	device_memory::set_debug_label(label);
	set_vulkan_debug_label(vk_dev, VK_OBJECT_TYPE_BUFFER, uint64_t(buffer), label);
}

} // namespace fl

#endif
