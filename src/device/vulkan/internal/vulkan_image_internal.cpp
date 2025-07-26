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

#include <floor/core/essentials.hpp>

#if !defined(FLOOR_NO_VULKAN)
#include "vulkan_image_internal.hpp"
#include <floor/device/device_context.hpp>
#include <floor/device/vulkan/vulkan_queue.hpp>
#include <floor/device/vulkan/vulkan_device.hpp>
#include "vulkan_conversion.hpp"
#include "vulkan_heap.hpp"
#include <cassert>
#include <cstring>

namespace fl {

IMAGE_TYPE compute_vulkan_image_type(const external_vulkan_image_info& info, const MEMORY_FLAG flags) {
	IMAGE_TYPE type { IMAGE_TYPE::NONE };
	
	// start with the base format
	type |= (info.image_base_type & (IMAGE_TYPE::__DIM_MASK |
									 IMAGE_TYPE::__CHANNELS_MASK |
									 IMAGE_TYPE::FLAG_ARRAY |
									 IMAGE_TYPE::FLAG_BUFFER |
									 IMAGE_TYPE::FLAG_CUBE |
									 IMAGE_TYPE::FLAG_DEPTH |
									 IMAGE_TYPE::FLAG_STENCIL |
									 IMAGE_TYPE::FLAG_MSAA));
	
	// handle the pixel format
	const auto img_type = image_type_from_vulkan_format(info.format);
	if (!img_type) {
		log_error("unsupported image format: $X", info.format);
		return IMAGE_TYPE::NONE;
	}
	type |= *img_type;
	
	// check if this is a render target
	if ((info.access_mask & VK_ACCESS_2_INPUT_ATTACHMENT_READ_BIT) != 0 ||
		(info.access_mask & VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT) != 0 ||
		(info.access_mask & VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT) != 0 ||
		(info.access_mask & VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT) != 0 ||
		(info.access_mask & VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT) != 0 ||
		(info.access_mask & VK_ACCESS_2_COLOR_ATTACHMENT_READ_NONCOHERENT_BIT_EXT) != 0) {
		type |= IMAGE_TYPE::FLAG_RENDER_TARGET;
	}
	if (info.layout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL ||
		info.layout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL ||
		info.layout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL ||
		info.layout == VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL ||
		info.layout == VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_STENCIL_READ_ONLY_OPTIMAL) {
		type |= IMAGE_TYPE::FLAG_RENDER_TARGET;
	}
	// NOTE: MEMORY_FLAG::RENDER_TARGET will be set automatically in the device_image constructor
	
	// handle read/write flags
	if (has_flag<MEMORY_FLAG::READ>(flags)) {
		type |= IMAGE_TYPE::READ;
	}
	if (has_flag<MEMORY_FLAG::WRITE>(flags)) {
		type |= IMAGE_TYPE::WRITE;
	}
	if (!has_flag<MEMORY_FLAG::READ>(flags) &&
		!has_flag<MEMORY_FLAG::WRITE>(flags) &&
		!has_flag<IMAGE_TYPE::FLAG_RENDER_TARGET>(type)) {
		// assume read/write if no flags are set and this is not a render target
		type |= IMAGE_TYPE::READ_WRITE;
	}
	
	// TODO: handle/check mip-mapping
	// type |= IMAGE_TYPE::FLAG_MIPMAPPED;
	
	return type;
}

static VkPipelineStageFlags2 stage_mask_from_access(const VkAccessFlags2& access_mask_in, const VkPipelineStageFlags2& stage_mask_in,
													const bool is_compute_only) {
	switch (access_mask_in) {
		case VK_PIPELINE_STAGE_2_TRANSFER_BIT:
			return VK_PIPELINE_STAGE_2_TRANSFER_BIT;
		default: break;
	}
	if (is_compute_only && (stage_mask_in & VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT) != 0) {
		return ((stage_mask_in & ~VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT) |
				VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT |
				VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT);
	}
	return stage_mask_in;
}

vulkan_image_internal::vulkan_image_internal(const device_queue& cqueue_,
											 const uint4 image_dim_,
											 const IMAGE_TYPE image_type_,
											 std::span<uint8_t> host_data_,
											 const MEMORY_FLAG flags_,
											 const uint32_t mip_level_limit_) :
vulkan_image(cqueue_, image_dim_, image_type_, host_data_, flags_, mip_level_limit_) {
	const auto is_render_target = has_flag<IMAGE_TYPE::FLAG_RENDER_TARGET>(image_type);
	const auto is_transient = has_flag<IMAGE_TYPE::FLAG_TRANSIENT>(image_type);
	assert(!is_render_target || has_flag<MEMORY_FLAG::RENDER_TARGET>(flags));
	
	VkImageUsageFlags usage = 0;
	if (!is_transient) {
		switch (flags & MEMORY_FLAG::READ_WRITE) {
			case MEMORY_FLAG::READ:
				usage |= VK_IMAGE_USAGE_SAMPLED_BIT;
				break;
			case MEMORY_FLAG::WRITE:
				usage |= VK_IMAGE_USAGE_STORAGE_BIT;
				break;
			case MEMORY_FLAG::READ_WRITE:
				usage |= VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT;
				break;
			default:
				if (is_render_target) {
					break;
				}
				// all possible cases handled
				floor_unreachable();
		}
	} else {
		usage |= VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT;
	}
	
	if (is_render_target) {
		if (!has_flag<IMAGE_TYPE::FLAG_DEPTH>(image_type)) {
			usage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
		} else {
			usage |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
		}
		
		// if readable: allow use as an input attachment
		if (!is_transient && has_flag<IMAGE_TYPE::READ>(image_type)) {
			usage |= VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT;
		}
	}
	
	if (!is_transient) {
		// must be able to write to the image when mip-map generation is enabled
		if (generate_mip_maps) {
			usage |= VK_IMAGE_USAGE_STORAGE_BIT;
		}
		
		// always need this for now
		usage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
		usage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	}
	
	if (has_flag<MEMORY_FLAG::VULKAN_HOST_COHERENT>(flags) &&
		!vk_dev.has_device_host_coherent_opt_image_support) {
		log_error("device-local/host-coherent images are not supported by the Vulkan device");
		return;
	}
	
	// both heap flags must be enabled for this to be viable + must obviously not be backed by CPU memory
	const auto ctx_flags = dev.context->get_context_flags();
	if ((has_flag<MEMORY_FLAG::__EXP_HEAP_ALLOC>(flags) ||
		 has_flag<DEVICE_CONTEXT_FLAGS::__EXP_VULKAN_ALWAYS_HEAP>(ctx_flags)) &&
		!has_flag<MEMORY_FLAG::USE_HOST_MEMORY>(flags) &&
		// TODO: support sharing
		!has_flag<MEMORY_FLAG::VULKAN_SHARING>(flags) &&
		// TODO: support aliasing
		!has_flag<MEMORY_FLAG::VULKAN_ALIASING>(flags) &&
		// TODO: support transient
		!has_flag<IMAGE_TYPE::FLAG_TRANSIENT>(image_type) &&
		has_flag<DEVICE_CONTEXT_FLAGS::__EXP_INTERNAL_HEAP>(ctx_flags)) {
		is_heap_allocation = true;
	}
	
	// actually create the image
	if (!create_internal(true, cqueue_, usage)) {
		return; // can't do much else
	}
}

vulkan_image_internal::vulkan_image_internal(const device_queue& cqueue_, const external_vulkan_image_info& external_image_,
											 std::span<uint8_t> host_data_, const MEMORY_FLAG flags_) :
vulkan_image(cqueue_, external_image_, host_data_, flags_) {
	image_info.sampler = nullptr;
	image_info.imageView = image_view;
	image_info.imageLayout = external_image_.layout;
	vk_format = external_image_.format;
	if (shim_image_type != image_type) {
		throw std::runtime_error("shim image type is not supported for external Vulkan images");
	}
}

vulkan_image_internal::~vulkan_image_internal() {
	// nop?
}

bool vulkan_image_internal::create_internal(const bool copy_host_data, const device_queue& cqueue, const VkImageUsageFlags& usage) {
	const auto& vk_dev = (const vulkan_device&)cqueue.get_device();
	auto vulkan_dev = vk_dev.device;
	const auto dim_count = image_dim_count(image_type);
	const bool is_array = has_flag<IMAGE_TYPE::FLAG_ARRAY>(image_type);
	const bool is_cube = has_flag<IMAGE_TYPE::FLAG_CUBE>(image_type);
	const bool is_msaa = has_flag<IMAGE_TYPE::FLAG_MSAA>(image_type);
	const bool is_depth = has_flag<IMAGE_TYPE::FLAG_DEPTH>(image_type);
	//const bool is_compressed = image_compressed(image_type); // TODO: check incompatible usage
	const bool is_read_only = has_flag<IMAGE_TYPE::READ>(image_type) && !has_flag<IMAGE_TYPE::WRITE>(image_type);
	const bool is_render_target = has_flag<IMAGE_TYPE::FLAG_RENDER_TARGET>(image_type);
	const auto is_transient = has_flag<IMAGE_TYPE::FLAG_TRANSIENT>(image_type);
	const bool is_aliasing = has_flag<MEMORY_FLAG::VULKAN_ALIASING>(flags);
	
	// format conversion
	const auto vk_format_opt = vulkan_format_from_image_type(image_type);
	if (!vk_format_opt) {
		log_error("unsupported image format: $ ($X)", image_type_to_string(image_type), image_type);
		return false;
	}
	vk_format = *vk_format_opt;
	
	// dim handling
	const VkImageType vk_image_type = (dim_count == 1 ? VK_IMAGE_TYPE_1D :
									   dim_count == 2 ? VK_IMAGE_TYPE_2D : VK_IMAGE_TYPE_3D);
	const VkExtent3D extent {
		.width = image_dim.x,
		.height = dim_count >= 2 ? image_dim.y : 1,
		.depth = dim_count >= 3 ? image_dim.z : 1,
	};
	if(is_cube) {
		if(extent.width != extent.height) {
			log_error("cube map width and height must be equal");
			return false;
		}
	}
	
	// TODO: when using linear memory, can also use VK_IMAGE_LAYOUT_PREINITIALIZED here
	const VkImageLayout initial_layout = VK_IMAGE_LAYOUT_UNDEFINED;
	VkImageLayout final_layout = VK_IMAGE_LAYOUT_GENERAL;
	
	// TODO: handle render targets via additional image transfer?
	VkAccessFlags2 dst_access_flags = 0;
	if (is_render_target) {
		if(!is_depth) {
			final_layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
			dst_access_flags = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
		} else {
			final_layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
			dst_access_flags = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
		}
	}
	
	// TODO: might want VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT later on
	VkImageCreateFlags vk_create_flags = 0;
	if (is_cube) {
		vk_create_flags |= VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
	}
	if (is_aliasing) {
		vk_create_flags |= VK_IMAGE_CREATE_ALIAS_BIT;
	}
	
	// create the image
	const auto is_sharing = has_flag<MEMORY_FLAG::VULKAN_SHARING>(flags);
	VkExternalMemoryImageCreateInfo ext_create_info;
	if (is_sharing) {
		ext_create_info = {
			.sType = VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO,
			.pNext = nullptr,
#if defined(__WINDOWS__)
			.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT,
#else
			.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT,
#endif
		};
	}
	const auto is_concurrent_sharing = ((vk_dev.all_queue_family_index != vk_dev.compute_queue_family_index) &&
										!is_render_target);
	const auto is_aliased_array = (is_aliasing && is_array);
	const VkImageCreateInfo image_create_info {
		.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
		.pNext = (is_sharing ? &ext_create_info : nullptr),
		.flags = vk_create_flags,
		.imageType = vk_image_type,
		.format = vk_format,
		.extent = extent,
		.mipLevels = mip_level_count,
		.arrayLayers = layer_count,
		.samples = is_msaa ? sample_count_to_vulkan_sample_count(image_sample_count(image_type)) : VK_SAMPLE_COUNT_1_BIT,
		.tiling = VK_IMAGE_TILING_OPTIMAL, // TODO: might want linear as well later on?
		.usage = usage,
		.sharingMode = (is_concurrent_sharing ? VK_SHARING_MODE_CONCURRENT : VK_SHARING_MODE_EXCLUSIVE),
		.queueFamilyIndexCount = (is_concurrent_sharing ? uint32_t(vk_dev.queue_families.size()) : 0),
		.pQueueFamilyIndices = (is_concurrent_sharing ? vk_dev.queue_families.data() : nullptr),
		.initialLayout = initial_layout,
	};
	VkExportMemoryAllocateInfo export_alloc_info;
	if (!is_heap_allocation) {
		VK_CALL_RET(vkCreateImage(vulkan_dev, &image_create_info, nullptr, &image),
					"image creation failed", false)
		
		// aliased array: create images for each plane
		if (is_aliased_array) {
			const auto layer_count = image_layer_count(image_dim, image_type);
			image_aliased_layers.resize(layer_count, nullptr);
			
			auto image_layer_create_info = image_create_info;
			image_layer_create_info.arrayLayers = 1;
			image_layer_create_info.extent.depth = 1;
			for (uint32_t layer = 0; layer < layer_count; ++layer) {
				VK_CALL_RET(vkCreateImage(vulkan_dev, &image_layer_create_info, nullptr, &image_aliased_layers[layer]),
							"image layer creation failed", false)
			}
		}
		
		// export memory alloc info (if sharing is enabled)
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
			.pNext = (!is_aliasing ? &ded_req : nullptr),
			.memoryRequirements = {},
		};
		const VkImageMemoryRequirementsInfo2 mem_req_info {
			.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_REQUIREMENTS_INFO_2,
			.pNext = nullptr,
			.image = image,
		};
		vkGetImageMemoryRequirements2(vulkan_dev, &mem_req_info, &mem_req2);
		const auto is_dedicated = (!is_aliasing && (ded_req.prefersDedicatedAllocation || ded_req.requiresDedicatedAllocation));
		const auto& mem_req = mem_req2.memoryRequirements;
		allocation_size = mem_req.size;
		
		const VkMemoryDedicatedAllocateInfo ded_alloc_info {
			.sType = VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO,
			.pNext = nullptr,
			.image = image,
			.buffer = VK_NULL_HANDLE,
		};
		if (is_sharing && is_dedicated) {
			export_alloc_info.pNext = &ded_alloc_info;
		}
		const VkMemoryAllocateInfo alloc_info {
			.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
			.pNext = (is_sharing ? (void*)&export_alloc_info : (is_dedicated ? (void*)&ded_alloc_info : nullptr)),
			.allocationSize = allocation_size,
			.memoryTypeIndex = find_memory_type_index(mem_req.memoryTypeBits, true /* prefer device memory */,
													  is_sharing /* sharing requires device memory */,
													  false /* host-coherent is not required */),
		};
		VK_CALL_RET(vkAllocateMemory(vulkan_dev, &alloc_info, nullptr, &mem),
					"image allocation (" + std::to_string(allocation_size) + " bytes) failed", false)
		const VkBindImageMemoryInfo bind_info {
			.sType = VK_STRUCTURE_TYPE_BIND_IMAGE_MEMORY_INFO,
			.pNext = nullptr,
			.image = image,
			.memory = mem,
			.memoryOffset = 0,
		};
		VK_CALL_RET(vkBindImageMemory2(vulkan_dev, 1, &bind_info), "image allocation binding failed", false)
		
		// aliased array: back each layer
		if (is_aliased_array) {
			VkMemoryRequirements2 layer_mem_req2 {
				.sType = VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2,
				.pNext = nullptr,
				.memoryRequirements = {},
			};
			const VkImageMemoryRequirementsInfo2 layer_mem_req_info {
				.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_REQUIREMENTS_INFO_2,
				.pNext = nullptr,
				.image = image_aliased_layers[0],
			};
			vkGetImageMemoryRequirements2(vulkan_dev, &layer_mem_req_info, &layer_mem_req2);
			const auto& layer_mem_req = layer_mem_req2.memoryRequirements;
			
			const auto per_layer_size = layer_mem_req.size;
			std::vector<VkBindImageMemoryInfo> per_layer_bind_info(layer_count);
			for (uint32_t layer = 0; layer < layer_count; ++layer) {
				per_layer_bind_info[layer] = {
					.sType = VK_STRUCTURE_TYPE_BIND_IMAGE_MEMORY_INFO,
					.pNext = nullptr,
					.image = image_aliased_layers[layer],
					.memory = mem,
					.memoryOffset = per_layer_size * layer,
				};
			}
			VK_CALL_RET(vkBindImageMemory2(vulkan_dev, (uint32_t)per_layer_bind_info.size(), per_layer_bind_info.data()),
						"image layer allocation binding failed", false)
		}
	} else {
		// NOTE: if VMA fails to perform a heap allocation, it will automatically fall back to a dedicated allocation -> no fallback needed
		auto alloc = vk_dev.heap->create_image(image_create_info, flags, image_type);
		if (!alloc.is_valid()) {
			log_error("image heap creation failed");
			return false;
		}
		image = alloc.image;
		heap_allocation = alloc.allocation;
		mem = alloc.memory;
		allocation_size = alloc.allocation_size;
		is_heap_allocation_host_visible = alloc.is_host_visible;
	}
	
