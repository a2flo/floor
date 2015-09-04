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

#ifndef __FLOOR_COMPUTE_DEVICE_OPAQUE_IMAGE_HPP__
#define __FLOOR_COMPUTE_DEVICE_OPAQUE_IMAGE_HPP__

#if defined(FLOOR_COMPUTE_OPENCL) || defined(FLOOR_COMPUTE_METAL)

// COMPUTE_IMAGE_TYPE -> metal image type map
#include <floor/compute/device/opaque_image_map.hpp>

namespace floor_image {
	//////////////////////////////////////////
	// image object implementation (opencl/metal)
	
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
	
	template <COMPUTE_IMAGE_TYPE image_type, typename image_storage>
	struct image {
		floor_inline_always static constexpr COMPUTE_IMAGE_TYPE type() { return image_type; }
		
		// convert any coordinate vector type to int* or float*
		template <typename coord_type, typename ret_coord_type = vector_n<conditional_t<is_integral<typename coord_type::decayed_scalar_type>::value, int, float>, coord_type::dim>>
		static auto convert_coord(const coord_type& coord) {
			return ret_coord_type { coord };
		}
		
		// convert any fundamental (single value) coordinate type to int or float
		template <typename coord_type, typename ret_coord_type = conditional_t<is_integral<coord_type>::value, int, float>, enable_if_t<is_fundamental<coord_type>::value>>
		static auto convert_coord(const coord_type& coord) {
			return ret_coord_type { coord };
		}
		
		// converts any fundamental (single value) type to a vector4 type (which can then be converted to a corresponding clang_*4 type)
		template <typename expected_scalar_type, typename data_type, enable_if_t<is_fundamental<data_type>::value>* = nullptr>
		static auto convert_data(const data_type& data) {
			using scalar_type = data_type;
			static_assert(is_same<scalar_type, expected_scalar_type>::value, "invalid data type");
			return vector_n<scalar_type, 4> { data, (scalar_type)0, (scalar_type)0, (scalar_type)0 };
		}
		
		// converts any vector* type to a vector4 type (which can then be converted to a corresponding clang_*4 type)
		template <typename expected_scalar_type, typename data_type, enable_if_t<!is_fundamental<data_type>::value>* = nullptr>
		static auto convert_data(const data_type& data) {
			using scalar_type = typename data_type::decayed_scalar_type;
			static_assert(is_same<scalar_type, expected_scalar_type>::value, "invalid data type");
			return vector_n<scalar_type, 4> { data };
		}
		
		// intel does not support sampler-less read functions -> create and use a sampler instead, which should do the same
#if defined(FLOOR_COMPUTE_OPENCL) && \
	(defined(FLOOR_COMPUTE_INFO_PLATFORM_VENDOR_INTEL) || defined(FLOOR_COMPUTE_INFO_PLATFORM_VENDOR_UNKNOWN))
#define FLOOR_INTEL_SAMPLER_QUIRK 1
#endif
		
		// read functions
		template <typename coord_type, COMPUTE_IMAGE_TYPE image_type_ = image_type,
				  enable_if_t<is_sample_float(image_type_) && !has_flag<COMPUTE_IMAGE_TYPE::FLAG_DEPTH>(image_type_)>* = nullptr>
		auto read(const coord_type& coord) const {
#if defined(FLOOR_INTEL_SAMPLER_QUIRK)
			const sampler_t smplr = FLOOR_OPENCL_NORMALIZED_COORDS_FALSE | FLOOR_OPENCL_ADDRESS_CLAMP_TO_EDGE | FLOOR_OPENCL_FILTER_NEAREST;
#endif
			const auto clang_vec = read_imagef(static_cast<const image_storage*>(this)->readable_img(),
#if defined(FLOOR_INTEL_SAMPLER_QUIRK)
											   smplr,
#endif
											   convert_coord(coord));
			return image_vec_ret_type<image_type, float>::fit(float4::from_clang_vector(clang_vec));
		}
		template <typename coord_type, COMPUTE_IMAGE_TYPE image_type_ = image_type,
				  enable_if_t<is_sample_float(image_type_) && has_flag<COMPUTE_IMAGE_TYPE::FLAG_DEPTH>(image_type_)>* = nullptr>
		auto read(const coord_type& coord) const {
#if defined(FLOOR_INTEL_SAMPLER_QUIRK)
			const sampler_t smplr = FLOOR_OPENCL_NORMALIZED_COORDS_FALSE | FLOOR_OPENCL_ADDRESS_CLAMP_TO_EDGE | FLOOR_OPENCL_FILTER_NEAREST;
#endif
			const auto clang_vec = read_imagef(static_cast<const image_storage*>(this)->readable_img(),
#if defined(FLOOR_INTEL_SAMPLER_QUIRK)
											   smplr,
#endif
#if defined(FLOOR_COMPUTE_METAL)
											   1,
#endif
											   convert_coord(coord));
			return image_vec_ret_type<image_type, float>::fit(float4::from_clang_vector(clang_vec));
		}
		
