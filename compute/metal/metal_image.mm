/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2015 Florian Ziesche
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

//! converts RGB data to RGBA data and returns the owning RGBA image data pointer
static uint8_t* rgb_to_rgba(const COMPUTE_IMAGE_TYPE& rgb_type,
							const COMPUTE_IMAGE_TYPE& rgba_type,
							const uint4& image_dim,
							const uint8_t* rgb_data) {
	// need to copy/convert the RGB host data to RGBA
	const auto rgba_size = image_data_size_from_types(image_dim, rgba_type);
	const auto rgb_bytes_per_pixel = image_bytes_per_pixel(rgb_type);
	const auto rgba_bytes_per_pixel = image_bytes_per_pixel(rgba_type);
	
	uint8_t* rgba_data_ptr = new uint8_t[rgba_size];
	memset(rgba_data_ptr, 0xFF, rgba_size); // opaque
	for(size_t i = 0, count = rgba_size / rgba_bytes_per_pixel; i < count; ++i) {
		memcpy(&rgba_data_ptr[i * rgba_bytes_per_pixel],
			   &((const uint8_t*)rgb_data)[rgb_bytes_per_pixel * i],
			   rgb_bytes_per_pixel);
	}
	return rgba_data_ptr;
}

//! converts RGBA data to RGB data. if "dst_rgb_data" is non-null, the RGB data is directly written to it and no memory is allocated
//! and nullptr is returned. otherwise RGB image data is allocated and an owning pointer to it is returned.
static uint8_t* rgba_to_rgb(const COMPUTE_IMAGE_TYPE& rgba_type,
							const COMPUTE_IMAGE_TYPE& rgb_type,
							const uint4& image_dim,
							const uint8_t* rgba_data,
							uint8_t* dst_rgb_data = nullptr) {
	// need to copy/convert the RGB host data to RGBA
	const auto rgba_size = image_data_size_from_types(image_dim, rgba_type);
	const auto rgb_size = image_data_size_from_types(image_dim, rgb_type);
	const auto rgb_bytes_per_pixel = image_bytes_per_pixel(rgb_type);
	const auto rgba_bytes_per_pixel = image_bytes_per_pixel(rgba_type);
	
	uint8_t* rgb_data_ptr = (dst_rgb_data != nullptr ? dst_rgb_data : new uint8_t[rgb_size]);
	for(size_t i = 0, count = rgba_size / rgba_bytes_per_pixel; i < count; ++i) {
		memcpy(&rgb_data_ptr[i * rgb_bytes_per_pixel],
			   &((const uint8_t*)rgba_data)[rgba_bytes_per_pixel * i],
			   rgb_bytes_per_pixel);
	}
	return (dst_rgb_data != nullptr ? nullptr : rgb_data_ptr);
}

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
#if !defined(FLOOR_IOS)
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
#endif
	
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
	
#if !defined(FLOOR_IOS)
	if((flags & COMPUTE_MEMORY_FLAG::HOST_READ_WRITE) == COMPUTE_MEMORY_FLAG::NONE) {
		options |= MTLResourceStorageModePrivate;
		storage_options = MTLStorageModePrivate;
	}
	else {
		options |= MTLResourceStorageModeManaged;
		storage_options = MTLStorageModeManaged;
	}
#endif
	
	// actually create the image
	if(!create_internal(true, device, nullptr)) {
		return; // can't do much else
	}
}