	// create the view
	VkImageViewType view_type = VK_IMAGE_VIEW_TYPE_2D;
	switch(dim_count) {
		case 1:
			view_type = (is_array ? VK_IMAGE_VIEW_TYPE_1D_ARRAY : VK_IMAGE_VIEW_TYPE_1D);
			break;
		case 2:
			if(!is_cube) {
				view_type = (is_array ? VK_IMAGE_VIEW_TYPE_2D_ARRAY : VK_IMAGE_VIEW_TYPE_2D);
			}
			else {
				view_type = (is_array ? VK_IMAGE_VIEW_TYPE_CUBE_ARRAY : VK_IMAGE_VIEW_TYPE_CUBE);
			}
			break;
		case 3:
			view_type = VK_IMAGE_VIEW_TYPE_3D;
			break;
		default: floor_unreachable();
	}
	
	VkImageAspectFlags aspect = vk_aspect_flags_from_type(image_type);
	const VkImageSubresourceRange sub_rsrc_range {
		.aspectMask = aspect,
		.baseMipLevel = 0,
		.levelCount = mip_level_count,
		.baseArrayLayer = 0,
		.layerCount = layer_count,
	};
	
	const VkImageViewCreateInfo image_view_create_info {
		.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
		.pNext = nullptr,
		.flags = 0,
		.image = image,
		.viewType = view_type,
		.format = vk_format,
		.components = {
			VK_COMPONENT_SWIZZLE_IDENTITY,
			VK_COMPONENT_SWIZZLE_IDENTITY,
			VK_COMPONENT_SWIZZLE_IDENTITY,
			VK_COMPONENT_SWIZZLE_IDENTITY
		},
		.subresourceRange = sub_rsrc_range,
	};
	VK_CALL_RET(vkCreateImageView(vulkan_dev, &image_view_create_info, nullptr, &image_view),
				"image view creation failed", false)
	
