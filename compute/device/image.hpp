/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2016 Florian Ziesche
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

//! depth compare functions
enum class COMPARE_FUNCTION : uint32_t {
	NONE				= 0u,
	LESS_OR_EQUAL		= 1u,
	GREATER_OR_EQUAL	= 2u,
	LESS				= 3u,
	GREATER				= 4u,
	EQUAL				= 5u,
	NOT_EQUAL			= 6u,
	ALWAYS				= 7u,
	NEVER				= 8u,
};

//! preliminary/wip sampler type
struct sampler {
	COMPARE_FUNCTION compare_function;
};

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
	
#if defined(FLOOR_COMPUTE_OPENCL) || defined(FLOOR_COMPUTE_METAL)
#if defined(FLOOR_COMPUTE_METAL)
	constexpr metal_image::sampler::COMPARE_FUNCTION compare_function_floor_to_metal(const COMPARE_FUNCTION& func) {
		switch(func) {
			case COMPARE_FUNCTION::NONE: return metal_image::sampler::COMPARE_FUNCTION::NONE;
			case COMPARE_FUNCTION::LESS: return metal_image::sampler::COMPARE_FUNCTION::LESS;
			case COMPARE_FUNCTION::LESS_OR_EQUAL: return metal_image::sampler::COMPARE_FUNCTION::LESS_EQUAL;
			case COMPARE_FUNCTION::GREATER: return metal_image::sampler::COMPARE_FUNCTION::GREATER;
			case COMPARE_FUNCTION::GREATER_OR_EQUAL: return metal_image::sampler::COMPARE_FUNCTION::GREATER_EQUAL;
			case COMPARE_FUNCTION::EQUAL: return metal_image::sampler::COMPARE_FUNCTION::EQUAL;
			case COMPARE_FUNCTION::NOT_EQUAL: return metal_image::sampler::COMPARE_FUNCTION::NOT_EQUAL;
			case COMPARE_FUNCTION::ALWAYS: return metal_image::sampler::COMPARE_FUNCTION::ALWAYS;
			case COMPARE_FUNCTION::NEVER: return metal_image::sampler::COMPARE_FUNCTION::NEVER;
		}
		floor_unreachable();
	}
#endif
	
	//! backend specific default sampler (for integral and floating point coordinates)
	template <typename coord_type, bool sample_linear, COMPARE_FUNCTION = COMPARE_FUNCTION::NONE, typename = void>
	struct default_sampler {};
	template <typename coord_type, bool sample_linear, COMPARE_FUNCTION compare_function>
	struct default_sampler<coord_type, sample_linear, compare_function,
						   enable_if_t<is_int_coord<coord_type>() && !sample_linear>> { // int (nearest)
		static constexpr auto value() {
#if defined(FLOOR_COMPUTE_OPENCL)
			return (opencl_image::sampler::ADDRESS_MODE::CLAMP_TO_EDGE,
					opencl_image::sampler::COORD_MODE::PIXEL |
					opencl_image::sampler::FILTER_MODE::NEAREST);
#elif defined(FLOOR_COMPUTE_METAL)
			return metal_image::sampler {
				metal_image::sampler::ADDRESS_MODE::CLAMP_TO_EDGE,
				metal_image::sampler::COORD_MODE::PIXEL,
				metal_image::sampler::FILTER_MODE::NEAREST,
				metal_image::sampler::MIP_FILTER_MODE::MIP_NONE,
				compare_function_floor_to_metal(compare_function)
			};
#endif
		}
	};
	template <typename coord_type, bool sample_linear, COMPARE_FUNCTION compare_function>
	struct default_sampler<coord_type, sample_linear, compare_function,
						   enable_if_t<!is_int_coord<coord_type>() && !sample_linear>> { // float (nearest)
		static constexpr auto value() {
#if defined(FLOOR_COMPUTE_OPENCL)
			return (opencl_image::sampler::ADDRESS_MODE::CLAMP_TO_EDGE,
					opencl_image::sampler::COORD_MODE::NORMALIZED |
					opencl_image::sampler::FILTER_MODE::NEAREST);
#elif defined(FLOOR_COMPUTE_METAL)
			return metal_image::sampler {
				metal_image::sampler::ADDRESS_MODE::CLAMP_TO_EDGE,
				metal_image::sampler::COORD_MODE::NORMALIZED,
				metal_image::sampler::FILTER_MODE::NEAREST,
				metal_image::sampler::MIP_FILTER_MODE::MIP_NONE,
				compare_function_floor_to_metal(compare_function)
			};
#endif
		}
	};
	template <typename coord_type, bool sample_linear, COMPARE_FUNCTION compare_function>
	struct default_sampler<coord_type, sample_linear, compare_function,
						   enable_if_t<is_int_coord<coord_type>() && sample_linear>> { // int (linear)
		static constexpr auto value() {
#if defined(FLOOR_COMPUTE_OPENCL)
			return (opencl_image::sampler::ADDRESS_MODE::CLAMP_TO_EDGE |
					opencl_image::sampler::COORD_MODE::PIXEL |
					opencl_image::sampler::FILTER_MODE::LINEAR);
#elif defined(FLOOR_COMPUTE_METAL)
			return metal_image::sampler {
				metal_image::sampler::ADDRESS_MODE::CLAMP_TO_EDGE,
				metal_image::sampler::COORD_MODE::PIXEL,
				metal_image::sampler::FILTER_MODE::LINEAR,
				metal_image::sampler::MIP_FILTER_MODE::MIP_LINEAR,
				compare_function_floor_to_metal(compare_function)
			};
#endif
		}
	};
	template <typename coord_type, bool sample_linear, COMPARE_FUNCTION compare_function>
	struct default_sampler<coord_type, sample_linear, compare_function,
						   enable_if_t<!is_int_coord<coord_type>() && sample_linear>> { // float (linear)
		static constexpr auto value() {
#if defined(FLOOR_COMPUTE_OPENCL)
			return (opencl_image::sampler::ADDRESS_MODE::CLAMP_TO_EDGE |
					opencl_image::sampler::COORD_MODE::NORMALIZED |
					opencl_image::sampler::FILTER_MODE::LINEAR);
#elif defined(FLOOR_COMPUTE_METAL)
			return metal_image::sampler {
				metal_image::sampler::ADDRESS_MODE::CLAMP_TO_EDGE,
				metal_image::sampler::COORD_MODE::NORMALIZED,
				metal_image::sampler::FILTER_MODE::LINEAR,
				metal_image::sampler::MIP_FILTER_MODE::MIP_LINEAR,
				compare_function_floor_to_metal(compare_function)
			};
#endif
		}
	};
