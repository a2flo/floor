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

namespace floor_image {
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
	
	//! returns true if coord_type is an int/integral type (int, int2, int3, ...), false if float (or anything else)
	template <typename coord_type, bool ret_value = ((is_floor_vector<coord_type>::value &&
													  is_integral<typename coord_type::decayed_scalar_type>::value) ||
													 (is_fundamental<coord_type>::value && is_integral<coord_type>::value))>
	static constexpr bool is_int_coord() {
		return ret_value;
	}
	
	//! backend specific default sampler (for integral and floating point coordinates)
	template <typename coord_type, typename = void> struct default_sampler {};
	template <typename coord_type> struct default_sampler<coord_type, enable_if_t<is_int_coord<coord_type>()>> { // int
		static constexpr auto value() {
#if defined(FLOOR_COMPUTE_OPENCL)
			return (FLOOR_OPENCL_NORMALIZED_COORDS_FALSE |
					FLOOR_OPENCL_ADDRESS_CLAMP_TO_EDGE |
					FLOOR_OPENCL_FILTER_NEAREST);
#elif defined(FLOOR_COMPUTE_METAL)
			return metal_image::sampler { /* use default params in sampler constructor */ };
#endif
		}
	};
	template <typename coord_type> struct default_sampler<coord_type, enable_if_t<!is_int_coord<coord_type>()>> { // float
		static constexpr auto value() {
#if defined(FLOOR_COMPUTE_OPENCL)
			return (FLOOR_OPENCL_NORMALIZED_COORDS_TRUE |
					FLOOR_OPENCL_ADDRESS_CLAMP_TO_EDGE |
					FLOOR_OPENCL_FILTER_NEAREST);
#elif defined(FLOOR_COMPUTE_METAL)
			return metal_image::sampler {
				metal_image::sampler::ADDRESS_MODE::CLAMP_TO_ZERO,
				metal_image::sampler::COORD_MODE::NORMALIZED
				// rest: use defaults
			};
#endif
		}
	};
	
	//! backend specific sampler type
#if defined(FLOOR_COMPUTE_OPENCL)
	typedef sampler_t sampler_type;
#elif defined(FLOOR_COMPUTE_METAL)
	typedef metal_image::sampler sampler_type;
#elif defined(FLOOR_COMPUTE_CUDA)
	enum CUDA_SAMPLER_TYPE : uint32_t {
		CUDA_CLAMP_NEAREST_NON_NORMALIZED_COORDS = 0,
		CUDA_CLAMP_NEAREST_NORMALIZED_COORDS,
		CUDA_CLAMP_LINEAR_NON_NORMALIZED_COORDS,
		CUDA_CLAMP_LINEAR_NORMALIZED_COORDS,
		__MAX_CUDA_SAMPLER_TYPE
	};
#endif
	
