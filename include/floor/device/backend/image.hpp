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

#if defined(FLOOR_DEVICE_OPENCL) || defined(FLOOR_DEVICE_METAL) || defined(FLOOR_DEVICE_VULKAN)
// IMAGE_TYPE -> OpenCL/Vulkan/Metal image type map
#include <floor/device/backend/opaque_image_map.hpp>
// OpenCL/Vulkan/Metal image read/write functions
#include <floor/device/backend/opaque_image.hpp>
#endif

namespace fl {

namespace floor_image {
	//! is image type sampling return type a float?
	static constexpr bool is_sample_float(IMAGE_TYPE image_type) {
		return ((has_flag<IMAGE_TYPE::FLAG_NORMALIZED>(image_type) ||
				(image_type & IMAGE_TYPE::__DATA_TYPE_MASK) == IMAGE_TYPE::FLOAT) &&
				!has_flag<IMAGE_TYPE::FLAG_16_BIT_SAMPLING>(image_type));
	}

	//! is image type sampling return type a half?
	static constexpr bool is_sample_half(IMAGE_TYPE image_type) {
		return ((has_flag<IMAGE_TYPE::FLAG_NORMALIZED>(image_type) ||
				(image_type & IMAGE_TYPE::__DATA_TYPE_MASK) == IMAGE_TYPE::FLOAT) &&
				has_flag<IMAGE_TYPE::FLAG_16_BIT_SAMPLING>(image_type));
	}
	
	//! is image type sampling return type an int?
	static constexpr bool is_sample_int(IMAGE_TYPE image_type) {
		return (!has_flag<IMAGE_TYPE::FLAG_NORMALIZED>(image_type) &&
				(image_type & IMAGE_TYPE::__DATA_TYPE_MASK) == IMAGE_TYPE::INT &&
				!has_flag<IMAGE_TYPE::FLAG_16_BIT_SAMPLING>(image_type));
	}
	
	//! is image type sampling return type a short?
	static constexpr bool is_sample_short(IMAGE_TYPE image_type) {
		return (!has_flag<IMAGE_TYPE::FLAG_NORMALIZED>(image_type) &&
				(image_type & IMAGE_TYPE::__DATA_TYPE_MASK) == IMAGE_TYPE::INT &&
				has_flag<IMAGE_TYPE::FLAG_16_BIT_SAMPLING>(image_type));
	}
	
	//! is image type sampling return type an uint?
	static constexpr bool is_sample_uint(IMAGE_TYPE image_type) {
		return (!has_flag<IMAGE_TYPE::FLAG_NORMALIZED>(image_type) &&
				(image_type & IMAGE_TYPE::__DATA_TYPE_MASK) == IMAGE_TYPE::UINT &&
				!has_flag<IMAGE_TYPE::FLAG_16_BIT_SAMPLING>(image_type));
	}

	//! is image type sampling return type an ushort?
	static constexpr bool is_sample_ushort(IMAGE_TYPE image_type) {
		return (!has_flag<IMAGE_TYPE::FLAG_NORMALIZED>(image_type) &&
				(image_type & IMAGE_TYPE::__DATA_TYPE_MASK) == IMAGE_TYPE::UINT &&
				has_flag<IMAGE_TYPE::FLAG_16_BIT_SAMPLING>(image_type));
	}
	
	//! returns true if coord_type is an int/integral type (int, int2, int3, ...), false if anything else
	template <typename coord_type, bool ret_value = ((is_floor_vector<coord_type>::value &&
													  ext::is_integral_v<typename coord_type::decayed_scalar_type>) ||
													 (ext::is_fundamental_v<coord_type> && ext::is_integral_v<coord_type>))>
	static constexpr bool is_int_coord() {
		return ret_value;
	}
	
	//! returns true if coord_type is a float type (float, float2, float3, ...), false if anything else
	template <typename coord_type, bool ret_value = ((is_floor_vector<coord_type>::value &&
													  ext::is_floating_point_v<typename coord_type::decayed_scalar_type>) ||
													 ext::is_floating_point_v<coord_type>)>
	static constexpr bool is_float_coord() {
		return ret_value;
	}
	
	//! get the gradient vector type (dPdx and dPdy) of an image type (sanely default to float2 as this is the case for most formats)
	template <IMAGE_TYPE image_type>
	struct gradient_vec_type_for_image_type {
		using type = float2;
	};
	template <IMAGE_TYPE image_type>
	requires(has_flag<IMAGE_TYPE::FLAG_CUBE>(image_type) ||
			 image_dim_count(image_type) == 3)
	struct gradient_vec_type_for_image_type<image_type> {
		using type = float3;
	};
	template <IMAGE_TYPE image_type>
	requires(image_dim_count(image_type) == 1)
	struct gradient_vec_type_for_image_type<image_type> {
		using type = float1;
	};
	
