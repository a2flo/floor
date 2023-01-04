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

#ifndef __FLOOR_COMPUTE_DEVICE_OPAQUE_IMAGE_MAP_HPP__
#define __FLOOR_COMPUTE_DEVICE_OPAQUE_IMAGE_MAP_HPP__

// COMPUTE_IMAGE_TYPE -> opencl/metal image*d_*t type mapping
#define OPAQUE_IMAGE_MASK ( \
COMPUTE_IMAGE_TYPE::__DIM_MASK | \
COMPUTE_IMAGE_TYPE::FLAG_DEPTH | \
COMPUTE_IMAGE_TYPE::FLAG_ARRAY | \
COMPUTE_IMAGE_TYPE::FLAG_BUFFER | \
COMPUTE_IMAGE_TYPE::FLAG_CUBE | \
COMPUTE_IMAGE_TYPE::FLAG_MSAA)

// nicer error message than "incomplete type" or "type does not exist"
#if defined(FLOOR_COMPUTE_OPENCL) || defined(FLOOR_COMPUTE_VULKAN)
struct unavailable_opencl_image_type;
#endif
#if defined(FLOOR_COMPUTE_METAL)
struct unavailable_metal_image_type;
#endif

template <COMPUTE_IMAGE_TYPE image_type> struct opaque_image_type {};

template <COMPUTE_IMAGE_TYPE image_type>
requires((image_type & OPAQUE_IMAGE_MASK) == COMPUTE_IMAGE_TYPE::IMAGE_1D)
struct opaque_image_type<image_type> {
	typedef image1d_t type;
};

template <COMPUTE_IMAGE_TYPE image_type>
requires((image_type & OPAQUE_IMAGE_MASK) == COMPUTE_IMAGE_TYPE::IMAGE_1D_ARRAY)
struct opaque_image_type<image_type> {
	typedef image1d_array_t type;
};

template <COMPUTE_IMAGE_TYPE image_type>
requires((image_type & OPAQUE_IMAGE_MASK) == COMPUTE_IMAGE_TYPE::IMAGE_1D_BUFFER)
struct opaque_image_type<image_type> {
#if !defined(FLOOR_COMPUTE_METAL)
	typedef image1d_buffer_t type;
#else
	typedef unavailable_metal_image_type type;
#endif
};

template <COMPUTE_IMAGE_TYPE image_type>
requires((image_type & OPAQUE_IMAGE_MASK) == COMPUTE_IMAGE_TYPE::IMAGE_2D)
struct opaque_image_type<image_type> {
	typedef image2d_t type;
};

template <COMPUTE_IMAGE_TYPE image_type>
requires((image_type & OPAQUE_IMAGE_MASK) == COMPUTE_IMAGE_TYPE::IMAGE_2D_ARRAY)
struct opaque_image_type<image_type> {
	typedef image2d_array_t type;
};

template <COMPUTE_IMAGE_TYPE image_type>
requires((image_type & OPAQUE_IMAGE_MASK) == COMPUTE_IMAGE_TYPE::IMAGE_2D_MSAA)
struct opaque_image_type<image_type> {
	typedef image2d_msaa_t type;
};

template <COMPUTE_IMAGE_TYPE image_type>
requires((image_type & OPAQUE_IMAGE_MASK) == COMPUTE_IMAGE_TYPE::IMAGE_2D_MSAA_ARRAY)
struct opaque_image_type<image_type> {
#if !defined(FLOOR_COMPUTE_METAL)
	typedef image2d_array_msaa_t type;
#else
#if FLOOR_COMPUTE_METAL_MAJOR > 2 || (FLOOR_COMPUTE_METAL_MAJOR == 2 && FLOOR_COMPUTE_METAL_MINOR >= 1)
	// supported since Metal 2.1
	typedef image2d_array_msaa_t type;
#else
	typedef unavailable_metal_image_type type;
#endif
#endif
};

// NOTE: also applies to combined stencil format
template <COMPUTE_IMAGE_TYPE image_type>
requires((image_type & OPAQUE_IMAGE_MASK) == (COMPUTE_IMAGE_TYPE::IMAGE_2D | COMPUTE_IMAGE_TYPE::FLAG_DEPTH))
struct opaque_image_type<image_type> {
	typedef image2d_depth_t type;
};