	//! COMPUTE_IMAGE_TYPE -> sample type
	template <COMPUTE_IMAGE_TYPE image_type, typename = void> struct to_sample_type {};
	template <COMPUTE_IMAGE_TYPE image_type> struct to_sample_type<image_type, enable_if_t<is_sample_float(image_type)>> {
		typedef float type;
	};
	template <COMPUTE_IMAGE_TYPE image_type> struct to_sample_type<image_type, enable_if_t<is_sample_int(image_type)>> {
		typedef int32_t type;
	};
	template <COMPUTE_IMAGE_TYPE image_type> struct to_sample_type<image_type, enable_if_t<is_sample_uint(image_type)>> {
		typedef uint32_t type;
	};
	//! (vector) sample type -> COMPUTE_IMAGE_TYPE
	//! NOTE: scalar sample types will always return the 4-channel variant,
	//!       vector sample types will return the corresponding channel variant
	template <typename sample_type, typename = void> struct from_sample_type {};
	template <typename sample_type> struct from_sample_type<sample_type, enable_if_t<is_same<float, sample_type>::value>> {
		floor_inline_always static constexpr COMPUTE_IMAGE_TYPE type() {
			return COMPUTE_IMAGE_TYPE::FLOAT | COMPUTE_IMAGE_TYPE::CHANNELS_4;
		}
	};
	template <typename sample_type> struct from_sample_type<sample_type, enable_if_t<is_same<int32_t, sample_type>::value>> {
		floor_inline_always static constexpr COMPUTE_IMAGE_TYPE type() {
			return COMPUTE_IMAGE_TYPE::INT | COMPUTE_IMAGE_TYPE::CHANNELS_4;
		}
	};
	template <typename sample_type> struct from_sample_type<sample_type, enable_if_t<is_same<uint32_t, sample_type>::value>> {
		floor_inline_always static constexpr COMPUTE_IMAGE_TYPE type() {
			return COMPUTE_IMAGE_TYPE::UINT | COMPUTE_IMAGE_TYPE::CHANNELS_4;
		}
	};
	template <typename sample_type> struct from_sample_type<sample_type, enable_if_t<is_floor_vector<sample_type>::value>> {
		floor_inline_always static constexpr COMPUTE_IMAGE_TYPE type() {
			// get scalar type, clear out channel count, OR with actual channel count, set FIXED_CHANNELS flag
			return ((from_sample_type<typename sample_type::this_scalar_type>::type() & ~COMPUTE_IMAGE_TYPE::__CHANNELS_MASK) |
					(COMPUTE_IMAGE_TYPE)(uint32_t(sample_type::dim - 1u) << uint32_t(COMPUTE_IMAGE_TYPE::__CHANNELS_SHIFT)) |
					COMPUTE_IMAGE_TYPE::FLAG_FIXED_CHANNELS);
		}
	};
	
	//! implementation specific image storage: read-only, write-only and read-write
	template <COMPUTE_IMAGE_TYPE image_type, typename = void> struct image_storage {};
	//! read-only
	template <COMPUTE_IMAGE_TYPE image_type>
	struct image_storage<image_type, enable_if_t<(has_flag<COMPUTE_IMAGE_TYPE::READ>(image_type) &&
												  !has_flag<COMPUTE_IMAGE_TYPE::WRITE>(image_type))>> {
		typedef typename to_sample_type<image_type>::type sample_type;
		
#if defined(FLOOR_COMPUTE_OPENCL) || defined(FLOOR_COMPUTE_METAL)
		typedef typename opaque_image_type<image_type>::type opaque_type;
		__attribute__((floor_image(sample_type), image_read_only)) opaque_type r_img;
#elif defined(FLOOR_COMPUTE_CUDA)
		// NOTE: this needs to packed like this, so that clang/llvm don't do struct expansion when only one is used
		union __attribute__((image_read_only)) {
			struct {
				struct {
					const uint64_t tex[__MAX_CUDA_SAMPLER_TYPE];
				} r_img;
				struct {
					const uint64_t surf;
				} w_img;
				const COMPUTE_IMAGE_TYPE runtime_image_type;
			};
		};
#elif defined(FLOOR_COMPUTE_HOST)
		const host_device_image<image_type>* r_img;
#endif
	};
	//! write-only
	template <COMPUTE_IMAGE_TYPE image_type>
	struct image_storage<image_type, enable_if_t<(!has_flag<COMPUTE_IMAGE_TYPE::READ>(image_type) &&
												  has_flag<COMPUTE_IMAGE_TYPE::WRITE>(image_type))>> {
		typedef typename to_sample_type<image_type>::type sample_type;
		
#if defined(FLOOR_COMPUTE_OPENCL) || defined(FLOOR_COMPUTE_METAL)
		typedef typename opaque_image_type<image_type>::type opaque_type;
		__attribute__((floor_image(sample_type), image_write_only)) opaque_type w_img;
#elif defined(FLOOR_COMPUTE_CUDA)
		// NOTE: this needs to packed like this, so that clang/llvm don't do struct expansion when only one is used
		union __attribute__((image_write_only)) {
			struct {
				struct {
					const uint64_t tex[__MAX_CUDA_SAMPLER_TYPE];
				} r_img;
				struct {
					const uint64_t surf;
				} w_img;
				const COMPUTE_IMAGE_TYPE runtime_image_type;
			};
		};
#elif defined(FLOOR_COMPUTE_HOST)
		const host_device_image<image_type>* w_img;
#endif
	};
	//! read-write
	template <COMPUTE_IMAGE_TYPE image_type>
	struct image_storage<image_type, enable_if_t<(has_flag<COMPUTE_IMAGE_TYPE::READ>(image_type) &&
												  has_flag<COMPUTE_IMAGE_TYPE::WRITE>(image_type))>> {
		typedef typename to_sample_type<image_type>::type sample_type;
		
#if defined(FLOOR_COMPUTE_OPENCL) || defined(FLOOR_COMPUTE_METAL)
		typedef typename opaque_image_type<image_type>::type opaque_type;
		__attribute__((floor_image(sample_type), image_read_only)) opaque_type r_img;
		__attribute__((floor_image(sample_type), image_write_only)) opaque_type w_img;
#elif defined(FLOOR_COMPUTE_CUDA)
		// NOTE: this needs to packed like this, so that clang/llvm don't do struct expansion when only one is used
		union __attribute__((image_read_write)) {
			struct {
				struct {
					const uint64_t tex[__MAX_CUDA_SAMPLER_TYPE];
				} r_img;
				struct {
					const uint64_t surf;
				} w_img;
				const COMPUTE_IMAGE_TYPE runtime_image_type;
			};
		};
#elif defined(FLOOR_COMPUTE_HOST)
		// all the same, just different names
		union {
			const host_device_image<image_type>* r_img;
			const host_device_image<image_type>* w_img;
		};
#endif
	};
	
