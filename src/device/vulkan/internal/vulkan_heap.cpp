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

#include "device/vulkan/internal/vulkan_heap.hpp"

//! set to 1 to enable more in-depth and costly debugging functionality in VMA
#define FLOOR_VMA_DEBUGGING 0

#if !defined(FLOOR_NO_VULKAN)
#include <floor/core/logger.hpp>
#include <floor/constexpr/const_string.hpp>
#include <floor/device/vulkan/vulkan_context.hpp>

#define VMA_VULKAN_VERSION 1004000
#define VMA_DEDICATED_ALLOCATION 1
#define VMA_BIND_MEMORY2 1
#define VMA_MEMORY_BUDGET 1
#define VMA_BUFFER_DEVICE_ADDRESS 1
#define VMA_MEMORY_PRIORITY 1
#define VMA_KHR_MAINTENANCE4 1
#define VMA_KHR_MAINTENANCE5 1
#define VMA_EXTERNAL_MEMORY 1
#define VMA_MAPPING_HYSTERESIS_ENABLED 1
#if defined(__WINDOWS__)
#define VMA_EXTERNAL_MEMORY_WIN32 1
#else
#define VMA_EXTERNAL_MEMORY_WIN32 0
#endif

#define VMA_CPP20 1
#define VMA_USE_STL_SHARED_MUTEX 1

#define VMA_STATIC_VULKAN_FUNCTIONS 1
#define VMA_DYNAMIC_VULKAN_FUNCTIONS 0

#if defined(FLOOR_DEBUG)
#define VMA_HEAVY_ASSERT(expr) assert(expr)
#define VMA_ASSERT(expr) assert(expr)
#else
#define VMA_HEAVY_ASSERT(expr)
#define VMA_ASSERT(expr)
#endif

template <size_t format_len>
static inline constexpr size_t count_printf_format_args(const char (&format)[format_len]) {
	size_t format_arg_count = 0;
	for (size_t i = 0; i < format_len; ++i) {
		if (format[i] == '%') {
			++format_arg_count;
			++i; // skip format char
		}
	}
	return format_arg_count;
}

template <size_t arg_count, size_t format_len>
static inline constexpr auto convert_log_format(const char (&format)[format_len]) {
	char str[format_len - arg_count] {};
	for (size_t in = 0, out = 0; in < format_len; ++in) {
		if (format[in] == '%') {
			str[out++] = '$';
			++in; // skip format char
		} else {
			str[out++] = format[in];
		}
	}
	return fl::make_const_string(str);
}

