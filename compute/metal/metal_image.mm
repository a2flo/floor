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

#include <floor/compute/metal/metal_image.hpp>

#if !defined(FLOOR_NO_METAL)

#include <floor/core/logger.hpp>
#include <floor/compute/metal/metal_buffer.hpp>
#include <floor/compute/metal/metal_queue.hpp>
#include <floor/compute/metal/metal_device.hpp>
#include <floor/compute/compute_context.hpp>
#include <floor/core/aligned_ptr.hpp>

// TODO: proper error (return) value handling everywhere

metal_image::metal_image(const compute_queue& cqueue,
						 const uint4 image_dim_,
						 const COMPUTE_IMAGE_TYPE image_type_,
						 void* floor_nullable host_ptr_,
						 const COMPUTE_MEMORY_FLAG flags_,
						 const uint32_t opengl_type_,
						 const uint32_t external_gl_object_,
						 const opengl_image_info* floor_nullable gl_image_info) :
compute_image(cqueue, image_dim_, image_type_, host_ptr_, flags_,
			  opengl_type_, external_gl_object_, gl_image_info) {
	// introduced with os x 10.11 / ios 9.0, kernel/shader access flags can actually be set
	switch(flags & COMPUTE_MEMORY_FLAG::READ_WRITE) {
		case COMPUTE_MEMORY_FLAG::READ:
			usage_options = MTLTextureUsageShaderRead;
			break;
		case COMPUTE_MEMORY_FLAG::WRITE:
			usage_options = MTLTextureUsageShaderWrite;
			break;
		case COMPUTE_MEMORY_FLAG::READ_WRITE:
			usage_options = MTLTextureUsageShaderRead | MTLTextureUsageShaderWrite;
			break;
		// all possible cases handled
		default: floor_unreachable();
	}
	
	if(has_flag<COMPUTE_IMAGE_TYPE::FLAG_RENDER_TARGET>(image_type)) {
		usage_options |= MTLTextureUsageRenderTarget;
	}
	
	// NOTE: same as metal_buffer:
	switch(flags & COMPUTE_MEMORY_FLAG::HOST_READ_WRITE) {
		case COMPUTE_MEMORY_FLAG::HOST_READ:
		case COMPUTE_MEMORY_FLAG::HOST_READ_WRITE:
			// keep the default MTLCPUCacheModeDefaultCache
			break;
		case COMPUTE_MEMORY_FLAG::HOST_WRITE:
			// host will only write -> can use write combined
			options = MTLCPUCacheModeWriteCombined;
			break;
		case COMPUTE_MEMORY_FLAG::NONE:
			// don' set anything -> private storage will be set later
			break;
		// all possible cases handled
		default: floor_unreachable();
	}
	
	if((flags & COMPUTE_MEMORY_FLAG::HOST_READ_WRITE) == COMPUTE_MEMORY_FLAG::NONE) {
		options |= MTLResourceStorageModePrivate;
		storage_options = MTLStorageModePrivate;
	} else {
#if !defined(FLOOR_IOS)
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
	if(!create_internal(true, cqueue)) {
		return; // can't do much else
	}
}

static uint4 compute_metal_image_dim(id <MTLTexture> floor_nonnull img) {
	// start with straightforward copy, some of these may be 0
	uint4 dim = { (uint32_t)[img width], (uint32_t)[img height], (uint32_t)[img depth], 0 };
	
	// set layer count for array formats
	const auto texture_type = [img textureType];
	const auto layer_count = (uint32_t)[img arrayLength];
	if(texture_type == MTLTextureType1DArray) dim.y = layer_count;
	else if(texture_type == MTLTextureType2DArray
#if !defined(FLOOR_IOS)
			|| texture_type == MTLTextureTypeCubeArray
#endif
			) {
		dim.z = layer_count;
	}
	
	return dim;
}

static COMPUTE_IMAGE_TYPE compute_metal_image_type(id <MTLTexture> floor_nonnull img, const COMPUTE_MEMORY_FLAG flags) {
	COMPUTE_IMAGE_TYPE type { COMPUTE_IMAGE_TYPE::NONE };
	
	// start with the base format
	switch([img textureType]) {
		case MTLTextureType1D: type = COMPUTE_IMAGE_TYPE::IMAGE_1D; break;
		case MTLTextureType1DArray: type = COMPUTE_IMAGE_TYPE::IMAGE_1D_ARRAY; break;
		case MTLTextureType2D: type = COMPUTE_IMAGE_TYPE::IMAGE_2D; break;
		case MTLTextureType2DArray: type = COMPUTE_IMAGE_TYPE::IMAGE_2D_ARRAY; break;
		case MTLTextureType2DMultisample: type = COMPUTE_IMAGE_TYPE::IMAGE_2D_MSAA; break;
		case MTLTextureType3D: type = COMPUTE_IMAGE_TYPE::IMAGE_3D; break;
		case MTLTextureTypeCube: type = COMPUTE_IMAGE_TYPE::IMAGE_CUBE; break;
		case MTLTextureTypeCubeArray: type = COMPUTE_IMAGE_TYPE::IMAGE_CUBE_ARRAY; break;
#if defined(MAC_OS_X_VERSION_MAX_ALLOWED) && MAC_OS_X_VERSION_MAX_ALLOWED >= 101400
		case MTLTextureType2DMultisampleArray: type = COMPUTE_IMAGE_TYPE::IMAGE_2D_MSAA_ARRAY; break;
#endif
#if (defined(MAC_OS_X_VERSION_MAX_ALLOWED) && MAC_OS_X_VERSION_MAX_ALLOWED >= 101400) || \
	(defined(__IPHONE_OS_VERSION_MAX_ALLOWED) && __IPHONE_OS_VERSION_MAX_ALLOWED >= 120000)
		case MTLTextureTypeTextureBuffer: type = COMPUTE_IMAGE_TYPE::IMAGE_1D_BUFFER; break;
#endif
		
		// yay for forwards compatibility, or at least detecting that something is wrong(tm) ...
FLOOR_PUSH_WARNINGS()
FLOOR_IGNORE_WARNING(covered-switch-default)
		default:
			log_error("invalid or unknown metal texture type: $X", [img textureType]);
			return COMPUTE_IMAGE_TYPE::NONE;
FLOOR_POP_WARNINGS()
	}
	
	// handle the pixel format
	static const unordered_map<MTLPixelFormat, COMPUTE_IMAGE_TYPE> format_lut {
		// R
		{ MTLPixelFormatR8Unorm, COMPUTE_IMAGE_TYPE::R8UI_NORM },
		{ MTLPixelFormatR8Snorm, COMPUTE_IMAGE_TYPE::R8I_NORM },
		{ MTLPixelFormatR8Uint, COMPUTE_IMAGE_TYPE::R8UI },
		{ MTLPixelFormatR8Sint, COMPUTE_IMAGE_TYPE::R8I },
		{ MTLPixelFormatR16Unorm, COMPUTE_IMAGE_TYPE::R16UI_NORM },
		{ MTLPixelFormatR16Snorm, COMPUTE_IMAGE_TYPE::R16I_NORM },
		{ MTLPixelFormatR16Uint, COMPUTE_IMAGE_TYPE::R16UI },
		{ MTLPixelFormatR16Sint, COMPUTE_IMAGE_TYPE::R16I },
		{ MTLPixelFormatR16Float, COMPUTE_IMAGE_TYPE::R16F },
		{ MTLPixelFormatR32Uint, COMPUTE_IMAGE_TYPE::R32UI },
		{ MTLPixelFormatR32Sint, COMPUTE_IMAGE_TYPE::R32I },
		{ MTLPixelFormatR32Float, COMPUTE_IMAGE_TYPE::R32F },
		// RG
		{ MTLPixelFormatRG8Unorm, COMPUTE_IMAGE_TYPE::RG8UI_NORM },
		{ MTLPixelFormatRG8Snorm, COMPUTE_IMAGE_TYPE::RG8I_NORM },
		{ MTLPixelFormatRG8Uint, COMPUTE_IMAGE_TYPE::RG8UI },
		{ MTLPixelFormatRG8Sint, COMPUTE_IMAGE_TYPE::RG8I },
		{ MTLPixelFormatRG16Unorm, COMPUTE_IMAGE_TYPE::RG16UI_NORM },
		{ MTLPixelFormatRG16Snorm, COMPUTE_IMAGE_TYPE::RG16I_NORM },
		{ MTLPixelFormatRG16Uint, COMPUTE_IMAGE_TYPE::RG16UI },
		{ MTLPixelFormatRG16Sint, COMPUTE_IMAGE_TYPE::RG16I },
		{ MTLPixelFormatRG16Float, COMPUTE_IMAGE_TYPE::RG16F },
		{ MTLPixelFormatRG32Uint, COMPUTE_IMAGE_TYPE::RG32UI },
		{ MTLPixelFormatRG32Sint, COMPUTE_IMAGE_TYPE::RG32I },
		{ MTLPixelFormatRG32Float, COMPUTE_IMAGE_TYPE::RG32F },
		// NOTE: no RGB here
		// RGBA
		{ MTLPixelFormatRGBA8Unorm, COMPUTE_IMAGE_TYPE::RGBA8UI_NORM },
		{ MTLPixelFormatRGBA8Snorm, COMPUTE_IMAGE_TYPE::RGBA8I_NORM },
		{ MTLPixelFormatRGBA8Uint, COMPUTE_IMAGE_TYPE::RGBA8UI },
		{ MTLPixelFormatRGBA8Sint, COMPUTE_IMAGE_TYPE::RGBA8I },
		{ MTLPixelFormatRGBA16Unorm, COMPUTE_IMAGE_TYPE::RGBA16UI_NORM },
		{ MTLPixelFormatRGBA16Snorm, COMPUTE_IMAGE_TYPE::RGBA16I_NORM },
		{ MTLPixelFormatRGBA16Uint, COMPUTE_IMAGE_TYPE::RGBA16UI },
		{ MTLPixelFormatRGBA16Sint, COMPUTE_IMAGE_TYPE::RGBA16I },
		{ MTLPixelFormatRGBA16Float, COMPUTE_IMAGE_TYPE::RGBA16F },
		{ MTLPixelFormatRGBA32Uint, COMPUTE_IMAGE_TYPE::RGBA32UI },
		{ MTLPixelFormatRGBA32Sint, COMPUTE_IMAGE_TYPE::RGBA32I },
		{ MTLPixelFormatRGBA32Float, COMPUTE_IMAGE_TYPE::RGBA32F },
		// BGR(A)
		{ MTLPixelFormatBGRA8Unorm, COMPUTE_IMAGE_TYPE::BGRA8UI_NORM },
		{ MTLPixelFormatBGR10A2Unorm, COMPUTE_IMAGE_TYPE::A2BGR10UI_NORM },
#if defined(FLOOR_IOS)
		{ MTLPixelFormatBGR10_XR, COMPUTE_IMAGE_TYPE::BGR10UI_NORM },
		{ MTLPixelFormatBGR10_XR_sRGB, COMPUTE_IMAGE_TYPE::BGR10UI_NORM | COMPUTE_IMAGE_TYPE::FLAG_SRGB },
		{ MTLPixelFormatBGRA10_XR, COMPUTE_IMAGE_TYPE::BGRA10UI_NORM },
		{ MTLPixelFormatBGRA10_XR_sRGB, COMPUTE_IMAGE_TYPE::BGRA10UI_NORM | COMPUTE_IMAGE_TYPE::FLAG_SRGB },
#endif
		// sRGB
		{ MTLPixelFormatR8Unorm_sRGB, COMPUTE_IMAGE_TYPE::R8UI_NORM | COMPUTE_IMAGE_TYPE::FLAG_SRGB },
		{ MTLPixelFormatRG8Unorm_sRGB, COMPUTE_IMAGE_TYPE::RG8UI_NORM | COMPUTE_IMAGE_TYPE::FLAG_SRGB },
		{ MTLPixelFormatRGBA8Unorm_sRGB, COMPUTE_IMAGE_TYPE::RGBA8UI_NORM | COMPUTE_IMAGE_TYPE::FLAG_SRGB },
		{ MTLPixelFormatBGRA8Unorm_sRGB, COMPUTE_IMAGE_TYPE::BGRA8UI_NORM | COMPUTE_IMAGE_TYPE::FLAG_SRGB },
		// depth / depth+stencil
		{ MTLPixelFormatDepth32Float, (COMPUTE_IMAGE_TYPE::FLOAT |
									   COMPUTE_IMAGE_TYPE::CHANNELS_1 |
									   COMPUTE_IMAGE_TYPE::FORMAT_32 |
									   COMPUTE_IMAGE_TYPE::FLAG_DEPTH) },
#if !defined(FLOOR_IOS) || (__IPHONE_OS_VERSION_MAX_ALLOWED >= 130000)
		{ MTLPixelFormatDepth16Unorm, (COMPUTE_IMAGE_TYPE::UINT |
									   COMPUTE_IMAGE_TYPE::CHANNELS_1 |
									   COMPUTE_IMAGE_TYPE::FORMAT_16 |
									   COMPUTE_IMAGE_TYPE::FLAG_DEPTH) },
#endif
#if !defined(FLOOR_IOS) // os x only
		{ MTLPixelFormatDepth24Unorm_Stencil8, (COMPUTE_IMAGE_TYPE::UINT |
												COMPUTE_IMAGE_TYPE::CHANNELS_2 |
												COMPUTE_IMAGE_TYPE::FORMAT_24_8 |
												COMPUTE_IMAGE_TYPE::FLAG_DEPTH |
												COMPUTE_IMAGE_TYPE::FLAG_STENCIL) },
#endif
		{ MTLPixelFormatDepth32Float_Stencil8, (COMPUTE_IMAGE_TYPE::FLOAT |
												COMPUTE_IMAGE_TYPE::CHANNELS_2 |
												COMPUTE_IMAGE_TYPE::FORMAT_32_8 |
												COMPUTE_IMAGE_TYPE::FLAG_DEPTH |
												COMPUTE_IMAGE_TYPE::FLAG_STENCIL) },
		// BC formats
		{ MTLPixelFormatBC1_RGBA, COMPUTE_IMAGE_TYPE::BC1_RGBA },
		{ MTLPixelFormatBC1_RGBA_sRGB, COMPUTE_IMAGE_TYPE::BC1_RGBA_SRGB },
		{ MTLPixelFormatBC2_RGBA, COMPUTE_IMAGE_TYPE::BC2_RGBA },
		{ MTLPixelFormatBC2_RGBA_sRGB, COMPUTE_IMAGE_TYPE::BC2_RGBA_SRGB },
		{ MTLPixelFormatBC3_RGBA, COMPUTE_IMAGE_TYPE::BC3_RGBA },
		{ MTLPixelFormatBC3_RGBA_sRGB, COMPUTE_IMAGE_TYPE::BC3_RGBA_SRGB },
		{ MTLPixelFormatBC4_RUnorm, COMPUTE_IMAGE_TYPE::BC4_RUI },
		{ MTLPixelFormatBC4_RSnorm, COMPUTE_IMAGE_TYPE::BC4_RI },
		{ MTLPixelFormatBC5_RGUnorm, COMPUTE_IMAGE_TYPE::BC5_RGUI },
		{ MTLPixelFormatBC5_RGSnorm, COMPUTE_IMAGE_TYPE::BC5_RGI },
		{ MTLPixelFormatBC6H_RGBFloat, COMPUTE_IMAGE_TYPE::BC6H_RGBHF },
		{ MTLPixelFormatBC6H_RGBUfloat, COMPUTE_IMAGE_TYPE::BC6H_RGBUHF },
		{ MTLPixelFormatBC7_RGBAUnorm, COMPUTE_IMAGE_TYPE::BC7_RGBA },
		{ MTLPixelFormatBC7_RGBAUnorm_sRGB, COMPUTE_IMAGE_TYPE::BC7_RGBA_SRGB },
		// EAC/ETC formats
		{ MTLPixelFormatEAC_R11Unorm, COMPUTE_IMAGE_TYPE::EAC_R11UI },
		{ MTLPixelFormatEAC_R11Snorm, COMPUTE_IMAGE_TYPE::EAC_R11I },
		{ MTLPixelFormatEAC_RG11Unorm, COMPUTE_IMAGE_TYPE::EAC_RG11UI },
		{ MTLPixelFormatEAC_RG11Snorm, COMPUTE_IMAGE_TYPE::EAC_RG11I },
		{ MTLPixelFormatEAC_RGBA8, COMPUTE_IMAGE_TYPE::EAC_RGBA8 },
		{ MTLPixelFormatEAC_RGBA8_sRGB, COMPUTE_IMAGE_TYPE::EAC_RGBA8_SRGB },
		{ MTLPixelFormatETC2_RGB8, COMPUTE_IMAGE_TYPE::ETC2_RGB8 },
		{ MTLPixelFormatETC2_RGB8_sRGB, COMPUTE_IMAGE_TYPE::ETC2_RGB8_SRGB },
		{ MTLPixelFormatETC2_RGB8A1, COMPUTE_IMAGE_TYPE::ETC2_RGB8A1 },
		{ MTLPixelFormatETC2_RGB8A1_sRGB, COMPUTE_IMAGE_TYPE::ETC2_RGB8A1_SRGB },
		// ASTC formats
		{ MTLPixelFormatASTC_4x4_sRGB, COMPUTE_IMAGE_TYPE::ASTC_4X4_SRGB },
		{ MTLPixelFormatASTC_4x4_LDR, COMPUTE_IMAGE_TYPE::ASTC_4X4_LDR },
		{ MTLPixelFormatASTC_4x4_HDR, COMPUTE_IMAGE_TYPE::ASTC_4X4_HDR },
#if defined(FLOOR_IOS)
		// PVRTC formats
		{ MTLPixelFormatPVRTC_RGB_2BPP, COMPUTE_IMAGE_TYPE::PVRTC_RGB2 },
		{ MTLPixelFormatPVRTC_RGB_4BPP, COMPUTE_IMAGE_TYPE::PVRTC_RGB4 },
		{ MTLPixelFormatPVRTC_RGBA_2BPP, COMPUTE_IMAGE_TYPE::PVRTC_RGBA2 },
		{ MTLPixelFormatPVRTC_RGBA_4BPP, COMPUTE_IMAGE_TYPE::PVRTC_RGBA4 },
		{ MTLPixelFormatPVRTC_RGB_2BPP_sRGB, COMPUTE_IMAGE_TYPE::PVRTC_RGB2_SRGB },
		{ MTLPixelFormatPVRTC_RGB_4BPP_sRGB, COMPUTE_IMAGE_TYPE::PVRTC_RGB4_SRGB },
		{ MTLPixelFormatPVRTC_RGBA_2BPP_sRGB, COMPUTE_IMAGE_TYPE::PVRTC_RGBA2_SRGB },
		{ MTLPixelFormatPVRTC_RGBA_4BPP_sRGB, COMPUTE_IMAGE_TYPE::PVRTC_RGBA4_SRGB },
#endif
	};
	const auto metal_format = format_lut.find([img pixelFormat]);
	if(metal_format == end(format_lut)) {
		log_error("unsupported image pixel format: $X", [img pixelFormat]);
		return COMPUTE_IMAGE_TYPE::NONE;
	}
	type |= metal_format->second;
	
	// handle read/write flags
	if(has_flag<COMPUTE_MEMORY_FLAG::READ>(flags)) type |= COMPUTE_IMAGE_TYPE::READ;
	if(has_flag<COMPUTE_MEMORY_FLAG::WRITE>(flags)) type |= COMPUTE_IMAGE_TYPE::WRITE;
	if(!has_flag<COMPUTE_MEMORY_FLAG::READ>(flags) && !has_flag<COMPUTE_MEMORY_FLAG::WRITE>(flags)) {
		// assume read/write if no flags are set
		type |= COMPUTE_IMAGE_TYPE::READ_WRITE;
	}
	
	// handle mip-mapping / MSAA (although both are possible with == 1 as well)
	if([img mipmapLevelCount] > 1) type |= COMPUTE_IMAGE_TYPE::FLAG_MIPMAPPED;
	const auto sample_count = [img sampleCount];
	if (sample_count > 1) {
		type |= COMPUTE_IMAGE_TYPE::FLAG_MSAA;
		type |= image_sample_type_from_count((uint32_t)sample_count);
	}
	
	if(([img usage] & MTLTextureUsageRenderTarget) != 0 ||
	   [img isFramebufferOnly]) {
		type |= COMPUTE_IMAGE_TYPE::FLAG_RENDER_TARGET;
	}
	
	return type;
}

metal_image::metal_image(const compute_queue& cqueue,
						 id <MTLTexture> floor_nonnull external_image,
						 void* floor_nullable host_ptr_,
						 const COMPUTE_MEMORY_FLAG flags_) :
compute_image(cqueue, compute_metal_image_dim(external_image), compute_metal_image_type(external_image, flags_),
			  host_ptr_, flags_, 0, 0, nullptr), image(external_image), is_external(true) {
	// device must match
	if(((const metal_device&)dev).device != [external_image device]) {
		log_error("specified metal device does not match the device set in the external image");
		return;
	}
	
	// copy existing options
	options = [image cpuCacheMode];

#if defined(FLOOR_IOS)
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
#if !defined(FLOOR_IOS)
		case MTLStorageModeManaged:
			options |= MTLResourceStorageModeManaged;
			break;
#endif
	}
	
#if defined(FLOOR_IOS)
FLOOR_POP_WARNINGS()
#endif
	
	usage_options = [image usage];
	storage_options = [image storageMode];
	
	// shim type is unnecessary here
	shim_image_type = image_type;
}