template <COMPUTE_IMAGE_TYPE image_type>
requires((image_type & OPAQUE_IMAGE_MASK) == (COMPUTE_IMAGE_TYPE::IMAGE_2D_ARRAY | COMPUTE_IMAGE_TYPE::FLAG_DEPTH))
struct opaque_image_type<image_type> {
	typedef image2d_array_depth_t type;
};

template <COMPUTE_IMAGE_TYPE image_type>
requires((image_type & OPAQUE_IMAGE_MASK) == (COMPUTE_IMAGE_TYPE::IMAGE_2D_MSAA | COMPUTE_IMAGE_TYPE::FLAG_DEPTH))
struct opaque_image_type<image_type> {
	typedef image2d_msaa_depth_t type;
};

template <COMPUTE_IMAGE_TYPE image_type>
requires((image_type & OPAQUE_IMAGE_MASK) == (COMPUTE_IMAGE_TYPE::IMAGE_2D_MSAA_ARRAY | COMPUTE_IMAGE_TYPE::FLAG_DEPTH))
struct opaque_image_type<image_type> {
#if !defined(FLOOR_COMPUTE_METAL)
	typedef image2d_array_msaa_depth_t type;
#else
#if FLOOR_COMPUTE_METAL_MAJOR > 2 || (FLOOR_COMPUTE_METAL_MAJOR == 2 && FLOOR_COMPUTE_METAL_MINOR >= 1)
	// supported since Metal 2.1
	typedef image2d_array_msaa_depth_t type;
#else
	typedef unavailable_metal_image_type type;
#endif
#endif
};

template <COMPUTE_IMAGE_TYPE image_type>
requires((image_type & OPAQUE_IMAGE_MASK) == COMPUTE_IMAGE_TYPE::IMAGE_3D)
struct opaque_image_type<image_type> {
	typedef image3d_t type;
};

template <COMPUTE_IMAGE_TYPE image_type>
requires((image_type & OPAQUE_IMAGE_MASK) == COMPUTE_IMAGE_TYPE::IMAGE_CUBE)
struct opaque_image_type<image_type> {
#if !defined(FLOOR_COMPUTE_OPENCL) || defined(FLOOR_COMPUTE_VULKAN)
	typedef imagecube_t type;
#else
	typedef unavailable_opencl_image_type type;
#endif
};

template <COMPUTE_IMAGE_TYPE image_type>
requires((image_type & OPAQUE_IMAGE_MASK) == COMPUTE_IMAGE_TYPE::IMAGE_CUBE_ARRAY)
struct opaque_image_type<image_type> {
#if !defined(FLOOR_COMPUTE_OPENCL) || defined(FLOOR_COMPUTE_VULKAN)
	typedef imagecube_array_t type;
#else
	typedef unavailable_opencl_image_type type;
#endif
};

template <COMPUTE_IMAGE_TYPE image_type>
requires((image_type & OPAQUE_IMAGE_MASK) == (COMPUTE_IMAGE_TYPE::IMAGE_CUBE | COMPUTE_IMAGE_TYPE::FLAG_DEPTH))
struct opaque_image_type<image_type> {
#if !defined(FLOOR_COMPUTE_OPENCL) || defined(FLOOR_COMPUTE_VULKAN)
	typedef imagecube_depth_t type;
#else
	typedef unavailable_opencl_image_type type;
#endif
};

template <COMPUTE_IMAGE_TYPE image_type>
requires((image_type & OPAQUE_IMAGE_MASK) == (COMPUTE_IMAGE_TYPE::IMAGE_CUBE_ARRAY | COMPUTE_IMAGE_TYPE::FLAG_DEPTH))
struct opaque_image_type<image_type> {
#if !defined(FLOOR_COMPUTE_OPENCL) || defined(FLOOR_COMPUTE_VULKAN)
	typedef imagecube_array_depth_t type;
#else
	typedef unavailable_opencl_image_type type;
#endif
};

#undef OPAQUE_IMAGE_MASK

#endif