	//! image base type, containing the appropriate storage type
	template <COMPUTE_IMAGE_TYPE image_type>
	struct image_base : image_storage<image_type> {
		typedef image_storage<image_type> image_storage_type;
		
		floor_inline_always static constexpr COMPUTE_IMAGE_TYPE type() { return image_type; }
		floor_inline_always static constexpr uint32_t channel_count() { return image_channel_count(image_type); }
		
		using typename image_storage_type::sample_type;
		typedef conditional_t<channel_count() == 1, sample_type, vector_n<sample_type, channel_count()>> vector_sample_type;
		
		static constexpr bool is_readable() { return has_flag<COMPUTE_IMAGE_TYPE::READ>(image_type); }
		static constexpr bool is_writable() { return has_flag<COMPUTE_IMAGE_TYPE::WRITE>(image_type); }
		static constexpr bool is_read_only() { return is_readable() && !is_writable(); }
		static constexpr bool is_write_only() { return !is_readable() && is_writable(); }
		static constexpr bool is_read_write() { return is_readable() && is_writable(); }
		
		//! convert any coordinate vector type to int* or float*
		template <typename coord_type>
		static auto convert_coord(const coord_type& coord) {
			return vector_n<conditional_t<is_integral<typename coord_type::decayed_scalar_type>::value, int, float>, coord_type::dim> {
				coord
			};
		}
		
		//! convert any fundamental (single value) coordinate type to int or float
		template <typename coord_type,
				  enable_if_t<is_fundamental<coord_type>::value>>
		static auto convert_coord(const coord_type& coord) {
			return conditional_t<is_integral<coord_type>::value, int, float> { coord };
		}
		
		//! converts any fundamental (single value) type to a vector4 type (which can then be converted to a corresponding clang_*4 type)
		template <typename expected_scalar_type, typename data_type, enable_if_t<is_fundamental<data_type>::value>* = nullptr>
		static auto convert_data(const data_type& data) {
			using scalar_type = data_type;
			static_assert(is_same<scalar_type, expected_scalar_type>::value, "invalid data type");
			return vector_n<scalar_type, 4> { data, (scalar_type)0, (scalar_type)0, (scalar_type)0 };
		}
		
		//! converts any vector* type to a vector4 type (which can then be converted to a corresponding clang_*4 type)
		template <typename expected_scalar_type, typename data_type, enable_if_t<!is_fundamental<data_type>::value>* = nullptr>
		static auto convert_data(const data_type& data) {
			using scalar_type = typename data_type::decayed_scalar_type;
			static_assert(is_same<scalar_type, expected_scalar_type>::value, "invalid data type");
			return vector_n<scalar_type, 4> { data };
		}
	};
	