bool metal_image::create_internal(const bool copy_host_data, const metal_device* device, const compute_queue* cqueue) {
	// NOTE: opengl sharing flag is ignored, because there is no metal/opengl sharing and metal can interop with itself w/o explicit sharing
	
	// create an appropriate texture descriptor
	desc = [[MTLTextureDescriptor alloc] init];
	
	const auto dim_count = image_dim_count(image_type);
	const auto channel_count = image_channel_count(image_type);
	const bool is_array = has_flag<COMPUTE_IMAGE_TYPE::FLAG_ARRAY>(image_type);
	const bool is_cube = has_flag<COMPUTE_IMAGE_TYPE::FLAG_CUBE>(image_type);
	const bool is_msaa = has_flag<COMPUTE_IMAGE_TYPE::FLAG_MSAA>(image_type);
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
		// depth / depth+stencil
		{ (COMPUTE_IMAGE_TYPE::FLOAT |
		   COMPUTE_IMAGE_TYPE::CHANNELS_1 |
		   COMPUTE_IMAGE_TYPE::FORMAT_32 |
		   COMPUTE_IMAGE_TYPE::FLAG_DEPTH), MTLPixelFormatDepth32Float },
#if !defined(FLOOR_IOS) // os x only (or ios 9)
		{ (COMPUTE_IMAGE_TYPE::UINT |
		   COMPUTE_IMAGE_TYPE::CHANNELS_2 |
		   COMPUTE_IMAGE_TYPE::FORMAT_24_8 |
		   COMPUTE_IMAGE_TYPE::FLAG_DEPTH |
		   COMPUTE_IMAGE_TYPE::FLAG_STENCIL), MTLPixelFormatDepth24Unorm_Stencil8 },
		{ (COMPUTE_IMAGE_TYPE::FLOAT |
		   COMPUTE_IMAGE_TYPE::CHANNELS_2 |
		   COMPUTE_IMAGE_TYPE::FORMAT_32_8 |
		   COMPUTE_IMAGE_TYPE::FLAG_DEPTH |
		   COMPUTE_IMAGE_TYPE::FLAG_STENCIL), MTLPixelFormatDepth32Float_Stencil8 },
#endif
		// TODO: special image formats, these are partially supported
	};
	const auto metal_format = format_lut.find(image_type & (COMPUTE_IMAGE_TYPE::__DATA_TYPE_MASK |
															COMPUTE_IMAGE_TYPE::__CHANNELS_MASK |
															COMPUTE_IMAGE_TYPE::__FORMAT_MASK |
															COMPUTE_IMAGE_TYPE::FLAG_NORMALIZED |
															COMPUTE_IMAGE_TYPE::FLAG_DEPTH |
															COMPUTE_IMAGE_TYPE::FLAG_STENCIL));
	if(metal_format == end(format_lut)) {
		log_error("unsupported image format: %X", image_type);
		return false;
	}
	[desc setPixelFormat:metal_format->second];
	
	// set shim format to the corresponding 4-channel format
	if(channel_count == 3) {
		shim_image_type = (image_type & ~COMPUTE_IMAGE_TYPE::__CHANNELS_MASK) | COMPUTE_IMAGE_TYPE::RGBA;
	}
	// == original type if not 3-channel -> 4-channel emulation
	else shim_image_type = image_type;
	
	// misc options
	if(!has_flag<COMPUTE_IMAGE_TYPE::FLAG_MIPMAPPED>(image_type)) {
		[desc setMipmapLevelCount:1];
	}
	else {
		const auto max_dim = std::max(std::max(region.size.width, region.size.height), region.size.depth);
		if(max_dim > 1) {
			// each mip level is half the size of its upper/parent level, until dim == 1
			// -> get the closest power-of-two, then "ln(2^N) + 1"
			// this can be done the fastest by counting the leading zeros in the 32-bit value
			[desc setMipmapLevelCount:uint32_t(32 - __builtin_clz((uint32_t)const_math::next_pot(max_dim)))];
		}
		else [desc setMipmapLevelCount:1];
	}
	[desc setSampleCount:1];
	
	// usage/access options
	[desc setResourceOptions:options];
#if !defined(FLOOR_IOS)
	[desc setStorageMode:storage_options]; // don't know why this needs to be set twice
	[desc setUsage:usage_options];
#endif
	
	// create the actual metal image with the just created descriptor
	image = [mtl_device newTextureWithDescriptor:desc];
	
	// copy host memory to device if it is non-null and NO_INITIAL_COPY is not specified
	if(copy_host_data && host_ptr != nullptr && !has_flag<COMPUTE_MEMORY_FLAG::NO_INITIAL_COPY>(flags)) {
		// figure out where we're getting our command queue from
		metal_queue* mqueue = (metal_queue*)(dev != nullptr ? device->internal_queue : cqueue);
		
		// copy to device memory must go through a blit command
		// TODO: handle arrays/slices correctly!
		id <MTLCommandBuffer> cmd_buffer = mqueue->make_command_buffer();
		id <MTLBlitCommandEncoder> blit_encoder = [cmd_buffer blitCommandEncoder];
		
		// NOTE: original size/type for non-3-channel types, and the 4-channel shim size/type for 3-channel types
		const auto bytes_per_row = (dim_count == 0 ? 0 : image_bytes_per_pixel(shim_image_type) * image_dim.x);
		const auto bytes_per_slice = image_slice_data_size_from_types(image_dim, shim_image_type);
		
		const void* data_ptr = host_ptr;
		if(image_type != shim_image_type) {
			// need to copy/convert the RGB host data to RGBA
			data_ptr = rgb_to_rgba(image_type, shim_image_type, image_dim, (const uint8_t*)host_ptr);
		}
		
		[image replaceRegion:region
				 mipmapLevel:0
					   slice:0
				   withBytes:data_ptr
				 bytesPerRow:bytes_per_row
			   bytesPerImage:bytes_per_slice];
		
		// also create mip-map chain when initializing from a host pointer and mip-map flag is set
		if(has_flag<COMPUTE_IMAGE_TYPE::FLAG_MIPMAPPED>(image_type) &&
		   [desc mipmapLevelCount] > 1) {
			[blit_encoder generateMipmapsForTexture:image];
		}
		
		[blit_encoder endEncoding];
		[cmd_buffer commit];
		[cmd_buffer waitUntilCompleted];
		
		// clean up shim data
		if(image_type != shim_image_type) {
			delete [] (const uint8_t*)data_ptr;
		}
	}
	
	return true;
}

