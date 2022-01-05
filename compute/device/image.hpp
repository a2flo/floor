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

#ifndef __FLOOR_COMPUTE_DEVICE_IMAGE_HPP__
#define __FLOOR_COMPUTE_DEVICE_IMAGE_HPP__

#if defined(FLOOR_COMPUTE_OPENCL) || defined(FLOOR_COMPUTE_METAL) || defined(FLOOR_COMPUTE_VULKAN)
// COMPUTE_IMAGE_TYPE -> opencl/metal image type map
#include <floor/compute/device/opaque_image_map.hpp>
// opencl/metal image read/write functions
#include <floor/compute/device/opaque_image.hpp>
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
													  ext::is_integral_v<typename coord_type::decayed_scalar_type>) ||
													 (ext::is_fundamental_v<coord_type> && ext::is_integral_v<coord_type>))>
	static constexpr bool is_int_coord() {
		return ret_value;
	}
	
	//! get the gradient vector type (dPdx and dPdy) of an image type (sanely default to float2 as this is the case for most formats)
	template <COMPUTE_IMAGE_TYPE image_type>
	struct gradient_vec_type_for_image_type {
		typedef float2 type;
	};
	template <COMPUTE_IMAGE_TYPE image_type>
	requires(has_flag<COMPUTE_IMAGE_TYPE::FLAG_CUBE>(image_type) ||
			 image_dim_count(image_type) == 3)
	struct gradient_vec_type_for_image_type<image_type> {
		typedef float3 type;
	};
	template <COMPUTE_IMAGE_TYPE image_type>
	requires(image_dim_count(image_type) == 1)
	struct gradient_vec_type_for_image_type<image_type> {
		typedef float1 type;
	};
	
	//! get the offset vector type of an image type (sanely default to int2 as this is the case for most formats)
	template <COMPUTE_IMAGE_TYPE image_type>
	struct offset_vec_type_for_image_type {
		typedef int2 type;
	};
	template <COMPUTE_IMAGE_TYPE image_type>
	requires(image_dim_count(image_type) == 3
#if !defined(FLOOR_COMPUTE_HOST)
			 // this is a total hack, but cube map offsets aren't supported with
			 // cuda/metal/opencl and I don't want to add image functions/handling
			 // for something that isn't going to be used anyways
			 // -> use int3 offset instead of the actual int2 offset (b/c symmetry)
			 || has_flag<COMPUTE_IMAGE_TYPE::FLAG_CUBE>(image_type)
#endif
			)
	struct offset_vec_type_for_image_type<image_type> {
		typedef int3 type;
	};
	template <COMPUTE_IMAGE_TYPE image_type>
	requires(image_dim_count(image_type) == 1)
	struct offset_vec_type_for_image_type<image_type> {
		typedef int1 type;
	};
	
#if defined(FLOOR_COMPUTE_OPENCL) || defined(FLOOR_COMPUTE_METAL) || defined(FLOOR_COMPUTE_VULKAN)
#if defined(FLOOR_COMPUTE_METAL)
	constexpr metal_image::sampler::COMPARE_FUNCTION compare_function_floor_to_metal(const COMPARE_FUNCTION& func) {
		switch(func) {
			// Metal has both a "never" and "none" -> map these dependent on the metal version mirroring what the metal compiler is doing
			case COMPARE_FUNCTION::NEVER: return metal_image::sampler::COMPARE_FUNCTION::NEVER;
			case COMPARE_FUNCTION::LESS: return metal_image::sampler::COMPARE_FUNCTION::LESS;
			case COMPARE_FUNCTION::LESS_OR_EQUAL: return metal_image::sampler::COMPARE_FUNCTION::LESS_EQUAL;
			case COMPARE_FUNCTION::GREATER: return metal_image::sampler::COMPARE_FUNCTION::GREATER;
			case COMPARE_FUNCTION::GREATER_OR_EQUAL: return metal_image::sampler::COMPARE_FUNCTION::GREATER_EQUAL;
			case COMPARE_FUNCTION::EQUAL: return metal_image::sampler::COMPARE_FUNCTION::EQUAL;
			case COMPARE_FUNCTION::NOT_EQUAL: return metal_image::sampler::COMPARE_FUNCTION::NOT_EQUAL;
			case COMPARE_FUNCTION::ALWAYS: return metal_image::sampler::COMPARE_FUNCTION::ALWAYS;
		}
		floor_unreachable();
	}
#endif
	
	//! backend specific default sampler (for integral and floating point coordinates)
	template <typename coord_type, bool sample_linear, bool sample_repeat, COMPARE_FUNCTION compare_function = COMPARE_FUNCTION::NEVER>
	struct default_sampler {
		static constexpr auto value() {
#if defined(FLOOR_COMPUTE_OPENCL)
			return ((!sample_repeat ? opencl_image::sampler::ADDRESS_MODE::CLAMP_TO_EDGE : opencl_image::sampler::ADDRESS_MODE::REPEAT) |
					(is_int_coord<coord_type>() ? opencl_image::sampler::COORD_MODE::PIXEL : opencl_image::sampler::COORD_MODE::NORMALIZED) |
					(!sample_linear ? opencl_image::sampler::FILTER_MODE::NEAREST : opencl_image::sampler::FILTER_MODE::LINEAR));
#elif defined(FLOOR_COMPUTE_VULKAN)
			return (vulkan_image::sampler {
				(!sample_linear ? vulkan_image::sampler::NEAREST : vulkan_image::sampler::LINEAR),
				(!sample_repeat ? vulkan_image::sampler::CLAMP_TO_EDGE : vulkan_image::sampler::REPEAT),
				(is_int_coord<coord_type>() ? vulkan_image::sampler::PIXEL : vulkan_image::sampler::NORMALIZED),
				(vulkan_image::sampler::COMPARE_FUNCTION)(uint32_t(compare_function) <<
														  vulkan_image::sampler::__COMPARE_FUNCTION_SHIFT)
			}).value;
#elif defined(FLOOR_COMPUTE_METAL)
			return (metal_sampler_t)(metal_image::sampler {
				(!sample_repeat ? metal_image::sampler::ADDRESS_MODE::CLAMP_TO_EDGE : metal_image::sampler::ADDRESS_MODE::REPEAT),
				(is_int_coord<coord_type>() ? metal_image::sampler::COORD_MODE::PIXEL : metal_image::sampler::COORD_MODE::NORMALIZED),
				(!sample_linear ? metal_image::sampler::FILTER_MODE::NEAREST : metal_image::sampler::FILTER_MODE::LINEAR),
				(!sample_linear ? metal_image::sampler::MIP_FILTER_MODE::MIP_NONE : metal_image::sampler::MIP_FILTER_MODE::MIP_LINEAR),
				compare_function_floor_to_metal(compare_function)
			});
#else
#error "unhandled backend"
#endif
		}
	};
#endif
	
	//! backend specific sampler type
#if defined(FLOOR_COMPUTE_OPENCL)
	typedef sampler_t sampler_type;
#elif defined(FLOOR_COMPUTE_VULKAN)
	typedef decltype(vulkan_image::sampler::value) sampler_type;
#elif defined(FLOOR_COMPUTE_METAL)
	typedef metal_sampler_t sampler_type;