bool metal_image::create_internal(const bool copy_host_data, const compute_queue& cqueue) {
	// NOTE: opengl sharing flag is ignored, because there is no metal/opengl sharing and metal can interop with itself w/o explicit sharing
	const auto& mtl_dev = (const metal_device&)cqueue.get_device();
	
	// should not be called under that condition, but just to be safe
	if(is_external) {
		log_error("image is external!");
		return false;
	}
	
	// create an appropriate texture descriptor
	desc = [[MTLTextureDescriptor alloc] init];
	
	const auto dim_count = image_dim_count(image_type);
	const bool is_array = has_flag<COMPUTE_IMAGE_TYPE::FLAG_ARRAY>(image_type);
	const bool is_cube = has_flag<COMPUTE_IMAGE_TYPE::FLAG_CUBE>(image_type);
	const bool is_msaa = has_flag<COMPUTE_IMAGE_TYPE::FLAG_MSAA>(image_type);
	const bool is_buffer = has_flag<COMPUTE_IMAGE_TYPE::FLAG_BUFFER>(image_type);
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
#if (defined(MAC_OS_X_VERSION_MAX_ALLOWED) && MAC_OS_X_VERSION_MAX_ALLOWED >= 101400) || \
	(defined(__IPHONE_OS_VERSION_MAX_ALLOWED) && __IPHONE_OS_VERSION_MAX_ALLOWED >= 120000)
				tex_type = MTLTextureTypeTextureBuffer;
#else
				log_error("1D buffer image is not supported");
				return false;
#endif
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
#if defined(MAC_OS_X_VERSION_MAX_ALLOWED) && MAC_OS_X_VERSION_MAX_ALLOWED >= 101400
					tex_type = MTLTextureType2DMultisampleArray;
#else
					log_error("2D MSAA array image is not supported");
					return false;
#endif
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
	
	// set shim format info if necessary
	set_shim_type_info();
	
	// misc options
	[desc setMipmapLevelCount:mip_level_count];
	[desc setSampleCount:image_sample_count(image_type)];
	
	// usage/access options
	[desc setResourceOptions:options];
	[desc setStorageMode:storage_options]; // don't know why this needs to be set twice
	[desc setUsage:usage_options];
	
	// create the actual metal image with the just created descriptor
	image = [mtl_device newTextureWithDescriptor:desc];
	
	// copy host memory to device if it is non-null and NO_INITIAL_COPY is not specified
	if(copy_host_data && host_ptr != nullptr && !has_flag<COMPUTE_MEMORY_FLAG::NO_INITIAL_COPY>(flags)) {
		// copy to device memory must go through a blit command
		id <MTLCommandBuffer> cmd_buffer = ((const metal_queue&)cqueue).make_command_buffer();
		id <MTLBlitCommandEncoder> blit_encoder = [cmd_buffer blitCommandEncoder];
		
		// NOTE: arrays/slices must be copied in per slice (for all else: there just is one slice)
		const size_t slice_count = layer_count;
		
		const uint8_t* cpy_host_ptr = (const uint8_t*)host_ptr;
		apply_on_levels([this, &cpy_host_ptr, &slice_count,
						 &is_compressed, &dim_count](const uint32_t& level,
													 const uint4& mip_image_dim,
													 const uint32_t& slice_data_size,
													 const uint32_t& level_data_size) {
			// NOTE: original size/type for non-3-channel types, and the 4-channel shim size/type for 3-channel types
			uint32_t bytes_per_row = max(mip_image_dim.x, 1u);
			if (is_compressed) {
				// for compressed formats, we need to consider the actual bits-per-pixel and full block size per row -> need to multiply with Y block size
				const auto block_size = image_compression_block_size(shim_image_type);
				// at small mip levels (< block size), we need to ensure that we actually upload the full block
				bytes_per_row = max(block_size.x, bytes_per_row);
				assert((bytes_per_row % block_size.x) == 0u && "image width must be a multiple of the underlying block size");
				bytes_per_row *= image_bits_per_pixel(shim_image_type) * block_size.y;
				bytes_per_row = (bytes_per_row + 7u) / 8u;
			} else {
				bytes_per_row *= image_bytes_per_pixel(shim_image_type);
			}
			
			const uint8_t* data_ptr = cpy_host_ptr;
			unique_ptr<uint8_t[]> conv_data_ptr;
			if (image_type != shim_image_type) {
				// need to copy/convert the RGB host data to RGBA
				conv_data_ptr = rgb_to_rgba(image_type, shim_image_type, cpy_host_ptr, true /* ignore mip levels as we do this manually */);
				data_ptr = conv_data_ptr.get();
			}
			
			const MTLRegion mipmap_region {
				.origin = { 0, 0, 0 },
				.size = {
					max(mip_image_dim.x, 1u),
					dim_count >= 2 ? max(mip_image_dim.y, 1u) : 1,
					dim_count >= 3 ? max(mip_image_dim.z, 1u) : 1,
				}
			};
			for(size_t slice = 0; slice < slice_count; ++slice) {
				[image replaceRegion:mipmap_region
						 mipmapLevel:level
							   slice:slice
						   withBytes:(data_ptr + slice * slice_data_size)
						 bytesPerRow:bytes_per_row
					   bytesPerImage:(is_compressed ? 0 : slice_data_size)];
			}
			
			// mip-level image data provided by user, advance pointer
			if (!generate_mip_maps) {
				cpy_host_ptr += level_data_size;
			}
			
			return true;
		}, shim_image_type);
		
		// manually create the mip-map chain if this was specified
		if(is_mip_mapped && mip_level_count > 1 && generate_mip_maps) {
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

metal_image::~metal_image() {
	// kill the image
	image = nil;
	if(desc != nil) {
		desc = nil;
	}
}

bool metal_image::blit(const compute_queue& cqueue, const compute_image& src) {
	if(image == nil) return false;
	
	const auto src_image_dim = src.get_image_dim();
	if ((src_image_dim != image_dim).any()) {
		log_error("blit: dim mismatch: src $ != dst $", src_image_dim, image_dim);
		return false;
	}
	
	const auto src_layer_count = src.get_layer_count();
	if (src_layer_count != layer_count) {
		log_error("blit: layer count mismatch: src $ != dst $", src_layer_count, layer_count);
		return false;
	}
	
	const auto src_data_size = src.get_image_data_size();
	if (src_data_size != image_data_size) {
		log_error("blit: size mismatch: src $ != dst $", src_data_size, image_data_size);
		return false;
	}
	
	const auto src_format = image_format(src.get_image_type());
	const auto dst_format = image_format(image_type);
	if (src_format != dst_format) {
		log_error("blit: format mismatch ($ != $)", src_format, dst_format);
		return false;
	}
	
	if (image_compressed(image_type) || image_compressed(src.get_image_type())) {
		log_error("blit: blitting of compressed formats is not supported");
		return false;
	}
	
	id <MTLCommandBuffer> cmd_buffer = ((const metal_queue&)cqueue).make_command_buffer();
	id <MTLBlitCommandEncoder> blit_encoder = [cmd_buffer blitCommandEncoder];
	
	auto src_image = ((const metal_image&)src).get_metal_image();
	const auto dim_count = image_dim_count(image_type);
	
	apply_on_levels<true /* blit all levels */>([this, &blit_encoder, &src_image, &dim_count](const uint32_t& level,
																							  const uint4& mip_image_dim,
																							  const uint32_t&,
																							  const uint32_t&) {
		const MTLSize mipmap_size {
			max(mip_image_dim.x, 1u),
			dim_count >= 2 ? max(mip_image_dim.y, 1u) : 1u,
			dim_count >= 3 ? max(mip_image_dim.z, 1u) : 1u,
		};
		for (size_t slice = 0; slice < layer_count; ++slice) {
			[blit_encoder copyFromTexture:src_image
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
	if (@available(iOS 12.0, macOS 10.14, *)) {
		[blit_encoder optimizeContentsForGPUAccess:image];
	}
	
	[blit_encoder endEncoding];
	[cmd_buffer commit];
	[cmd_buffer waitUntilCompleted];
	
	return true;
}

bool metal_image::zero(const compute_queue& cqueue) {
	if(image == nil) return false;
	
	const bool is_compressed = image_compressed(image_type);
	const auto dim_count = image_dim_count(image_type);
	const auto bytes_per_slice = image_slice_data_size_from_types(image_dim, shim_image_type);
	
	auto zero_buffer = cqueue.get_device().context->create_buffer(cqueue, bytes_per_slice,
																  COMPUTE_MEMORY_FLAG::READ_WRITE);
	zero_buffer->set_debug_label("zero_buffer");
	auto mtl_zero_buffer = ((const metal_buffer&)*zero_buffer).get_metal_buffer();
	
	id <MTLCommandBuffer> cmd_buffer = ((const metal_queue&)cqueue).make_command_buffer();
	id <MTLBlitCommandEncoder> blit_encoder = [cmd_buffer blitCommandEncoder];
	
	[blit_encoder fillBuffer:mtl_zero_buffer range:NSRange { 0, bytes_per_slice } value:0u];
	
	const auto success = apply_on_levels<true>([this, &mtl_zero_buffer, &blit_encoder, &dim_count, &is_compressed](const uint32_t& level,
																												   const uint4& mip_image_dim,
																												   const uint32_t& slice_data_size,
																												   const uint32_t&) {
		const auto bytes_per_row = image_bytes_per_pixel(shim_image_type) * max(mip_image_dim.x, 1u);
		for (size_t slice = 0; slice < layer_count; ++slice) {
			const MTLSize copy_size {
				max(mip_image_dim.x, 1u),
				dim_count >= 2 ? max(mip_image_dim.y, 1u) : 1,
				dim_count >= 3 ? max(mip_image_dim.z, 1u) : 1,
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
	
	return success;
}

void* floor_nullable __attribute__((aligned(128))) metal_image::map(const compute_queue& cqueue,
																	const COMPUTE_MEMORY_MAP_FLAG flags_) {
	if(image == nil) return nullptr;
	
	// TODO: parameter origin + region + layer
	const auto dim_count = image_dim_count(image_type);
	
	// TODO: image map check + move this check there:
	if((options & MTLResourceStorageModeMask) == MTLResourceStorageModePrivate) {
		log_error("can't map an image which is set to be inaccessible by the host!");
		return nullptr;
	}
	
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
				log_error("neither read nor write flag set for image mapping!");
				return nullptr;
		}
	}
	
	// alloc host memory
	auto host_buffer = make_aligned_ptr<uint8_t>(image_data_size);
	
	// check if we need to copy the image from the device (in case READ was specified)
	if(!write_only) {
		// must finish up all current work before we can properly read from the current image
		cqueue.finish();
		
		// need to sync image (resource) before being able to read it
		if (metal_buffer::metal_resource_type_needs_sync(options)) {
			metal_buffer::sync_metal_resource(cqueue, image);
		}
		
		// copy image data to the host
		id <MTLCommandBuffer> cmd_buffer = ((const metal_queue&)cqueue).make_command_buffer();
		id <MTLBlitCommandEncoder> blit_encoder = [cmd_buffer blitCommandEncoder];
		const bool is_compressed = image_compressed(image_type);
		
		aligned_ptr<uint8_t> host_shim_buffer;
		if(image_type != shim_image_type) {
			host_shim_buffer = make_aligned_ptr<uint8_t>(shim_image_data_size);
		}
		
		uint8_t* cpy_host_ptr = (image_type != shim_image_type ? host_shim_buffer.get() : host_buffer.get());
		apply_on_levels([this, &cpy_host_ptr,
						 &dim_count, &is_compressed](const uint32_t& level,
													 const uint4& mip_image_dim,
													 const uint32_t& slice_data_size,
													 const uint32_t&) {
			const auto bytes_per_row = image_bytes_per_pixel(shim_image_type) * max(mip_image_dim.x, 1u);
			const MTLRegion mipmap_region {
				.origin = { 0, 0, 0 },
				.size = {
					max(mip_image_dim.x, 1u),
					dim_count >= 2 ? max(mip_image_dim.y, 1u) : 1,
					dim_count >= 3 ? max(mip_image_dim.z, 1u) : 1,
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
		
		[blit_encoder endEncoding];
		[cmd_buffer commit];
		[cmd_buffer waitUntilCompleted];
		
		// convert to RGB + cleanup
		if(image_type != shim_image_type) {
			rgba_to_rgb(shim_image_type, image_type, host_shim_buffer.get(), host_buffer.get());
		}
	}
	
	// need to remember how much we mapped and where (so the host->device write-back copies the right amount of bytes)
	auto ret_ptr = host_buffer.get();
	mappings.emplace(ret_ptr, metal_mapping { move(host_buffer), flags_, write_only });
	
	return ret_ptr;
}

bool metal_image::unmap(const compute_queue& cqueue, void* floor_nullable __attribute__((aligned(128))) mapped_ptr) {
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
	if (has_flag<COMPUTE_MEMORY_MAP_FLAG::WRITE>(iter->second.flags) ||
		has_flag<COMPUTE_MEMORY_MAP_FLAG::WRITE_INVALIDATE>(iter->second.flags)) {
		// copy host memory to device memory
		id <MTLCommandBuffer> cmd_buffer = ((const metal_queue&)cqueue).make_command_buffer();
		id <MTLBlitCommandEncoder> blit_encoder = [cmd_buffer blitCommandEncoder];
		const bool is_compressed = image_compressed(image_type);
		const auto dim_count = image_dim_count(image_type);
		
		// again, need to convert RGB to RGBA if necessary
		const uint8_t* host_buffer = (const uint8_t*)mapped_ptr;
		unique_ptr<uint8_t[]> host_shim_buffer;
		if (image_type != shim_image_type) {
			host_shim_buffer = rgb_to_rgba(image_type, shim_image_type, host_buffer);
		}
		
		const uint8_t* cpy_host_ptr = (image_type != shim_image_type ? host_shim_buffer.get() : host_buffer);
		success = apply_on_levels([this, &cpy_host_ptr,
						 &dim_count, &is_compressed](const uint32_t& level,
													 const uint4& mip_image_dim,
													 const uint32_t& slice_data_size,
													 const uint32_t&) {
			const auto bytes_per_row = image_bytes_per_pixel(shim_image_type) * max(mip_image_dim.x, 1u);
			const MTLRegion mipmap_region {
				.origin = { 0, 0, 0 },
				.size = {
					max(mip_image_dim.x, 1u),
					dim_count >= 2 ? max(mip_image_dim.y, 1u) : 1,
					dim_count >= 3 ? max(mip_image_dim.z, 1u) : 1,
				}
			};
			for(size_t slice = 0; slice < layer_count; ++slice) {
				[image replaceRegion:mipmap_region
						 mipmapLevel:level
							   slice:slice
						   withBytes:cpy_host_ptr
						 bytesPerRow:(is_compressed ? 0 : bytes_per_row)
					   bytesPerImage:slice_data_size];
				cpy_host_ptr += slice_data_size;
			}
			return true;
		}, shim_image_type);
		
		[blit_encoder endEncoding];
		[cmd_buffer commit];
		[cmd_buffer waitUntilCompleted];
		
		// cleanup
		host_shim_buffer = nullptr;
		
		// update mip-map chain
		if(generate_mip_maps) {
			generate_mip_map_chain(cqueue);
		}
	}
	
	// free host memory again and remove the mapping
	mappings.erase(mapped_ptr);
	
	return success;
}

bool metal_image::acquire_opengl_object(const compute_queue* floor_nullable cqueue floor_unused) {
	if(image == nil) return false;
	if(gl_object_state) {
#if defined(FLOOR_DEBUG) && 0
		log_warn("image has already been acquired!");
#endif
		return true;
	}
	gl_object_state = true;
	return true;
}

bool metal_image::release_opengl_object(const compute_queue* floor_nullable cqueue floor_unused) {
	if(image == nil) return false;
	if(!gl_object_state) {
#if defined(FLOOR_DEBUG) && 0
		log_warn("image has already been released!");
#endif
		return true;
	}
	gl_object_state = false;
	return true;
}

void metal_image::generate_mip_map_chain(const compute_queue& cqueue) {
	// nothing to do here
	if([image mipmapLevelCount] == 1) return;
	
	id <MTLCommandBuffer> cmd_buffer = ((const metal_queue&)cqueue).make_command_buffer();
	id <MTLBlitCommandEncoder> blit_encoder = [cmd_buffer blitCommandEncoder];
	[blit_encoder generateMipmapsForTexture:image];
	[blit_encoder endEncoding];
	[cmd_buffer commit];
	[cmd_buffer waitUntilCompleted];
}

optional<MTLPixelFormat> metal_image::metal_pixel_format_from_image_type(const COMPUTE_IMAGE_TYPE& image_type_) {
	static const unordered_map<COMPUTE_IMAGE_TYPE, MTLPixelFormat> format_lut {
		// R
		{ COMPUTE_IMAGE_TYPE::R8UI_NORM, MTLPixelFormatR8Unorm },
		{ COMPUTE_IMAGE_TYPE::R8I_NORM, MTLPixelFormatR8Snorm },
		{ COMPUTE_IMAGE_TYPE::R8UI, MTLPixelFormatR8Uint },
		{ COMPUTE_IMAGE_TYPE::R8I, MTLPixelFormatR8Sint },
		{ COMPUTE_IMAGE_TYPE::R16UI_NORM, MTLPixelFormatR16Unorm },
		{ COMPUTE_IMAGE_TYPE::R16I_NORM, MTLPixelFormatR16Snorm },
		{ COMPUTE_IMAGE_TYPE::R16UI, MTLPixelFormatR16Uint },
		{ COMPUTE_IMAGE_TYPE::R16I, MTLPixelFormatR16Sint },
		{ COMPUTE_IMAGE_TYPE::R16F, MTLPixelFormatR16Float },
		{ COMPUTE_IMAGE_TYPE::R32UI, MTLPixelFormatR32Uint },
		{ COMPUTE_IMAGE_TYPE::R32I, MTLPixelFormatR32Sint },
		{ COMPUTE_IMAGE_TYPE::R32F, MTLPixelFormatR32Float },
		// RG
		{ COMPUTE_IMAGE_TYPE::RG8UI_NORM, MTLPixelFormatRG8Unorm },
		{ COMPUTE_IMAGE_TYPE::RG8I_NORM, MTLPixelFormatRG8Snorm },
		{ COMPUTE_IMAGE_TYPE::RG8UI, MTLPixelFormatRG8Uint },
		{ COMPUTE_IMAGE_TYPE::RG8I, MTLPixelFormatRG8Sint },
		{ COMPUTE_IMAGE_TYPE::RG16UI_NORM, MTLPixelFormatRG16Unorm },
		{ COMPUTE_IMAGE_TYPE::RG16I_NORM, MTLPixelFormatRG16Snorm },
		{ COMPUTE_IMAGE_TYPE::RG16UI, MTLPixelFormatRG16Uint },
		{ COMPUTE_IMAGE_TYPE::RG16I, MTLPixelFormatRG16Sint },
		{ COMPUTE_IMAGE_TYPE::RG16F, MTLPixelFormatRG16Float },
		{ COMPUTE_IMAGE_TYPE::RG32UI, MTLPixelFormatRG32Uint },
		{ COMPUTE_IMAGE_TYPE::RG32I, MTLPixelFormatRG32Sint },
		{ COMPUTE_IMAGE_TYPE::RG32F, MTLPixelFormatRG32Float },
		// RGB -> RGBA
		{ COMPUTE_IMAGE_TYPE::RGB8UI_NORM, MTLPixelFormatRGBA8Unorm },
		{ COMPUTE_IMAGE_TYPE::RGB8I_NORM, MTLPixelFormatRGBA8Snorm },
		{ COMPUTE_IMAGE_TYPE::RGB8UI, MTLPixelFormatRGBA8Uint },
		{ COMPUTE_IMAGE_TYPE::RGB8I, MTLPixelFormatRGBA8Sint },
		{ COMPUTE_IMAGE_TYPE::RGB16UI_NORM, MTLPixelFormatRGBA16Unorm },
		{ COMPUTE_IMAGE_TYPE::RGB16I_NORM, MTLPixelFormatRGBA16Snorm },
		{ COMPUTE_IMAGE_TYPE::RGB16UI, MTLPixelFormatRGBA16Uint },
		{ COMPUTE_IMAGE_TYPE::RGB16I, MTLPixelFormatRGBA16Sint },
		{ COMPUTE_IMAGE_TYPE::RGB16F, MTLPixelFormatRGBA16Float },
		{ COMPUTE_IMAGE_TYPE::RGB32UI, MTLPixelFormatRGBA32Uint },
		{ COMPUTE_IMAGE_TYPE::RGB32I, MTLPixelFormatRGBA32Sint },
		{ COMPUTE_IMAGE_TYPE::RGB32F, MTLPixelFormatRGBA32Float },
		// RGBA
		{ COMPUTE_IMAGE_TYPE::RGBA8UI_NORM, MTLPixelFormatRGBA8Unorm },
		{ COMPUTE_IMAGE_TYPE::RGBA8I_NORM, MTLPixelFormatRGBA8Snorm },
		{ COMPUTE_IMAGE_TYPE::RGBA8UI, MTLPixelFormatRGBA8Uint },
		{ COMPUTE_IMAGE_TYPE::RGBA8I, MTLPixelFormatRGBA8Sint },
		{ COMPUTE_IMAGE_TYPE::RGBA16UI_NORM, MTLPixelFormatRGBA16Unorm },
		{ COMPUTE_IMAGE_TYPE::RGBA16I_NORM, MTLPixelFormatRGBA16Snorm },
		{ COMPUTE_IMAGE_TYPE::RGBA16UI, MTLPixelFormatRGBA16Uint },
		{ COMPUTE_IMAGE_TYPE::RGBA16I, MTLPixelFormatRGBA16Sint },
		{ COMPUTE_IMAGE_TYPE::RGBA16F, MTLPixelFormatRGBA16Float },
		{ COMPUTE_IMAGE_TYPE::RGBA32UI, MTLPixelFormatRGBA32Uint },
		{ COMPUTE_IMAGE_TYPE::RGBA32I, MTLPixelFormatRGBA32Sint },
		{ COMPUTE_IMAGE_TYPE::RGBA32F, MTLPixelFormatRGBA32Float },
		// BGR(A)
		{ COMPUTE_IMAGE_TYPE::BGRA8UI_NORM, MTLPixelFormatBGRA8Unorm },
		{ COMPUTE_IMAGE_TYPE::A2BGR10UI_NORM, MTLPixelFormatBGR10A2Unorm },
#if defined(FLOOR_IOS)
		{ COMPUTE_IMAGE_TYPE::BGR10UI_NORM, MTLPixelFormatBGR10_XR },
		{ COMPUTE_IMAGE_TYPE::BGR10UI_NORM | COMPUTE_IMAGE_TYPE::FLAG_SRGB, MTLPixelFormatBGR10_XR_sRGB },
		{ COMPUTE_IMAGE_TYPE::BGRA10UI_NORM, MTLPixelFormatBGRA10_XR },
		{ COMPUTE_IMAGE_TYPE::BGRA10UI_NORM | COMPUTE_IMAGE_TYPE::FLAG_SRGB, MTLPixelFormatBGRA10_XR_sRGB },
#endif
		// sRGB
		{ COMPUTE_IMAGE_TYPE::R8UI_NORM | COMPUTE_IMAGE_TYPE::FLAG_SRGB, MTLPixelFormatR8Unorm_sRGB },
		{ COMPUTE_IMAGE_TYPE::RG8UI_NORM | COMPUTE_IMAGE_TYPE::FLAG_SRGB, MTLPixelFormatRG8Unorm_sRGB },
		{ COMPUTE_IMAGE_TYPE::RGB8UI_NORM | COMPUTE_IMAGE_TYPE::FLAG_SRGB, MTLPixelFormatRGBA8Unorm_sRGB }, // allow RGB -> RGBA
		{ COMPUTE_IMAGE_TYPE::RGBA8UI_NORM | COMPUTE_IMAGE_TYPE::FLAG_SRGB, MTLPixelFormatRGBA8Unorm_sRGB },
		{ COMPUTE_IMAGE_TYPE::BGRA8UI_NORM | COMPUTE_IMAGE_TYPE::FLAG_SRGB, MTLPixelFormatBGRA8Unorm_sRGB },
		// depth / depth+stencil
		{ (COMPUTE_IMAGE_TYPE::FLOAT |
		   COMPUTE_IMAGE_TYPE::CHANNELS_1 |
		   COMPUTE_IMAGE_TYPE::FORMAT_32 |
		   COMPUTE_IMAGE_TYPE::FLAG_DEPTH), MTLPixelFormatDepth32Float },
#if !defined(FLOOR_IOS) || (__IPHONE_OS_VERSION_MAX_ALLOWED >= 130000)
		{ (COMPUTE_IMAGE_TYPE::UINT |
		   COMPUTE_IMAGE_TYPE::CHANNELS_1 |
		   COMPUTE_IMAGE_TYPE::FORMAT_16 |
		   COMPUTE_IMAGE_TYPE::FLAG_DEPTH), MTLPixelFormatDepth16Unorm },
#endif
#if !defined(FLOOR_IOS) // os x only
		{ (COMPUTE_IMAGE_TYPE::UINT |
		   COMPUTE_IMAGE_TYPE::CHANNELS_2 |
		   COMPUTE_IMAGE_TYPE::FORMAT_24_8 |
		   COMPUTE_IMAGE_TYPE::FLAG_DEPTH |
		   COMPUTE_IMAGE_TYPE::FLAG_STENCIL), MTLPixelFormatDepth24Unorm_Stencil8 },
#endif
		{ (COMPUTE_IMAGE_TYPE::FLOAT |
		   COMPUTE_IMAGE_TYPE::CHANNELS_2 |
		   COMPUTE_IMAGE_TYPE::FORMAT_32_8 |
		   COMPUTE_IMAGE_TYPE::FLAG_DEPTH |
		   COMPUTE_IMAGE_TYPE::FLAG_STENCIL), MTLPixelFormatDepth32Float_Stencil8 },
		// BC formats
		{ COMPUTE_IMAGE_TYPE::BC1_RGBA, MTLPixelFormatBC1_RGBA },
		{ COMPUTE_IMAGE_TYPE::BC1_RGBA_SRGB, MTLPixelFormatBC1_RGBA_sRGB },
		{ COMPUTE_IMAGE_TYPE::BC2_RGBA, MTLPixelFormatBC2_RGBA },
		{ COMPUTE_IMAGE_TYPE::BC2_RGBA_SRGB, MTLPixelFormatBC2_RGBA_sRGB },
		{ COMPUTE_IMAGE_TYPE::BC3_RGBA, MTLPixelFormatBC3_RGBA },
		{ COMPUTE_IMAGE_TYPE::BC3_RGBA_SRGB, MTLPixelFormatBC3_RGBA_sRGB },
		{ COMPUTE_IMAGE_TYPE::BC4_RUI, MTLPixelFormatBC4_RUnorm },
		{ COMPUTE_IMAGE_TYPE::BC4_RI, MTLPixelFormatBC4_RSnorm },
		{ COMPUTE_IMAGE_TYPE::BC5_RGUI, MTLPixelFormatBC5_RGUnorm },
		{ COMPUTE_IMAGE_TYPE::BC5_RGI, MTLPixelFormatBC5_RGSnorm },
		{ COMPUTE_IMAGE_TYPE::BC6H_RGBHF, MTLPixelFormatBC6H_RGBFloat },
		{ COMPUTE_IMAGE_TYPE::BC6H_RGBUHF, MTLPixelFormatBC6H_RGBUfloat },
		{ COMPUTE_IMAGE_TYPE::BC7_RGBA, MTLPixelFormatBC7_RGBAUnorm },
		{ COMPUTE_IMAGE_TYPE::BC7_RGBA_SRGB, MTLPixelFormatBC7_RGBAUnorm_sRGB },
		// EAC/ETC formats
		{ COMPUTE_IMAGE_TYPE::EAC_R11UI, MTLPixelFormatEAC_R11Unorm },
		{ COMPUTE_IMAGE_TYPE::EAC_R11I, MTLPixelFormatEAC_R11Snorm },
		{ COMPUTE_IMAGE_TYPE::EAC_RG11UI, MTLPixelFormatEAC_RG11Unorm },
		{ COMPUTE_IMAGE_TYPE::EAC_RG11I, MTLPixelFormatEAC_RG11Snorm },
		{ COMPUTE_IMAGE_TYPE::EAC_RGBA8, MTLPixelFormatEAC_RGBA8 },
		{ COMPUTE_IMAGE_TYPE::EAC_RGBA8_SRGB, MTLPixelFormatEAC_RGBA8_sRGB },
		{ COMPUTE_IMAGE_TYPE::ETC2_RGB8, MTLPixelFormatETC2_RGB8 },
		{ COMPUTE_IMAGE_TYPE::ETC2_RGB8_SRGB, MTLPixelFormatETC2_RGB8_sRGB },
		{ COMPUTE_IMAGE_TYPE::ETC2_RGB8A1, MTLPixelFormatETC2_RGB8A1 },
		{ COMPUTE_IMAGE_TYPE::ETC2_RGB8A1_SRGB, MTLPixelFormatETC2_RGB8A1_sRGB },
		// ASTC formats
		{ COMPUTE_IMAGE_TYPE::ASTC_4X4_SRGB, MTLPixelFormatASTC_4x4_sRGB },
		{ COMPUTE_IMAGE_TYPE::ASTC_4X4_LDR, MTLPixelFormatASTC_4x4_LDR },
		{ COMPUTE_IMAGE_TYPE::ASTC_4X4_HDR, MTLPixelFormatASTC_4x4_HDR },
#if defined(FLOOR_IOS)
		// PVRTC formats
		{ COMPUTE_IMAGE_TYPE::PVRTC_RGB2, MTLPixelFormatPVRTC_RGB_2BPP },
		{ COMPUTE_IMAGE_TYPE::PVRTC_RGB4, MTLPixelFormatPVRTC_RGB_4BPP },
		{ COMPUTE_IMAGE_TYPE::PVRTC_RGBA2, MTLPixelFormatPVRTC_RGBA_2BPP },
		{ COMPUTE_IMAGE_TYPE::PVRTC_RGBA4, MTLPixelFormatPVRTC_RGBA_4BPP },
		{ COMPUTE_IMAGE_TYPE::PVRTC_RGB2_SRGB, MTLPixelFormatPVRTC_RGB_2BPP_sRGB },
		{ COMPUTE_IMAGE_TYPE::PVRTC_RGB4_SRGB, MTLPixelFormatPVRTC_RGB_4BPP_sRGB },
		{ COMPUTE_IMAGE_TYPE::PVRTC_RGBA2_SRGB, MTLPixelFormatPVRTC_RGBA_2BPP_sRGB },
		{ COMPUTE_IMAGE_TYPE::PVRTC_RGBA4_SRGB, MTLPixelFormatPVRTC_RGBA_4BPP_sRGB },
#endif
		// TODO: special image formats, these are partially supported
	};
	const auto masked_image_type = (image_type_ & (COMPUTE_IMAGE_TYPE::__DATA_TYPE_MASK |
												   COMPUTE_IMAGE_TYPE::__CHANNELS_MASK |
												   COMPUTE_IMAGE_TYPE::__COMPRESSION_MASK |
												   COMPUTE_IMAGE_TYPE::__FORMAT_MASK |
												   COMPUTE_IMAGE_TYPE::__LAYOUT_MASK |
												   COMPUTE_IMAGE_TYPE::FLAG_NORMALIZED |
												   COMPUTE_IMAGE_TYPE::FLAG_DEPTH |
												   COMPUTE_IMAGE_TYPE::FLAG_STENCIL |
												   COMPUTE_IMAGE_TYPE::FLAG_SRGB));
	const auto metal_pixel_format = format_lut.find(masked_image_type);
	if (metal_pixel_format == end(format_lut)) {
		return {};
	}
	return metal_pixel_format->second;
}

void* floor_nullable metal_image::get_metal_image_void_ptr() const {
	return (__bridge void*)image;
}

void metal_image::set_debug_label(const string& label) {
	compute_memory::set_debug_label(label);
	if (image) {
		image.label = [NSString stringWithUTF8String:debug_label.c_str()];
	}
}

#endif