metal_image::~metal_image() {
	// kill the image
	image = nil;
	if(desc != nil) {
		[desc dealloc];
		desc = nil;
	}
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
	// TODO: cube/array
	const auto map_size = region.size.width * region.size.height * region.size.depth * image_bytes_per_pixel(image_type);
	
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
		// TODO: handle arrays/slices correctly!
		id <MTLCommandBuffer> cmd_buffer = ((metal_queue*)cqueue.get())->make_command_buffer();
		id <MTLBlitCommandEncoder> blit_encoder = [cmd_buffer blitCommandEncoder];
		const auto bytes_per_row = (dim_count == 0 ? 0 : image_bytes_per_pixel(shim_image_type) * image_dim.x);
		const auto bytes_per_slice = image_slice_data_size_from_types(image_dim, shim_image_type);
		
		uint8_t* data_ptr = host_buffer;
		if(image_type != shim_image_type) {
			const auto shim_size = region.size.width * region.size.height * region.size.depth * image_bytes_per_pixel(shim_image_type);
			data_ptr = new uint8_t[shim_size];
		}
		
		[image getBytes:data_ptr
			bytesPerRow:bytes_per_row
		  bytesPerImage:bytes_per_slice
			 fromRegion:region
			mipmapLevel:0
				  slice:0];
		[blit_encoder endEncoding];
		[cmd_buffer commit];
		[cmd_buffer waitUntilCompleted];
		
		// convert to RGB + cleanup
		if(image_type != shim_image_type) {
			rgba_to_rgb(shim_image_type, image_type, image_dim, data_ptr, host_buffer);
			delete [] data_ptr;
		}
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
		// TODO: handle arrays/slices correctly!
		const auto dim_count = image_dim_count(image_type);
		id <MTLCommandBuffer> cmd_buffer = ((metal_queue*)cqueue.get())->make_command_buffer();
		id <MTLBlitCommandEncoder> blit_encoder = [cmd_buffer blitCommandEncoder];
		const auto bytes_per_row = (dim_count == 0 ? 0 : image_bytes_per_pixel(shim_image_type) * image_dim.x);
		const auto bytes_per_slice = image_slice_data_size_from_types(image_dim, shim_image_type);
		
		// again, need to convert RGB to RGBA if necessary
		const uint8_t* data_ptr = (const uint8_t*)mapped_ptr;
		if(image_type != shim_image_type) {
			data_ptr = rgb_to_rgba(image_type, shim_image_type, image_dim, (const uint8_t*)mapped_ptr);
		}
		
		[image replaceRegion:iter->second.region
				 mipmapLevel:0
					   slice:0
				   withBytes:data_ptr
				 bytesPerRow:bytes_per_row
			   bytesPerImage:bytes_per_slice];
		[blit_encoder endEncoding];
		[cmd_buffer commit];
		[cmd_buffer waitUntilCompleted];
		
		// cleanup
		if(image_type != shim_image_type) {
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
		log_warn("image has already been acquired!");
		return true;
	}
	gl_object_state = true;
	return true;
}

bool metal_image::release_opengl_object(shared_ptr<compute_queue> cqueue floor_unused) {
	if(image == nil) return false;
	if(!gl_object_state) {
		log_warn("image has already been released!");
		return true;
	}
	gl_object_state = false;
	return true;
}

void metal_image::generate_mip_maps(shared_ptr<compute_queue> cqueue) const {
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