#endif
	
	//! COMPUTE_IMAGE_TYPE -> sample type
	template <COMPUTE_IMAGE_TYPE image_type> struct to_sample_type {};
	template <COMPUTE_IMAGE_TYPE image_type> requires(is_sample_float(image_type)) struct to_sample_type<image_type> {
		typedef float type;
	};
	template <COMPUTE_IMAGE_TYPE image_type> requires(is_sample_int(image_type)) struct to_sample_type<image_type> {
		typedef int32_t type;
	};
	template <COMPUTE_IMAGE_TYPE image_type> requires(is_sample_uint(image_type)) struct to_sample_type<image_type> {
		typedef uint32_t type;
	};
	//! (vector) sample type -> COMPUTE_IMAGE_TYPE
	//! NOTE: scalar sample types will always return the 4-channel variant,
	//!       vector sample types will return the corresponding channel variant
	template <typename sample_type> struct from_sample_type {};
	template <typename sample_type> requires(is_same_v<float, sample_type>) struct from_sample_type<sample_type> {
		static constexpr COMPUTE_IMAGE_TYPE type {
			COMPUTE_IMAGE_TYPE::FLOAT | COMPUTE_IMAGE_TYPE::CHANNELS_4
		};
	};
	template <typename sample_type> requires(is_same_v<int32_t, sample_type>) struct from_sample_type<sample_type> {
		static constexpr COMPUTE_IMAGE_TYPE type {
			COMPUTE_IMAGE_TYPE::INT | COMPUTE_IMAGE_TYPE::CHANNELS_4
		};
	};
	template <typename sample_type> requires(is_same_v<uint32_t, sample_type>) struct from_sample_type<sample_type> {
		static constexpr COMPUTE_IMAGE_TYPE type {
			COMPUTE_IMAGE_TYPE::UINT | COMPUTE_IMAGE_TYPE::CHANNELS_4
		};
	};
	template <typename sample_type> requires(is_floor_vector<sample_type>::value) struct from_sample_type<sample_type> {
		static constexpr COMPUTE_IMAGE_TYPE type {
			// get scalar type, clear out channel count, OR with actual channel count, set FIXED_CHANNELS flag
			((from_sample_type<typename sample_type::this_scalar_type>::type & ~COMPUTE_IMAGE_TYPE::__CHANNELS_MASK) |
			 (COMPUTE_IMAGE_TYPE)(uint32_t(sample_type::dim() - 1u) << uint32_t(COMPUTE_IMAGE_TYPE::__CHANNELS_SHIFT)) |
			 COMPUTE_IMAGE_TYPE::FLAG_FIXED_CHANNELS)
		};
	};

	//! image struct that is used for disabled image members
	struct disabled_image_t {};
	
	//! implementation specific image storage
	template <COMPUTE_IMAGE_TYPE image_type>
	class image {
	public: // image storage
		// helpers
		static constexpr bool is_readable() { return has_flag<COMPUTE_IMAGE_TYPE::READ>(image_type); }
		static constexpr bool is_writable() { return has_flag<COMPUTE_IMAGE_TYPE::WRITE>(image_type); }
		static constexpr bool is_read_only() { return is_readable() && !is_writable(); }
		static constexpr bool is_write_only() { return !is_readable() && is_writable(); }
		static constexpr bool is_read_write() { return is_readable() && is_writable(); }
		static constexpr bool has_read_write_support() { return FLOOR_COMPUTE_INFO_HAS_IMAGE_READ_WRITE_SUPPORT; }
		static constexpr bool is_array() { return has_flag<COMPUTE_IMAGE_TYPE::FLAG_ARRAY>(image_type); }
		static constexpr uint32_t channel_count() { return image_channel_count(image_type); }
		static constexpr COMPUTE_IMAGE_TYPE type { image_type };
		
		using sample_type = typename to_sample_type<image_type>::type;
		using vector_sample_type = conditional_t<channel_count() == 1, sample_type, vector_n<sample_type, channel_count()>>;
		using offset_vec_type = typename offset_vec_type_for_image_type<image_type>::type;
		using gradient_vec_type = typename gradient_vec_type_for_image_type<image_type>::type;
		
		// actual image storage/types
#if defined(FLOOR_COMPUTE_OPENCL) || defined(FLOOR_COMPUTE_METAL) || defined(FLOOR_COMPUTE_VULKAN)
		// OpenCL/Metal/Vulkan have/store different image types dependent on access mode
		using opaque_type = typename opaque_image_type<image_type>::type;
		
#if defined(FLOOR_COMPUTE_OPENCL) || defined(FLOOR_COMPUTE_METAL)
		using primary_image_type = opaque_type;
		using secondary_image_type = conditional_t<is_read_write() && !has_read_write_support(), opaque_type, disabled_image_t>;
#elif defined(FLOOR_COMPUTE_VULKAN)
		// Vulkan needs to deal with selecting between a simple image (readable) and a pointer to an image array (writable)
		static_assert(!has_read_write_support(), "Vulkan has no native read+write support");
		using writeable_image_array_type = opaque_type[FLOOR_COMPUTE_INFO_MAX_MIP_LEVELS];
		using writeable_image_type = writeable_image_array_type*;
		using primary_image_type = conditional_t<is_readable(), opaque_type, writeable_image_type>;
		using secondary_image_type = conditional_t<is_read_write(), writeable_image_type, disabled_image_t>;
#endif
		
		// primary image: can be read-only or write-only, or read+write if supported by backend
		static constexpr const COMPUTE_IMAGE_TYPE primary_image_flags = ((image_type & ~COMPUTE_IMAGE_TYPE::__ACCESS_MASK) |
																		 (is_read_write() && has_read_write_support() ? COMPUTE_IMAGE_TYPE::READ_WRITE :
																		  (is_readable() ? COMPUTE_IMAGE_TYPE::READ : COMPUTE_IMAGE_TYPE::WRITE)));
		
		// secondary image: can only be write-only if image is read+write and backend has no read+write support
		static constexpr const COMPUTE_IMAGE_TYPE secondary_image_flags = ((image_type & ~COMPUTE_IMAGE_TYPE::__ACCESS_MASK) |
																		   (is_read_write() && !has_read_write_support() ? COMPUTE_IMAGE_TYPE::WRITE : COMPUTE_IMAGE_TYPE::NONE));
		
		[[floor::image_data_type(sample_type), floor::image_flags(primary_image_flags)]] primary_image_type primary_img_obj;
		[[no_unique_address, floor::image_data_type(sample_type), floor::image_flags(secondary_image_flags)]] secondary_image_type secondary_img_obj;
		
#elif defined(FLOOR_COMPUTE_CUDA)
		// readable and writable images always exist, regardless of access mode
		const uint32_t r_img_obj[cuda_sampler::max_sampler_count];
		uint64_t w_img_obj;
		uint64_t* w_img_lod_obj;
		const COMPUTE_IMAGE_TYPE runtime_image_type;
#elif defined(FLOOR_COMPUTE_HOST)
		// always the same image object, regardless of access mode
		host_device_image<image_type>* img_obj;
#endif
		
		// image accessors
		constexpr auto& r_img() const requires(is_readable()) {
#if defined(FLOOR_COMPUTE_OPENCL) || defined(FLOOR_COMPUTE_METAL) || defined(FLOOR_COMPUTE_VULKAN)
			return primary_img_obj;
#elif defined(FLOOR_COMPUTE_CUDA)
			return r_img_obj;
#elif defined(FLOOR_COMPUTE_HOST)
			return img_obj;
#endif
		}
		constexpr auto& w_img(const uint32_t lod [[maybe_unused]] = 0) const requires(is_writable()) {
#if defined(FLOOR_COMPUTE_OPENCL) || defined(FLOOR_COMPUTE_METAL)
			if constexpr (has_read_write_support() || !is_readable()) {
				return primary_img_obj;
			} else {
				return secondary_img_obj;
			}
#elif defined(FLOOR_COMPUTE_VULKAN)
			if constexpr (!is_readable()) {
				return (*primary_img_obj)[lod];
			} else {
				return (*secondary_img_obj)[lod];
			}
#elif defined(FLOOR_COMPUTE_CUDA)
			return w_img_obj;
#elif defined(FLOOR_COMPUTE_HOST)
			return img_obj;
#endif
		}
		
	public: // additional image helper functions
#if !defined(FLOOR_COMPUTE_HOST) // opencl/metal/cuda coordinate conversion
		//! convert any coordinate vector type to int* or float* clang vector types
		template <typename coord_type>
		static auto convert_coord(const coord_type& coord) {
			return (vector_n<conditional_t<ext::is_integral_v<typename coord_type::decayed_scalar_type>, int, float>, coord_type::dim()> {
				coord
			}).to_clang_vector();
		}
		
		//! convert any fundamental (single value) coordinate type to int or float
		template <typename coord_type>
		requires(ext::is_fundamental_v<coord_type>)
		static auto convert_coord(const coord_type& coord) {
			return conditional_t<ext::is_integral_v<coord_type>, int, float> { coord };
		}
#else // host-compute
		//! convert any coordinate vector type to floor int{1,2,3,4} or float{1,2,3,4} vectors
		template <typename coord_type>
		static auto convert_coord(const coord_type& coord) {
			if constexpr(!ext::is_fundamental_v<coord_type>) {
				return vector_n<conditional_t<ext::is_integral_v<typename coord_type::decayed_scalar_type>, int, float>, coord_type::dim()> { coord };
			}
			else {
				return vector_n<conditional_t<ext::is_integral_v<coord_type>, int, float>, 1> { coord };
			}
		}
#endif
		
		//! converts any fundamental (single value) type to a vector4 type (which can then be converted to a corresponding clang_*4 type)
		template <typename expected_scalar_type, typename data_type>
		requires(ext::is_fundamental_v<data_type>)
		static auto convert_data(const data_type& data) {
			using scalar_type = data_type;
			static_assert(is_same_v<scalar_type, expected_scalar_type>, "invalid data type");
			return vector_n<scalar_type, 4> { data, (scalar_type)0, (scalar_type)0, (scalar_type)0 };
		}
		
		//! converts any vector* type to a vector4 type (which can then be converted to a corresponding clang_*4 type)
		template <typename expected_scalar_type, typename data_type>
		requires(!ext::is_fundamental_v<data_type>)
		static auto convert_data(const data_type& data) {
			using scalar_type = typename data_type::decayed_scalar_type;
			static_assert(is_same_v<scalar_type, expected_scalar_type>, "invalid data type");
			return vector_n<scalar_type, 4> { data };
		}
		
	public: // image query functions
		//! queries the image dimension at run-time, returning it in the same format as compute_image::image_dim
		uint4 dim(const uint32_t lod = 0) const {
#if defined(FLOOR_COMPUTE_OPENCL) || defined(FLOOR_COMPUTE_METAL) || defined(FLOOR_COMPUTE_VULKAN)
			if constexpr (is_readable()) {
				return uint4::from_clang_vector(opaque_image::get_image_dim(r_img(), image_type, lod));
			} else {
				return uint4::from_clang_vector(opaque_image::get_image_dim(w_img(), image_type, lod));
			}
#elif defined(FLOOR_COMPUTE_CUDA)
			if constexpr (is_readable()) {
				return uint4::from_clang_vector(cuda_image::get_image_dim(r_img()[0], image_type, lod));
			} else {
				return uint4::from_clang_vector(cuda_image::get_image_dim(w_img(), image_type, lod));
			}
#elif defined(FLOOR_COMPUTE_HOST)
			if constexpr (is_readable()) {
				return r_img()->level_info[lod].dim;
			} else {
				return w_img()->level_info[lod].dim;
			}
#else
#error "unknown backend"
#endif
		}
		
	public: // image read functions
		//! internal read function, handling all kinds of reads
		//! NOTE: while this is an internal function, it might be useful for anyone insane enough to use it directly on the outside
		//!       -> this is a public function and not protected
		template <bool sample_linear, bool sample_repeat = false, bool is_lod = false, bool is_gradient = false,
				  bool is_compare = false, COMPARE_FUNCTION compare_function = COMPARE_FUNCTION::NEVER,
				  typename coord_type, typename lod_type = int32_t>
		requires (is_readable())
		floor_inline_always auto read_internal(const coord_type& coord,
											   const uint32_t layer,
#if defined(FLOOR_COMPUTE_HOST) // NOTE: MSAA/sample is not supported on host-compute
											   const uint32_t sample floor_unused,
#else
											   const uint32_t sample,
#endif
											   const offset_vec_type offset,
											   const float bias = 0.0f,
											   const lod_type lod = (lod_type)0,
#if defined(FLOOR_COMPUTE_HOST) // NOTE: read with an explicit gradient is currently not supported on host-compute
											   const pair<gradient_vec_type, gradient_vec_type> gradient floor_unused = {},
#else
											   const pair<gradient_vec_type, gradient_vec_type> gradient = {},
#endif
											   const float compare_value = 0.0f
		) const {
			// sample type must be 32-bit float, int or uint
			constexpr const bool is_float = is_sample_float(image_type);
			constexpr const bool is_int = is_sample_int(image_type);
			static_assert(is_float || is_int || is_sample_uint(image_type), "invalid sample type");
			
			// explicit lod and gradient are mutually exclusive
			static_assert(!(is_lod && is_gradient), "can't use both lod and gradient");
			
			// lod type must be 32-bit float, int or uint (uint will be casted to int)
			typedef decay_t<lod_type> decayed_lod_type;
			constexpr const bool is_lod_float = is_same_v<decayed_lod_type, float>;
			static_assert(is_same_v<decayed_lod_type, int32_t> ||
						  is_same_v<decayed_lod_type, uint32_t> ||
						  is_same_v<decayed_lod_type, float>, "lod type must be float, int32_t or uint32_t");
			
			// explicit lod or gradient read is not possible with msaa images
			static_assert((!is_lod && !is_gradient) ||
						  ((is_lod || is_gradient) && !has_flag<COMPUTE_IMAGE_TYPE::FLAG_MSAA>(image_type)),
						  "image type does not support mip-maps");
			
			// if not explicit lod or gradient, then always use bias (neither lod nor gradient have a bias option)
			constexpr const bool is_bias = (!is_lod && !is_gradient);
			
			// depth compare read is only allowed for depth images
			static_assert((is_compare && has_flag<COMPUTE_IMAGE_TYPE::FLAG_DEPTH>(image_type)) || !is_compare,
						  "compare is only allowed with depth images");
			
			// backend specific coordinate conversion (also: any input -> float or int)
			const auto converted_coord = convert_coord(coord);
			
			const auto fit_output = [](const auto& color) {
				typedef image_vec_ret_type<image_type, sample_type> output_type;
#if defined(FLOOR_COMPUTE_OPENCL) || defined(FLOOR_COMPUTE_METAL) || defined(FLOOR_COMPUTE_CUDA) || defined(FLOOR_COMPUTE_VULKAN)
				if constexpr(!has_flag<COMPUTE_IMAGE_TYPE::FLAG_DEPTH>(image_type)) {
					return output_type::fit(vector_n<sample_type, 4>::from_clang_vector(color));
				} else {
					return output_type::fit(color.x);
				}
#else
				return output_type::fit(color);
#endif
			};
#if defined(FLOOR_COMPUTE_OPENCL) || defined(FLOOR_COMPUTE_METAL) || defined(FLOOR_COMPUTE_VULKAN)
#if !defined(FLOOR_COMPUTE_METAL) // only constexpr with opencl/vulkan
			constexpr
#endif
			const sampler_type smplr = default_sampler<coord_type, sample_linear, sample_repeat, compare_function>::value();
			if constexpr(is_float) {
				return fit_output(opaque_image::read_image_float(r_img(), smplr, image_type, converted_coord, layer, sample, offset,
																 (!is_lod_float ? int32_t(lod) : 0), (!is_bias ? (is_lod_float ? lod : 0.0f) : bias), is_lod, is_lod_float, is_bias,
																 gradient.first, gradient.second, is_gradient,
																 compare_function, compare_value, is_compare));
			} else if constexpr(is_int) {
				return fit_output(opaque_image::read_image_int(r_img(), smplr, image_type, converted_coord, layer, sample, offset,
															   (!is_lod_float ? int32_t(lod) : 0), (!is_bias ? (is_lod_float ? lod : 0.0f) : bias), is_lod, is_lod_float, is_bias,
															   gradient.first, gradient.second, is_gradient,
															   compare_function, compare_value, is_compare));
			} else {
				return fit_output(opaque_image::read_image_uint(r_img(), smplr, image_type, converted_coord, layer, sample, offset,
																(!is_lod_float ? int32_t(lod) : 0), (!is_bias ? (is_lod_float ? lod : 0.0f) : bias), is_lod, is_lod_float, is_bias,
																gradient.first, gradient.second, is_gradient,
																compare_function, compare_value, is_compare));
			}
#elif defined(FLOOR_COMPUTE_CUDA)
			constexpr const auto cuda_tex_idx = cuda_sampler::sampler_index(is_int_coord<coord_type>() ? cuda_sampler::PIXEL : cuda_sampler::NORMALIZED,
																			sample_linear ? cuda_sampler::LINEAR : cuda_sampler::NEAREST,
																			sample_repeat ? cuda_sampler::REPEAT : cuda_sampler::CLAMP_TO_EDGE,
																			(!is_compare ||
																			 compare_function == COMPARE_FUNCTION::ALWAYS ||
																			 compare_function == COMPARE_FUNCTION::NEVER) ?
																			cuda_sampler::NONE : (cuda_sampler::COMPARE_FUNCTION)compare_function);
			if constexpr(is_float) {
				return fit_output(cuda_image::read_image_float(r_img_obj[cuda_tex_idx], image_type, converted_coord, layer, sample, offset,
															   (!is_lod_float ? int32_t(lod) : 0), (!is_bias ? (is_lod_float ? lod : 0.0f) : bias), is_lod, is_lod_float, is_bias,
															   gradient.first, gradient.second, is_gradient,
															   compare_function, compare_value, is_compare));
			} else if constexpr(is_int) {
				return fit_output(cuda_image::read_image_int(r_img_obj[cuda_tex_idx], image_type, converted_coord, layer, sample, offset,
															 (!is_lod_float ? int32_t(lod) : 0), (!is_bias ? (is_lod_float ? lod : 0.0f) : bias), is_lod, is_lod_float, is_bias,
															 gradient.first, gradient.second, is_gradient,
															 compare_function, compare_value, is_compare));
			} else {
				return fit_output(cuda_image::read_image_uint(r_img_obj[cuda_tex_idx], image_type, converted_coord, layer, sample, offset,
															  (!is_lod_float ? int32_t(lod) : 0), (!is_bias ? (is_lod_float ? lod : 0.0f) : bias), is_lod, is_lod_float, is_bias,
															  gradient.first, gradient.second, is_gradient,
															  compare_function, compare_value, is_compare));
			}
#elif defined(FLOOR_COMPUTE_HOST)
			if constexpr(!is_compare) {
				if constexpr(!sample_linear) {
					return fit_output(host_device_image<image_type, is_lod, is_lod_float, is_bias>::read((const host_device_image<image_type, is_lod, is_lod_float, is_bias>*)r_img(),
																										 converted_coord, offset, layer,
																										 (!is_lod_float ? int32_t(lod) : 0),
																										 (!is_bias ? (is_lod_float ? float(lod) : 0.0f) : bias)));
				} else {
					return fit_output(host_device_image<image_type, is_lod, is_lod_float, is_bias>::read_linear((const host_device_image<image_type, is_lod, is_lod_float, is_bias>*)r_img(),
																												converted_coord, offset, layer,
																												(!is_lod_float ? int32_t(lod) : 0),
																												(!is_bias ? (is_lod_float ? float(lod) : 0.0f) : bias)));
				}
			} else {
				if constexpr(!sample_linear) {
					return fit_output(host_device_image<image_type, is_lod, is_lod_float, is_bias>::compare((const host_device_image<image_type, is_lod, is_lod_float, is_bias>*)r_img(),
																											converted_coord, offset, layer,
																											(!is_lod_float ? int32_t(lod) : 0),
																											(!is_bias ? (is_lod_float ? float(lod) : 0.0f) : bias),
																											compare_function, compare_value));
				} else {
					return fit_output(host_device_image<image_type, is_lod, is_lod_float, is_bias>::compare_linear((const host_device_image<image_type, is_lod, is_lod_float, is_bias>*)r_img(),
																														 converted_coord, offset, layer,
																														 (!is_lod_float ? int32_t(lod) : 0),
																														 (!is_bias ? (is_lod_float ? float(lod) : 0.0f) : bias),
																														 compare_function, compare_value));
				}
			}
#endif
		}
		
		//! image read with nearest/point sampling and clamp-to-edge address mode (non-array, non-msaa)
		template <typename coord_type>
		requires(!has_flag<COMPUTE_IMAGE_TYPE::FLAG_ARRAY>(image_type) &&
				 !has_flag<COMPUTE_IMAGE_TYPE::FLAG_MSAA>(image_type))
		auto read(const coord_type& coord, const offset_vec_type offset = {}, const float bias = 0.0f) const {
			return read_internal<false>(coord, 0, 0, offset, bias);
		}
		
		//! image read with nearest/point sampling and clamp-to-edge address mode (array, non-msaa)
		template <typename coord_type>
		requires(has_flag<COMPUTE_IMAGE_TYPE::FLAG_ARRAY>(image_type) &&
				 !has_flag<COMPUTE_IMAGE_TYPE::FLAG_MSAA>(image_type))
		auto read(const coord_type& coord, const uint32_t layer, const offset_vec_type offset = {}, const float bias = 0.0f) const {
			return read_internal<false>(coord, layer, 0, offset, bias);
		}
		
		//! image read with nearest/point sampling and clamp-to-edge address mode (non-array, msaa)
		template <typename coord_type>
		requires(!has_flag<COMPUTE_IMAGE_TYPE::FLAG_ARRAY>(image_type) &&
				 has_flag<COMPUTE_IMAGE_TYPE::FLAG_MSAA>(image_type))
		auto read(const coord_type& coord, const uint32_t sample, const offset_vec_type offset = {}, const float bias = 0.0f) const {
			return read_internal<false>(coord, 0, sample, offset, bias);
		}
		
		//! image read with nearest/point sampling and clamp-to-edge address mode (array, msaa)
		template <typename coord_type>
		requires(has_flag<COMPUTE_IMAGE_TYPE::FLAG_ARRAY>(image_type) &&
				 has_flag<COMPUTE_IMAGE_TYPE::FLAG_MSAA>(image_type))
		auto read(const coord_type& coord, const uint32_t layer, const uint32_t sample, const offset_vec_type offset = {}, const float bias = 0.0f) const {
			return read_internal<false>(coord, layer, sample, offset, bias);
		}
		
		//! image read with nearest/point sampling and repeat address mode (non-array, non-msaa)
		template <typename coord_type>
		requires(!has_flag<COMPUTE_IMAGE_TYPE::FLAG_ARRAY>(image_type) &&
				 !has_flag<COMPUTE_IMAGE_TYPE::FLAG_MSAA>(image_type))
		auto read_repeat(const coord_type& coord, const offset_vec_type offset = {}, const float bias = 0.0f) const {
			return read_internal<false, true>(coord, 0, 0, offset, bias);
		}
		
		//! image read with nearest/point sampling and repeat address mode (array, non-msaa)
		template <typename coord_type>
		requires(has_flag<COMPUTE_IMAGE_TYPE::FLAG_ARRAY>(image_type) &&
				 !has_flag<COMPUTE_IMAGE_TYPE::FLAG_MSAA>(image_type))
		auto read_repeat(const coord_type& coord, const uint32_t layer, const offset_vec_type offset = {}, const float bias = 0.0f) const {
			return read_internal<false, true>(coord, layer, 0, offset, bias);
		}
		
		//! image read with nearest/point sampling and repeat address mode (non-array, msaa)
		template <typename coord_type>
		requires(!has_flag<COMPUTE_IMAGE_TYPE::FLAG_ARRAY>(image_type) &&
				 has_flag<COMPUTE_IMAGE_TYPE::FLAG_MSAA>(image_type))
		auto read_repeat(const coord_type& coord, const uint32_t sample, const offset_vec_type offset = {}, const float bias = 0.0f) const {
			return read_internal<false, true>(coord, 0, sample, offset, bias);
		}
		
		//! image read with nearest/point sampling and repeat address mode (array, msaa)
		template <typename coord_type>
		requires(has_flag<COMPUTE_IMAGE_TYPE::FLAG_ARRAY>(image_type) &&
				 has_flag<COMPUTE_IMAGE_TYPE::FLAG_MSAA>(image_type))
		auto read_repeat(const coord_type& coord, const uint32_t layer, const uint32_t sample, const offset_vec_type offset = {}, const float bias = 0.0f) const {
			return read_internal<false, true>(coord, layer, sample, offset, bias);
		}
		
		//! image read with linear sampling and clamp-to-edge address mode (non-array, non-msaa)
		template <typename coord_type>
		requires(!has_flag<COMPUTE_IMAGE_TYPE::FLAG_ARRAY>(image_type) &&
				 !has_flag<COMPUTE_IMAGE_TYPE::FLAG_MSAA>(image_type))
		auto read_linear(const coord_type& coord, const offset_vec_type offset = {}, const float bias = 0.0f) const {
			return read_internal<true>(coord, 0, 0, offset, bias);
		}
		
		//! image read with linear sampling and clamp-to-edge address mode (array, non-msaa)
		template <typename coord_type>
		requires(has_flag<COMPUTE_IMAGE_TYPE::FLAG_ARRAY>(image_type) &&
				 !has_flag<COMPUTE_IMAGE_TYPE::FLAG_MSAA>(image_type))
		auto read_linear(const coord_type& coord, const uint32_t layer, const offset_vec_type offset = {}, const float bias = 0.0f) const {
			return read_internal<true>(coord, layer, 0, offset, bias);
		}
		
		//! image read with linear sampling and clamp-to-edge address mode (non-array, msaa)
		template <typename coord_type>
		requires(!has_flag<COMPUTE_IMAGE_TYPE::FLAG_ARRAY>(image_type) &&
				 has_flag<COMPUTE_IMAGE_TYPE::FLAG_MSAA>(image_type))
		auto read_linear(const coord_type& coord, const uint32_t sample, const offset_vec_type offset = {}, const float bias = 0.0f) const {
			return read_internal<true>(coord, 0, sample, offset, bias);
		}
		
		//! image read with linear sampling and clamp-to-edge address mode (array, msaa)
		template <typename coord_type>
		requires(has_flag<COMPUTE_IMAGE_TYPE::FLAG_ARRAY>(image_type) &&
				 has_flag<COMPUTE_IMAGE_TYPE::FLAG_MSAA>(image_type))
		auto read_linear(const coord_type& coord, const uint32_t layer, const uint32_t sample, const offset_vec_type offset = {}, const float bias = 0.0f) const {
			return read_internal<true>(coord, layer, sample, offset, bias);
		}
		
		//! image read with linear sampling and repeat address mode (non-array, non-msaa)
		template <typename coord_type>
		requires(!has_flag<COMPUTE_IMAGE_TYPE::FLAG_ARRAY>(image_type) &&
				 !has_flag<COMPUTE_IMAGE_TYPE::FLAG_MSAA>(image_type))
		auto read_linear_repeat(const coord_type& coord, const offset_vec_type offset = {}, const float bias = 0.0f) const {
			return read_internal<true, true>(coord, 0, 0, offset, bias);
		}
		
		//! image read with linear sampling and repeat address mode (array, non-msaa)
		template <typename coord_type>
		requires(has_flag<COMPUTE_IMAGE_TYPE::FLAG_ARRAY>(image_type) &&
				 !has_flag<COMPUTE_IMAGE_TYPE::FLAG_MSAA>(image_type))
		auto read_linear_repeat(const coord_type& coord, const uint32_t layer, const offset_vec_type offset = {}, const float bias = 0.0f) const {
			return read_internal<true, true>(coord, layer, 0, offset, bias);
		}
		
		//! image read with linear sampling and repeat address mode (non-array, msaa)
		template <typename coord_type>
		requires(!has_flag<COMPUTE_IMAGE_TYPE::FLAG_ARRAY>(image_type) &&
				 has_flag<COMPUTE_IMAGE_TYPE::FLAG_MSAA>(image_type))
		auto read_linear_repeat(const coord_type& coord, const uint32_t sample, const offset_vec_type offset = {}, const float bias = 0.0f) const {
			return read_internal<true, true>(coord, 0, sample, offset, bias);
		}
		
		//! image read with linear sampling and repeat address mode (array, msaa)
		template <typename coord_type>
		requires(has_flag<COMPUTE_IMAGE_TYPE::FLAG_ARRAY>(image_type) &&
				 has_flag<COMPUTE_IMAGE_TYPE::FLAG_MSAA>(image_type))
		auto read_linear_repeat(const coord_type& coord, const uint32_t layer, const uint32_t sample, const offset_vec_type offset = {}, const float bias = 0.0f) const {
			return read_internal<true, true>(coord, layer, sample, offset, bias);
		}
		
		//! image read at an explicit lod level with nearest/point sampling and clamp-to-edge address mode (non-array)
		template <typename coord_type, typename lod_type>
		requires(!has_flag<COMPUTE_IMAGE_TYPE::FLAG_ARRAY>(image_type) &&
				 !has_flag<COMPUTE_IMAGE_TYPE::FLAG_MSAA>(image_type))
		auto read_lod(const coord_type& coord, const lod_type lod, const offset_vec_type offset = {}) const {
			return read_internal<false, false, true>(coord, 0, 0, offset, 0.0f, lod);
		}
		
		//! image read at an explicit lod level with nearest/point sampling and clamp-to-edge address mode (array)
		template <typename coord_type, typename lod_type>
		requires(has_flag<COMPUTE_IMAGE_TYPE::FLAG_ARRAY>(image_type) &&
				 !has_flag<COMPUTE_IMAGE_TYPE::FLAG_MSAA>(image_type))
		auto read_lod(const coord_type& coord, const uint32_t layer, const lod_type lod, const offset_vec_type offset = {}) const {
			return read_internal<false, false, true>(coord, layer, 0, offset, 0.0f, lod);
		}
		
		//! image read at an explicit lod level with nearest/point sampling and repeat address mode (non-array)
		template <typename coord_type, typename lod_type>
		requires(!has_flag<COMPUTE_IMAGE_TYPE::FLAG_ARRAY>(image_type) &&
				 !has_flag<COMPUTE_IMAGE_TYPE::FLAG_MSAA>(image_type))
		auto read_lod_repeat(const coord_type& coord, const lod_type lod, const offset_vec_type offset = {}) const {
			return read_internal<false, true, true>(coord, 0, 0, offset, 0.0f, lod);
		}
		
		//! image read at an explicit lod level with nearest/point sampling and repeat address mode (array)
		template <typename coord_type, typename lod_type>
		requires(has_flag<COMPUTE_IMAGE_TYPE::FLAG_ARRAY>(image_type) &&
				 !has_flag<COMPUTE_IMAGE_TYPE::FLAG_MSAA>(image_type))
		auto read_lod_repeat(const coord_type& coord, const uint32_t layer, const lod_type lod, const offset_vec_type offset = {}) const {
			return read_internal<false, true, true>(coord, layer, 0, offset, 0.0f, lod);
		}
		
		//! image read at an explicit lod level with linear sampling and clamp-to-edge address mode (non-array)
		template <typename coord_type, typename lod_type>
		requires(!has_flag<COMPUTE_IMAGE_TYPE::FLAG_ARRAY>(image_type) &&
				 !has_flag<COMPUTE_IMAGE_TYPE::FLAG_MSAA>(image_type))
		auto read_lod_linear(const coord_type& coord, const lod_type lod, const offset_vec_type offset = {}) const {
			return read_internal<true, false, true>(coord, 0, 0, offset, 0.0f, lod);
		}
		
		//! image read at an explicit lod level with linear sampling and clamp-to-edge address mode (array)
		template <typename coord_type, typename lod_type>
		requires(has_flag<COMPUTE_IMAGE_TYPE::FLAG_ARRAY>(image_type) &&
				 !has_flag<COMPUTE_IMAGE_TYPE::FLAG_MSAA>(image_type))
		auto read_lod_linear(const coord_type& coord, const uint32_t layer, const lod_type lod, const offset_vec_type offset = {}) const {
			return read_internal<true, false, true>(coord, layer, 0, offset, 0.0f, lod);
		}
		
		//! image read at an explicit lod level with linear sampling and repeat address mode (non-array)
		template <typename coord_type, typename lod_type>
		requires(!has_flag<COMPUTE_IMAGE_TYPE::FLAG_ARRAY>(image_type) &&
				 !has_flag<COMPUTE_IMAGE_TYPE::FLAG_MSAA>(image_type))
		auto read_lod_linear_repeat(const coord_type& coord, const lod_type lod, const offset_vec_type offset = {}) const {
			return read_internal<true, true, true>(coord, 0, 0, offset, 0.0f, lod);
		}
		
		//! image read at an explicit lod level with linear sampling and repeat address mode (array)
		template <typename coord_type, typename lod_type>
		requires(has_flag<COMPUTE_IMAGE_TYPE::FLAG_ARRAY>(image_type) &&
				 !has_flag<COMPUTE_IMAGE_TYPE::FLAG_MSAA>(image_type))
		auto read_lod_linear_repeat(const coord_type& coord, const uint32_t layer, const lod_type lod, const offset_vec_type offset = {}) const {
			return read_internal<true, true, true>(coord, layer, 0, offset, 0.0f, lod);
		}
		
		//! image read with an explicit gradient (dPdx, dPdy) with nearest/point sampling and clamp-to-edge address mode (non-array)
		template <typename coord_type>
		requires(!has_flag<COMPUTE_IMAGE_TYPE::FLAG_ARRAY>(image_type) &&
				 !has_flag<COMPUTE_IMAGE_TYPE::FLAG_MSAA>(image_type))
		auto read_gradient(const coord_type& coord,
						   const pair<gradient_vec_type, gradient_vec_type> gradient,
						   const offset_vec_type offset = {}) const {
			return read_internal<false, false, false, true>(coord, 0, 0, offset, 0.0f, 0, gradient);
		}
		
		//! image read with an explicit gradient (dPdx, dPdy) with nearest/point sampling and clamp-to-edge address mode (array)
		template <typename coord_type>
		requires(has_flag<COMPUTE_IMAGE_TYPE::FLAG_ARRAY>(image_type) &&
				 !has_flag<COMPUTE_IMAGE_TYPE::FLAG_MSAA>(image_type))
		auto read_gradient(const coord_type& coord, const uint32_t layer,
						   const pair<gradient_vec_type, gradient_vec_type> gradient,
						   const offset_vec_type offset = {}) const {
			return read_internal<false, false, false, true>(coord, layer, 0, offset, 0.0f, 0, gradient);
		}
		
		//! image read with an explicit gradient (dPdx, dPdy) with nearest/point sampling and repeat address mode (non-array)
		template <typename coord_type>
		requires(!has_flag<COMPUTE_IMAGE_TYPE::FLAG_ARRAY>(image_type) &&
				 !has_flag<COMPUTE_IMAGE_TYPE::FLAG_MSAA>(image_type))
		auto read_gradient_repeat(const coord_type& coord,
								  const pair<gradient_vec_type, gradient_vec_type> gradient,
								  const offset_vec_type offset = {}) const {
			return read_internal<false, true, false, true>(coord, 0, 0, offset, 0.0f, 0, gradient);
		}
		
		//! image read with an explicit gradient (dPdx, dPdy) with nearest/point sampling and repeat address mode (array)
		template <typename coord_type>
		requires(has_flag<COMPUTE_IMAGE_TYPE::FLAG_ARRAY>(image_type) &&
				 !has_flag<COMPUTE_IMAGE_TYPE::FLAG_MSAA>(image_type))
		auto read_gradient_repeat(const coord_type& coord, const uint32_t layer,
								  const pair<gradient_vec_type, gradient_vec_type> gradient,
								  const offset_vec_type offset = {}) const {
			return read_internal<false, true, false, true>(coord, layer, 0, offset, 0.0f, 0, gradient);
		}
		
		//! image read with an explicit gradient (dPdx, dPdy) with linear sampling and clamp-to-edge address mode (non-array)
		template <typename coord_type>
		requires(!has_flag<COMPUTE_IMAGE_TYPE::FLAG_ARRAY>(image_type) &&
				 !has_flag<COMPUTE_IMAGE_TYPE::FLAG_MSAA>(image_type))
		auto read_gradient_linear(const coord_type& coord,
								  const pair<gradient_vec_type, gradient_vec_type> gradient,
								  const offset_vec_type offset = {}) const {
			return read_internal<true, false, false, true>(coord, 0, 0, offset, 0.0f, 0, gradient);
		}
		
		//! image read with an explicit gradient (dPdx, dPdy) with linear sampling and clamp-to-edge address mode (array)
		template <typename coord_type>
		requires(has_flag<COMPUTE_IMAGE_TYPE::FLAG_ARRAY>(image_type) &&
				 !has_flag<COMPUTE_IMAGE_TYPE::FLAG_MSAA>(image_type))
		auto read_gradient_linear(const coord_type& coord, const uint32_t layer,
								  const pair<gradient_vec_type, gradient_vec_type> gradient,
								  const offset_vec_type offset = {}) const {
			return read_internal<true, false, false, true>(coord, layer, 0, offset, 0.0f, 0, gradient);
		}
		
		//! image read with an explicit gradient (dPdx, dPdy) with linear sampling and repeat address mode (non-array)
		template <typename coord_type>
		requires(!has_flag<COMPUTE_IMAGE_TYPE::FLAG_ARRAY>(image_type) &&
				 !has_flag<COMPUTE_IMAGE_TYPE::FLAG_MSAA>(image_type))
		auto read_gradient_linear_repeat(const coord_type& coord,
										 const pair<gradient_vec_type, gradient_vec_type> gradient,
										 const offset_vec_type offset = {}) const {
			return read_internal<true, true, false, true>(coord, 0, 0, offset, 0.0f, 0, gradient);
		}
		
		//! image read with an explicit gradient (dPdx, dPdy) with linear sampling and repeat address mode (array)
		template <typename coord_type>
		requires(has_flag<COMPUTE_IMAGE_TYPE::FLAG_ARRAY>(image_type) &&
				 !has_flag<COMPUTE_IMAGE_TYPE::FLAG_MSAA>(image_type))
		auto read_gradient_linear_repeat(const coord_type& coord, const uint32_t layer,
										 const pair<gradient_vec_type, gradient_vec_type> gradient,
										 const offset_vec_type offset = {}) const {
			return read_internal<true, true, false, true>(coord, layer, 0, offset, 0.0f, 0, gradient);
		}
		
		//////////////////////////////////////////
		// depth compare functions:
		// * metal: has full support for this
		// * host-compute: has full support for this
		// * vulkan: has full support for this
		// * cuda: technically supports depth compare ptx instructions, but no way to set the compare function?! (using s/w compare for now)
		// * opencl/spir: doesn't have support for this, compare will be performed in s/w
		
		//! image depth compare read with nearest/point sampling (non-array)
		template <COMPARE_FUNCTION compare_function, typename coord_type>
		requires(!has_flag<COMPUTE_IMAGE_TYPE::FLAG_ARRAY>(image_type))
		auto compare(const coord_type& coord, const float& compare_value, const offset_vec_type offset = {}, const float bias = 0.0f) const {
			return read_internal<false, false, false, false, true, compare_function>(coord, 0, 0, offset, bias, 0, {}, compare_value);
		}
		
		//! image depth compare read with nearest/point sampling (array)
		template <COMPARE_FUNCTION compare_function, typename coord_type>
		requires(has_flag<COMPUTE_IMAGE_TYPE::FLAG_ARRAY>(image_type))
		auto compare(const coord_type& coord, const uint32_t layer, const float& compare_value, const offset_vec_type offset = {}, const float bias = 0.0f) const {
			return read_internal<false, false, false, false, true, compare_function>(coord, layer, 0, offset, bias, 0, {}, compare_value);
		}
		
		//! image depth compare read with linear sampling (non-array)
		template <COMPARE_FUNCTION compare_function, typename coord_type>
		requires(!has_flag<COMPUTE_IMAGE_TYPE::FLAG_ARRAY>(image_type))
		auto compare_linear(const coord_type& coord, const float& compare_value, const offset_vec_type offset = {}, const float bias = 0.0f) const {
			return read_internal<true, false, false, false, true, compare_function>(coord, 0, 0, offset, bias, 0, {}, compare_value);
		}
		
		//! image depth compare read with linear sampling (array)
		template <COMPARE_FUNCTION compare_function, typename coord_type>
		requires(has_flag<COMPUTE_IMAGE_TYPE::FLAG_ARRAY>(image_type))
		auto compare_linear(const coord_type& coord, const uint32_t layer, const float& compare_value, const offset_vec_type offset = {}, const float bias = 0.0f) const {
			return read_internal<true, false, false, false, true, compare_function>(coord, layer, 0, offset, bias, 0, {}, compare_value);
		}
		
		//! image depth compare read at an explicit lod level with nearest/point sampling (non-array)
		template <COMPARE_FUNCTION compare_function, typename coord_type, typename lod_type>
		requires(!has_flag<COMPUTE_IMAGE_TYPE::FLAG_ARRAY>(image_type))
		auto compare_lod(const coord_type& coord, const float& compare_value, const lod_type lod, const offset_vec_type offset = {}) const {
			return read_internal<false, false, true, false, true, compare_function>(coord, 0, 0, offset, 0.0f, lod, {}, compare_value);
		}
		
		//! image depth compare read at an explicit lod level with nearest/point sampling (array)
		template <COMPARE_FUNCTION compare_function, typename coord_type, typename lod_type>
		requires(has_flag<COMPUTE_IMAGE_TYPE::FLAG_ARRAY>(image_type))
		auto compare_lod(const coord_type& coord, const uint32_t layer, const float& compare_value, const lod_type lod, const offset_vec_type offset = {}) const {
			return read_internal<false, false, true, false, true, compare_function>(coord, layer, 0, offset, 0.0f, lod, {}, compare_value);
		}
		
		//! image depth compare read at an explicit lod level with linear sampling (non-array)
		template <COMPARE_FUNCTION compare_function, typename coord_type, typename lod_type>
		requires(!has_flag<COMPUTE_IMAGE_TYPE::FLAG_ARRAY>(image_type))
		auto compare_lod_linear(const coord_type& coord, const float& compare_value, const lod_type lod, const offset_vec_type offset = {}) const {
			return read_internal<true, false, true, false, true, compare_function>(coord, 0, 0, offset, 0.0f, lod, {}, compare_value);
		}
		
		//! image depth compare read at an explicit lod level with linear sampling (array)
		template <COMPARE_FUNCTION compare_function, typename coord_type, typename lod_type>
		requires(has_flag<COMPUTE_IMAGE_TYPE::FLAG_ARRAY>(image_type))
		auto compare_lod_linear(const coord_type& coord, const uint32_t layer, const float& compare_value, const lod_type lod, const offset_vec_type offset = {}) const {
			return read_internal<true, false, true, false, true, compare_function>(coord, layer, 0, offset, 0.0f, lod, {}, compare_value);
		}
		
		//! image depth compare read with an explicit gradient (dPdx, dPdy) with nearest/point sampling (non-array)
		template <COMPARE_FUNCTION compare_function, typename coord_type>
		requires(!has_flag<COMPUTE_IMAGE_TYPE::FLAG_ARRAY>(image_type))
		auto compare_gradient(const coord_type& coord, const float& compare_value,
							  const pair<gradient_vec_type, gradient_vec_type> gradient,
							  const offset_vec_type offset = {}) const {
			return read_internal<false, false, false, true, true, compare_function>(coord, 0, 0, offset, 0.0f, 0, gradient, compare_value);
		}
		
		//! image depth compare read with an explicit gradient (dPdx, dPdy) with nearest/point sampling (array)
		template <COMPARE_FUNCTION compare_function, typename coord_type>
		requires(has_flag<COMPUTE_IMAGE_TYPE::FLAG_ARRAY>(image_type))
		auto compare_gradient(const coord_type& coord, const uint32_t layer, const float& compare_value,
							  const pair<gradient_vec_type, gradient_vec_type> gradient,
							  const offset_vec_type offset = {}) const {
			return read_internal<false, false, false, true, true, compare_function>(coord, layer, 0, offset, 0.0f, 0, gradient, compare_value);
		}
		
		//! image depth compare read with an explicit gradient (dPdx, dPdy) with linear sampling (non-array)
		template <COMPARE_FUNCTION compare_function, typename coord_type>
		requires(!has_flag<COMPUTE_IMAGE_TYPE::FLAG_ARRAY>(image_type))
		auto compare_gradient_linear(const coord_type& coord, const float& compare_value,
									 const pair<gradient_vec_type, gradient_vec_type> gradient,
									 const offset_vec_type offset = {}) const {
			return read_internal<true, false, false, true, true, compare_function>(coord, 0, 0, offset, 0.0f, 0, gradient, compare_value);
		}
		
		//! image depth compare read with an explicit gradient (dPdx, dPdy) with linear sampling (array)
		template <COMPARE_FUNCTION compare_function, typename coord_type>
		requires(has_flag<COMPUTE_IMAGE_TYPE::FLAG_ARRAY>(image_type))
		auto compare_gradient_linear(const coord_type& coord, const uint32_t layer, const float& compare_value,
									 const pair<gradient_vec_type, gradient_vec_type> gradient,
									 const offset_vec_type offset = {}) const {
			return read_internal<true, false, false, true, true, compare_function>(coord, layer, 0, offset, 0.0f, 0, gradient, compare_value);
		}
		
	public: // image write functions
		//! internal write function
		template <bool is_lod = false, typename coord_type>
		requires (is_writable())
		floor_inline_always void write_internal(const coord_type& coord,
												const uint32_t layer,
												const uint32_t lod,
												const vector_sample_type& data) {
			// sample type must be 32-bit float, int or uint
			constexpr const bool is_float = is_sample_float(image_type);
			constexpr const bool is_int = is_sample_int(image_type);
			constexpr const bool is_depth = has_flag<COMPUTE_IMAGE_TYPE::FLAG_DEPTH>(image_type);
			static_assert(is_float || is_int || is_sample_uint(image_type), "invalid sample type");
			static_assert(!is_depth || (is_depth && is_float && is_same_v<vector_sample_type, float>), "depth value must always be a float");
			
			// backend specific coordinate conversion (should always be int here)
			const auto converted_coord = convert_coord(coord);
			
			// convert input data (vector) to a vector4 (if color or depth on resp. backends) or float (if depth on resp. backends)
#if defined(FLOOR_COMPUTE_METAL) || defined(FLOOR_COMPUTE_HOST)
			const auto converted_data = [&data]() {
				if constexpr (!is_depth) {
					return convert_data<sample_type>(data);
				} else {
					return data;
				}
			}();
#else
			const auto converted_data = convert_data<sample_type>(data);
#endif
			
			// NOTE: data casts are necessary because clang is doing sema checking for these even if they're not used
#if defined(FLOOR_COMPUTE_OPENCL)
			if constexpr(is_float) opaque_image::write_image_float(w_img(), image_type, converted_coord, layer, lod, is_lod, (float4)converted_data);
			else if constexpr(is_int) opaque_image::write_image_int(w_img(), image_type, converted_coord, layer, lod, is_lod, (int4)converted_data);
			else opaque_image::write_image_uint(w_img(), image_type, converted_coord, layer, lod, is_lod, (uint4)converted_data);
#elif defined(FLOOR_COMPUTE_METAL)
			if constexpr (is_depth) opaque_image::write_image_float(w_img(), image_type, converted_coord, layer, lod, is_lod, converted_data);
			else if constexpr(is_float) opaque_image::write_image_float(w_img(), image_type, converted_coord, layer, lod, is_lod, (float4)converted_data);
			else if constexpr(is_int) opaque_image::write_image_int(w_img(), image_type, converted_coord, layer, lod, is_lod, (int4)converted_data);
			else opaque_image::write_image_uint(w_img(), image_type, converted_coord, layer, lod, is_lod, (uint4)converted_data);
#elif defined(FLOOR_COMPUTE_VULKAN)
			if constexpr(!is_lod) {
				if constexpr(is_float) opaque_image::write_image_float(w_img(), image_type, converted_coord, layer, 0, false, (float4)converted_data);
				else if constexpr(is_int) opaque_image::write_image_int(w_img(), image_type, converted_coord, layer, 0, false, (int4)converted_data);
				else opaque_image::write_image_uint(w_img(), image_type, converted_coord, layer, 0, false, (uint4)converted_data);
			} else {
				if constexpr(is_float) opaque_image::write_image_float(w_img(lod), image_type, converted_coord, layer, lod, is_lod, (float4)converted_data);
				else if constexpr(is_int) opaque_image::write_image_int(w_img(lod), image_type, converted_coord, layer, lod, is_lod, (int4)converted_data);
				else opaque_image::write_image_uint(w_img(lod), image_type, converted_coord, layer, lod, is_lod, (uint4)converted_data);
			}
#elif defined(FLOOR_COMPUTE_CUDA)
			if constexpr(!is_lod) {
				if constexpr(is_float) cuda_image::write_float<image_type>(w_img_obj, runtime_image_type, converted_coord, layer, 0, false, (float4)converted_data);
				else if constexpr(is_int) cuda_image::write_int<image_type>(w_img_obj, runtime_image_type, converted_coord, layer, 0, false, (int4)converted_data);
				else cuda_image::write_uint<image_type>(w_img_obj, runtime_image_type, converted_coord, layer, 0, false, (uint4)converted_data);
			} else {
				if constexpr(is_float) cuda_image::write_float<image_type>(w_img_lod_obj[lod], runtime_image_type, converted_coord, layer, lod, is_lod, (float4)converted_data);
				else if constexpr(is_int) cuda_image::write_int<image_type>(w_img_lod_obj[lod], runtime_image_type, converted_coord, layer, lod, is_lod, (int4)converted_data);
				else cuda_image::write_uint<image_type>(w_img_lod_obj[lod], runtime_image_type, converted_coord, layer, lod, is_lod, (uint4)converted_data);
			}
#elif defined(FLOOR_COMPUTE_HOST)
			host_device_image<image_type, is_lod, false, false>::write((host_device_image<image_type, is_lod, false, false>*)w_img(), converted_coord, layer, lod, converted_data);
#endif
		}
		
		//! image write (non-array)
		template <typename coord_type>
		requires(!has_flag<COMPUTE_IMAGE_TYPE::FLAG_ARRAY>(image_type))
		floor_inline_always auto write(const coord_type& coord,
									   const vector_sample_type& data) {
			static_assert(is_int_coord<coord_type>(), "image write coordinates must be of integer type");
			return write_internal<false>(coord, 0, 0, data);
		}
		
		//! image write (array)
		template <typename coord_type>
		requires(has_flag<COMPUTE_IMAGE_TYPE::FLAG_ARRAY>(image_type))
		floor_inline_always auto write(const coord_type& coord,
									   const uint32_t layer,
									   const vector_sample_type& data) {
			static_assert(is_int_coord<coord_type>(), "image write coordinates must be of integer type");
			return write_internal<false>(coord, layer, 0, data);
		}
		
		//! image write at the specified lod level (non-array)
		template <typename coord_type>
		requires(!has_flag<COMPUTE_IMAGE_TYPE::FLAG_ARRAY>(image_type))
		floor_inline_always auto write_lod(const coord_type& coord,
										   const uint32_t lod,
										   const vector_sample_type& data) {
			static_assert(is_int_coord<coord_type>(), "image write coordinates must be of integer type");
			return write_internal<true>(coord, 0, lod, data);
		}
		
		//! image write at the specified lod level (array)
		template <typename coord_type>
		requires(has_flag<COMPUTE_IMAGE_TYPE::FLAG_ARRAY>(image_type))
		floor_inline_always auto write_lod(const coord_type& coord,
										   const uint32_t layer,
										   const uint32_t lod,
										   const vector_sample_type& data) {
			static_assert(is_int_coord<coord_type>(), "image write coordinates must be of integer type");
			return write_internal<true>(coord, layer, lod, data);
		}
		
		// TODO: msaa write functions (supported by vulkan)
		
	};
	
}

