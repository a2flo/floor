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

#ifndef __FLOOR_COMPUTE_IMAGE_TYPES_HPP__
#define __FLOOR_COMPUTE_IMAGE_TYPES_HPP__

//! image type
enum class COMPUTE_IMAGE_TYPE : uint32_t {
	//! invalid/uninitialized
	NONE					= (0u),
	
	//////////////////////////////////////////
	// -> image flags and types
	//! upper 14-bit (18-31): type flags (currently used: 13/14)
	__FLAG_MASK				= (0xFFFC0000u),
	__FLAG_SHIFT			= (18u),
	//! base type: image is an array (aka has layers)
	FLAG_ARRAY				= (1u << (__FLAG_SHIFT + 0u)),
	//! base type: image is a buffer object
	FLAG_BUFFER				= (1u << (__FLAG_SHIFT + 1u)),
	//! base type: image uses mutli-sampling (consists of multiple samples)
	FLAG_MSAA				= (1u << (__FLAG_SHIFT + 2u)),
	//! base type: image is a cube map
	FLAG_CUBE				= (1u << (__FLAG_SHIFT + 3u)),
	//! base type: image is a depth image
	FLAG_DEPTH				= (1u << (__FLAG_SHIFT + 4u)),
	//! base type: image is a stencil image
	FLAG_STENCIL			= (1u << (__FLAG_SHIFT + 5u)),
	//! base type: image is a renderbuffer
	//! NOTE: only applicable when using opengl sharing
	FLAG_RENDERBUFFER		= (1u << (__FLAG_SHIFT + 6u)),
	//! optional type: image uses mip-mapping, i.e. has multiple LODs
	FLAG_MIPMAPPED			= (1u << (__FLAG_SHIFT + 7u)),
	//! optional type: image uses anisotropic filtering
	FLAG_ANISOTROPIC		= (1u << (__FLAG_SHIFT + 8u)),
	//! optional type: image doesn't need a sampler (i.e. only point/nearest/pixel sampled)
	//! NOTE: on some platforms this might provide better performance and/or less overhead
	FLAG_NO_SAMPLER			= (1u << (__FLAG_SHIFT + 9u)),
	//! optional type: image uses gather sampling (aka tld4/fetch4)
	FLAG_GATHER				= (1u << (__FLAG_SHIFT + 10u)),
	//! optional type: when using integer storage formats, the data is normalized in [0, 1]
	// TODO: actually needed/wanted?
	//FLAG_NORMALIZED_DATA	= (1u << (__FLAG_SHIFT + 11u)),
	//! optional type: image data is stored in reverse order (i.e. BGRA instead of RGBA)
	FLAG_REVERSE			= (1u << (__FLAG_SHIFT + 12u)),
	
	//! bits 16-17: dimensionality
	__DIM_MASK				= (0x00030000u),
	__DIM_SHIFT				= (16u),
	DIM_1D					= (1u << __DIM_SHIFT),
	DIM_2D					= (2u << __DIM_SHIFT),
	DIM_3D					= (3u << __DIM_SHIFT),
	
	//! bits 14-15: actual storage dimensionality (for arrays/layers: dim + 1, else: == dim)
	__DIM_STORAGE_MASK		= (0x0000C000u),
	__DIM_STORAGE_SHIFT		= (14u),
	DIM_STORAGE_1D			= (1u << __DIM_STORAGE_SHIFT),
	DIM_STORAGE_2D			= (2u << __DIM_STORAGE_SHIFT),
	DIM_STORAGE_3D			= (3u << __DIM_STORAGE_SHIFT),
	
	//! bits 12-13: channel count
	__CHANNELS_MASK			= (0x00003000u),
	__CHANNELS_SHIFT		= (12u),
	CHANNELS_1				= (0u << __CHANNELS_SHIFT),
	CHANNELS_2				= (1u << __CHANNELS_SHIFT),
	CHANNELS_3				= (2u << __CHANNELS_SHIFT),
	CHANNELS_4				= (3u << __CHANNELS_SHIFT),
	//! channel convenience aliases
	R 						= CHANNELS_1,
	RG 						= CHANNELS_2,
	RGB 					= CHANNELS_3,
	RGBA					= CHANNELS_4,
	
	//! bits 10-11: storage data type
	__DATA_TYPE_MASK		= (0x00000C00u),
	__DATA_TYPE_SHIFT		= (10u),
	INT						= (1u << __DATA_TYPE_SHIFT),
	UINT					= (2u << __DATA_TYPE_SHIFT),
	FLOAT					= (3u << __DATA_TYPE_SHIFT),
	