		template <typename coord_type, COMPUTE_IMAGE_TYPE image_type_ = image_type,
				  enable_if_t<is_sample_int(image_type_)>* = nullptr>
		auto read(const coord_type& coord) const {
#if defined(FLOOR_INTEL_SAMPLER_QUIRK)
			const sampler_t smplr = FLOOR_OPENCL_NORMALIZED_COORDS_FALSE | FLOOR_OPENCL_ADDRESS_CLAMP_TO_EDGE | FLOOR_OPENCL_FILTER_NEAREST;
#endif
			const auto clang_vec = read_imagei(static_cast<const image_storage*>(this)->readable_img(),
#if defined(FLOOR_INTEL_SAMPLER_QUIRK)
											   smplr,
#endif
											   convert_coord(coord));
			return image_vec_ret_type<image_type, int32_t>::fit(int4::from_clang_vector(clang_vec));
		}
		
		template <typename coord_type, COMPUTE_IMAGE_TYPE image_type_ = image_type,
				  enable_if_t<is_sample_uint(image_type_)>* = nullptr>
		auto read(const coord_type& coord) const {
#if defined(FLOOR_INTEL_SAMPLER_QUIRK)
			const sampler_t smplr = FLOOR_OPENCL_NORMALIZED_COORDS_FALSE | FLOOR_OPENCL_ADDRESS_CLAMP_TO_EDGE | FLOOR_OPENCL_FILTER_NEAREST;
#endif
			const auto clang_vec = read_imageui(static_cast<const image_storage*>(this)->readable_img(),
#if defined(FLOOR_INTEL_SAMPLER_QUIRK)
												smplr,
#endif
												convert_coord(coord));
			return image_vec_ret_type<image_type, uint32_t>::fit(uint4::from_clang_vector(clang_vec));
		}
		
		// write functions
		template <typename coord_type,
			      typename color_type = conditional_t<image_channel_count(image_type) == 1,
													  float, vector_n<float, image_channel_count(image_type)>>,
				  COMPUTE_IMAGE_TYPE image_type_ = image_type, enable_if_t<is_sample_float(image_type_)>* = nullptr>
		void write(const coord_type& coord, const color_type& data) const {
			write_imagef(static_cast<const image_storage*>(this)->writable_img(), convert_coord(coord), convert_data<float>(data));
		}
		
		template <typename coord_type,
			      typename color_type = conditional_t<image_channel_count(image_type) == 1,
													  int32_t, vector_n<int32_t, image_channel_count(image_type)>>,
				  COMPUTE_IMAGE_TYPE image_type_ = image_type, enable_if_t<is_sample_int(image_type_)>* = nullptr>
		void write(const coord_type& coord, const color_type& data) const {
			write_imagei(static_cast<const image_storage*>(this)->writable_img(), convert_coord(coord), convert_data<int32_t>(data));
		}
		
		template <typename coord_type,
			      typename color_type = conditional_t<image_channel_count(image_type) == 1,
													  uint32_t, vector_n<uint32_t, image_channel_count(image_type)>>,
				  COMPUTE_IMAGE_TYPE image_type_ = image_type, enable_if_t<is_sample_uint(image_type_)>* = nullptr>
		void write(const coord_type& coord, const color_type& data) const {
			write_imageui(static_cast<const image_storage*>(this)->writable_img(), convert_coord(coord), convert_data<uint32_t>(data));
		}
	};
	
	// read-only float/int/uint
	template <COMPUTE_IMAGE_TYPE image_type, typename = void>
	struct read_only_image : image<image_type, read_only_image<image_type>> {};
	
	template <COMPUTE_IMAGE_TYPE image_type>
	struct read_only_image<image_type, enable_if_t<is_sample_float(image_type)>> : image<image_type, read_only_image<image_type>> {
		typedef typename opaque_image_type<image_type>::type opaque_type;
		const opaque_type& readable_img() const { return r_img; }
		const opaque_type& writable_img() const __attribute__((unavailable("image is read-only")));
	protected:
		__attribute__((floor_image_float)) read_only opaque_type r_img;
	};
	template <COMPUTE_IMAGE_TYPE image_type>
	struct read_only_image<image_type, enable_if_t<is_sample_int(image_type)>> : image<image_type, read_only_image<image_type>> {
		typedef typename opaque_image_type<image_type>::type opaque_type;
		const opaque_type& readable_img() const { return r_img; }
		const opaque_type& writable_img() const __attribute__((unavailable("image is read-only")));
	protected:
		__attribute__((floor_image_int)) read_only opaque_type r_img;
	};
	template <COMPUTE_IMAGE_TYPE image_type>
	struct read_only_image<image_type, enable_if_t<is_sample_uint(image_type)>> : image<image_type, read_only_image<image_type>> {
		typedef typename opaque_image_type<image_type>::type opaque_type;
		const opaque_type& readable_img() const { return r_img; }
		const opaque_type& writable_img() const __attribute__((unavailable("image is read-only")));
	protected:
		__attribute__((floor_image_uint)) read_only opaque_type r_img;
	};
	