//! read-write image (if write_only == false), write-only image (if write_only == true)
template <COMPUTE_IMAGE_TYPE image_type, bool write_only = false>
using image = floor_image::image<((image_type & ~COMPUTE_IMAGE_TYPE::__ACCESS_MASK) |
								  COMPUTE_IMAGE_TYPE::WRITE |
								  (write_only ? COMPUTE_IMAGE_TYPE::NONE : COMPUTE_IMAGE_TYPE::READ))>;

//! const/read-only image
template <COMPUTE_IMAGE_TYPE image_type>
using const_image = floor_image::image<(image_type & ~COMPUTE_IMAGE_TYPE::__ACCESS_MASK) | COMPUTE_IMAGE_TYPE::READ>;

// const/read-only image types
template <typename sample_type, COMPUTE_IMAGE_TYPE ext_type = COMPUTE_IMAGE_TYPE::NONE> using const_image_1d =
const_image<COMPUTE_IMAGE_TYPE::IMAGE_1D | ext_type | floor_image::from_sample_type<sample_type>::type>;

template <typename sample_type, COMPUTE_IMAGE_TYPE ext_type = COMPUTE_IMAGE_TYPE::NONE> using const_image_1d_array =
const_image<COMPUTE_IMAGE_TYPE::IMAGE_1D_ARRAY | ext_type | floor_image::from_sample_type<sample_type>::type>;