	// TODO: handle MSAA and layers (parameters)
	// TODO: read_linear
	
	//! const/read-only image container
	template <COMPUTE_IMAGE_TYPE image_type>
	class const_image : public image_base<image_type> {
	public:
		typedef image_base<image_type> image_base_type;
		using typename image_base_type::image_storage_type;
		using image_storage_type::r_img;
		using image_base_type::convert_coord;
		using image_base_type::convert_data;
		
	public:
		// read functions (floating point coordinates)
		//! fp read, with fp coord
		template <typename coord_type, COMPUTE_IMAGE_TYPE image_type_ = image_type,
				  enable_if_t<(is_sample_float(image_type_) &&
							   !has_flag<COMPUTE_IMAGE_TYPE::FLAG_DEPTH>(image_type_) &&
							   !is_int_coord<coord_type>())>* = nullptr>
		auto read(const coord_type& coord) const {
#if defined(FLOOR_COMPUTE_OPENCL) || defined(FLOOR_COMPUTE_METAL)
			const sampler_type smplr = default_sampler<coord_type>::value();
			const auto clang_vec = read_imagef(r_img, smplr, convert_coord(coord));
			const auto color = float4::from_clang_vector(clang_vec);
#elif defined(FLOOR_COMPUTE_CUDA)
			const auto color = float4::from_clang_vector(cuda_image::read_imagef(r_img.tex[CUDA_CLAMP_NEAREST_NORMALIZED_COORDS],
																				 image_type, convert_coord(coord), 0));
#elif defined(FLOOR_COMPUTE_HOST)
			const auto color = r_img->read(coord);
#endif
			return image_vec_ret_type<image_type, float>::fit(color);
		}
		
		//! depth image fp read, with fp coord
		template <typename coord_type, COMPUTE_IMAGE_TYPE image_type_ = image_type,
				  enable_if_t<(is_sample_float(image_type_) &&
							   has_flag<COMPUTE_IMAGE_TYPE::FLAG_DEPTH>(image_type_) &&
							   !is_int_coord<coord_type>())>* = nullptr>
		auto read(const coord_type& coord) const {
#if defined(FLOOR_COMPUTE_OPENCL) || defined(FLOOR_COMPUTE_METAL)
			const sampler_type smplr = default_sampler<coord_type>::value();
			return read_imagef(r_img, smplr,
#if defined(FLOOR_COMPUTE_METAL) // metal weirdness, signals that depth is a float (no other types are supported)
							   1,
#endif
							   convert_coord(coord));
#elif defined(FLOOR_COMPUTE_CUDA)
			return cuda_image::read_imagef(r_img.tex[CUDA_CLAMP_NEAREST_NORMALIZED_COORDS],
										   image_type, convert_coord(coord), 0).x;
#elif defined(FLOOR_COMPUTE_HOST)
			return r_img->read(coord);
#endif
		}
		
		//! int read, with fp coord
		template <typename coord_type, COMPUTE_IMAGE_TYPE image_type_ = image_type,
				  enable_if_t<is_sample_int(image_type_) && !is_int_coord<coord_type>()>* = nullptr>
		auto read(const coord_type& coord) const {
#if defined(FLOOR_COMPUTE_OPENCL) || defined(FLOOR_COMPUTE_METAL)
			const sampler_type smplr = default_sampler<coord_type>::value();
			const auto clang_vec = read_imagei(r_img, smplr, convert_coord(coord));
			const auto color = int4::from_clang_vector(clang_vec);
#elif defined(FLOOR_COMPUTE_CUDA)
			const auto color = int4::from_clang_vector(cuda_image::read_imagei(r_img.tex[CUDA_CLAMP_NEAREST_NORMALIZED_COORDS],
																			   image_type, convert_coord(coord), 0));
#elif defined(FLOOR_COMPUTE_HOST)
			const auto color = r_img->read(coord);
#endif
			return image_vec_ret_type<image_type, int32_t>::fit(color);
		}
		
