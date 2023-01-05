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

#ifndef __FLOOR_VULKAN_DEVICE_HPP__
#define __FLOOR_VULKAN_DEVICE_HPP__

#include <floor/compute/vulkan/vulkan_common.hpp>
#include <floor/compute/opencl/opencl_common.hpp>
#include <floor/compute/compute_device.hpp>
#include <memory>
#include <unordered_set>

FLOOR_PUSH_WARNINGS()
FLOOR_IGNORE_WARNING(weak-vtables)

#if !defined(FLOOR_NO_VULKAN)
class vulkan_compute;
#endif

class vulkan_device final : public compute_device {
public:
	vulkan_device();
	
	//! supported Vulkan version
	VULKAN_VERSION vulkan_version { VULKAN_VERSION::NONE };
	
	//! supported SPIR-V version
	SPIRV_VERSION spirv_version { SPIRV_VERSION::NONE };
	
#if !defined(FLOOR_NO_VULKAN)
	//! physical vulkan device
	VkPhysicalDevice physical_device { nullptr };
	
	//! logical vulkan device
	VkDevice device { nullptr };
	
	//! memory properties of the device/implementation/host
	shared_ptr<VkPhysicalDeviceMemoryProperties> mem_props;
#else
	void* _physical_device { nullptr };
	void* _device { nullptr };
	shared_ptr<void*> _mem_props;
#endif
	
	//! queue count per queue family
	//! (obviously also stores the queue family count)
	vector<uint32_t> queue_counts {};
	
	//! for internal purposes, do not change this
	mutable uint32_t cur_queue_idx { 0 };
	
	//! max push constants size
	uint32_t max_push_constants_size { 0u };
	
	//! preferred memory type index for device memory allocation
	uint32_t device_mem_index { ~0u };
	
	//! preferred memory type index for cached host + device-visible memory allocation
	uint32_t host_mem_cached_index { ~0u };
	
	//! preferred memory type index for coherent host + device-local memory allocation
	uint32_t device_mem_host_coherent_index { ~0u };
	
	//! all available memory type indices for device memory allocation
	unordered_set<uint32_t> device_mem_indices;
	
	//! all available memory type indices for cached host + device-visible memory allocation
	unordered_set<uint32_t> host_mem_cached_indices;
	
	//! all available memory type indices for coherent host + device-local memory allocation
	unordered_set<uint32_t> device_mem_host_coherent_indices;
	
	//! if set, prefer host coherent memory over host cached memory,
	//! i.e. this is the case on systems all device memory is host coherent ("Resizable BAR"/"SAM")
	bool prefer_host_coherent_mem { false };
	
	//! feature support: can use 16-bit int types in SPIR-V
	bool int16_support { false };
	
	//! feature support: can use 16-bit float types in SPIR-V
	bool float16_support { false };
	
	//! max per-IUB size in bytes
	uint32_t max_inline_uniform_block_size { 0u };
	
	//! max number of IUBs that can be used per function
	uint32_t max_inline_uniform_block_count { 0u };
	
	//! min offset alignment in SSBOs
	uint32_t min_storage_buffer_offset_alignment { 0u };
	
	//! feature support: can make full use of descriptor buffers
	bool descriptor_buffer_support { false };
	
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
	} desc_buffer_sizes;
	
	// put these at the end, b/c they are rather large
#if !defined(FLOOR_NO_VULKAN)
	//! fixed sampler descriptor set
	//! NOTE: this is allocated once at context creation
	VkDescriptorSetLayout fixed_sampler_desc_set_layout { nullptr };
	//! NOTE: not used when there is descriptor buffer support
	VkDescriptorPool legacy_fixed_sampler_desc_pool { nullptr };
	//! NOTE: not used when there is descriptor buffer support
	VkDescriptorSet legacy_fixed_sampler_desc_set { nullptr };
	
	//! fixed sampler set
	//! NOTE: this is allocated once at context creation
	vector<VkSampler> fixed_sampler_set;
	
	//! fixed sampler descriptor image infos, used to to update + bind the descriptor set
	//! NOTE: this solely consists of { nullptr, nullptr, 0 } objects, but is sadly necessary when updating/setting
	//!       the descriptor set (.sampler is ignored if immutable samplers are used, others are ignored anyways)
	vector<VkDescriptorImageInfo> fixed_sampler_image_info;
#else
	uint64_t _fixed_sampler_desc_set_layout;
	uint64_t _fixed_sampler_desc_pool;
	uint64_t _fixed_sampler_desc_set;
	vector<uint64_t> _fixed_sampler_set;
	struct _dummy_desc_img_info { void* _a; void* _b; uint32_t _c; };
	vector<_dummy_desc_img_info> _fixed_sampler_image_info;
#endif
	
	//! minimum required inline uniform block size that must be supported by a device
	static constexpr const uint32_t min_required_inline_uniform_block_size { 256 };
	
	//! minimum required inline uniform block count that must be supported by a device
	static constexpr const uint32_t min_required_inline_uniform_block_count { 16 };
	
	//! returns true if the specified object is the same object as this
	bool operator==(const vulkan_device& dev) const {
		return (this == &dev);
	}
	
};

FLOOR_POP_WARNINGS()

#endif
