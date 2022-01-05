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

#include <floor/compute/vulkan/vulkan_buffer.hpp>

#if !defined(FLOOR_NO_VULKAN)

#include <floor/core/logger.hpp>
#include <floor/core/core.hpp>
#include <floor/compute/vulkan/vulkan_queue.hpp>
#include <floor/compute/vulkan/vulkan_device.hpp>
#include <floor/compute/vulkan/vulkan_compute.hpp>

#if defined(__WINDOWS__)
#include <floor/core/platform_windows.hpp>
#include <floor/core/essentials.hpp>
#include <vulkan/vulkan_win32.h>
#endif

// TODO: proper error (return) value handling everywhere

vulkan_buffer::vulkan_buffer(const compute_queue& cqueue,
							 const size_t& size_,
							 void* host_ptr_,
							 const COMPUTE_MEMORY_FLAG flags_,
							 const uint32_t opengl_type_,
							 const uint32_t external_gl_object_) :
compute_buffer(cqueue, size_, host_ptr_, flags_, opengl_type_, external_gl_object_),
vulkan_memory((const vulkan_device&)cqueue.get_device(), &buffer, flags) {
	if(size < min_multiple()) return;
	
	// TODO: handle the remaining flags + host ptr
	if(host_ptr_ != nullptr && !has_flag<COMPUTE_MEMORY_FLAG::NO_INITIAL_COPY>(flags)) {
		// TODO: flag?
	}
	
	// actually create the buffer
	if(!create_internal(true, cqueue)) {
		return; // can't do much else
	}
}