	//! bits 8-9: access qualifier
	__ACCESS_MASK			= (0x00000300u),
	__ACCESS_SHIFT			= (8u),
	//! image is read-only (exluding host operations)
	READ					= (1u << __ACCESS_SHIFT),
	//! image is write-only (exluding host operations)
	//! NOTE: automatically sets FLAG_NO_SAMPLER (since the image won't be read/sampled)
	WRITE					= (2u << __ACCESS_SHIFT) | FLAG_NO_SAMPLER,
	//! image is read-write
	//! NOTE: also applies if neither is set
	READ_WRITE				= (3u << __ACCESS_SHIFT),
	
	//! bits 0-7: formats
	//! NOTE: unless specified otherwise, a format is usable with any channel count
	__FORMAT_MASK			= (0x000000FFu),
	//! 2 bits per channel
	FORMAT_2				= (1u),
	//! 3 channel format: 3-bit/3-bit/2-bit
	FORMAT_3_3_2			= (2u),
	//! 4 bits per channel
	FORMAT_4				= (3u),
	//! 3 channel format: 5-bit/5-bit/5-bit
	FORMAT_5_5_5			= (4u),
	//! 4 channel format: 5-bit/5-bit/5-bit/1-bit
	FORMAT_5_5_5_1			= (5u),
	//! 3 channel format: 5-bit/6-bit/5-bit
	FORMAT_5_6_5			= (6u),
	//! 8 bits per channel
	FORMAT_8				= (7u),
	//! 3 channel format: 9-bit/9-bit/9-bit (5-bit exp)
	FORMAT_9_9_9_5			= (8u),
	//! 3 channel format: 10-bit/10-bit/10-bit
	FORMAT_10				= (9u),
	//! 4 channel format: 10-bit/10-bit/10-bit/2-bit
	FORMAT_10_10_10_2		= (10u),
	//! 3 channel format: 11-bit/11-bit/10-bit
	FORMAT_11_11_10			= (11u),
	//! 3 channel format: 12-bit/12-bit/12-bit
	FORMAT_12_12_12			= (12u),
	//! 4 channel format: 12-bit/12-bit/12-bit/12-bit
	FORMAT_12_12_12_12		= (13u),
	//! 16 bits per channel
	FORMAT_16				= (14u),
	//! 1 channel format: 24-bit
	FORMAT_24				= (15u),
	//! 2 channel format: 24-bit/8-bit
	FORMAT_24_8				= (16u),
	//! 32 bits per channel
	FORMAT_32				= (17u),
	//! 2 channel format: 32-bit/8-bit
	FORMAT_32_8				= (18u),
	//! 64 bits per channel
	FORMAT_64				= (19u),
	__FORMAT_MAX			= FORMAT_64,
	
	//////////////////////////////////////////
	// -> base image types
	//! 1D image
	IMAGE_1D				= DIM_1D | DIM_STORAGE_1D,
	//! array of 1D images
	IMAGE_1D_ARRAY			= DIM_1D | DIM_STORAGE_2D | FLAG_ARRAY,
	//! 1D image buffer (special format on some platforms)
	IMAGE_1D_BUFFER			= DIM_1D | DIM_STORAGE_1D | FLAG_BUFFER,
	
	//! 2D image
	IMAGE_2D				= DIM_2D | DIM_STORAGE_2D,
	//! array of 2D images
	IMAGE_2D_ARRAY			= DIM_2D | DIM_STORAGE_3D | FLAG_ARRAY,
	//! multi-sampled 2D image
	IMAGE_2D_MSAA			= DIM_2D | DIM_STORAGE_2D | FLAG_MSAA,
	//! array of multi-sampled 2D images
	IMAGE_2D_MSAA_ARRAY		= DIM_2D | DIM_STORAGE_3D | FLAG_MSAA | FLAG_ARRAY,
	
	//! 3D image
	IMAGE_3D				= DIM_3D | DIM_STORAGE_3D,
	//! cube map image
	IMAGE_CUBE				= DIM_2D | DIM_STORAGE_3D | FLAG_CUBE,
	//! array of cube map images
	IMAGE_CUBE_ARRAY		= DIM_2D | DIM_STORAGE_3D | FLAG_CUBE | FLAG_ARRAY,
	
	//! 2D depth image
	IMAGE_DEPTH				= FLAG_DEPTH | CHANNELS_1 | IMAGE_2D,
	//! combined 2D depth + stencil image
	IMAGE_DEPTH_STENCIL		= FLAG_DEPTH | CHANNELS_2 | IMAGE_2D | FLAG_STENCIL,
	//! array of 2D depth images
	IMAGE_DEPTH_ARRAY		= FLAG_DEPTH | CHANNELS_1 | IMAGE_2D_ARRAY,
	//! depth cube map image
	IMAGE_DEPTH_CUBE		= FLAG_DEPTH | CHANNELS_1 | IMAGE_CUBE,
	//! multi-sampled 2D depth image
	IMAGE_DEPTH_MSAA		= FLAG_DEPTH | CHANNELS_1 | IMAGE_2D_MSAA,
	//! array of multi-sampled 2D depth images
	IMAGE_DEPTH_MSAA_ARRAY	= FLAG_DEPTH | CHANNELS_1 | IMAGE_2D_MSAA_ARRAY,
	