	//! get the offset vector type of an image type (sanely default to int2 as this is the case for most formats)
	template <IMAGE_TYPE image_type>
	struct offset_vec_type_for_image_type {
		using type = int2;
	};
	template <IMAGE_TYPE image_type>
	requires(image_dim_count(image_type) == 3
#if !defined(FLOOR_DEVICE_HOST_COMPUTE)
			 // this is a total hack, but cube map offsets aren't supported with
			 // CUDA/Metal/OpenCL and I don't want to add image functions/handling
			 // for something that isn't going to be used anyways
			 // -> use int3 offset instead of the actual int2 offset (b/c symmetry)
			 || has_flag<IMAGE_TYPE::FLAG_CUBE>(image_type)
#endif
			)
	struct offset_vec_type_for_image_type<image_type> {
		using type = int3;
	};
	template <IMAGE_TYPE image_type>
	requires(image_dim_count(image_type) == 1)
	struct offset_vec_type_for_image_type<image_type> {
		using type = int1;
	};
	
#if defined(FLOOR_DEVICE_OPENCL) || defined(FLOOR_DEVICE_METAL) || defined(FLOOR_DEVICE_VULKAN)
#if defined(FLOOR_DEVICE_METAL)
	constexpr metal_image::sampler::COMPARE_FUNCTION compare_function_floor_to_metal(const COMPARE_FUNCTION& func) {
		switch(func) {
			// Metal has both a "never" and "none" -> map these dependent on the Metal version mirroring what the Metal compiler is doing
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
	template <typename coord_type, bool sample_linear, bool sample_repeat, bool sample_repeat_mirrored,
			  COMPARE_FUNCTION compare_function = COMPARE_FUNCTION::NEVER>
	struct default_sampler {
		static constexpr auto value() {
#if defined(FLOOR_DEVICE_OPENCL)
			return ((sample_repeat ? opencl_image::sampler::ADDRESS_MODE::REPEAT :
					 (sample_repeat_mirrored ? opencl_image::sampler::ADDRESS_MODE::MIRRORED_REPEAT :
					  opencl_image::sampler::ADDRESS_MODE::CLAMP_TO_EDGE)) |
					(is_int_coord<coord_type>() ? opencl_image::sampler::COORD_MODE::PIXEL : opencl_image::sampler::COORD_MODE::NORMALIZED) |
					(!sample_linear ? opencl_image::sampler::FILTER_MODE::NEAREST : opencl_image::sampler::FILTER_MODE::LINEAR));
#elif defined(FLOOR_DEVICE_VULKAN)
			return (vulkan_image::sampler {
				(!sample_linear ? vulkan_image::sampler::NEAREST : vulkan_image::sampler::LINEAR),
				(sample_repeat ? vulkan_image::sampler::REPEAT :
				 (sample_repeat_mirrored ? vulkan_image::sampler::REPEAT_MIRRORED : vulkan_image::sampler::CLAMP_TO_EDGE)),
				(is_int_coord<coord_type>() ? vulkan_image::sampler::PIXEL : vulkan_image::sampler::NORMALIZED),
				(vulkan_image::sampler::COMPARE_FUNCTION)(uint32_t(compare_function) <<
														  vulkan_image::sampler::__COMPARE_FUNCTION_SHIFT)
			}).value;
#elif defined(FLOOR_DEVICE_METAL)
			return (metal_sampler_t)(metal_image::sampler {
				(sample_repeat ? metal_image::sampler::ADDRESS_MODE::REPEAT :
				 (sample_repeat_mirrored ? metal_image::sampler::ADDRESS_MODE::MIRRORED_REPEAT :
				  metal_image::sampler::ADDRESS_MODE::CLAMP_TO_EDGE)),
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
#if defined(FLOOR_DEVICE_OPENCL)
	using sampler_type = sampler_t;
#elif defined(FLOOR_DEVICE_VULKAN)
	using sampler_type = decltype(vulkan_image::sampler::value);
#elif defined(FLOOR_DEVICE_METAL)
	using sampler_type = metal_sampler_t;
#endif
	
	//! IMAGE_TYPE -> sample type
	template <IMAGE_TYPE image_type> struct to_sample_type {};
	template <IMAGE_TYPE image_type> requires(is_sample_float(image_type)) struct to_sample_type<image_type> {
		using type = float;
	};
	template <IMAGE_TYPE image_type> requires(is_sample_half(image_type)) struct to_sample_type<image_type> {
		using type = half;
	};
	template <IMAGE_TYPE image_type> requires(is_sample_int(image_type)) struct to_sample_type<image_type> {
		using type = int32_t;
	};
	template <IMAGE_TYPE image_type> requires(is_sample_short(image_type)) struct to_sample_type<image_type> {
		using type = int16_t;
	};
	template <IMAGE_TYPE image_type> requires(is_sample_uint(image_type)) struct to_sample_type<image_type> {
		using type = uint32_t;
	};
	template <IMAGE_TYPE image_type> requires(is_sample_ushort(image_type)) struct to_sample_type<image_type> {
		using type = uint16_t;
	};
	//! (vector) sample type -> IMAGE_TYPE
	//! NOTE: scalar sample types will always return the 4-channel variant,
	//!       vector sample types will return the corresponding channel variant
	template <typename sample_type> struct from_sample_type {};
	template <typename sample_type> requires(std::is_same_v<float, sample_type>) struct from_sample_type<sample_type> {
		static constexpr IMAGE_TYPE type {
			IMAGE_TYPE::FLOAT | IMAGE_TYPE::CHANNELS_4
		};
	};
	template <typename sample_type> requires(std::is_same_v<half, sample_type>) struct from_sample_type<sample_type> {
		static constexpr IMAGE_TYPE type {
			IMAGE_TYPE::FLOAT | IMAGE_TYPE::CHANNELS_4 | IMAGE_TYPE::FLAG_16_BIT_SAMPLING
		};
	};
	template <typename sample_type> requires(std::is_same_v<int32_t, sample_type>) struct from_sample_type<sample_type> {
		static constexpr IMAGE_TYPE type {
			IMAGE_TYPE::INT | IMAGE_TYPE::CHANNELS_4
		};
	};
	template <typename sample_type> requires(std::is_same_v<int16_t, sample_type>) struct from_sample_type<sample_type> {
		static constexpr IMAGE_TYPE type {
			IMAGE_TYPE::INT | IMAGE_TYPE::CHANNELS_4 | IMAGE_TYPE::FLAG_16_BIT_SAMPLING
		};
	};
	template <typename sample_type> requires(std::is_same_v<uint32_t, sample_type>) struct from_sample_type<sample_type> {
		static constexpr IMAGE_TYPE type {
			IMAGE_TYPE::UINT | IMAGE_TYPE::CHANNELS_4
		};
	};
	template <typename sample_type> requires(std::is_same_v<uint16_t, sample_type>) struct from_sample_type<sample_type> {
		static constexpr IMAGE_TYPE type {
			IMAGE_TYPE::UINT | IMAGE_TYPE::CHANNELS_4 | IMAGE_TYPE::FLAG_16_BIT_SAMPLING
		};
	};
	template <typename sample_type> requires(is_floor_vector<sample_type>::value) struct from_sample_type<sample_type> {
		static constexpr IMAGE_TYPE type {
			// get scalar type, clear out channel count, OR with actual channel count, set FIXED_CHANNELS flag
			((from_sample_type<typename sample_type::this_scalar_type>::type & ~IMAGE_TYPE::__CHANNELS_MASK) |
			 (IMAGE_TYPE)(uint32_t(sample_type::dim() - 1u) << uint32_t(IMAGE_TYPE::__CHANNELS_SHIFT)) |
			 IMAGE_TYPE::FLAG_FIXED_CHANNELS)
		};
	};

	//! image struct that is used for disabled image members
	struct disabled_image_t {};
	
	//! implementation specific image storage
	template <IMAGE_TYPE image_type>
	class image {
	public: // image storage
		// helpers
		static constexpr bool is_readable() { return has_flag<IMAGE_TYPE::READ>(image_type); }
		static constexpr bool is_writable() { return has_flag<IMAGE_TYPE::WRITE>(image_type); }
		static constexpr bool is_read_only() { return is_readable() && !is_writable(); }
		static constexpr bool is_write_only() { return !is_readable() && is_writable(); }
		static constexpr bool is_read_write() { return is_readable() && is_writable(); }
		static constexpr bool has_read_write_support() { return FLOOR_DEVICE_INFO_HAS_IMAGE_READ_WRITE_SUPPORT; }
		static constexpr bool is_array() { return has_flag<IMAGE_TYPE::FLAG_ARRAY>(image_type); }
		static constexpr uint32_t channel_count() { return image_channel_count(image_type); }
		static constexpr IMAGE_TYPE type { image_type };
		
		using sample_type = typename to_sample_type<image_type>::type;
		using vector_sample_type = std::conditional_t<channel_count() == 1, sample_type, vector_n<sample_type, channel_count()>>;
		using offset_vec_type = typename offset_vec_type_for_image_type<image_type>::type;
		using gradient_vec_type = typename gradient_vec_type_for_image_type<image_type>::type;
		
		// actual image storage/types
#if defined(FLOOR_DEVICE_OPENCL) || defined(FLOOR_DEVICE_METAL) || defined(FLOOR_DEVICE_VULKAN)
		// OpenCL/Metal/Vulkan have/store different image types dependent on access mode
		using opaque_type = typename opaque_image_type<image_type>::type;
		
#if defined(FLOOR_DEVICE_OPENCL) || defined(FLOOR_DEVICE_METAL)
		using primary_image_type = opaque_type;
		using secondary_image_type = std::conditional_t<is_read_write() && !has_read_write_support(), opaque_type, disabled_image_t>;
#elif defined(FLOOR_DEVICE_VULKAN)
		// Vulkan needs to deal with selecting between a simple image (readable) and a pointer to an image array (writable)
		static_assert(!has_read_write_support(), "Vulkan has no native read+write support");
		using writeable_image_array_type = opaque_type[FLOOR_DEVICE_INFO_MAX_MIP_LEVELS];
		using writeable_image_type = writeable_image_array_type*;
		using primary_image_type = std::conditional_t<is_readable(), opaque_type, writeable_image_type>;
		using secondary_image_type = std::conditional_t<is_read_write(), writeable_image_type, disabled_image_t>;
#endif
		
		// primary image: can be read-only or write-only, or read+write if supported by backend
		static constexpr const IMAGE_TYPE primary_image_flags = ((image_type & ~IMAGE_TYPE::__ACCESS_MASK) |
																		 (is_read_write() && has_read_write_support() ? IMAGE_TYPE::READ_WRITE :
																		  (is_readable() ? IMAGE_TYPE::READ : IMAGE_TYPE::WRITE)));
		
		// secondary image: can only be write-only if image is read+write and backend has no read+write support
		static constexpr const IMAGE_TYPE secondary_image_flags = ((image_type & ~IMAGE_TYPE::__ACCESS_MASK) |
																		   (is_read_write() && !has_read_write_support() ? IMAGE_TYPE::WRITE : IMAGE_TYPE::NONE));
		
		[[floor::image_data_type(sample_type), floor::image_flags(primary_image_flags)]] primary_image_type primary_img_obj;
		[[no_unique_address, floor::image_data_type(sample_type), floor::image_flags(secondary_image_flags)]] secondary_image_type secondary_img_obj;
		
#elif defined(FLOOR_DEVICE_CUDA)
		// readable and writable images always exist, regardless of access mode
		const uint32_t r_img_obj[cuda_sampler::max_sampler_count];
		uint64_t w_img_obj;
		uint64_t* w_img_lod_obj;
		const IMAGE_TYPE runtime_image_type;
#elif defined(FLOOR_DEVICE_HOST_COMPUTE)
		// always the same image object, regardless of access mode
		host_device_image<image_type>* img_obj;
#endif
		
		// image accessors
		constexpr auto& r_img() const requires(is_readable()) {
#if defined(FLOOR_DEVICE_OPENCL) || defined(FLOOR_DEVICE_METAL) || defined(FLOOR_DEVICE_VULKAN)
			return primary_img_obj;
#elif defined(FLOOR_DEVICE_CUDA)
			return r_img_obj;
#elif defined(FLOOR_DEVICE_HOST_COMPUTE)
			return img_obj;
#endif
		}
		constexpr auto& w_img(const uint32_t lod [[maybe_unused]] = 0) const requires(is_writable()) {
#if defined(FLOOR_DEVICE_OPENCL) || defined(FLOOR_DEVICE_METAL)
			if constexpr (has_read_write_support() || !is_readable()) {
				return primary_img_obj;
			} else {
				return secondary_img_obj;
			}
#elif defined(FLOOR_DEVICE_VULKAN)
			if constexpr (!is_readable()) {
				return (*primary_img_obj)[lod];
			} else {
				return (*secondary_img_obj)[lod];
			}
#elif defined(FLOOR_DEVICE_CUDA)
			return w_img_obj;
#elif defined(FLOOR_DEVICE_HOST_COMPUTE)
			return img_obj;
#endif
		}
		
	public: // additional image helper functions
#if !defined(FLOOR_DEVICE_HOST_COMPUTE) // OpenCL/Metal/CUDA coordinate conversion
		//! convert any coordinate vector type to int* or float* clang vector types
		template <typename coord_type>
		static auto convert_coord(const coord_type& coord) {
			return (vector_n<std::conditional_t<ext::is_integral_v<typename coord_type::decayed_scalar_type>, int, float>, coord_type::dim()> {
				coord
			}).to_clang_vector();
		}
		
		//! convert any fundamental (single value) coordinate type to int or float
		template <typename coord_type>
		requires(ext::is_fundamental_v<coord_type>)
		static auto convert_coord(const coord_type& coord) {
			return std::conditional_t<ext::is_integral_v<coord_type>, int, float> { coord };
		}
#else // Host-Compute
		//! convert any coordinate vector type to floor int{1,2,3,4} or float{1,2,3,4} vectors
		template <typename coord_type>
		static auto convert_coord(const coord_type& coord) {
			if constexpr (!ext::is_fundamental_v<coord_type>) {
				return vector_n<std::conditional_t<ext::is_integral_v<typename coord_type::decayed_scalar_type>, int, float>, coord_type::dim()> { coord };
			} else {
				return vector_n<std::conditional_t<ext::is_integral_v<coord_type>, int, float>, 1> { coord };
			}
		}
#endif
		
		//! converts any fundamental (single value) type to a vector4 type (which can then be converted to a corresponding clang_*4 type)
		template <typename expected_scalar_type, typename data_type>
		requires(ext::is_fundamental_v<data_type>)
		static auto convert_data(const data_type& data) {
			using scalar_type = data_type;
			static_assert(std::is_same_v<scalar_type, expected_scalar_type>, "invalid data type");
			return vector_n<scalar_type, 4> { data, (scalar_type)0, (scalar_type)0, (scalar_type)0 };
		}
		
		//! converts any vector* type to a vector4 type (which can then be converted to a corresponding clang_*4 type)
		template <typename expected_scalar_type, typename data_type>
		requires(!ext::is_fundamental_v<data_type>)
		static auto convert_data(const data_type& data) {
			using scalar_type = typename data_type::decayed_scalar_type;
			static_assert(std::is_same_v<scalar_type, expected_scalar_type>, "invalid data type");
			return vector_n<scalar_type, 4> { data };
		}
		
	public: // image query functions
		//! queries the image dimension at run-time, returning it in the same format as device_image::image_dim
		uint4 dim(const uint32_t lod = 0) const {
#if defined(FLOOR_DEVICE_OPENCL) || defined(FLOOR_DEVICE_METAL) || defined(FLOOR_DEVICE_VULKAN)
			if constexpr (is_readable()) {
				return uint4::from_clang_vector(opaque_image::get_image_dim(r_img(), image_type, lod));
			} else {
				return uint4::from_clang_vector(opaque_image::get_image_dim(w_img(), image_type, lod));
			}
#elif defined(FLOOR_DEVICE_CUDA)
			if constexpr (is_readable()) {
				return uint4::from_clang_vector(cuda_image::get_image_dim(r_img()[0], image_type, lod));
			} else {
				return uint4::from_clang_vector(cuda_image::get_image_dim(w_img(), image_type, lod));
			}
#elif defined(FLOOR_DEVICE_HOST_COMPUTE)
			if constexpr (is_readable()) {
				return r_img()->level_info[lod].dim;
			} else {
				return w_img()->level_info[lod].dim;
			}
#else
#error "unknown backend"
#endif
		}
		
		//! queries the LOD that would be used when sampling this image with an implicit LOD at the specified coordinate,
		//! the returned LOD may be fractional, e.g. 2.5f defines a 50/50 mix of LOD2 and LOD3
		//! NOTE: the image must be readable and the coordinate must by a floating point vector/scalar type
		//! NOTE: this can not be called on multi-sampled or buffer images
		//! NOTE: this may only be called inside a fragment shader
		template <typename coord_type>
		requires (is_readable() &&
				  is_float_coord<coord_type>() &&
				  !has_flag<IMAGE_TYPE::FLAG_MSAA>(image_type) &&
				  !has_flag<IMAGE_TYPE::FLAG_BUFFER>(image_type))
		float query_lod([[maybe_unused]] const coord_type& coord) const {
#if defined(FLOOR_DEVICE_METAL) || defined(FLOOR_DEVICE_VULKAN)
#if !defined(FLOOR_DEVICE_METAL) // only constexpr with Vulkan
			constexpr
#endif
			const sampler_type smplr = default_sampler<coord_type, true, true, false, COMPARE_FUNCTION::NEVER>::value();
			const auto converted_coord = convert_coord(coord);
			return opaque_image::query_image_lod(r_img(), smplr, image_type, converted_coord);
#else
			return 0.0f;
#endif
		}
		
	public: // image read functions
		//! internal read function, handling all kinds of reads
		//! NOTE: while this is an internal function, it might be useful for anyone insane enough to use it directly on the outside
		//!       -> this is a public function and not protected
		template <bool sample_linear, bool sample_repeat = false, bool sample_repeat_mirrored = false,
				  bool is_lod = false, bool is_gradient = false, bool is_compare = false,
				  COMPARE_FUNCTION compare_function = COMPARE_FUNCTION::NEVER,
				  typename coord_type, typename lod_type = int32_t>
		requires (is_readable())
		floor_inline_always auto read_internal(const coord_type& coord,
											   const uint32_t layer,
#if defined(FLOOR_DEVICE_HOST_COMPUTE) // NOTE: MSAA/sample is not supported on Host-Compute
											   const uint32_t sample floor_unused,
#else
											   const uint32_t sample,
#endif
											   const offset_vec_type offset,
											   const float bias = 0.0f,
											   const lod_type lod = (lod_type)0,
#if defined(FLOOR_DEVICE_HOST_COMPUTE) // NOTE: read with an explicit gradient is currently not supported on Host-Compute
											   const std::pair<gradient_vec_type, gradient_vec_type> gradient floor_unused = {},
#else
											   const std::pair<gradient_vec_type, gradient_vec_type> gradient = {},
#endif
											   const float compare_value = 0.0f
		) const {
			// sample type must be 32-bit or 16-bit float/int/uint
			static_assert(is_sample_float(image_type) || is_sample_half(image_type) ||
						  is_sample_int(image_type) || is_sample_short(image_type) ||
						  is_sample_uint(image_type) || is_sample_ushort(image_type), "invalid sample type");
			
			// explicit lod and gradient are mutually exclusive
			static_assert(!(is_lod && is_gradient), "can't use both lod and gradient");
			
			// lod type must be 32-bit float, int or uint (uint will be casted to int)
			using decayed_lod_type = std::decay_t<lod_type>;
			constexpr const bool is_lod_float = std::is_same_v<decayed_lod_type, float>;
			static_assert(std::is_same_v<decayed_lod_type, int32_t> ||
						  std::is_same_v<decayed_lod_type, uint32_t> ||
						  std::is_same_v<decayed_lod_type, float>, "lod type must be float, int32_t or uint32_t");
			
			// explicit lod or gradient read is not possible with msaa images
			static_assert((!is_lod && !is_gradient) ||
						  ((is_lod || is_gradient) && !has_flag<IMAGE_TYPE::FLAG_MSAA>(image_type)),
						  "image type does not support mip-maps");
			
			// if not explicit lod or gradient, then always use bias (neither lod nor gradient have a bias option)
			constexpr const bool is_bias = (!is_lod && !is_gradient);
			
			// depth compare read is only allowed for depth images
			static_assert((is_compare && has_flag<IMAGE_TYPE::FLAG_DEPTH>(image_type)) || !is_compare,
						  "compare is only allowed with depth images");
			
			// backend specific coordinate conversion (also: any input -> float or int)
			const auto converted_coord = convert_coord(coord);
			
			const auto fit_output = [](const auto& color) {
				using output_type = image_vec_ret_type<image_type, sample_type>;
#if defined(FLOOR_DEVICE_OPENCL) || defined(FLOOR_DEVICE_METAL) || defined(FLOOR_DEVICE_CUDA) || defined(FLOOR_DEVICE_VULKAN)
				if constexpr(!has_flag<IMAGE_TYPE::FLAG_DEPTH>(image_type)) {
					return output_type::fit(vector_n<sample_type, 4>::from_clang_vector(color));
				} else {
					return output_type::fit(color.x);
				}
#else
				return output_type::fit(color);
#endif
			};
#if defined(FLOOR_DEVICE_OPENCL) || defined(FLOOR_DEVICE_METAL) || defined(FLOOR_DEVICE_VULKAN)
#if !defined(FLOOR_DEVICE_METAL) // only constexpr with OpenCL/Vulkan
			constexpr
#endif
			const sampler_type smplr = default_sampler<coord_type, sample_linear, sample_repeat, sample_repeat_mirrored, compare_function>::value();
			return fit_output(opaque_image::read_image<sample_type>(r_img(), smplr, image_type, converted_coord, layer, sample, offset,
																	(!is_lod_float ? int32_t(lod) : 0), (!is_bias ? (is_lod_float ? lod : 0.0f) : bias), is_lod, is_lod_float, is_bias,
																	gradient.first, gradient.second, is_gradient,
																	compare_function, compare_value, is_compare));
#elif defined(FLOOR_DEVICE_CUDA)
			constexpr const auto cuda_tex_idx = cuda_sampler::sampler_index(is_int_coord<coord_type>() ? cuda_sampler::PIXEL : cuda_sampler::NORMALIZED,
																			sample_linear ? cuda_sampler::LINEAR : cuda_sampler::NEAREST,
																			(sample_repeat ? cuda_sampler::REPEAT :
																			 (sample_repeat_mirrored ? cuda_sampler::REPEAT_MIRRORED : cuda_sampler::CLAMP_TO_EDGE)),
																			(!is_compare ||
																			 compare_function == COMPARE_FUNCTION::ALWAYS ||
																			 compare_function == COMPARE_FUNCTION::NEVER) ?
																			cuda_sampler::NONE : (cuda_sampler::COMPARE_FUNCTION)compare_function);
			return fit_output(cuda_image::read_image<sample_type>(r_img_obj[cuda_tex_idx], image_type, converted_coord, layer, sample, offset,
																  (!is_lod_float ? int32_t(lod) : 0), (!is_bias ? (is_lod_float ? lod : 0.0f) : bias), is_lod, is_lod_float, is_bias,
																  gradient.first, gradient.second, is_gradient,
																  compare_function, compare_value, is_compare));
#elif defined(FLOOR_DEVICE_HOST_COMPUTE)
			if constexpr (!is_compare) {
				if constexpr (!sample_linear) {
					return fit_output(host_device_image<image_type, is_lod, is_lod_float, is_bias, sample_repeat,
									  sample_repeat_mirrored>::read((const host_device_image<image_type, is_lod, is_lod_float, is_bias, sample_repeat, sample_repeat_mirrored>*)r_img(),
																	converted_coord, offset, layer,
																	(!is_lod_float ? int32_t(lod) : 0),
																	(!is_bias ? (is_lod_float ? float(lod) : 0.0f) : bias)));
				} else {
					return fit_output(host_device_image<image_type, is_lod, is_lod_float, is_bias, sample_repeat,
									  sample_repeat_mirrored>::read_linear((const host_device_image<image_type, is_lod, is_lod_float, is_bias, sample_repeat, sample_repeat_mirrored>*)r_img(),
																		   converted_coord, offset, layer,
																		   (!is_lod_float ? int32_t(lod) : 0),
																		   (!is_bias ? (is_lod_float ? float(lod) : 0.0f) : bias)));
				}
			} else {
				if constexpr (!sample_linear) {
					return fit_output(host_device_image<image_type, is_lod, is_lod_float, is_bias, sample_repeat,
									  sample_repeat_mirrored>::compare((const host_device_image<image_type, is_lod, is_lod_float, is_bias, sample_repeat, sample_repeat_mirrored>*)r_img(),
																	   converted_coord, offset, layer,
																	   (!is_lod_float ? int32_t(lod) : 0),
																	   (!is_bias ? (is_lod_float ? float(lod) : 0.0f) : bias),
																	   compare_function, compare_value));
				} else {
					return fit_output(host_device_image<image_type, is_lod, is_lod_float, is_bias, sample_repeat,
									  sample_repeat_mirrored>::compare_linear((const host_device_image<image_type, is_lod, is_lod_float, is_bias, sample_repeat, sample_repeat_mirrored>*)r_img(),
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
		requires(!has_flag<IMAGE_TYPE::FLAG_ARRAY>(image_type) &&
				 !has_flag<IMAGE_TYPE::FLAG_MSAA>(image_type))
		auto read(const coord_type& coord, const offset_vec_type offset = {}, const float bias = 0.0f) const {
			return read_internal<false>(coord, 0, 0, offset, bias);
		}
		
		//! image read with nearest/point sampling and clamp-to-edge address mode (array, non-msaa)
		template <typename coord_type>
		requires(has_flag<IMAGE_TYPE::FLAG_ARRAY>(image_type) &&
				 !has_flag<IMAGE_TYPE::FLAG_MSAA>(image_type))
		auto read(const coord_type& coord, const uint32_t layer, const offset_vec_type offset = {}, const float bias = 0.0f) const {
			return read_internal<false>(coord, layer, 0, offset, bias);
		}
		
		//! image read with nearest/point sampling and clamp-to-edge address mode (non-array, msaa)
		template <typename coord_type>
		requires(!has_flag<IMAGE_TYPE::FLAG_ARRAY>(image_type) &&
				 has_flag<IMAGE_TYPE::FLAG_MSAA>(image_type))
		auto read(const coord_type& coord, const uint32_t sample, const offset_vec_type offset = {}, const float bias = 0.0f) const {
			return read_internal<false>(coord, 0, sample, offset, bias);
		}
		
		//! image read with nearest/point sampling and clamp-to-edge address mode (array, msaa)
		template <typename coord_type>
		requires(has_flag<IMAGE_TYPE::FLAG_ARRAY>(image_type) &&
				 has_flag<IMAGE_TYPE::FLAG_MSAA>(image_type))
		auto read(const coord_type& coord, const uint32_t layer, const uint32_t sample, const offset_vec_type offset = {}, const float bias = 0.0f) const {
			return read_internal<false>(coord, layer, sample, offset, bias);
		}
		
		//! image read with nearest/point sampling and repeat address mode (non-array, non-msaa)
		template <typename coord_type>
		requires(!has_flag<IMAGE_TYPE::FLAG_ARRAY>(image_type) &&
				 !has_flag<IMAGE_TYPE::FLAG_MSAA>(image_type))
		auto read_repeat(const coord_type& coord, const offset_vec_type offset = {}, const float bias = 0.0f) const {
			return read_internal<false, true>(coord, 0, 0, offset, bias);
		}
		
		//! image read with nearest/point sampling and repeat address mode (array, non-msaa)
		template <typename coord_type>
		requires(has_flag<IMAGE_TYPE::FLAG_ARRAY>(image_type) &&
				 !has_flag<IMAGE_TYPE::FLAG_MSAA>(image_type))
		auto read_repeat(const coord_type& coord, const uint32_t layer, const offset_vec_type offset = {}, const float bias = 0.0f) const {
			return read_internal<false, true>(coord, layer, 0, offset, bias);
		}
		
		//! image read with nearest/point sampling and repeat address mode (non-array, msaa)
		template <typename coord_type>
		requires(!has_flag<IMAGE_TYPE::FLAG_ARRAY>(image_type) &&
				 has_flag<IMAGE_TYPE::FLAG_MSAA>(image_type))
		auto read_repeat(const coord_type& coord, const uint32_t sample, const offset_vec_type offset = {}, const float bias = 0.0f) const {
			return read_internal<false, true>(coord, 0, sample, offset, bias);
		}
		
		//! image read with nearest/point sampling and repeat address mode (array, msaa)
		template <typename coord_type>
		requires(has_flag<IMAGE_TYPE::FLAG_ARRAY>(image_type) &&
				 has_flag<IMAGE_TYPE::FLAG_MSAA>(image_type))
		auto read_repeat(const coord_type& coord, const uint32_t layer, const uint32_t sample, const offset_vec_type offset = {}, const float bias = 0.0f) const {
			return read_internal<false, true>(coord, layer, sample, offset, bias);
		}
		
		//! image read with nearest/point sampling and repeat-mirrored address mode (non-array, non-msaa)
		template <typename coord_type>
		requires(!has_flag<IMAGE_TYPE::FLAG_ARRAY>(image_type) &&
				 !has_flag<IMAGE_TYPE::FLAG_MSAA>(image_type))
		auto read_repeat_mirrored(const coord_type& coord, const offset_vec_type offset = {}, const float bias = 0.0f) const {
			return read_internal<false, false, true>(coord, 0, 0, offset, bias);
		}
		
		//! image read with nearest/point sampling and repeat-mirrored address mode (array, non-msaa)
		template <typename coord_type>
		requires(has_flag<IMAGE_TYPE::FLAG_ARRAY>(image_type) &&
				 !has_flag<IMAGE_TYPE::FLAG_MSAA>(image_type))
		auto read_repeat_mirrored(const coord_type& coord, const uint32_t layer, const offset_vec_type offset = {}, const float bias = 0.0f) const {
			return read_internal<false, false, true>(coord, layer, 0, offset, bias);
		}
		
		//! image read with nearest/point sampling and repeat-mirrored address mode (non-array, msaa)
		template <typename coord_type>
		requires(!has_flag<IMAGE_TYPE::FLAG_ARRAY>(image_type) &&
				 has_flag<IMAGE_TYPE::FLAG_MSAA>(image_type))
		auto read_repeat_mirrored(const coord_type& coord, const uint32_t sample, const offset_vec_type offset = {}, const float bias = 0.0f) const {
			return read_internal<false, false, true>(coord, 0, sample, offset, bias);
		}
		
		//! image read with nearest/point sampling and repeat-mirrored address mode (array, msaa)
		template <typename coord_type>
		requires(has_flag<IMAGE_TYPE::FLAG_ARRAY>(image_type) &&
				 has_flag<IMAGE_TYPE::FLAG_MSAA>(image_type))
		auto read_repeat_mirrored(const coord_type& coord, const uint32_t layer, const uint32_t sample, const offset_vec_type offset = {}, const float bias = 0.0f) const {
			return read_internal<false, false, true>(coord, layer, sample, offset, bias);
		}
		
		//! image read with linear sampling and clamp-to-edge address mode (non-array, non-msaa)
		template <typename coord_type>
		requires(!has_flag<IMAGE_TYPE::FLAG_ARRAY>(image_type) &&
				 !has_flag<IMAGE_TYPE::FLAG_MSAA>(image_type))
		auto read_linear(const coord_type& coord, const offset_vec_type offset = {}, const float bias = 0.0f) const {
			return read_internal<true>(coord, 0, 0, offset, bias);
		}
		
		//! image read with linear sampling and clamp-to-edge address mode (array, non-msaa)
		template <typename coord_type>
		requires(has_flag<IMAGE_TYPE::FLAG_ARRAY>(image_type) &&
				 !has_flag<IMAGE_TYPE::FLAG_MSAA>(image_type))
		auto read_linear(const coord_type& coord, const uint32_t layer, const offset_vec_type offset = {}, const float bias = 0.0f) const {
			return read_internal<true>(coord, layer, 0, offset, bias);
		}
		
		//! image read with linear sampling and clamp-to-edge address mode (non-array, msaa)
		template <typename coord_type>
		requires(!has_flag<IMAGE_TYPE::FLAG_ARRAY>(image_type) &&
				 has_flag<IMAGE_TYPE::FLAG_MSAA>(image_type))
		auto read_linear(const coord_type& coord, const uint32_t sample, const offset_vec_type offset = {}, const float bias = 0.0f) const {
			return read_internal<true>(coord, 0, sample, offset, bias);
		}
		
		//! image read with linear sampling and clamp-to-edge address mode (array, msaa)
		template <typename coord_type>
		requires(has_flag<IMAGE_TYPE::FLAG_ARRAY>(image_type) &&
				 has_flag<IMAGE_TYPE::FLAG_MSAA>(image_type))
		auto read_linear(const coord_type& coord, const uint32_t layer, const uint32_t sample, const offset_vec_type offset = {}, const float bias = 0.0f) const {
			return read_internal<true>(coord, layer, sample, offset, bias);
		}
		
		//! image read with linear sampling and repeat address mode (non-array, non-msaa)
		template <typename coord_type>
		requires(!has_flag<IMAGE_TYPE::FLAG_ARRAY>(image_type) &&
				 !has_flag<IMAGE_TYPE::FLAG_MSAA>(image_type))
		auto read_linear_repeat(const coord_type& coord, const offset_vec_type offset = {}, const float bias = 0.0f) const {
			return read_internal<true, true>(coord, 0, 0, offset, bias);
		}
		
		//! image read with linear sampling and repeat address mode (array, non-msaa)
		template <typename coord_type>
		requires(has_flag<IMAGE_TYPE::FLAG_ARRAY>(image_type) &&
				 !has_flag<IMAGE_TYPE::FLAG_MSAA>(image_type))
		auto read_linear_repeat(const coord_type& coord, const uint32_t layer, const offset_vec_type offset = {}, const float bias = 0.0f) const {
			return read_internal<true, true>(coord, layer, 0, offset, bias);
		}
		
		//! image read with linear sampling and repeat address mode (non-array, msaa)
		template <typename coord_type>
		requires(!has_flag<IMAGE_TYPE::FLAG_ARRAY>(image_type) &&
				 has_flag<IMAGE_TYPE::FLAG_MSAA>(image_type))
		auto read_linear_repeat(const coord_type& coord, const uint32_t sample, const offset_vec_type offset = {}, const float bias = 0.0f) const {
			return read_internal<true, true>(coord, 0, sample, offset, bias);
		}
		
		//! image read with linear sampling and repeat address mode (array, msaa)
		template <typename coord_type>
		requires(has_flag<IMAGE_TYPE::FLAG_ARRAY>(image_type) &&
				 has_flag<IMAGE_TYPE::FLAG_MSAA>(image_type))
		auto read_linear_repeat(const coord_type& coord, const uint32_t layer, const uint32_t sample, const offset_vec_type offset = {}, const float bias = 0.0f) const {
			return read_internal<true, true>(coord, layer, sample, offset, bias);
		}
		
		//! image read with linear sampling and repeat-mirrored address mode (non-array, non-msaa)
		template <typename coord_type>
		requires(!has_flag<IMAGE_TYPE::FLAG_ARRAY>(image_type) &&
				 !has_flag<IMAGE_TYPE::FLAG_MSAA>(image_type))
		auto read_linear_repeat_mirrored(const coord_type& coord, const offset_vec_type offset = {}, const float bias = 0.0f) const {
			return read_internal<true, false, true>(coord, 0, 0, offset, bias);
		}
		
		//! image read with linear sampling and repeat-mirrored address mode (array, non-msaa)
		template <typename coord_type>
		requires(has_flag<IMAGE_TYPE::FLAG_ARRAY>(image_type) &&
				 !has_flag<IMAGE_TYPE::FLAG_MSAA>(image_type))
		auto read_linear_repeat_mirrored(const coord_type& coord, const uint32_t layer, const offset_vec_type offset = {}, const float bias = 0.0f) const {
			return read_internal<true, false, true>(coord, layer, 0, offset, bias);
		}
		
		//! image read with linear sampling and repeat-mirrored address mode (non-array, msaa)
		template <typename coord_type>
		requires(!has_flag<IMAGE_TYPE::FLAG_ARRAY>(image_type) &&
				 has_flag<IMAGE_TYPE::FLAG_MSAA>(image_type))
		auto read_linear_repeat_mirrored(const coord_type& coord, const uint32_t sample, const offset_vec_type offset = {}, const float bias = 0.0f) const {
			return read_internal<true, false, true>(coord, 0, sample, offset, bias);
		}
		
		//! image read with linear sampling and repeat-mirrored address mode (array, msaa)
		template <typename coord_type>
		requires(has_flag<IMAGE_TYPE::FLAG_ARRAY>(image_type) &&
				 has_flag<IMAGE_TYPE::FLAG_MSAA>(image_type))
		auto read_linear_repeat_mirrored(const coord_type& coord, const uint32_t layer, const uint32_t sample, const offset_vec_type offset = {}, const float bias = 0.0f) const {
			return read_internal<true, false, true>(coord, layer, sample, offset, bias);
		}
		
		//! image read at an explicit lod level with nearest/point sampling and clamp-to-edge address mode (non-array)
		template <typename coord_type, typename lod_type>
		requires(!has_flag<IMAGE_TYPE::FLAG_ARRAY>(image_type) &&
				 !has_flag<IMAGE_TYPE::FLAG_MSAA>(image_type))
		auto read_lod(const coord_type& coord, const lod_type lod, const offset_vec_type offset = {}) const {
			return read_internal<false, false, false, true>(coord, 0, 0, offset, 0.0f, lod);
		}
		
		//! image read at an explicit lod level with nearest/point sampling and clamp-to-edge address mode (array)
		template <typename coord_type, typename lod_type>
		requires(has_flag<IMAGE_TYPE::FLAG_ARRAY>(image_type) &&
				 !has_flag<IMAGE_TYPE::FLAG_MSAA>(image_type))
		auto read_lod(const coord_type& coord, const uint32_t layer, const lod_type lod, const offset_vec_type offset = {}) const {
			return read_internal<false, false, false, true>(coord, layer, 0, offset, 0.0f, lod);
		}
		
		//! image read at an explicit lod level with nearest/point sampling and repeat address mode (non-array)
		template <typename coord_type, typename lod_type>
		requires(!has_flag<IMAGE_TYPE::FLAG_ARRAY>(image_type) &&
				 !has_flag<IMAGE_TYPE::FLAG_MSAA>(image_type))
		auto read_lod_repeat(const coord_type& coord, const lod_type lod, const offset_vec_type offset = {}) const {
			return read_internal<false, true, false, true>(coord, 0, 0, offset, 0.0f, lod);
		}
		
		//! image read at an explicit lod level with nearest/point sampling and repeat address mode (array)
		template <typename coord_type, typename lod_type>
		requires(has_flag<IMAGE_TYPE::FLAG_ARRAY>(image_type) &&
				 !has_flag<IMAGE_TYPE::FLAG_MSAA>(image_type))
		auto read_lod_repeat(const coord_type& coord, const uint32_t layer, const lod_type lod, const offset_vec_type offset = {}) const {
			return read_internal<false, true, false, true>(coord, layer, 0, offset, 0.0f, lod);
		}
		
		//! image read at an explicit lod level with nearest/point sampling and repeat-mirrored address mode (non-array)
		template <typename coord_type, typename lod_type>
		requires(!has_flag<IMAGE_TYPE::FLAG_ARRAY>(image_type) &&
				 !has_flag<IMAGE_TYPE::FLAG_MSAA>(image_type))
		auto read_lod_repeat_mirrored(const coord_type& coord, const lod_type lod, const offset_vec_type offset = {}) const {
			return read_internal<false, false, true, true>(coord, 0, 0, offset, 0.0f, lod);
		}
		
		//! image read at an explicit lod level with nearest/point sampling and repeat-mirrored address mode (array)
		template <typename coord_type, typename lod_type>
		requires(has_flag<IMAGE_TYPE::FLAG_ARRAY>(image_type) &&
				 !has_flag<IMAGE_TYPE::FLAG_MSAA>(image_type))
		auto read_lod_repeat_mirrored(const coord_type& coord, const uint32_t layer, const lod_type lod, const offset_vec_type offset = {}) const {
			return read_internal<false, false, true, true>(coord, layer, 0, offset, 0.0f, lod);
		}
		
		//! image read at an explicit lod level with linear sampling and clamp-to-edge address mode (non-array)
		template <typename coord_type, typename lod_type>
		requires(!has_flag<IMAGE_TYPE::FLAG_ARRAY>(image_type) &&
				 !has_flag<IMAGE_TYPE::FLAG_MSAA>(image_type))
		auto read_lod_linear(const coord_type& coord, const lod_type lod, const offset_vec_type offset = {}) const {
			return read_internal<true, false, false, true>(coord, 0, 0, offset, 0.0f, lod);
		}
		
		//! image read at an explicit lod level with linear sampling and clamp-to-edge address mode (array)
		template <typename coord_type, typename lod_type>
		requires(has_flag<IMAGE_TYPE::FLAG_ARRAY>(image_type) &&
				 !has_flag<IMAGE_TYPE::FLAG_MSAA>(image_type))
		auto read_lod_linear(const coord_type& coord, const uint32_t layer, const lod_type lod, const offset_vec_type offset = {}) const {
			return read_internal<true, false, false, true>(coord, layer, 0, offset, 0.0f, lod);
		}
		
		//! image read at an explicit lod level with linear sampling and repeat address mode (non-array)
		template <typename coord_type, typename lod_type>
		requires(!has_flag<IMAGE_TYPE::FLAG_ARRAY>(image_type) &&
				 !has_flag<IMAGE_TYPE::FLAG_MSAA>(image_type))
		auto read_lod_linear_repeat(const coord_type& coord, const lod_type lod, const offset_vec_type offset = {}) const {
			return read_internal<true, true, false, true>(coord, 0, 0, offset, 0.0f, lod);
		}
		
		//! image read at an explicit lod level with linear sampling and repeat address mode (array)
		template <typename coord_type, typename lod_type>
		requires(has_flag<IMAGE_TYPE::FLAG_ARRAY>(image_type) &&
				 !has_flag<IMAGE_TYPE::FLAG_MSAA>(image_type))
		auto read_lod_linear_repeat(const coord_type& coord, const uint32_t layer, const lod_type lod, const offset_vec_type offset = {}) const {
			return read_internal<true, true, false, true>(coord, layer, 0, offset, 0.0f, lod);
		}
		
		//! image read at an explicit lod level with linear sampling and repeat-mirrored address mode (non-array)
		template <typename coord_type, typename lod_type>
		requires(!has_flag<IMAGE_TYPE::FLAG_ARRAY>(image_type) &&
				 !has_flag<IMAGE_TYPE::FLAG_MSAA>(image_type))
		auto read_lod_linear_repeat_mirrored(const coord_type& coord, const lod_type lod, const offset_vec_type offset = {}) const {
			return read_internal<true, false, true, true>(coord, 0, 0, offset, 0.0f, lod);
		}
		
		//! image read at an explicit lod level with linear sampling and repeat-mirrored address mode (array)
		template <typename coord_type, typename lod_type>
		requires(has_flag<IMAGE_TYPE::FLAG_ARRAY>(image_type) &&
				 !has_flag<IMAGE_TYPE::FLAG_MSAA>(image_type))
		auto read_lod_linear_repeat_mirrored(const coord_type& coord, const uint32_t layer, const lod_type lod, const offset_vec_type offset = {}) const {
			return read_internal<true, false, true, true>(coord, layer, 0, offset, 0.0f, lod);
		}
		
		//! image read with an explicit gradient (dPdx, dPdy) with nearest/point sampling and clamp-to-edge address mode (non-array)
		template <typename coord_type>
		requires(!has_flag<IMAGE_TYPE::FLAG_ARRAY>(image_type) &&
				 !has_flag<IMAGE_TYPE::FLAG_MSAA>(image_type))
		auto read_gradient(const coord_type& coord,
						   const std::pair<gradient_vec_type, gradient_vec_type> gradient,
						   const offset_vec_type offset = {}) const {
			return read_internal<false, false, false, false, true>(coord, 0, 0, offset, 0.0f, 0, gradient);
		}
		
		//! image read with an explicit gradient (dPdx, dPdy) with nearest/point sampling and clamp-to-edge address mode (array)
		template <typename coord_type>
		requires(has_flag<IMAGE_TYPE::FLAG_ARRAY>(image_type) &&
				 !has_flag<IMAGE_TYPE::FLAG_MSAA>(image_type))
		auto read_gradient(const coord_type& coord, const uint32_t layer,
						   const std::pair<gradient_vec_type, gradient_vec_type> gradient,
						   const offset_vec_type offset = {}) const {
			return read_internal<false, false, false, false, true>(coord, layer, 0, offset, 0.0f, 0, gradient);
		}
		
		//! image read with an explicit gradient (dPdx, dPdy) with nearest/point sampling and repeat address mode (non-array)
		template <typename coord_type>
		requires(!has_flag<IMAGE_TYPE::FLAG_ARRAY>(image_type) &&
				 !has_flag<IMAGE_TYPE::FLAG_MSAA>(image_type))
		auto read_gradient_repeat(const coord_type& coord,
								  const std::pair<gradient_vec_type, gradient_vec_type> gradient,
								  const offset_vec_type offset = {}) const {
			return read_internal<false, true, false, false, true>(coord, 0, 0, offset, 0.0f, 0, gradient);
		}
		
		//! image read with an explicit gradient (dPdx, dPdy) with nearest/point sampling and repeat address mode (array)
		template <typename coord_type>
		requires(has_flag<IMAGE_TYPE::FLAG_ARRAY>(image_type) &&
				 !has_flag<IMAGE_TYPE::FLAG_MSAA>(image_type))
		auto read_gradient_repeat(const coord_type& coord, const uint32_t layer,
								  const std::pair<gradient_vec_type, gradient_vec_type> gradient,
								  const offset_vec_type offset = {}) const {
			return read_internal<false, true, false, false, true>(coord, layer, 0, offset, 0.0f, 0, gradient);
		}
		
		//! image read with an explicit gradient (dPdx, dPdy) with nearest/point sampling and repeat-mirrored address mode (non-array)
		template <typename coord_type>
		requires(!has_flag<IMAGE_TYPE::FLAG_ARRAY>(image_type) &&
				 !has_flag<IMAGE_TYPE::FLAG_MSAA>(image_type))
		auto read_gradient_repeat_mirrored(const coord_type& coord,
										   const std::pair<gradient_vec_type, gradient_vec_type> gradient,
										   const offset_vec_type offset = {}) const {
			return read_internal<false, false, true, false, true>(coord, 0, 0, offset, 0.0f, 0, gradient);
		}
		
		//! image read with an explicit gradient (dPdx, dPdy) with nearest/point sampling and repeat-mirrored address mode (array)
		template <typename coord_type>
		requires(has_flag<IMAGE_TYPE::FLAG_ARRAY>(image_type) &&
				 !has_flag<IMAGE_TYPE::FLAG_MSAA>(image_type))
		auto read_gradient_repeat_mirrored(const coord_type& coord, const uint32_t layer,
										   const std::pair<gradient_vec_type, gradient_vec_type> gradient,
										   const offset_vec_type offset = {}) const {
			return read_internal<false, false, true, false, true>(coord, layer, 0, offset, 0.0f, 0, gradient);
		}
		
		//! image read with an explicit gradient (dPdx, dPdy) with linear sampling and clamp-to-edge address mode (non-array)
		template <typename coord_type>
		requires(!has_flag<IMAGE_TYPE::FLAG_ARRAY>(image_type) &&
				 !has_flag<IMAGE_TYPE::FLAG_MSAA>(image_type))
		auto read_gradient_linear(const coord_type& coord,
								  const std::pair<gradient_vec_type, gradient_vec_type> gradient,
								  const offset_vec_type offset = {}) const {
			return read_internal<true, false, false, false, true>(coord, 0, 0, offset, 0.0f, 0, gradient);
		}
		
		//! image read with an explicit gradient (dPdx, dPdy) with linear sampling and clamp-to-edge address mode (array)
		template <typename coord_type>
		requires(has_flag<IMAGE_TYPE::FLAG_ARRAY>(image_type) &&
				 !has_flag<IMAGE_TYPE::FLAG_MSAA>(image_type))
		auto read_gradient_linear(const coord_type& coord, const uint32_t layer,
								  const std::pair<gradient_vec_type, gradient_vec_type> gradient,
								  const offset_vec_type offset = {}) const {
			return read_internal<true, false, false, false, true>(coord, layer, 0, offset, 0.0f, 0, gradient);
		}
		
		//! image read with an explicit gradient (dPdx, dPdy) with linear sampling and repeat address mode (non-array)
		template <typename coord_type>
		requires(!has_flag<IMAGE_TYPE::FLAG_ARRAY>(image_type) &&
				 !has_flag<IMAGE_TYPE::FLAG_MSAA>(image_type))
		auto read_gradient_linear_repeat(const coord_type& coord,
										 const std::pair<gradient_vec_type, gradient_vec_type> gradient,
										 const offset_vec_type offset = {}) const {
			return read_internal<true, true, false, false, true>(coord, 0, 0, offset, 0.0f, 0, gradient);
		}
		
		//! image read with an explicit gradient (dPdx, dPdy) with linear sampling and repeat address mode (array)
		template <typename coord_type>
		requires(has_flag<IMAGE_TYPE::FLAG_ARRAY>(image_type) &&
				 !has_flag<IMAGE_TYPE::FLAG_MSAA>(image_type))
		auto read_gradient_linear_repeat(const coord_type& coord, const uint32_t layer,
										 const std::pair<gradient_vec_type, gradient_vec_type> gradient,
										 const offset_vec_type offset = {}) const {
			return read_internal<true, true, false, false, true>(coord, layer, 0, offset, 0.0f, 0, gradient);
		}
		
		//! image read with an explicit gradient (dPdx, dPdy) with linear sampling and repeat-mirrored address mode (non-array)
		template <typename coord_type>
		requires(!has_flag<IMAGE_TYPE::FLAG_ARRAY>(image_type) &&
				 !has_flag<IMAGE_TYPE::FLAG_MSAA>(image_type))
		auto read_gradient_linear_repeat_mirrored(const coord_type& coord,
												  const std::pair<gradient_vec_type, gradient_vec_type> gradient,
												  const offset_vec_type offset = {}) const {
			return read_internal<true, false, true, false, true>(coord, 0, 0, offset, 0.0f, 0, gradient);
		}
		
		//! image read with an explicit gradient (dPdx, dPdy) with linear sampling and repeat-mirrored address mode (array)
		template <typename coord_type>
		requires(has_flag<IMAGE_TYPE::FLAG_ARRAY>(image_type) &&
				 !has_flag<IMAGE_TYPE::FLAG_MSAA>(image_type))
		auto read_gradient_linear_repeat_mirrored(const coord_type& coord, const uint32_t layer,
												  const std::pair<gradient_vec_type, gradient_vec_type> gradient,
												  const offset_vec_type offset = {}) const {
			return read_internal<true, false, true, false, true>(coord, layer, 0, offset, 0.0f, 0, gradient);
		}
		
		//////////////////////////////////////////
		// depth compare functions:
		// * Metal: has full support for this
		// * Host-Compute: has full support for this
		// * Vulkan: has full support for this
		// * CUDA: technically supports depth compare PTX instructions, but no way to set the compare function?! (using s/w compare for now)
		// * OpenCL/SPIR: doesn't have support for this, compare will be performed in s/w
		
		//! image depth compare read with nearest/point sampling (non-array)
		template <COMPARE_FUNCTION compare_function, typename coord_type>
		requires(!has_flag<IMAGE_TYPE::FLAG_ARRAY>(image_type))
		auto compare(const coord_type& coord, const float& compare_value, const offset_vec_type offset = {}, const float bias = 0.0f) const {
			return read_internal<false, false, false, false, false, true, compare_function>(coord, 0, 0, offset, bias, 0, {}, compare_value);
		}
		
		//! image depth compare read with nearest/point sampling (array)
		template <COMPARE_FUNCTION compare_function, typename coord_type>
		requires(has_flag<IMAGE_TYPE::FLAG_ARRAY>(image_type))
		auto compare(const coord_type& coord, const uint32_t layer, const float& compare_value, const offset_vec_type offset = {}, const float bias = 0.0f) const {
			return read_internal<false, false, false, false, false, true, compare_function>(coord, layer, 0, offset, bias, 0, {}, compare_value);
		}
		
		//! image depth compare read with linear sampling (non-array)
		template <COMPARE_FUNCTION compare_function, typename coord_type>
		requires(!has_flag<IMAGE_TYPE::FLAG_ARRAY>(image_type))
		auto compare_linear(const coord_type& coord, const float& compare_value, const offset_vec_type offset = {}, const float bias = 0.0f) const {
			return read_internal<true, false, false, false, false, true, compare_function>(coord, 0, 0, offset, bias, 0, {}, compare_value);
		}
		
		//! image depth compare read with linear sampling (array)
		template <COMPARE_FUNCTION compare_function, typename coord_type>
		requires(has_flag<IMAGE_TYPE::FLAG_ARRAY>(image_type))
		auto compare_linear(const coord_type& coord, const uint32_t layer, const float& compare_value, const offset_vec_type offset = {}, const float bias = 0.0f) const {
			return read_internal<true, false, false, false, false, true, compare_function>(coord, layer, 0, offset, bias, 0, {}, compare_value);
		}
		
		//! image depth compare read at an explicit lod level with nearest/point sampling (non-array)
		template <COMPARE_FUNCTION compare_function, typename coord_type, typename lod_type>
		requires(!has_flag<IMAGE_TYPE::FLAG_ARRAY>(image_type))
		auto compare_lod(const coord_type& coord, const float& compare_value, const lod_type lod, const offset_vec_type offset = {}) const {
			return read_internal<false, false, false, true, false, true, compare_function>(coord, 0, 0, offset, 0.0f, lod, {}, compare_value);
		}
		
		//! image depth compare read at an explicit lod level with nearest/point sampling (array)
		template <COMPARE_FUNCTION compare_function, typename coord_type, typename lod_type>
		requires(has_flag<IMAGE_TYPE::FLAG_ARRAY>(image_type))
		auto compare_lod(const coord_type& coord, const uint32_t layer, const float& compare_value, const lod_type lod, const offset_vec_type offset = {}) const {
			return read_internal<false, false, false, true, false, true, compare_function>(coord, layer, 0, offset, 0.0f, lod, {}, compare_value);
		}
		
		//! image depth compare read at an explicit lod level with linear sampling (non-array)
		template <COMPARE_FUNCTION compare_function, typename coord_type, typename lod_type>
		requires(!has_flag<IMAGE_TYPE::FLAG_ARRAY>(image_type))
		auto compare_lod_linear(const coord_type& coord, const float& compare_value, const lod_type lod, const offset_vec_type offset = {}) const {
			return read_internal<true, false, false, true, false, true, compare_function>(coord, 0, 0, offset, 0.0f, lod, {}, compare_value);
		}
		
		//! image depth compare read at an explicit lod level with linear sampling (array)
		template <COMPARE_FUNCTION compare_function, typename coord_type, typename lod_type>
		requires(has_flag<IMAGE_TYPE::FLAG_ARRAY>(image_type))
		auto compare_lod_linear(const coord_type& coord, const uint32_t layer, const float& compare_value, const lod_type lod, const offset_vec_type offset = {}) const {
			return read_internal<true, false, false, true, false, true, compare_function>(coord, layer, 0, offset, 0.0f, lod, {}, compare_value);
		}
		
		//! image depth compare read with an explicit gradient (dPdx, dPdy) with nearest/point sampling (non-array)
		template <COMPARE_FUNCTION compare_function, typename coord_type>
		requires(!has_flag<IMAGE_TYPE::FLAG_ARRAY>(image_type))
		auto compare_gradient(const coord_type& coord, const float& compare_value,
							  const std::pair<gradient_vec_type, gradient_vec_type> gradient,
							  const offset_vec_type offset = {}) const {
			return read_internal<false, false, false, false, true, true, compare_function>(coord, 0, 0, offset, 0.0f, 0, gradient, compare_value);
		}
		
		//! image depth compare read with an explicit gradient (dPdx, dPdy) with nearest/point sampling (array)
		template <COMPARE_FUNCTION compare_function, typename coord_type>
		requires(has_flag<IMAGE_TYPE::FLAG_ARRAY>(image_type))
		auto compare_gradient(const coord_type& coord, const uint32_t layer, const float& compare_value,
							  const std::pair<gradient_vec_type, gradient_vec_type> gradient,
							  const offset_vec_type offset = {}) const {
			return read_internal<false, false, false, false, true, true, compare_function>(coord, layer, 0, offset, 0.0f, 0, gradient, compare_value);
		}
		
		//! image depth compare read with an explicit gradient (dPdx, dPdy) with linear sampling (non-array)
		template <COMPARE_FUNCTION compare_function, typename coord_type>
		requires(!has_flag<IMAGE_TYPE::FLAG_ARRAY>(image_type))
		auto compare_gradient_linear(const coord_type& coord, const float& compare_value,
									 const std::pair<gradient_vec_type, gradient_vec_type> gradient,
									 const offset_vec_type offset = {}) const {
			return read_internal<true, false, false, false, true, true, compare_function>(coord, 0, 0, offset, 0.0f, 0, gradient, compare_value);
		}
		
		//! image depth compare read with an explicit gradient (dPdx, dPdy) with linear sampling (array)
		template <COMPARE_FUNCTION compare_function, typename coord_type>
		requires(has_flag<IMAGE_TYPE::FLAG_ARRAY>(image_type))
		auto compare_gradient_linear(const coord_type& coord, const uint32_t layer, const float& compare_value,
									 const std::pair<gradient_vec_type, gradient_vec_type> gradient,
									 const offset_vec_type offset = {}) const {
			return read_internal<true, false, false, false, true, true, compare_function>(coord, layer, 0, offset, 0.0f, 0, gradient, compare_value);
		}
		
	public: // image write functions
		//! internal write function
		template <bool is_lod = false, typename coord_type>
		requires (is_writable())
		floor_inline_always void write_internal(const coord_type& coord,
												const uint32_t layer,
												const uint32_t lod,
												const vector_sample_type& data) {
			// sample type must be 32-bit or 16-bit float/int/uint
			constexpr const bool is_float = is_sample_float(image_type);
			constexpr const bool is_half = is_sample_half(image_type);
			constexpr const bool is_int = is_sample_int(image_type);
			constexpr const bool is_short = is_sample_short(image_type);
			constexpr const bool is_uint = is_sample_uint(image_type);
			constexpr const bool is_ushort = is_sample_ushort(image_type);
			static_assert(is_float || is_half || is_int || is_short || is_uint || is_ushort, "invalid sample type");
			// depth data type must always be a float
			constexpr const bool is_depth = has_flag<IMAGE_TYPE::FLAG_DEPTH>(image_type);
			static_assert(!is_depth || (is_depth && is_float && std::is_same_v<vector_sample_type, float>), "depth value must always be a float");
			
			// backend specific coordinate conversion (should always be int here)
			const auto converted_coord = convert_coord(coord);
			
			// convert input data (vector) to a vector4 (if color or depth on resp. backends) or float (if depth on resp. backends)
#if defined(FLOOR_DEVICE_METAL) || defined(FLOOR_DEVICE_HOST_COMPUTE)
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
#if defined(FLOOR_DEVICE_OPENCL)
			if constexpr (is_float) opaque_image::write_image_float(w_img(), image_type, converted_coord, layer, lod, is_lod, (float4)converted_data);
			else if constexpr (is_half) opaque_image::write_image_half(w_img(), image_type, converted_coord, layer, lod, is_lod, (half4)converted_data);
			else if constexpr (is_int) opaque_image::write_image_int(w_img(), image_type, converted_coord, layer, lod, is_lod, (int4)converted_data);
			else if constexpr (is_short) opaque_image::write_image_short(w_img(), image_type, converted_coord, layer, lod, is_lod, (short4)converted_data);
			else if constexpr (is_uint) opaque_image::write_image_uint(w_img(), image_type, converted_coord, layer, lod, is_lod, (uint4)converted_data);
			else if constexpr (is_ushort) opaque_image::write_image_ushort(w_img(), image_type, converted_coord, layer, lod, is_lod, (ushort4)converted_data);
#elif defined(FLOOR_DEVICE_METAL)
			if constexpr (is_depth) opaque_image::write_image_float(w_img(), image_type, converted_coord, layer, lod, is_lod, converted_data);
			else if constexpr (is_float) opaque_image::write_image_float(w_img(), image_type, converted_coord, layer, lod, is_lod, (float4)converted_data);
			else if constexpr (is_half) opaque_image::write_image_half(w_img(), image_type, converted_coord, layer, lod, is_lod, (half4)converted_data);
			else if constexpr (is_int) opaque_image::write_image_int(w_img(), image_type, converted_coord, layer, lod, is_lod, (int4)converted_data);
			else if constexpr (is_short) opaque_image::write_image_short(w_img(), image_type, converted_coord, layer, lod, is_lod, (short4)converted_data);
			else if constexpr (is_uint) opaque_image::write_image_uint(w_img(), image_type, converted_coord, layer, lod, is_lod, (uint4)converted_data);
			else if constexpr (is_ushort) opaque_image::write_image_ushort(w_img(), image_type, converted_coord, layer, lod, is_lod, (ushort4)converted_data);
#elif defined(FLOOR_DEVICE_VULKAN)
			if constexpr (!is_lod) {
				if constexpr (is_float) opaque_image::write_image_float(w_img(), image_type, converted_coord, layer, 0, false, (float4)converted_data);
				else if constexpr (is_half) opaque_image::write_image_half(w_img(), image_type, converted_coord, layer, 0, false, (half4)converted_data);
				else if constexpr (is_int) opaque_image::write_image_int(w_img(), image_type, converted_coord, layer, 0, false, (int4)converted_data);
				else if constexpr (is_short) opaque_image::write_image_short(w_img(), image_type, converted_coord, layer, 0, false, (short4)converted_data);
				else if constexpr (is_uint) opaque_image::write_image_uint(w_img(), image_type, converted_coord, layer, 0, false, (uint4)converted_data);
				else if constexpr (is_ushort) opaque_image::write_image_ushort(w_img(), image_type, converted_coord, layer, 0, false, (ushort4)converted_data);
			} else {
				if constexpr (is_float) opaque_image::write_image_float(w_img(lod), image_type, converted_coord, layer, lod, is_lod, (float4)converted_data);
				else if constexpr (is_half) opaque_image::write_image_half(w_img(lod), image_type, converted_coord, layer, lod, is_lod, (half4)converted_data);
				else if constexpr (is_int) opaque_image::write_image_int(w_img(lod), image_type, converted_coord, layer, lod, is_lod, (int4)converted_data);
				else if constexpr (is_short) opaque_image::write_image_short(w_img(lod), image_type, converted_coord, layer, lod, is_lod, (short4)converted_data);
				else if constexpr (is_uint) opaque_image::write_image_uint(w_img(lod), image_type, converted_coord, layer, lod, is_lod, (uint4)converted_data);
				else if constexpr (is_ushort) opaque_image::write_image_ushort(w_img(lod), image_type, converted_coord, layer, lod, is_lod, (ushort4)converted_data);
			}
#elif defined(FLOOR_DEVICE_CUDA)
			if constexpr (!is_lod) {
				if constexpr (is_float) cuda_image::write_float<image_type>(w_img_obj, runtime_image_type, converted_coord, layer, 0, false, (float4)converted_data);
				else if constexpr (is_half) cuda_image::write_half<image_type>(w_img_obj, runtime_image_type, converted_coord, layer, 0, false, (half4)converted_data);
				else if constexpr (is_int) cuda_image::write_int<image_type>(w_img_obj, runtime_image_type, converted_coord, layer, 0, false, (int4)converted_data);
				else if constexpr (is_short) cuda_image::write_short<image_type>(w_img_obj, runtime_image_type, converted_coord, layer, 0, false, (short4)converted_data);
				else if constexpr (is_uint) cuda_image::write_uint<image_type>(w_img_obj, runtime_image_type, converted_coord, layer, 0, false, (uint4)converted_data);
				else if constexpr (is_ushort) cuda_image::write_ushort<image_type>(w_img_obj, runtime_image_type, converted_coord, layer, 0, false, (ushort4)converted_data);
			} else {
				if constexpr (is_float) cuda_image::write_float<image_type>(w_img_lod_obj[lod], runtime_image_type, converted_coord, layer, lod, is_lod, (float4)converted_data);
				else if constexpr (is_half) cuda_image::write_half<image_type>(w_img_lod_obj[lod], runtime_image_type, converted_coord, layer, lod, is_lod, (half4)converted_data);
				else if constexpr (is_int) cuda_image::write_int<image_type>(w_img_lod_obj[lod], runtime_image_type, converted_coord, layer, lod, is_lod, (int4)converted_data);
				else if constexpr (is_short) cuda_image::write_short<image_type>(w_img_lod_obj[lod], runtime_image_type, converted_coord, layer, lod, is_lod, (short4)converted_data);
				else if constexpr (is_uint) cuda_image::write_uint<image_type>(w_img_lod_obj[lod], runtime_image_type, converted_coord, layer, lod, is_lod, (uint4)converted_data);
				else if constexpr (is_ushort) cuda_image::write_ushort<image_type>(w_img_lod_obj[lod], runtime_image_type, converted_coord, layer, lod, is_lod, (ushort4)converted_data);
			}
#elif defined(FLOOR_DEVICE_HOST_COMPUTE)
			host_device_image<image_type, is_lod, false, false, false>::write((host_device_image<image_type, is_lod, false, false, false>*)w_img(), converted_coord, layer, lod, converted_data);
#endif
		}
		
		//! image write (non-array)
		template <typename coord_type>
		requires(!has_flag<IMAGE_TYPE::FLAG_ARRAY>(image_type))
		floor_inline_always auto write(const coord_type& coord,
									   const vector_sample_type& data) {
			static_assert(is_int_coord<coord_type>(), "image write coordinates must be of integer type");
			return write_internal<false>(coord, 0, 0, data);
		}
		
		//! image write (array)
		template <typename coord_type>
		requires(has_flag<IMAGE_TYPE::FLAG_ARRAY>(image_type))
		floor_inline_always auto write(const coord_type& coord,
									   const uint32_t layer,
									   const vector_sample_type& data) {
			static_assert(is_int_coord<coord_type>(), "image write coordinates must be of integer type");
			return write_internal<false>(coord, layer, 0, data);
		}
		
		//! image write at the specified lod level (non-array)
		template <typename coord_type>
		requires(!has_flag<IMAGE_TYPE::FLAG_ARRAY>(image_type))
		floor_inline_always auto write_lod(const coord_type& coord,
										   const uint32_t lod,
										   const vector_sample_type& data) {
			static_assert(is_int_coord<coord_type>(), "image write coordinates must be of integer type");
			return write_internal<true>(coord, 0, lod, data);
		}
		
		//! image write at the specified lod level (array)
		template <typename coord_type>
		requires(has_flag<IMAGE_TYPE::FLAG_ARRAY>(image_type))
		floor_inline_always auto write_lod(const coord_type& coord,
										   const uint32_t layer,
										   const uint32_t lod,
										   const vector_sample_type& data) {
			static_assert(is_int_coord<coord_type>(), "image write coordinates must be of integer type");
			return write_internal<true>(coord, layer, lod, data);
		}
		
		// TODO: msaa write functions (supported by vulkan)
		
	public: // image type checking
		//! returns true if this is a 1D image
		constexpr bool is_image_1d() const {
			return fl::is_image_1d(image_type);
		}
		
		//! returns true if this is a 1D image array
		constexpr bool is_image_1d_array() const {
			return fl::is_image_1d_array(image_type);
		}
		
		//! returns true if this is a 1D image buffer
		constexpr bool is_image_1d_buffer() const {
			return fl::is_image_1d_buffer(image_type);
		}
		
		//! returns true if this is a 2D image
		constexpr bool is_image_2d() const {
			return fl::is_image_2d(image_type);
		}
		
		//! returns true if this is a 2D image array
		constexpr bool is_image_2d_array() const {
			return fl::is_image_2d_array(image_type);
		}
		
		//! returns true if this is a 2D MSAA image
		constexpr bool is_image_2d_msaa() const {
			return fl::is_image_2d_msaa(image_type);
		}
		
		//! returns true if this is a 2D MSAA image array
		constexpr bool is_image_2d_msaa_array() const {
			return fl::is_image_2d_msaa_array(image_type);
		}
		
		//! returns true if this is a cube image
		constexpr bool is_image_cube() const {
			return fl::is_image_cube(image_type);
		}
		
		//! returns true if this is a cube image array
		constexpr bool is_image_cube_array() const {
			return fl::is_image_cube_array(image_type);
		}
		
		//! returns true if this is a 2D depth image
		constexpr bool is_image_depth() const {
			return fl::is_image_depth(image_type);
		}
		
		//! returns true if this is a 2D depth/stencil image
		constexpr bool is_image_depth_stencil() const {
			return fl::is_image_depth_stencil(image_type);
		}
		
		//! returns true if this is a 2D depth image array
		constexpr bool is_image_depth_array() const {
			return fl::is_image_depth_array(image_type);
		}
		
		//! returns true if this is a cube depth image
		constexpr bool is_image_depth_cube() const {
			return fl::is_image_depth_cube(image_type);
		}
		
		//! returns true if this is a cube depth image array
		constexpr bool is_image_depth_cube_array() const {
			return fl::is_image_depth_cube_array(image_type);
		}
		
		//! returns true if this is a 2D MSAA depth image
		constexpr bool is_image_depth_msaa() const {
			return fl::is_image_depth_msaa(image_type);
		}
		
		//! returns true if this is a 2D MSAA depth image array
		constexpr bool is_image_depth_msaa_array() const {
			return fl::is_image_depth_msaa_array(image_type);
		}
		
		//! returns true if this is a 3D image
		constexpr bool is_image_3d() const {
			return fl::is_image_3d(image_type);
		}
		
	};
	
} // namespace floor_image

//! read-write image (if write_only == false), write-only image (if write_only == true)
template <IMAGE_TYPE image_type, bool write_only = false>
using image = floor_image::image<((image_type & ~IMAGE_TYPE::__ACCESS_MASK) |
								  IMAGE_TYPE::WRITE |
								  (write_only ? IMAGE_TYPE::NONE : IMAGE_TYPE::READ))>;

//! const/read-only image
template <IMAGE_TYPE image_type>
using const_image = floor_image::image<(image_type & ~IMAGE_TYPE::__ACCESS_MASK) | IMAGE_TYPE::READ>;

// const/read-only image types
template <typename sample_type, IMAGE_TYPE ext_type = IMAGE_TYPE::NONE> using const_image_1d =
const_image<IMAGE_TYPE::IMAGE_1D | ext_type | floor_image::from_sample_type<sample_type>::type>;

template <typename sample_type, IMAGE_TYPE ext_type = IMAGE_TYPE::NONE> using const_image_1d_array =
const_image<IMAGE_TYPE::IMAGE_1D_ARRAY | ext_type | floor_image::from_sample_type<sample_type>::type>;

template <typename sample_type, IMAGE_TYPE ext_type = IMAGE_TYPE::NONE> using const_image_2d =
const_image<IMAGE_TYPE::IMAGE_2D | ext_type | floor_image::from_sample_type<sample_type>::type>;

template <typename sample_type, IMAGE_TYPE ext_type = IMAGE_TYPE::NONE> using const_image_2d_array =
const_image<IMAGE_TYPE::IMAGE_2D_ARRAY | ext_type | floor_image::from_sample_type<sample_type>::type>;

template <typename sample_type, IMAGE_TYPE ext_type = IMAGE_TYPE::NONE> using const_image_2d_msaa =
const_image<IMAGE_TYPE::IMAGE_2D_MSAA | ext_type | floor_image::from_sample_type<sample_type>::type>;

template <typename sample_type, IMAGE_TYPE ext_type = IMAGE_TYPE::NONE> using const_image_2d_msaa_array =
const_image<IMAGE_TYPE::IMAGE_2D_MSAA_ARRAY | ext_type | floor_image::from_sample_type<sample_type>::type>;

template <typename sample_type, IMAGE_TYPE ext_type = IMAGE_TYPE::NONE> using const_image_cube =
const_image<IMAGE_TYPE::IMAGE_CUBE | ext_type | floor_image::from_sample_type<sample_type>::type>;

template <typename sample_type, IMAGE_TYPE ext_type = IMAGE_TYPE::NONE> using const_image_cube_array =
const_image<IMAGE_TYPE::IMAGE_CUBE_ARRAY | ext_type | floor_image::from_sample_type<sample_type>::type>;

template <typename sample_type, IMAGE_TYPE ext_type = IMAGE_TYPE::NONE> using const_image_2d_depth =
const_image<(IMAGE_TYPE::IMAGE_DEPTH | ext_type |
			 // always single channel
			 IMAGE_TYPE::FLAG_FIXED_CHANNELS |
			 (floor_image::from_sample_type<sample_type>::type & ~IMAGE_TYPE::__CHANNELS_MASK))>;

template <typename sample_type, IMAGE_TYPE ext_type = IMAGE_TYPE::NONE> using const_image_2d_depth_stencil =
const_image<(IMAGE_TYPE::IMAGE_DEPTH_STENCIL | ext_type |
			 // always 2 channels
			 IMAGE_TYPE::FLAG_FIXED_CHANNELS |
			 (floor_image::from_sample_type<sample_type>::type & ~IMAGE_TYPE::__CHANNELS_MASK))>;

template <typename sample_type, IMAGE_TYPE ext_type = IMAGE_TYPE::NONE> using const_image_2d_depth_array =
const_image<(IMAGE_TYPE::IMAGE_DEPTH_ARRAY | ext_type |
			 // always single channel
			 IMAGE_TYPE::FLAG_FIXED_CHANNELS |
			 (floor_image::from_sample_type<sample_type>::type & ~IMAGE_TYPE::__CHANNELS_MASK))>;

template <typename sample_type, IMAGE_TYPE ext_type = IMAGE_TYPE::NONE> using const_image_cube_depth =
const_image<(IMAGE_TYPE::IMAGE_DEPTH_CUBE | ext_type |
			 // always single channel
			 IMAGE_TYPE::FLAG_FIXED_CHANNELS |
			 (floor_image::from_sample_type<sample_type>::type & ~IMAGE_TYPE::__CHANNELS_MASK))>;

template <typename sample_type, IMAGE_TYPE ext_type = IMAGE_TYPE::NONE> using const_image_cube_depth_array =
const_image<(IMAGE_TYPE::IMAGE_DEPTH_CUBE_ARRAY | ext_type |
			 // always single channel
			 IMAGE_TYPE::FLAG_FIXED_CHANNELS |
			 (floor_image::from_sample_type<sample_type>::type & ~IMAGE_TYPE::__CHANNELS_MASK))>;

template <typename sample_type, IMAGE_TYPE ext_type = IMAGE_TYPE::NONE> using const_image_2d_depth_msaa =
const_image<(IMAGE_TYPE::IMAGE_DEPTH_MSAA | ext_type |
			 // always single channel
			 IMAGE_TYPE::FLAG_FIXED_CHANNELS |
			 (floor_image::from_sample_type<sample_type>::type & ~IMAGE_TYPE::__CHANNELS_MASK))>;

template <typename sample_type, IMAGE_TYPE ext_type = IMAGE_TYPE::NONE> using const_image_2d_depth_msaa_array =
const_image<(IMAGE_TYPE::IMAGE_DEPTH_MSAA_ARRAY | ext_type |
			 // always single channel
			 IMAGE_TYPE::FLAG_FIXED_CHANNELS |
			 (floor_image::from_sample_type<sample_type>::type & ~IMAGE_TYPE::__CHANNELS_MASK))>;

template <typename sample_type, IMAGE_TYPE ext_type = IMAGE_TYPE::NONE> using const_image_3d =
const_image<IMAGE_TYPE::IMAGE_3D | ext_type | floor_image::from_sample_type<sample_type>::type>;

// read-write/write-only image types
template <typename sample_type, bool write_only = false, IMAGE_TYPE ext_type = IMAGE_TYPE::NONE> using image_1d =
image<IMAGE_TYPE::IMAGE_1D | ext_type | floor_image::from_sample_type<sample_type>::type, write_only>;

template <typename sample_type, bool write_only = false, IMAGE_TYPE ext_type = IMAGE_TYPE::NONE> using image_1d_array =
image<IMAGE_TYPE::IMAGE_1D_ARRAY | ext_type | floor_image::from_sample_type<sample_type>::type, write_only>;

template <typename sample_type, bool write_only = false, IMAGE_TYPE ext_type = IMAGE_TYPE::NONE> using image_2d =
image<IMAGE_TYPE::IMAGE_2D | ext_type | floor_image::from_sample_type<sample_type>::type, write_only>;

template <typename sample_type, bool write_only = false, IMAGE_TYPE ext_type = IMAGE_TYPE::NONE> using image_2d_array =
image<IMAGE_TYPE::IMAGE_2D_ARRAY | ext_type | floor_image::from_sample_type<sample_type>::type, write_only>;

template <typename sample_type, bool write_only = false, IMAGE_TYPE ext_type = IMAGE_TYPE::NONE> using image_2d_msaa =
image<IMAGE_TYPE::IMAGE_2D_MSAA | ext_type | floor_image::from_sample_type<sample_type>::type, write_only>;

template <typename sample_type, bool write_only = false, IMAGE_TYPE ext_type = IMAGE_TYPE::NONE> using image_2d_msaa_array =
image<IMAGE_TYPE::IMAGE_2D_MSAA_ARRAY | ext_type | floor_image::from_sample_type<sample_type>::type, write_only>;

template <typename sample_type, bool write_only = false, IMAGE_TYPE ext_type = IMAGE_TYPE::NONE> using image_cube =
image<IMAGE_TYPE::IMAGE_CUBE | ext_type | floor_image::from_sample_type<sample_type>::type, write_only>;

template <typename sample_type, bool write_only = false, IMAGE_TYPE ext_type = IMAGE_TYPE::NONE> using image_cube_array =
image<IMAGE_TYPE::IMAGE_CUBE_ARRAY | ext_type | floor_image::from_sample_type<sample_type>::type, write_only>;

template <typename sample_type, bool write_only = false, IMAGE_TYPE ext_type = IMAGE_TYPE::NONE> using image_2d_depth =
image<(IMAGE_TYPE::IMAGE_DEPTH | ext_type |
	   // always single channel
	   IMAGE_TYPE::FLAG_FIXED_CHANNELS |
	   (floor_image::from_sample_type<sample_type>::type & ~IMAGE_TYPE::__CHANNELS_MASK)), write_only>;

template <typename sample_type, bool write_only = false, IMAGE_TYPE ext_type = IMAGE_TYPE::NONE> using image_2d_depth_stencil =
image<(IMAGE_TYPE::IMAGE_DEPTH_STENCIL | ext_type |
	   // always 2 channels
	   IMAGE_TYPE::FLAG_FIXED_CHANNELS |
	   (floor_image::from_sample_type<sample_type>::type & ~IMAGE_TYPE::__CHANNELS_MASK)), write_only>;

template <typename sample_type, bool write_only = false, IMAGE_TYPE ext_type = IMAGE_TYPE::NONE> using image_2d_depth_array =
image<(IMAGE_TYPE::IMAGE_DEPTH_ARRAY | ext_type |
	   // always single channel
	   IMAGE_TYPE::FLAG_FIXED_CHANNELS |
	   (floor_image::from_sample_type<sample_type>::type & ~IMAGE_TYPE::__CHANNELS_MASK)), write_only>;

template <typename sample_type, bool write_only = false, IMAGE_TYPE ext_type = IMAGE_TYPE::NONE> using image_cube_depth =
image<(IMAGE_TYPE::IMAGE_DEPTH_CUBE | ext_type |
	   // always single channel
	   IMAGE_TYPE::FLAG_FIXED_CHANNELS |
	   (floor_image::from_sample_type<sample_type>::type & ~IMAGE_TYPE::__CHANNELS_MASK)), write_only>;

template <typename sample_type, bool write_only = false, IMAGE_TYPE ext_type = IMAGE_TYPE::NONE> using image_cube_depth_array =
image<(IMAGE_TYPE::IMAGE_DEPTH_CUBE_ARRAY | ext_type |
	   // always single channel
	   IMAGE_TYPE::FLAG_FIXED_CHANNELS |
	   (floor_image::from_sample_type<sample_type>::type & ~IMAGE_TYPE::__CHANNELS_MASK)), write_only>;

template <typename sample_type, bool write_only = false, IMAGE_TYPE ext_type = IMAGE_TYPE::NONE> using image_2d_depth_msaa =
image<(IMAGE_TYPE::IMAGE_DEPTH_MSAA | ext_type |
	   // always single channel
	   IMAGE_TYPE::FLAG_FIXED_CHANNELS |
	   (floor_image::from_sample_type<sample_type>::type & ~IMAGE_TYPE::__CHANNELS_MASK)), write_only>;

template <typename sample_type, bool write_only = false, IMAGE_TYPE ext_type = IMAGE_TYPE::NONE> using image_2d_depth_msaa_array =
image<(IMAGE_TYPE::IMAGE_DEPTH_MSAA_ARRAY | ext_type |
	   // always single channel
	   IMAGE_TYPE::FLAG_FIXED_CHANNELS |
	   (floor_image::from_sample_type<sample_type>::type & ~IMAGE_TYPE::__CHANNELS_MASK)), write_only>;

template <typename sample_type, bool write_only = false, IMAGE_TYPE ext_type = IMAGE_TYPE::NONE> using image_3d =
image<IMAGE_TYPE::IMAGE_3D | ext_type | floor_image::from_sample_type<sample_type>::type, write_only>;

} // namespace fl
