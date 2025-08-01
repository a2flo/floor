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

#pragma once

#include <floor/device/vulkan/vulkan_common.hpp>
#include <floor/device/opencl/opencl_common.hpp>
#include <floor/device/device.hpp>
#include <memory>
#include <unordered_set>

namespace fl {

FLOOR_PUSH_WARNINGS()
FLOOR_IGNORE_WARNING(weak-vtables)

#if !defined(FLOOR_NO_VULKAN)
class vulkan_context;
#endif
class vulkan_heap;

class vulkan_device final : public device {
public:
	vulkan_device();
	
	//! supported Vulkan version
	VULKAN_VERSION vulkan_version { VULKAN_VERSION::NONE };
	
	//! supported SPIR-V version
	SPIRV_VERSION spirv_version { SPIRV_VERSION::NONE };
	
	//! Vulkan conformance version
	std::string conformance_version;
	
#if !defined(FLOOR_NO_VULKAN)
	//! physical Vulkan device
	VkPhysicalDevice physical_device { nullptr };
	
	//! logical Vulkan device
	VkDevice device { nullptr };
	
	//! memory properties of the device/implementation/host
	std::shared_ptr<VkPhysicalDeviceMemoryProperties> mem_props;
#else
	void* _physical_device { nullptr };
	void* _device { nullptr };
	std::shared_ptr<void*> _mem_props;
#endif
	
	//! internal memory heap
	//! NOTE: this exists by default, unless DEVICE_CONTEXT_FLAGS::DISABLE_HEAP was specified
	vulkan_heap* heap { nullptr };
	
	//! Vulkan physical device index inside the parent context/instance
	uint32_t physical_device_index { 0u };
	
	//! queue count per queue family
	//! (obviously also stores the queue family count)
	std::vector<uint32_t> queue_counts {};
	
	//! for internal purposes, do not change this
	mutable uint32_t cur_queue_idx { 0 };
	
	//! for internal purposes, do not change this
	mutable uint32_t cur_compute_queue_idx { 0u };
	
	//! queue family index for queues that support everything (graphics/compute/transfer)
	uint32_t all_queue_family_index { ~0u };
	
	//! queue family index for queues that support compute-only
	//! NOTE: for devices that don't have this, this falls back to the same value as "all_queue_family_index"
	uint32_t compute_queue_family_index { ~0u };
	
	//! queue families for concurrent resource creation
	std::array<uint32_t, 2> queue_families {{ 0, 0 }};
	
	//! max push constants size
	uint32_t max_push_constants_size { 0u };
	
	//! preferred memory type index for device memory allocations
	uint32_t device_mem_index { ~0u };
	
	//! preferred memory type index for cached host + device-visible memory allocations
	uint32_t host_mem_cached_index { ~0u };
	
	//! preferred memory type index for coherent host + device-local memory allocations
	uint32_t device_mem_host_coherent_index { ~0u };
	
	//! all available memory type indices for device memory allocations
	std::vector<uint32_t> device_mem_indices;
	
	//! all available memory type indices for cached host + device-visible memory allocations
	std::vector<uint32_t> host_mem_cached_indices;
	
	//! all available memory type indices for coherent host + device-local memory allocations
	std::vector<uint32_t> device_mem_host_coherent_indices;
	
	//! all available memory type indices for host-visible memory allocations
	std::vector<uint32_t> host_visible_indices;
	
	//! all heap indices that are device-local
	std::vector<uint32_t> device_heap_indices;
	
	//! if set, prefer host coherent memory over host cached memory,
	//! i.e. this is the case on systems all device memory is host coherent ("Resizable BAR"/"SAM")
	bool prefer_host_coherent_mem { false };
	
	//! if set, the device supports optimal image layouts in host-coherent device-local memory
	bool has_device_host_coherent_opt_image_support { false };
	
	//! feature support: can use 16-bit float types in SPIR-V
	bool float16_support { false };
	
	//! max per-IUB size in bytes
	uint32_t max_inline_uniform_block_size { 0u };
	
	//! max number of IUBs that can be used per function
	uint32_t max_inline_uniform_block_count { 0u };
	
	//! min offset alignment in SSBOs
	uint32_t min_storage_buffer_offset_alignment { 0u };
	
	//! device-specific descriptor sizes for use in descriptor buffers
	struct desc_buffer_sizes_t {
		//! size of a sampled image descriptor
		uint32_t sampled_image { 0u };
		//! size of a storage image descriptor
		uint32_t storage_image { 0u };
		//! size of a uniform buffer descriptor
		uint32_t ubo { 0u };
		//! size of a storage buffer descriptor
		uint32_t ssbo { 0u };
		//! size of a sampler descriptor
		uint32_t sampler { 0u };
	} desc_buffer_sizes;
	
	//! default max mip level count we use for precompiled FUBAR binaries
	//! NOTE: this can be overwritten when building FUBAR binaries
	static constexpr const uint32_t default_max_mip_levels { 16u };
	
	//! alignment requirement when setting descriptor buffer offsets (i.e. per sub-set within a buffer)
	uint32_t descriptor_buffer_offset_alignment { 0u };
	
	//! feature support: VK_NV_inherited_viewport_scissor
	bool inherited_viewport_scissor_support { false };
	
	//! feature support: VK_EXT_nested_command_buffer with all features supported
	bool nested_cmd_buffers_support { false };
	
	//! feature support: VK_EXT_swapchain_maintenance1
	bool swapchain_maintenance1_support { false };
	
	// put these at the end, b/c they are rather large
#if !defined(FLOOR_NO_VULKAN)
	//! fixed sampler descriptor set
	//! NOTE: this is allocated once at context creation
	VkDescriptorSetLayout fixed_sampler_desc_set_layout { nullptr };
	
	//! fixed sampler set
	//! NOTE: this is allocated once at context creation
	std::vector<VkSampler> fixed_sampler_set;
#else
	uint64_t _fixed_sampler_desc_set_layout;
	std::vector<uint64_t> _fixed_sampler_set;
#endif
	
	//! minimum required inline uniform block size that must be supported by a device
	static constexpr const uint32_t min_required_inline_uniform_block_size { 256 };
	
	//! minimum required inline uniform block count that must be supported by a device
	static constexpr const uint32_t min_required_inline_uniform_block_count { 16 };
	
	//! minimum required number of bindable descriptor sets for "argument_buffer_support"
	static constexpr const uint32_t min_required_bound_descriptor_sets_for_argument_buffer_support { 16u };
	
	//! returns true if the specified object is the same object as this
	bool operator==(const vulkan_device& dev) const {
		return (this == &dev);
	}
	
#if !defined(FLOOR_NO_VULKAN)
	std::shared_ptr<VolkDeviceTable> vk;
#endif
};

FLOOR_POP_WARNINGS()

} // namespace fl