	// transition to general layout or attachment layout (if render target)
	cur_access_mask = 0;
	image_info.imageLayout = initial_layout;
	const auto transition_stage = (is_render_target ? VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT : VK_PIPELINE_STAGE_2_HOST_BIT);
	transition(&cqueue, nullptr, dst_access_flags, final_layout, transition_stage, transition_stage);
	
	// update image desc info
	image_info.sampler = nullptr;
	image_info.imageView = image_view;
	image_info.imageLayout = final_layout;
	
	// if mip-mapping is enabled and the image is writable or mip-maps should be generated,
	// we need to create a per-level image view, so that functions can actually write to each mip-map level
	// (Vulkan doesn't support this at this point, although SPIR-V does)
	if(is_mip_mapped && (generate_mip_maps || has_flag<IMAGE_TYPE::WRITE>(image_type))) {
		mip_map_image_info.resize(dev.max_mip_levels);
		mip_map_image_view.resize(dev.max_mip_levels);
		const auto last_level = mip_level_count - 1;
		for(uint32_t i = 0; i < dev.max_mip_levels; ++i) {
			mip_map_image_info[i].sampler = nullptr;
			
			// fill unused views with the last level view
			if(i > last_level) {
				mip_map_image_view[i] = mip_map_image_view[last_level];
				mip_map_image_info[i].imageView = mip_map_image_view[last_level];
				continue;
			}
			
			// create a view of a single mip level
			const VkImageSubresourceRange mip_sub_rsrc_range {
				.aspectMask = aspect,
				.baseMipLevel = i,
				.levelCount = 1,
				.baseArrayLayer = 0,
				.layerCount = layer_count,
			};
			
			const VkImageViewCreateInfo mip_image_view_create_info {
				.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
				.pNext = nullptr,
				.flags = 0,
				.image = image,
				.viewType = view_type,
				.format = vk_format,
				.components = {
					VK_COMPONENT_SWIZZLE_IDENTITY,
					VK_COMPONENT_SWIZZLE_IDENTITY,
					VK_COMPONENT_SWIZZLE_IDENTITY,
					VK_COMPONENT_SWIZZLE_IDENTITY
				},
				.subresourceRange = mip_sub_rsrc_range,
			};
			VK_CALL_RET(vkCreateImageView(vulkan_dev, &mip_image_view_create_info, nullptr, &mip_map_image_view[i]),
						"mip-map image view creation failed", false)
			mip_map_image_info[i].imageView = mip_map_image_view[i];
		}
	}
	else {
		mip_map_image_info.resize(dev.max_mip_levels, image_info);
		mip_map_image_view.resize(dev.max_mip_levels, image_view);
	}
	update_mip_map_info();
	
