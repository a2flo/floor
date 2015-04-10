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

#ifndef __FLOOR_COMPUTE_IMAGE_HPP__
#define __FLOOR_COMPUTE_IMAGE_HPP__

#include <floor/compute/compute_memory.hpp>

//! image type
enum class COMPUTE_IMAGE_TYPE : uint32_t {
	//! invalid/uninitialized
	NONE					= (0u),
	
	//////////////////////////////////////////
	// -> image flags and types
	//! upper 16-bit: type flags (currently used: 10)
	__FLAG_MASK				= (0xFFFF0000u),
	__FLAG_SHIFT			= (16u),
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
	//! optional type: image uses mip-mapping, i.e. has multiple LODs
	FLAG_MIPMAPPED			= (1u << (__FLAG_SHIFT + 6u)),
	//! optional type: image uses anisotropic filtering
	FLAG_ANISOTROPIC		= (1u << (__FLAG_SHIFT + 7u)),
	//! optional type: image doesn't need a sampler (i.e. only point/nearest/pixel sampled)
	//! NOTE: on some platforms this might provide better performance and/or less overhead
	FLAG_NO_SAMPLER			= (1u << (__FLAG_SHIFT + 8u)),
	//! optional type: image uses gather sampling (aka tld4/fetch4)
	FLAG_GATHER				= (1u << (__FLAG_SHIFT + 9u)),
	
	//! bits 13-15: dimensionality
	__DIM_MASK				= (0x0000E000u),
	__DIM_SHIFT				= (13u),
	DIM_1D					= (1u << __DIM_SHIFT),
	DIM_2D					= (2u << __DIM_SHIFT),
	DIM_3D					= (3u << __DIM_SHIFT),
	DIM_4D					= (4u << __DIM_SHIFT),
	
	//! bits 10-12: actual storage dimensionality (for arrays/layers: dim + 1, else: == dim)
	__DIM_STORAGE_MASK		= (0x00001C00u),
	__DIM_STORAGE_SHIFT		= (10u),
	DIM_STORAGE_1D			= (1u << __DIM_STORAGE_SHIFT),
	DIM_STORAGE_2D			= (2u << __DIM_STORAGE_SHIFT),
	DIM_STORAGE_3D			= (3u << __DIM_STORAGE_SHIFT),
	DIM_STORAGE_4D			= (4u << __DIM_STORAGE_SHIFT),
	
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
	IMAGE_DEPTH				= DIM_2D | DIM_STORAGE_2D | FLAG_DEPTH,
	//! combined 2D depth + stencil image
	IMAGE_DEPTH_STENCIL		= DIM_2D | DIM_STORAGE_2D | FLAG_DEPTH | FLAG_STENCIL,
	//! array of 2D depth images
	IMAGE_DEPTH_ARRAY		= DIM_2D | DIM_STORAGE_3D | FLAG_DEPTH | FLAG_ARRAY,
	//! depth cube map image
	IMAGE_DEPTH_CUBE		= DIM_2D | DIM_STORAGE_3D | FLAG_DEPTH | FLAG_CUBE,
	//! multi-sampled 2D depth image
	IMAGE_DEPTH_MSAA		= DIM_2D | DIM_STORAGE_2D | FLAG_DEPTH | FLAG_MSAA,
	//! array of multi-sampled 2D depth images
	IMAGE_DEPTH_MSAA_ARRAY	= DIM_2D | DIM_STORAGE_3D | FLAG_DEPTH | FLAG_MSAA | FLAG_ARRAY,
	
};
floor_global_enum_ext(COMPUTE_IMAGE_TYPE)

//! image storage type (per channel, except for crooked special cases)
enum class COMPUTE_IMAGE_STORAGE_TYPE : uint32_t {
	//! invalid/uninitialized
	NONE					= (0u),
	
	//! signed 5-bit (char)
	INT_5					= (1u),
	//! signed 5-bit/6-bit/5-bit (char)
	INT_565					= (2u),
	//! signed 8-bit (char)
	INT_8					= (3u),
	//! signed 10-bit (short)
	INT_10					= (4u),
	//! signed 16-bit (short)
	INT_16					= (5u),
	//! signed 24-bit (int)
	INT_24					= (6u),
	//! signed 32-bit (int)
	INT_32					= (7u),
	//! signed 64-bit (long / long long)
	INT_64					= (8u),
	
