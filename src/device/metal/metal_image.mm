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

#include <floor/device/metal/metal_image.hpp>

#if !defined(FLOOR_NO_METAL)

#include <floor/core/logger.hpp>
#include <floor/device/metal/metal_context.hpp>
#include <floor/device/metal/metal_buffer.hpp>
#include <floor/device/metal/metal_queue.hpp>
#include <floor/device/metal/metal_device.hpp>
#include <floor/device/metal/metal_fence.hpp>
#include <floor/device/device_context.hpp>
#include <floor/core/aligned_ptr.hpp>
#include <floor/floor.hpp>

namespace fl {

// TODO: proper error (return) value handling everywhere

metal_image::metal_image(const device_queue& cqueue,
						 const uint4 image_dim_,
						 const IMAGE_TYPE image_type_,
						 std::span<uint8_t> host_data_,
						 const MEMORY_FLAG flags_,
						 const uint32_t mip_level_limit_) :
device_image(cqueue, image_dim_, image_type_, host_data_, flags_, nullptr, true /* may need shim type */, mip_level_limit_) {
	const auto is_render_target = has_flag<IMAGE_TYPE::FLAG_RENDER_TARGET>(image_type);
	assert(!is_render_target || has_flag<MEMORY_FLAG::RENDER_TARGET>(flags));
	
	switch (flags & MEMORY_FLAG::READ_WRITE) {
		case MEMORY_FLAG::READ:
			usage_options = MTLTextureUsageShaderRead;
			break;
		case MEMORY_FLAG::WRITE:
			usage_options = MTLTextureUsageShaderWrite;
			break;
		case MEMORY_FLAG::READ_WRITE:
			usage_options = MTLTextureUsageShaderRead | MTLTextureUsageShaderWrite;
			break;
		default:
			if (is_render_target) {
				break;
			}
			// all possible cases handled
			floor_unreachable();
	}
	
	const auto shared_only = (floor::get_metal_shared_only_with_unified_memory() && dev.unified_memory);
	const auto is_transient = has_flag<IMAGE_TYPE::FLAG_TRANSIENT>(image_type);
	if (is_render_target) {
		usage_options |= MTLTextureUsageRenderTarget;
	}
	
	bool is_untracked = false;
	if (has_flag<MEMORY_FLAG::NO_RESOURCE_TRACKING>(flags) ||
		has_flag<DEVICE_CONTEXT_FLAGS::NO_RESOURCE_TRACKING>(dev.context->get_context_flags())) {
		options |= MTLResourceHazardTrackingModeUntracked;
		is_untracked = true;
	}
	
	const auto is_host_accessible = ((flags & MEMORY_FLAG::HOST_READ_WRITE) != MEMORY_FLAG::NONE);
	
	// for this to be put into a heap, the following must be fulfilled:
	//  * both heap flags must be enabled for this to be viable
	//  * the image must be untracked
	//  * the image must be in private or shared storage mode (!managed and !transient)
	// NOTE: we don't have any support for images being backed by host memory
	assert(!has_flag<MEMORY_FLAG::USE_HOST_MEMORY>(flags));
	if (has_flag<MEMORY_FLAG::__EXP_HEAP_ALLOC>(flags) &&
		has_flag<DEVICE_CONTEXT_FLAGS::__EXP_INTERNAL_HEAP>(dev.context->get_context_flags()) &&
		!is_transient && is_untracked && (dev.unified_memory || !is_host_accessible)) {
		// enable (for now), may get disabled in create_internal() if other conditions aren't met
		is_heap_image = true;
		// always use default
		options |= MTLCPUCacheModeDefaultCache;
	}
	
	// NOTE: same as metal_buffer:
	switch (flags & MEMORY_FLAG::HOST_READ_WRITE) {
		case MEMORY_FLAG::HOST_READ:
		case MEMORY_FLAG::HOST_READ_WRITE:
			// keep the default MTLCPUCacheModeDefaultCache
			break;
		case MEMORY_FLAG::HOST_WRITE:
			// host will only write -> can use write combined
			if (!shared_only && !is_heap_image) {
				options = MTLCPUCacheModeWriteCombined;
			}
			break;
		case MEMORY_FLAG::NONE:
			// don' set anything -> private storage will be set later
			break;
		// all possible cases handled
		default: floor_unreachable();
	}
	
	if (!is_host_accessible) {
		bool is_memory_less = false;
		if (is_render_target && is_transient) {
			options |= MTLResourceStorageModeMemoryless;
			storage_options = MTLStorageModeMemoryless;
			is_memory_less = true;
		}
		if (!is_memory_less) {
			// prefer to put render targets into private storage mode even if shared-only is set (but must not be heap-allocated)
			if (!shared_only || (is_render_target && !is_heap_image)) {
				options |= MTLResourceStorageModePrivate;
				storage_options = MTLStorageModePrivate;
			} else {
				options |= MTLResourceStorageModeShared;
				storage_options = MTLStorageModeShared;
			}
		}
	} else {
#if !defined(FLOOR_IOS) && !defined(FLOOR_VISIONOS)
		if (!dev.unified_memory) {
			options |= MTLResourceStorageModeManaged;
			storage_options = MTLStorageModeManaged;
		} else {
			// used shared storage when we have unified memory that needs to be accessed by both the CPU and GPU
			options |= MTLResourceStorageModeShared;
			storage_options = MTLStorageModeShared;
		}
#else
		options |= MTLResourceStorageModeShared;
		storage_options = MTLStorageModeShared;
#endif
	}
	
	// actually create the image
	if (!create_internal(true, cqueue)) {
		return; // can't do much else
	}
}

static uint4 compute_metal_image_dim(id <MTLTexture> floor_nonnull img) {
	// start with straightforward copy, some of these may be 0
	uint4 dim = { (uint32_t)[img width], (uint32_t)[img height], (uint32_t)[img depth], 0 };
	
	// set layer count for array formats
	const auto texture_type = [img textureType];
	const auto layer_count = (uint32_t)[img arrayLength];
	if (texture_type == MTLTextureType1DArray) {
		dim.y = layer_count;
	} else if (texture_type == MTLTextureType2DArray || texture_type == MTLTextureTypeCubeArray) {
		dim.z = layer_count;
	}
	
	return dim;
}

static IMAGE_TYPE compute_metal_image_type(id <MTLTexture> floor_nonnull img, const MEMORY_FLAG flags) {
	IMAGE_TYPE type { IMAGE_TYPE::NONE };
	
	// start with the base format
	switch ([img textureType]) {
		case MTLTextureType1D: type = IMAGE_TYPE::IMAGE_1D; break;
		case MTLTextureType1DArray: type = IMAGE_TYPE::IMAGE_1D_ARRAY; break;
		case MTLTextureType2D: type = IMAGE_TYPE::IMAGE_2D; break;
		case MTLTextureType2DArray: type = IMAGE_TYPE::IMAGE_2D_ARRAY; break;
		case MTLTextureType2DMultisample: type = IMAGE_TYPE::IMAGE_2D_MSAA; break;
		case MTLTextureType3D: type = IMAGE_TYPE::IMAGE_3D; break;
		case MTLTextureTypeCube: type = IMAGE_TYPE::IMAGE_CUBE; break;
		case MTLTextureTypeCubeArray: type = IMAGE_TYPE::IMAGE_CUBE_ARRAY; break;
		case MTLTextureType2DMultisampleArray: type = IMAGE_TYPE::IMAGE_2D_MSAA_ARRAY; break;
		case MTLTextureTypeTextureBuffer: type = IMAGE_TYPE::IMAGE_1D_BUFFER; break;
		
		// yay for forwards compatibility, or at least detecting that something is wrong(tm) ...
FLOOR_PUSH_WARNINGS()
FLOOR_IGNORE_WARNING(covered-switch-default)
		default:
			log_error("invalid or unknown Metal texture type: $X", [img textureType]);
			return IMAGE_TYPE::NONE;
FLOOR_POP_WARNINGS()
	}
	
	// handle the pixel format
	static const std::unordered_map<MTLPixelFormat, IMAGE_TYPE> format_lut {
		// R
		{ MTLPixelFormatR8Unorm, IMAGE_TYPE::R8UI_NORM },
		{ MTLPixelFormatR8Snorm, IMAGE_TYPE::R8I_NORM },
		{ MTLPixelFormatR8Uint, IMAGE_TYPE::R8UI },
		{ MTLPixelFormatR8Sint, IMAGE_TYPE::R8I },
		{ MTLPixelFormatR16Unorm, IMAGE_TYPE::R16UI_NORM },
		{ MTLPixelFormatR16Snorm, IMAGE_TYPE::R16I_NORM },
		{ MTLPixelFormatR16Uint, IMAGE_TYPE::R16UI },
		{ MTLPixelFormatR16Sint, IMAGE_TYPE::R16I },
		{ MTLPixelFormatR16Float, IMAGE_TYPE::R16F },
		{ MTLPixelFormatR32Uint, IMAGE_TYPE::R32UI },
		{ MTLPixelFormatR32Sint, IMAGE_TYPE::R32I },
		{ MTLPixelFormatR32Float, IMAGE_TYPE::R32F },
		// RG
		{ MTLPixelFormatRG8Unorm, IMAGE_TYPE::RG8UI_NORM },
		{ MTLPixelFormatRG8Snorm, IMAGE_TYPE::RG8I_NORM },
		{ MTLPixelFormatRG8Uint, IMAGE_TYPE::RG8UI },
		{ MTLPixelFormatRG8Sint, IMAGE_TYPE::RG8I },
		{ MTLPixelFormatRG16Unorm, IMAGE_TYPE::RG16UI_NORM },
		{ MTLPixelFormatRG16Snorm, IMAGE_TYPE::RG16I_NORM },
		{ MTLPixelFormatRG16Uint, IMAGE_TYPE::RG16UI },
		{ MTLPixelFormatRG16Sint, IMAGE_TYPE::RG16I },
		{ MTLPixelFormatRG16Float, IMAGE_TYPE::RG16F },
		{ MTLPixelFormatRG32Uint, IMAGE_TYPE::RG32UI },
		{ MTLPixelFormatRG32Sint, IMAGE_TYPE::RG32I },
		{ MTLPixelFormatRG32Float, IMAGE_TYPE::RG32F },
		// RGB
		{ MTLPixelFormatRG11B10Float, IMAGE_TYPE::RG11B10F },
		{ MTLPixelFormatRGB9E5Float, IMAGE_TYPE::RGB9E5F },
		// RGBA
		{ MTLPixelFormatRGBA8Unorm, IMAGE_TYPE::RGBA8UI_NORM },
		{ MTLPixelFormatRGBA8Snorm, IMAGE_TYPE::RGBA8I_NORM },
		{ MTLPixelFormatRGBA8Uint, IMAGE_TYPE::RGBA8UI },
		{ MTLPixelFormatRGBA8Sint, IMAGE_TYPE::RGBA8I },
		{ MTLPixelFormatRGBA16Unorm, IMAGE_TYPE::RGBA16UI_NORM },
		{ MTLPixelFormatRGBA16Snorm, IMAGE_TYPE::RGBA16I_NORM },
		{ MTLPixelFormatRGBA16Uint, IMAGE_TYPE::RGBA16UI },
		{ MTLPixelFormatRGBA16Sint, IMAGE_TYPE::RGBA16I },
		{ MTLPixelFormatRGBA16Float, IMAGE_TYPE::RGBA16F },
		{ MTLPixelFormatRGBA32Uint, IMAGE_TYPE::RGBA32UI },
		{ MTLPixelFormatRGBA32Sint, IMAGE_TYPE::RGBA32I },
		{ MTLPixelFormatRGBA32Float, IMAGE_TYPE::RGBA32F },
		// BGR(A)
		{ MTLPixelFormatBGRA8Unorm, IMAGE_TYPE::BGRA8UI_NORM },
		{ MTLPixelFormatBGR10A2Unorm, IMAGE_TYPE::A2BGR10UI_NORM },
		{ MTLPixelFormatBGR10_XR, IMAGE_TYPE::BGR10UI_NORM },
		{ MTLPixelFormatBGR10_XR_sRGB, IMAGE_TYPE::BGR10UI_NORM | IMAGE_TYPE::FLAG_SRGB },
		{ MTLPixelFormatBGRA10_XR, IMAGE_TYPE::BGRA10UI_NORM },
		{ MTLPixelFormatBGRA10_XR_sRGB, IMAGE_TYPE::BGRA10UI_NORM | IMAGE_TYPE::FLAG_SRGB },
		// sRGB
		{ MTLPixelFormatR8Unorm_sRGB, IMAGE_TYPE::R8UI_NORM | IMAGE_TYPE::FLAG_SRGB },
		{ MTLPixelFormatRG8Unorm_sRGB, IMAGE_TYPE::RG8UI_NORM | IMAGE_TYPE::FLAG_SRGB },
		{ MTLPixelFormatRGBA8Unorm_sRGB, IMAGE_TYPE::RGBA8UI_NORM | IMAGE_TYPE::FLAG_SRGB },
		{ MTLPixelFormatBGRA8Unorm_sRGB, IMAGE_TYPE::BGRA8UI_NORM | IMAGE_TYPE::FLAG_SRGB },
		// depth / depth+stencil
		{ MTLPixelFormatDepth32Float, (IMAGE_TYPE::FLOAT |
									   IMAGE_TYPE::CHANNELS_1 |
									   IMAGE_TYPE::FORMAT_32 |
									   IMAGE_TYPE::FLAG_DEPTH) },
		{ MTLPixelFormatDepth16Unorm, (IMAGE_TYPE::UINT |
									   IMAGE_TYPE::CHANNELS_1 |
									   IMAGE_TYPE::FORMAT_16 |
									   IMAGE_TYPE::FLAG_DEPTH) },
#if !defined(FLOOR_IOS) && !defined(FLOOR_VISIONOS) // macOS only
		{ MTLPixelFormatDepth24Unorm_Stencil8, (IMAGE_TYPE::UINT |
												IMAGE_TYPE::CHANNELS_2 |
												IMAGE_TYPE::FORMAT_24_8 |
												IMAGE_TYPE::FLAG_DEPTH |
												IMAGE_TYPE::FLAG_STENCIL) },
#endif
		{ MTLPixelFormatDepth32Float_Stencil8, (IMAGE_TYPE::FLOAT |
												IMAGE_TYPE::CHANNELS_2 |
												IMAGE_TYPE::FORMAT_32_8 |
												IMAGE_TYPE::FLAG_DEPTH |
												IMAGE_TYPE::FLAG_STENCIL) },
#if !defined(FLOOR_IOS) || defined(__IPHONE_16_4) || defined(FLOOR_VISIONOS)
		// BC formats
		{ MTLPixelFormatBC1_RGBA, IMAGE_TYPE::BC1_RGBA },
		{ MTLPixelFormatBC1_RGBA_sRGB, IMAGE_TYPE::BC1_RGBA_SRGB },
		{ MTLPixelFormatBC2_RGBA, IMAGE_TYPE::BC2_RGBA },
		{ MTLPixelFormatBC2_RGBA_sRGB, IMAGE_TYPE::BC2_RGBA_SRGB },
		{ MTLPixelFormatBC3_RGBA, IMAGE_TYPE::BC3_RGBA },
		{ MTLPixelFormatBC3_RGBA_sRGB, IMAGE_TYPE::BC3_RGBA_SRGB },
		{ MTLPixelFormatBC4_RUnorm, IMAGE_TYPE::BC4_RUI },
		{ MTLPixelFormatBC4_RSnorm, IMAGE_TYPE::BC4_RI },
		{ MTLPixelFormatBC5_RGUnorm, IMAGE_TYPE::BC5_RGUI },
		{ MTLPixelFormatBC5_RGSnorm, IMAGE_TYPE::BC5_RGI },
		{ MTLPixelFormatBC6H_RGBFloat, IMAGE_TYPE::BC6H_RGBHF },
		{ MTLPixelFormatBC6H_RGBUfloat, IMAGE_TYPE::BC6H_RGBUHF },
		{ MTLPixelFormatBC7_RGBAUnorm, IMAGE_TYPE::BC7_RGBA },
		{ MTLPixelFormatBC7_RGBAUnorm_sRGB, IMAGE_TYPE::BC7_RGBA_SRGB },
#endif
		// EAC/ETC formats
		{ MTLPixelFormatEAC_R11Unorm, IMAGE_TYPE::EAC_R11UI },
		{ MTLPixelFormatEAC_R11Snorm, IMAGE_TYPE::EAC_R11I },
		{ MTLPixelFormatEAC_RG11Unorm, IMAGE_TYPE::EAC_RG11UI },
		{ MTLPixelFormatEAC_RG11Snorm, IMAGE_TYPE::EAC_RG11I },
		{ MTLPixelFormatEAC_RGBA8, IMAGE_TYPE::EAC_RGBA8 },
		{ MTLPixelFormatEAC_RGBA8_sRGB, IMAGE_TYPE::EAC_RGBA8_SRGB },
		{ MTLPixelFormatETC2_RGB8, IMAGE_TYPE::ETC2_RGB8 },
		{ MTLPixelFormatETC2_RGB8_sRGB, IMAGE_TYPE::ETC2_RGB8_SRGB },
		{ MTLPixelFormatETC2_RGB8A1, IMAGE_TYPE::ETC2_RGB8A1 },
		{ MTLPixelFormatETC2_RGB8A1_sRGB, IMAGE_TYPE::ETC2_RGB8A1_SRGB },
		// ASTC formats
		{ MTLPixelFormatASTC_4x4_sRGB, IMAGE_TYPE::ASTC_4X4_SRGB },
		{ MTLPixelFormatASTC_4x4_LDR, IMAGE_TYPE::ASTC_4X4_LDR },
		{ MTLPixelFormatASTC_4x4_HDR, IMAGE_TYPE::ASTC_4X4_HDR },
#if !defined(FLOOR_VISIONOS)
		// PVRTC formats
		{ MTLPixelFormatPVRTC_RGB_2BPP, IMAGE_TYPE::PVRTC_RGB2 },
		{ MTLPixelFormatPVRTC_RGB_4BPP, IMAGE_TYPE::PVRTC_RGB4 },
		{ MTLPixelFormatPVRTC_RGBA_2BPP, IMAGE_TYPE::PVRTC_RGBA2 },
		{ MTLPixelFormatPVRTC_RGBA_4BPP, IMAGE_TYPE::PVRTC_RGBA4 },
		{ MTLPixelFormatPVRTC_RGB_2BPP_sRGB, IMAGE_TYPE::PVRTC_RGB2_SRGB },
		{ MTLPixelFormatPVRTC_RGB_4BPP_sRGB, IMAGE_TYPE::PVRTC_RGB4_SRGB },
		{ MTLPixelFormatPVRTC_RGBA_2BPP_sRGB, IMAGE_TYPE::PVRTC_RGBA2_SRGB },
		{ MTLPixelFormatPVRTC_RGBA_4BPP_sRGB, IMAGE_TYPE::PVRTC_RGBA4_SRGB },
#endif
	};
	const auto metal_format = format_lut.find([img pixelFormat]);
	if (metal_format == end(format_lut)) {
		log_error("unsupported image pixel format: $X", [img pixelFormat]);
		return IMAGE_TYPE::NONE;
	}
	type |= metal_format->second;
	
	// handle render target
	if (([img usage] & MTLTextureUsageRenderTarget) == MTLTextureUsageRenderTarget || [img isFramebufferOnly]) {
		type |= IMAGE_TYPE::FLAG_RENDER_TARGET;
		// NOTE: MEMORY_FLAG::RENDER_TARGET will be set automatically in the device_image constructor
	}
	
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
	
	// handle mip-mapping / MSAA (although both are possible with == 1 as well)
	if ([img mipmapLevelCount] > 1) {
		type |= IMAGE_TYPE::FLAG_MIPMAPPED;
	}
	const auto sample_count = [img sampleCount];
	if (sample_count > 1) {
		type |= IMAGE_TYPE::FLAG_MSAA;
		type |= image_sample_type_from_count((uint32_t)sample_count);
	}
	
	return type;
}

metal_image::metal_image(const device_queue& cqueue,
						 id <MTLTexture> floor_nonnull external_image,
						 std::span<uint8_t> host_data_,
						 const MEMORY_FLAG flags_) :
device_image(cqueue, compute_metal_image_dim(external_image), compute_metal_image_type(external_image, flags_),
			  host_data_, flags_, nullptr, false /* no shim type here */, 0u), image(external_image), is_external(true) {
	// device must match
	if(((const metal_device&)dev).device != [external_image device]) {
		log_error("specified Metal device does not match the device set in the external image");
		return;
	}
	
	// copy existing options
	options = [image cpuCacheMode];
	
	if (has_flag<MEMORY_FLAG::NO_RESOURCE_TRACKING>(flags) ||
		has_flag<DEVICE_CONTEXT_FLAGS::NO_RESOURCE_TRACKING>(dev.context->get_context_flags())) {
		options |= MTLResourceHazardTrackingModeUntracked;
	}
	
#if defined(FLOOR_IOS) || defined(FLOOR_VISIONOS)
FLOOR_PUSH_WARNINGS()
FLOOR_IGNORE_WARNING(switch) // MTLStorageModeManaged can't be handled on iOS
#endif
	
	switch([image storageMode]) {
		default:
		case MTLStorageModeShared:
			options |= MTLResourceStorageModeShared;
			break;
		case MTLStorageModePrivate:
			options |= MTLResourceStorageModePrivate;
			break;
#if !defined(FLOOR_IOS) && !defined(FLOOR_VISIONOS)
		case MTLStorageModeManaged:
			options |= MTLResourceStorageModeManaged;
			break;
#endif
	}
	
#if defined(FLOOR_IOS) || defined(FLOOR_VISIONOS)
FLOOR_POP_WARNINGS()
#endif
	
	usage_options = [image usage];
	storage_options = [image storageMode];
	
	// shim type is unnecessary here
	shim_image_type = image_type;
}

bool metal_image::create_internal(const bool copy_host_data, const device_queue& cqueue) {
	const auto& mtl_dev = (const metal_device&)cqueue.get_device();
	
	// should not be called under that condition, but just to be safe
	if(is_external) {
		log_error("image is external!");
		return false;
	}
	
	@autoreleasepool {
		// create an appropriate texture descriptor
		desc = [[MTLTextureDescriptor alloc] init];
		
		const auto dim_count = image_dim_count(image_type);
		const bool is_array = has_flag<IMAGE_TYPE::FLAG_ARRAY>(image_type);
		const bool is_cube = has_flag<IMAGE_TYPE::FLAG_CUBE>(image_type);
		const bool is_msaa = has_flag<IMAGE_TYPE::FLAG_MSAA>(image_type);
		const bool is_buffer = has_flag<IMAGE_TYPE::FLAG_BUFFER>(image_type);
		const bool is_compressed = image_compressed(image_type);
		auto mtl_device = mtl_dev.device;
		if (is_msaa && is_cube) {
			log_error("MSAA cube image is not supported");
			return false;
		}
		
		const MTLRegion region {
			.origin = { 0, 0, 0 },
			.size = {
				image_dim.x,
				dim_count >= 2 ? image_dim.y : 1,
				dim_count >= 3 ? image_dim.z : 1,
			}
		};
		[desc setWidth:region.size.width];
		[desc setHeight:region.size.height];
		[desc setDepth:region.size.depth];
		[desc setArrayLength:(!is_cube ? layer_count : layer_count / 6)];
		
		// type + nD handling
		MTLTextureType tex_type { MTLTextureType2D };
		switch (dim_count) {
			case 1:
				if (is_cube || is_msaa) {
					log_error("cube/MSAA is not supported for 1D images");
					return false;
				}
				
				if (!is_array && !is_buffer) {
					tex_type = MTLTextureType1D;
				} else if (is_array && !is_buffer) {
					tex_type = MTLTextureType1DArray;
				} else if (!is_array && is_buffer) {
					tex_type = MTLTextureTypeTextureBuffer;
				} else if (is_array && is_buffer) {
					log_error("1D array buffer image is not supported");
					return false;
				}
				break;
			case 2:
				if (is_buffer) {
					log_error("buffer is not supported for 2D images");
					return false;
				}
				
				if (!is_cube) {
					if (!is_msaa && !is_array) {
						tex_type = MTLTextureType2D;
					} else if (is_msaa && is_array) {
						tex_type = MTLTextureType2DMultisampleArray;
					} else if (is_msaa) {
						tex_type = MTLTextureType2DMultisample;
					} else if (is_array) {
						tex_type = MTLTextureType2DArray;
					}
				} else {
					if (!is_array) {
						tex_type = MTLTextureTypeCube;
					} else {
						tex_type = MTLTextureTypeCubeArray;
					}
				}
				break;
			case 3:
				if (is_array || is_cube || is_msaa || is_buffer) {
					log_error("array/cube/MSAA/buffer is not supported for 3D images");
					return false;
				}
				tex_type = MTLTextureType3D;
				break;
			default:
				log_error("invalid dim count: $", dim_count);
				return false;
		}
		[desc setTextureType:tex_type];
		
		// and now for the fun bit: pixel format conversion ...
		const auto metal_format = metal_pixel_format_from_image_type(image_type);
		if (!metal_format) {
			log_error("unsupported image format: $ ($X)", image_type_to_string(image_type), image_type);
			return false;
		}
		[desc setPixelFormat:*metal_format];
		
		// misc options
		[desc setMipmapLevelCount:mip_level_count];
		[desc setSampleCount:image_sample_count(image_type)];
		
		// usage/access options
		[desc setResourceOptions:options];
		[desc setStorageMode:storage_options]; // don't know why this needs to be set twice
		[desc setUsage:usage_options];
		
		// create the actual Metal image with the just created descriptor
		const auto is_private = (options & MTLResourceStorageModeMask) == MTLResourceStorageModePrivate;
		const auto is_shared = (options & MTLResourceStorageModeMask) == MTLResourceStorageModeShared;
		if (is_heap_image) {
			assert((is_private && mtl_dev.heap_private) || (is_shared && mtl_dev.heap_shared));
			image = [(is_private ? mtl_dev.heap_private : mtl_dev.heap_shared) newTextureWithDescriptor:desc];
			if (!image) {
#if defined(FLOOR_DEBUG)
				log_warn("failed to allocate heap image");
#endif
				is_heap_image = false;
			}
		}
		image = [mtl_device newTextureWithDescriptor:desc];
		
		// copy host memory to device if it is non-null and NO_INITIAL_COPY is not specified
		if (copy_host_data && host_data.data() != nullptr && !has_flag<MEMORY_FLAG::NO_INITIAL_COPY>(flags)) {
			// copy to device memory must go through a blit command
			id <MTLCommandBuffer> cmd_buffer = ((const metal_queue&)cqueue).make_command_buffer();
			id <MTLBlitCommandEncoder> blit_encoder = [cmd_buffer blitCommandEncoder];
			
			// NOTE: arrays/slices must be copied in per slice (for all else: there just is one slice)
			const size_t slice_count = layer_count;
			
			auto cpy_host_data = host_data;
			apply_on_levels([this, &cpy_host_data, &slice_count,
							 &is_compressed, &dim_count](const uint32_t& level,
														 const uint4& mip_image_dim,
														 const uint32_t& slice_data_size,
														 const uint32_t& level_data_size) {
				// NOTE: original size/type for non-3-channel types, and the 4-channel shim size/type for 3-channel types
				uint32_t bytes_per_row = std::max(mip_image_dim.x, 1u);
				if (is_compressed) {
					// for compressed formats, we need to consider the actual bits-per-pixel and full block size per row -> need to multiply with Y block size
					const auto block_size = image_compression_block_size(shim_image_type);
					// at small mip levels (< block size), we need to ensure that we actually upload the full block
					bytes_per_row = std::max(block_size.x, bytes_per_row);
					assert((bytes_per_row % block_size.x) == 0u && "image width must be a multiple of the underlying block size");
					bytes_per_row *= image_bits_per_pixel(shim_image_type) * block_size.y;
					bytes_per_row = (bytes_per_row + 7u) / 8u;
				} else {
					bytes_per_row *= image_bytes_per_pixel(shim_image_type);
				}
				
				auto data = cpy_host_data;
				std::unique_ptr<uint8_t[]> conv_data;
				if (image_type != shim_image_type) {
					// need to copy/convert the RGB host data to RGBA
					size_t conv_data_size = 0;
					std::tie(conv_data, conv_data_size) = rgb_to_rgba(image_type, shim_image_type, cpy_host_data, true /* ignore mip levels as we do this manually */);
					data = { conv_data.get(), conv_data_size };
				} else {
					assert(cpy_host_data.size_bytes() >= level_data_size);
				}
				
				const MTLRegion mipmap_region {
					.origin = { 0, 0, 0 },
					.size = {
						std::max(mip_image_dim.x, 1u),
						dim_count >= 2 ? std::max(mip_image_dim.y, 1u) : 1,
						dim_count >= 3 ? std::max(mip_image_dim.z, 1u) : 1,
					}
				};
				for (size_t slice = 0; slice < slice_count; ++slice) {
					auto slice_data = data.subspan(slice * slice_data_size, slice_data_size);
					[image replaceRegion:mipmap_region
							 mipmapLevel:level
								   slice:slice
							   withBytes:slice_data.data()
							 bytesPerRow:bytes_per_row
						   bytesPerImage:(is_compressed ? 0 : slice_data_size)];
				}
				
				// mip-level image data provided by user, advance pointer
				if (!generate_mip_maps) {
					cpy_host_data = cpy_host_data.subspan(level_data_size, cpy_host_data.size_bytes() - level_data_size);
				}
				
				return true;
			}, shim_image_type);
			
#if !defined(FLOOR_IOS) && !defined(FLOOR_VISIONOS)
			if ((storage_options & MTLResourceStorageModeMask) == MTLResourceStorageModeManaged) {
				[blit_encoder synchronizeResource:image];
			}
#endif
			
			// manually create the mip-map chain if this was specified
			if (is_mip_mapped && mip_level_count > 1 && generate_mip_maps) {
				// NOTE: can only generate mip-maps on-the-fly for uncompressed image data (compressed is not renderable)
				//       -> compressed + generate_mip_maps already fails at image creation
				[blit_encoder generateMipmapsForTexture:image];
			}
			
			[blit_encoder endEncoding];
			[cmd_buffer commit];
			[cmd_buffer waitUntilCompleted];
		}
		
		return true;
	}
}

metal_image::~metal_image() {
#if FLOOR_METAL_INTERNAL_MEM_TRACKING_DEBUGGING
	GUARD(metal_mem_tracking_lock);
	uint64_t pre_alloc_size = 0u;
	uint64_t image_alloc_size = 0u;
	if (image && !is_external && !has_flag<IMAGE_TYPE::FLAG_TRANSIENT>(image_type)) {
		pre_alloc_size = [((const metal_device&)dev).device currentAllocatedSize];
		image_alloc_size = [image allocatedSize];
		if (image_data_size > image_alloc_size) {
			log_error("image size $' > alloc size $'", image_data_size, image_alloc_size);
		}
	}
#endif
	@autoreleasepool {
		// kill the image
		if (image
#if TARGET_OS_SIMULATOR // in the simulator, doing this may lead to issues with external images
			&& !is_external
#endif
			) {
			[image setPurgeableState:MTLPurgeableStateEmpty];
		}
		image = nil;
		if (desc != nil) {
			desc = nil;
		}
	}
#if FLOOR_METAL_INTERNAL_MEM_TRACKING_DEBUGGING
	if (pre_alloc_size > 0) {
		std::this_thread::yield();
		const auto post_alloc_size = [((const metal_device&)dev).device currentAllocatedSize];
		const auto alloc_diff = (pre_alloc_size >= post_alloc_size ? pre_alloc_size - post_alloc_size : 0);
		if (alloc_diff != image_alloc_size && alloc_diff != const_math::round_next_multiple(image_alloc_size, 16384ull) &&
			image_alloc_size >= 16384u) {
			metal_mem_tracking_leak_total += image_alloc_size;
			log_error("buffer ($) dealloc mismatch: expected $' to have been freed, got $' -> leak total $'",
					  debug_label, image_alloc_size, alloc_diff, metal_mem_tracking_leak_total);
		}
	}
#endif
}

bool metal_image::blit_internal(const bool is_async, const device_queue& cqueue, device_image& src,
								const std::vector<const device_fence*>& wait_fences,
								const std::vector<device_fence*>& signal_fences) {
	if (image == nil) {
		return false;
	}
	
	if (!blit_check(cqueue, src)) {
		return false;
	}
	
	@autoreleasepool {
		auto src_image = ((const metal_image&)src).get_metal_image();
		if (!src_image) {
			log_error("blit: source Metal image is null");
			return false;
		}
		
		id <MTLCommandBuffer> cmd_buffer = ((const metal_queue&)cqueue).make_command_buffer();
		id <MTLBlitCommandEncoder> blit_encoder = [cmd_buffer blitCommandEncoder];
		const auto dim_count = image_dim_count(image_type);
		
		if (is_async) {
			for (const auto& fence : wait_fences) {
				[blit_encoder waitForFence:((const metal_fence*)fence)->get_metal_fence()];
			}
		}
		
		apply_on_levels<true /* blit all levels */>([this, &blit_encoder, &src_image, &dim_count](const uint32_t& level,
																								  const uint4& mip_image_dim,
																								  const uint32_t&,
																								  const uint32_t&) {
			const MTLSize mipmap_size {
				std::max(mip_image_dim.x, 1u),
				dim_count >= 2 ? std::max(mip_image_dim.y, 1u) : 1u,
				dim_count >= 3 ? std::max(mip_image_dim.z, 1u) : 1u,
			};
			for (size_t slice = 0; slice < layer_count; ++slice) {
				[blit_encoder copyFromTexture:floor_force_nonnull(src_image)
								  sourceSlice:slice
								  sourceLevel:level
								 sourceOrigin:{ 0, 0, 0 }
								   sourceSize:mipmap_size
									toTexture:image
							 destinationSlice:slice
							 destinationLevel:level
							destinationOrigin:{ 0, 0, 0 }];
			}
			
			return true;
		}, shim_image_type);
		
		// always optimize for GPU use
		[blit_encoder optimizeContentsForGPUAccess:image];
		
		if (is_async) {
			for (const auto& fence : signal_fences) {
				[blit_encoder updateFence:((const metal_fence*)fence)->get_metal_fence()];
			}
		}
		
		[blit_encoder endEncoding];
		[cmd_buffer commit];
		if (!is_async) {
			[cmd_buffer waitUntilCompleted];
		}
		
		return true;
	}
}

bool metal_image::blit(const device_queue& cqueue, device_image& src) {
	return blit_internal(false, cqueue, src, {}, {});
}

bool metal_image::blit_async(const device_queue& cqueue, device_image& src,
							 std::vector<const device_fence*>&& wait_fences,
							 std::vector<device_fence*>&& signal_fences) {
	return blit_internal(true, cqueue, src, wait_fences, signal_fences);
}

bool metal_image::zero(const device_queue& cqueue) {
	if(image == nil) return false;
	
	bool success = false;
	@autoreleasepool {
		const bool is_compressed = image_compressed(image_type);
		const auto dim_count = image_dim_count(image_type);
		if (!is_compressed && dim_count == 2 &&
			has_flag<IMAGE_TYPE::FLAG_RENDER_TARGET>(image_type) &&
			!has_flag<IMAGE_TYPE::FLAG_MIPMAPPED>(image_type)) {
			// create and run an empty render pass that clears the image values
			id <MTLCommandBuffer> cmd_buffer = ((const metal_queue&)cqueue).make_command_buffer();
			
			MTLRenderPassDescriptor* pass_desc = [MTLRenderPassDescriptor renderPassDescriptor];
			if (has_flag<IMAGE_TYPE::FLAG_DEPTH>(image_type)) {
				pass_desc.depthAttachment.texture = image;
				pass_desc.depthAttachment.loadAction = MTLLoadActionClear;
				pass_desc.depthAttachment.storeAction = MTLStoreActionStore;
				pass_desc.depthAttachment.clearDepth = 1.0;
			} else {
				pass_desc.colorAttachments[0].texture = image;
				pass_desc.colorAttachments[0].loadAction = MTLLoadActionClear;
				pass_desc.colorAttachments[0].storeAction = MTLStoreActionStore;
				pass_desc.colorAttachments[0].clearColor = MTLClearColorMake(0.0, 0.0, 0.0, 0.0);
			}
			if (has_flag<IMAGE_TYPE::FLAG_ARRAY>(image_type)) {
				pass_desc.renderTargetArrayLength = image_layer_count(image_dim, image_type);
			}
			auto encoder = [cmd_buffer renderCommandEncoderWithDescriptor:pass_desc];
			
			MTLViewport viewport {
				.originX = 0.0, .originY = 0.0, .width = double(image_dim.x), .height = double(image_dim.y), .znear = 0.0, .zfar = 1.0
			};
			[encoder setViewport:viewport];
			
			MTLScissorRect scissor_rect { .x = 0, .y = 0, .width = image_dim.x, .height = image_dim.y };
			[encoder setScissorRect:scissor_rect];
			
			[encoder endEncoding];
			[cmd_buffer commit];
			[cmd_buffer waitUntilCompleted];
		} else {
			// TODO: builtin kernel to zero an image (unless compressed)
			
			const auto bytes_per_slice = image_slice_data_size_from_types(image_dim, shim_image_type);
			
			auto zero_buffer = cqueue.get_device().context->create_buffer(cqueue, bytes_per_slice,
																		  MEMORY_FLAG::READ_WRITE |
																		  MEMORY_FLAG::NO_RESOURCE_TRACKING |
																		  MEMORY_FLAG::__EXP_HEAP_ALLOC);
			zero_buffer->set_debug_label("zero_buffer");
			auto mtl_zero_buffer = ((const metal_buffer&)*zero_buffer).get_metal_buffer();
			
			id <MTLCommandBuffer> cmd_buffer = ((const metal_queue&)cqueue).make_command_buffer();
			id <MTLBlitCommandEncoder> blit_encoder = [cmd_buffer blitCommandEncoder];
			
			[blit_encoder fillBuffer:mtl_zero_buffer range:NSRange { 0, bytes_per_slice } value:0u];
			
			success = apply_on_levels<true>([this, &mtl_zero_buffer, &blit_encoder, &dim_count, &is_compressed](const uint32_t& level,
																												const uint4& mip_image_dim,
																												const uint32_t& slice_data_size,
																												const uint32_t&) {
				const auto bytes_per_row = image_bytes_per_pixel(shim_image_type) * std::max(mip_image_dim.x, 1u);
				for (size_t slice = 0; slice < layer_count; ++slice) {
					const MTLSize copy_size {
						std::max(mip_image_dim.x, 1u),
						dim_count >= 2 ? std::max(mip_image_dim.y, 1u) : 1,
						dim_count >= 3 ? std::max(mip_image_dim.z, 1u) : 1,
					};
					[blit_encoder copyFromBuffer:mtl_zero_buffer
									sourceOffset:0
							   sourceBytesPerRow:(is_compressed ? 0 : bytes_per_row)
							 sourceBytesPerImage:slice_data_size
									  sourceSize:copy_size
									   toTexture:image
								destinationSlice:slice
								destinationLevel:level
							   destinationOrigin:MTLOrigin { 0, 0, 0 }];
				}
				return true;
			}, shim_image_type);
			
			[blit_encoder endEncoding];
			[cmd_buffer commit];
			[cmd_buffer waitUntilCompleted];
		}
	}
	
	return success;
}

void* floor_nullable __attribute__((aligned(128))) metal_image::map(const device_queue& cqueue,
																	const MEMORY_MAP_FLAG flags_) {
	if(image == nil) return nullptr;
	
	// TODO: parameter origin + region + layer
	const auto dim_count = image_dim_count(image_type);
	
	// TODO: image map check + move this check there:
	if((options & MTLResourceStorageModeMask) == MTLResourceStorageModePrivate) {
		log_error("can't map an image which is set to be inaccessible by the host!");
		return nullptr;
	}
	
	bool write_only = false;
	if(has_flag<MEMORY_MAP_FLAG::WRITE_INVALIDATE>(flags_)) {
		write_only = true;
	}
	else {
		switch(flags_ & MEMORY_MAP_FLAG::READ_WRITE) {
			case MEMORY_MAP_FLAG::READ:
				write_only = false;
				break;
			case MEMORY_MAP_FLAG::WRITE:
				write_only = true;
				break;
			case MEMORY_MAP_FLAG::READ_WRITE:
				write_only = false;
				break;
			case MEMORY_MAP_FLAG::NONE:
			default:
				log_error("neither read nor write flag set for image mapping!");
				return nullptr;
		}
	}
	
	// alloc host memory
	auto host_buffer = make_aligned_ptr<uint8_t>(image_data_size);
	std::span<uint8_t> host_buffer_span { host_buffer.get(), host_buffer.allocation_size() };
	
	// check if we need to copy the image from the device (in case READ was specified)
	if(!write_only) {
		// must finish up all current work before we can properly read from the current image
		cqueue.finish();
		
		// need to sync image (resource) before being able to read it
		if (metal_buffer::metal_resource_type_needs_sync(options)) {
			metal_buffer::sync_metal_resource(cqueue, image);
		}
		
		// copy image data to the host
		const bool is_compressed = image_compressed(image_type);
		
		aligned_ptr<uint8_t> host_shim_buffer;
		std::span<uint8_t> host_shim_buffer_span;
		if (image_type != shim_image_type) {
			host_shim_buffer = make_aligned_ptr<uint8_t>(shim_image_data_size);
			host_shim_buffer_span = { host_shim_buffer.get(), host_shim_buffer.allocation_size() };
		}
		
		uint8_t* cpy_host_ptr = (image_type != shim_image_type ? host_shim_buffer.get() : host_buffer.get());
		apply_on_levels([this, &cpy_host_ptr,
						 &dim_count, &is_compressed](const uint32_t& level,
													 const uint4& mip_image_dim,
													 const uint32_t& slice_data_size,
													 const uint32_t&) {
			const auto bytes_per_row = image_bytes_per_pixel(shim_image_type) * std::max(mip_image_dim.x, 1u);
			const MTLRegion mipmap_region {
				.origin = { 0, 0, 0 },
				.size = {
					std::max(mip_image_dim.x, 1u),
					dim_count >= 2 ? std::max(mip_image_dim.y, 1u) : 1,
					dim_count >= 3 ? std::max(mip_image_dim.z, 1u) : 1,
				}
			};
			for(size_t slice = 0; slice < layer_count; ++slice) {
				[image getBytes:cpy_host_ptr
					bytesPerRow:(is_compressed ? 0 : bytes_per_row)
				  bytesPerImage:slice_data_size
					 fromRegion:mipmap_region
					mipmapLevel:level
						  slice:slice];
				cpy_host_ptr += slice_data_size;
			}
			return true;
		}, shim_image_type);
		
		// convert to RGB + cleanup
		if (image_type != shim_image_type) {
			rgba_to_rgb(shim_image_type, image_type, host_shim_buffer_span, host_buffer_span);
		}
	}
	
	// need to remember how much we mapped and where (so the host->device write-back copies the right amount of bytes)
	auto ret_ptr = host_buffer.get();
	mappings.emplace(ret_ptr, metal_mapping { std::move(host_buffer), flags_, write_only });
	
	return ret_ptr;
}

bool metal_image::unmap(const device_queue& cqueue, void* floor_nullable __attribute__((aligned(128))) mapped_ptr) {
	if(image == nil) return false;
	if(mapped_ptr == nullptr) return false;
	
	// check if this is actually a mapped pointer (+get the mapped size)
	const auto iter = mappings.find(mapped_ptr);
	if(iter == mappings.end()) {
		log_error("invalid mapped pointer: $X", mapped_ptr);
		return false;
	}
	
	// check if we need to actually copy data back to the device (not the case if read-only mapping)
	bool success = true;
	if (has_flag<MEMORY_MAP_FLAG::WRITE>(iter->second.flags) ||
		has_flag<MEMORY_MAP_FLAG::WRITE_INVALIDATE>(iter->second.flags)) {
		@autoreleasepool {
			// copy host memory to device memory
			id <MTLCommandBuffer> cmd_buffer = ((const metal_queue&)cqueue).make_command_buffer();
			id <MTLBlitCommandEncoder> blit_encoder = [cmd_buffer blitCommandEncoder];
			const bool is_compressed = image_compressed(image_type);
			const auto dim_count = image_dim_count(image_type);
			
			// again, need to convert RGB to RGBA if necessary
			assert(iter->second.ptr.allocation_size() >= image_data_size);
			std::span<const uint8_t> host_buffer { (const uint8_t*)mapped_ptr, image_data_size };
			std::unique_ptr<uint8_t[]> host_shim_buffer;
			size_t host_shim_buffer_size = 0;
			if (image_type != shim_image_type) {
				std::tie(host_shim_buffer, host_shim_buffer_size) = rgb_to_rgba(image_type, shim_image_type, host_buffer);
			}
			
			auto cpy_host_data = (image_type != shim_image_type ?
								  std::span<const uint8_t> { host_shim_buffer.get(), host_shim_buffer_size } :
								  host_buffer);
			success = apply_on_levels([this, &cpy_host_data,
									   &dim_count, &is_compressed](const uint32_t& level,
																   const uint4& mip_image_dim,
																   const uint32_t& slice_data_size,
																   const uint32_t&) {
				const auto bytes_per_row = image_bytes_per_pixel(shim_image_type) * std::max(mip_image_dim.x, 1u);
				const MTLRegion mipmap_region {
					.origin = { 0, 0, 0 },
					.size = {
						std::max(mip_image_dim.x, 1u),
						dim_count >= 2 ? std::max(mip_image_dim.y, 1u) : 1,
						dim_count >= 3 ? std::max(mip_image_dim.z, 1u) : 1,
					}
				};
				for (size_t slice = 0; slice < layer_count; ++slice) {
					assert(cpy_host_data.size_bytes() >= slice_data_size);
					[image replaceRegion:mipmap_region
							 mipmapLevel:level
								   slice:slice
							   withBytes:cpy_host_data.data()
							 bytesPerRow:(is_compressed ? 0 : bytes_per_row)
						   bytesPerImage:slice_data_size];
					cpy_host_data = cpy_host_data.subspan(slice_data_size, cpy_host_data.size_bytes() - slice_data_size);
				}
				return true;
			}, shim_image_type);
			
#if !defined(FLOOR_IOS) && !defined(FLOOR_VISIONOS)
			if ((storage_options & MTLResourceStorageModeMask) == MTLResourceStorageModeManaged) {
				[blit_encoder synchronizeResource:image];
			}
#endif
			[blit_encoder endEncoding];
			[cmd_buffer commit];
			[cmd_buffer waitUntilCompleted];
			
			// cleanup
			host_shim_buffer = nullptr;
			
			// update mip-map chain
			if (generate_mip_maps) {
				generate_mip_map_chain(cqueue);
			}
		}
	}
	
	// free host memory again and remove the mapping
	mappings.erase(mapped_ptr);
	
	return success;
}

void metal_image::generate_mip_map_chain(const device_queue& cqueue) {
	@autoreleasepool {
		// nothing to do here
		if([image mipmapLevelCount] == 1) return;
		
		id <MTLCommandBuffer> cmd_buffer = ((const metal_queue&)cqueue).make_command_buffer();
		id <MTLBlitCommandEncoder> blit_encoder = [cmd_buffer blitCommandEncoder];
		[blit_encoder generateMipmapsForTexture:image];
		[blit_encoder endEncoding];
		[cmd_buffer commit];
		[cmd_buffer waitUntilCompleted];
	}
}

std::optional<MTLPixelFormat> metal_image::metal_pixel_format_from_image_type(const IMAGE_TYPE& image_type_) {
	static const std::unordered_map<IMAGE_TYPE, MTLPixelFormat> format_lut {
		// R
		{ IMAGE_TYPE::R8UI_NORM, MTLPixelFormatR8Unorm },
		{ IMAGE_TYPE::R8I_NORM, MTLPixelFormatR8Snorm },
		{ IMAGE_TYPE::R8UI, MTLPixelFormatR8Uint },
		{ IMAGE_TYPE::R8I, MTLPixelFormatR8Sint },
		{ IMAGE_TYPE::R16UI_NORM, MTLPixelFormatR16Unorm },
		{ IMAGE_TYPE::R16I_NORM, MTLPixelFormatR16Snorm },
		{ IMAGE_TYPE::R16UI, MTLPixelFormatR16Uint },
		{ IMAGE_TYPE::R16I, MTLPixelFormatR16Sint },
		{ IMAGE_TYPE::R16F, MTLPixelFormatR16Float },
		{ IMAGE_TYPE::R32UI, MTLPixelFormatR32Uint },
		{ IMAGE_TYPE::R32I, MTLPixelFormatR32Sint },
		{ IMAGE_TYPE::R32F, MTLPixelFormatR32Float },
		// RG
		{ IMAGE_TYPE::RG8UI_NORM, MTLPixelFormatRG8Unorm },
		{ IMAGE_TYPE::RG8I_NORM, MTLPixelFormatRG8Snorm },
		{ IMAGE_TYPE::RG8UI, MTLPixelFormatRG8Uint },
		{ IMAGE_TYPE::RG8I, MTLPixelFormatRG8Sint },
		{ IMAGE_TYPE::RG16UI_NORM, MTLPixelFormatRG16Unorm },
		{ IMAGE_TYPE::RG16I_NORM, MTLPixelFormatRG16Snorm },
		{ IMAGE_TYPE::RG16UI, MTLPixelFormatRG16Uint },
		{ IMAGE_TYPE::RG16I, MTLPixelFormatRG16Sint },
		{ IMAGE_TYPE::RG16F, MTLPixelFormatRG16Float },
		{ IMAGE_TYPE::RG32UI, MTLPixelFormatRG32Uint },
		{ IMAGE_TYPE::RG32I, MTLPixelFormatRG32Sint },
		{ IMAGE_TYPE::RG32F, MTLPixelFormatRG32Float },
		// RGB
		{ IMAGE_TYPE::RG11B10F, MTLPixelFormatRG11B10Float },
		{ IMAGE_TYPE::RGB9E5F, MTLPixelFormatRGB9E5Float },
		// RGB -> RGBA
		{ IMAGE_TYPE::RGB8UI_NORM, MTLPixelFormatRGBA8Unorm },
		{ IMAGE_TYPE::RGB8I_NORM, MTLPixelFormatRGBA8Snorm },
		{ IMAGE_TYPE::RGB8UI, MTLPixelFormatRGBA8Uint },
		{ IMAGE_TYPE::RGB8I, MTLPixelFormatRGBA8Sint },
		{ IMAGE_TYPE::RGB16UI_NORM, MTLPixelFormatRGBA16Unorm },
		{ IMAGE_TYPE::RGB16I_NORM, MTLPixelFormatRGBA16Snorm },
		{ IMAGE_TYPE::RGB16UI, MTLPixelFormatRGBA16Uint },
		{ IMAGE_TYPE::RGB16I, MTLPixelFormatRGBA16Sint },
		{ IMAGE_TYPE::RGB16F, MTLPixelFormatRGBA16Float },
		{ IMAGE_TYPE::RGB32UI, MTLPixelFormatRGBA32Uint },
		{ IMAGE_TYPE::RGB32I, MTLPixelFormatRGBA32Sint },
		{ IMAGE_TYPE::RGB32F, MTLPixelFormatRGBA32Float },
		// RGBA
		{ IMAGE_TYPE::RGBA8UI_NORM, MTLPixelFormatRGBA8Unorm },
		{ IMAGE_TYPE::RGBA8I_NORM, MTLPixelFormatRGBA8Snorm },
		{ IMAGE_TYPE::RGBA8UI, MTLPixelFormatRGBA8Uint },
		{ IMAGE_TYPE::RGBA8I, MTLPixelFormatRGBA8Sint },
		{ IMAGE_TYPE::RGBA16UI_NORM, MTLPixelFormatRGBA16Unorm },
		{ IMAGE_TYPE::RGBA16I_NORM, MTLPixelFormatRGBA16Snorm },
		{ IMAGE_TYPE::RGBA16UI, MTLPixelFormatRGBA16Uint },
		{ IMAGE_TYPE::RGBA16I, MTLPixelFormatRGBA16Sint },
		{ IMAGE_TYPE::RGBA16F, MTLPixelFormatRGBA16Float },
		{ IMAGE_TYPE::RGBA32UI, MTLPixelFormatRGBA32Uint },
		{ IMAGE_TYPE::RGBA32I, MTLPixelFormatRGBA32Sint },
		{ IMAGE_TYPE::RGBA32F, MTLPixelFormatRGBA32Float },
		// BGR(A)
		{ IMAGE_TYPE::BGRA8UI_NORM, MTLPixelFormatBGRA8Unorm },
		{ IMAGE_TYPE::A2BGR10UI_NORM, MTLPixelFormatBGR10A2Unorm },
		{ IMAGE_TYPE::BGR10UI_NORM, MTLPixelFormatBGR10_XR },
		{ IMAGE_TYPE::BGR10UI_NORM | IMAGE_TYPE::FLAG_SRGB, MTLPixelFormatBGR10_XR_sRGB },
		{ IMAGE_TYPE::BGRA10UI_NORM, MTLPixelFormatBGRA10_XR },
		{ IMAGE_TYPE::BGRA10UI_NORM | IMAGE_TYPE::FLAG_SRGB, MTLPixelFormatBGRA10_XR_sRGB },
		// sRGB
		{ IMAGE_TYPE::R8UI_NORM | IMAGE_TYPE::FLAG_SRGB, MTLPixelFormatR8Unorm_sRGB },
		{ IMAGE_TYPE::RG8UI_NORM | IMAGE_TYPE::FLAG_SRGB, MTLPixelFormatRG8Unorm_sRGB },
		{ IMAGE_TYPE::RGB8UI_NORM | IMAGE_TYPE::FLAG_SRGB, MTLPixelFormatRGBA8Unorm_sRGB }, // allow RGB -> RGBA
		{ IMAGE_TYPE::RGBA8UI_NORM | IMAGE_TYPE::FLAG_SRGB, MTLPixelFormatRGBA8Unorm_sRGB },
		{ IMAGE_TYPE::BGRA8UI_NORM | IMAGE_TYPE::FLAG_SRGB, MTLPixelFormatBGRA8Unorm_sRGB },
		// depth / depth+stencil
		{ (IMAGE_TYPE::FLOAT |
		   IMAGE_TYPE::CHANNELS_1 |
		   IMAGE_TYPE::FORMAT_32 |
		   IMAGE_TYPE::FLAG_DEPTH), MTLPixelFormatDepth32Float },
		{ (IMAGE_TYPE::UINT |
		   IMAGE_TYPE::CHANNELS_1 |
		   IMAGE_TYPE::FORMAT_16 |
		   IMAGE_TYPE::FLAG_DEPTH), MTLPixelFormatDepth16Unorm },
#if !defined(FLOOR_IOS) && !defined(FLOOR_VISIONOS) // macOS only
		{ (IMAGE_TYPE::UINT |
		   IMAGE_TYPE::CHANNELS_2 |
		   IMAGE_TYPE::FORMAT_24_8 |
		   IMAGE_TYPE::FLAG_DEPTH |
		   IMAGE_TYPE::FLAG_STENCIL), MTLPixelFormatDepth24Unorm_Stencil8 },
#endif
		{ (IMAGE_TYPE::FLOAT |
		   IMAGE_TYPE::CHANNELS_2 |
		   IMAGE_TYPE::FORMAT_32_8 |
		   IMAGE_TYPE::FLAG_DEPTH |
		   IMAGE_TYPE::FLAG_STENCIL), MTLPixelFormatDepth32Float_Stencil8 },
#if !defined(FLOOR_IOS) || defined(__IPHONE_16_4) || defined(FLOOR_VISIONOS)
		// BC formats
		{ IMAGE_TYPE::BC1_RGBA, MTLPixelFormatBC1_RGBA },
		{ IMAGE_TYPE::BC1_RGBA_SRGB, MTLPixelFormatBC1_RGBA_sRGB },
		{ IMAGE_TYPE::BC2_RGBA, MTLPixelFormatBC2_RGBA },
		{ IMAGE_TYPE::BC2_RGBA_SRGB, MTLPixelFormatBC2_RGBA_sRGB },
		{ IMAGE_TYPE::BC3_RGBA, MTLPixelFormatBC3_RGBA },
		{ IMAGE_TYPE::BC3_RGBA_SRGB, MTLPixelFormatBC3_RGBA_sRGB },
		{ IMAGE_TYPE::BC4_RUI, MTLPixelFormatBC4_RUnorm },
		{ IMAGE_TYPE::BC4_RI, MTLPixelFormatBC4_RSnorm },
		{ IMAGE_TYPE::BC5_RGUI, MTLPixelFormatBC5_RGUnorm },
		{ IMAGE_TYPE::BC5_RGI, MTLPixelFormatBC5_RGSnorm },
		{ IMAGE_TYPE::BC6H_RGBHF, MTLPixelFormatBC6H_RGBFloat },
		{ IMAGE_TYPE::BC6H_RGBUHF, MTLPixelFormatBC6H_RGBUfloat },
		{ IMAGE_TYPE::BC7_RGBA, MTLPixelFormatBC7_RGBAUnorm },
		{ IMAGE_TYPE::BC7_RGBA_SRGB, MTLPixelFormatBC7_RGBAUnorm_sRGB },
#endif
		// EAC/ETC formats
		{ IMAGE_TYPE::EAC_R11UI, MTLPixelFormatEAC_R11Unorm },
		{ IMAGE_TYPE::EAC_R11I, MTLPixelFormatEAC_R11Snorm },
		{ IMAGE_TYPE::EAC_RG11UI, MTLPixelFormatEAC_RG11Unorm },
		{ IMAGE_TYPE::EAC_RG11I, MTLPixelFormatEAC_RG11Snorm },
		{ IMAGE_TYPE::EAC_RGBA8, MTLPixelFormatEAC_RGBA8 },
		{ IMAGE_TYPE::EAC_RGBA8_SRGB, MTLPixelFormatEAC_RGBA8_sRGB },
		{ IMAGE_TYPE::ETC2_RGB8, MTLPixelFormatETC2_RGB8 },
		{ IMAGE_TYPE::ETC2_RGB8_SRGB, MTLPixelFormatETC2_RGB8_sRGB },
		{ IMAGE_TYPE::ETC2_RGB8A1, MTLPixelFormatETC2_RGB8A1 },
		{ IMAGE_TYPE::ETC2_RGB8A1_SRGB, MTLPixelFormatETC2_RGB8A1_sRGB },
		// ASTC formats
		{ IMAGE_TYPE::ASTC_4X4_SRGB, MTLPixelFormatASTC_4x4_sRGB },
		{ IMAGE_TYPE::ASTC_4X4_LDR, MTLPixelFormatASTC_4x4_LDR },
		{ IMAGE_TYPE::ASTC_4X4_HDR, MTLPixelFormatASTC_4x4_HDR },
#if !defined(FLOOR_VISIONOS)
		// PVRTC formats
		{ IMAGE_TYPE::PVRTC_RGB2, MTLPixelFormatPVRTC_RGB_2BPP },
		{ IMAGE_TYPE::PVRTC_RGB4, MTLPixelFormatPVRTC_RGB_4BPP },
		{ IMAGE_TYPE::PVRTC_RGBA2, MTLPixelFormatPVRTC_RGBA_2BPP },
		{ IMAGE_TYPE::PVRTC_RGBA4, MTLPixelFormatPVRTC_RGBA_4BPP },
		{ IMAGE_TYPE::PVRTC_RGB2_SRGB, MTLPixelFormatPVRTC_RGB_2BPP_sRGB },
		{ IMAGE_TYPE::PVRTC_RGB4_SRGB, MTLPixelFormatPVRTC_RGB_4BPP_sRGB },
		{ IMAGE_TYPE::PVRTC_RGBA2_SRGB, MTLPixelFormatPVRTC_RGBA_2BPP_sRGB },
		{ IMAGE_TYPE::PVRTC_RGBA4_SRGB, MTLPixelFormatPVRTC_RGBA_4BPP_sRGB },
#endif
		// TODO: special image formats, these are partially supported
	};
	const auto masked_image_type = (image_type_ & (IMAGE_TYPE::__DATA_TYPE_MASK |
												   IMAGE_TYPE::__CHANNELS_MASK |
												   IMAGE_TYPE::__COMPRESSION_MASK |
												   IMAGE_TYPE::__FORMAT_MASK |
												   IMAGE_TYPE::__LAYOUT_MASK |
												   IMAGE_TYPE::FLAG_NORMALIZED |
												   IMAGE_TYPE::FLAG_DEPTH |
												   IMAGE_TYPE::FLAG_STENCIL |
												   IMAGE_TYPE::FLAG_SRGB));
	const auto metal_pixel_format = format_lut.find(masked_image_type);
	if (metal_pixel_format == end(format_lut)) {
		return {};
	}
	return metal_pixel_format->second;
}

void* floor_nullable metal_image::get_metal_image_void_ptr() const {
	return (__bridge void*)image;
}

void metal_image::set_debug_label(const std::string& label) {
	device_memory::set_debug_label(label);
	if (image) {
		@autoreleasepool {
			image.label = [NSString stringWithUTF8String:debug_label.c_str()];
		}
	}
}

} // namespace fl

#endif