		//! uint read, with fp coord
		template <typename coord_type, COMPUTE_IMAGE_TYPE image_type_ = image_type,
				  enable_if_t<is_sample_uint(image_type_) && !is_int_coord<coord_type>()>* = nullptr>
		auto read(const coord_type& coord) const {
#if defined(FLOOR_COMPUTE_OPENCL) || defined(FLOOR_COMPUTE_METAL)
			const sampler_type smplr = default_sampler<coord_type>::value();
			const auto clang_vec = read_imageui(r_img, smplr, convert_coord(coord));
			const auto color = uint4::from_clang_vector(clang_vec);
#elif defined(FLOOR_COMPUTE_CUDA)
			const auto color = uint4::from_clang_vector(cuda_image::read_imageui(r_img.tex[CUDA_CLAMP_NEAREST_NORMALIZED_COORDS],
																				 image_type, convert_coord(coord), 0));
#elif defined(FLOOR_COMPUTE_HOST)
			const auto color = r_img->read(coord);
#endif
			return image_vec_ret_type<image_type, uint32_t>::fit(color);
		}
		
		// read functions (integral coordinates)
		//! fp read, with int coord
		template <typename coord_type, COMPUTE_IMAGE_TYPE image_type_ = image_type,
				  enable_if_t<(is_sample_float(image_type_) &&
							   !has_flag<COMPUTE_IMAGE_TYPE::FLAG_DEPTH>(image_type_) &&
							   is_int_coord<coord_type>())>* = nullptr>
		auto read(const coord_type& coord) const {
#if defined(FLOOR_COMPUTE_OPENCL) || defined(FLOOR_COMPUTE_METAL)
#if !defined(FLOOR_COMPUTE_METAL)
			const sampler_type smplr = default_sampler<coord_type>::value();
#endif
			const auto clang_vec = read_imagef(r_img,
#if !defined(FLOOR_COMPUTE_METAL)
											   smplr,
#endif
											   convert_coord(coord));
			const auto color = float4::from_clang_vector(clang_vec);
#elif defined(FLOOR_COMPUTE_CUDA)
			const auto color = float4::from_clang_vector(cuda_image::read_imagef(r_img.tex[CUDA_CLAMP_NEAREST_NON_NORMALIZED_COORDS],
																				 image_type, convert_coord(coord), 0));
#elif defined(FLOOR_COMPUTE_HOST)
			const auto color = r_img->read(coord);
#endif
			return image_vec_ret_type<image_type, float>::fit(color);
		}
		
		//! depth image fp read, with int coord
		template <typename coord_type, COMPUTE_IMAGE_TYPE image_type_ = image_type,
				  enable_if_t<(is_sample_float(image_type_) &&
							   has_flag<COMPUTE_IMAGE_TYPE::FLAG_DEPTH>(image_type_) &&
							   is_int_coord<coord_type>())>* = nullptr>
		auto read(const coord_type& coord) const {
#if defined(FLOOR_COMPUTE_OPENCL) || defined(FLOOR_COMPUTE_METAL)
#if !defined(FLOOR_COMPUTE_METAL)
			const sampler_type smplr = default_sampler<coord_type>::value();
#endif
			return read_imagef(r_img,
#if !defined(FLOOR_COMPUTE_METAL)
							   smplr,
#else // metal weirdness, signals that depth is a float (no other types are supported)
							   1,
#endif
							   convert_coord(coord));
#elif defined(FLOOR_COMPUTE_CUDA)
			return cuda_image::read_imagef(r_img.tex[CUDA_CLAMP_NEAREST_NON_NORMALIZED_COORDS],
										   image_type, convert_coord(coord), 0).x;
#elif defined(FLOOR_COMPUTE_HOST)
			return r_img->read(coord);
#endif
		}
		
