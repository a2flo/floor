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

#pragma once

namespace fl {

// IMAGE_TYPE -> OpenCL/Vulkan/Metal image*d_*t type mapping
#define OPAQUE_IMAGE_MASK ( \
IMAGE_TYPE::__DIM_MASK | \
IMAGE_TYPE::FLAG_DEPTH | \
IMAGE_TYPE::FLAG_ARRAY | \
IMAGE_TYPE::FLAG_BUFFER | \
IMAGE_TYPE::FLAG_CUBE | \
IMAGE_TYPE::FLAG_MSAA)

// nicer error message than "incomplete type" or "type does not exist"
#if defined(FLOOR_DEVICE_OPENCL) || defined(FLOOR_DEVICE_VULKAN)
struct unavailable_opencl_image_type;
#endif
#if defined(FLOOR_DEVICE_METAL)
struct unavailable_metal_image_type;
#endif

template <IMAGE_TYPE image_type> struct opaque_image_type {};

template <IMAGE_TYPE image_type>
requires((image_type & OPAQUE_IMAGE_MASK) == IMAGE_TYPE::IMAGE_1D)
struct opaque_image_type<image_type> {
	using type = image1d_t;
};

template <IMAGE_TYPE image_type>
requires((image_type & OPAQUE_IMAGE_MASK) == IMAGE_TYPE::IMAGE_1D_ARRAY)
struct opaque_image_type<image_type> {
	using type = image1d_array_t;
};

template <IMAGE_TYPE image_type>
requires((image_type & OPAQUE_IMAGE_MASK) == IMAGE_TYPE::IMAGE_1D_BUFFER)
struct opaque_image_type<image_type> {
#if !defined(FLOOR_DEVICE_METAL)
	using type = image1d_buffer_t;
#else
	using type = unavailable_metal_image_type;
#endif
};

template <IMAGE_TYPE image_type>
requires((image_type & OPAQUE_IMAGE_MASK) == IMAGE_TYPE::IMAGE_2D)
struct opaque_image_type<image_type> {
	using type = image2d_t;
};

template <IMAGE_TYPE image_type>
requires((image_type & OPAQUE_IMAGE_MASK) == IMAGE_TYPE::IMAGE_2D_ARRAY)
struct opaque_image_type<image_type> {
	using type = image2d_array_t;
};

template <IMAGE_TYPE image_type>
requires((image_type & OPAQUE_IMAGE_MASK) == IMAGE_TYPE::IMAGE_2D_MSAA)
struct opaque_image_type<image_type> {
	using type = image2d_msaa_t;
};

template <IMAGE_TYPE image_type>
requires((image_type & OPAQUE_IMAGE_MASK) == IMAGE_TYPE::IMAGE_2D_MSAA_ARRAY)
struct opaque_image_type<image_type> {
	using type = image2d_array_msaa_t;
};

// NOTE: also applies to combined stencil format
template <IMAGE_TYPE image_type>
requires((image_type & OPAQUE_IMAGE_MASK) == (IMAGE_TYPE::IMAGE_2D | IMAGE_TYPE::FLAG_DEPTH))
struct opaque_image_type<image_type> {
	using type = image2d_depth_t;
};

template <IMAGE_TYPE image_type>
requires((image_type & OPAQUE_IMAGE_MASK) == (IMAGE_TYPE::IMAGE_2D_ARRAY | IMAGE_TYPE::FLAG_DEPTH))
struct opaque_image_type<image_type> {
	using type = image2d_array_depth_t;
};

template <IMAGE_TYPE image_type>
requires((image_type & OPAQUE_IMAGE_MASK) == (IMAGE_TYPE::IMAGE_2D_MSAA | IMAGE_TYPE::FLAG_DEPTH))
struct opaque_image_type<image_type> {
	using type = image2d_msaa_depth_t;
};

template <IMAGE_TYPE image_type>
requires((image_type & OPAQUE_IMAGE_MASK) == (IMAGE_TYPE::IMAGE_2D_MSAA_ARRAY | IMAGE_TYPE::FLAG_DEPTH))
struct opaque_image_type<image_type> {
	using type = image2d_array_msaa_depth_t;
};

template <IMAGE_TYPE image_type>
requires((image_type & OPAQUE_IMAGE_MASK) == IMAGE_TYPE::IMAGE_3D)
struct opaque_image_type<image_type> {
	using type = image3d_t;
};

template <IMAGE_TYPE image_type>
requires((image_type & OPAQUE_IMAGE_MASK) == IMAGE_TYPE::IMAGE_CUBE)
struct opaque_image_type<image_type> {
#if !defined(FLOOR_DEVICE_OPENCL) || defined(FLOOR_DEVICE_VULKAN)
	using type = imagecube_t;
#else
	using type = unavailable_opencl_image_type;
#endif
};

template <IMAGE_TYPE image_type>
requires((image_type & OPAQUE_IMAGE_MASK) == IMAGE_TYPE::IMAGE_CUBE_ARRAY)
struct opaque_image_type<image_type> {
#if !defined(FLOOR_DEVICE_OPENCL) || defined(FLOOR_DEVICE_VULKAN)
	using type = imagecube_array_t;
#else
	using type = unavailable_opencl_image_type;
#endif
};

template <IMAGE_TYPE image_type>
requires((image_type & OPAQUE_IMAGE_MASK) == (IMAGE_TYPE::IMAGE_CUBE | IMAGE_TYPE::FLAG_DEPTH))
struct opaque_image_type<image_type> {
#if !defined(FLOOR_DEVICE_OPENCL) || defined(FLOOR_DEVICE_VULKAN)
	using type = imagecube_depth_t;
#else
	using type = unavailable_opencl_image_type;
#endif
};

template <IMAGE_TYPE image_type>
requires((image_type & OPAQUE_IMAGE_MASK) == (IMAGE_TYPE::IMAGE_CUBE_ARRAY | IMAGE_TYPE::FLAG_DEPTH))
struct opaque_image_type<image_type> {
#if !defined(FLOOR_DEVICE_OPENCL) || defined(FLOOR_DEVICE_VULKAN)
	using type = imagecube_array_depth_t;
#else
	using type = unavailable_opencl_image_type;
#endif
};

#undef OPAQUE_IMAGE_MASK

} // namespace fl