#define PRIu64 "_"
#define VMA_LEAK_LOG_FORMAT(format, ...) \
fl::logger::log<count_printf_format_args(format)>(fl::logger::LOG_TYPE::DEBUG_MSG, __FILE_NAME__, __func__, \
												  (fl::make_const_string("VMA: leak: ") + \
												   convert_log_format<count_printf_format_args(format)>(format)).data(), \
												  ##__VA_ARGS__)

#if FLOOR_VMA_DEBUGGING
#define VMA_DEBUG_LOG(str) log_debug("VMA: $", str)
#define VMA_STATS_STRING_ENABLED 1
#define VMA_DEBUG_MARGIN 16
#define VMA_DEBUG_DETECT_CORRUPTION 1
#define VMA_DEBUG_MIN_BUFFER_IMAGE_GRANULARITY 256
#define VMA_DEBUG_DONT_EXCEED_MAX_MEMORY_ALLOCATION_COUNT 1
#else
#define VMA_DEBUG_LOG(str)
#define VMA_STATS_STRING_ENABLED 0
#define VMA_DEBUG_MARGIN 0
#define VMA_DEBUG_DETECT_CORRUPTION 0
#define VMA_DEBUG_MIN_BUFFER_IMAGE_GRANULARITY 1
#define VMA_DEBUG_DONT_EXCEED_MAX_MEMORY_ALLOCATION_COUNT 0
#endif

FLOOR_PUSH_WARNINGS()
FLOOR_IGNORE_WARNING(cast-align)
FLOOR_IGNORE_WARNING(cast-function-type-strict)
FLOOR_IGNORE_WARNING(covered-switch-default)
FLOOR_IGNORE_WARNING(extra-semi-stmt)
FLOOR_IGNORE_WARNING(inconsistent-missing-destructor-override)
FLOOR_IGNORE_WARNING(missing-field-initializers)
FLOOR_IGNORE_WARNING(nullability-completeness)
FLOOR_IGNORE_WARNING(sign-conversion)
FLOOR_IGNORE_WARNING(suggest-destructor-override)
FLOOR_IGNORE_WARNING(unused-parameter)
FLOOR_IGNORE_WARNING(unused-private-field)
FLOOR_IGNORE_WARNING(unused-template)
FLOOR_IGNORE_WARNING(unused-variable)
FLOOR_IGNORE_WARNING(weak-vtables)
FLOOR_IGNORE_WARNING(thread-safety-analysis) // TODO: implement support for this

#define VMA_IMPLEMENTATION
#include "../../../../external/vma/include/vk_mem_alloc.h"

FLOOR_POP_WARNINGS()

namespace fl {

vulkan_heap::vulkan_heap(const vulkan_device& dev_) : dev(dev_) {
	const VmaVulkanFunctions vk_funcs {
		// global functions (via global volk functions)
		.vkGetInstanceProcAddr = vkGetInstanceProcAddr,
		.vkGetDeviceProcAddr = vkGetDeviceProcAddr,
		.vkGetPhysicalDeviceProperties = vkGetPhysicalDeviceProperties,
		.vkGetPhysicalDeviceMemoryProperties = vkGetPhysicalDeviceMemoryProperties,
		// device specific functions (via volk device table)
		.vkAllocateMemory = dev.vk->vkAllocateMemory,
		.vkFreeMemory = dev.vk->vkFreeMemory,
		.vkMapMemory = dev.vk->vkMapMemory,
		.vkUnmapMemory = dev.vk->vkUnmapMemory,
		.vkFlushMappedMemoryRanges = dev.vk->vkFlushMappedMemoryRanges,
		.vkInvalidateMappedMemoryRanges = dev.vk->vkInvalidateMappedMemoryRanges,
		.vkBindBufferMemory = dev.vk->vkBindBufferMemory,
		.vkBindImageMemory = dev.vk->vkBindImageMemory,
		.vkGetBufferMemoryRequirements = dev.vk->vkGetBufferMemoryRequirements,
		.vkGetImageMemoryRequirements = dev.vk->vkGetImageMemoryRequirements,
		.vkCreateBuffer = dev.vk->vkCreateBuffer,
		.vkDestroyBuffer = dev.vk->vkDestroyBuffer,
		.vkCreateImage = dev.vk->vkCreateImage,
		.vkDestroyImage = dev.vk->vkDestroyImage,
		.vkCmdCopyBuffer = dev.vk->vkCmdCopyBuffer,
		.vkGetBufferMemoryRequirements2KHR = dev.vk->vkGetBufferMemoryRequirements2,
		.vkGetImageMemoryRequirements2KHR = dev.vk->vkGetImageMemoryRequirements2,
		.vkBindBufferMemory2KHR = dev.vk->vkBindBufferMemory2,
		.vkBindImageMemory2KHR = dev.vk->vkBindImageMemory2,
		// one more global
		.vkGetPhysicalDeviceMemoryProperties2KHR = vkGetPhysicalDeviceMemoryProperties2,
		.vkGetDeviceBufferMemoryRequirements = dev.vk->vkGetDeviceBufferMemoryRequirements,
		.vkGetDeviceImageMemoryRequirements = dev.vk->vkGetDeviceImageMemoryRequirements,
#if defined(__WINDOWS__)
		.vkGetMemoryWin32HandleKHR = dev.vk->vkGetMemoryWin32HandleKHR,
#else
		.vkGetMemoryWin32HandleKHR = nullptr,
#endif
	};
	
	// VMA uses a default block size of 256MiB, but this is generally too small for GPUs with larger VRAM and our use cases
	uint64_t heap_block_size = 0u;
	// NOTE: not checking for exact sizes here as reported sizes may be somewhat less than their advertized GB size
	if (dev.global_mem_size >= 16'000'000'000ull) {
		// 16GB-ish+: use 1GiB blocks
		heap_block_size = 1024ull * 1024ull * 1024ull;
	} else if (dev.global_mem_size >= 8'000'000'000ull) {
		// 8GB-ish+: use 512MiB blocks
		heap_block_size = 512ull * 1024ull * 1024ull;
	}
	
	const VmaAllocatorCreateInfo create_info {
		.flags = (VMA_ALLOCATOR_CREATE_EXT_MEMORY_BUDGET_BIT |
				  VMA_ALLOCATOR_CREATE_EXT_MEMORY_PRIORITY_BIT |
				  VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT |
				  VMA_ALLOCATOR_CREATE_KHR_MAINTENANCE4_BIT |
				  VMA_ALLOCATOR_CREATE_KHR_MAINTENANCE5_BIT
#if defined(__WINDOWS__)
				  | VMA_ALLOCATOR_CREATE_KHR_EXTERNAL_MEMORY_WIN32_BIT
#endif
				  ),
		.physicalDevice = dev.physical_device,
		.device = dev.device,
		.preferredLargeHeapBlockSize = heap_block_size,
		.pAllocationCallbacks = nullptr,
		.pDeviceMemoryCallbacks = nullptr,
		.pHeapSizeLimit = nullptr,
		.pVulkanFunctions = &vk_funcs,
		.instance = ((const vulkan_context*)dev.context)->get_vulkan_context(),
		.vulkanApiVersion = VK_MAKE_VERSION(1, 4, 0),
		.pTypeExternalMemoryHandleTypes = nullptr, // TODO: sharing/external mem support
	};
	if (vmaCreateAllocator(&create_info, &allocator) != VK_SUCCESS) {
		throw std::runtime_error("failed to create Vulkan memory allocator for device " + dev.name);
	}
}

struct allocation_flags_t {
	VmaAllocationCreateFlags vma_flags;
	VkMemoryPropertyFlags req_flags;
	VkMemoryPropertyFlags pref_flags;
};

static allocation_flags_t compute_common_allocation_flags(const vulkan_device& dev, const MEMORY_FLAG flags) {
	// TODO: VMA_ALLOCATION_CREATE_DONT_BIND_BIT/VMA_ALLOCATION_CREATE_CAN_ALIAS_BIT when allocating image with is_aliased_array
	// TODO: VMA_MEMORY_USAGE_GPU_LAZILY_ALLOCATED if transient (+must have VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT)
	
	allocation_flags_t alloc_flags {
		.req_flags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
	};
	
	const auto is_host_coherent = has_flag<MEMORY_FLAG::VULKAN_HOST_COHERENT>(flags);
	bool is_host_accessible = is_host_coherent;
	if (has_flag<MEMORY_FLAG::HOST_READ>(flags)) {
		// read-only or read-write
		alloc_flags.vma_flags |= VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT;
		is_host_accessible = true;
	} else if (has_flag<MEMORY_FLAG::HOST_WRITE>(flags)) {
		// write-only
		alloc_flags.vma_flags |= VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
		is_host_accessible = true;
	} else if (is_host_coherent) {
		// neither host-read or host-write was requested, but host-coherent was -> assume random access
		alloc_flags.vma_flags |= VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT;
	}
	
	// if host-cohorent isn't required (just preferred), allow non-host-visible memory regardless
	if (is_host_accessible && !is_host_coherent) {
		if (dev.prefer_host_coherent_mem) {
			alloc_flags.pref_flags |= VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
		}
		alloc_flags.vma_flags |= VMA_ALLOCATION_CREATE_HOST_ACCESS_ALLOW_TRANSFER_INSTEAD_BIT;
	}
	
	return alloc_flags;
}

vulkan_heap::buffer_allocation_t vulkan_heap::create_buffer(const VkBufferCreateInfo& create_info,
															const MEMORY_FLAG flags) {
	auto [vma_flags, req_flags, pref_flags] = compute_common_allocation_flags(dev, flags);
	
	const VmaAllocationCreateInfo vma_create_info {
		.flags = vma_flags,
		.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
		.requiredFlags = req_flags,
		.preferredFlags = pref_flags,
		.memoryTypeBits = 0u,
		.pool = nullptr,
		.pUserData = nullptr,
		// giver higher prio to descriptor buffers
		.priority = has_flag<MEMORY_FLAG::VULKAN_DESCRIPTOR_BUFFER>(flags) ? 0.5f : 0.0f,
	};
	buffer_allocation_t ret {};
	VmaAllocationInfo alloc_info {};
	VK_CALL_RET(vmaCreateBuffer(allocator, &create_info, &vma_create_info, &ret.buffer, &ret.allocation, &alloc_info),
				"failed to allocate buffer", {})
	ret.memory = alloc_info.deviceMemory;
	ret.allocation_size = alloc_info.size;
	ret.is_host_visible = std::ranges::contains(dev.host_visible_indices, alloc_info.memoryType);
	floor_return_no_nrvo(ret);
}

vulkan_heap::image_allocation_t vulkan_heap::create_image(const VkImageCreateInfo& create_info,
														  const MEMORY_FLAG flags, const IMAGE_TYPE image_type) {
	auto [vma_flags, req_flags, pref_flags] = compute_common_allocation_flags(dev, flags);
	
	const VmaAllocationCreateInfo vma_create_info {
		.flags = vma_flags,
		.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
		.requiredFlags = req_flags,
		.preferredFlags = pref_flags,
		.memoryTypeBits = 0u,
		.pool = nullptr,
		.pUserData = nullptr,
		// always set highest prio for render targets
		.priority = has_flag<IMAGE_TYPE::FLAG_RENDER_TARGET>(image_type) ? 1.0f : 0.0f,
	};
	image_allocation_t ret {};
	VmaAllocationInfo alloc_info {};
	VK_CALL_RET(vmaCreateImage(allocator, &create_info, &vma_create_info, &ret.image, &ret.allocation, &alloc_info),
				"failed to allocate image", {})
	ret.memory = alloc_info.deviceMemory;
	ret.allocation_size = alloc_info.size;
	ret.is_host_visible = std::ranges::contains(dev.host_visible_indices, alloc_info.memoryType);
	floor_return_no_nrvo(ret);
}

void vulkan_heap::destroy_allocation(VmaAllocation allocation, VkBuffer buffer) {
	assert(allocation && buffer);
	vmaDestroyBuffer(allocator, buffer, allocation);
}

void vulkan_heap::destroy_allocation(VmaAllocation allocation, VkImage image) {
	assert(allocation && image);
	vmaDestroyImage(allocator, image, allocation);
}

void* vulkan_heap::map_memory(VmaAllocation allocation) {
	void* mapped_ptr = nullptr;
	if (const auto ret = vmaMapMemory(allocator, allocation, &mapped_ptr); ret != VK_SUCCESS) {
		log_error("failed to map heap memory: $: $", ret, vulkan_error_to_string(ret));
		return nullptr;
	}
	return mapped_ptr;
}

void vulkan_heap::unmap_memory(VmaAllocation allocation) {
	vmaUnmapMemory(allocator, allocation);
}

bool vulkan_heap::host_to_device_copy(const void* host_ptr, VmaAllocation allocation, const uint64_t alloc_offset, const uint64_t copy_size) {
	return (vmaCopyMemoryToAllocation(allocator, host_ptr, allocation, alloc_offset, copy_size) == VK_SUCCESS);
}

bool vulkan_heap::device_to_host_copy(VmaAllocation allocation, void* host_ptr, const uint64_t alloc_offset, const uint64_t copy_size) {
	return (vmaCopyAllocationToMemory(allocator, allocation, alloc_offset, host_ptr, copy_size) == VK_SUCCESS);
}

uint64_t vulkan_heap::query_total_usage() {
	VmaBudget budgets[VK_MAX_MEMORY_HEAPS] {};
	const auto heap_count = std::min(allocator->GetMemoryHeapCount(), VK_MAX_MEMORY_HEAPS);
	vmaGetHeapBudgets(allocator, budgets);
	
	uint64_t usage = 0u;
	for (const auto& dev_heap_idx : dev.device_heap_indices) {
		if (dev_heap_idx >= heap_count) {
			continue;
		}
		usage += budgets[dev_heap_idx].statistics.allocationBytes;
		
#if 0
		log_warn("heap budget #$: alloc count $', alloc bytes $', block count $', block bytes $', usage $', budget $'",
				 dev_heap_idx,
				 budgets[dev_heap_idx].statistics.allocationCount,
				 budgets[dev_heap_idx].statistics.allocationBytes,
				 budgets[dev_heap_idx].statistics.blockCount,
				 budgets[dev_heap_idx].statistics.blockBytes,
				 budgets[dev_heap_idx].usage,
				 budgets[dev_heap_idx].budget);
#endif
	}
	
	return usage;
}

} // namespace fl

#endif