template <typename sample_type, COMPUTE_IMAGE_TYPE ext_type = COMPUTE_IMAGE_TYPE::NONE> using const_image_2d =
const_image<COMPUTE_IMAGE_TYPE::IMAGE_2D | ext_type | floor_image::from_sample_type<sample_type>::type>;

template <typename sample_type, COMPUTE_IMAGE_TYPE ext_type = COMPUTE_IMAGE_TYPE::NONE> using const_image_2d_array =
const_image<COMPUTE_IMAGE_TYPE::IMAGE_2D_ARRAY | ext_type | floor_image::from_sample_type<sample_type>::type>;

template <typename sample_type, COMPUTE_IMAGE_TYPE ext_type = COMPUTE_IMAGE_TYPE::NONE> using const_image_2d_msaa =
const_image<COMPUTE_IMAGE_TYPE::IMAGE_2D_MSAA | ext_type | floor_image::from_sample_type<sample_type>::type>;

template <typename sample_type, COMPUTE_IMAGE_TYPE ext_type = COMPUTE_IMAGE_TYPE::NONE> using const_image_2d_msaa_array =
const_image<COMPUTE_IMAGE_TYPE::IMAGE_2D_MSAA_ARRAY | ext_type | floor_image::from_sample_type<sample_type>::type>;