#endif
	
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
		
#if !defined(FLOOR_COMPUTE_HOST) // opencl/metal/cuda coordinate conversion
		//! convert any coordinate vector type to int* or float* clang vector types
		template <typename coord_type>
		static auto convert_coord(const coord_type& coord) {
			return (vector_n<conditional_t<is_integral<typename coord_type::decayed_scalar_type>::value, int, float>, coord_type::dim> {
				coord
			}).to_clang_vector();
		}
		
		//! convert any fundamental (single value) coordinate type to int or float
		template <typename coord_type,
				  enable_if_t<is_fundamental<coord_type>::value>>
		static auto convert_coord(const coord_type& coord) {
			return conditional_t<is_integral<coord_type>::value, int, float> { coord };
		}
#else // host-compute
		//! convert any coordinate vector type to floor int{1,2,3,4} or float{1,2,3,4} vectors
		template <typename coord_type>
		static auto convert_coord(const coord_type& coord) {
			typedef conditional_t<is_fundamental<coord_type>::value,
								  conditional_t<is_integral<coord_type>::value, int, float>,
								  conditional_t<is_integral<typename coord_type::decayed_scalar_type>::value, int, float>> scalar_type;
			return vector_n<scalar_type, __builtin_choose_expr(is_fundamental<coord_type>::value, 1, coord_type::dim)> {
				coord
			};
		}
