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

#ifndef __FLOOR_COMPUTE_DEVICE_IMAGE_HPP__
#define __FLOOR_COMPUTE_DEVICE_IMAGE_HPP__

#if defined(FLOOR_COMPUTE_OPENCL) || defined(FLOOR_COMPUTE_METAL)
// COMPUTE_IMAGE_TYPE -> opencl/metal image type map
#include <floor/compute/device/opaque_image_map.hpp>
#endif

#if 0 // very wip

namespace floor_image_exp {
	
	//! is image type sampling return type a float?
	static constexpr bool is_sample_float(COMPUTE_IMAGE_TYPE image_type) {
		return (has_flag<COMPUTE_IMAGE_TYPE::FLAG_NORMALIZED>(image_type) ||
				(image_type & COMPUTE_IMAGE_TYPE::__DATA_TYPE_MASK) == COMPUTE_IMAGE_TYPE::FLOAT);
	}
	
	//! is image type sampling return type an int?
	static constexpr bool is_sample_int(COMPUTE_IMAGE_TYPE image_type) {
		return (!has_flag<COMPUTE_IMAGE_TYPE::FLAG_NORMALIZED>(image_type) &&
				(image_type & COMPUTE_IMAGE_TYPE::__DATA_TYPE_MASK) == COMPUTE_IMAGE_TYPE::INT);
	}
	
	//! is image type sampling return type an uint?
	static constexpr bool is_sample_uint(COMPUTE_IMAGE_TYPE image_type) {
		return (!has_flag<COMPUTE_IMAGE_TYPE::FLAG_NORMALIZED>(image_type) &&
				(image_type & COMPUTE_IMAGE_TYPE::__DATA_TYPE_MASK) == COMPUTE_IMAGE_TYPE::UINT);
	}
	
	template <COMPUTE_IMAGE_TYPE image_type>
	class const_image {
	public:
		floor_inline_always static constexpr COMPUTE_IMAGE_TYPE type() { return image_type; }
		
	protected:
		// obviously impossible
		const_image(const const_image&) = delete;
		~const_image() = delete;
		void operator=(const const_image&) = delete;
		
		// implementation specific image object
#if defined(FLOOR_COMPUTE_OPENCL) || defined(FLOOR_COMPUTE_METAL)
		__attribute__((floor_image_float)) read_only image2d_t r_img; // TODO
#elif defined(FLOOR_COMPUTE_CUDA)
		// TODO
#endif
		
	};
	
	template <COMPUTE_IMAGE_TYPE image_type>
	class image {
	public:
		floor_inline_always static constexpr COMPUTE_IMAGE_TYPE type() { return image_type; }
		
	protected:
		// obviously impossible
		image(const image&) = delete;
		~image() = delete;
		void operator=(const image&) = delete;
		
		// implementation specific image objects
#if defined(FLOOR_COMPUTE_OPENCL) || defined(FLOOR_COMPUTE_METAL)
		__attribute__((floor_image_float)) read_only image2d_t r_img; // TODO
		__attribute__((floor_image_float)) write_only image2d_t w_img; // TODO
#elif defined(FLOOR_COMPUTE_CUDA)
		// TODO
#endif
		
	};
	
}

template <COMPUTE_IMAGE_TYPE image_type> using image = floor_image_exp::image<image_type>;
template <COMPUTE_IMAGE_TYPE image_type> using const_image = floor_image_exp::const_image<image_type>;

//
template <typename sample_type, COMPUTE_IMAGE_TYPE ext_type = COMPUTE_IMAGE_TYPE::NONE>
using const_image_1d = const_image<COMPUTE_IMAGE_TYPE::IMAGE_1D | ext_type>;

template <typename sample_type, COMPUTE_IMAGE_TYPE ext_type = COMPUTE_IMAGE_TYPE::NONE>
using const_image_2d = const_image<COMPUTE_IMAGE_TYPE::IMAGE_2D | ext_type>;

template <typename sample_type, COMPUTE_IMAGE_TYPE ext_type = COMPUTE_IMAGE_TYPE::NONE>
using const_image_3d = const_image<COMPUTE_IMAGE_TYPE::IMAGE_3D | ext_type>;

#endif

#endif