	//////////////////////////////////////////
	// -> convenience aliases
	R8UI					= CHANNELS_1 | FORMAT_8 | UINT,
	RG8UI					= CHANNELS_2 | FORMAT_8 | UINT,
	RGB8UI					= CHANNELS_3 | FORMAT_8 | UINT,
	RGBA8UI					= CHANNELS_4 | FORMAT_8 | UINT,
	R8I						= CHANNELS_1 | FORMAT_8 | INT,
	RG8I					= CHANNELS_2 | FORMAT_8 | INT,
	RGB8I					= CHANNELS_3 | FORMAT_8 | INT,
	RGBA8I					= CHANNELS_4 | FORMAT_8 | INT,
	R16UI					= CHANNELS_1 | FORMAT_16 | UINT,
	RG16UI					= CHANNELS_2 | FORMAT_16 | UINT,
	RGB16UI					= CHANNELS_3 | FORMAT_16 | UINT,
	RGBA16UI				= CHANNELS_4 | FORMAT_16 | UINT,
	R16I					= CHANNELS_1 | FORMAT_16 | INT,
	RG16I					= CHANNELS_2 | FORMAT_16 | INT,
	RGB16I					= CHANNELS_3 | FORMAT_16 | INT,
	RGBA16I					= CHANNELS_4 | FORMAT_16 | INT,
	R16F					= CHANNELS_1 | FORMAT_16 | FLOAT,
	RG16F					= CHANNELS_2 | FORMAT_16 | FLOAT,
	RGB16F					= CHANNELS_3 | FORMAT_16 | FLOAT,
	RGBA16F					= CHANNELS_4 | FORMAT_16 | FLOAT,
	R32F					= CHANNELS_1 | FORMAT_32 | FLOAT,
	RG32F					= CHANNELS_2 | FORMAT_32 | FLOAT,
	RGB32F					= CHANNELS_3 | FORMAT_32 | FLOAT,
	RGBA32F					= CHANNELS_4 | FORMAT_32 | FLOAT,
	D16						= IMAGE_DEPTH | FORMAT_16 | UINT,
	D24						= IMAGE_DEPTH | FORMAT_24 | UINT,
	D32						= IMAGE_DEPTH | FORMAT_32 | UINT,
	D32F					= IMAGE_DEPTH | FORMAT_32 | FLOAT,
	DS24_8					= IMAGE_DEPTH_STENCIL | FORMAT_24_8 | UINT,
	DS32F_8					= IMAGE_DEPTH_STENCIL | FORMAT_32_8 | FLOAT,
	
};
floor_global_enum_ext(COMPUTE_IMAGE_TYPE)

//! returns the dimensionality of the specified image type
floor_inline_always static constexpr uint32_t image_dim_count(const COMPUTE_IMAGE_TYPE& image_type) {
	return uint32_t(image_type & COMPUTE_IMAGE_TYPE::__DIM_MASK) >> uint32_t(COMPUTE_IMAGE_TYPE::__DIM_SHIFT);
}

//! returns the storage dimensionality of the specified image type
floor_inline_always static constexpr uint32_t image_storage_dim_count(const COMPUTE_IMAGE_TYPE& image_type) {
	return uint32_t(image_type & COMPUTE_IMAGE_TYPE::__DIM_STORAGE_MASK) >> uint32_t(COMPUTE_IMAGE_TYPE::__DIM_STORAGE_SHIFT);
}

//! returns the channel count of the specified image type
floor_inline_always static constexpr uint32_t image_channel_count(const COMPUTE_IMAGE_TYPE& image_type) {
	return (uint32_t(image_type & COMPUTE_IMAGE_TYPE::__CHANNELS_MASK) >> uint32_t(COMPUTE_IMAGE_TYPE::__CHANNELS_SHIFT)) + 1u;
}

