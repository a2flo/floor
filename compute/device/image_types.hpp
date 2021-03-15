/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2021 Florian Ziesche
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
enum class COMPUTE_IMAGE_TYPE : uint64_t {
	//! invalid/uninitialized
	NONE					= (0ull),
	
	//////////////////////////////////////////
	// -> image flags and types
	
	//! bits 60-63: extended type flags
	__EXT_FLAG_MASK			= (0xF000'0000'0000'0000ull),
	__EXT_FLAG_SHIFT		= (60ull),
	//! extended type: in combination with FLAG_MSAA, an MSAA image can be made transient, i.e. does not need to be stored in memory
	//! NOTE: only applicable for Metal and Vulkan
	FLAG_TRANSIENT			= (1ull << (__EXT_FLAG_SHIFT + 0ull)),
	__UNUSED_EXT_FLAG_1		= (1ull << (__EXT_FLAG_SHIFT + 1ull)),
	__UNUSED_EXT_FLAG_2		= (1ull << (__EXT_FLAG_SHIFT + 2ull)),
	__UNUSED_EXT_FLAG_3		= (1ull << (__EXT_FLAG_SHIFT + 3ull)),
	
	//! bits 38-59: unused
	
	//! bits 35-37: anisotropy (stored as power-of-two)
	__ANISOTROPY_MASK		= (0x0000'0038'0000'0000ull),
	__ANISOTROPY_SHIFT		= (35ull),
	ANISOTROPY_1			= (0ull << __ANISOTROPY_SHIFT),
	ANISOTROPY_2			= (1ull << __ANISOTROPY_SHIFT),
	ANISOTROPY_4			= (2ull << __ANISOTROPY_SHIFT),
	ANISOTROPY_8			= (3ull << __ANISOTROPY_SHIFT),
	ANISOTROPY_16			= (4ull << __ANISOTROPY_SHIFT),
	
	//! bits 32-34: multi-sampling sample count (stored as power-of-two)
	__SAMPLE_COUNT_MASK		= (0x0000'0007'0000'0000ull),
	__SAMPLE_COUNT_SHIFT	= (32ull),
	SAMPLE_COUNT_1			= (0ull << __SAMPLE_COUNT_SHIFT),
	SAMPLE_COUNT_2			= (1ull << __SAMPLE_COUNT_SHIFT),
	SAMPLE_COUNT_4			= (2ull << __SAMPLE_COUNT_SHIFT),
	SAMPLE_COUNT_8			= (3ull << __SAMPLE_COUNT_SHIFT),
	SAMPLE_COUNT_16			= (4ull << __SAMPLE_COUNT_SHIFT),
	SAMPLE_COUNT_32			= (5ull << __SAMPLE_COUNT_SHIFT),
	SAMPLE_COUNT_64			= (6ull << __SAMPLE_COUNT_SHIFT),
	
	//! bits 20-31: type flags
	__FLAG_MASK				= (0x0000'0000'FFFC'0000ull),
	__FLAG_SHIFT			= (20ull),
	//! base type: image is an array (aka has layers)
	FLAG_ARRAY				= (1ull << (__FLAG_SHIFT + 0ull)),
	//! base type: image is a buffer object
	FLAG_BUFFER				= (1ull << (__FLAG_SHIFT + 1ull)),
	//! base type: image uses mutli-sampling (consists of multiple samples)
	FLAG_MSAA				= (1ull << (__FLAG_SHIFT + 2ull)),
	//! base type: image is a cube map
	FLAG_CUBE				= (1ull << (__FLAG_SHIFT + 3ull)),
	//! base type: image is a depth image
	FLAG_DEPTH				= (1ull << (__FLAG_SHIFT + 4ull)),
	//! base type: image is a stencil image
	FLAG_STENCIL			= (1ull << (__FLAG_SHIFT + 5ull)),
	//! base type: image is a render target (Metal) / renderbuffer (OpenGL) / framebuffer attachment (Vulkan)
	//! NOTE: only applicable when using OpenGL sharing, Metal or Vulkan
	FLAG_RENDER_TARGET		= (1ull << (__FLAG_SHIFT + 6ull)),
	//! optional type: image uses mip-mapping, i.e. has multiple LODs
	FLAG_MIPMAPPED			= (1ull << (__FLAG_SHIFT + 7ull)),
	//! optional type: image uses a fixed channel count
	//! NOTE: only used internally, serves no purpose on the user-side
	FLAG_FIXED_CHANNELS		= (1ull << (__FLAG_SHIFT + 8ull)),
	//! optional type: image uses gather sampling (aka tld4/fetch4)
	FLAG_GATHER				= (1ull << (__FLAG_SHIFT + 9ull)),
	//! optional type: when using integer storage formats, the data is normalized in [0, 1]
	FLAG_NORMALIZED			= (1ull << (__FLAG_SHIFT + 10ull)),
	//! optional type: image data contains sRGB data
	FLAG_SRGB				= (1ull << (__FLAG_SHIFT + 11ull)),
	
	//! bits 18-19: channel layout
	__LAYOUT_MASK			= (0x0000'0000'000C'0000ull),
	__LAYOUT_SHIFT			= (18ull),
	LAYOUT_RGBA				= (0ull << __LAYOUT_SHIFT),
	LAYOUT_BGRA				= (1ull << __LAYOUT_SHIFT),
	LAYOUT_ABGR				= (2ull << __LAYOUT_SHIFT),
	LAYOUT_ARGB				= (3ull << __LAYOUT_SHIFT),
	//! layout convenience aliases
	LAYOUT_R				= LAYOUT_RGBA,
	LAYOUT_RG				= LAYOUT_RGBA,
	LAYOUT_RGB				= LAYOUT_RGBA,
	LAYOUT_BGR				= LAYOUT_BGRA,
	
	//! bits 16-17: dimensionality
	//! NOTE: cube maps and arrays use the dimensionality of their underlying image data
	//!       -> 2D for cube maps, 2D for 2D arrays, 1D for 1D arrays
	__DIM_MASK				= (0x0000'0000'0003'0000ull),
	__DIM_SHIFT				= (16ull),
	DIM_1D					= (1ull << __DIM_SHIFT),
	DIM_2D					= (2ull << __DIM_SHIFT),
	DIM_3D					= (3ull << __DIM_SHIFT),
	
	//! bits 14-15: channel count
	__CHANNELS_MASK			= (0x0000'0000'0000'C000ull),
	__CHANNELS_SHIFT		= (14ull),
	CHANNELS_1				= (0ull << __CHANNELS_SHIFT),
	CHANNELS_2				= (1ull << __CHANNELS_SHIFT),
	CHANNELS_3				= (2ull << __CHANNELS_SHIFT),
	CHANNELS_4				= (3ull << __CHANNELS_SHIFT),
	//! channel convenience aliases
	R 						= CHANNELS_1,
	RG 						= CHANNELS_2,
	RGB 					= CHANNELS_3,
	RGBA					= CHANNELS_4,
	
	//! bits 12-13: storage data type
	__DATA_TYPE_MASK		= (0x0000'0000'0000'3000ull),
	__DATA_TYPE_SHIFT		= (12ull),
	INT						= (1ull << __DATA_TYPE_SHIFT),
	UINT					= (2ull << __DATA_TYPE_SHIFT),
	FLOAT					= (3ull << __DATA_TYPE_SHIFT),
	
	//! bits 10-11: access qualifier
	__ACCESS_MASK			= (0x0000'0000'0000'0C00ull),
	__ACCESS_SHIFT			= (10ull),
	//! image is read-only (exluding host operations)
	READ					= (1ull << __ACCESS_SHIFT),
	//! image is write-only (exluding host operations)
	WRITE					= (2ull << __ACCESS_SHIFT),
	//! image is read-write
	//! NOTE: also applies if neither is set
	READ_WRITE				= (READ | WRITE),
	
	//! bits 6-9: compressed formats
	__COMPRESSION_MASK		= (0x0000'0000'0000'03C0),
	__COMPRESSION_SHIFT		= (6ull),
	//! image data is not compressed
	UNCOMPRESSED			= (0ull << __COMPRESSION_SHIFT),
	//! S3TC/DXTn
	BC1						= (1ull << __COMPRESSION_SHIFT),
	BC2						= (2ull << __COMPRESSION_SHIFT),
	BC3						= (3ull << __COMPRESSION_SHIFT),
	//! RGTC1/RGTC2
	RGTC					= (4ull << __COMPRESSION_SHIFT),
	BC4						= RGTC,
	BC5						= RGTC,
	//! BPTC/BPTC_FLOAT
	BPTC					= (5ull << __COMPRESSION_SHIFT),
	BC6H					= BPTC,
	BC7						= BPTC,
	//! PVRTC
	PVRTC					= (6ull << __COMPRESSION_SHIFT),
	//! PVRTC2
	PVRTC2					= (7ull << __COMPRESSION_SHIFT),
	//! EAC/ETC1
	EAC						= (8ull << __COMPRESSION_SHIFT),
	ETC1					= EAC,
	//! ETC2
	ETC2					= (9ull << __COMPRESSION_SHIFT),
	//! ASTC
	ASTC					= (10ull << __COMPRESSION_SHIFT),
	
	//! bits 0-5: formats
	//! NOTE: unless specified otherwise, a format is usable with any channel count
	//! NOTE: not all backends support all formats (for portability, stick to 8-bit/16-bit/32-bit)
	//! NOTE: channel layout / order is determined by LAYOUT_* -> bit/channel order in here can be different to the actual layout
	__FORMAT_MASK			= (0x0000'0000'0000'003Full),
	//! 1 bit per channel
	FORMAT_1				= (1ull),
	//! 2 bits per channel
	FORMAT_2				= (2ull),
	//! 3 channel format: 3-bit/3-bit/2-bit
	FORMAT_3_3_2			= (3ull),
	//! 4 bits per channel or YUV444
	FORMAT_4				= (4ull),
	//! YUV420
	FORMAT_4_2_0			= (5ull),
	//! YUV411
	FORMAT_4_1_1			= (6ull),
	//! YUV422
	FORMAT_4_2_2			= (7ull),
	//! 3 channel format: 5-bit/5-bit/5-bit
	FORMAT_5_5_5			= (8ull),
	//! 4 channel format: 5-bit/5-bit/5-bit/1-bit
	FORMAT_5_5_5_ALPHA_1	= (9ull),
	//! 3 channel format: 5-bit/6-bit/5-bit
	FORMAT_5_6_5			= (10ull),
	//! 8 bits per channel
	FORMAT_8				= (11ull),
	//! 3 channel format: 9-bit/9-bit/9-bit (5-bit exp)
	FORMAT_9_9_9_EXP_5		= (12ull),
	//! 3 or 4 channel format: 10-bit/10-bit/10-bit(/10-bit)
	FORMAT_10				= (13ull),
	//! 4 channel format: 10-bit/10-bit/10-bit/2-bit
	FORMAT_10_10_10_ALPHA_2	= (14ull),
	//! 3 channel format: 11-bit/11-bit/10-bit
	FORMAT_11_11_10			= (15ull),
	//! 3 channel format: 12-bit/12-bit/12-bit
	FORMAT_12_12_12			= (16ull),
	//! 4 channel format: 12-bit/12-bit/12-bit/12-bit
	FORMAT_12_12_12_12		= (17ull),
	//! 16 bits per channel
	FORMAT_16				= (18ull),
	//! 2 channel format: 16-bit/8-bit
	FORMAT_16_8				= (19ull),
	//! 1 channel format: 24-bit
	FORMAT_24				= (20ull),
	//! 2 channel format: 24-bit/8-bit
	FORMAT_24_8				= (21ull),
	//! 32 bits per channel
	FORMAT_32				= (22ull),
	//! 2 channel format: 32-bit/8-bit
	FORMAT_32_8				= (23ull),
	//! 64 bits per channel
	FORMAT_64				= (24ull),
	__FORMAT_MAX			= FORMAT_64,
	
	//////////////////////////////////////////
	// -> base image types
	//! 1D image
	IMAGE_1D				= DIM_1D,
	//! array of 1D images
	IMAGE_1D_ARRAY			= DIM_1D | FLAG_ARRAY,
	//! 1D image buffer (special format on some platforms)
	IMAGE_1D_BUFFER			= DIM_1D | FLAG_BUFFER,
	
	//! 2D image
	IMAGE_2D				= DIM_2D,
	//! array of 2D images
	IMAGE_2D_ARRAY			= DIM_2D | FLAG_ARRAY,
	//! multi-sampled 2D image
	IMAGE_2D_MSAA			= DIM_2D | FLAG_MSAA,
	//! array of multi-sampled 2D images
	IMAGE_2D_MSAA_ARRAY		= DIM_2D | FLAG_MSAA | FLAG_ARRAY,
	
	//! cube map image
	IMAGE_CUBE				= DIM_2D | FLAG_CUBE,
	//! array of cube map images
	IMAGE_CUBE_ARRAY		= DIM_2D | FLAG_CUBE | FLAG_ARRAY,
	
	//! 2D depth image
	IMAGE_DEPTH				= FLAG_DEPTH | CHANNELS_1 | IMAGE_2D,
	//! combined 2D depth + stencil image
	IMAGE_DEPTH_STENCIL		= FLAG_DEPTH | CHANNELS_2 | IMAGE_2D | FLAG_STENCIL,
	//! array of 2D depth images
	IMAGE_DEPTH_ARRAY		= FLAG_DEPTH | CHANNELS_1 | IMAGE_2D_ARRAY,
	//! depth cube map image
	IMAGE_DEPTH_CUBE		= FLAG_DEPTH | CHANNELS_1 | IMAGE_CUBE,
	//! array of depth cube map images
	IMAGE_DEPTH_CUBE_ARRAY	= FLAG_DEPTH | CHANNELS_1 | IMAGE_CUBE | FLAG_ARRAY,
	//! multi-sampled 2D depth image
	IMAGE_DEPTH_MSAA		= FLAG_DEPTH | CHANNELS_1 | IMAGE_2D_MSAA,
	//! array of multi-sampled 2D depth images
	IMAGE_DEPTH_MSAA_ARRAY	= FLAG_DEPTH | CHANNELS_1 | IMAGE_2D_MSAA_ARRAY,
	
	//! 3D image
	IMAGE_3D				= DIM_3D,
	
	//////////////////////////////////////////
	// -> convenience aliases
	
	//! normalized unsigned integer formats (for consistency with opengl, without a UI and _NORM suffix)
	R8						= CHANNELS_1 | FORMAT_8 | UINT | FLAG_NORMALIZED,
	RG8						= CHANNELS_2 | FORMAT_8 | UINT | FLAG_NORMALIZED,
	RGB8					= CHANNELS_3 | FORMAT_8 | UINT | FLAG_NORMALIZED,
	BGR8					= CHANNELS_3 | FORMAT_8 | UINT | FLAG_NORMALIZED | LAYOUT_BGR,
	RGBA8					= CHANNELS_4 | FORMAT_8 | UINT | FLAG_NORMALIZED,
	ABGR8					= CHANNELS_4 | FORMAT_8 | UINT | FLAG_NORMALIZED | LAYOUT_ABGR,
	BGRA8					= CHANNELS_4 | FORMAT_8 | UINT | FLAG_NORMALIZED | LAYOUT_BGRA,
	R16						= CHANNELS_1 | FORMAT_16 | UINT | FLAG_NORMALIZED,
	RG16					= CHANNELS_2 | FORMAT_16 | UINT | FLAG_NORMALIZED,
	RGB16					= CHANNELS_3 | FORMAT_16 | UINT | FLAG_NORMALIZED,
	RGBA16					= CHANNELS_4 | FORMAT_16 | UINT | FLAG_NORMALIZED,
	//! normalized unsigned integer formats
	R8UI_NORM				= CHANNELS_1 | FORMAT_8 | UINT | FLAG_NORMALIZED,
	RG8UI_NORM				= CHANNELS_2 | FORMAT_8 | UINT | FLAG_NORMALIZED,
	RGB8UI_NORM				= CHANNELS_3 | FORMAT_8 | UINT | FLAG_NORMALIZED,
	BGR8UI_NORM				= CHANNELS_3 | FORMAT_8 | UINT | FLAG_NORMALIZED | LAYOUT_BGR,
	RGBA8UI_NORM			= CHANNELS_4 | FORMAT_8 | UINT | FLAG_NORMALIZED,
	ABGR8UI_NORM			= CHANNELS_4 | FORMAT_8 | UINT | FLAG_NORMALIZED | LAYOUT_ABGR,
	BGRA8UI_NORM			= CHANNELS_4 | FORMAT_8 | UINT | FLAG_NORMALIZED | LAYOUT_BGRA,
	BGR10UI_NORM			= CHANNELS_3 | FORMAT_10 | UINT | FLAG_NORMALIZED | LAYOUT_BGR,
	BGRA10UI_NORM			= CHANNELS_4 | FORMAT_10 | UINT | FLAG_NORMALIZED | LAYOUT_BGRA,
	A2BGR10UI_NORM			= CHANNELS_4 | FORMAT_10_10_10_ALPHA_2 | UINT | FLAG_NORMALIZED | LAYOUT_ABGR,
	A2RGB10UI_NORM			= CHANNELS_4 | FORMAT_10_10_10_ALPHA_2 | UINT | FLAG_NORMALIZED | LAYOUT_ARGB,
	R16UI_NORM				= CHANNELS_1 | FORMAT_16 | UINT | FLAG_NORMALIZED,
	RG16UI_NORM				= CHANNELS_2 | FORMAT_16 | UINT | FLAG_NORMALIZED,
	RGB16UI_NORM			= CHANNELS_3 | FORMAT_16 | UINT | FLAG_NORMALIZED,
	RGBA16UI_NORM			= CHANNELS_4 | FORMAT_16 | UINT | FLAG_NORMALIZED,
	
	//! normalized integer formats
	R8I_NORM				= CHANNELS_1 | FORMAT_8 | INT | FLAG_NORMALIZED,
	RG8I_NORM				= CHANNELS_2 | FORMAT_8 | INT | FLAG_NORMALIZED,
	RGB8I_NORM				= CHANNELS_3 | FORMAT_8 | INT | FLAG_NORMALIZED,
	BGR8I_NORM				= CHANNELS_3 | FORMAT_8 | INT | FLAG_NORMALIZED | LAYOUT_BGR,
	RGBA8I_NORM				= CHANNELS_4 | FORMAT_8 | INT | FLAG_NORMALIZED,
	ABGR8I_NORM				= CHANNELS_4 | FORMAT_8 | INT | FLAG_NORMALIZED | LAYOUT_ABGR,
	BGRA8I_NORM				= CHANNELS_4 | FORMAT_8 | INT | FLAG_NORMALIZED | LAYOUT_BGRA,
	R16I_NORM				= CHANNELS_1 | FORMAT_16 | INT | FLAG_NORMALIZED,
	RG16I_NORM				= CHANNELS_2 | FORMAT_16 | INT | FLAG_NORMALIZED,
	RGB16I_NORM				= CHANNELS_3 | FORMAT_16 | INT | FLAG_NORMALIZED,
	RGBA16I_NORM			= CHANNELS_4 | FORMAT_16 | INT | FLAG_NORMALIZED,
	
	//! non-normalized formats
	R8UI					= CHANNELS_1 | FORMAT_8 | UINT,
	RG8UI					= CHANNELS_2 | FORMAT_8 | UINT,
	RGB8UI					= CHANNELS_3 | FORMAT_8 | UINT,
	BGR8UI					= CHANNELS_3 | FORMAT_8 | UINT | LAYOUT_BGR,
	RGBA8UI					= CHANNELS_4 | FORMAT_8 | UINT,
	ABGR8UI					= CHANNELS_4 | FORMAT_8 | UINT | LAYOUT_ABGR,
	BGRA8UI					= CHANNELS_4 | FORMAT_8 | UINT | LAYOUT_BGRA,
	R8I						= CHANNELS_1 | FORMAT_8 | INT,
	RG8I					= CHANNELS_2 | FORMAT_8 | INT,
	RGB8I					= CHANNELS_3 | FORMAT_8 | INT,
	BGR8I					= CHANNELS_3 | FORMAT_8 | INT | LAYOUT_BGR,
	RGBA8I					= CHANNELS_4 | FORMAT_8 | INT,
	ABGR8I					= CHANNELS_4 | FORMAT_8 | INT | LAYOUT_ABGR,
	BGRA8I					= CHANNELS_4 | FORMAT_8 | INT | LAYOUT_BGRA,
	A2BGR10UI				= CHANNELS_4 | FORMAT_10_10_10_ALPHA_2 | UINT | LAYOUT_ABGR,
	A2RGB10UI				= CHANNELS_4 | FORMAT_10_10_10_ALPHA_2 | UINT | LAYOUT_ARGB,
	R16UI					= CHANNELS_1 | FORMAT_16 | UINT,
	RG16UI					= CHANNELS_2 | FORMAT_16 | UINT,
	RGB16UI					= CHANNELS_3 | FORMAT_16 | UINT,
	RGBA16UI				= CHANNELS_4 | FORMAT_16 | UINT,
	R16I					= CHANNELS_1 | FORMAT_16 | INT,
	RG16I					= CHANNELS_2 | FORMAT_16 | INT,
	RGB16I					= CHANNELS_3 | FORMAT_16 | INT,
	RGBA16I					= CHANNELS_4 | FORMAT_16 | INT,
	R32UI					= CHANNELS_1 | FORMAT_32 | UINT,
	RG32UI					= CHANNELS_2 | FORMAT_32 | UINT,
	RGB32UI					= CHANNELS_3 | FORMAT_32 | UINT,
	RGBA32UI				= CHANNELS_4 | FORMAT_32 | UINT,
	R32I					= CHANNELS_1 | FORMAT_32 | INT,
	RG32I					= CHANNELS_2 | FORMAT_32 | INT,
	RGB32I					= CHANNELS_3 | FORMAT_32 | INT,
	RGBA32I					= CHANNELS_4 | FORMAT_32 | INT,
	R16F					= CHANNELS_1 | FORMAT_16 | FLOAT,
	RG16F					= CHANNELS_2 | FORMAT_16 | FLOAT,
	RGB16F					= CHANNELS_3 | FORMAT_16 | FLOAT,
	RGBA16F					= CHANNELS_4 | FORMAT_16 | FLOAT,
	R32F					= CHANNELS_1 | FORMAT_32 | FLOAT,
	RG32F					= CHANNELS_2 | FORMAT_32 | FLOAT,
	RGB32F					= CHANNELS_3 | FORMAT_32 | FLOAT,
	RGBA32F					= CHANNELS_4 | FORMAT_32 | FLOAT,
	
	//! depth and depth+stencil formats
	D16						= IMAGE_DEPTH | FORMAT_16 | UINT,
	D24						= IMAGE_DEPTH | FORMAT_24 | UINT,
	D32						= IMAGE_DEPTH | FORMAT_32 | UINT,
	D32F					= IMAGE_DEPTH | FORMAT_32 | FLOAT,
	DS24_8					= IMAGE_DEPTH_STENCIL | FORMAT_24_8 | UINT,
	DS32F_8					= IMAGE_DEPTH_STENCIL | FORMAT_32_8 | FLOAT,
	
	//! compressed formats
	BC1_RGB					= BC1 | CHANNELS_3 | FORMAT_1 | UINT | FLAG_NORMALIZED,
	BC1_RGBA				= BC1 | CHANNELS_4 | FORMAT_1 | UINT | FLAG_NORMALIZED,
	BC2_RGBA				= BC2 | CHANNELS_4 | FORMAT_2 | UINT | FLAG_NORMALIZED,
	BC3_RGBA				= BC3 | CHANNELS_4 | FORMAT_2 | UINT | FLAG_NORMALIZED,
	BC1_RGB_SRGB			= BC1 | CHANNELS_3 | FORMAT_1 | UINT | FLAG_NORMALIZED | FLAG_SRGB,
	BC1_RGBA_SRGB			= BC1 | CHANNELS_4 | FORMAT_1 | UINT | FLAG_NORMALIZED | FLAG_SRGB,
	BC2_RGBA_SRGB			= BC2 | CHANNELS_4 | FORMAT_2 | UINT | FLAG_NORMALIZED | FLAG_SRGB,
	BC3_RGBA_SRGB			= BC3 | CHANNELS_4 | FORMAT_2 | UINT | FLAG_NORMALIZED | FLAG_SRGB,
	RGTC_RUI				= RGTC | CHANNELS_1 | FORMAT_4 | UINT | FLAG_NORMALIZED,
	RGTC_RI					= RGTC | CHANNELS_1 | FORMAT_4 | INT | FLAG_NORMALIZED,
	RGTC_RGUI				= RGTC | CHANNELS_2 | FORMAT_4 | UINT | FLAG_NORMALIZED,
	RGTC_RGI				= RGTC | CHANNELS_2 | FORMAT_4 | INT | FLAG_NORMALIZED,
	BPTC_RGBHF				= BPTC | CHANNELS_3 | FORMAT_3_3_2 | FLOAT,
	BPTC_RGBUHF				= BPTC | CHANNELS_3 | FORMAT_3_3_2 | FLOAT | FLAG_NORMALIZED,
	BPTC_RGBA				= BPTC | CHANNELS_4 | FORMAT_2 | UINT | FLAG_NORMALIZED,
	BPTC_RGBA_SRGB			= BPTC | CHANNELS_4 | FORMAT_2 | UINT | FLAG_NORMALIZED | FLAG_SRGB,
	PVRTC_RGB2				= PVRTC | CHANNELS_3 | FORMAT_2 | UINT | FLAG_NORMALIZED,
	PVRTC_RGB4				= PVRTC | CHANNELS_3 | FORMAT_4 | UINT | FLAG_NORMALIZED,
	PVRTC_RGBA2				= PVRTC | CHANNELS_4 | FORMAT_2 | UINT | FLAG_NORMALIZED,
	PVRTC_RGBA4				= PVRTC | CHANNELS_4 | FORMAT_4 | UINT | FLAG_NORMALIZED,
	PVRTC_RGB2_SRGB			= PVRTC | CHANNELS_3 | FORMAT_2 | UINT | FLAG_NORMALIZED | FLAG_SRGB,
	PVRTC_RGB4_SRGB			= PVRTC | CHANNELS_3 | FORMAT_4 | UINT | FLAG_NORMALIZED | FLAG_SRGB,
	PVRTC_RGBA2_SRGB		= PVRTC | CHANNELS_4 | FORMAT_2 | UINT | FLAG_NORMALIZED | FLAG_SRGB,
	PVRTC_RGBA4_SRGB		= PVRTC | CHANNELS_4 | FORMAT_4 | UINT | FLAG_NORMALIZED | FLAG_SRGB,
	
};
floor_global_enum_ext(COMPUTE_IMAGE_TYPE)

//! returns true if the image layout is R, RG, RGB or RGBA
floor_inline_always static constexpr bool image_layout_rgba(const COMPUTE_IMAGE_TYPE& image_type) {
	return ((image_type & COMPUTE_IMAGE_TYPE::__LAYOUT_MASK) == COMPUTE_IMAGE_TYPE::LAYOUT_RGBA);
}

//! returns true if the image layout is ABGR or BGR
floor_inline_always static constexpr bool image_layout_abgr(const COMPUTE_IMAGE_TYPE& image_type) {
	return ((image_type & COMPUTE_IMAGE_TYPE::__LAYOUT_MASK) == COMPUTE_IMAGE_TYPE::LAYOUT_ABGR);
}

//! returns true if the image layout is BGRA
floor_inline_always static constexpr bool image_layout_bgra(const COMPUTE_IMAGE_TYPE& image_type) {
	return ((image_type & COMPUTE_IMAGE_TYPE::__LAYOUT_MASK) == COMPUTE_IMAGE_TYPE::LAYOUT_BGRA);
}

//! returns true if the image layout is ARGB
floor_inline_always static constexpr bool image_layout_argb(const COMPUTE_IMAGE_TYPE& image_type) {
	return ((image_type & COMPUTE_IMAGE_TYPE::__LAYOUT_MASK) == COMPUTE_IMAGE_TYPE::LAYOUT_ARGB);
}

//! returns the dimensionality of the specified image type
floor_inline_always static constexpr uint32_t image_dim_count(const COMPUTE_IMAGE_TYPE& image_type) {
	return uint32_t(image_type & COMPUTE_IMAGE_TYPE::__DIM_MASK) >> uint32_t(COMPUTE_IMAGE_TYPE::__DIM_SHIFT);
}

//! returns the storage dimensionality of the specified image type
floor_inline_always static constexpr uint32_t image_storage_dim_count(const COMPUTE_IMAGE_TYPE& image_type) {
	return image_dim_count(image_type) + (has_flag<COMPUTE_IMAGE_TYPE::FLAG_CUBE>(image_type) ||
										  has_flag<COMPUTE_IMAGE_TYPE::FLAG_ARRAY>(image_type) ? 1 : 0);
}

//! returns the channel count of the specified image type
floor_inline_always static constexpr uint32_t image_channel_count(const COMPUTE_IMAGE_TYPE& image_type) {
	return (uint32_t(image_type & COMPUTE_IMAGE_TYPE::__CHANNELS_MASK) >> uint32_t(COMPUTE_IMAGE_TYPE::__CHANNELS_SHIFT)) + 1u;
}

//! return the format of the specified image type
floor_inline_always static constexpr uint32_t image_format(const COMPUTE_IMAGE_TYPE& image_type) {
	return uint32_t(image_type & COMPUTE_IMAGE_TYPE::__FORMAT_MASK);
}

//! return the anisotropy of the specified image type
floor_inline_always static constexpr uint32_t image_anisotropy(const COMPUTE_IMAGE_TYPE& image_type) {
	return uint32_t(1ull << (uint64_t(image_type & COMPUTE_IMAGE_TYPE::__ANISOTROPY_MASK) >>
							 uint64_t(COMPUTE_IMAGE_TYPE::__ANISOTROPY_SHIFT)));
}

//! return the sample count of the specified image type
floor_inline_always static constexpr uint32_t image_sample_count(const COMPUTE_IMAGE_TYPE& image_type) {
	if (!has_flag<COMPUTE_IMAGE_TYPE::FLAG_MSAA>(image_type)) return 1u;
	return uint32_t(1ull << (uint64_t(image_type & COMPUTE_IMAGE_TYPE::__SAMPLE_COUNT_MASK) >>
							 uint64_t(COMPUTE_IMAGE_TYPE::__SAMPLE_COUNT_SHIFT)));
}

//! return the COMPUTE_IMAGE_TYPE::SAMPLE_COUNT_* type matching the specified sample count
//! NOTE: "sample_count" must be in [0, 64]
floor_inline_always static constexpr COMPUTE_IMAGE_TYPE image_sample_type_from_count(const uint32_t& sample_count) {
	static_assert(uint32_t(COMPUTE_IMAGE_TYPE::SAMPLE_COUNT_1) == uint32_t(COMPUTE_IMAGE_TYPE::NONE), "sample count definition changed");
	if (sample_count <= 1 || sample_count > 64u) {
		return COMPUTE_IMAGE_TYPE::NONE;
	}
	if (sample_count <= 2u) {
		return COMPUTE_IMAGE_TYPE::SAMPLE_COUNT_2;
	} else if (sample_count <= 4u) {
		return COMPUTE_IMAGE_TYPE::SAMPLE_COUNT_4;
	} else if (sample_count <= 8u) {
		return COMPUTE_IMAGE_TYPE::SAMPLE_COUNT_8;
	} else if (sample_count <= 16u) {
		return COMPUTE_IMAGE_TYPE::SAMPLE_COUNT_16;
	} else if (sample_count <= 32u) {
		return COMPUTE_IMAGE_TYPE::SAMPLE_COUNT_32;
	}
	return COMPUTE_IMAGE_TYPE::SAMPLE_COUNT_64;
}

//! returns the coordinate width required to address a single texel in the image
//! NOTE: this is usually identical to "image_storage_dim_count", but needs to be increased by 1 for cube array formats
floor_inline_always static constexpr uint32_t image_coordinate_width(const COMPUTE_IMAGE_TYPE& image_type) {
	uint32_t ret = image_storage_dim_count(image_type);
	if(has_flag<COMPUTE_IMAGE_TYPE::FLAG_ARRAY>(image_type) &&
	   has_flag<COMPUTE_IMAGE_TYPE::FLAG_CUBE>(image_type)) {
		++ret;
	}
	return ret;
}

//! returns true if the image type is using a compressed image format
floor_inline_always static constexpr bool image_compressed(const COMPUTE_IMAGE_TYPE& image_type) {
	return ((image_type & COMPUTE_IMAGE_TYPE::__COMPRESSION_MASK) != COMPUTE_IMAGE_TYPE::UNCOMPRESSED);
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
		case COMPUTE_IMAGE_TYPE::FORMAT_5_5_5_ALPHA_1: return (channel_count == 4);
		case COMPUTE_IMAGE_TYPE::FORMAT_5_6_5: return (channel_count == 3);
		case COMPUTE_IMAGE_TYPE::FORMAT_9_9_9_EXP_5: return (channel_count == 3);
		case COMPUTE_IMAGE_TYPE::FORMAT_10: return (channel_count == 3 || channel_count == 4);
		case COMPUTE_IMAGE_TYPE::FORMAT_10_10_10_ALPHA_2: return (channel_count == 4);
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
	const auto format = image_type & COMPUTE_IMAGE_TYPE::__FORMAT_MASK;
	if (!image_compressed(image_type)) {
		const auto channel_count = image_channel_count(image_type);
		const auto sample_count = image_sample_count(image_type);
		switch (format) {
			// arbitrary channel formats
			case COMPUTE_IMAGE_TYPE::FORMAT_2: return 2 * channel_count * sample_count;
			case COMPUTE_IMAGE_TYPE::FORMAT_4: return 4 * channel_count * sample_count;
			case COMPUTE_IMAGE_TYPE::FORMAT_8: return 8 * channel_count * sample_count;
			case COMPUTE_IMAGE_TYPE::FORMAT_10: return 10 * channel_count * sample_count;
			case COMPUTE_IMAGE_TYPE::FORMAT_16: return 16 * channel_count * sample_count;
			case COMPUTE_IMAGE_TYPE::FORMAT_32: return 32 * channel_count * sample_count;
			case COMPUTE_IMAGE_TYPE::FORMAT_64: return 64 * channel_count * sample_count;
				
			// special channel specific formats
			case COMPUTE_IMAGE_TYPE::FORMAT_3_3_2: return 8 * sample_count;
			case COMPUTE_IMAGE_TYPE::FORMAT_5_5_5: return 15 * sample_count;
			case COMPUTE_IMAGE_TYPE::FORMAT_5_5_5_ALPHA_1: return 16 * sample_count;
			case COMPUTE_IMAGE_TYPE::FORMAT_5_6_5: return 16 * sample_count;
			case COMPUTE_IMAGE_TYPE::FORMAT_9_9_9_EXP_5: return 32 * sample_count;
			case COMPUTE_IMAGE_TYPE::FORMAT_10_10_10_ALPHA_2: return 32 * sample_count;
			case COMPUTE_IMAGE_TYPE::FORMAT_11_11_10: return 32 * sample_count;
			case COMPUTE_IMAGE_TYPE::FORMAT_12_12_12: return 36 * sample_count;
			case COMPUTE_IMAGE_TYPE::FORMAT_12_12_12_12: return 48 * sample_count;
			case COMPUTE_IMAGE_TYPE::FORMAT_24: return 24 * sample_count;
			case COMPUTE_IMAGE_TYPE::FORMAT_24_8: return 32 * sample_count;
			case COMPUTE_IMAGE_TYPE::FORMAT_32_8: return 40 * sample_count;
			default: return 1 * sample_count;
		}
	} else {
		switch (image_type & COMPUTE_IMAGE_TYPE::__COMPRESSION_MASK) {
			case COMPUTE_IMAGE_TYPE::PVRTC: return (format == COMPUTE_IMAGE_TYPE::FORMAT_2 ? 2 : 4);
			// TODO: other compressed formats
			default: return 1;
		}
	}
}

//! returns the amount of bits needed to store the specified channel
//! NOTE: not viable for compressed image formats
static constexpr uint32_t image_bits_of_channel(const COMPUTE_IMAGE_TYPE& image_type, const uint32_t& channel) {
	if (channel >= image_channel_count(image_type)) return 0;
	if (image_compressed(image_type)) return 0;
	
	const auto sample_count = image_sample_count(image_type);
	switch (image_type & COMPUTE_IMAGE_TYPE::__FORMAT_MASK) {
		// arbitrary channel formats
		case COMPUTE_IMAGE_TYPE::FORMAT_2: return 2 * sample_count;
		case COMPUTE_IMAGE_TYPE::FORMAT_4: return 4 * sample_count;
		case COMPUTE_IMAGE_TYPE::FORMAT_8: return 8 * sample_count;
		case COMPUTE_IMAGE_TYPE::FORMAT_16: return 16 * sample_count;
		case COMPUTE_IMAGE_TYPE::FORMAT_32: return 32 * sample_count;
		case COMPUTE_IMAGE_TYPE::FORMAT_64: return 64 * sample_count;
			
		// special channel specific formats
		case COMPUTE_IMAGE_TYPE::FORMAT_3_3_2: return (channel <= 1 ? 3 : 2) * sample_count;
		case COMPUTE_IMAGE_TYPE::FORMAT_5_5_5: return 5 * sample_count;
		case COMPUTE_IMAGE_TYPE::FORMAT_5_5_5_ALPHA_1: return (channel <= 2 ? 5 : 1) * sample_count;
		case COMPUTE_IMAGE_TYPE::FORMAT_5_6_5: return (channel == 1 ? 6 : 5) * sample_count;
		case COMPUTE_IMAGE_TYPE::FORMAT_9_9_9_EXP_5: return (channel <= 2 ? 14 : 0) * sample_count; // tricky
		case COMPUTE_IMAGE_TYPE::FORMAT_10: return 10 * sample_count;
		case COMPUTE_IMAGE_TYPE::FORMAT_10_10_10_ALPHA_2: return (channel <= 2 ? 10 : 2) * sample_count;
		case COMPUTE_IMAGE_TYPE::FORMAT_11_11_10: return (channel <= 1 ? 11 : 10) * sample_count;
		case COMPUTE_IMAGE_TYPE::FORMAT_12_12_12: return 12 * sample_count;
		case COMPUTE_IMAGE_TYPE::FORMAT_12_12_12_12: return 12 * sample_count;
		case COMPUTE_IMAGE_TYPE::FORMAT_24: return 24 * sample_count;
		case COMPUTE_IMAGE_TYPE::FORMAT_24_8: return (channel == 0 ? 24 : 8) * sample_count;
		case COMPUTE_IMAGE_TYPE::FORMAT_32_8: return (channel == 0 ? 32 : 8) * sample_count;
		default: return 0;
	}
}

//! returns the amount of bytes needed to store one pixel
//! NOTE: rounded up if "bits per pixel" is not divisible by 8
static constexpr uint32_t image_bytes_per_pixel(const COMPUTE_IMAGE_TYPE& image_type) {
	const auto bpp = image_bits_per_pixel(image_type);
	return ((bpp + 7u) / 8u);
}

//! returns the total amount of bytes needed to store a slice of an image of the specified dimensions and types
//! (or of the complete image w/o mip levels if it isn't an array or cube image)
static constexpr size_t image_slice_data_size_from_types(const uint4& image_dim,
														 const COMPUTE_IMAGE_TYPE& image_type) {
	const auto dim_count = image_dim_count(image_type);
	size_t size = size_t(image_dim.x);
	if (dim_count >= 2) size *= size_t(image_dim.y);
	if (dim_count == 3) size *= size_t(image_dim.z);
	
	if (has_flag<COMPUTE_IMAGE_TYPE::FLAG_MSAA>(image_type)) {
		size *= image_sample_count(image_type);
	}
	
	// TODO: make sure special formats correspond to channel count
	size = (size * image_bits_per_pixel(image_type)) / 8u;
	
	return size;
}

//! returns the amount of mip-map levels required by the specified max image dimension (no flag checking)
static constexpr uint32_t image_mip_level_count_from_max_dim(const uint32_t& max_dim) {
	// each mip level is half the size of its upper/parent level, until dim == 1
	// -> get the closest power-of-two, then "log2(2^N) + 1"
	// this can be done the fastest by counting the leading zeros in the 32-bit value
	return uint32_t(32 - __builtin_clz((uint32_t)const_math::next_pot(max_dim)));
}

//! returns the amount of mip-map levels required by the specified image dim and type
//! NOTE: #mip-levels from image dim to 1px if uncompressed, or 8px if compressed
static constexpr uint32_t image_mip_level_count(const uint4& image_dim, const COMPUTE_IMAGE_TYPE image_type) {
	if(!has_flag<COMPUTE_IMAGE_TYPE::FLAG_MIPMAPPED>(image_type)) {
		return 1;
	}
	
	const auto dim_count = image_dim_count(image_type);
	const auto max_dim = std::max(std::max(image_dim.x, dim_count >= 2 ? image_dim.y : 1), dim_count >= 3 ? image_dim.z : 1);
	if(max_dim == 1) return 1;
	
	auto levels = image_mip_level_count_from_max_dim(max_dim);
	
	// for compressed images, 8x8 is the minimum image and mip-map size -> substract 3 levels (1x1, 2x2 and 4x4)
	if(image_compressed(image_type)) {
		levels = (std::max(levels, 4u) - 3u);
	}
	
	return levels;
}

//! returns the amount of image layers specified by the image dim and type
//! NOTE: this count includes cube map sides (layers)
static constexpr uint32_t image_layer_count(const uint4& image_dim, const COMPUTE_IMAGE_TYPE image_type) {
	const auto dim_count = image_dim_count(image_type);
	const bool is_array = has_flag<COMPUTE_IMAGE_TYPE::FLAG_ARRAY>(image_type);
	const bool is_cube = has_flag<COMPUTE_IMAGE_TYPE::FLAG_CUBE>(image_type);
	uint32_t layer_count = (!is_array ? 1 : (dim_count == 1 ? image_dim.y :
											 (dim_count == 2 ? image_dim.z : image_dim.w)));
	if(is_cube) {
		layer_count *= 6;
	}
	return layer_count;
}

//! returns the total amount of bytes needed to store the image of the specified dimensions, types and mip-levels
//! NOTE: each subsequent mip-level dim is computed as >>= 1, stopping at 1px for uncompressed images, or 8px for uncompressed ones
static constexpr size_t image_data_size_from_types(const uint4& image_dim,
												   const COMPUTE_IMAGE_TYPE& image_type,
												   const bool ignore_mip_levels = false) {
	const auto dim_count = image_dim_count(image_type);
	const auto mip_levels = (ignore_mip_levels ? 1 : image_mip_level_count(image_dim, image_type));
	
	// array count after: width (, height (, depth))
	const size_t array_dim = (dim_count == 3 ? image_dim.w : (dim_count == 2 ? image_dim.z : image_dim.y));
	
	size_t size = 0;
	uint4 mip_image_dim {
		image_dim.x,
		dim_count >= 2 ? image_dim.y : 0u,
		dim_count >= 3 ? image_dim.z : 0u,
		0u
	};
	for(size_t level = 0; level < mip_levels; ++level) {
		size_t slice_size = image_slice_data_size_from_types(mip_image_dim, image_type);
		
		if(has_flag<COMPUTE_IMAGE_TYPE::FLAG_ARRAY>(image_type)) {
			slice_size *= array_dim;
		}
		
		if(has_flag<COMPUTE_IMAGE_TYPE::FLAG_CUBE>(image_type)) {
			// 6 cube sides
			slice_size *= 6u;
		}
		
		size += slice_size;
		mip_image_dim >>= 1;
	}
	
	return size;
}

//! image data size -> data type mapping
template <COMPUTE_IMAGE_TYPE image_type, size_t size, typename = void> struct image_sized_data_type {};
template <COMPUTE_IMAGE_TYPE image_type, size_t size>
struct image_sized_data_type<image_type, size,
							 enable_if_t<((image_type & COMPUTE_IMAGE_TYPE::__DATA_TYPE_MASK) == COMPUTE_IMAGE_TYPE::UINT &&
										  size > 0u && size <= 64u)>> {
	typedef conditional_t<(size > 0u && size <= 8u), uint8_t, conditional_t<
						  (size > 8u && size <= 16u), uint16_t, conditional_t<
						  (size > 16u && size <= 32u), uint32_t, conditional_t<
						  (size > 32u && size <= 64u), uint64_t, void>>>> type;
};
template <COMPUTE_IMAGE_TYPE image_type, size_t size>
struct image_sized_data_type<image_type, size,
							 enable_if_t<((image_type & COMPUTE_IMAGE_TYPE::__DATA_TYPE_MASK) == COMPUTE_IMAGE_TYPE::INT &&
										  size > 0u && size <= 64u)>> {
	typedef conditional_t<(size > 0u && size <= 8u), int8_t, conditional_t<
						  (size > 8u && size <= 16u), int16_t, conditional_t<
						  (size > 16u && size <= 32u), int32_t, conditional_t<
						  (size > 32u && size <= 64u), int64_t, void>>>> type;
};
template <COMPUTE_IMAGE_TYPE image_type, size_t size>
struct image_sized_data_type<image_type, size,
							 enable_if_t<((image_type & COMPUTE_IMAGE_TYPE::__DATA_TYPE_MASK) == COMPUTE_IMAGE_TYPE::FLOAT &&
										  size > 0u && size <= 64u)>> {
	typedef conditional_t<(size > 0u && size <= 16u), float /* no half type, load/stores via float */, conditional_t<
						  (size > 16u && size <= 32u), float, conditional_t<
						  (size > 32u && size <= 64u), double, void>>> type;
};

//! data type of a single image channel (always 32-bit), used for image reads and writes
template <COMPUTE_IMAGE_TYPE image_type,
		  enable_if_t<(image_type & COMPUTE_IMAGE_TYPE::__DATA_TYPE_MASK) != COMPUTE_IMAGE_TYPE::NONE>* = nullptr>
struct image_tex_channel_data_type {
	typedef typename image_sized_data_type<image_type, 32u>::type type;
};

//! image texel data type used for image reads and writes: vector { 1, 2, 3, 4 } < { float, uint32_t, int32_t } >
template <COMPUTE_IMAGE_TYPE image_type>
struct image_texel_data_type {
	typedef vector_n<typename image_tex_channel_data_type<image_type>::type, image_channel_count(image_type)> type;
};

//! fits a 4-component vector to the corresponding image data vector type, or passthrough for scalar values
template <COMPUTE_IMAGE_TYPE image_type, typename data_type, typename = void> struct image_vec_ret_type {};
template <COMPUTE_IMAGE_TYPE image_type, typename data_type>
struct image_vec_ret_type<image_type, data_type, enable_if_t<image_channel_count(image_type) == 1>> {
	//! scalar passthrough
	static constexpr floor_inline_always data_type fit(const data_type& color) { return color; }
	//! 4-component -> scalar
	static constexpr floor_inline_always data_type fit(const vector_n<data_type, 4>& color) { return color.x; }
};
template <COMPUTE_IMAGE_TYPE image_type, typename data_type>
struct image_vec_ret_type<image_type, data_type, enable_if_t<image_channel_count(image_type) == 2>> {
	static constexpr floor_inline_always vector_n<data_type, 2> fit(const vector_n<data_type, 4>& color) { return color.xy; }
};
template <COMPUTE_IMAGE_TYPE image_type, typename data_type>
struct image_vec_ret_type<image_type, data_type, enable_if_t<image_channel_count(image_type) == 3>> {
	static constexpr floor_inline_always vector_n<data_type, 3> fit(const vector_n<data_type, 4>& color) { return color.xyz; }
};
template <COMPUTE_IMAGE_TYPE image_type, typename data_type>
struct image_vec_ret_type<image_type, data_type, enable_if_t<image_channel_count(image_type) == 4>> {
	static constexpr floor_inline_always vector_n<data_type, 4> fit(const vector_n<data_type, 4>& color) { return color; }
};

#endif