	// write-only float/int/uint
	template <COMPUTE_IMAGE_TYPE image_type, typename = void>
	struct write_only_image : image<image_type, write_only_image<image_type>> {};
	
	template <COMPUTE_IMAGE_TYPE image_type>
	struct write_only_image<image_type, enable_if_t<is_sample_float(image_type)>> : image<image_type, write_only_image<image_type>> {
		typedef typename opaque_image_type<image_type>::type opaque_type;
		const opaque_type& readable_img() const __attribute__((unavailable("image is write-only")));
		const opaque_type& writable_img() const { return w_img; }
	protected:
		__attribute__((floor_image_float)) write_only opaque_type w_img;
	};
	template <COMPUTE_IMAGE_TYPE image_type>
	struct write_only_image<image_type, enable_if_t<is_sample_int(image_type)>> : image<image_type, write_only_image<image_type>> {
		typedef typename opaque_image_type<image_type>::type opaque_type;
		const opaque_type& readable_img() const __attribute__((unavailable("image is write-only")));
		const opaque_type& writable_img() const { return w_img; }
	protected:
		__attribute__((floor_image_int)) write_only opaque_type w_img;
	};
	template <COMPUTE_IMAGE_TYPE image_type>
	struct write_only_image<image_type, enable_if_t<is_sample_uint(image_type)>> : image<image_type, write_only_image<image_type>> {
		typedef typename opaque_image_type<image_type>::type opaque_type;
		const opaque_type& readable_img() const __attribute__((unavailable("image is write-only")));
		const opaque_type& writable_img() const { return w_img; }
	protected:
		__attribute__((floor_image_uint)) write_only opaque_type w_img;
	};
	
	// read-write float/int/uint
	template <COMPUTE_IMAGE_TYPE image_type, typename = void>
	struct read_write_image : image<image_type, read_write_image<image_type>> {};
	
	template <COMPUTE_IMAGE_TYPE image_type>
	struct read_write_image<image_type, enable_if_t<is_sample_float(image_type)>> : image<image_type, read_write_image<image_type>> {
		typedef typename opaque_image_type<image_type>::type opaque_type;
		const opaque_type& readable_img() const { return r_img; }
		const opaque_type& writable_img() const { return w_img; }
	protected:
		__attribute__((floor_image_float)) read_only opaque_type r_img;
		__attribute__((floor_image_float)) write_only opaque_type w_img;
	};
	template <COMPUTE_IMAGE_TYPE image_type>
	struct read_write_image<image_type, enable_if_t<is_sample_int(image_type)>> : image<image_type, read_write_image<image_type>> {
		typedef typename opaque_image_type<image_type>::type opaque_type;
		const opaque_type& readable_img() const { return r_img; }
		const opaque_type& writable_img() const { return w_img; }
	protected:
		__attribute__((floor_image_int)) read_only opaque_type r_img;
		__attribute__((floor_image_int)) write_only opaque_type w_img;
	};
	template <COMPUTE_IMAGE_TYPE image_type>
	struct read_write_image<image_type, enable_if_t<is_sample_uint(image_type)>> : image<image_type, read_write_image<image_type>> {
		typedef typename opaque_image_type<image_type>::type opaque_type;
		const opaque_type& readable_img() const { return r_img; }
		const opaque_type& writable_img() const { return w_img; }
	protected:
		__attribute__((floor_image_uint)) read_only opaque_type r_img;
		__attribute__((floor_image_uint)) write_only opaque_type w_img;
	};
}


//! read-only image
template <COMPUTE_IMAGE_TYPE image_type> using ro_image = floor_image::read_only_image<image_type>;
//! write-only image
template <COMPUTE_IMAGE_TYPE image_type> using wo_image = floor_image::write_only_image<image_type>;
//! read-write image
template <COMPUTE_IMAGE_TYPE image_type> using rw_image = floor_image::read_write_image<image_type>;

// floor image read/write wrappers
template <COMPUTE_IMAGE_TYPE image_type, typename image_storage, typename coord_type>
floor_inline_always auto read(const floor_image::image<image_type, image_storage>& img, const coord_type& coord) {
	return img.read(coord);
}

template <COMPUTE_IMAGE_TYPE image_type, typename image_storage, typename coord_type, typename data_type>
floor_inline_always void write(const floor_image::image<image_type, image_storage>& img,
							   const coord_type& coord, const data_type& data) {
	img.write(coord, data);
}

#endif

#endif