	// query descriptor data
	descriptor_sampled_size = vk_dev.desc_buffer_sizes.sampled_image;
	descriptor_storage_size = vk_dev.desc_buffer_sizes.storage_image * mip_map_image_view.size();
	descriptor_data_sampled = std::make_unique<uint8_t[]>(descriptor_sampled_size);
	descriptor_data_storage = std::make_unique<uint8_t[]>(descriptor_storage_size);
	
	// while not explicitly forbidden, we should not query the descriptor info of transient images
	if (!is_transient) {
		const VkDescriptorImageInfo desc_img_info {
			.sampler = VK_NULL_HANDLE,
			.imageView = image_info.imageView,
			.imageLayout = VK_IMAGE_LAYOUT_GENERAL,
		};
		
		const VkDescriptorGetInfoEXT desc_info_sampled {
			.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_GET_INFO_EXT,
			.pNext = nullptr,
			.type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
			.data = {
				.pSampledImage = &desc_img_info,
			},
		};
		vkGetDescriptorEXT(vulkan_dev, &desc_info_sampled, vk_dev.desc_buffer_sizes.sampled_image, descriptor_data_sampled.get());
		
		for (size_t mip_level = 0, level_count = mip_map_image_view.size(); mip_level < level_count; ++mip_level) {
			const VkDescriptorImageInfo mm_desc_img_info {
				.sampler = VK_NULL_HANDLE,
				.imageView = mip_map_image_view[mip_level],
				.imageLayout = VK_IMAGE_LAYOUT_GENERAL,
			};
			
			const VkDescriptorGetInfoEXT mm_desc_info_storage {
				.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_GET_INFO_EXT,
				.pNext = nullptr,
				.type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
				.data = {
					.pStorageImage = &mm_desc_img_info,
				},
			};
			vkGetDescriptorEXT(vulkan_dev, &mm_desc_info_storage, vk_dev.desc_buffer_sizes.storage_image,
							   descriptor_data_storage.get() + mip_level * vk_dev.desc_buffer_sizes.storage_image);
		}
	} else {
		memset(descriptor_data_sampled.get(), 0, descriptor_sampled_size);
		if (descriptor_storage_size > 0) {
			memset(descriptor_data_storage.get(), 0, descriptor_storage_size);
		}
	}
	