		//! int read, with int coord
		template <typename coord_type, COMPUTE_IMAGE_TYPE image_type_ = image_type,
				  enable_if_t<is_sample_int(image_type_) && is_int_coord<coord_type>()>* = nullptr>
		auto read(const coord_type& coord) const {
#if defined(FLOOR_COMPUTE_OPENCL) || defined(FLOOR_COMPUTE_METAL)
#if !defined(FLOOR_COMPUTE_METAL)
			const sampler_type smplr = default_sampler<coord_type>::value();
#endif
			const auto clang_vec = read_imagei(r_img,
#if !defined(FLOOR_COMPUTE_METAL)
											   smplr,
#endif
											   convert_coord(coord));
			const auto color = int4::from_clang_vector(clang_vec);
#elif defined(FLOOR_COMPUTE_CUDA)
			const auto color = int4::from_clang_vector(cuda_image::read_imagei(r_img.tex[CUDA_CLAMP_NEAREST_NON_NORMALIZED_COORDS],
																			   image_type, convert_coord(coord), 0));
#elif defined(FLOOR_COMPUTE_HOST)
			const auto color = r_img->read(coord);
#endif
			return image_vec_ret_type<image_type, int32_t>::fit(color);
		}
		
		//! uint read, with int coord
		template <typename coord_type, COMPUTE_IMAGE_TYPE image_type_ = image_type,
				  enable_if_t<is_sample_uint(image_type_) && is_int_coord<coord_type>()>* = nullptr>
		auto read(const coord_type& coord) const {
#if defined(FLOOR_COMPUTE_OPENCL) || defined(FLOOR_COMPUTE_METAL)
#if !defined(FLOOR_COMPUTE_METAL)
			const sampler_type smplr = default_sampler<coord_type>::value();
#endif
			const auto clang_vec = read_imageui(r_img,
#if !defined(FLOOR_COMPUTE_METAL)
												smplr,
#endif
												convert_coord(coord));
			const auto color = uint4::from_clang_vector(clang_vec);
#elif defined(FLOOR_COMPUTE_CUDA)
			const auto color = uint4::from_clang_vector(cuda_image::read_imageui(r_img.tex[CUDA_CLAMP_NEAREST_NON_NORMALIZED_COORDS],
																				 image_type, convert_coord(coord), 0));
#elif defined(FLOOR_COMPUTE_HOST)
			const auto color = r_img->read(coord);
#endif
			return image_vec_ret_type<image_type, uint32_t>::fit(color);
		}
		
	};
	
	//! read-write/write-only image container
	//! NOTE: write function will be inlined for performance/code size reasons (matters for CUDA)
	template <COMPUTE_IMAGE_TYPE image_type>
	class image : public conditional_t<has_flag<COMPUTE_IMAGE_TYPE::READ>(image_type), const_image<image_type> /* r/w */, image_base<image_type> /* w/o */> {
	protected:
		typedef image_base<image_type> image_base_type;
		using typename image_base_type::image_storage_type;
		using typename image_base_type::sample_type;
		using typename image_base_type::vector_sample_type;
		using image_base_type::convert_coord;
		using image_storage_type::w_img;
#if defined(FLOOR_COMPUTE_CUDA)
		using image_storage_type::runtime_image_type;
#endif
		
	public:
		// write functions
		//! fp write
		template <typename coord_type,
				  COMPUTE_IMAGE_TYPE image_type_ = image_type, enable_if_t<is_sample_float(image_type_)>* = nullptr>
		floor_inline_always void write(const coord_type& coord, const vector_sample_type& data) const {
#if defined(FLOOR_COMPUTE_OPENCL) || defined(FLOOR_COMPUTE_METAL)
			write_imagef(w_img, convert_coord(coord), image_base_type::template convert_data<sample_type>(data));
#elif defined(FLOOR_COMPUTE_CUDA)
			cuda_image::write_float<image_type>(w_img.surf, runtime_image_type, convert_coord(coord), 0,
												image_base_type::template convert_data<sample_type>(data));
#elif defined(FLOOR_COMPUTE_HOST)
			w_img->write(coord, image_base_type::template convert_data<sample_type>(data));
#endif
		}
		