//! returns true if the specified image format/type is valid
//! NOTE: this currently only makes sure that the format corresponds to the channel count and that dim != 0
floor_inline_always static constexpr bool image_format_valid(const COMPUTE_IMAGE_TYPE& image_type) {
	if(image_dim_count(image_type) == 0) return false;
	if(image_storage_dim_count(image_type) == 0) return false;
	const auto channel_count = image_channel_count(image_type);
	switch(image_type & COMPUTE_IMAGE_TYPE::__FORMAT_MASK) {
		case COMPUTE_IMAGE_TYPE::FORMAT_3_3_2: return (channel_count == 3);
		case COMPUTE_IMAGE_TYPE::FORMAT_5_5_5: return (channel_count == 3);
		case COMPUTE_IMAGE_TYPE::FORMAT_5_5_5_1: return (channel_count == 4);
		case COMPUTE_IMAGE_TYPE::FORMAT_5_6_5: return (channel_count == 3);
		case COMPUTE_IMAGE_TYPE::FORMAT_9_9_9_5: return (channel_count == 3);
		case COMPUTE_IMAGE_TYPE::FORMAT_10: return (channel_count == 3);
		case COMPUTE_IMAGE_TYPE::FORMAT_10_10_10_2: return (channel_count == 4);
		case COMPUTE_IMAGE_TYPE::FORMAT_11_11_10: return (channel_count == 3);
		case COMPUTE_IMAGE_TYPE::FORMAT_12_12_12: return (channel_count == 3);
		case COMPUTE_IMAGE_TYPE::FORMAT_12_12_12_12: return (channel_count == 4);
		case COMPUTE_IMAGE_TYPE::FORMAT_24: return (channel_count == 1);
		case COMPUTE_IMAGE_TYPE::FORMAT_24_8: return (channel_count == 2);
		case COMPUTE_IMAGE_TYPE::FORMAT_32_8: return (channel_count == 2);
		default: break;
	}
	return true;
}

//! returns the amount of bits needed to store one pixel
static constexpr uint32_t image_bits_per_pixel(const COMPUTE_IMAGE_TYPE& image_type) {
	const auto channel_count = image_channel_count(image_type);
	switch(image_type & COMPUTE_IMAGE_TYPE::__FORMAT_MASK) {
		// arbitrary channel formats
		case COMPUTE_IMAGE_TYPE::FORMAT_2: return 2 * channel_count;
		case COMPUTE_IMAGE_TYPE::FORMAT_4: return 4 * channel_count;
		case COMPUTE_IMAGE_TYPE::FORMAT_8: return 8 * channel_count;
		case COMPUTE_IMAGE_TYPE::FORMAT_16: return 16 * channel_count;
		case COMPUTE_IMAGE_TYPE::FORMAT_32: return 32 * channel_count;
		case COMPUTE_IMAGE_TYPE::FORMAT_64: return 64 * channel_count;
		
		// special channel specific formats
		case COMPUTE_IMAGE_TYPE::FORMAT_3_3_2: return 8;
		case COMPUTE_IMAGE_TYPE::FORMAT_5_5_5: return 15;
		case COMPUTE_IMAGE_TYPE::FORMAT_5_5_5_1: return 16;
		case COMPUTE_IMAGE_TYPE::FORMAT_5_6_5: return 16;
		case COMPUTE_IMAGE_TYPE::FORMAT_9_9_9_5: return 32;
		case COMPUTE_IMAGE_TYPE::FORMAT_10: return 30;
		case COMPUTE_IMAGE_TYPE::FORMAT_10_10_10_2: return 32;
		case COMPUTE_IMAGE_TYPE::FORMAT_11_11_10: return 32;
		case COMPUTE_IMAGE_TYPE::FORMAT_12_12_12: return 36;
		case COMPUTE_IMAGE_TYPE::FORMAT_12_12_12_12: return 48;
		case COMPUTE_IMAGE_TYPE::FORMAT_24: return 24;
		case COMPUTE_IMAGE_TYPE::FORMAT_24_8: return 32;
		case COMPUTE_IMAGE_TYPE::FORMAT_32_8: return 40;
		default: return 0;
	}
}

//! returns the amount of bytes needed to store one pixel
//! NOTE: rounded up if "bits per pixel" is not divisible by 8
static constexpr uint32_t image_bytes_per_pixel(const COMPUTE_IMAGE_TYPE& image_type) {
	const auto bpp = image_bits_per_pixel(image_type);
	return ((bpp + 7u) / 8u);
}

//! returns the total amount of bytes needed to store the image of the specified dimensions and types
static constexpr size_t image_data_size_from_types(const uint4& image_dim, const COMPUTE_IMAGE_TYPE& image_type) {
	const auto dim_count = image_dim_count(image_type);
	size_t size = size_t(image_dim.x);
	if(dim_count >= 2) size *= size_t(image_dim.y);
	if(dim_count == 3) size *= size_t(image_dim.z);
	
	if(has_flag<COMPUTE_IMAGE_TYPE::FLAG_ARRAY>(image_type)) {
		// array count after: width (, height (, depth))
		size *= size_t(dim_count == 3 ? image_dim.w : (dim_count == 2 ? image_dim.z : image_dim.y));
	}
	
	if(has_flag<COMPUTE_IMAGE_TYPE::FLAG_CUBE>(image_type)) {
		// 6 cube sides
		size *= 6u;
	}
	
	// TODO: make sure special formats correspond to channel count
	size *= image_bytes_per_pixel(image_type);
	
	return size;
}

#endif