template <typename sample_type, COMPUTE_IMAGE_TYPE ext_type = COMPUTE_IMAGE_TYPE::NONE> using const_image_cube =
const_image<COMPUTE_IMAGE_TYPE::IMAGE_CUBE | ext_type | floor_image::from_sample_type<sample_type>::type>;

template <typename sample_type, COMPUTE_IMAGE_TYPE ext_type = COMPUTE_IMAGE_TYPE::NONE> using const_image_cube_array =
const_image<COMPUTE_IMAGE_TYPE::IMAGE_CUBE_ARRAY | ext_type | floor_image::from_sample_type<sample_type>::type>;

template <typename sample_type, COMPUTE_IMAGE_TYPE ext_type = COMPUTE_IMAGE_TYPE::NONE> using const_image_2d_depth =
const_image<(COMPUTE_IMAGE_TYPE::IMAGE_DEPTH | ext_type |
			 // always single channel
			 COMPUTE_IMAGE_TYPE::FLAG_FIXED_CHANNELS |
			 (floor_image::from_sample_type<sample_type>::type & ~COMPUTE_IMAGE_TYPE::__CHANNELS_MASK))>;

template <typename sample_type, COMPUTE_IMAGE_TYPE ext_type = COMPUTE_IMAGE_TYPE::NONE> using const_image_2d_depth_stencil =
const_image<(COMPUTE_IMAGE_TYPE::IMAGE_DEPTH_STENCIL | ext_type |
			 // always 2 channels
			 COMPUTE_IMAGE_TYPE::FLAG_FIXED_CHANNELS |
			 (floor_image::from_sample_type<sample_type>::type & ~COMPUTE_IMAGE_TYPE::__CHANNELS_MASK))>;

