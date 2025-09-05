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

#include <floor/device/vulkan/vulkan_image.hpp>

#if !defined(FLOOR_NO_VULKAN)

#include <floor/core/logger.hpp>
#include "internal/vulkan_headers.hpp"
#include "internal/vulkan_debug.hpp"
#include "internal/vulkan_conversion.hpp"
#include "internal/vulkan_image_internal.hpp"
#include "internal/vulkan_heap.hpp"
#include <floor/device/vulkan/vulkan_queue.hpp>
#include <floor/device/vulkan/vulkan_device.hpp>
#include <floor/device/vulkan/vulkan_context.hpp>
#include <floor/device/vulkan/vulkan_fence.hpp>

#if defined(__WINDOWS__)
#include <floor/core/platform_windows.hpp>
#include <floor/core/essentials.hpp>
#include <vulkan/vulkan_win32.h>
#endif

namespace fl {

// TODO: proper error (return) value handling everywhere

vulkan_image::vulkan_image(const device_queue& cqueue,
						   const uint4 image_dim_,
						   const IMAGE_TYPE image_type_,
						   std::span<uint8_t> host_data_,
						   const MEMORY_FLAG flags_,
						   const uint32_t mip_level_limit_) :
device_image(cqueue, image_dim_, image_type_, host_data_, flags_, nullptr, true /* may need shim type */, mip_level_limit_),
vulkan_memory((const vulkan_device&)cqueue.get_device(), &image, flags) {
	// NOTE: actual image will be created in vulkan_image_internal constructor
}

vulkan_image::vulkan_image(const device_queue& cqueue, const external_vulkan_image_info& external_image, std::span<uint8_t> host_data_, const MEMORY_FLAG flags_) :
device_image(cqueue, external_image.dim, compute_vulkan_image_type(external_image, flags_), host_data_, flags_,
			  nullptr, true /* we don't support this, but still need to set all vars + needed error checking */, 0u),
vulkan_memory((const vulkan_device&)cqueue.get_device(), &image, flags), is_external(true) {
	image = external_image.image;
	image_view = external_image.image_view;
	cur_access_mask = external_image.access_mask;
}

vulkan_image::~vulkan_image() {
	auto vulkan_dev = ((const vulkan_device&)dev).device;
	
	if (!is_external) {
		if (image_view != nullptr) {
			vkDestroyImageView(vulkan_dev, image_view, nullptr);
			image_view = nullptr;
		}
		
		// mip-map image views
		if (is_mip_mapped && (generate_mip_maps || has_flag<IMAGE_TYPE::WRITE>(image_type))) {
			// only need to destroy all created ones (not up to dev->max_mip_levels)
			for (uint32_t i = 0; i < mip_level_count; ++i) {
				vkDestroyImageView(vulkan_dev, mip_map_image_view[i], nullptr);
			}
		}
		
		if (image != nullptr) {
			if (is_heap_allocation) {
				vk_dev.heap->destroy_allocation(heap_allocation, image);
				heap_allocation = nullptr;
			} else {
				vkDestroyImage(vulkan_dev, image, nullptr);
			}
			image = nullptr;
		}
	}
}

bool vulkan_image::zero(const device_queue& cqueue) {
	if (image == nullptr) {
		return false;
	}
	
	if (has_flag<IMAGE_TYPE::FLAG_TRANSIENT>(image_type)) {
		// nothing to clear
		return true;
	}
	
	const auto& vk_queue = (const vulkan_queue&)cqueue;
	VK_CMD_BLOCK_RET(vk_queue, "image zero", ({
		// transition to optimal layout, then back to our current one at the end
		auto& internal = (vulkan_image_internal&)*this;
		
		const auto restore_access_mask = cur_access_mask;
		const auto restore_layout = internal.image_info.imageLayout;
		
		internal.transition(&cqueue, block_cmd_buffer.cmd_buffer, VK_ACCESS_2_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
							VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT, VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT);
		
		VkImageSubresourceRange zero_range {
			.aspectMask = vk_aspect_flags_from_type(image_type),
			.baseMipLevel = 0,
			.levelCount = mip_level_count,
			.baseArrayLayer = 0,
			.layerCount = layer_count,
		};
		if (has_flag<IMAGE_TYPE::FLAG_DEPTH>(image_type)) {
			zero_range.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
			if (has_flag<IMAGE_TYPE::FLAG_STENCIL>(image_type)) {
				zero_range.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
			}
			
			const VkClearDepthStencilValue clear_value {
				.depth = 0.0f,
				.stencil = 0u,
			};
			vkCmdClearDepthStencilImage(block_cmd_buffer.cmd_buffer, image, internal.image_info.imageLayout, &clear_value, 1, &zero_range);
		} else {
			const VkClearColorValue clear_value {
				.float32 = { 0.0f, 0.0f, 0.0f, 0.0f },
			};
			vkCmdClearColorImage(block_cmd_buffer.cmd_buffer, image, internal.image_info.imageLayout, &clear_value, 1, &zero_range);
		}
		
		internal.transition(&cqueue, block_cmd_buffer.cmd_buffer, restore_access_mask, restore_layout,
							VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT, VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT);
	}), false /* return false on error */, true /* always blocking */);
	
	return true;
}

bool vulkan_image::blit_internal(const bool is_async, const device_queue& cqueue, device_image& src,
								 const std::vector<const device_fence*>& wait_fences,
								 const std::vector<device_fence*>& signal_fences) {
	if (image == nullptr) {
		return false;
	}
	
	if (!blit_check(cqueue, src)) {
		return false;
	}
	
	auto& vk_src = (vulkan_image_internal&)src;
	auto src_image = vk_src.get_vulkan_image();
	if (!src_image) {
		log_error("blit: source Vulkan image is null");
		return false;
	}
	
	std::vector<vulkan_queue::wait_fence_t> vk_wait_fences;
	std::vector<vulkan_queue::signal_fence_t> vk_signal_fences;
	if (is_async) {
		vk_wait_fences = vulkan_queue::encode_wait_fences(wait_fences);
		vk_signal_fences = vulkan_queue::encode_signal_fences(signal_fences);
	}
	
	const auto dim_count = image_dim_count(image_type);
	const auto& vk_queue = (const vulkan_queue&)cqueue;
	VK_CMD_BLOCK_RET(vk_queue, "image blit", ({
		// transition to optimal layout, then back to our current one at the end
		auto& internal = (vulkan_image_internal&)*this;
		
		const auto restore_access_mask = cur_access_mask;
		const auto restore_layout = internal.image_info.imageLayout;
		
		const auto src_restore_access_mask = vk_src.cur_access_mask;
		const auto src_restore_layout = vk_src.image_info.imageLayout;
		
		vk_src.transition(&cqueue, block_cmd_buffer.cmd_buffer, VK_ACCESS_2_TRANSFER_READ_BIT, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
						  VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT, VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT);
		internal.transition(&cqueue, block_cmd_buffer.cmd_buffer, VK_ACCESS_2_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
							VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT, VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT);
		
		VkImageAspectFlags aspect_mask = vk_aspect_flags_from_type(image_type);
		
		std::vector<VkImageBlit2> regions;
		regions.reserve(mip_level_count);
		apply_on_levels<true /* blit all levels */>([this, &regions, &dim_count, &aspect_mask](const uint32_t& level,
																							   const uint4& mip_image_dim,
																							   const uint32_t&,
																							   const uint32_t&) {
			const auto imip_image_dim = mip_image_dim.cast<int>();
			const VkOffset3D extent {
				std::max(imip_image_dim.x, 1),
				dim_count >= 2 ? std::max(imip_image_dim.y, 1) : 1,
				dim_count >= 3 ? std::max(imip_image_dim.z, 1) : 1,
			};
			regions.emplace_back(VkImageBlit2 {
				.sType = VK_STRUCTURE_TYPE_IMAGE_BLIT_2,
				.pNext = nullptr,
				.srcSubresource = {
					.aspectMask = aspect_mask,
					.mipLevel = level,
					.baseArrayLayer = 0,
					.layerCount = layer_count,
				},
				.srcOffsets = { { 0, 0, 0 }, extent },
				.dstSubresource = {
					.aspectMask = aspect_mask,
					.mipLevel = level,
					.baseArrayLayer = 0,
					.layerCount = layer_count,
				},
				.dstOffsets = { { 0, 0, 0 }, extent },
			});
			return true;
		}, shim_image_type);
		
		assert(!regions.empty());
		const VkBlitImageInfo2 blit_info {
			.sType = VK_STRUCTURE_TYPE_BLIT_IMAGE_INFO_2,
			.pNext = nullptr,
			.srcImage = src_image,
			.srcImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
			.dstImage = image,
			.dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			.regionCount = uint32_t(regions.size()),
			.pRegions = &regions[0],
			.filter = VK_FILTER_NEAREST,
		};
		vkCmdBlitImage2(block_cmd_buffer.cmd_buffer, &blit_info);
		
		internal.transition(&cqueue, block_cmd_buffer.cmd_buffer, restore_access_mask, restore_layout,
							VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT, VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT);
		
		vk_src.transition(&cqueue, block_cmd_buffer.cmd_buffer, src_restore_access_mask, src_restore_layout,
						  VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT, VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT);
	}), false /* return false on error */, !is_async /* blocking if not async */, std::move(vk_wait_fences), std::move(vk_signal_fences));
	
	return true;
}

bool vulkan_image::blit(const device_queue& cqueue, device_image& src) {
	return blit_internal(false, cqueue, src, {}, {});
}

bool vulkan_image::blit_async(const device_queue& cqueue, device_image& src,
							  std::vector<const device_fence*>&& wait_fences,
							  std::vector<device_fence*>&& signal_fences) {
	return blit_internal(true, cqueue, src, wait_fences, signal_fences);
}

bool vulkan_image::write(const device_queue& cqueue, const void* src, const size_t src_size,
						 const uint3 offset, const uint3 extent, const uint2 mip_level_range, const uint2 layer_range) {
	if (!src) {
		return false;
	}
	
	if (!write_check(src_size, offset, extent, mip_level_range, layer_range)) {
		return false;
	}
	
	// need to convert RGB to RGBA if necessary
	std::span<const uint8_t> host_buffer { (const uint8_t*)src, src_size };
	std::unique_ptr<uint8_t[]> host_shim_buffer;
	size_t host_shim_buffer_size = 0;
	if (image_type != shim_image_type) {
		std::tie(host_shim_buffer, host_shim_buffer_size) = rgb_to_rgba(image_type, shim_image_type, host_buffer, generate_mip_maps);
	}
	auto cpy_host_data = (image_type != shim_image_type ?
						  std::span<const uint8_t> { host_shim_buffer.get(), host_shim_buffer_size } :
						  host_buffer);
	
	// compute copy/write regions
	auto& internal = (vulkan_image_internal&)*this;
	const bool is_compressed = image_compressed(image_type);
	const auto write_level_count = (mip_level_range.y - mip_level_range.x) + 1u;
	const auto write_layer_count = (layer_range.y - layer_range.x) + 1u;
	const auto img_type = (shim_image_type != IMAGE_TYPE::NONE ? shim_image_type : image_type);
	// run on all levels, skip those not in range
	uint64_t total_buffer_offset = 0u;
	std::vector<VkMemoryToImageCopy> mem_regions;
	std::vector<VkBufferImageCopy2> buffer_regions;
	if (vk_dev.host_image_copy_support) {
		mem_regions.reserve(write_level_count);
	} else {
		buffer_regions.reserve(write_level_count);
	}
	auto region_cpy_host_data = cpy_host_data;
	if (!apply_on_levels([this, &region_cpy_host_data, &total_buffer_offset, is_compressed, img_type,
						  offset, extent, mip_level_range, layer_range, write_layer_count,
						  &mem_regions, &buffer_regions](const uint32_t& level,
														 const uint4&,
														 const uint32_t&,
														 const uint32_t&) {
		if (level < mip_level_range.x || level > mip_level_range.y) {
			return true;
		}
		
		// derive mip extent/dim from user-specified extent
		const auto mip_extent = (extent >> level).maxed(1u);
		const auto mip_offset = (offset >> level);
		
		uint2 compression_block_dim;
		if (is_compressed) {
			// for compressed formats, we need to consider the actual bits-per-pixel and full block size per row -> need to multiply with Y block size
			compression_block_dim = image_compression_block_size(img_type);
			compression_block_dim.max(mip_extent.xy);
		}
		
		const auto write_data_size = image_mip_level_data_size_from_types(extent /* original extent! */, img_type, level, write_layer_count);
		assert(write_data_size > 0);
		if (region_cpy_host_data.size_bytes() < write_data_size) {
			log_error("image write: insufficient host data at mip-level $", level);
			return false;
		}
		
		if (vk_dev.host_image_copy_support) {
			mem_regions.emplace_back(VkMemoryToImageCopy {
				.sType = VK_STRUCTURE_TYPE_MEMORY_TO_IMAGE_COPY,
				.pNext = nullptr,
				.pHostPointer = region_cpy_host_data.data(),
				.memoryRowLength = (is_compressed ? compression_block_dim.x : 0 /* tightly packed */),
				.memoryImageHeight = (is_compressed ? compression_block_dim.y : 0 /* tightly packed */),
				.imageSubresource = {
					.aspectMask = vk_aspect_flags_from_type(img_type),
					.mipLevel = level,
					.baseArrayLayer = layer_range.x,
					.layerCount = write_layer_count,
				},
				.imageOffset = { int(mip_offset.x), int(mip_offset.y), int(mip_offset.z) },
				.imageExtent = { mip_extent.x, mip_extent.y, mip_extent.z },
			});
		} else {
			buffer_regions.emplace_back(VkBufferImageCopy2 {
				.sType = VK_STRUCTURE_TYPE_BUFFER_IMAGE_COPY_2,
				.pNext = nullptr,
				.bufferOffset = total_buffer_offset,
				.bufferRowLength = (is_compressed ? compression_block_dim.x : 0 /* tightly packed */),
				.bufferImageHeight = (is_compressed ? compression_block_dim.y : 0 /* tightly packed */),
				.imageSubresource = {
					.aspectMask = vk_aspect_flags_from_type(img_type),
					.mipLevel = level,
					.baseArrayLayer = layer_range.x,
					.layerCount = write_layer_count,
				},
				.imageOffset = { int(mip_offset.x), int(mip_offset.y), int(mip_offset.z) },
				.imageExtent = { mip_extent.x, mip_extent.y, mip_extent.z },
			});
		}
		region_cpy_host_data = region_cpy_host_data.subspan(write_data_size, region_cpy_host_data.size_bytes() - write_data_size);
		total_buffer_offset += write_data_size;
		
		return true;
	}, img_type)) {
		return false;
	}
	
	// use host image copy if available
	if (vk_dev.host_image_copy_support) {
		const VkCopyMemoryToImageInfo copy_info {
			.sType = VK_STRUCTURE_TYPE_COPY_MEMORY_TO_IMAGE_INFO,
			.pNext = nullptr,
			.flags = 0,
			.dstImage = image,
			.dstImageLayout = internal.image_info.imageLayout,
			.regionCount = uint32_t(mem_regions.size()),
			.pRegions = mem_regions.data(),
		};
		if (vk_dev.vulkan_version >= VULKAN_VERSION::VULKAN_1_4) {
			VK_CALL_RET(vkCopyMemoryToImage(vk_dev.device, &copy_info), "failed to copy host memory to device image", false)
		} else {
			VK_CALL_RET(vkCopyMemoryToImageEXT(vk_dev.device, &copy_info), "failed to copy host memory to device image", false)
		}
		return true;
	}
	
	// create device copy buffer
	auto copy_buffer = cqueue.get_context().create_buffer(cqueue, cpy_host_data,
														  MEMORY_FLAG::READ | MEMORY_FLAG::HOST_WRITE | MEMORY_FLAG::HEAP_ALLOCATION |
														  MEMORY_FLAG::VULKAN_HOST_COHERENT |
														  MEMORY_FLAG::VULKAN_MAY_USE_HOST_MEMORY);
	
	const auto& vk_queue = (const vulkan_queue&)cqueue;
	VK_CMD_BLOCK(vk_queue, "image write", ({
		// transition to dst-optimal, b/c of perf
		internal.transition(&vk_queue, block_cmd_buffer.cmd_buffer,
							VK_ACCESS_2_TRANSFER_WRITE_BIT,
							VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
							VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_2_TRANSFER_BIT);
		
		const VkCopyBufferToImageInfo2 info {
			.sType = VK_STRUCTURE_TYPE_COPY_BUFFER_TO_IMAGE_INFO_2,
			.pNext = nullptr,
			.srcBuffer = copy_buffer->get_underlying_vulkan_buffer_safe()->get_vulkan_buffer(),
			.dstImage = image,
			.dstImageLayout = internal.image_info.imageLayout,
			.regionCount = uint32_t(buffer_regions.size()),
			.pRegions = buffer_regions.data(),
		};
		vkCmdCopyBufferToImage2(block_cmd_buffer.cmd_buffer, &info);
		
		// transition back to previous
		if (has_flag<IMAGE_TYPE::READ>(image_type) && !has_flag<IMAGE_TYPE::WRITE>(image_type)) {
			internal.transition_read(&cqueue, block_cmd_buffer.cmd_buffer);
		} else {
			internal.transition_write(&cqueue, block_cmd_buffer.cmd_buffer);
		}
	}), true);
	
	// update mip-map chain
	if (generate_mip_maps) {
		generate_mip_map_chain(cqueue);
	}
	
	return true;
}

void* __attribute__((aligned(128))) vulkan_image::map(const device_queue& cqueue,
													  const MEMORY_MAP_FLAG flags_) {
	return vulkan_memory::map(cqueue, flags_, (image_type == shim_image_type ?
											   image_data_size : shim_image_data_size), 0);
}

bool vulkan_image::unmap(const device_queue& cqueue,
						 void* __attribute__((aligned(128))) mapped_ptr) {
	const auto iter = mappings.find(mapped_ptr);
	if(iter == mappings.end()) {
		log_error("invalid mapped pointer: $X", mapped_ptr);
		return false;
	}
	
	if (!vulkan_memory::unmap(cqueue, mapped_ptr)) {
		return false;
	}
	
	// if we transitioned to a transfer layout during mapping, transition back now
	auto& internal = (vulkan_image_internal&)*this;
	if (internal.image_info.imageLayout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL ||
		internal.image_info.imageLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
		if (has_flag<IMAGE_TYPE::READ>(image_type) && !has_flag<IMAGE_TYPE::WRITE>(image_type)) {
			internal.transition_read(&cqueue, nullptr);
		} else {
			internal.transition_write(&cqueue, nullptr);
		}
	}
	
	// manually create mip-map chain
	if(generate_mip_maps &&
	   (has_flag<MEMORY_MAP_FLAG::WRITE>(iter->second.flags) ||
		has_flag<MEMORY_MAP_FLAG::WRITE_INVALIDATE>(iter->second.flags))) {
		generate_mip_map_chain(cqueue);
	}
	
	return true;
}

std::vector<VkBufferImageCopy2> vulkan_image::build_image_buffer_copy_regions(const bool with_shim_type) const {
	const auto dim_count = image_dim_count(image_type);
	const bool is_compressed = image_compressed(image_type);
	std::vector<VkBufferImageCopy2> regions;
	regions.reserve(mip_level_count);
	uint64_t buffer_offset = 0;
	const auto img_type = (with_shim_type ? shim_image_type : image_type);
	apply_on_levels([this, &regions, &buffer_offset, &dim_count, &is_compressed, &img_type](const uint32_t& level,
																							const uint4& mip_image_dim,
																							const uint32_t&,
																							const uint32_t& level_data_size) {
		// NOTE: original size/type for non-3-channel types, and the 4-channel shim size/type for 3-channel types
		uint2 buffer_dim;
		if (is_compressed) {
			// for compressed formats, we need to consider the actual bits-per-pixel and full block size per row -> need to multiply with Y block size
			buffer_dim = image_compression_block_size(img_type);
			buffer_dim.max(mip_image_dim.xy);
		}
		
		regions.emplace_back(VkBufferImageCopy2 {
			.sType = VK_STRUCTURE_TYPE_BUFFER_IMAGE_COPY_2,
			.pNext = nullptr,
			.bufferOffset = buffer_offset,
			.bufferRowLength = (is_compressed ? buffer_dim.x : 0 /* tightly packed */),
			.bufferImageHeight = (is_compressed ? buffer_dim.y : 0 /* tightly packed */),
			.imageSubresource = {
				.aspectMask = vk_aspect_flags_from_type(img_type),
				.mipLevel = level,
				.baseArrayLayer = 0,
				.layerCount = layer_count,
			},
			.imageOffset = { 0, 0, 0 },
			.imageExtent = {
				std::max(mip_image_dim.x, 1u),
				dim_count >= 2 ? std::max(mip_image_dim.y, 1u) : 1,
				dim_count >= 3 ? std::max(mip_image_dim.z, 1u) : 1,
			},
		});
		buffer_offset += level_data_size;
		return true;
	}, img_type);
	return regions;
}

void vulkan_image::image_copy_dev_to_host(const device_queue& cqueue, VkCommandBuffer cmd_buffer, VkBuffer host_buffer) {
	// TODO: depth/stencil support
	if (image_type != shim_image_type) {
		throw std::runtime_error("dev -> host copy is unsupported when using a shim type (RGB)");
	}
	
	// transition to src-optimal, b/c of perf
	auto& internal = (vulkan_image_internal&)*this;
	internal.transition(&cqueue, cmd_buffer,
						VK_ACCESS_2_TRANSFER_READ_BIT,
						VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
						VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_2_TRANSFER_BIT);
	
	//
	const auto regions = build_image_buffer_copy_regions(false);
	const VkCopyImageToBufferInfo2 copy_info {
		.sType = VK_STRUCTURE_TYPE_COPY_IMAGE_TO_BUFFER_INFO_2,
		.pNext = nullptr,
		.srcImage = image,
		.srcImageLayout = internal.image_info.imageLayout,
		.dstBuffer = host_buffer,
		.regionCount = (uint32_t)regions.size(),
		.pRegions = regions.data(),
	};
	vkCmdCopyImageToBuffer2(cmd_buffer, &copy_info);
}

void vulkan_image::image_copy_host_to_dev(const device_queue& cqueue, VkCommandBuffer cmd_buffer, VkBuffer host_buffer, std::span<uint8_t> data) {
	// TODO: depth/stencil support
	auto& internal = (vulkan_image_internal&)*this;
	
	// transition to dst-optimal, b/c of perf
	internal.transition(&cqueue, cmd_buffer,
						VK_ACCESS_2_TRANSFER_WRITE_BIT,
						VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
						VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_2_TRANSFER_BIT);
	
	// RGB -> RGBA data conversion if necessary
	if (image_type != shim_image_type) {
		rgb_to_rgba_inplace(image_type, shim_image_type, data, generate_mip_maps);
	}
	
	//
	const auto regions = build_image_buffer_copy_regions(true);
	const VkCopyBufferToImageInfo2 info {
		.sType = VK_STRUCTURE_TYPE_COPY_BUFFER_TO_IMAGE_INFO_2,
		.pNext = nullptr,
		.srcBuffer = host_buffer,
		.dstImage = image,
		.dstImageLayout = internal.image_info.imageLayout,
		.regionCount = (uint32_t)regions.size(),
		.pRegions = regions.data(),
	};
	vkCmdCopyBufferToImage2(cmd_buffer, &info);
}

void vulkan_image::set_debug_label(const std::string& label) {
	device_memory::set_debug_label(label);
	set_vulkan_debug_label(vk_dev, VK_OBJECT_TYPE_IMAGE, uint64_t(image), label);
	if (image_view) {
		set_vulkan_debug_label(vk_dev, VK_OBJECT_TYPE_IMAGE_VIEW, uint64_t(image_view), label);
	}
}

const VkImageView& vulkan_image::get_vulkan_image_layer_view(const uint32_t layer_idx) {
	if (!has_flag<IMAGE_TYPE::FLAG_ARRAY>(image_type)) {
		log_error("image is not an image array");
		return image_view;
	} else if (layer_idx >= layer_count) {
		log_error("layer index $' is out-of-bounds (layer count: $')", layer_idx, layer_count);
		return image_view;
	}
	
	if (layer_image_views.empty()) {
		layer_image_views.resize(layer_count, nullptr);
	}
	
	auto& layer_image_view = layer_image_views[layer_idx];
	if (!layer_image_view) {
		// create the layer image view
		VkImageViewType view_type = VK_IMAGE_VIEW_TYPE_2D;
		switch (image_dim_count(image_type)) {
			case 1:
				view_type = VK_IMAGE_VIEW_TYPE_1D;
				break;
			case 2:
				view_type = (has_flag<IMAGE_TYPE::FLAG_CUBE>(image_type) ? VK_IMAGE_VIEW_TYPE_CUBE : VK_IMAGE_VIEW_TYPE_2D);
				break;
			case 3:
				view_type = VK_IMAGE_VIEW_TYPE_3D;
				break;
			default: floor_unreachable();
		}
		
		auto vulkan_dev = vk_dev.device;
		auto& internal = (vulkan_image_internal&)*this;
		VkImageAspectFlags aspect = vk_aspect_flags_from_type(image_type);
		
		const VkImageSubresourceRange sub_rsrc_range {
			.aspectMask = aspect,
			.baseMipLevel = 0,
			.levelCount = 1,
			.baseArrayLayer = layer_idx,
			.layerCount = 1,
		};
		const VkImageViewCreateInfo image_view_create_info {
			.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
			.pNext = nullptr,
			.flags = 0,
			.image = image,
			.viewType = view_type,
			.format = internal.vk_format,
			.components = {
				VK_COMPONENT_SWIZZLE_IDENTITY,
				VK_COMPONENT_SWIZZLE_IDENTITY,
				VK_COMPONENT_SWIZZLE_IDENTITY,
				VK_COMPONENT_SWIZZLE_IDENTITY
			},
			.subresourceRange = sub_rsrc_range,
		};
		VK_CALL_RET(vkCreateImageView(vulkan_dev, &image_view_create_info, nullptr, &layer_image_view),
					"image layer view creation failed", image_view /* fall back */)
	}
	
	return layer_image_view;
}

} // namespace fl

#endif