	//! unsigned 5-bit (uchar)
	UINT_5					= (9u),
	//! unsigned 5-bit/6-bi/5-bit (uchar)
	UINT_565				= (10u),
	//! unsigned 8-bit (uchar)
	UINT_8					= (11u),
	//! unsigned 10-bit (ushort)
	UINT_10					= (12u),
	//! unsigned 16-bit (ushort)
	UINT_16					= (13u),
	//! unsigned 24-bit (uint)
	UINT_24					= (14u),
	//! unsigned 32-bit (uint)
	UINT_32					= (15u),
	//! unsigned 64-bit (ulong / unsigned long long)
	UINT_64					= (16u),
	
	//! 16-bit half precision fp (half)
	FLOAT_16				= (17u),
	//! 32-bit single precision fp (float)
	FLOAT_32				= (18u),
	//! 64-bit double precision fp (double)
	FLOAT_64				= (19u),
	
	//! normalized signed 8-bit (char)
	NORM_INT_8				= (20u),
	//! normalized signed 16-bit (short)
	NORM_INT_16				= (21u),
	//! normalized signed 24-bit (int)
	NORM_INT_24				= (22u),
	//! normalized signed 32-bit (int)
	NORM_INT_32				= (23u),
	
	//! normalized unsigned 8-bit (uchar)
	NORM_UINT_8				= (24u),
	//! normalized unsigned 16-bit (short)
	NORM_UINT_16			= (25u),
	//! normalized unsigned 24-bit (uint)
	NORM_UINT_24			= (26u),
	//! normalized unsigned 32-bit (uint)
	NORM_UINT_32			= (27u),
	
	//! for internal use
	__COMPUTE_IMAGE_STORAGE_TYPE_MAX
	
};

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wweak-vtables"
#endif

class compute_image : public compute_memory {
public:
	//! TODO: create image descriptor for advanced use?
	//! TODO: ...
	compute_image(const void* device,
				  const uint4 image_dim,
				  const COMPUTE_IMAGE_TYPE image_type,
				  const COMPUTE_IMAGE_STORAGE_TYPE storage_type,
				  const uint32_t channel_count,
				  void* host_ptr = nullptr,
				  const COMPUTE_MEMORY_FLAG flags_ = (COMPUTE_MEMORY_FLAG::READ_WRITE |
													  COMPUTE_MEMORY_FLAG::HOST_READ_WRITE),
				  const uint32_t opengl_type = 0);
	
	virtual ~compute_image() = 0;
	
	// TODO: read, write, copy, fill, zero/clear, resize?, map/unmap, opengl interop
	
	//! returns the dimensionality of the specified image type
	static constexpr uint32_t dim_count(const COMPUTE_IMAGE_TYPE& image_type) {
		return uint32_t(image_type & COMPUTE_IMAGE_TYPE::__DIM_MASK) >> uint32_t(COMPUTE_IMAGE_TYPE::__DIM_SHIFT);
	}
	
	//! returns the storage dimensionality of the specified image type
	static constexpr uint32_t storage_dim_count(const COMPUTE_IMAGE_TYPE& image_type) {
		return uint32_t(image_type & COMPUTE_IMAGE_TYPE::__DIM_STORAGE_MASK) >> uint32_t(COMPUTE_IMAGE_TYPE::__DIM_STORAGE_SHIFT);
	}
	
protected:
	const uint4 image_dim;
	const COMPUTE_IMAGE_TYPE image_type;
	const COMPUTE_IMAGE_STORAGE_TYPE storage_type;
	const uint32_t channel_count;
	const size_t image_data_size;

	// internal function to create/delete an opengl image if compute/opengl sharing is used
	bool create_gl_image(const bool copy_host_data);
	void delete_gl_image();
	
};

#if defined(__clang__)
#pragma clang diagnostic pop
#endif

#endif