template <typename sample_type, COMPUTE_IMAGE_TYPE ext_type = COMPUTE_IMAGE_TYPE::NONE> using const_image_2d_depth_array =
const_image<(COMPUTE_IMAGE_TYPE::IMAGE_DEPTH_ARRAY | ext_type |
			 // always single channel
			 COMPUTE_IMAGE_TYPE::FLAG_FIXED_CHANNELS |
			 (floor_image::from_sample_type<sample_type>::type & ~COMPUTE_IMAGE_TYPE::__CHANNELS_MASK))>;

template <typename sample_type, COMPUTE_IMAGE_TYPE ext_type = COMPUTE_IMAGE_TYPE::NONE> using const_image_cube_depth =
const_image<(COMPUTE_IMAGE_TYPE::IMAGE_DEPTH_CUBE | ext_type |
			 // always single channel
			 COMPUTE_IMAGE_TYPE::FLAG_FIXED_CHANNELS |
			 (floor_image::from_sample_type<sample_type>::type & ~COMPUTE_IMAGE_TYPE::__CHANNELS_MASK))>;

template <typename sample_type, COMPUTE_IMAGE_TYPE ext_type = COMPUTE_IMAGE_TYPE::NONE> using const_image_cube_depth_array =
const_image<(COMPUTE_IMAGE_TYPE::IMAGE_DEPTH_CUBE_ARRAY | ext_type |
			 // always single channel
			 COMPUTE_IMAGE_TYPE::FLAG_FIXED_CHANNELS |
			 (floor_image::from_sample_type<sample_type>::type & ~COMPUTE_IMAGE_TYPE::__CHANNELS_MASK))>;

