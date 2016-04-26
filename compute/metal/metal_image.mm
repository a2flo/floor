/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2016 Florian Ziesche
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

// TODO: proper error (return) value handling everywhere

metal_image::metal_image(const metal_device* device,
						 const uint4 image_dim_,
						 const COMPUTE_IMAGE_TYPE image_type_,
						 void* host_ptr_,
						 const COMPUTE_MEMORY_FLAG flags_,
						 const uint32_t opengl_type_,
						 const uint32_t external_gl_object_,
						 const opengl_image_info* gl_image_info) :
compute_image(device, image_dim_, image_type_, host_ptr_, flags_,
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
	
	if(has_flag<COMPUTE_IMAGE_TYPE::FLAG_RENDERBUFFER>(image_type)) {
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
	}
	else {
#if !defined(FLOOR_IOS)
		options |= MTLResourceStorageModeManaged;
		storage_options = MTLStorageModeManaged;
#else
		options |= MTLResourceStorageModeShared;
		storage_options = MTLStorageModeShared;
#endif
	}
	
	// actually create the image
	if(!create_internal(true, device, nullptr)) {
		return; // can't do much else
	}
}

static uint4 compute_metal_image_dim(id <MTLTexture> img) {
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

static COMPUTE_IMAGE_TYPE compute_metal_image_type(id <MTLTexture> img, const COMPUTE_MEMORY_FLAG flags) {
	COMPUTE_IMAGE_TYPE type { COMPUTE_IMAGE_TYPE::NONE };
	
	if([img isFramebufferOnly]) {
		log_error("metal image can only be used as a framebuffer");
		return COMPUTE_IMAGE_TYPE::NONE;
	}
	
	// start with the base format
	switch([img textureType]) {
		case MTLTextureType1D: type = COMPUTE_IMAGE_TYPE::IMAGE_1D; break;
		case MTLTextureType1DArray: type = COMPUTE_IMAGE_TYPE::IMAGE_1D_ARRAY; break;
		case MTLTextureType2D: type = COMPUTE_IMAGE_TYPE::IMAGE_2D; break;
		case MTLTextureType2DArray: type = COMPUTE_IMAGE_TYPE::IMAGE_2D_ARRAY; break;
		case MTLTextureType2DMultisample: type = COMPUTE_IMAGE_TYPE::IMAGE_2D_MSAA; break;
		case MTLTextureType3D: type = COMPUTE_IMAGE_TYPE::IMAGE_3D; break;
		case MTLTextureTypeCube: type = COMPUTE_IMAGE_TYPE::IMAGE_CUBE; break;
#if !defined(FLOOR_IOS)
		case MTLTextureTypeCubeArray: type = COMPUTE_IMAGE_TYPE::IMAGE_CUBE_ARRAY; break;
#endif
		
		// yay for forwards compatibility, or at least detecting that something is wrong(tm) ...
FLOOR_PUSH_WARNINGS()
FLOOR_IGNORE_WARNING(covered-switch-default)
		default:
			log_error("invalid or unknown metal texture type: %X", [img textureType]);
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
		// BGRA
		{ MTLPixelFormatBGRA8Unorm, COMPUTE_IMAGE_TYPE::BGRA8UI_NORM },
		// depth / depth+stencil
		{ MTLPixelFormatDepth32Float, (COMPUTE_IMAGE_TYPE::FLOAT |
									   COMPUTE_IMAGE_TYPE::CHANNELS_1 |
									   COMPUTE_IMAGE_TYPE::FORMAT_32 |
									   COMPUTE_IMAGE_TYPE::FLAG_DEPTH) },
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
		log_error("unsupported image pixel format: %X", [img pixelFormat]);
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
	
	// handle mip-mapping / msaa flags (although both are possible with == 1 as well)
	if([img mipmapLevelCount] > 1) type |= COMPUTE_IMAGE_TYPE::FLAG_MIPMAPPED;
	if([img sampleCount] > 1) type |= COMPUTE_IMAGE_TYPE::FLAG_MSAA;
	
	if(([img usage] & MTLTextureUsageRenderTarget) != 0) {
		type |= COMPUTE_IMAGE_TYPE::FLAG_RENDERBUFFER;
	}
	
	return type;
}

metal_image::metal_image(shared_ptr<compute_device> device,
						 id <MTLTexture> external_image,
						 void* host_ptr_,
						 const COMPUTE_MEMORY_FLAG flags_) :
compute_image(device.get(), compute_metal_image_dim(external_image), compute_metal_image_type(external_image, flags_),
			  host_ptr_, flags_, 0, 0, nullptr), image(external_image), is_external(true) {
	// device must match
	if(((metal_device*)device.get())->device != [external_image device]) {
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

bool metal_image::create_internal(const bool copy_host_data, const metal_device* device, const compute_queue* cqueue) {
	// NOTE: opengl sharing flag is ignored, because there is no metal/opengl sharing and metal can interop with itself w/o explicit sharing
	
	// should not be called under that condition, but just to be safe
	if(is_external) {
		log_error("image is external!");
		return false;
	}
	
	// create an appropriate texture descriptor
	desc = [[MTLTextureDescriptor alloc] init];
	
	const auto dim_count = image_dim_count(image_type);
	const auto channel_count = image_channel_count(image_type);
	const bool is_array = has_flag<COMPUTE_IMAGE_TYPE::FLAG_ARRAY>(image_type);
	const bool is_cube = has_flag<COMPUTE_IMAGE_TYPE::FLAG_CUBE>(image_type);
	const bool is_msaa = has_flag<COMPUTE_IMAGE_TYPE::FLAG_MSAA>(image_type);
	const bool is_mipmapped = has_flag<COMPUTE_IMAGE_TYPE::FLAG_MIPMAPPED>(image_type);
	const bool is_compressed = image_compressed(image_type);
	auto mtl_device = device->device;
	if(is_msaa && is_array) {
		log_error("msaa array image not supported by metal!");
		return false;
	}
#if defined(FLOOR_IOS)
	if(is_cube && is_array) {
		log_error("cuba array image not support by metal (on ios)!");
	}
#endif
	
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
	
	if(is_array) {
		// 1D or 2D array?
		[desc setArrayLength:(dim_count == 1 ? image_dim.y : image_dim.z)];
	}
	else [desc setArrayLength:1];
	
	// type + nD handling
	MTLTextureType tex_type;
	switch(dim_count) {
		case 1:
			if(!is_array) tex_type = MTLTextureType1D;
			else tex_type = MTLTextureType1DArray;
			break;
		case 2:
			if(!is_array && !is_msaa && !is_cube) tex_type = MTLTextureType2D;
			else if(is_msaa) tex_type = MTLTextureType2DMultisample;
			else if(is_array) {
				if(!is_cube) tex_type = MTLTextureType2DArray;
#if !defined(FLOOR_IOS) // os x only for now
				else tex_type = MTLTextureTypeCubeArray;
#else
				else {
					log_error("cube array is not supported on iOS!");
					return false;
				}
#endif
			}
			else /* is_cube */ tex_type = MTLTextureTypeCube;
			break;
		case 3:
			tex_type = MTLTextureType3D;
			break;
		default:
			log_error("invalid dim count: %u", dim_count);
			return false;
	}
	[desc setTextureType:tex_type];
	
	// and now for the fun bit: pixel format conversion ...
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
		// BGRA
		{ COMPUTE_IMAGE_TYPE::BGRA8UI_NORM, MTLPixelFormatBGRA8Unorm },
		// depth / depth+stencil
		{ (COMPUTE_IMAGE_TYPE::FLOAT |
		   COMPUTE_IMAGE_TYPE::CHANNELS_1 |
		   COMPUTE_IMAGE_TYPE::FORMAT_32 |
		   COMPUTE_IMAGE_TYPE::FLAG_DEPTH), MTLPixelFormatDepth32Float },
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
	const auto metal_format = format_lut.find(image_type & (COMPUTE_IMAGE_TYPE::__DATA_TYPE_MASK |
															COMPUTE_IMAGE_TYPE::__CHANNELS_MASK |
															COMPUTE_IMAGE_TYPE::__COMPRESSION_MASK |
															COMPUTE_IMAGE_TYPE::__FORMAT_MASK |
															COMPUTE_IMAGE_TYPE::FLAG_NORMALIZED |
															COMPUTE_IMAGE_TYPE::FLAG_DEPTH |
															COMPUTE_IMAGE_TYPE::FLAG_STENCIL |
															COMPUTE_IMAGE_TYPE::FLAG_REVERSE |
															COMPUTE_IMAGE_TYPE::FLAG_SRGB));
	if(metal_format == end(format_lut)) {
		log_error("unsupported image format: %X", image_type);
		return false;
	}
	[desc setPixelFormat:metal_format->second];
	
	// set shim format to the corresponding 4-channel format
	// compressed images will always be used in their original state, even if they are RGB
	if(channel_count == 3 && !is_compressed) {
		shim_image_type = (image_type & ~COMPUTE_IMAGE_TYPE::__CHANNELS_MASK) | COMPUTE_IMAGE_TYPE::RGBA;
	}
	// == original type if not 3-channel -> 4-channel emulation
	else shim_image_type = image_type;
	
	// misc options
	const size_t level_count = image_mip_level_count(image_dim, image_type);
	[desc setMipmapLevelCount:level_count];
	[desc setSampleCount:1];
	
	// usage/access options
	[desc setResourceOptions:options];
	[desc setStorageMode:storage_options]; // don't know why this needs to be set twice
	[desc setUsage:usage_options];
	
	// create the actual metal image with the just created descriptor
	image = [mtl_device newTextureWithDescriptor:desc];
	
	// copy host memory to device if it is non-null and NO_INITIAL_COPY is not specified
	if(copy_host_data && host_ptr != nullptr && !has_flag<COMPUTE_MEMORY_FLAG::NO_INITIAL_COPY>(flags)) {
		// figure out where we're getting our command queue from
		metal_queue* mqueue = (metal_queue*)(dev != nullptr ? device->internal_queue : cqueue);
		
		// copy to device memory must go through a blit command
		id <MTLCommandBuffer> cmd_buffer = mqueue->make_command_buffer();
		id <MTLBlitCommandEncoder> blit_encoder = [cmd_buffer blitCommandEncoder];
		
		// NOTE: arrays/slices must be copied in per slice (for all else: there just is one slice)
		const size_t slice_count = [desc arrayLength] * (is_cube ? 6u : 1u);
		
		// upload level count == level count (if provided by user), or 1 if we have to manually generate the mip levels
		const size_t upload_level_count = (generate_mip_maps ? 1 : level_count);
		uint4 mip_image_dim {
			image_dim.x,
			dim_count >= 2 ? image_dim.y : 0,
			dim_count >= 3 ? image_dim.z : 0,
			0
		};
		const uint8_t* host_level_data = (const uint8_t*)host_ptr;
		for(size_t level = 0; level < upload_level_count; ++level, mip_image_dim >>= 1) {
			// NOTE: original size/type for non-3-channel types, and the 4-channel shim size/type for 3-channel types
			const auto bytes_per_row = image_bytes_per_pixel(shim_image_type) * mip_image_dim.x;
			const auto bytes_per_slice = image_slice_data_size_from_types(mip_image_dim, shim_image_type);
			const auto level_data_size = bytes_per_slice * slice_count;
			
			const uint8_t* data_ptr {
				image_type != shim_image_type ?
				// need to copy/convert the RGB host data to RGBA
				rgb_to_rgba(image_type, shim_image_type, host_level_data, true /* ignore mip levels as we do this manually */) :
				// else: can use host ptr directly
				host_level_data
			};
			
			const MTLRegion mipmap_region {
				.origin = { 0, 0, 0 },
				.size = {
					mip_image_dim.x,
					dim_count >= 2 ? mip_image_dim.y : 1,
					dim_count >= 3 ? mip_image_dim.z : 1,
				}
			};
			for(size_t slice = 0; slice < slice_count; ++slice) {
				[image replaceRegion:mipmap_region
						 mipmapLevel:level
							   slice:slice
						   withBytes:(data_ptr + slice * bytes_per_slice)
						 bytesPerRow:(is_compressed ? 0 : bytes_per_row)
					   bytesPerImage:(is_compressed ? 0 : bytes_per_slice)];
			}
			
			// clean up shim data
			if(image_type != shim_image_type) {
				delete [] data_ptr;
#if defined(__clang_analyzer__) // suppress false positive about use after free
				assert(!is_compressed);
#endif
			}
			
			// mip-level image data provided by user, advance pointer
			if(!generate_mip_maps) {
				host_level_data += level_data_size;
			}
		}
		
		// manually create the mip-map chain if this was specified
		if(is_mipmapped && level_count > 1 && generate_mip_maps) {
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

void metal_image::zero(shared_ptr<compute_queue> cqueue) {
	if(image == nil) return;
	
	id <MTLCommandBuffer> cmd_buffer = ((metal_queue*)cqueue.get())->make_command_buffer();
	id <MTLBlitCommandEncoder> blit_encoder = [cmd_buffer blitCommandEncoder];
	const auto bytes_per_row = image_bytes_per_pixel(shim_image_type) * image_dim.x;
	const auto bytes_per_slice = image_slice_data_size_from_types(image_dim, shim_image_type);
	const bool is_compressed = image_compressed(image_type);
	
	alignas(128) uint8_t* zero_data = new uint8_t[bytes_per_slice];
	memset(zero_data, 0, bytes_per_slice);
	
	const auto dim_count = image_dim_count(image_type);
	const MTLRegion region {
		.origin = { 0, 0, 0 },
		.size = {
			image_dim.x,
			dim_count >= 2 ? image_dim.y : 1,
			dim_count >= 3 ? image_dim.z : 1,
		}
	};
	const size_t slice_count = [desc arrayLength] * (has_flag<COMPUTE_IMAGE_TYPE::FLAG_CUBE>(image_type) ? 6u : 1u);
	for(size_t slice = 0; slice < slice_count; ++slice) {
		[image replaceRegion:region
				 mipmapLevel:0
					   slice:slice
				   withBytes:zero_data
				 bytesPerRow:(is_compressed ? 0 : bytes_per_row)
			   bytesPerImage:bytes_per_slice];
	}
	
	[blit_encoder endEncoding];
	[cmd_buffer commit];
	[cmd_buffer waitUntilCompleted];
	
	delete [] zero_data;
}

void* __attribute__((aligned(128))) metal_image::map(shared_ptr<compute_queue> cqueue,
													 const COMPUTE_MEMORY_MAP_FLAG flags_) {
	if(image == nil) return nullptr;
	
	// TODO: parameter origin + region + layer
	const auto dim_count = image_dim_count(image_type);
	const MTLRegion region {
		.origin = { 0, 0, 0 },
		.size = {
			image_dim.x,
			dim_count >= 2 ? image_dim.y : 1,
			dim_count >= 3 ? image_dim.z : 1,
		}
	};
	const size_t slice_count = [desc arrayLength] * (has_flag<COMPUTE_IMAGE_TYPE::FLAG_CUBE>(image_type) ? 6u : 1u);
	const auto map_size = region.size.width * region.size.height * region.size.depth * image_bytes_per_pixel(image_type) * slice_count;
	const auto shim_map_size = region.size.width * region.size.height * region.size.depth * image_bytes_per_pixel(shim_image_type) * slice_count;
	
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
	alignas(128) unsigned char* host_buffer = new unsigned char[map_size] alignas(128);
	
	// check if we need to copy the image from the device (in case READ was specified)
	if(!write_only) {
		// must finish up all current work before we can properly read from the current image
		cqueue->finish();
		
#if !defined(FLOOR_IOS)
		// need to sync image (resource) before being able to read it
		if((options & MTLResourceStorageModeMask) == MTLResourceStorageModeManaged) {
			metal_buffer::sync_metal_resource(cqueue, image);
		}
#endif
		
		// copy image data to the host
		id <MTLCommandBuffer> cmd_buffer = ((metal_queue*)cqueue.get())->make_command_buffer();
		id <MTLBlitCommandEncoder> blit_encoder = [cmd_buffer blitCommandEncoder];
		const auto bytes_per_row = image_bytes_per_pixel(shim_image_type) * image_dim.x;
		const auto bytes_per_slice = image_slice_data_size_from_types(image_dim, shim_image_type);
		const bool is_compressed = image_compressed(image_type);
		
		uint8_t* data_ptr {
			image_type != shim_image_type ?
			new uint8_t[shim_map_size] :
			host_buffer
		};
		for(size_t slice = 0; slice < slice_count; ++slice) {
			[image getBytes:(data_ptr + slice * bytes_per_slice)
				bytesPerRow:(is_compressed ? 0 : bytes_per_row)
			  bytesPerImage:bytes_per_slice
				 fromRegion:region
				mipmapLevel:0
					  slice:slice];
		}
		[blit_encoder endEncoding];
		[cmd_buffer commit];
		[cmd_buffer waitUntilCompleted];
		
		// convert to RGB + cleanup
		if(image_type != shim_image_type) {
			rgba_to_rgb(shim_image_type, image_type, data_ptr, host_buffer);
			delete [] data_ptr;
		}
#if defined(__clang_analyzer__) // suppress false positive about leaking memory
		else { assert(data_ptr == host_buffer); }
#endif
	}
	
	// need to remember how much we mapped and where (so the host->device write-back copies the right amount of bytes)
	mappings.emplace(host_buffer, metal_mapping { map_size, region, flags_, write_only });
	
	return host_buffer;
}

void metal_image::unmap(shared_ptr<compute_queue> cqueue, void* __attribute__((aligned(128))) mapped_ptr) {
	if(image == nil) return;
	if(mapped_ptr == nullptr) return;
	
	// check if this is actually a mapped pointer (+get the mapped size)
	const auto iter = mappings.find(mapped_ptr);
	if(iter == mappings.end()) {
		log_error("invalid mapped pointer: %X", mapped_ptr);
		return;
	}
	
	// check if we need to actually copy data back to the device (not the case if read-only mapping)
	if(has_flag<COMPUTE_MEMORY_MAP_FLAG::WRITE>(iter->second.flags) ||
	   has_flag<COMPUTE_MEMORY_MAP_FLAG::WRITE_INVALIDATE>(iter->second.flags)) {
		// copy host memory to device memory
		id <MTLCommandBuffer> cmd_buffer = ((metal_queue*)cqueue.get())->make_command_buffer();
		id <MTLBlitCommandEncoder> blit_encoder = [cmd_buffer blitCommandEncoder];
		const auto bytes_per_row = image_bytes_per_pixel(shim_image_type) * image_dim.x;
		const auto bytes_per_slice = image_slice_data_size_from_types(image_dim, shim_image_type);
		const bool is_compressed = image_compressed(image_type);
		
		// again, need to convert RGB to RGBA if necessary
		const uint8_t* data_ptr = (const uint8_t*)mapped_ptr;
		if(image_type != shim_image_type) {
			data_ptr = rgb_to_rgba(image_type, shim_image_type, (const uint8_t*)mapped_ptr);
		}
		
		const size_t slice_count = [desc arrayLength] * (has_flag<COMPUTE_IMAGE_TYPE::FLAG_CUBE>(image_type) ? 6u : 1u);
		for(size_t slice = 0; slice < slice_count; ++slice) {
			[image replaceRegion:iter->second.region
					 mipmapLevel:0
						   slice:slice
					   withBytes:(data_ptr + slice * bytes_per_slice)
					 bytesPerRow:(is_compressed ? 0 : bytes_per_row)
				   bytesPerImage:bytes_per_slice];
		}
		[blit_encoder endEncoding];
		[cmd_buffer commit];
		[cmd_buffer waitUntilCompleted];
		
		// cleanup
		if(image_type != shim_image_type) {
#if defined(__clang_analyzer__) // suppress false positive about freeing released memory
			assert(data_ptr != mapped_ptr);
#endif
			delete [] data_ptr;
		}
	}
	
	// free host memory again and remove the mapping
	delete [] (unsigned char*)mapped_ptr;
	mappings.erase(mapped_ptr);
}

bool metal_image::acquire_opengl_object(shared_ptr<compute_queue> cqueue floor_unused) {
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

bool metal_image::release_opengl_object(shared_ptr<compute_queue> cqueue floor_unused) {
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

void metal_image::generate_mip_map_chain(shared_ptr<compute_queue> cqueue) const {
	// nothing to do here
	if([image mipmapLevelCount] == 1) return;
	
	id <MTLCommandBuffer> cmd_buffer = ((metal_queue*)cqueue.get())->make_command_buffer();
	id <MTLBlitCommandEncoder> blit_encoder = [cmd_buffer blitCommandEncoder];
	[blit_encoder generateMipmapsForTexture:image];
	[blit_encoder endEncoding];
	[cmd_buffer commit];
	[cmd_buffer waitUntilCompleted];
}

#endif