	// buffer init from host data pointer
	if (copy_host_data &&
		host_data.data() != nullptr &&
		!has_flag<MEMORY_FLAG::NO_INITIAL_COPY>(flags)) {
		if (is_render_target) {
			log_error("can't initialize a render target with host data!");
		} else {
			if (!write_memory_data(cqueue, host_data, 0,
								   (shim_image_type != image_type ? shim_image_data_size : 0),
								   "failed to initialize image with host data (map failed)")) {
				return false;
			}
		}
	}
	
	// manually create mip-map chain
	if(generate_mip_maps) {
		generate_mip_map_chain(cqueue);
	}
	
	// transition image to its defined usage (render targets already have been transitioned)
	if (!is_render_target) {
		if (is_read_only) {
			transition_read(&cqueue, nullptr);
		} else {
			transition_write(&cqueue, nullptr);
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
	
	return false;
}

std::pair<bool, VkImageMemoryBarrier2> vulkan_image_internal::transition(const device_queue* cqueue,
																		 VkCommandBuffer cmd_buffer_,
																		 const VkAccessFlags2 dst_access,
																		 const VkImageLayout new_layout,
																		 const VkPipelineStageFlags2 src_stage_mask_in,
																		 const VkPipelineStageFlags2 dst_stage_mask_in,
																		 const bool soft_transition) {
	VkImageAspectFlags aspect_mask = vk_aspect_flags_from_type(image_type);
	const auto is_compute_only = (cqueue && cqueue->get_queue_type() == device_queue::QUEUE_TYPE::COMPUTE);
	VkPipelineStageFlags2 src_stage_mask = stage_mask_from_access(cur_access_mask, src_stage_mask_in, is_compute_only);
	VkPipelineStageFlags2 dst_stage_mask = stage_mask_from_access(dst_access, dst_stage_mask_in, is_compute_only);
	
	const VkImageMemoryBarrier2 image_barrier {
		.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
		.pNext = nullptr,
		.srcStageMask = src_stage_mask,
		.srcAccessMask = cur_access_mask,
		.dstStageMask = dst_stage_mask,
		.dstAccessMask = dst_access,
		.oldLayout = image_info.imageLayout,
		.newLayout = new_layout,
		.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
		.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
		.image = image,
		.subresourceRange = {
			.aspectMask = aspect_mask,
			.baseMipLevel = 0,
			.levelCount = mip_level_count,
			.baseArrayLayer = 0,
			.layerCount = layer_count,
		},
	};
	
	if (!soft_transition) {
		assert(cqueue);
		const VkDependencyInfo dep_info {
			.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
			.pNext = nullptr,
			.dependencyFlags = 0,
			.memoryBarrierCount = 0,
			.pMemoryBarriers = nullptr,
			.bufferMemoryBarrierCount = 0,
			.pBufferMemoryBarriers = nullptr,
			.imageMemoryBarrierCount = 1,
			.pImageMemoryBarriers = &image_barrier,
		};
		if (cmd_buffer_ == nullptr) {
			const auto& vk_queue = *(const vulkan_queue*)cqueue;
			VK_CMD_BLOCK(vk_queue, "image transition", ({
				vkCmdPipelineBarrier2(block_cmd_buffer.cmd_buffer, &dep_info);
			}), true /* always blocking */);
		} else {
			vkCmdPipelineBarrier2(cmd_buffer_, &dep_info);
		}
	}
	// else: soft transition: don't actually encode a pipeline barrier (must be done manually by the caller)
	
	cur_access_mask = dst_access;
	image_info.imageLayout = new_layout;
	update_mip_map_info();
	
	return { true, image_barrier };
}

std::pair<bool, VkImageMemoryBarrier2> vulkan_image_internal::transition_read(const device_queue* cqueue,
																			  VkCommandBuffer cmd_buffer,
																			  const bool allow_general_layout,
																			  const bool soft_transition) {
	// normal images
	if (!has_flag<IMAGE_TYPE::FLAG_RENDER_TARGET>(image_type)) {
		const VkAccessFlags2 access_flags = VK_ACCESS_2_SHADER_READ_BIT;
		if ((cur_access_mask & access_flags) == access_flags) {
			if (image_info.imageLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL ||
				(allow_general_layout && image_info.imageLayout == VK_IMAGE_LAYOUT_GENERAL)) {
				return { false, {} };
			}
		}
		return transition(cqueue, cmd_buffer, access_flags, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
						  VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT, VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT,
						  soft_transition);
	}
	// attachments / render-targets
	else {
		VkImageLayout layout;
		VkAccessFlags2 access_flags;
		if (!has_flag<IMAGE_TYPE::FLAG_DEPTH>(image_type)) {
			layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			access_flags = VK_ACCESS_2_SHADER_READ_BIT;
		} else {
			layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
			access_flags = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
		}
		if ((cur_access_mask & access_flags) == access_flags) {
			if (image_info.imageLayout == layout ||
				(allow_general_layout && image_info.imageLayout == VK_IMAGE_LAYOUT_GENERAL)) {
				return { false, {} };
			}
		}
		
		return transition(cqueue, cmd_buffer, access_flags, layout,
						  VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT, VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT,
						  soft_transition);
	}
}

std::pair<bool, VkImageMemoryBarrier2> vulkan_image_internal::transition_write(const device_queue* cqueue, VkCommandBuffer cmd_buffer,
																			   const bool read_write, const bool is_rt_direct_write,
																			   const bool allow_general_layout, const bool soft_transition) {
	// normal images
	if (!has_flag<IMAGE_TYPE::FLAG_RENDER_TARGET>(image_type) || is_rt_direct_write) {
		VkAccessFlags2 access_flags = VK_ACCESS_2_SHADER_WRITE_BIT;
		if (read_write) {
			access_flags |= VK_ACCESS_2_SHADER_READ_BIT;
		}
		
		if (image_info.imageLayout == VK_IMAGE_LAYOUT_GENERAL &&
			(cur_access_mask & access_flags) == access_flags) {
			return { false, {} };
		}
		return transition(cqueue, cmd_buffer, access_flags, VK_IMAGE_LAYOUT_GENERAL,
						  VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT, VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT,
						  soft_transition);
	}
	// attachments / render-targets
	else {
#if defined(FLOOR_DEBUG)
		if (read_write) {
			log_error("attachment / render-target can't be read-write");
		}
#endif
		
		VkImageLayout layout;
		VkAccessFlags2 access_flags;
		if (!has_flag<IMAGE_TYPE::FLAG_DEPTH>(image_type)) {
			layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
			access_flags = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
		} else {
			layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
			access_flags = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
		}
		if ((cur_access_mask & access_flags) == access_flags) {
			if (image_info.imageLayout == layout ||
				(allow_general_layout && image_info.imageLayout == VK_IMAGE_LAYOUT_GENERAL)) {
				return { false, {} };
			}
		}
		
		return transition(cqueue, cmd_buffer, access_flags, layout,
						  VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT, VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT,
						  soft_transition);
	}
}

void vulkan_image_internal::update_with_external_vulkan_state(const VkImageLayout& layout, const VkAccessFlags2& access) {
	image_info.imageLayout = layout;
	cur_access_mask = access;
	update_mip_map_info();
}

void vulkan_image_internal::update_mip_map_info() {
	// NOTE: sampler is always nullptr, imageView is always the same, so we only need to update the current layout here
	for(auto& info : mip_map_image_info) {
		info.imageLayout = image_info.imageLayout;
	}
}

} // namespace fl

#endif // FLOOR_NO_VULKAN
