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

#ifndef __FLOOR_COMPUTE_DEVICE_METAL_IMAGE_HPP__
#define __FLOOR_COMPUTE_DEVICE_METAL_IMAGE_HPP__

#if defined(FLOOR_COMPUTE_METAL) && 0

// COMPUTE_IMAGE_TYPE -> metal image type map
#include <floor/compute/device/opaque_image_map.hpp>

//
namespace metal_image {
	struct sampler {
		enum ADDRESS_MODE {
			CLAMP_TO_ZERO	= 0,
			CLAMP_TO_EDGE	= 1,
			REPEAT			= 2,
			MIRRORED_REPEAT	= 3
		};
		enum FILTER_MODE {
			NEAREST			= 0,
			LINEAR			= 1
		};
		enum MIP_FILTER_MODE {
			MIP_NONE		= 0,
			MIP_NEAREST		= 1,
			MIP_LINEAR		= 2
		};
		enum COORD_MODE {
			NORMALIZED		= 0,
			PIXEL			= 1
		};
		enum COMPARE_FUNC {
			NONE			= 0,
			LESS			= 1,
			LESS_EQUAL		= 2,
			GREATER			= 3,
			GREATER_EQUAL	= 4,
			EQUAL			= 5,
			NOT_EQUAL		= 6,
			// unavailable right now
			// ALWAYS		= 7,
			// NEVER		= 8
		};
		
		union {
			struct {
				// address modes
				uint64_t s_address : 3;
				uint64_t t_address : 3;
				uint64_t r_address : 3;
				
				// filter modes
				uint64_t mag_filter : 2;
				uint64_t min_filter : 2;
				uint64_t mip_filter : 2;
				
				// coord mode
				uint64_t coord_mode : 1;
				
				// compare function
				uint64_t compare_func : 4;
				
				// currently unused/reserved
				uint64_t _unused : 43;
				
				// constant sampler flag
				uint64_t is_constant : 1;
			};
			uint64_t value;
		};
		
		// must be known at compile-time for now
		constexpr sampler(const ADDRESS_MODE address_mode = CLAMP_TO_EDGE,
						  const COORD_MODE coord_mode_ = PIXEL,
						  const FILTER_MODE filter_mode = NEAREST,
						  const MIP_FILTER_MODE mip_filter_mode = MIP_NONE,
						  const COMPARE_FUNC compare_func_ = NONE) :
		s_address(address_mode), t_address(address_mode), r_address(address_mode),
		coord_mode(coord_mode_),
		mag_filter(filter_mode), min_filter(filter_mode),
		mip_filter(mip_filter_mode),
		compare_func(compare_func_),
		_unused(0u), is_constant(1u) {}
		
		// unavailable
		sampler(const sampler&) = delete;
		void operator=(const sampler&) = delete;
		void operator&() = delete;
	};
	
	//
	template <COMPUTE_IMAGE_TYPE image_type>
	struct image {
		floor_inline_always static constexpr COMPUTE_IMAGE_TYPE type() { return image_type; }
		
	private:
		typename opaque_image_type<image_type>::type img;
	};
}

//
#define ro_image read_only metal_image_t
#define wo_image write_only metal_image_t
#define rw_image read_write metal_image_t
template <COMPUTE_IMAGE_TYPE image_type> using metal_image_t = metal_image::image<image_type>;


// convert any coordinate vector type to int* or float*
template <typename coord_type, typename ret_coord_type = vector_n<conditional_t<is_integral<typename coord_type::decayed_scalar_type>::value, int, float>, coord_type::dim>>
auto convert_ocl_coord(const coord_type& coord) {
	return ret_coord_type { coord };
}
// convert any fundamental (single value) coordinate type to int or float
template <typename coord_type, typename ret_coord_type = conditional_t<is_integral<coord_type>::value, int, float>, enable_if_t<is_fundamental<coord_type>::value>>
auto convert_ocl_coord(const coord_type& coord) {
	return ret_coord_type { coord };
}

// floor image read/write wrappers
template <COMPUTE_IMAGE_TYPE image_type, typename coord_type,
		  enable_if_t<(has_flag<COMPUTE_IMAGE_TYPE::FLAG_NORMALIZED>(image_type) ||
					   (image_type & COMPUTE_IMAGE_TYPE::__DATA_TYPE_MASK) == COMPUTE_IMAGE_TYPE::FLOAT), int> = 0>
auto read(const metal_image_t<image_type>& img, const coord_type& coord) {
	constexpr const metal_image::sampler smplr {};
	return image_vec_ret_type<image_type, float>::fit(float4 {});
}

template <COMPUTE_IMAGE_TYPE image_type, typename coord_type,
		  enable_if_t<(!has_flag<COMPUTE_IMAGE_TYPE::FLAG_NORMALIZED>(image_type) &&
					   (image_type & COMPUTE_IMAGE_TYPE::__DATA_TYPE_MASK) == COMPUTE_IMAGE_TYPE::INT), int> = 0>
auto read(const metal_image_t<image_type>& img, const coord_type& coord) {
	constexpr const metal_image::sampler smplr {};
	return image_vec_ret_type<image_type, int32_t>::fit(int4 {});
}

template <COMPUTE_IMAGE_TYPE image_type, typename coord_type,
		  enable_if_t<(!has_flag<COMPUTE_IMAGE_TYPE::FLAG_NORMALIZED>(image_type) &&
					   (image_type & COMPUTE_IMAGE_TYPE::__DATA_TYPE_MASK) == COMPUTE_IMAGE_TYPE::UINT), int> = 0>
auto read(const metal_image_t<image_type>& img, const coord_type& coord) {
	constexpr const metal_image::sampler smplr {};
	return image_vec_ret_type<image_type, uint32_t>::fit(uint4 {});
}

// TODO: other write functions
template <COMPUTE_IMAGE_TYPE image_type>
floor_inline_always void write(const metal_image_t<image_type>& img, const int2& coord, const float4& data) {
}

#endif

#endif