#endif
		
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
	
	//! const/read-only image container
	template <COMPUTE_IMAGE_TYPE image_type>
	class const_image : public image_base<image_type> {
	public:
		typedef image_base<image_type> image_base_type;
		using typename image_base_type::image_storage_type;
		using typename image_base_type::sample_type;
		using image_storage_type::r_img;
		using image_base_type::convert_coord;
		
	public:
		//! internal read function, handling all kinds of reads
		//! NOTE: while this is an internal function, it might be useful for anyone insane enough to use it directly on the outside
		//!       -> this is a public function and not protected
		template <bool sample_linear, typename coord_type>
		floor_inline_always auto read_internal(const coord_type& coord,
											   const uint32_t layer,
											   const uint32_t sample
#if defined(FLOOR_COMPUTE_HOST) // NOTE: MSAA/sample is not supported on host-compute
											   floor_unused
#endif
		) const {
			constexpr const bool is_float = is_sample_float(image_type);
			constexpr const bool is_int = is_sample_int(image_type);
			static_assert(is_float || is_int || is_sample_uint(image_type), "invalid sample type");
			
			// backend specific coordinate conversion (also: any input -> float or int)
			const auto converted_coord = convert_coord(coord);
			
#if defined(FLOOR_COMPUTE_OPENCL) || defined(FLOOR_COMPUTE_METAL)
			const sampler_type smplr = default_sampler<coord_type, sample_linear>::value();
#if defined(FLOOR_COMPUTE_OPENCL)
			const auto read_color = __builtin_choose_expr(is_float,
														  opencl_image::read_float(r_img, smplr, converted_coord, layer, sample),
														  __builtin_choose_expr(is_int,
														  opencl_image::read_int(r_img, smplr, converted_coord, layer, sample),
														  opencl_image::read_uint(r_img, smplr, converted_coord, layer, sample)));
#else
			const auto read_color = __builtin_choose_expr(is_float,
														  metal_image::read_float(r_img, smplr, converted_coord, layer, sample),
														  __builtin_choose_expr(is_int,
														  metal_image::read_int(r_img, smplr, converted_coord, layer, sample),
														  metal_image::read_uint(r_img, smplr, converted_coord, layer, sample)));
#endif
#elif defined(FLOOR_COMPUTE_CUDA)
			constexpr const auto cuda_tex_idx = (!sample_linear ?
												 (!is_int_coord<coord_type>() ?
												  CUDA_CLAMP_NEAREST_NORMALIZED_COORDS : CUDA_CLAMP_NEAREST_NON_NORMALIZED_COORDS) :
												 (!is_int_coord<coord_type>() ?
												  CUDA_CLAMP_LINEAR_NORMALIZED_COORDS : CUDA_CLAMP_LINEAR_NON_NORMALIZED_COORDS));
			const auto read_color = __builtin_choose_expr(is_float,
														  cuda_image::read_imagef(r_img.tex[cuda_tex_idx], image_type, converted_coord, layer, sample),
														  __builtin_choose_expr(is_int,
														  cuda_image::read_imagei(r_img.tex[cuda_tex_idx], image_type, converted_coord, layer, sample),
														  cuda_image::read_imageui(r_img.tex[cuda_tex_idx], image_type, converted_coord, layer, sample)));
#elif defined(FLOOR_COMPUTE_HOST)
			// TODO: (bi)linear sampling
			const auto color = r_img->read(converted_coord, layer);
#endif
			
#if defined(FLOOR_COMPUTE_OPENCL) || defined(FLOOR_COMPUTE_METAL) || defined(FLOOR_COMPUTE_CUDA)
			const auto color = __builtin_choose_expr(!has_flag<COMPUTE_IMAGE_TYPE::FLAG_DEPTH>(image_type),
													 vector_n<sample_type, 4>::from_clang_vector(read_color),
#if !defined(FLOOR_COMPUTE_CUDA)
													 // opencl/metal: depth is always a scalar value, so just pass it through
													 read_color
#else
													 // cuda: take .x component manually
													 read_color.x
#endif
													 );
#endif
			return image_vec_ret_type<image_type, sample_type>::fit(color);
		}
		
		//! image read with nearest/point sampling (non-array, non-msaa)
		template <typename coord_type, COMPUTE_IMAGE_TYPE image_type_ = image_type,
				  enable_if_t<(!has_flag<COMPUTE_IMAGE_TYPE::FLAG_ARRAY>(image_type_) &&
							   !has_flag<COMPUTE_IMAGE_TYPE::FLAG_MSAA>(image_type_))>* = nullptr>
		auto read(const coord_type& coord) const {
			return read_internal<false>(coord, 0, 0);
		}
		
		//! image read with nearest/point sampling (array, non-msaa)
		template <typename coord_type, COMPUTE_IMAGE_TYPE image_type_ = image_type,
				  enable_if_t<(has_flag<COMPUTE_IMAGE_TYPE::FLAG_ARRAY>(image_type_) &&
							   !has_flag<COMPUTE_IMAGE_TYPE::FLAG_MSAA>(image_type_))>* = nullptr>
		auto read(const coord_type& coord, const uint32_t layer) const {
			return read_internal<false>(coord, layer, 0);
		}
		
		//! image read with nearest/point sampling (non-array, msaa)
		template <typename coord_type, COMPUTE_IMAGE_TYPE image_type_ = image_type,
				  enable_if_t<(!has_flag<COMPUTE_IMAGE_TYPE::FLAG_ARRAY>(image_type_) &&
							   has_flag<COMPUTE_IMAGE_TYPE::FLAG_MSAA>(image_type_))>* = nullptr>
		auto read(const coord_type& coord, const uint32_t sample) const {
			return read_internal<false>(coord, 0, sample);
		}
		
		//! image read with nearest/point sampling (array, msaa)
		template <typename coord_type, COMPUTE_IMAGE_TYPE image_type_ = image_type,
				  enable_if_t<(has_flag<COMPUTE_IMAGE_TYPE::FLAG_ARRAY>(image_type_) &&
							   has_flag<COMPUTE_IMAGE_TYPE::FLAG_MSAA>(image_type_))>* = nullptr>
		auto read(const coord_type& coord, const uint32_t layer, const uint32_t sample) const {
			return read_internal<false>(coord, layer, sample);
		}
		
		//! image read with linear sampling (non-array, non-msaa)
		template <typename coord_type, COMPUTE_IMAGE_TYPE image_type_ = image_type,
				  enable_if_t<(!has_flag<COMPUTE_IMAGE_TYPE::FLAG_ARRAY>(image_type_) &&
							   !has_flag<COMPUTE_IMAGE_TYPE::FLAG_MSAA>(image_type_))>* = nullptr>
		auto read_linear(const coord_type& coord) const {
			return read_internal<true>(coord, 0, 0);
		}
		
		//! image read with linear sampling (array, non-msaa)
		template <typename coord_type, COMPUTE_IMAGE_TYPE image_type_ = image_type,
				  enable_if_t<(has_flag<COMPUTE_IMAGE_TYPE::FLAG_ARRAY>(image_type_) &&
							   !has_flag<COMPUTE_IMAGE_TYPE::FLAG_MSAA>(image_type_))>* = nullptr>
		auto read_linear(const coord_type& coord, const uint32_t layer) const {
			return read_internal<true>(coord, layer, 0);
		}
		
		//! image read with linear sampling (non-array, msaa)
		template <typename coord_type, COMPUTE_IMAGE_TYPE image_type_ = image_type,
				  enable_if_t<(!has_flag<COMPUTE_IMAGE_TYPE::FLAG_ARRAY>(image_type_) &&
							   has_flag<COMPUTE_IMAGE_TYPE::FLAG_MSAA>(image_type_))>* = nullptr>
		auto read_linear(const coord_type& coord, const uint32_t sample) const {
			return read_internal<true>(coord, 0, sample);
		}
		
		//! image read with linear sampling (array, msaa)
		template <typename coord_type, COMPUTE_IMAGE_TYPE image_type_ = image_type,
				  enable_if_t<(has_flag<COMPUTE_IMAGE_TYPE::FLAG_ARRAY>(image_type_) &&
							   has_flag<COMPUTE_IMAGE_TYPE::FLAG_MSAA>(image_type_))>* = nullptr>
		auto read_linear(const coord_type& coord, const uint32_t layer, const uint32_t sample) const {
			return read_internal<true>(coord, layer, sample);
		}
		
		// depth compare read functions are currently only available for metal,
		// host-compute will follow at some point,
		// cuda technically supports depth compare ptx instructions, but no way to set the compare function?!,
		// opencl/spir doesn't support this at all right now
#if defined(FLOOR_COMPUTE_METAL)
		//! internal depth compare read function, handling all kinds of depth compare reads
		template <bool sample_linear, COMPARE_FUNCTION compare_function, typename coord_type,
				  COMPUTE_IMAGE_TYPE image_type_ = image_type, enable_if_t<is_sample_float(image_type_)>* = nullptr>
		floor_inline_always auto compare_internal(const coord_type& coord,
												  const float& compare_value,
												  const uint32_t layer
		) const {
			// backend specific coordinate conversion (also: any input -> float or int)
			const auto converted_coord = convert_coord(coord);
			const sampler_type smplr = default_sampler<coord_type, sample_linear, compare_function>::value();
			return metal_image::read_compare_float(r_img, smplr, converted_coord, layer, compare_value);
		}
		
		//! image depth compare read with nearest/point sampling (non-array)
		template <COMPARE_FUNCTION compare_function, typename coord_type, COMPUTE_IMAGE_TYPE image_type_ = image_type,
				  enable_if_t<!has_flag<COMPUTE_IMAGE_TYPE::FLAG_ARRAY>(image_type_)>* = nullptr>
		auto compare(const coord_type& coord, const float& compare_value) const {
			return compare_internal<false, compare_function>(coord, compare_value, 0);
		}
		
		//! image depth compare read with nearest/point sampling (array)
		template <COMPARE_FUNCTION compare_function, typename coord_type, COMPUTE_IMAGE_TYPE image_type_ = image_type,
				  enable_if_t<has_flag<COMPUTE_IMAGE_TYPE::FLAG_ARRAY>(image_type_)>* = nullptr>
		auto compare(const coord_type& coord, const uint32_t layer, const float& compare_value) const {
			return compare_internal<false, compare_function>(coord, compare_value, layer);
		}
		
		//! image depth compare read with linear sampling (non-array)
		template <COMPARE_FUNCTION compare_function, typename coord_type, COMPUTE_IMAGE_TYPE image_type_ = image_type,
				  enable_if_t<!has_flag<COMPUTE_IMAGE_TYPE::FLAG_ARRAY>(image_type_)>* = nullptr>
		auto compare_linear(const coord_type& coord, const float& compare_value) const {
			return compare_internal<true, compare_function>(coord, compare_value, 0);
		}
		
		//! image depth compare read with linear sampling (array)
		template <COMPARE_FUNCTION compare_function, typename coord_type, COMPUTE_IMAGE_TYPE image_type_ = image_type,
				  enable_if_t<has_flag<COMPUTE_IMAGE_TYPE::FLAG_ARRAY>(image_type_)>* = nullptr>
		auto compare_linear(const coord_type& coord, const uint32_t layer, const float& compare_value) const {
			return compare_internal<true, compare_function>(coord, compare_value, layer);
		}
#endif
		
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
		//! internal fp write function
		template <typename coord_type,
				  COMPUTE_IMAGE_TYPE image_type_ = image_type, enable_if_t<is_sample_float(image_type_)>* = nullptr>
		floor_inline_always void write_internal(const coord_type& coord, const uint32_t layer, const vector_sample_type& data) const {
#if defined(FLOOR_COMPUTE_OPENCL)
			opencl_image::write_float(w_img, convert_coord(coord), layer, image_base_type::template convert_data<sample_type>(data));
#elif defined(FLOOR_COMPUTE_METAL)
			metal_image::write_float(w_img, convert_coord(coord), layer, image_base_type::template convert_data<sample_type>(data));
#elif defined(FLOOR_COMPUTE_CUDA)
			cuda_image::write_float<image_type>(w_img.surf, runtime_image_type, convert_coord(coord), layer,
												image_base_type::template convert_data<sample_type>(data));
#elif defined(FLOOR_COMPUTE_HOST)
			w_img->write(convert_coord(coord), layer, image_base_type::template convert_data<sample_type>(data));
#endif
		}
		
		//! internal int write function
		template <typename coord_type,
				  COMPUTE_IMAGE_TYPE image_type_ = image_type, enable_if_t<is_sample_int(image_type_)>* = nullptr>
		floor_inline_always void write_internal(const coord_type& coord, const uint32_t layer, const vector_sample_type& data) const {
#if defined(FLOOR_COMPUTE_OPENCL)
			opencl_image::write_int(w_img, convert_coord(coord), layer, image_base_type::template convert_data<sample_type>(data));
#elif defined(FLOOR_COMPUTE_METAL)
			metal_image::write_int(w_img, convert_coord(coord), layer, image_base_type::template convert_data<sample_type>(data));
#elif defined(FLOOR_COMPUTE_CUDA)
			cuda_image::write_int<image_type>(w_img.surf, runtime_image_type, convert_coord(coord), layer,
											  image_base_type::template convert_data<sample_type>(data));
#elif defined(FLOOR_COMPUTE_HOST)
			w_img->write(convert_coord(coord), layer, image_base_type::template convert_data<sample_type>(data));
#endif
		}
		
		//! internal uint write function
		template <typename coord_type,
				  COMPUTE_IMAGE_TYPE image_type_ = image_type, enable_if_t<is_sample_uint(image_type_)>* = nullptr>
		floor_inline_always void write_internal(const coord_type& coord, const uint32_t layer, const vector_sample_type& data) const {
#if defined(FLOOR_COMPUTE_OPENCL)
			opencl_image::write_uint(w_img, convert_coord(coord), layer, image_base_type::template convert_data<sample_type>(data));
#elif defined(FLOOR_COMPUTE_METAL)
			metal_image::write_uint(w_img, convert_coord(coord), layer, image_base_type::template convert_data<sample_type>(data));
#elif defined(FLOOR_COMPUTE_CUDA)
			cuda_image::write_uint<image_type>(w_img.surf, runtime_image_type, convert_coord(coord), layer,
											   image_base_type::template convert_data<sample_type>(data));
#elif defined(FLOOR_COMPUTE_HOST)
			w_img->write(convert_coord(coord), layer, image_base_type::template convert_data<sample_type>(data));
#endif
		}
		
		//! image write (non-array)
		template <typename coord_type, COMPUTE_IMAGE_TYPE image_type_ = image_type,
				  enable_if_t<!has_flag<COMPUTE_IMAGE_TYPE::FLAG_ARRAY>(image_type_)>* = nullptr>
		floor_inline_always auto write(const coord_type& coord,
				   const vector_sample_type& data) const {
			static_assert(is_int_coord<coord_type>(), "image write coordinates must be of integer type");
			return write_internal(coord, 0, data);
		}
		
		//! image write (array)
		template <typename coord_type, COMPUTE_IMAGE_TYPE image_type_ = image_type,
				  enable_if_t<has_flag<COMPUTE_IMAGE_TYPE::FLAG_ARRAY>(image_type_)>* = nullptr>
		floor_inline_always auto write(const coord_type& coord,
				   const uint32_t layer,
				   const vector_sample_type& data) const {
			static_assert(is_int_coord<coord_type>(), "image write coordinates must be of integer type");
			return write_internal(coord, layer, data);
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
template <typename sample_type, COMPUTE_IMAGE_TYPE ext_type = COMPUTE_IMAGE_TYPE::NONE> using const_image_1d =
const_image<COMPUTE_IMAGE_TYPE::IMAGE_1D | ext_type | floor_image::from_sample_type<sample_type>::type()>;

template <typename sample_type, COMPUTE_IMAGE_TYPE ext_type = COMPUTE_IMAGE_TYPE::NONE> using const_image_1d_array =
const_image<COMPUTE_IMAGE_TYPE::IMAGE_1D_ARRAY | ext_type | floor_image::from_sample_type<sample_type>::type()>;

template <typename sample_type, COMPUTE_IMAGE_TYPE ext_type = COMPUTE_IMAGE_TYPE::NONE> using const_image_2d =
const_image<COMPUTE_IMAGE_TYPE::IMAGE_2D | ext_type | floor_image::from_sample_type<sample_type>::type()>;

template <typename sample_type, COMPUTE_IMAGE_TYPE ext_type = COMPUTE_IMAGE_TYPE::NONE> using const_image_2d_array =
const_image<COMPUTE_IMAGE_TYPE::IMAGE_2D_ARRAY | ext_type | floor_image::from_sample_type<sample_type>::type()>;

template <typename sample_type, COMPUTE_IMAGE_TYPE ext_type = COMPUTE_IMAGE_TYPE::NONE> using const_image_2d_msaa =
const_image<COMPUTE_IMAGE_TYPE::IMAGE_2D_MSAA | ext_type | floor_image::from_sample_type<sample_type>::type()>;

template <typename sample_type, COMPUTE_IMAGE_TYPE ext_type = COMPUTE_IMAGE_TYPE::NONE> using const_image_2d_msaa_array =
const_image<COMPUTE_IMAGE_TYPE::IMAGE_2D_MSAA_ARRAY | ext_type | floor_image::from_sample_type<sample_type>::type()>;

template <typename sample_type, COMPUTE_IMAGE_TYPE ext_type = COMPUTE_IMAGE_TYPE::NONE> using const_image_cube =
const_image<COMPUTE_IMAGE_TYPE::IMAGE_CUBE | ext_type | floor_image::from_sample_type<sample_type>::type()>;

template <typename sample_type, COMPUTE_IMAGE_TYPE ext_type = COMPUTE_IMAGE_TYPE::NONE> using const_image_cube_array =
const_image<COMPUTE_IMAGE_TYPE::IMAGE_CUBE_ARRAY | ext_type | floor_image::from_sample_type<sample_type>::type()>;

template <typename sample_type, COMPUTE_IMAGE_TYPE ext_type = COMPUTE_IMAGE_TYPE::NONE> using const_image_2d_depth =
const_image<(COMPUTE_IMAGE_TYPE::IMAGE_DEPTH | ext_type |
			 // always single channel
			 COMPUTE_IMAGE_TYPE::FLAG_FIXED_CHANNELS |
			 (floor_image::from_sample_type<sample_type>::type() & ~COMPUTE_IMAGE_TYPE::__CHANNELS_MASK))>;

template <typename sample_type, COMPUTE_IMAGE_TYPE ext_type = COMPUTE_IMAGE_TYPE::NONE> using const_image_2d_depth_stencil =
const_image<(COMPUTE_IMAGE_TYPE::IMAGE_DEPTH_STENCIL | ext_type |
			 // always 2 channels
			 COMPUTE_IMAGE_TYPE::FLAG_FIXED_CHANNELS |
			 (floor_image::from_sample_type<sample_type>::type() & ~COMPUTE_IMAGE_TYPE::__CHANNELS_MASK))>;

template <typename sample_type, COMPUTE_IMAGE_TYPE ext_type = COMPUTE_IMAGE_TYPE::NONE> using const_image_2d_depth_array =
const_image<(COMPUTE_IMAGE_TYPE::IMAGE_DEPTH_ARRAY | ext_type |
			 // always single channel
			 COMPUTE_IMAGE_TYPE::FLAG_FIXED_CHANNELS |
			 (floor_image::from_sample_type<sample_type>::type() & ~COMPUTE_IMAGE_TYPE::__CHANNELS_MASK))>;

template <typename sample_type, COMPUTE_IMAGE_TYPE ext_type = COMPUTE_IMAGE_TYPE::NONE> using const_image_cube_depth =
const_image<(COMPUTE_IMAGE_TYPE::IMAGE_DEPTH_CUBE | ext_type |
			 // always single channel
			 COMPUTE_IMAGE_TYPE::FLAG_FIXED_CHANNELS |
			 (floor_image::from_sample_type<sample_type>::type() & ~COMPUTE_IMAGE_TYPE::__CHANNELS_MASK))>;

template <typename sample_type, COMPUTE_IMAGE_TYPE ext_type = COMPUTE_IMAGE_TYPE::NONE> using const_image_2d_depth_msaa =
const_image<(COMPUTE_IMAGE_TYPE::IMAGE_DEPTH_MSAA | ext_type |
			 // always single channel
			 COMPUTE_IMAGE_TYPE::FLAG_FIXED_CHANNELS |
			 (floor_image::from_sample_type<sample_type>::type() & ~COMPUTE_IMAGE_TYPE::__CHANNELS_MASK))>;

template <typename sample_type, COMPUTE_IMAGE_TYPE ext_type = COMPUTE_IMAGE_TYPE::NONE> using const_image_2d_depth_msaa_array =
const_image<(COMPUTE_IMAGE_TYPE::IMAGE_DEPTH_MSAA_ARRAY | ext_type |
			 // always single channel
			 COMPUTE_IMAGE_TYPE::FLAG_FIXED_CHANNELS |
			 (floor_image::from_sample_type<sample_type>::type() & ~COMPUTE_IMAGE_TYPE::__CHANNELS_MASK))>;

template <typename sample_type, COMPUTE_IMAGE_TYPE ext_type = COMPUTE_IMAGE_TYPE::NONE> using const_image_3d =
const_image<COMPUTE_IMAGE_TYPE::IMAGE_3D | ext_type | floor_image::from_sample_type<sample_type>::type()>;

// read-write/write-only image types
template <typename sample_type, bool write_only = false, COMPUTE_IMAGE_TYPE ext_type = COMPUTE_IMAGE_TYPE::NONE> using image_1d =
image<COMPUTE_IMAGE_TYPE::IMAGE_1D | ext_type | floor_image::from_sample_type<sample_type>::type(), write_only>;

template <typename sample_type, bool write_only = false, COMPUTE_IMAGE_TYPE ext_type = COMPUTE_IMAGE_TYPE::NONE> using image_1d_array =
image<COMPUTE_IMAGE_TYPE::IMAGE_1D_ARRAY | ext_type | floor_image::from_sample_type<sample_type>::type(), write_only>;

template <typename sample_type, bool write_only = false, COMPUTE_IMAGE_TYPE ext_type = COMPUTE_IMAGE_TYPE::NONE> using image_2d =
image<COMPUTE_IMAGE_TYPE::IMAGE_2D | ext_type | floor_image::from_sample_type<sample_type>::type(), write_only>;

template <typename sample_type, bool write_only = false, COMPUTE_IMAGE_TYPE ext_type = COMPUTE_IMAGE_TYPE::NONE> using image_2d_array =
image<COMPUTE_IMAGE_TYPE::IMAGE_2D_ARRAY | ext_type | floor_image::from_sample_type<sample_type>::type(), write_only>;

// NOTE: not supported by anyone
//template <typename sample_type, bool write_only = false, COMPUTE_IMAGE_TYPE ext_type = COMPUTE_IMAGE_TYPE::NONE> using image_2d_msaa =
//image<COMPUTE_IMAGE_TYPE::IMAGE_2D_MSAA | ext_type | floor_image::from_sample_type<sample_type>::type(), write_only>;

// NOTE: not supported by anyone
//template <typename sample_type, bool write_only = false, COMPUTE_IMAGE_TYPE ext_type = COMPUTE_IMAGE_TYPE::NONE> using image_2d_msaa_array =
//image<COMPUTE_IMAGE_TYPE::IMAGE_2D_MSAA_ARRAY | ext_type | floor_image::from_sample_type<sample_type>::type(), write_only>;

template <typename sample_type, bool write_only = false, COMPUTE_IMAGE_TYPE ext_type = COMPUTE_IMAGE_TYPE::NONE> using image_cube =
image<COMPUTE_IMAGE_TYPE::IMAGE_CUBE | ext_type | floor_image::from_sample_type<sample_type>::type(), write_only>;

template <typename sample_type, bool write_only = false, COMPUTE_IMAGE_TYPE ext_type = COMPUTE_IMAGE_TYPE::NONE> using image_cube_array =
image<COMPUTE_IMAGE_TYPE::IMAGE_CUBE_ARRAY | ext_type | floor_image::from_sample_type<sample_type>::type(), write_only>;

template <typename sample_type, bool write_only = false, COMPUTE_IMAGE_TYPE ext_type = COMPUTE_IMAGE_TYPE::NONE> using image_2d_depth =
image<(COMPUTE_IMAGE_TYPE::IMAGE_DEPTH | ext_type |
			 // always single channel
			 COMPUTE_IMAGE_TYPE::FLAG_FIXED_CHANNELS |
			 (floor_image::from_sample_type<sample_type>::type() & ~COMPUTE_IMAGE_TYPE::__CHANNELS_MASK)), write_only>;

template <typename sample_type, bool write_only = false, COMPUTE_IMAGE_TYPE ext_type = COMPUTE_IMAGE_TYPE::NONE> using image_2d_depth_stencil =
image<(COMPUTE_IMAGE_TYPE::IMAGE_DEPTH_STENCIL | ext_type |
			 // always 2 channels
			 COMPUTE_IMAGE_TYPE::FLAG_FIXED_CHANNELS |
			 (floor_image::from_sample_type<sample_type>::type() & ~COMPUTE_IMAGE_TYPE::__CHANNELS_MASK)), write_only>;

template <typename sample_type, bool write_only = false, COMPUTE_IMAGE_TYPE ext_type = COMPUTE_IMAGE_TYPE::NONE> using image_2d_depth_array =
image<(COMPUTE_IMAGE_TYPE::IMAGE_DEPTH_ARRAY | ext_type |
			 // always single channel
			 COMPUTE_IMAGE_TYPE::FLAG_FIXED_CHANNELS |
			 (floor_image::from_sample_type<sample_type>::type() & ~COMPUTE_IMAGE_TYPE::__CHANNELS_MASK)), write_only>;

template <typename sample_type, bool write_only = false, COMPUTE_IMAGE_TYPE ext_type = COMPUTE_IMAGE_TYPE::NONE> using image_cube_depth =
image<(COMPUTE_IMAGE_TYPE::IMAGE_DEPTH_CUBE | ext_type |
			 // always single channel
			 COMPUTE_IMAGE_TYPE::FLAG_FIXED_CHANNELS |
			 (floor_image::from_sample_type<sample_type>::type() & ~COMPUTE_IMAGE_TYPE::__CHANNELS_MASK)), write_only>;

// NOTE: not supported by anyone
//template <typename sample_type, bool write_only = false, COMPUTE_IMAGE_TYPE ext_type = COMPUTE_IMAGE_TYPE::NONE> using image_2d_depth_msaa =
//image<(COMPUTE_IMAGE_TYPE::IMAGE_DEPTH_MSAA | ext_type |
//			 // always single channel
//			 COMPUTE_IMAGE_TYPE::FLAG_FIXED_CHANNELS |
//			 (floor_image::from_sample_type<sample_type>::type() & ~COMPUTE_IMAGE_TYPE::__CHANNELS_MASK)), write_only>;

// NOTE: not supported by anyone
//template <typename sample_type, bool write_only = false, COMPUTE_IMAGE_TYPE ext_type = COMPUTE_IMAGE_TYPE::NONE> using image_2d_depth_msaa_array =
//image<(COMPUTE_IMAGE_TYPE::IMAGE_DEPTH_MSAA_ARRAY | ext_type |
//			 // always single channel
//			 COMPUTE_IMAGE_TYPE::FLAG_FIXED_CHANNELS |
//			 (floor_image::from_sample_type<sample_type>::type() & ~COMPUTE_IMAGE_TYPE::__CHANNELS_MASK)), write_only>;

template <typename sample_type, bool write_only = false, COMPUTE_IMAGE_TYPE ext_type = COMPUTE_IMAGE_TYPE::NONE> using image_3d =
image<COMPUTE_IMAGE_TYPE::IMAGE_3D | ext_type | floor_image::from_sample_type<sample_type>::type(), write_only>;

#endif