		//! int write
		template <typename coord_type,
				  COMPUTE_IMAGE_TYPE image_type_ = image_type, enable_if_t<is_sample_int(image_type_)>* = nullptr>
		floor_inline_always void write(const coord_type& coord, const vector_sample_type& data) const {
#if defined(FLOOR_COMPUTE_OPENCL) || defined(FLOOR_COMPUTE_METAL)
			write_imagei(w_img, convert_coord(coord), image_base_type::template convert_data<sample_type>(data));
#elif defined(FLOOR_COMPUTE_CUDA)
			cuda_image::write_int<image_type>(w_img.surf, runtime_image_type, convert_coord(coord), 0,
											  image_base_type::template convert_data<sample_type>(data));
#elif defined(FLOOR_COMPUTE_HOST)
			w_img->write(coord, image_base_type::template convert_data<sample_type>(data));
#endif
		}
		
		//! uint write
		template <typename coord_type,
				  COMPUTE_IMAGE_TYPE image_type_ = image_type, enable_if_t<is_sample_uint(image_type_)>* = nullptr>
		floor_inline_always void write(const coord_type& coord, const vector_sample_type& data) const {
#if defined(FLOOR_COMPUTE_OPENCL) || defined(FLOOR_COMPUTE_METAL)
			write_imageui(w_img, convert_coord(coord), image_base_type::template convert_data<sample_type>(data));
#elif defined(FLOOR_COMPUTE_CUDA)
			cuda_image::write_uint<image_type>(w_img.surf, runtime_image_type, convert_coord(coord), 0,
											   image_base_type::template convert_data<sample_type>(data));
#elif defined(FLOOR_COMPUTE_HOST)
			w_img->write(coord, image_base_type::template convert_data<sample_type>(data));
#endif
		}
		
	};
	
}

//! read-write image (if write_only == false), write-only image (if write_only == true)
template <COMPUTE_IMAGE_TYPE image_type, bool write_only = false>
using image = floor_image::image<(image_type | COMPUTE_IMAGE_TYPE::WRITE |
								  (write_only ? COMPUTE_IMAGE_TYPE::NONE : COMPUTE_IMAGE_TYPE::READ))>;

//! const/read-only image
template <COMPUTE_IMAGE_TYPE image_type>
using const_image = floor_image::const_image<image_type | COMPUTE_IMAGE_TYPE::READ>;

// const/read-only image types
template <typename sample_type, COMPUTE_IMAGE_TYPE ext_type = COMPUTE_IMAGE_TYPE::NONE>
using const_image_1d = const_image<COMPUTE_IMAGE_TYPE::IMAGE_1D | ext_type | floor_image::from_sample_type<sample_type>::type()>;

template <typename sample_type, COMPUTE_IMAGE_TYPE ext_type = COMPUTE_IMAGE_TYPE::NONE>
using const_image_2d = const_image<COMPUTE_IMAGE_TYPE::IMAGE_2D | ext_type | floor_image::from_sample_type<sample_type>::type()>;

template <typename sample_type, COMPUTE_IMAGE_TYPE ext_type = COMPUTE_IMAGE_TYPE::NONE>
using const_image_2d_depth = const_image<(COMPUTE_IMAGE_TYPE::IMAGE_DEPTH | ext_type |
										  // always single channel
										  (floor_image::from_sample_type<sample_type>::type() & ~COMPUTE_IMAGE_TYPE::__CHANNELS_MASK))>;

template <typename sample_type, COMPUTE_IMAGE_TYPE ext_type = COMPUTE_IMAGE_TYPE::NONE>
using const_image_3d = const_image<COMPUTE_IMAGE_TYPE::IMAGE_3D | ext_type | floor_image::from_sample_type<sample_type>::type()>;

// read-write/write-only image types
template <typename sample_type, bool write_only = false, COMPUTE_IMAGE_TYPE ext_type = COMPUTE_IMAGE_TYPE::NONE>
using image_2d = image<COMPUTE_IMAGE_TYPE::IMAGE_2D | ext_type | floor_image::from_sample_type<sample_type>::type(), write_only>;

#endif