template <typename sample_type, COMPUTE_IMAGE_TYPE ext_type = COMPUTE_IMAGE_TYPE::NONE> using const_image_2d_depth_msaa =
const_image<(COMPUTE_IMAGE_TYPE::IMAGE_DEPTH_MSAA | ext_type |
			 // always single channel
			 COMPUTE_IMAGE_TYPE::FLAG_FIXED_CHANNELS |
			 (floor_image::from_sample_type<sample_type>::type & ~COMPUTE_IMAGE_TYPE::__CHANNELS_MASK))>;

template <typename sample_type, COMPUTE_IMAGE_TYPE ext_type = COMPUTE_IMAGE_TYPE::NONE> using const_image_2d_depth_msaa_array =
const_image<(COMPUTE_IMAGE_TYPE::IMAGE_DEPTH_MSAA_ARRAY | ext_type |
			 // always single channel
			 COMPUTE_IMAGE_TYPE::FLAG_FIXED_CHANNELS |
			 (floor_image::from_sample_type<sample_type>::type & ~COMPUTE_IMAGE_TYPE::__CHANNELS_MASK))>;

template <typename sample_type, COMPUTE_IMAGE_TYPE ext_type = COMPUTE_IMAGE_TYPE::NONE> using const_image_3d =
const_image<COMPUTE_IMAGE_TYPE::IMAGE_3D | ext_type | floor_image::from_sample_type<sample_type>::type>;