bool vulkan_buffer::create_internal(const bool copy_host_data, const compute_queue& cqueue) {
	const auto& vulkan_dev = ((const vulkan_device&)cqueue.get_device()).device;

	// create the buffer
	const auto is_sharing = has_flag<COMPUTE_MEMORY_FLAG::VULKAN_SHARING>(flags);
	const auto is_host_coherent = has_flag<COMPUTE_MEMORY_FLAG::VULKAN_HOST_COHERENT>(flags);
	VkExternalMemoryBufferCreateInfo ext_create_info;
	if (is_sharing) {
		ext_create_info = {
			.sType = VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_BUFFER_CREATE_INFO,
			.pNext = nullptr,
#if defined(__WINDOWS__)
			.handleTypes = (core::is_windows_8_or_higher() ?
							VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT :
							VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_KMT_BIT),
#else
			.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT,
#endif
		};
	}
	const VkBufferCreateInfo buffer_create_info {
		.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
		.pNext = (is_sharing ? &ext_create_info : nullptr),
		.flags = 0,
		.size = size,
		// set all the bits here, might need some better restrictions later on
		// NOTE: not setting vertex bit here, b/c we're always using SSBOs
		.usage = (VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
				  VK_BUFFER_USAGE_TRANSFER_DST_BIT |
				  VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
				  VK_BUFFER_USAGE_INDEX_BUFFER_BIT |
				  VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT |
				  VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT),
		// NOTE: for performance reasons, we always want exclusive sharing
		.sharingMode = VK_SHARING_MODE_EXCLUSIVE,
		.queueFamilyIndexCount = 0,
		.pQueueFamilyIndices = nullptr,
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
		if (core::is_windows_8_or_higher()) {
			export_mem_win32_info = {
				.sType = VK_STRUCTURE_TYPE_EXPORT_MEMORY_WIN32_HANDLE_INFO_KHR,
				.pNext = nullptr,
				// NOTE: SECURITY_ATTRIBUTES are only required if we want a child process to inherit this handle
				//       -> we don't need this, so set it to nullptr
				.pAttributes = nullptr,
				.dwAccess = (DXGI_SHARED_RESOURCE_READ | DXGI_SHARED_RESOURCE_WRITE),
				.name = nullptr,
			};
		}
#endif
		
		export_alloc_info = {
			.sType = VK_STRUCTURE_TYPE_EXPORT_MEMORY_ALLOCATE_INFO,
#if defined(__WINDOWS__)
			.pNext = (core::is_windows_8_or_higher() ? &export_mem_win32_info : nullptr),
			.handleTypes = (core::is_windows_8_or_higher() ?
							VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT :
							VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_KMT_BIT),
#else
			.pNext = nullptr,
			.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT,
#endif
		};
	}
	
	// allocate / back it up
	VkMemoryRequirements mem_req;
	vkGetBufferMemoryRequirements(vulkan_dev, buffer, &mem_req);
	allocation_size = mem_req.size;
	
	const VkMemoryAllocateFlagsInfo alloc_flags_info {
		.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO,
		.pNext = (is_sharing ? &export_alloc_info : nullptr),
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
	VK_CALL_RET(vkBindBufferMemory(vulkan_dev, buffer, mem, 0), "buffer allocation binding failed", false)
	
	// update buffer desc info
	buffer_info.buffer = buffer;
	buffer_info.offset = 0;
	buffer_info.range = size;
	
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
	
	// buffer init from host data pointer
	if(copy_host_data &&
	   host_ptr != nullptr &&
	   !has_flag<COMPUTE_MEMORY_FLAG::NO_INITIAL_COPY>(flags)) {
		if(!write_memory_data(cqueue, host_ptr, size, 0, 0,
							  "failed to initialize buffer with host data (map failed)")) {
			return false;
		}
	}
	
	// get shared memory handle (if sharing is enabled)
	if (is_sharing) {
		const auto& vk_ctx = *((const vulkan_compute*)cqueue.get_device().context);
#if defined(__WINDOWS__)
		VkMemoryGetWin32HandleInfoKHR get_win32_handle {
			.sType = VK_STRUCTURE_TYPE_MEMORY_GET_WIN32_HANDLE_INFO_KHR,
			.pNext = nullptr,
			.memory = mem,
			.handleType = (VkExternalMemoryHandleTypeFlagBits)export_alloc_info.handleTypes,
		};
		VK_CALL_RET(vk_ctx.vulkan_get_memory_win32_handle(vulkan_dev, &get_win32_handle, &shared_handle),
					"failed to retrieve shared win32 memory handle", false)
#else
		VkMemoryGetFdInfoKHR get_fd_handle {
			.sType = VK_STRUCTURE_TYPE_MEMORY_GET_FD_INFO_KHR,
			.pNext = nullptr,
			.memory = mem,
			.handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT,
		};
		VK_CALL_RET(vk_ctx.vulkan_get_memory_fd(vulkan_dev, &get_fd_handle, &shared_handle),
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
	buffer_info = { nullptr, 0, 0 };
}

void vulkan_buffer::read(const compute_queue& cqueue, const size_t size_, const size_t offset) {
	read(cqueue, host_ptr, size_, offset);
}

void vulkan_buffer::read(const compute_queue& cqueue, void* dst,
						 const size_t size_, const size_t offset) {
	if(buffer == nullptr) return;
	
	const size_t read_size = (size_ == 0 ? size : size_);
	if(!read_check(size, read_size, offset, flags)) return;
	
	GUARD(lock);
	read_memory_data(cqueue, dst, read_size, offset, 0, "failed to read buffer");
}

void vulkan_buffer::write(const compute_queue& cqueue, const size_t size_, const size_t offset) {
	write(cqueue, host_ptr, size_, offset);
}

void vulkan_buffer::write(const compute_queue& cqueue, const void* src,
						  const size_t size_, const size_t offset) {
	if(buffer == nullptr) return;
	
	const size_t write_size = (size_ == 0 ? size : size_);
	if(!write_check(size, write_size, offset, flags)) return;
	
	GUARD(lock);
	write_memory_data(cqueue, src, write_size, offset, 0, "failed to write buffer");
}

void vulkan_buffer::copy(const compute_queue& cqueue, const compute_buffer& src,
						 const size_t size_, const size_t src_offset, const size_t dst_offset) {
	if(buffer == nullptr) return;
	
	const size_t src_size = src.get_size();
	const size_t copy_size = (size_ == 0 ? std::min(src_size, size) : size_);
	if(!copy_check(size, src_size, copy_size, dst_offset, src_offset)) return;
	
	const auto& vk_queue = (const vulkan_queue&)cqueue;
	VK_CMD_BLOCK_RET(vk_queue, "buffer zero", ({
		const VkBufferCopy region {
			.srcOffset = src_offset,
			.dstOffset = dst_offset,
			.size = copy_size,
		};
		vkCmdCopyBuffer(cmd_buffer.cmd_buffer, ((const vulkan_buffer&)src).buffer, buffer, 1, &region);
	}), , true /* always blocking */);
}

bool vulkan_buffer::fill(const compute_queue& cqueue floor_unused,
						 const void* pattern floor_unused, const size_t& pattern_size floor_unused,
						 const size_t size_ floor_unused, const size_t offset floor_unused) {
	if(buffer == nullptr) return false;
	
	// TODO: implement this
	log_error("vulkan_buffer::fill not implemented yet");
	return false;
}

bool vulkan_buffer::zero(const compute_queue& cqueue) {
	if(buffer == nullptr) return false;
	
	const auto& vk_queue = (const vulkan_queue&)cqueue;
	VK_CMD_BLOCK(vk_queue, "buffer zero", ({
		vkCmdFillBuffer(cmd_buffer.cmd_buffer, buffer, 0, size, 0);
	}), true /* always blocking */);
	return true;
}

bool vulkan_buffer::resize(const compute_queue& cqueue floor_unused, const size_t& new_size_ floor_unused,
						   const bool copy_old_data floor_unused, const bool copy_host_data floor_unused,
						   void* new_host_ptr floor_unused) {
	// TODO: implement this
	return false;
}

void* __attribute__((aligned(128))) vulkan_buffer::map(const compute_queue& cqueue,
													   const COMPUTE_MEMORY_MAP_FLAG flags_,
													   const size_t size_, const size_t offset) {
	const size_t map_size = (size_ == 0 ? size : size_);
	if(!map_check(size, map_size, flags, flags_, offset)) return nullptr;
	return vulkan_memory::map(cqueue, flags_, map_size, offset);
}

bool vulkan_buffer::unmap(const compute_queue& cqueue, void* __attribute__((aligned(128))) mapped_ptr) {
	return vulkan_memory::unmap(cqueue, mapped_ptr);
}

bool vulkan_buffer::acquire_opengl_object(const compute_queue*) {
	log_error("not supported by vulkan");
	return false;
}

bool vulkan_buffer::release_opengl_object(const compute_queue*) {
	log_error("not supported by vulkan");
	return false;
}

void vulkan_buffer::set_debug_label(const string& label) {
	compute_memory::set_debug_label(label);
	((const vulkan_compute*)device.context)->set_vulkan_debug_label(device, VK_OBJECT_TYPE_BUFFER, uint64_t(buffer), label);
}

#endif