// read-write/write-only image types
template <typename sample_type, bool write_only = false, COMPUTE_IMAGE_TYPE ext_type = COMPUTE_IMAGE_TYPE::NONE> using image_1d =
image<COMPUTE_IMAGE_TYPE::IMAGE_1D | ext_type | floor_image::from_sample_type<sample_type>::type, write_only>;

template <typename sample_type, bool write_only = false, COMPUTE_IMAGE_TYPE ext_type = COMPUTE_IMAGE_TYPE::NONE> using image_1d_array =
image<COMPUTE_IMAGE_TYPE::IMAGE_1D_ARRAY | ext_type | floor_image::from_sample_type<sample_type>::type, write_only>;

template <typename sample_type, bool write_only = false, COMPUTE_IMAGE_TYPE ext_type = COMPUTE_IMAGE_TYPE::NONE> using image_2d =
image<COMPUTE_IMAGE_TYPE::IMAGE_2D | ext_type | floor_image::from_sample_type<sample_type>::type, write_only>;

template <typename sample_type, bool write_only = false, COMPUTE_IMAGE_TYPE ext_type = COMPUTE_IMAGE_TYPE::NONE> using image_2d_array =
image<COMPUTE_IMAGE_TYPE::IMAGE_2D_ARRAY | ext_type | floor_image::from_sample_type<sample_type>::type, write_only>;

template <typename sample_type, bool write_only = false, COMPUTE_IMAGE_TYPE ext_type = COMPUTE_IMAGE_TYPE::NONE> using image_2d_msaa =
image<COMPUTE_IMAGE_TYPE::IMAGE_2D_MSAA | ext_type | floor_image::from_sample_type<sample_type>::type, write_only>;

template <typename sample_type, bool write_only = false, COMPUTE_IMAGE_TYPE ext_type = COMPUTE_IMAGE_TYPE::NONE> using image_2d_msaa_array =
image<COMPUTE_IMAGE_TYPE::IMAGE_2D_MSAA_ARRAY | ext_type | floor_image::from_sample_type<sample_type>::type, write_only>;

template <typename sample_type, bool write_only = false, COMPUTE_IMAGE_TYPE ext_type = COMPUTE_IMAGE_TYPE::NONE> using image_cube =
image<COMPUTE_IMAGE_TYPE::IMAGE_CUBE | ext_type | floor_image::from_sample_type<sample_type>::type, write_only>;

template <typename sample_type, bool write_only = false, COMPUTE_IMAGE_TYPE ext_type = COMPUTE_IMAGE_TYPE::NONE> using image_cube_array =
image<COMPUTE_IMAGE_TYPE::IMAGE_CUBE_ARRAY | ext_type | floor_image::from_sample_type<sample_type>::type, write_only>;

template <typename sample_type, bool write_only = false, COMPUTE_IMAGE_TYPE ext_type = COMPUTE_IMAGE_TYPE::NONE> using image_2d_depth =
image<(COMPUTE_IMAGE_TYPE::IMAGE_DEPTH | ext_type |
	   // always single channel
	   COMPUTE_IMAGE_TYPE::FLAG_FIXED_CHANNELS |
	   (floor_image::from_sample_type<sample_type>::type & ~COMPUTE_IMAGE_TYPE::__CHANNELS_MASK)), write_only>;

template <typename sample_type, bool write_only = false, COMPUTE_IMAGE_TYPE ext_type = COMPUTE_IMAGE_TYPE::NONE> using image_2d_depth_stencil =
image<(COMPUTE_IMAGE_TYPE::IMAGE_DEPTH_STENCIL | ext_type |
	   // always 2 channels
	   COMPUTE_IMAGE_TYPE::FLAG_FIXED_CHANNELS |
	   (floor_image::from_sample_type<sample_type>::type & ~COMPUTE_IMAGE_TYPE::__CHANNELS_MASK)), write_only>;

template <typename sample_type, bool write_only = false, COMPUTE_IMAGE_TYPE ext_type = COMPUTE_IMAGE_TYPE::NONE> using image_2d_depth_array =
image<(COMPUTE_IMAGE_TYPE::IMAGE_DEPTH_ARRAY | ext_type |
	   // always single channel
	   COMPUTE_IMAGE_TYPE::FLAG_FIXED_CHANNELS |
	   (floor_image::from_sample_type<sample_type>::type & ~COMPUTE_IMAGE_TYPE::__CHANNELS_MASK)), write_only>;

template <typename sample_type, bool write_only = false, COMPUTE_IMAGE_TYPE ext_type = COMPUTE_IMAGE_TYPE::NONE> using image_cube_depth =
image<(COMPUTE_IMAGE_TYPE::IMAGE_DEPTH_CUBE | ext_type |
	   // always single channel
	   COMPUTE_IMAGE_TYPE::FLAG_FIXED_CHANNELS |
	   (floor_image::from_sample_type<sample_type>::type & ~COMPUTE_IMAGE_TYPE::__CHANNELS_MASK)), write_only>;

template <typename sample_type, bool write_only = false, COMPUTE_IMAGE_TYPE ext_type = COMPUTE_IMAGE_TYPE::NONE> using image_cube_depth_array =
image<(COMPUTE_IMAGE_TYPE::IMAGE_DEPTH_CUBE_ARRAY | ext_type |
	   // always single channel
	   COMPUTE_IMAGE_TYPE::FLAG_FIXED_CHANNELS |
	   (floor_image::from_sample_type<sample_type>::type & ~COMPUTE_IMAGE_TYPE::__CHANNELS_MASK)), write_only>;

template <typename sample_type, bool write_only = false, COMPUTE_IMAGE_TYPE ext_type = COMPUTE_IMAGE_TYPE::NONE> using image_2d_depth_msaa =
image<(COMPUTE_IMAGE_TYPE::IMAGE_DEPTH_MSAA | ext_type |
	   // always single channel
	   COMPUTE_IMAGE_TYPE::FLAG_FIXED_CHANNELS |
	   (floor_image::from_sample_type<sample_type>::type & ~COMPUTE_IMAGE_TYPE::__CHANNELS_MASK)), write_only>;

template <typename sample_type, bool write_only = false, COMPUTE_IMAGE_TYPE ext_type = COMPUTE_IMAGE_TYPE::NONE> using image_2d_depth_msaa_array =
image<(COMPUTE_IMAGE_TYPE::IMAGE_DEPTH_MSAA_ARRAY | ext_type |
	   // always single channel
	   COMPUTE_IMAGE_TYPE::FLAG_FIXED_CHANNELS |
	   (floor_image::from_sample_type<sample_type>::type & ~COMPUTE_IMAGE_TYPE::__CHANNELS_MASK)), write_only>;

template <typename sample_type, bool write_only = false, COMPUTE_IMAGE_TYPE ext_type = COMPUTE_IMAGE_TYPE::NONE> using image_3d =
image<COMPUTE_IMAGE_TYPE::IMAGE_3D | ext_type | floor_image::from_sample_type<sample_type>::type, write_only>;

#endif
