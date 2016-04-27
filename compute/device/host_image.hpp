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

#ifndef __FLOOR_COMPUTE_DEVICE_HOST_IMAGE_HPP__
#define __FLOOR_COMPUTE_DEVICE_HOST_IMAGE_HPP__

#if defined(FLOOR_COMPUTE_HOST)

// ignore vectorization/optimization/etc. hints and infos
FLOOR_PUSH_WARNINGS()
#if defined(__clang__)
#pragma clang diagnostic ignored "-Rpass-analysis"
#pragma clang diagnostic ignored "-Rpass"
#endif

template <COMPUTE_IMAGE_TYPE, bool is_lod, bool is_lod_float, bool is_bias> struct host_device_image;

namespace host_image_impl {
	struct image_clamp_dim_type {
		const uint4 int_dim;
		const float4 float_dim;
	};
	
	//! due to the fact that there are normalized image formats that differ in sample type from their storage type,
	//! we can't simply create templated functions for the specified 'image_type', but have to instantiate all possible
	//! ones and select the appropriate one at run-time (-> 'runtime_image_type').
	template <COMPUTE_IMAGE_TYPE fixed_image_type, bool is_lod, bool is_lod_float, bool is_bias>
	struct fixed_image {
		// returns the image_dim vector corresponding to the coord dimensionality
		template <size_t dim, typename clamp_dim_type, enable_if_t<dim == 1>* = nullptr>
		floor_inline_always static auto image_dim_to_coord_dim(const clamp_dim_type& clamp_dim) {
			return clamp_dim.x;
		}
		template <size_t dim, typename clamp_dim_type, enable_if_t<dim == 2>* = nullptr>
		floor_inline_always static auto image_dim_to_coord_dim(const clamp_dim_type& clamp_dim) {
			return clamp_dim.xy;
		}
		template <size_t dim, typename clamp_dim_type, enable_if_t<dim == 3>* = nullptr>
		floor_inline_always static auto image_dim_to_coord_dim(const clamp_dim_type& clamp_dim) {
			return clamp_dim.xyz;
		}
		template <size_t dim, typename clamp_dim_type, enable_if_t<dim == 4>* = nullptr>
		floor_inline_always static auto image_dim_to_coord_dim(const clamp_dim_type& clamp_dim) {
			return clamp_dim;
		}
		
		// clamps input coordinates to image_dim (image_clamp_dim/image_float_clamp_dim) and converts them to uint vectors
		//! int/uint coordinates
		template <typename coord_type,
				  enable_if_t<!is_floating_point<typename coord_type::decayed_scalar_type>::value>* = nullptr>
		floor_inline_always static auto process_coord(const image_clamp_dim_type& clamp_dim,
													  const coord_type& coord,
													  const vector_n<int32_t, coord_type::dim> offset = {}) {
			// clamp to [0, image_dim - 1]
			return vector_n<uint32_t, coord_type::dim> {
				(coord + offset).clamped(image_dim_to_coord_dim<coord_type::dim>(clamp_dim.int_dim))
			};
		}
		
		//! float coordinates, non cube map
		template <typename coord_type, COMPUTE_IMAGE_TYPE type = fixed_image_type,
				  enable_if_t<(is_floating_point<typename coord_type::decayed_scalar_type>::value &&
							   !has_flag<COMPUTE_IMAGE_TYPE::FLAG_CUBE>(type))>* = nullptr>
		floor_inline_always static auto process_coord(const image_clamp_dim_type& clamp_dim,
													  const coord_type& coord,
													  const vector_n<int32_t, coord_type::dim> offset = {}) {
			// clamp to [0, image_dim - 1], with normalized coord
			const auto fdim = image_dim_to_coord_dim<coord_type::dim>(clamp_dim.float_dim);
			return vector_n<uint32_t, coord_type::dim> { ((coord * fdim + 0.5f) + coord_type(offset)).clamp(fdim) };
		}
		
		//! float coordinates, cube map
		template <typename coord_type, COMPUTE_IMAGE_TYPE type = fixed_image_type,
				  enable_if_t<(is_floating_point<typename coord_type::decayed_scalar_type>::value &&
							   has_flag<COMPUTE_IMAGE_TYPE::FLAG_CUBE>(type))>* = nullptr>
		floor_inline_always static auto process_coord(const image_clamp_dim_type& clamp_dim,
													  const coord_type& coord,
													  // NOTE: offset coords with a cube map aren't supported anywhere (do the same)
													  const vector_n<int32_t, coord_type::dim> offset floor_unused = {}) {
			// clamp to [0, image_dim - 1], with normalized coord
			const auto coord_layer = compute_cube_coord_and_layer(coord);
			const auto fdim = image_dim_to_coord_dim<coord_type::dim - 1u>(clamp_dim.float_dim);
			return vector_n<uint32_t, coord_type::dim> {
				(coord_layer.first * fdim + 0.5f).clamp(fdim),
				coord_layer.second
			};
		}
		
		//! cube mapping helper function, transforms the input 3D coordinate/direction to the resp. 2D texture coordinate + layer index
		floor_inline_always static pair<float2, uint32_t> compute_cube_coord_and_layer(float3 coord) {
			// NOTE: cube face order is: +X, -X, +Y, -Y, +Z, -Z
			const auto abs_coord = coord.absed();
			const uint32_t layer = (abs_coord.x >= abs_coord.y ?
									(abs_coord.x >= abs_coord.z ? (coord.x >= 0.0f ? 0 : 1) : (coord.z >= 0.0f ? 4 : 5)) :
									(abs_coord.y >= abs_coord.z ? (coord.y >= 0.0f ? 2 : 3) : (coord.z >= 0.0f ? 4 : 5)));
			float3 st_ma;
			switch(layer) {
				case 0: st_ma = { -coord.z, -coord.y, abs_coord.x }; break;
				case 1: st_ma = { coord.z, -coord.y, abs_coord.x }; break;
				case 2: st_ma = { coord.x, coord.z, abs_coord.y }; break;
				case 3: st_ma = { coord.x, -coord.z, abs_coord.y }; break;
				case 4: st_ma = { coord.x, -coord.y, abs_coord.z }; break;
				case 5: st_ma = { -coord.x, -coord.y, abs_coord.z }; break;
				default: floor_unreachable();
			}
			st_ma.xy = (st_ma.xy * (0.5f / st_ma.z)) + 0.5f;
			return { st_ma.xy, layer };
		}
		
		// -> coord to offset functions for all image dims, note that coord is assumed to be clamped and a floor vector
		//! 1D, 1D buffer
		floor_inline_always static size_t coord_to_offset(const uint4&, uint1 coord) {
			return size_t(coord.x) * image_bytes_per_pixel(fixed_image_type);
		}
		
		//! 1D array
		floor_inline_always static size_t coord_to_offset(const uint4& image_dim, uint1 coord, uint32_t layer) {
			return coord_to_offset(image_dim, coord.x) + image_slice_data_size_from_types(image_dim, fixed_image_type) * layer;
		}
		
		//! 2D, 2D depth, 2D depth+stencil
		floor_inline_always static size_t coord_to_offset(const uint4& image_dim, uint2 coord) {
			return size_t(image_dim.x * coord.y + coord.x) * image_bytes_per_pixel(fixed_image_type);
		}
		
		//! 2D array, 2D depth array
		floor_inline_always static size_t coord_to_offset(const uint4& image_dim, uint2 coord, uint32_t layer) {
			return coord_to_offset(image_dim, coord) + image_slice_data_size_from_types(image_dim, fixed_image_type) * layer;
		}
		
		//! 3D
		template <COMPUTE_IMAGE_TYPE type = fixed_image_type, enable_if_t<!has_flag<COMPUTE_IMAGE_TYPE::FLAG_CUBE>(type)>* = nullptr>
		floor_inline_always static size_t coord_to_offset(const uint4& image_dim, uint3 coord) {
			return size_t(image_dim.x * image_dim.y * coord.z +
						  image_dim.x * coord.y +
						  coord.x) * image_bytes_per_pixel(type);
		}
		
		//! cube, depth cube
		template <COMPUTE_IMAGE_TYPE type = fixed_image_type, enable_if_t<has_flag<COMPUTE_IMAGE_TYPE::FLAG_CUBE>(type)>* = nullptr>
		floor_inline_always static size_t coord_to_offset(const uint4& image_dim, uint3 coord) {
			return coord_to_offset(image_dim, coord.xy, coord.z);
		}
		
		//! cube array, depth cube array
		floor_inline_always static size_t coord_to_offset(const uint4& image_dim, uint3 coord, uint32_t layer) {
			return coord_to_offset(image_dim, coord.xy, layer * 6u + coord.z);
		}
		
		// helper functions
		static constexpr auto compute_image_bpc() {
			return const_array<uint64_t, 4> {{
				image_bits_of_channel(fixed_image_type, 0),
				image_bits_of_channel(fixed_image_type, 1),
				image_bits_of_channel(fixed_image_type, 2),
				image_bits_of_channel(fixed_image_type, 3)
			}};
		}
		
		static constexpr void normalized_format_validity_check() {
			constexpr const auto image_format = (fixed_image_type & COMPUTE_IMAGE_TYPE::__FORMAT_MASK);
			
			// validity checking: depth reading is handled elsewhere
			static_assert(image_format != COMPUTE_IMAGE_TYPE::FORMAT_24 &&
						  image_format != COMPUTE_IMAGE_TYPE::FORMAT_24_8 &&
						  image_format != COMPUTE_IMAGE_TYPE::FORMAT_32_8, "invalid integer format!");
			
			// validity checking: unsupported formats
			static_assert(image_format == COMPUTE_IMAGE_TYPE::FORMAT_2 ||
						  image_format == COMPUTE_IMAGE_TYPE::FORMAT_4 ||
						  image_format == COMPUTE_IMAGE_TYPE::FORMAT_8 ||
						  image_format == COMPUTE_IMAGE_TYPE::FORMAT_16 ||
						  image_format == COMPUTE_IMAGE_TYPE::FORMAT_32 ||
						  image_format == COMPUTE_IMAGE_TYPE::FORMAT_64, "unsupported format!");
		}
		
		static constexpr void depth_format_validity_check() {
			constexpr const auto data_type = (fixed_image_type & COMPUTE_IMAGE_TYPE::__DATA_TYPE_MASK);
			static_assert(data_type == COMPUTE_IMAGE_TYPE::FLOAT ||
						  data_type == COMPUTE_IMAGE_TYPE::UINT, "invalid depth data type!");
			
			constexpr const auto image_format = (fixed_image_type & COMPUTE_IMAGE_TYPE::__FORMAT_MASK);
			constexpr const bool has_stencil = has_flag<COMPUTE_IMAGE_TYPE::FLAG_STENCIL>(fixed_image_type);
			static_assert((!has_stencil &&
						   ((image_format == COMPUTE_IMAGE_TYPE::FORMAT_16 && data_type == COMPUTE_IMAGE_TYPE::UINT) ||
							(image_format == COMPUTE_IMAGE_TYPE::FORMAT_24 && data_type == COMPUTE_IMAGE_TYPE::UINT) ||
							image_format == COMPUTE_IMAGE_TYPE::FORMAT_32 /* supported by both */)) ||
						  (has_stencil &&
						   ((image_format == COMPUTE_IMAGE_TYPE::FORMAT_24_8 && data_type == COMPUTE_IMAGE_TYPE::UINT) ||
							(image_format == COMPUTE_IMAGE_TYPE::FORMAT_32_8 && data_type == COMPUTE_IMAGE_TYPE::FLOAT))),
						  "invalid depth format!");
			
			constexpr const auto channel_count = image_channel_count(fixed_image_type);
			static_assert((!has_stencil && channel_count == 1) ||
						  (has_stencil && channel_count == 2), "invalid channel count for depth format!");
		}

		template <size_t N, COMPUTE_IMAGE_TYPE type = fixed_image_type,
				  enable_if_t<(type & COMPUTE_IMAGE_TYPE::__DATA_TYPE_MASK) != COMPUTE_IMAGE_TYPE::FLOAT>* = nullptr>
		floor_inline_always static auto extract_channels(const uint8_t (&raw_data)[N]) {
			constexpr const auto image_format = (type & COMPUTE_IMAGE_TYPE::__FORMAT_MASK);
			normalized_format_validity_check();
			
			// get bits of each channel (0 if channel doesn't exist)
			constexpr const auto bpc = compute_image_bpc();
			constexpr const auto max_bpc = max(max(bpc[0], bpc[1]), max(bpc[2], bpc[3]));
			// figure out which type we need to store each channel (max size is used)
			typedef typename image_sized_data_type<type, max_bpc>::type channel_type;
			// and the appropriate unsigned format
			typedef typename image_sized_data_type<COMPUTE_IMAGE_TYPE::UINT, max_bpc>::type uchannel_type;
			
			// and now for bit voodoo:
			constexpr const uint32_t channel_count = image_channel_count(type);
			array<channel_type, channel_count> ret;
			switch(image_format) {
				// uniform formats
				case COMPUTE_IMAGE_TYPE::FORMAT_2:
#pragma unroll
					for(uint32_t i = 0; i < channel_count; ++i) {
						*((uchannel_type*)&ret[i]) = (raw_data[0] >> (6u - 2u * i)) & 0b11u;
					}
					break;
				case COMPUTE_IMAGE_TYPE::FORMAT_4:
#pragma unroll
					for(uint32_t i = 0; i < channel_count; ++i) {
						*((uchannel_type*)&ret[i]) = (raw_data[i / 2] >> (i % 2u == 0 ? 4u : 0u)) & 0b1111u;
					}
					break;
				case COMPUTE_IMAGE_TYPE::FORMAT_8:
				case COMPUTE_IMAGE_TYPE::FORMAT_16:
				case COMPUTE_IMAGE_TYPE::FORMAT_32:
				case COMPUTE_IMAGE_TYPE::FORMAT_64:
					memcpy(&ret[0], raw_data, N);
					break;
				
				//  TODO: special formats, implement this when needed, opencl/cuda don't support this either
				default:
					floor_unreachable();
			}
			
			// need to fix up the sign bit for non-8/16/32/64-bit signed formats
			if((type & COMPUTE_IMAGE_TYPE::__DATA_TYPE_MASK) == COMPUTE_IMAGE_TYPE::INT) {
				constexpr const const_array<uchannel_type, 4> high_bits {{
					uchannel_type(1) << (max(bpc[0], 1ull) - 1u),
					uchannel_type(1) << (max(bpc[1], 1ull) - 1u),
					uchannel_type(1) << (max(bpc[2], 1ull) - 1u),
					uchannel_type(1) << (max(bpc[3], 1ull) - 1u)
				}};
				for(uint32_t i = 0; i < channel_count; ++i) {
					if(bpc[i] % 8u != 0u &&
					   *((uchannel_type*)&ret[i]) & high_bits[i] != 0u) {
						ret[i] ^= high_bits[i];
						ret[i] = -ret[i];
					}
				}
			}
			
			return ret;
		}
		template <size_t N, COMPUTE_IMAGE_TYPE type = fixed_image_type,
				  enable_if_t<(type & COMPUTE_IMAGE_TYPE::__DATA_TYPE_MASK) == COMPUTE_IMAGE_TYPE::FLOAT>* = nullptr>
		floor_inline_always static auto extract_channels(const uint8_t (&raw_data)[N] floor_unused) {
			// this is a dummy function and never called, but necessary for compilation
			return vector_n<uint8_t, image_channel_count(type)> {};
		}


		template <COMPUTE_IMAGE_TYPE type = fixed_image_type,
				  enable_if_t<(type & COMPUTE_IMAGE_TYPE::__DATA_TYPE_MASK) != COMPUTE_IMAGE_TYPE::FLOAT>* = nullptr>
		floor_inline_always static auto insert_channels(const float4& color) {
			constexpr const auto image_format = (type & COMPUTE_IMAGE_TYPE::__FORMAT_MASK);
			constexpr const auto data_type = (type & COMPUTE_IMAGE_TYPE::__DATA_TYPE_MASK);
			constexpr const uint32_t channel_count = image_channel_count(type);
			normalized_format_validity_check();
			
			// figure out how much data (raw data) must be returned/created
			constexpr const auto bpp = image_bytes_per_pixel(type);
			array<uint8_t, bpp> raw_data;
			
			// scale fp vals to the expected int/uint size
			constexpr const auto bpc = compute_image_bpc();
			constexpr const auto max_bpc = max(max(bpc[0], bpc[1]), max(bpc[2], bpc[3]));
			typedef typename image_sized_data_type<type, max_bpc>::type channel_type;
			typedef typename image_sized_data_type<COMPUTE_IMAGE_TYPE::UINT, max_bpc>::type uchannel_type;
			typedef conditional_t<max_bpc <= 8u, float, conditional_t<max_bpc <= 16u, double, long double>> fp_scale_type;
			vector_n<channel_type, channel_count> scaled_color;
#pragma clang loop unroll(FLOOR_CLANG_UNROLL_FULL) vectorize(enable) interleave(enable)
			for(uint32_t i = 0; i < channel_count; ++i) {
				// scale with 2^(bpc (- 1 if signed)) - 1 (e.g. 255 for unsigned 8-bit, 127 for signed 8-bit)
				scaled_color[i] = channel_type(fp_scale_type(color[i]) * fp_scale_type((uchannel_type(1)
																						<< uchannel_type(max(bpc[i], 1ull)
																										 - (data_type == COMPUTE_IMAGE_TYPE::UINT ? 0u : 1u)))
																					   - uchannel_type(1)));
			}
			
			constexpr const bool is_signed_format = (data_type == COMPUTE_IMAGE_TYPE::INT);
			switch(image_format) {
FLOOR_PUSH_WARNINGS()
FLOOR_IGNORE_WARNING(sign-conversion) // ignore sign conversion warnings, unsigned code path is never taken for signed formats
					
				// uniform formats
				case COMPUTE_IMAGE_TYPE::FORMAT_2:
					memset(&raw_data[0], 0, bpp);
					if(!is_signed_format) {
						for(uint32_t i = 0; i < channel_count; ++i) {
							raw_data[0] |= (scaled_color[i] & 0b11u) << (6u - 2u * i);
						}
					}
					else {
						// since the sign bit isn't the msb, it must be handled manually
						for(uint32_t i = 0; i < channel_count; ++i) {
							raw_data[0] |= ((scaled_color[i] & 0b1u) | (scaled_color[i] < 0 ? 0b10u : 0u)) << (6u - 2u * i);
						}
					}
					break;
				case COMPUTE_IMAGE_TYPE::FORMAT_4:
					memset(&raw_data[0], 0, bpp);
					if(!is_signed_format) {
						for(uint32_t i = 0; i < channel_count; ++i) {
							raw_data[i / 2u] |= (scaled_color[i] & 0b1111u) << (i % 2u == 0 ? 4u : 0u);
						}
					}
					else {
						// since the sign bit isn't the msb, it must be handled manually
						for(uint32_t i = 0; i < channel_count; ++i) {
							raw_data[i / 2u] |= ((scaled_color[i] & 0b111u) | (scaled_color[i] < 0 ? 0b1000u : 0u)) << (i % 2u == 0 ? 4u : 0u);
						}
					}
					break;
				case COMPUTE_IMAGE_TYPE::FORMAT_8:
				case COMPUTE_IMAGE_TYPE::FORMAT_16:
				case COMPUTE_IMAGE_TYPE::FORMAT_32:
				case COMPUTE_IMAGE_TYPE::FORMAT_64:
					memcpy(&raw_data[0], &scaled_color, bpp);
					break;
					
				//  TODO: special formats, implement this when needed, opencl/cuda don't support this either
				default:
					floor_unreachable();
					
FLOOR_POP_WARNINGS()
			}
			
			return raw_data;
		}
		template <COMPUTE_IMAGE_TYPE type = fixed_image_type,
				  enable_if_t<(type & COMPUTE_IMAGE_TYPE::__DATA_TYPE_MASK) == COMPUTE_IMAGE_TYPE::FLOAT>* = nullptr>
		floor_inline_always static auto insert_channels(const float4&) {
			// this is a dummy function and never called, but necessary for compilation
			return vector_n<uint8_t, image_channel_count(type)> {};
		}

FLOOR_PUSH_WARNINGS()
FLOOR_IGNORE_WARNING(cast-align) // kill "cast needs 4 byte alignment" warning in here (it is 4 byte aligned)

		// image read functions
		template <typename coord_type, typename offset_type, COMPUTE_IMAGE_TYPE type = fixed_image_type,
				  enable_if_t<((has_flag<COMPUTE_IMAGE_TYPE::FLAG_NORMALIZED>(type) ||
								(type & COMPUTE_IMAGE_TYPE::__DATA_TYPE_MASK) == COMPUTE_IMAGE_TYPE::FLOAT) &&
							   !has_flag<COMPUTE_IMAGE_TYPE::FLAG_DEPTH>(type))>* = nullptr>
		static auto read(const host_device_image<type, is_lod, is_lod_float, is_bias>* img,
						 const coord_type& coord,
						 const uint32_t layer,
						 const offset_type& coord_offset,
						 const int32_t lod_i,
						 const float lod_or_bias_f) {
			// read/copy raw data
			constexpr const bool is_array = has_flag<COMPUTE_IMAGE_TYPE::FLAG_ARRAY>(type);
			constexpr const size_t bpp = image_bytes_per_pixel(type);
			const auto offset = __builtin_choose_expr(!is_array,
													  coord_to_offset(img->image_dim, process_coord(img->image_clamp_dim, coord, coord_offset)),
													  coord_to_offset(img->image_dim, process_coord(img->image_clamp_dim, coord, coord_offset), layer));
			typedef uint8_t raw_data_type[bpp];
			const raw_data_type& raw_data = *(const raw_data_type*)&img->data[offset];
			
			// extract channel bits/bytes
			constexpr const auto data_type = (type & COMPUTE_IMAGE_TYPE::__DATA_TYPE_MASK);
			constexpr const auto image_format = (type & COMPUTE_IMAGE_TYPE::__FORMAT_MASK);
			constexpr const auto channel_count = image_channel_count(type);
			vector_n<conditional_t<(image_bits_of_channel(type, 0) <= 32 ||
									data_type != COMPUTE_IMAGE_TYPE::FLOAT), float, double>, 4> ret;
			if(data_type == COMPUTE_IMAGE_TYPE::FLOAT) {
				if(image_format == COMPUTE_IMAGE_TYPE::FORMAT_32 ||
				   image_format == COMPUTE_IMAGE_TYPE::FORMAT_64) {
					// for 32-bit/64-bit float formats, just pass-through raw data
					ret = *(decltype(ret)*)raw_data;
				}
#if !defined(_MSC_VER) && !defined(__linux__) // TODO: a s/w solution
				else if(image_format == COMPUTE_IMAGE_TYPE::FORMAT_16) {
					// 16-bit half float data must be converted to 32-bit float data
#pragma clang loop unroll(FLOOR_CLANG_UNROLL_FULL) vectorize(enable) interleave(enable)
					for(uint32_t i = 0; i < channel_count; ++i) {
						ret[i] = (float)*(__fp16*)(raw_data + i * 2);
					}
				}
#endif
				else floor_unreachable();
			}
			else {
				// extract all channels from the raw data
				const auto channels = extract_channels(raw_data);
				
				// normalize + convert to float
				constexpr const auto bpc = compute_image_bpc();
				constexpr const const_array<float, 4> unsigned_factors {{
					// normalized = channel / (2^bpc_i - 1)
					float(1.0 / max(1.0, double(const_math::bit_mask(max(uint64_t(1), bpc[0]))))),
					float(1.0 / max(1.0, double(const_math::bit_mask(max(uint64_t(1), bpc[1]))))),
					float(1.0 / max(1.0, double(const_math::bit_mask(max(uint64_t(1), bpc[2]))))),
					float(1.0 / max(1.0, double(const_math::bit_mask(max(uint64_t(1), bpc[3])))))
				}};
				constexpr const const_array<float, 4> signed_factors {{
					// normalized = channel / (2^(bpc_i - 1) - 1)
					float(1.0 / max(1.0, double(const_math::bit_mask(max(uint64_t(1), bpc[0])) >> 1ull))),
					float(1.0 / max(1.0, double(const_math::bit_mask(max(uint64_t(1), bpc[1])) >> 1ull))),
					float(1.0 / max(1.0, double(const_math::bit_mask(max(uint64_t(1), bpc[2])) >> 1ull))),
					float(1.0 / max(1.0, double(const_math::bit_mask(max(uint64_t(1), bpc[3])) >> 1ull)))
				}};
				if(data_type == COMPUTE_IMAGE_TYPE::UINT) {
					// normalized unsigned-integer formats, normalized to [0, 1]
#pragma clang loop unroll(FLOOR_CLANG_UNROLL_FULL) vectorize(enable) interleave(enable)
					for(uint32_t i = 0; i < channel_count; ++i) {
						ret[i] = float(channels[i]) * unsigned_factors[i];
					}
				}
				else if(data_type == COMPUTE_IMAGE_TYPE::INT) {
					// normalized integer formats, normalized to [-1, 1]
#pragma clang loop unroll(FLOOR_CLANG_UNROLL_FULL) vectorize(enable) interleave(enable)
					for(uint32_t i = 0; i < channel_count; ++i) {
						ret[i] = float(channels[i]) * signed_factors[i];
					}
				}
			}
			return ret;
		}

FLOOR_POP_WARNINGS()

		template <typename coord_type, typename offset_type, COMPUTE_IMAGE_TYPE type = fixed_image_type,
				  enable_if_t<has_flag<COMPUTE_IMAGE_TYPE::FLAG_DEPTH>(type)>* = nullptr>
		static auto read(const host_device_image<type, is_lod, is_lod_float, is_bias>* img,
						 const coord_type& coord,
						 const uint32_t layer,
						 const offset_type& coord_offset,
						 const int32_t lod_i,
						 const float lod_or_bias_f) {
			constexpr const bool is_array = has_flag<COMPUTE_IMAGE_TYPE::FLAG_ARRAY>(type);
			constexpr const auto data_type = (type & COMPUTE_IMAGE_TYPE::__DATA_TYPE_MASK);
			constexpr const auto image_format = (type & COMPUTE_IMAGE_TYPE::__FORMAT_MASK);
			constexpr const bool has_stencil = has_flag<COMPUTE_IMAGE_TYPE::FLAG_STENCIL>(type);
			
			// validate all the things
			depth_format_validity_check();
			
			// depth is always returned as a float, with stencil a a <float, 8-bit uint> pair
			// NOTE: neither opencl, nor cuda support reading depth+stencil images, so a proper return type is unclear right now
			typedef conditional_t<!has_stencil, float, pair<float, uint8_t>> ret_type;
			ret_type ret;
			const auto offset = __builtin_choose_expr(!is_array,
													  coord_to_offset(img->image_dim, process_coord(img->image_clamp_dim, coord, coord_offset)),
													  coord_to_offset(img->image_dim, process_coord(img->image_clamp_dim, coord, coord_offset), layer));
			if(data_type == COMPUTE_IMAGE_TYPE::FLOAT) {
				// can just pass-through the float value
				memcpy(&ret, &img->data[offset], sizeof(float));
				if(has_stencil) {
					memcpy(((uint8_t*)&ret) + sizeof(float), &img->data[offset + sizeof(float)], sizeof(uint8_t));
				}
			}
			else { // UINT
				constexpr const size_t depth_bytes = (image_format == COMPUTE_IMAGE_TYPE::FORMAT_16 ? 2 :
													  image_format == COMPUTE_IMAGE_TYPE::FORMAT_24 ? 3 :
													  image_format == COMPUTE_IMAGE_TYPE::FORMAT_24_8 ? 3 :
													  image_format == COMPUTE_IMAGE_TYPE::FORMAT_32 ? 4 :
													  1 /* don't produce an error, validity is already confirmed */);
				// always use double for better precision
				constexpr const double norm_factor = 1.0 / double((1ull << (8ull * depth_bytes)) - 1ull);
				uint32_t depth_val;
				switch(depth_bytes) {
					case 2:
						depth_val = (((img->data[offset] >> 8u) & 0xFFu) +
									 (img->data[offset + 1u] & 0xFFu));
						break;
					case 3:
						depth_val = (((img->data[offset] >> 16u) & 0xFFu) +
									 ((img->data[offset + 1u] >> 8u) & 0xFFu) +
									 (img->data[offset + 2u] & 0xFFu));
						break;
					case 4:
						depth_val = (((img->data[offset] >> 24u) & 0xFFu) +
									 ((img->data[offset + 1u] >> 16u) & 0xFFu) +
									 ((img->data[offset + 2u] >> 8u) & 0xFFu) +
									 (img->data[offset + 3u] & 0xFFu));
						break;
					default:
						floor_unreachable();
				}
				// normalize and convert to float
				*(float*)&ret = float(double(depth_val) * norm_factor);
				
				// finally, copy stencil
				if(has_stencil) {
					memcpy(((uint8_t*)&ret) + sizeof(float), &img->data[offset + depth_bytes], sizeof(uint8_t));
				}
			}
			return ret;
		}

FLOOR_PUSH_WARNINGS()
FLOOR_IGNORE_WARNING(cast-align) // kill "cast needs 4 byte alignment" warning in here (it is 4 byte aligned)

		template <typename coord_type, typename offset_type, COMPUTE_IMAGE_TYPE type = fixed_image_type,
				  enable_if_t<(!has_flag<COMPUTE_IMAGE_TYPE::FLAG_NORMALIZED>(type) &&
							   ((type & COMPUTE_IMAGE_TYPE::__DATA_TYPE_MASK) == COMPUTE_IMAGE_TYPE::INT ||
								(type & COMPUTE_IMAGE_TYPE::__DATA_TYPE_MASK) == COMPUTE_IMAGE_TYPE::UINT) &&
							   !has_flag<COMPUTE_IMAGE_TYPE::FLAG_DEPTH>(type))>* = nullptr>
		static auto read(const host_device_image<type, is_lod, is_lod_float, is_bias>* img,
						 const coord_type& coord,
						 const uint32_t layer,
						 const offset_type& coord_offset,
						 const int32_t lod_i,
						 const float lod_or_bias_f) {
			// read/copy raw data
			constexpr const bool is_array = has_flag<COMPUTE_IMAGE_TYPE::FLAG_ARRAY>(type);
			constexpr const size_t bpp = image_bytes_per_pixel(type);
			const auto offset = __builtin_choose_expr(!is_array,
													  coord_to_offset(img->image_dim, process_coord(img->image_clamp_dim, coord, coord_offset)),
													  coord_to_offset(img->image_dim, process_coord(img->image_clamp_dim, coord, coord_offset), layer));
			typedef uint8_t raw_data_type[bpp];
			const raw_data_type& raw_data = *(const raw_data_type*)&img->data[offset];
			
			// for compatibility with opencl/cuda, always return 32-bit values for anything <= 32-bit (and 64-bit values for > 32-bit)
			constexpr const bool is_signed_format = ((type & COMPUTE_IMAGE_TYPE::__DATA_TYPE_MASK) == COMPUTE_IMAGE_TYPE::INT);
			typedef conditional_t<image_bits_of_channel(type, 0) <= 32,
								  conditional_t<is_signed_format, int32_t, uint32_t>,
								  conditional_t<is_signed_format, int64_t, uint64_t>> ret_type;
			constexpr const auto channel_count = image_channel_count(type);
			const vector_n<ret_type, channel_count> ret_widened =
				*(vector_n<typename image_sized_data_type<type, image_bits_of_channel(type, 0)>::type, channel_count>*)raw_data;
			return vector_n<ret_type, 4> { ret_widened };
		}

FLOOR_POP_WARNINGS()

		// image write functions
		template <typename coord_type, COMPUTE_IMAGE_TYPE type = fixed_image_type,
				  enable_if_t<((has_flag<COMPUTE_IMAGE_TYPE::FLAG_NORMALIZED>(type) ||
								(type & COMPUTE_IMAGE_TYPE::__DATA_TYPE_MASK) == COMPUTE_IMAGE_TYPE::FLOAT) &&
							   !has_flag<COMPUTE_IMAGE_TYPE::FLAG_DEPTH>(type))>* = nullptr>
		static void write(const host_device_image<type, is_lod, is_lod_float, is_bias>* img,
						  const coord_type& coord,
						  const uint32_t layer,
						  const uint32_t lod,
						  const float4& color) {
			constexpr const bool is_array = has_flag<COMPUTE_IMAGE_TYPE::FLAG_ARRAY>(type);
			constexpr const auto data_type = (type & COMPUTE_IMAGE_TYPE::__DATA_TYPE_MASK);
			constexpr const auto image_format = (type & COMPUTE_IMAGE_TYPE::__FORMAT_MASK);
			const auto offset = __builtin_choose_expr(!is_array,
													  coord_to_offset(img->image_dim, process_coord(img->image_clamp_dim, coord)),
													  coord_to_offset(img->image_dim, process_coord(img->image_clamp_dim, coord), layer));
			
			if(data_type == COMPUTE_IMAGE_TYPE::FLOAT) {
				constexpr const auto channel_count = image_channel_count(type);
				
				// for 32-bit/64-bit float formats, just pass-through
				if(image_format == COMPUTE_IMAGE_TYPE::FORMAT_32) {
					memcpy(&img->data[offset], &color, sizeof(float) * channel_count);
				}
				else if(image_format == COMPUTE_IMAGE_TYPE::FORMAT_64) {
					// TODO: should have a write function that accepts double4
					const double4 double_color = color;
					memcpy(&img->data[offset], &double_color, sizeof(double) * channel_count);
				}
#if !defined(_MSC_VER) && !defined(__linux__) // TODO: add a s/w solution
				else if(image_format == COMPUTE_IMAGE_TYPE::FORMAT_16) {
					// for 16-bit half float formats, data must be converted to 32-bit float data
					__fp16 half_vals[4];
#pragma clang loop unroll(FLOOR_CLANG_UNROLL_FULL) vectorize(enable) interleave(enable)
					for(uint32_t i = 0; i < channel_count; ++i) {
						half_vals[i] = (__fp16)color[i];
					}
					memcpy(&img->data[offset], half_vals, sizeof(__fp16) * channel_count);
				}
#endif
				else floor_unreachable();
			}
			else {
				// for integer formats, need to rescale to storage integer format and convert
				const auto converted_color = insert_channels(color);
				
				// note that two pixels never share the same byte -> can just memcpy, even for crooked formats
				memcpy(&img->data[offset], &converted_color, sizeof(decltype(converted_color)));
			}
		}


		template <typename coord_type, COMPUTE_IMAGE_TYPE type = fixed_image_type,
				  /* float depth value, or float depth + uint8_t stencil */
				  typename color_type = conditional_t<image_channel_count(type) == 1, float, pair<float, uint8_t>>,
				  enable_if_t<has_flag<COMPUTE_IMAGE_TYPE::FLAG_DEPTH>(type)>* = nullptr>
		static void write(const host_device_image<type, is_lod, is_lod_float, is_bias>* img,
						  const coord_type& coord,
						  const uint32_t layer,
						  const uint32_t lod,
						  const color_type& color) {
			depth_format_validity_check();
			
			constexpr const bool is_array = has_flag<COMPUTE_IMAGE_TYPE::FLAG_ARRAY>(type);
			constexpr const auto data_type = (type & COMPUTE_IMAGE_TYPE::__DATA_TYPE_MASK);
			constexpr const auto image_format = (type & COMPUTE_IMAGE_TYPE::__FORMAT_MASK);
			constexpr const bool has_stencil = has_flag<COMPUTE_IMAGE_TYPE::FLAG_STENCIL>(type);
			
			// depth value input is always a float -> convert it to the correct output format
			const auto offset = __builtin_choose_expr(!is_array,
													  coord_to_offset(img->image_dim, process_coord(img->image_clamp_dim, coord)),
													  coord_to_offset(img->image_dim, process_coord(img->image_clamp_dim, coord), layer));
			if(data_type == COMPUTE_IMAGE_TYPE::FLOAT) {
				// can just pass-through the float value
				memcpy(&img->data[offset], &color, sizeof(float));
				if(has_stencil) {
					memcpy(&img->data[offset + sizeof(float)], ((uint8_t*)&color) + sizeof(float), sizeof(uint8_t));
				}
			}
			else { // UINT
				// need to create a normalized uint
				constexpr const size_t depth_bytes = (image_format == COMPUTE_IMAGE_TYPE::FORMAT_16 ? 2 :
													  image_format == COMPUTE_IMAGE_TYPE::FORMAT_24 ? 3 :
													  image_format == COMPUTE_IMAGE_TYPE::FORMAT_24_8 ? 3 :
													  image_format == COMPUTE_IMAGE_TYPE::FORMAT_32 ? 4 :
													  1 /* don't produce an error, validity is already confirmed */);
				constexpr const uint32_t clamp_value = (depth_bytes <= 1 ? 0xFFu :
														depth_bytes == 2 ? 0xFFFFu :
														depth_bytes == 3 ? 0xFFFFFFu : 0xFFFFFFFFu);
				// always use long double for better precision
				constexpr const auto scale_factor = (long double)((1ull << (8ull * depth_bytes)) - 1ull);
				uint32_t depth_val = (uint32_t)scale_factor * ((long double)*(float*)&color);
				if(depth_bytes != 4) {
					// clamp non-32-bit values to their correct range
					depth_val = const_math::clamp(depth_val, 0u, clamp_value);
				}
				switch(depth_bytes) {
					case 2:
						img->data[offset] = ((depth_val >> 8u) & 0xFFu);
						img->data[offset + 1u] = (depth_val & 0xFFu);
						break;
					case 3:
						img->data[offset] = ((depth_val >> 16u) & 0xFFu);
						img->data[offset + 1u] = ((depth_val >> 8u) & 0xFFu);
						img->data[offset + 2u] = (depth_val & 0xFFu);
						break;
					case 4:
						img->data[offset] = ((depth_val >> 24u) & 0xFFu);
						img->data[offset + 1u] = ((depth_val >> 16u) & 0xFFu);
						img->data[offset + 2u] = ((depth_val >> 8u) & 0xFFu);
						img->data[offset + 3u] = (depth_val & 0xFFu);
						break;
					default:
						floor_unreachable();
				}
				
				// finally, copy stencil
				if(has_stencil) {
					memcpy(&img->data[offset + depth_bytes], ((uint8_t*)&color) + sizeof(float), sizeof(uint8_t));
				}
			}
		}

		template <typename coord_type, COMPUTE_IMAGE_TYPE type = fixed_image_type,
				  typename scalar_type = conditional_t<image_bits_of_channel(type, 0) <= 32,
													   conditional_t<(type & COMPUTE_IMAGE_TYPE::__DATA_TYPE_MASK) == COMPUTE_IMAGE_TYPE::INT, int32_t, uint32_t>,
													   conditional_t<(type & COMPUTE_IMAGE_TYPE::__DATA_TYPE_MASK) == COMPUTE_IMAGE_TYPE::INT, int64_t, uint64_t>>,
				  enable_if_t<(!has_flag<COMPUTE_IMAGE_TYPE::FLAG_NORMALIZED>(type) &&
							   ((type & COMPUTE_IMAGE_TYPE::__DATA_TYPE_MASK) == COMPUTE_IMAGE_TYPE::INT ||
								(type & COMPUTE_IMAGE_TYPE::__DATA_TYPE_MASK) == COMPUTE_IMAGE_TYPE::UINT))>* = nullptr>
		static void write(const host_device_image<type, is_lod, is_lod_float, is_bias>* img,
						  const coord_type& coord,
						  const uint32_t layer,
						  const uint32_t lod,
						  const vector_n<scalar_type, 4>& color) {
			// figure out the storage type/format of the image and create (cast to) the correct storage type from the input
			constexpr const bool is_array = has_flag<COMPUTE_IMAGE_TYPE::FLAG_ARRAY>(type);
			constexpr const auto channel_count = image_channel_count(type);
			typedef vector_n<typename image_sized_data_type<type, image_bits_of_channel(type, 0)>::type, channel_count> storage_type;
			static_assert(sizeof(storage_type) == image_bytes_per_pixel(type), "invalid storage type size!");
			const storage_type raw_data = (storage_type)color; // nop if 32-bit/64-bit, cast down if 8-bit or 16-bit
			const auto offset = __builtin_choose_expr(!is_array,
													  coord_to_offset(img->image_dim, process_coord(img->image_clamp_dim, coord)),
													  coord_to_offset(img->image_dim, process_coord(img->image_clamp_dim, coord), layer));
			memcpy(&img->data[offset], raw_data);
		}
	};
}

template <COMPUTE_IMAGE_TYPE sample_image_type, bool is_lod = false, bool is_lod_float = false, bool is_bias = false>
struct host_device_image {
	typedef conditional_t<(has_flag<COMPUTE_IMAGE_TYPE::READ>(sample_image_type) &&
						   !has_flag<COMPUTE_IMAGE_TYPE::WRITE>(sample_image_type)), const uint8_t, uint8_t> storage_type;
	
	storage_type* data;
	const COMPUTE_IMAGE_TYPE runtime_image_type;
	const uint4 image_dim;
	const host_image_impl::image_clamp_dim_type image_clamp_dim;
	
	// statically/compile-time known base type that won't change at run-time
	floor_inline_always static constexpr COMPUTE_IMAGE_TYPE fixed_base_type() {
		// variable at run-time: channel count, format, data type, normalized flag
		return sample_image_type & (COMPUTE_IMAGE_TYPE::__DIM_MASK |
									COMPUTE_IMAGE_TYPE::__ACCESS_MASK |
									COMPUTE_IMAGE_TYPE::FLAG_ARRAY |
									COMPUTE_IMAGE_TYPE::FLAG_BUFFER |
									COMPUTE_IMAGE_TYPE::FLAG_CUBE |
									COMPUTE_IMAGE_TYPE::FLAG_DEPTH |
									COMPUTE_IMAGE_TYPE::FLAG_STENCIL |
									COMPUTE_IMAGE_TYPE::FLAG_GATHER |
									COMPUTE_IMAGE_TYPE::FLAG_NORMALIZED);
	}
	
#define FLOOR_RT_READ_IMAGE_CASE(rt_base_type) case (rt_base_type): \
return host_image_impl::fixed_image<(rt_base_type | fixed_base_type()), is_lod, is_lod_float, is_bias>::read( \
(const host_device_image<(rt_base_type | fixed_base_type()), is_lod, is_lod_float, is_bias>*)img, std::forward<Args>(args)...);

#define FLOOR_RT_WRITE_IMAGE_CASE(rt_base_type) case (rt_base_type): \
host_image_impl::fixed_image<(rt_base_type | fixed_base_type()), is_lod, is_lod_float, is_bias>::write( \
(const host_device_image<(rt_base_type | fixed_base_type()), is_lod, is_lod_float, is_bias>*)img, std::forward<Args>(args)...); return;

FLOOR_PUSH_WARNINGS()
FLOOR_IGNORE_WARNING(switch) // ignore "case value not in enumerated type 'COMPUTE_IMAGE_TYPE'" warnings, this is expected
	
	// normalized or float data (not double)
	template <COMPUTE_IMAGE_TYPE type = sample_image_type, typename... Args,
			  enable_if_t<(// float or normalized int/uint
						   (has_flag<COMPUTE_IMAGE_TYPE::FLAG_NORMALIZED>(type) ||
							(type & COMPUTE_IMAGE_TYPE::__DATA_TYPE_MASK) == COMPUTE_IMAGE_TYPE::FLOAT) &&
						   // !double format, TODO: support this
						   /*!((type & COMPUTE_IMAGE_TYPE::__FORMAT_MASK) == COMPUTE_IMAGE_TYPE::FORMAT_64 &&
							 (type & COMPUTE_IMAGE_TYPE::__DATA_TYPE_MASK) == COMPUTE_IMAGE_TYPE::FLOAT) &&*/
						   // !depth
						   !has_flag<COMPUTE_IMAGE_TYPE::FLAG_DEPTH>(type))>* = nullptr>
	static auto read(const host_device_image<sample_image_type, is_lod, is_lod_float, is_bias>* img, Args&&... args) {
		const auto runtime_base_type = img->runtime_image_type & (COMPUTE_IMAGE_TYPE::__FORMAT_MASK |
																  COMPUTE_IMAGE_TYPE::__CHANNELS_MASK |
																  COMPUTE_IMAGE_TYPE::__DATA_TYPE_MASK |
																  COMPUTE_IMAGE_TYPE::FLAG_NORMALIZED);
		// normalized int/uint
		if(has_flag<COMPUTE_IMAGE_TYPE::FLAG_NORMALIZED>(runtime_base_type)) {
			switch(runtime_base_type) {
				FLOOR_RT_READ_IMAGE_CASE(COMPUTE_IMAGE_TYPE::FORMAT_2 | COMPUTE_IMAGE_TYPE::CHANNELS_1 | COMPUTE_IMAGE_TYPE::INT | COMPUTE_IMAGE_TYPE::FLAG_NORMALIZED)
				FLOOR_RT_READ_IMAGE_CASE(COMPUTE_IMAGE_TYPE::FORMAT_2 | COMPUTE_IMAGE_TYPE::CHANNELS_2 | COMPUTE_IMAGE_TYPE::INT | COMPUTE_IMAGE_TYPE::FLAG_NORMALIZED)
				FLOOR_RT_READ_IMAGE_CASE(COMPUTE_IMAGE_TYPE::FORMAT_2 | COMPUTE_IMAGE_TYPE::CHANNELS_3 | COMPUTE_IMAGE_TYPE::INT | COMPUTE_IMAGE_TYPE::FLAG_NORMALIZED)
				FLOOR_RT_READ_IMAGE_CASE(COMPUTE_IMAGE_TYPE::FORMAT_2 | COMPUTE_IMAGE_TYPE::CHANNELS_4 | COMPUTE_IMAGE_TYPE::INT | COMPUTE_IMAGE_TYPE::FLAG_NORMALIZED)
				FLOOR_RT_READ_IMAGE_CASE(COMPUTE_IMAGE_TYPE::FORMAT_2 | COMPUTE_IMAGE_TYPE::CHANNELS_1 | COMPUTE_IMAGE_TYPE::UINT | COMPUTE_IMAGE_TYPE::FLAG_NORMALIZED)
				FLOOR_RT_READ_IMAGE_CASE(COMPUTE_IMAGE_TYPE::FORMAT_2 | COMPUTE_IMAGE_TYPE::CHANNELS_2 | COMPUTE_IMAGE_TYPE::UINT | COMPUTE_IMAGE_TYPE::FLAG_NORMALIZED)
				FLOOR_RT_READ_IMAGE_CASE(COMPUTE_IMAGE_TYPE::FORMAT_2 | COMPUTE_IMAGE_TYPE::CHANNELS_3 | COMPUTE_IMAGE_TYPE::UINT | COMPUTE_IMAGE_TYPE::FLAG_NORMALIZED)
				FLOOR_RT_READ_IMAGE_CASE(COMPUTE_IMAGE_TYPE::FORMAT_2 | COMPUTE_IMAGE_TYPE::CHANNELS_4 | COMPUTE_IMAGE_TYPE::UINT | COMPUTE_IMAGE_TYPE::FLAG_NORMALIZED)
				
				FLOOR_RT_READ_IMAGE_CASE(COMPUTE_IMAGE_TYPE::FORMAT_4 | COMPUTE_IMAGE_TYPE::CHANNELS_1 | COMPUTE_IMAGE_TYPE::INT | COMPUTE_IMAGE_TYPE::FLAG_NORMALIZED)
				FLOOR_RT_READ_IMAGE_CASE(COMPUTE_IMAGE_TYPE::FORMAT_4 | COMPUTE_IMAGE_TYPE::CHANNELS_2 | COMPUTE_IMAGE_TYPE::INT | COMPUTE_IMAGE_TYPE::FLAG_NORMALIZED)
				FLOOR_RT_READ_IMAGE_CASE(COMPUTE_IMAGE_TYPE::FORMAT_4 | COMPUTE_IMAGE_TYPE::CHANNELS_3 | COMPUTE_IMAGE_TYPE::INT | COMPUTE_IMAGE_TYPE::FLAG_NORMALIZED)
				FLOOR_RT_READ_IMAGE_CASE(COMPUTE_IMAGE_TYPE::FORMAT_4 | COMPUTE_IMAGE_TYPE::CHANNELS_4 | COMPUTE_IMAGE_TYPE::INT | COMPUTE_IMAGE_TYPE::FLAG_NORMALIZED)
				FLOOR_RT_READ_IMAGE_CASE(COMPUTE_IMAGE_TYPE::FORMAT_4 | COMPUTE_IMAGE_TYPE::CHANNELS_1 | COMPUTE_IMAGE_TYPE::UINT | COMPUTE_IMAGE_TYPE::FLAG_NORMALIZED)
				FLOOR_RT_READ_IMAGE_CASE(COMPUTE_IMAGE_TYPE::FORMAT_4 | COMPUTE_IMAGE_TYPE::CHANNELS_2 | COMPUTE_IMAGE_TYPE::UINT | COMPUTE_IMAGE_TYPE::FLAG_NORMALIZED)
				FLOOR_RT_READ_IMAGE_CASE(COMPUTE_IMAGE_TYPE::FORMAT_4 | COMPUTE_IMAGE_TYPE::CHANNELS_3 | COMPUTE_IMAGE_TYPE::UINT | COMPUTE_IMAGE_TYPE::FLAG_NORMALIZED)
				FLOOR_RT_READ_IMAGE_CASE(COMPUTE_IMAGE_TYPE::FORMAT_4 | COMPUTE_IMAGE_TYPE::CHANNELS_4 | COMPUTE_IMAGE_TYPE::UINT | COMPUTE_IMAGE_TYPE::FLAG_NORMALIZED)
				
				FLOOR_RT_READ_IMAGE_CASE(COMPUTE_IMAGE_TYPE::FORMAT_8 | COMPUTE_IMAGE_TYPE::CHANNELS_1 | COMPUTE_IMAGE_TYPE::INT | COMPUTE_IMAGE_TYPE::FLAG_NORMALIZED)
				FLOOR_RT_READ_IMAGE_CASE(COMPUTE_IMAGE_TYPE::FORMAT_8 | COMPUTE_IMAGE_TYPE::CHANNELS_2 | COMPUTE_IMAGE_TYPE::INT | COMPUTE_IMAGE_TYPE::FLAG_NORMALIZED)
				FLOOR_RT_READ_IMAGE_CASE(COMPUTE_IMAGE_TYPE::FORMAT_8 | COMPUTE_IMAGE_TYPE::CHANNELS_3 | COMPUTE_IMAGE_TYPE::INT | COMPUTE_IMAGE_TYPE::FLAG_NORMALIZED)
				FLOOR_RT_READ_IMAGE_CASE(COMPUTE_IMAGE_TYPE::FORMAT_8 | COMPUTE_IMAGE_TYPE::CHANNELS_4 | COMPUTE_IMAGE_TYPE::INT | COMPUTE_IMAGE_TYPE::FLAG_NORMALIZED)
				FLOOR_RT_READ_IMAGE_CASE(COMPUTE_IMAGE_TYPE::FORMAT_8 | COMPUTE_IMAGE_TYPE::CHANNELS_1 | COMPUTE_IMAGE_TYPE::UINT | COMPUTE_IMAGE_TYPE::FLAG_NORMALIZED)
				FLOOR_RT_READ_IMAGE_CASE(COMPUTE_IMAGE_TYPE::FORMAT_8 | COMPUTE_IMAGE_TYPE::CHANNELS_2 | COMPUTE_IMAGE_TYPE::UINT | COMPUTE_IMAGE_TYPE::FLAG_NORMALIZED)
				FLOOR_RT_READ_IMAGE_CASE(COMPUTE_IMAGE_TYPE::FORMAT_8 | COMPUTE_IMAGE_TYPE::CHANNELS_3 | COMPUTE_IMAGE_TYPE::UINT | COMPUTE_IMAGE_TYPE::FLAG_NORMALIZED)
				FLOOR_RT_READ_IMAGE_CASE(COMPUTE_IMAGE_TYPE::FORMAT_8 | COMPUTE_IMAGE_TYPE::CHANNELS_4 | COMPUTE_IMAGE_TYPE::UINT | COMPUTE_IMAGE_TYPE::FLAG_NORMALIZED)
				
				FLOOR_RT_READ_IMAGE_CASE(COMPUTE_IMAGE_TYPE::FORMAT_16 | COMPUTE_IMAGE_TYPE::CHANNELS_1 | COMPUTE_IMAGE_TYPE::INT | COMPUTE_IMAGE_TYPE::FLAG_NORMALIZED)
				FLOOR_RT_READ_IMAGE_CASE(COMPUTE_IMAGE_TYPE::FORMAT_16 | COMPUTE_IMAGE_TYPE::CHANNELS_2 | COMPUTE_IMAGE_TYPE::INT | COMPUTE_IMAGE_TYPE::FLAG_NORMALIZED)
				FLOOR_RT_READ_IMAGE_CASE(COMPUTE_IMAGE_TYPE::FORMAT_16 | COMPUTE_IMAGE_TYPE::CHANNELS_3 | COMPUTE_IMAGE_TYPE::INT | COMPUTE_IMAGE_TYPE::FLAG_NORMALIZED)
				FLOOR_RT_READ_IMAGE_CASE(COMPUTE_IMAGE_TYPE::FORMAT_16 | COMPUTE_IMAGE_TYPE::CHANNELS_4 | COMPUTE_IMAGE_TYPE::INT | COMPUTE_IMAGE_TYPE::FLAG_NORMALIZED)
				FLOOR_RT_READ_IMAGE_CASE(COMPUTE_IMAGE_TYPE::FORMAT_16 | COMPUTE_IMAGE_TYPE::CHANNELS_1 | COMPUTE_IMAGE_TYPE::UINT | COMPUTE_IMAGE_TYPE::FLAG_NORMALIZED)
				FLOOR_RT_READ_IMAGE_CASE(COMPUTE_IMAGE_TYPE::FORMAT_16 | COMPUTE_IMAGE_TYPE::CHANNELS_2 | COMPUTE_IMAGE_TYPE::UINT | COMPUTE_IMAGE_TYPE::FLAG_NORMALIZED)
				FLOOR_RT_READ_IMAGE_CASE(COMPUTE_IMAGE_TYPE::FORMAT_16 | COMPUTE_IMAGE_TYPE::CHANNELS_3 | COMPUTE_IMAGE_TYPE::UINT | COMPUTE_IMAGE_TYPE::FLAG_NORMALIZED)
				FLOOR_RT_READ_IMAGE_CASE(COMPUTE_IMAGE_TYPE::FORMAT_16 | COMPUTE_IMAGE_TYPE::CHANNELS_4 | COMPUTE_IMAGE_TYPE::UINT | COMPUTE_IMAGE_TYPE::FLAG_NORMALIZED)
				
				FLOOR_RT_READ_IMAGE_CASE(COMPUTE_IMAGE_TYPE::FORMAT_32 | COMPUTE_IMAGE_TYPE::CHANNELS_1 | COMPUTE_IMAGE_TYPE::INT | COMPUTE_IMAGE_TYPE::FLAG_NORMALIZED)
				FLOOR_RT_READ_IMAGE_CASE(COMPUTE_IMAGE_TYPE::FORMAT_32 | COMPUTE_IMAGE_TYPE::CHANNELS_2 | COMPUTE_IMAGE_TYPE::INT | COMPUTE_IMAGE_TYPE::FLAG_NORMALIZED)
				FLOOR_RT_READ_IMAGE_CASE(COMPUTE_IMAGE_TYPE::FORMAT_32 | COMPUTE_IMAGE_TYPE::CHANNELS_3 | COMPUTE_IMAGE_TYPE::INT | COMPUTE_IMAGE_TYPE::FLAG_NORMALIZED)
				FLOOR_RT_READ_IMAGE_CASE(COMPUTE_IMAGE_TYPE::FORMAT_32 | COMPUTE_IMAGE_TYPE::CHANNELS_4 | COMPUTE_IMAGE_TYPE::INT | COMPUTE_IMAGE_TYPE::FLAG_NORMALIZED)
				FLOOR_RT_READ_IMAGE_CASE(COMPUTE_IMAGE_TYPE::FORMAT_32 | COMPUTE_IMAGE_TYPE::CHANNELS_1 | COMPUTE_IMAGE_TYPE::UINT | COMPUTE_IMAGE_TYPE::FLAG_NORMALIZED)
				FLOOR_RT_READ_IMAGE_CASE(COMPUTE_IMAGE_TYPE::FORMAT_32 | COMPUTE_IMAGE_TYPE::CHANNELS_2 | COMPUTE_IMAGE_TYPE::UINT | COMPUTE_IMAGE_TYPE::FLAG_NORMALIZED)
				FLOOR_RT_READ_IMAGE_CASE(COMPUTE_IMAGE_TYPE::FORMAT_32 | COMPUTE_IMAGE_TYPE::CHANNELS_3 | COMPUTE_IMAGE_TYPE::UINT | COMPUTE_IMAGE_TYPE::FLAG_NORMALIZED)
				FLOOR_RT_READ_IMAGE_CASE(COMPUTE_IMAGE_TYPE::FORMAT_32 | COMPUTE_IMAGE_TYPE::CHANNELS_4 | COMPUTE_IMAGE_TYPE::UINT | COMPUTE_IMAGE_TYPE::FLAG_NORMALIZED)
				
				FLOOR_RT_READ_IMAGE_CASE(COMPUTE_IMAGE_TYPE::FORMAT_64 | COMPUTE_IMAGE_TYPE::CHANNELS_1 | COMPUTE_IMAGE_TYPE::INT | COMPUTE_IMAGE_TYPE::FLAG_NORMALIZED)
				FLOOR_RT_READ_IMAGE_CASE(COMPUTE_IMAGE_TYPE::FORMAT_64 | COMPUTE_IMAGE_TYPE::CHANNELS_2 | COMPUTE_IMAGE_TYPE::INT | COMPUTE_IMAGE_TYPE::FLAG_NORMALIZED)
				FLOOR_RT_READ_IMAGE_CASE(COMPUTE_IMAGE_TYPE::FORMAT_64 | COMPUTE_IMAGE_TYPE::CHANNELS_3 | COMPUTE_IMAGE_TYPE::INT | COMPUTE_IMAGE_TYPE::FLAG_NORMALIZED)
				FLOOR_RT_READ_IMAGE_CASE(COMPUTE_IMAGE_TYPE::FORMAT_64 | COMPUTE_IMAGE_TYPE::CHANNELS_4 | COMPUTE_IMAGE_TYPE::INT | COMPUTE_IMAGE_TYPE::FLAG_NORMALIZED)
				FLOOR_RT_READ_IMAGE_CASE(COMPUTE_IMAGE_TYPE::FORMAT_64 | COMPUTE_IMAGE_TYPE::CHANNELS_1 | COMPUTE_IMAGE_TYPE::UINT | COMPUTE_IMAGE_TYPE::FLAG_NORMALIZED)
				FLOOR_RT_READ_IMAGE_CASE(COMPUTE_IMAGE_TYPE::FORMAT_64 | COMPUTE_IMAGE_TYPE::CHANNELS_2 | COMPUTE_IMAGE_TYPE::UINT | COMPUTE_IMAGE_TYPE::FLAG_NORMALIZED)
				FLOOR_RT_READ_IMAGE_CASE(COMPUTE_IMAGE_TYPE::FORMAT_64 | COMPUTE_IMAGE_TYPE::CHANNELS_3 | COMPUTE_IMAGE_TYPE::UINT | COMPUTE_IMAGE_TYPE::FLAG_NORMALIZED)
				FLOOR_RT_READ_IMAGE_CASE(COMPUTE_IMAGE_TYPE::FORMAT_64 | COMPUTE_IMAGE_TYPE::CHANNELS_4 | COMPUTE_IMAGE_TYPE::UINT | COMPUTE_IMAGE_TYPE::FLAG_NORMALIZED)
				
				default: floor_unreachable();
			}
		}
		// non-normalized float
		else {
			switch(runtime_base_type) {
				FLOOR_RT_READ_IMAGE_CASE(COMPUTE_IMAGE_TYPE::FORMAT_16 | COMPUTE_IMAGE_TYPE::CHANNELS_1 | COMPUTE_IMAGE_TYPE::FLOAT)
				FLOOR_RT_READ_IMAGE_CASE(COMPUTE_IMAGE_TYPE::FORMAT_16 | COMPUTE_IMAGE_TYPE::CHANNELS_2 | COMPUTE_IMAGE_TYPE::FLOAT)
				FLOOR_RT_READ_IMAGE_CASE(COMPUTE_IMAGE_TYPE::FORMAT_16 | COMPUTE_IMAGE_TYPE::CHANNELS_3 | COMPUTE_IMAGE_TYPE::FLOAT)
				FLOOR_RT_READ_IMAGE_CASE(COMPUTE_IMAGE_TYPE::FORMAT_16 | COMPUTE_IMAGE_TYPE::CHANNELS_4 | COMPUTE_IMAGE_TYPE::FLOAT)
				
				FLOOR_RT_READ_IMAGE_CASE(COMPUTE_IMAGE_TYPE::FORMAT_32 | COMPUTE_IMAGE_TYPE::CHANNELS_1 | COMPUTE_IMAGE_TYPE::FLOAT)
				FLOOR_RT_READ_IMAGE_CASE(COMPUTE_IMAGE_TYPE::FORMAT_32 | COMPUTE_IMAGE_TYPE::CHANNELS_2 | COMPUTE_IMAGE_TYPE::FLOAT)
				FLOOR_RT_READ_IMAGE_CASE(COMPUTE_IMAGE_TYPE::FORMAT_32 | COMPUTE_IMAGE_TYPE::CHANNELS_3 | COMPUTE_IMAGE_TYPE::FLOAT)
				FLOOR_RT_READ_IMAGE_CASE(COMPUTE_IMAGE_TYPE::FORMAT_32 | COMPUTE_IMAGE_TYPE::CHANNELS_4 | COMPUTE_IMAGE_TYPE::FLOAT)
				
				default: floor_unreachable();
			}
		}
	}
	
#if 0 // TODO/NOTE: not supported yet
	// double data
	template <COMPUTE_IMAGE_TYPE type = sample_image_type, typename... Args,
			  enable_if_t<(// dobule
						   (type & COMPUTE_IMAGE_TYPE::__FORMAT_MASK) == COMPUTE_IMAGE_TYPE::FORMAT_64 &&
						   (type & COMPUTE_IMAGE_TYPE::__DATA_TYPE_MASK) == COMPUTE_IMAGE_TYPE::FLOAT &&
						   // !normalized
						   !has_flag<COMPUTE_IMAGE_TYPE::FLAG_NORMALIZED>(type) &&
						   // !depth
						   !has_flag<COMPUTE_IMAGE_TYPE::FLAG_DEPTH>(type))>* = nullptr>
	auto read(const host_device_image<sample_image_type, is_lod, is_lod_float, is_bias>* img, Args&&... args) {
		const auto runtime_base_type = img->runtime_image_type & (COMPUTE_IMAGE_TYPE::__FORMAT_MASK |
																  COMPUTE_IMAGE_TYPE::__CHANNELS_MASK |
																  COMPUTE_IMAGE_TYPE::__DATA_TYPE_MASK);
		switch(runtime_base_type) {
			FLOOR_RT_READ_IMAGE_CASE(COMPUTE_IMAGE_TYPE::FORMAT_64 | COMPUTE_IMAGE_TYPE::CHANNELS_1 | COMPUTE_IMAGE_TYPE::FLOAT)
			FLOOR_RT_READ_IMAGE_CASE(COMPUTE_IMAGE_TYPE::FORMAT_64 | COMPUTE_IMAGE_TYPE::CHANNELS_2 | COMPUTE_IMAGE_TYPE::FLOAT)
			FLOOR_RT_READ_IMAGE_CASE(COMPUTE_IMAGE_TYPE::FORMAT_64 | COMPUTE_IMAGE_TYPE::CHANNELS_3 | COMPUTE_IMAGE_TYPE::FLOAT)
			FLOOR_RT_READ_IMAGE_CASE(COMPUTE_IMAGE_TYPE::FORMAT_64 | COMPUTE_IMAGE_TYPE::CHANNELS_4 | COMPUTE_IMAGE_TYPE::FLOAT)
			
			default: floor_unreachable();
		}
	}
#endif
	
	// depth float
	template <COMPUTE_IMAGE_TYPE type = sample_image_type, typename... Args,
			  enable_if_t<(has_flag<COMPUTE_IMAGE_TYPE::FLAG_DEPTH>(type) &&
						   !has_flag<COMPUTE_IMAGE_TYPE::FLAG_STENCIL>(type))>* = nullptr>
	static auto read(const host_device_image<sample_image_type, is_lod, is_lod_float, is_bias>* img, Args&&... args) {
		const auto runtime_base_type = img->runtime_image_type & (COMPUTE_IMAGE_TYPE::__FORMAT_MASK |
																  COMPUTE_IMAGE_TYPE::__CHANNELS_MASK |
																  COMPUTE_IMAGE_TYPE::__DATA_TYPE_MASK |
																  COMPUTE_IMAGE_TYPE::FLAG_DEPTH |
																  COMPUTE_IMAGE_TYPE::FLAG_STENCIL);
		switch(runtime_base_type) {
			FLOOR_RT_READ_IMAGE_CASE(COMPUTE_IMAGE_TYPE::FORMAT_16 | COMPUTE_IMAGE_TYPE::CHANNELS_1 | COMPUTE_IMAGE_TYPE::UINT | COMPUTE_IMAGE_TYPE::FLAG_DEPTH)
			FLOOR_RT_READ_IMAGE_CASE(COMPUTE_IMAGE_TYPE::FORMAT_24 | COMPUTE_IMAGE_TYPE::CHANNELS_1 | COMPUTE_IMAGE_TYPE::UINT | COMPUTE_IMAGE_TYPE::FLAG_DEPTH)
			FLOOR_RT_READ_IMAGE_CASE(COMPUTE_IMAGE_TYPE::FORMAT_32 | COMPUTE_IMAGE_TYPE::CHANNELS_1 | COMPUTE_IMAGE_TYPE::UINT | COMPUTE_IMAGE_TYPE::FLAG_DEPTH)
			FLOOR_RT_READ_IMAGE_CASE(COMPUTE_IMAGE_TYPE::FORMAT_32 | COMPUTE_IMAGE_TYPE::CHANNELS_1 | COMPUTE_IMAGE_TYPE::FLOAT | COMPUTE_IMAGE_TYPE::FLAG_DEPTH)
			default: floor_unreachable();
		}
	}
	
	// depth+stencil float+uint8_t
	template <COMPUTE_IMAGE_TYPE type = sample_image_type, typename... Args,
			  enable_if_t<(has_flag<COMPUTE_IMAGE_TYPE::FLAG_DEPTH>(type) &&
						   has_flag<COMPUTE_IMAGE_TYPE::FLAG_STENCIL>(type))>* = nullptr>
	static auto read(const host_device_image<sample_image_type, is_lod, is_lod_float, is_bias>* img, Args&&... args) {
		const auto runtime_base_type = img->runtime_image_type & (COMPUTE_IMAGE_TYPE::__FORMAT_MASK |
																  COMPUTE_IMAGE_TYPE::__CHANNELS_MASK |
																  COMPUTE_IMAGE_TYPE::__DATA_TYPE_MASK |
																  COMPUTE_IMAGE_TYPE::FLAG_DEPTH |
																  COMPUTE_IMAGE_TYPE::FLAG_STENCIL);
		switch(runtime_base_type) {
			FLOOR_RT_READ_IMAGE_CASE(COMPUTE_IMAGE_TYPE::FORMAT_24_8 | COMPUTE_IMAGE_TYPE::CHANNELS_2 | COMPUTE_IMAGE_TYPE::UINT | COMPUTE_IMAGE_TYPE::FLAG_DEPTH | COMPUTE_IMAGE_TYPE::FLAG_STENCIL)
			FLOOR_RT_READ_IMAGE_CASE(COMPUTE_IMAGE_TYPE::FORMAT_32_8 | COMPUTE_IMAGE_TYPE::CHANNELS_2 | COMPUTE_IMAGE_TYPE::FLOAT | COMPUTE_IMAGE_TYPE::FLAG_DEPTH | COMPUTE_IMAGE_TYPE::FLAG_STENCIL)
			default: floor_unreachable();
		}
	}
	
	// non-normalized int/uint data (<= 32-bit)
	template <COMPUTE_IMAGE_TYPE type = sample_image_type, typename... Args,
			  COMPUTE_IMAGE_TYPE fixed_data_type = (type & COMPUTE_IMAGE_TYPE::__DATA_TYPE_MASK),
			  enable_if_t<(!has_flag<COMPUTE_IMAGE_TYPE::FLAG_NORMALIZED>(type) &&
						   (fixed_data_type == COMPUTE_IMAGE_TYPE::INT ||
							fixed_data_type == COMPUTE_IMAGE_TYPE::UINT) /*&&
						   (type & COMPUTE_IMAGE_TYPE::__FORMAT_MASK) != COMPUTE_IMAGE_TYPE::FORMAT_64*/)>* = nullptr>
	static auto read(const host_device_image<sample_image_type, is_lod, is_lod_float, is_bias>* img, Args&&... args) {
		const auto runtime_base_type = img->runtime_image_type & (COMPUTE_IMAGE_TYPE::__FORMAT_MASK |
																  COMPUTE_IMAGE_TYPE::__CHANNELS_MASK |
																  COMPUTE_IMAGE_TYPE::__DATA_TYPE_MASK);
		switch(runtime_base_type) {
			FLOOR_RT_READ_IMAGE_CASE(COMPUTE_IMAGE_TYPE::FORMAT_2 | COMPUTE_IMAGE_TYPE::CHANNELS_1 | fixed_data_type)
			FLOOR_RT_READ_IMAGE_CASE(COMPUTE_IMAGE_TYPE::FORMAT_2 | COMPUTE_IMAGE_TYPE::CHANNELS_2 | fixed_data_type)
			FLOOR_RT_READ_IMAGE_CASE(COMPUTE_IMAGE_TYPE::FORMAT_2 | COMPUTE_IMAGE_TYPE::CHANNELS_3 | fixed_data_type)
			FLOOR_RT_READ_IMAGE_CASE(COMPUTE_IMAGE_TYPE::FORMAT_2 | COMPUTE_IMAGE_TYPE::CHANNELS_4 | fixed_data_type)
			
			FLOOR_RT_READ_IMAGE_CASE(COMPUTE_IMAGE_TYPE::FORMAT_4 | COMPUTE_IMAGE_TYPE::CHANNELS_1 | fixed_data_type)
			FLOOR_RT_READ_IMAGE_CASE(COMPUTE_IMAGE_TYPE::FORMAT_4 | COMPUTE_IMAGE_TYPE::CHANNELS_2 | fixed_data_type)
			FLOOR_RT_READ_IMAGE_CASE(COMPUTE_IMAGE_TYPE::FORMAT_4 | COMPUTE_IMAGE_TYPE::CHANNELS_3 | fixed_data_type)
			FLOOR_RT_READ_IMAGE_CASE(COMPUTE_IMAGE_TYPE::FORMAT_4 | COMPUTE_IMAGE_TYPE::CHANNELS_4 | fixed_data_type)
			
			FLOOR_RT_READ_IMAGE_CASE(COMPUTE_IMAGE_TYPE::FORMAT_8 | COMPUTE_IMAGE_TYPE::CHANNELS_1 | fixed_data_type)
			FLOOR_RT_READ_IMAGE_CASE(COMPUTE_IMAGE_TYPE::FORMAT_8 | COMPUTE_IMAGE_TYPE::CHANNELS_2 | fixed_data_type)
			FLOOR_RT_READ_IMAGE_CASE(COMPUTE_IMAGE_TYPE::FORMAT_8 | COMPUTE_IMAGE_TYPE::CHANNELS_3 | fixed_data_type)
			FLOOR_RT_READ_IMAGE_CASE(COMPUTE_IMAGE_TYPE::FORMAT_8 | COMPUTE_IMAGE_TYPE::CHANNELS_4 | fixed_data_type)
			
			FLOOR_RT_READ_IMAGE_CASE(COMPUTE_IMAGE_TYPE::FORMAT_16 | COMPUTE_IMAGE_TYPE::CHANNELS_1 | fixed_data_type)
			FLOOR_RT_READ_IMAGE_CASE(COMPUTE_IMAGE_TYPE::FORMAT_16 | COMPUTE_IMAGE_TYPE::CHANNELS_2 | fixed_data_type)
			FLOOR_RT_READ_IMAGE_CASE(COMPUTE_IMAGE_TYPE::FORMAT_16 | COMPUTE_IMAGE_TYPE::CHANNELS_3 | fixed_data_type)
			FLOOR_RT_READ_IMAGE_CASE(COMPUTE_IMAGE_TYPE::FORMAT_16 | COMPUTE_IMAGE_TYPE::CHANNELS_4 | fixed_data_type)
			
			FLOOR_RT_READ_IMAGE_CASE(COMPUTE_IMAGE_TYPE::FORMAT_32 | COMPUTE_IMAGE_TYPE::CHANNELS_1 | fixed_data_type)
			FLOOR_RT_READ_IMAGE_CASE(COMPUTE_IMAGE_TYPE::FORMAT_32 | COMPUTE_IMAGE_TYPE::CHANNELS_2 | fixed_data_type)
			FLOOR_RT_READ_IMAGE_CASE(COMPUTE_IMAGE_TYPE::FORMAT_32 | COMPUTE_IMAGE_TYPE::CHANNELS_3 | fixed_data_type)
			FLOOR_RT_READ_IMAGE_CASE(COMPUTE_IMAGE_TYPE::FORMAT_32 | COMPUTE_IMAGE_TYPE::CHANNELS_4 | fixed_data_type)
			
			default: floor_unreachable();
		}
	}
	
#if 0 // TODO/NOTE: not supported yet
	// non-normalized int/uint data (== 64-bit)
	template <COMPUTE_IMAGE_TYPE type = sample_image_type, typename... Args,
			  COMPUTE_IMAGE_TYPE fixed_data_type = (type & COMPUTE_IMAGE_TYPE::__DATA_TYPE_MASK),
			  enable_if_t<(!has_flag<COMPUTE_IMAGE_TYPE::FLAG_NORMALIZED>(type) &&
						   (fixed_data_type == COMPUTE_IMAGE_TYPE::INT ||
							fixed_data_type == COMPUTE_IMAGE_TYPE::UINT) &&
						   (type & COMPUTE_IMAGE_TYPE::__FORMAT_MASK) == COMPUTE_IMAGE_TYPE::FORMAT_64)>* = nullptr>
	auto read(const host_device_image<sample_image_type, is_lod, is_lod_float, is_bias>* img, Args&&... args) {
		const auto runtime_base_type = img->runtime_image_type & (COMPUTE_IMAGE_TYPE::__FORMAT_MASK |
																  COMPUTE_IMAGE_TYPE::__CHANNELS_MASK |
																  COMPUTE_IMAGE_TYPE::__DATA_TYPE_MASK);
		switch(runtime_base_type) {
			FLOOR_RT_READ_IMAGE_CASE(COMPUTE_IMAGE_TYPE::FORMAT_64 | COMPUTE_IMAGE_TYPE::CHANNELS_1 | fixed_data_type)
			FLOOR_RT_READ_IMAGE_CASE(COMPUTE_IMAGE_TYPE::FORMAT_64 | COMPUTE_IMAGE_TYPE::CHANNELS_2 | fixed_data_type)
			FLOOR_RT_READ_IMAGE_CASE(COMPUTE_IMAGE_TYPE::FORMAT_64 | COMPUTE_IMAGE_TYPE::CHANNELS_3 | fixed_data_type)
			FLOOR_RT_READ_IMAGE_CASE(COMPUTE_IMAGE_TYPE::FORMAT_64 | COMPUTE_IMAGE_TYPE::CHANNELS_4 | fixed_data_type)
			
			default: floor_unreachable();
		}
	}
#endif
	
	// normalized or float data (not double)
	template <COMPUTE_IMAGE_TYPE type = sample_image_type, typename... Args,
			  enable_if_t<(// float or normalized int/uint
						   (has_flag<COMPUTE_IMAGE_TYPE::FLAG_NORMALIZED>(type) ||
							(type & COMPUTE_IMAGE_TYPE::__DATA_TYPE_MASK) == COMPUTE_IMAGE_TYPE::FLOAT) &&
						   // !double format
						   /*!((type & COMPUTE_IMAGE_TYPE::__FORMAT_MASK) == COMPUTE_IMAGE_TYPE::FORMAT_64 &&
							 (type & COMPUTE_IMAGE_TYPE::__DATA_TYPE_MASK) == COMPUTE_IMAGE_TYPE::FLOAT) &&*/
						   // !depth
						   !has_flag<COMPUTE_IMAGE_TYPE::FLAG_DEPTH>(type))>* = nullptr>
	static void write(const host_device_image<sample_image_type, is_lod, is_lod_float, is_bias>* img, Args&&... args) {
		const auto runtime_base_type = img->runtime_image_type & (COMPUTE_IMAGE_TYPE::__FORMAT_MASK |
																  COMPUTE_IMAGE_TYPE::__CHANNELS_MASK |
																  COMPUTE_IMAGE_TYPE::__DATA_TYPE_MASK |
																  COMPUTE_IMAGE_TYPE::FLAG_NORMALIZED);
		// normalized int/uint
		if(has_flag<COMPUTE_IMAGE_TYPE::FLAG_NORMALIZED>(runtime_base_type)) {
			switch(runtime_base_type) {
				FLOOR_RT_WRITE_IMAGE_CASE(COMPUTE_IMAGE_TYPE::FORMAT_2 | COMPUTE_IMAGE_TYPE::CHANNELS_1 | COMPUTE_IMAGE_TYPE::INT | COMPUTE_IMAGE_TYPE::FLAG_NORMALIZED)
				FLOOR_RT_WRITE_IMAGE_CASE(COMPUTE_IMAGE_TYPE::FORMAT_2 | COMPUTE_IMAGE_TYPE::CHANNELS_2 | COMPUTE_IMAGE_TYPE::INT | COMPUTE_IMAGE_TYPE::FLAG_NORMALIZED)
				FLOOR_RT_WRITE_IMAGE_CASE(COMPUTE_IMAGE_TYPE::FORMAT_2 | COMPUTE_IMAGE_TYPE::CHANNELS_3 | COMPUTE_IMAGE_TYPE::INT | COMPUTE_IMAGE_TYPE::FLAG_NORMALIZED)
				FLOOR_RT_WRITE_IMAGE_CASE(COMPUTE_IMAGE_TYPE::FORMAT_2 | COMPUTE_IMAGE_TYPE::CHANNELS_4 | COMPUTE_IMAGE_TYPE::INT | COMPUTE_IMAGE_TYPE::FLAG_NORMALIZED)
				FLOOR_RT_WRITE_IMAGE_CASE(COMPUTE_IMAGE_TYPE::FORMAT_2 | COMPUTE_IMAGE_TYPE::CHANNELS_1 | COMPUTE_IMAGE_TYPE::UINT | COMPUTE_IMAGE_TYPE::FLAG_NORMALIZED)
				FLOOR_RT_WRITE_IMAGE_CASE(COMPUTE_IMAGE_TYPE::FORMAT_2 | COMPUTE_IMAGE_TYPE::CHANNELS_2 | COMPUTE_IMAGE_TYPE::UINT | COMPUTE_IMAGE_TYPE::FLAG_NORMALIZED)
				FLOOR_RT_WRITE_IMAGE_CASE(COMPUTE_IMAGE_TYPE::FORMAT_2 | COMPUTE_IMAGE_TYPE::CHANNELS_3 | COMPUTE_IMAGE_TYPE::UINT | COMPUTE_IMAGE_TYPE::FLAG_NORMALIZED)
				FLOOR_RT_WRITE_IMAGE_CASE(COMPUTE_IMAGE_TYPE::FORMAT_2 | COMPUTE_IMAGE_TYPE::CHANNELS_4 | COMPUTE_IMAGE_TYPE::UINT | COMPUTE_IMAGE_TYPE::FLAG_NORMALIZED)
				
				FLOOR_RT_WRITE_IMAGE_CASE(COMPUTE_IMAGE_TYPE::FORMAT_4 | COMPUTE_IMAGE_TYPE::CHANNELS_1 | COMPUTE_IMAGE_TYPE::INT | COMPUTE_IMAGE_TYPE::FLAG_NORMALIZED)
				FLOOR_RT_WRITE_IMAGE_CASE(COMPUTE_IMAGE_TYPE::FORMAT_4 | COMPUTE_IMAGE_TYPE::CHANNELS_2 | COMPUTE_IMAGE_TYPE::INT | COMPUTE_IMAGE_TYPE::FLAG_NORMALIZED)
				FLOOR_RT_WRITE_IMAGE_CASE(COMPUTE_IMAGE_TYPE::FORMAT_4 | COMPUTE_IMAGE_TYPE::CHANNELS_3 | COMPUTE_IMAGE_TYPE::INT | COMPUTE_IMAGE_TYPE::FLAG_NORMALIZED)
				FLOOR_RT_WRITE_IMAGE_CASE(COMPUTE_IMAGE_TYPE::FORMAT_4 | COMPUTE_IMAGE_TYPE::CHANNELS_4 | COMPUTE_IMAGE_TYPE::INT | COMPUTE_IMAGE_TYPE::FLAG_NORMALIZED)
				FLOOR_RT_WRITE_IMAGE_CASE(COMPUTE_IMAGE_TYPE::FORMAT_4 | COMPUTE_IMAGE_TYPE::CHANNELS_1 | COMPUTE_IMAGE_TYPE::UINT | COMPUTE_IMAGE_TYPE::FLAG_NORMALIZED)
				FLOOR_RT_WRITE_IMAGE_CASE(COMPUTE_IMAGE_TYPE::FORMAT_4 | COMPUTE_IMAGE_TYPE::CHANNELS_2 | COMPUTE_IMAGE_TYPE::UINT | COMPUTE_IMAGE_TYPE::FLAG_NORMALIZED)
				FLOOR_RT_WRITE_IMAGE_CASE(COMPUTE_IMAGE_TYPE::FORMAT_4 | COMPUTE_IMAGE_TYPE::CHANNELS_3 | COMPUTE_IMAGE_TYPE::UINT | COMPUTE_IMAGE_TYPE::FLAG_NORMALIZED)
				FLOOR_RT_WRITE_IMAGE_CASE(COMPUTE_IMAGE_TYPE::FORMAT_4 | COMPUTE_IMAGE_TYPE::CHANNELS_4 | COMPUTE_IMAGE_TYPE::UINT | COMPUTE_IMAGE_TYPE::FLAG_NORMALIZED)
				
				FLOOR_RT_WRITE_IMAGE_CASE(COMPUTE_IMAGE_TYPE::FORMAT_8 | COMPUTE_IMAGE_TYPE::CHANNELS_1 | COMPUTE_IMAGE_TYPE::INT | COMPUTE_IMAGE_TYPE::FLAG_NORMALIZED)
				FLOOR_RT_WRITE_IMAGE_CASE(COMPUTE_IMAGE_TYPE::FORMAT_8 | COMPUTE_IMAGE_TYPE::CHANNELS_2 | COMPUTE_IMAGE_TYPE::INT | COMPUTE_IMAGE_TYPE::FLAG_NORMALIZED)
				FLOOR_RT_WRITE_IMAGE_CASE(COMPUTE_IMAGE_TYPE::FORMAT_8 | COMPUTE_IMAGE_TYPE::CHANNELS_3 | COMPUTE_IMAGE_TYPE::INT | COMPUTE_IMAGE_TYPE::FLAG_NORMALIZED)
				FLOOR_RT_WRITE_IMAGE_CASE(COMPUTE_IMAGE_TYPE::FORMAT_8 | COMPUTE_IMAGE_TYPE::CHANNELS_4 | COMPUTE_IMAGE_TYPE::INT | COMPUTE_IMAGE_TYPE::FLAG_NORMALIZED)
				FLOOR_RT_WRITE_IMAGE_CASE(COMPUTE_IMAGE_TYPE::FORMAT_8 | COMPUTE_IMAGE_TYPE::CHANNELS_1 | COMPUTE_IMAGE_TYPE::UINT | COMPUTE_IMAGE_TYPE::FLAG_NORMALIZED)
				FLOOR_RT_WRITE_IMAGE_CASE(COMPUTE_IMAGE_TYPE::FORMAT_8 | COMPUTE_IMAGE_TYPE::CHANNELS_2 | COMPUTE_IMAGE_TYPE::UINT | COMPUTE_IMAGE_TYPE::FLAG_NORMALIZED)
				FLOOR_RT_WRITE_IMAGE_CASE(COMPUTE_IMAGE_TYPE::FORMAT_8 | COMPUTE_IMAGE_TYPE::CHANNELS_3 | COMPUTE_IMAGE_TYPE::UINT | COMPUTE_IMAGE_TYPE::FLAG_NORMALIZED)
				FLOOR_RT_WRITE_IMAGE_CASE(COMPUTE_IMAGE_TYPE::FORMAT_8 | COMPUTE_IMAGE_TYPE::CHANNELS_4 | COMPUTE_IMAGE_TYPE::UINT | COMPUTE_IMAGE_TYPE::FLAG_NORMALIZED)
				
				FLOOR_RT_WRITE_IMAGE_CASE(COMPUTE_IMAGE_TYPE::FORMAT_16 | COMPUTE_IMAGE_TYPE::CHANNELS_1 | COMPUTE_IMAGE_TYPE::INT | COMPUTE_IMAGE_TYPE::FLAG_NORMALIZED)
				FLOOR_RT_WRITE_IMAGE_CASE(COMPUTE_IMAGE_TYPE::FORMAT_16 | COMPUTE_IMAGE_TYPE::CHANNELS_2 | COMPUTE_IMAGE_TYPE::INT | COMPUTE_IMAGE_TYPE::FLAG_NORMALIZED)
				FLOOR_RT_WRITE_IMAGE_CASE(COMPUTE_IMAGE_TYPE::FORMAT_16 | COMPUTE_IMAGE_TYPE::CHANNELS_3 | COMPUTE_IMAGE_TYPE::INT | COMPUTE_IMAGE_TYPE::FLAG_NORMALIZED)
				FLOOR_RT_WRITE_IMAGE_CASE(COMPUTE_IMAGE_TYPE::FORMAT_16 | COMPUTE_IMAGE_TYPE::CHANNELS_4 | COMPUTE_IMAGE_TYPE::INT | COMPUTE_IMAGE_TYPE::FLAG_NORMALIZED)
				FLOOR_RT_WRITE_IMAGE_CASE(COMPUTE_IMAGE_TYPE::FORMAT_16 | COMPUTE_IMAGE_TYPE::CHANNELS_1 | COMPUTE_IMAGE_TYPE::UINT | COMPUTE_IMAGE_TYPE::FLAG_NORMALIZED)
				FLOOR_RT_WRITE_IMAGE_CASE(COMPUTE_IMAGE_TYPE::FORMAT_16 | COMPUTE_IMAGE_TYPE::CHANNELS_2 | COMPUTE_IMAGE_TYPE::UINT | COMPUTE_IMAGE_TYPE::FLAG_NORMALIZED)
				FLOOR_RT_WRITE_IMAGE_CASE(COMPUTE_IMAGE_TYPE::FORMAT_16 | COMPUTE_IMAGE_TYPE::CHANNELS_3 | COMPUTE_IMAGE_TYPE::UINT | COMPUTE_IMAGE_TYPE::FLAG_NORMALIZED)
				FLOOR_RT_WRITE_IMAGE_CASE(COMPUTE_IMAGE_TYPE::FORMAT_16 | COMPUTE_IMAGE_TYPE::CHANNELS_4 | COMPUTE_IMAGE_TYPE::UINT | COMPUTE_IMAGE_TYPE::FLAG_NORMALIZED)
				
				FLOOR_RT_WRITE_IMAGE_CASE(COMPUTE_IMAGE_TYPE::FORMAT_32 | COMPUTE_IMAGE_TYPE::CHANNELS_1 | COMPUTE_IMAGE_TYPE::INT | COMPUTE_IMAGE_TYPE::FLAG_NORMALIZED)
				FLOOR_RT_WRITE_IMAGE_CASE(COMPUTE_IMAGE_TYPE::FORMAT_32 | COMPUTE_IMAGE_TYPE::CHANNELS_2 | COMPUTE_IMAGE_TYPE::INT | COMPUTE_IMAGE_TYPE::FLAG_NORMALIZED)
				FLOOR_RT_WRITE_IMAGE_CASE(COMPUTE_IMAGE_TYPE::FORMAT_32 | COMPUTE_IMAGE_TYPE::CHANNELS_3 | COMPUTE_IMAGE_TYPE::INT | COMPUTE_IMAGE_TYPE::FLAG_NORMALIZED)
				FLOOR_RT_WRITE_IMAGE_CASE(COMPUTE_IMAGE_TYPE::FORMAT_32 | COMPUTE_IMAGE_TYPE::CHANNELS_4 | COMPUTE_IMAGE_TYPE::INT | COMPUTE_IMAGE_TYPE::FLAG_NORMALIZED)
				FLOOR_RT_WRITE_IMAGE_CASE(COMPUTE_IMAGE_TYPE::FORMAT_32 | COMPUTE_IMAGE_TYPE::CHANNELS_1 | COMPUTE_IMAGE_TYPE::UINT | COMPUTE_IMAGE_TYPE::FLAG_NORMALIZED)
				FLOOR_RT_WRITE_IMAGE_CASE(COMPUTE_IMAGE_TYPE::FORMAT_32 | COMPUTE_IMAGE_TYPE::CHANNELS_2 | COMPUTE_IMAGE_TYPE::UINT | COMPUTE_IMAGE_TYPE::FLAG_NORMALIZED)
				FLOOR_RT_WRITE_IMAGE_CASE(COMPUTE_IMAGE_TYPE::FORMAT_32 | COMPUTE_IMAGE_TYPE::CHANNELS_3 | COMPUTE_IMAGE_TYPE::UINT | COMPUTE_IMAGE_TYPE::FLAG_NORMALIZED)
				FLOOR_RT_WRITE_IMAGE_CASE(COMPUTE_IMAGE_TYPE::FORMAT_32 | COMPUTE_IMAGE_TYPE::CHANNELS_4 | COMPUTE_IMAGE_TYPE::UINT | COMPUTE_IMAGE_TYPE::FLAG_NORMALIZED)
				
				FLOOR_RT_WRITE_IMAGE_CASE(COMPUTE_IMAGE_TYPE::FORMAT_64 | COMPUTE_IMAGE_TYPE::CHANNELS_1 | COMPUTE_IMAGE_TYPE::INT | COMPUTE_IMAGE_TYPE::FLAG_NORMALIZED)
				FLOOR_RT_WRITE_IMAGE_CASE(COMPUTE_IMAGE_TYPE::FORMAT_64 | COMPUTE_IMAGE_TYPE::CHANNELS_2 | COMPUTE_IMAGE_TYPE::INT | COMPUTE_IMAGE_TYPE::FLAG_NORMALIZED)
				FLOOR_RT_WRITE_IMAGE_CASE(COMPUTE_IMAGE_TYPE::FORMAT_64 | COMPUTE_IMAGE_TYPE::CHANNELS_3 | COMPUTE_IMAGE_TYPE::INT | COMPUTE_IMAGE_TYPE::FLAG_NORMALIZED)
				FLOOR_RT_WRITE_IMAGE_CASE(COMPUTE_IMAGE_TYPE::FORMAT_64 | COMPUTE_IMAGE_TYPE::CHANNELS_4 | COMPUTE_IMAGE_TYPE::INT | COMPUTE_IMAGE_TYPE::FLAG_NORMALIZED)
				FLOOR_RT_WRITE_IMAGE_CASE(COMPUTE_IMAGE_TYPE::FORMAT_64 | COMPUTE_IMAGE_TYPE::CHANNELS_1 | COMPUTE_IMAGE_TYPE::UINT | COMPUTE_IMAGE_TYPE::FLAG_NORMALIZED)
				FLOOR_RT_WRITE_IMAGE_CASE(COMPUTE_IMAGE_TYPE::FORMAT_64 | COMPUTE_IMAGE_TYPE::CHANNELS_2 | COMPUTE_IMAGE_TYPE::UINT | COMPUTE_IMAGE_TYPE::FLAG_NORMALIZED)
				FLOOR_RT_WRITE_IMAGE_CASE(COMPUTE_IMAGE_TYPE::FORMAT_64 | COMPUTE_IMAGE_TYPE::CHANNELS_3 | COMPUTE_IMAGE_TYPE::UINT | COMPUTE_IMAGE_TYPE::FLAG_NORMALIZED)
				FLOOR_RT_WRITE_IMAGE_CASE(COMPUTE_IMAGE_TYPE::FORMAT_64 | COMPUTE_IMAGE_TYPE::CHANNELS_4 | COMPUTE_IMAGE_TYPE::UINT | COMPUTE_IMAGE_TYPE::FLAG_NORMALIZED)
				
				default: floor_unreachable();
			}
		}
		// non-normalized float
		else {
			switch(runtime_base_type) {
				FLOOR_RT_WRITE_IMAGE_CASE(COMPUTE_IMAGE_TYPE::FORMAT_16 | COMPUTE_IMAGE_TYPE::CHANNELS_1 | COMPUTE_IMAGE_TYPE::FLOAT)
				FLOOR_RT_WRITE_IMAGE_CASE(COMPUTE_IMAGE_TYPE::FORMAT_16 | COMPUTE_IMAGE_TYPE::CHANNELS_2 | COMPUTE_IMAGE_TYPE::FLOAT)
				FLOOR_RT_WRITE_IMAGE_CASE(COMPUTE_IMAGE_TYPE::FORMAT_16 | COMPUTE_IMAGE_TYPE::CHANNELS_3 | COMPUTE_IMAGE_TYPE::FLOAT)
				FLOOR_RT_WRITE_IMAGE_CASE(COMPUTE_IMAGE_TYPE::FORMAT_16 | COMPUTE_IMAGE_TYPE::CHANNELS_4 | COMPUTE_IMAGE_TYPE::FLOAT)
				
				FLOOR_RT_WRITE_IMAGE_CASE(COMPUTE_IMAGE_TYPE::FORMAT_32 | COMPUTE_IMAGE_TYPE::CHANNELS_1 | COMPUTE_IMAGE_TYPE::FLOAT)
				FLOOR_RT_WRITE_IMAGE_CASE(COMPUTE_IMAGE_TYPE::FORMAT_32 | COMPUTE_IMAGE_TYPE::CHANNELS_2 | COMPUTE_IMAGE_TYPE::FLOAT)
				FLOOR_RT_WRITE_IMAGE_CASE(COMPUTE_IMAGE_TYPE::FORMAT_32 | COMPUTE_IMAGE_TYPE::CHANNELS_3 | COMPUTE_IMAGE_TYPE::FLOAT)
				FLOOR_RT_WRITE_IMAGE_CASE(COMPUTE_IMAGE_TYPE::FORMAT_32 | COMPUTE_IMAGE_TYPE::CHANNELS_4 | COMPUTE_IMAGE_TYPE::FLOAT)
				
				default: floor_unreachable();
			}
		}
	}
	
#if 0 // TODO/NOTE: not supported yet
	// double data
	template <COMPUTE_IMAGE_TYPE type = sample_image_type, typename... Args,
			  enable_if_t<(// dobule
						   (type & COMPUTE_IMAGE_TYPE::__FORMAT_MASK) == COMPUTE_IMAGE_TYPE::FORMAT_64 &&
						   (type & COMPUTE_IMAGE_TYPE::__DATA_TYPE_MASK) == COMPUTE_IMAGE_TYPE::FLOAT &&
						   // !normalized
						   !has_flag<COMPUTE_IMAGE_TYPE::FLAG_NORMALIZED>(type) &&
						   // !depth
						   !has_flag<COMPUTE_IMAGE_TYPE::FLAG_DEPTH>(type))>* = nullptr>
	static void write(const host_device_image<sample_image_type, is_lod, is_lod_float, is_bias>* img, Args&&... args) {
		const auto runtime_base_type = img->runtime_image_type & (COMPUTE_IMAGE_TYPE::__FORMAT_MASK |
																  COMPUTE_IMAGE_TYPE::__CHANNELS_MASK |
																  COMPUTE_IMAGE_TYPE::__DATA_TYPE_MASK);
		switch(runtime_base_type) {
			FLOOR_RT_WRITE_IMAGE_CASE(COMPUTE_IMAGE_TYPE::FORMAT_64 | COMPUTE_IMAGE_TYPE::CHANNELS_1 | COMPUTE_IMAGE_TYPE::FLOAT)
			FLOOR_RT_WRITE_IMAGE_CASE(COMPUTE_IMAGE_TYPE::FORMAT_64 | COMPUTE_IMAGE_TYPE::CHANNELS_2 | COMPUTE_IMAGE_TYPE::FLOAT)
			FLOOR_RT_WRITE_IMAGE_CASE(COMPUTE_IMAGE_TYPE::FORMAT_64 | COMPUTE_IMAGE_TYPE::CHANNELS_3 | COMPUTE_IMAGE_TYPE::FLOAT)
			FLOOR_RT_WRITE_IMAGE_CASE(COMPUTE_IMAGE_TYPE::FORMAT_64 | COMPUTE_IMAGE_TYPE::CHANNELS_4 | COMPUTE_IMAGE_TYPE::FLOAT)
			
			default: floor_unreachable();
		}
	}
#endif
	
	// depth float
	template <COMPUTE_IMAGE_TYPE type = sample_image_type, typename... Args,
			  enable_if_t<(has_flag<COMPUTE_IMAGE_TYPE::FLAG_DEPTH>(type) &&
						   !has_flag<COMPUTE_IMAGE_TYPE::FLAG_STENCIL>(type))>* = nullptr>
	static void write(const host_device_image<sample_image_type, is_lod, is_lod_float, is_bias>* img, Args&&... args) {
		const auto runtime_base_type = img->runtime_image_type & (COMPUTE_IMAGE_TYPE::__FORMAT_MASK |
																  COMPUTE_IMAGE_TYPE::__CHANNELS_MASK |
																  COMPUTE_IMAGE_TYPE::__DATA_TYPE_MASK |
																  COMPUTE_IMAGE_TYPE::FLAG_DEPTH |
																  COMPUTE_IMAGE_TYPE::FLAG_STENCIL);
		switch(runtime_base_type) {
			FLOOR_RT_WRITE_IMAGE_CASE(COMPUTE_IMAGE_TYPE::FORMAT_16 | COMPUTE_IMAGE_TYPE::CHANNELS_1 | COMPUTE_IMAGE_TYPE::UINT | COMPUTE_IMAGE_TYPE::FLAG_DEPTH)
			FLOOR_RT_WRITE_IMAGE_CASE(COMPUTE_IMAGE_TYPE::FORMAT_24 | COMPUTE_IMAGE_TYPE::CHANNELS_1 | COMPUTE_IMAGE_TYPE::UINT | COMPUTE_IMAGE_TYPE::FLAG_DEPTH)
			FLOOR_RT_WRITE_IMAGE_CASE(COMPUTE_IMAGE_TYPE::FORMAT_32 | COMPUTE_IMAGE_TYPE::CHANNELS_1 | COMPUTE_IMAGE_TYPE::UINT | COMPUTE_IMAGE_TYPE::FLAG_DEPTH)
			FLOOR_RT_WRITE_IMAGE_CASE(COMPUTE_IMAGE_TYPE::FORMAT_32 | COMPUTE_IMAGE_TYPE::CHANNELS_1 | COMPUTE_IMAGE_TYPE::FLOAT | COMPUTE_IMAGE_TYPE::FLAG_DEPTH)
			default: floor_unreachable();
		}
	}
	
	// depth+stencil float+uint8_t
	template <COMPUTE_IMAGE_TYPE type = sample_image_type, typename... Args,
			  enable_if_t<(has_flag<COMPUTE_IMAGE_TYPE::FLAG_DEPTH>(type) &&
						   has_flag<COMPUTE_IMAGE_TYPE::FLAG_STENCIL>(type))>* = nullptr>
	static void write(const host_device_image<sample_image_type, is_lod, is_lod_float, is_bias>* img, Args&&... args) {
		const auto runtime_base_type = img->runtime_image_type & (COMPUTE_IMAGE_TYPE::__FORMAT_MASK |
																  COMPUTE_IMAGE_TYPE::__CHANNELS_MASK |
																  COMPUTE_IMAGE_TYPE::__DATA_TYPE_MASK |
																  COMPUTE_IMAGE_TYPE::FLAG_DEPTH |
																  COMPUTE_IMAGE_TYPE::FLAG_STENCIL);
		switch(runtime_base_type) {
			FLOOR_RT_WRITE_IMAGE_CASE(COMPUTE_IMAGE_TYPE::FORMAT_24_8 | COMPUTE_IMAGE_TYPE::CHANNELS_2 | COMPUTE_IMAGE_TYPE::UINT | COMPUTE_IMAGE_TYPE::FLAG_DEPTH | COMPUTE_IMAGE_TYPE::FLAG_STENCIL)
			FLOOR_RT_WRITE_IMAGE_CASE(COMPUTE_IMAGE_TYPE::FORMAT_32_8 | COMPUTE_IMAGE_TYPE::CHANNELS_2 | COMPUTE_IMAGE_TYPE::FLOAT | COMPUTE_IMAGE_TYPE::FLAG_DEPTH | COMPUTE_IMAGE_TYPE::FLAG_STENCIL)
			default: floor_unreachable();
		}
	}
	
	// non-normalized int/uint data (<= 32-bit)
	template <COMPUTE_IMAGE_TYPE type = sample_image_type, typename... Args,
			  COMPUTE_IMAGE_TYPE fixed_data_type = (type & COMPUTE_IMAGE_TYPE::__DATA_TYPE_MASK),
			  enable_if_t<(!has_flag<COMPUTE_IMAGE_TYPE::FLAG_NORMALIZED>(type) &&
						   (fixed_data_type == COMPUTE_IMAGE_TYPE::INT ||
							fixed_data_type == COMPUTE_IMAGE_TYPE::UINT) /*&&
						   (type & COMPUTE_IMAGE_TYPE::__FORMAT_MASK) != COMPUTE_IMAGE_TYPE::FORMAT_64*/)>* = nullptr>
	static void write(const host_device_image<sample_image_type, is_lod, is_lod_float, is_bias>* img, Args&&... args) {
		const auto runtime_base_type = img->runtime_image_type & (COMPUTE_IMAGE_TYPE::__FORMAT_MASK |
																  COMPUTE_IMAGE_TYPE::__CHANNELS_MASK |
																  COMPUTE_IMAGE_TYPE::__DATA_TYPE_MASK);
		switch(runtime_base_type) {
			FLOOR_RT_WRITE_IMAGE_CASE(COMPUTE_IMAGE_TYPE::FORMAT_2 | COMPUTE_IMAGE_TYPE::CHANNELS_1 | fixed_data_type)
			FLOOR_RT_WRITE_IMAGE_CASE(COMPUTE_IMAGE_TYPE::FORMAT_2 | COMPUTE_IMAGE_TYPE::CHANNELS_2 | fixed_data_type)
			FLOOR_RT_WRITE_IMAGE_CASE(COMPUTE_IMAGE_TYPE::FORMAT_2 | COMPUTE_IMAGE_TYPE::CHANNELS_3 | fixed_data_type)
			FLOOR_RT_WRITE_IMAGE_CASE(COMPUTE_IMAGE_TYPE::FORMAT_2 | COMPUTE_IMAGE_TYPE::CHANNELS_4 | fixed_data_type)
			
			FLOOR_RT_WRITE_IMAGE_CASE(COMPUTE_IMAGE_TYPE::FORMAT_4 | COMPUTE_IMAGE_TYPE::CHANNELS_1 | fixed_data_type)
			FLOOR_RT_WRITE_IMAGE_CASE(COMPUTE_IMAGE_TYPE::FORMAT_4 | COMPUTE_IMAGE_TYPE::CHANNELS_2 | fixed_data_type)
			FLOOR_RT_WRITE_IMAGE_CASE(COMPUTE_IMAGE_TYPE::FORMAT_4 | COMPUTE_IMAGE_TYPE::CHANNELS_3 | fixed_data_type)
			FLOOR_RT_WRITE_IMAGE_CASE(COMPUTE_IMAGE_TYPE::FORMAT_4 | COMPUTE_IMAGE_TYPE::CHANNELS_4 | fixed_data_type)
			
			FLOOR_RT_WRITE_IMAGE_CASE(COMPUTE_IMAGE_TYPE::FORMAT_8 | COMPUTE_IMAGE_TYPE::CHANNELS_1 | fixed_data_type)
			FLOOR_RT_WRITE_IMAGE_CASE(COMPUTE_IMAGE_TYPE::FORMAT_8 | COMPUTE_IMAGE_TYPE::CHANNELS_2 | fixed_data_type)
			FLOOR_RT_WRITE_IMAGE_CASE(COMPUTE_IMAGE_TYPE::FORMAT_8 | COMPUTE_IMAGE_TYPE::CHANNELS_3 | fixed_data_type)
			FLOOR_RT_WRITE_IMAGE_CASE(COMPUTE_IMAGE_TYPE::FORMAT_8 | COMPUTE_IMAGE_TYPE::CHANNELS_4 | fixed_data_type)
			
			FLOOR_RT_WRITE_IMAGE_CASE(COMPUTE_IMAGE_TYPE::FORMAT_16 | COMPUTE_IMAGE_TYPE::CHANNELS_1 | fixed_data_type)
			FLOOR_RT_WRITE_IMAGE_CASE(COMPUTE_IMAGE_TYPE::FORMAT_16 | COMPUTE_IMAGE_TYPE::CHANNELS_2 | fixed_data_type)
			FLOOR_RT_WRITE_IMAGE_CASE(COMPUTE_IMAGE_TYPE::FORMAT_16 | COMPUTE_IMAGE_TYPE::CHANNELS_3 | fixed_data_type)
			FLOOR_RT_WRITE_IMAGE_CASE(COMPUTE_IMAGE_TYPE::FORMAT_16 | COMPUTE_IMAGE_TYPE::CHANNELS_4 | fixed_data_type)
			
			FLOOR_RT_WRITE_IMAGE_CASE(COMPUTE_IMAGE_TYPE::FORMAT_32 | COMPUTE_IMAGE_TYPE::CHANNELS_1 | fixed_data_type)
			FLOOR_RT_WRITE_IMAGE_CASE(COMPUTE_IMAGE_TYPE::FORMAT_32 | COMPUTE_IMAGE_TYPE::CHANNELS_2 | fixed_data_type)
			FLOOR_RT_WRITE_IMAGE_CASE(COMPUTE_IMAGE_TYPE::FORMAT_32 | COMPUTE_IMAGE_TYPE::CHANNELS_3 | fixed_data_type)
			FLOOR_RT_WRITE_IMAGE_CASE(COMPUTE_IMAGE_TYPE::FORMAT_32 | COMPUTE_IMAGE_TYPE::CHANNELS_4 | fixed_data_type)
			
			default: floor_unreachable();
		}
	}
	
#if 0 // TODO/NOTE: not supported yet
	// non-normalized int/uint data (== 64-bit)
	template <COMPUTE_IMAGE_TYPE type = sample_image_type, typename... Args,
			  COMPUTE_IMAGE_TYPE fixed_data_type = (type & COMPUTE_IMAGE_TYPE::__DATA_TYPE_MASK),
			  enable_if_t<(!has_flag<COMPUTE_IMAGE_TYPE::FLAG_NORMALIZED>(type) &&
						   (fixed_data_type == COMPUTE_IMAGE_TYPE::INT ||
							fixed_data_type == COMPUTE_IMAGE_TYPE::UINT) &&
						   (type & COMPUTE_IMAGE_TYPE::__FORMAT_MASK) == COMPUTE_IMAGE_TYPE::FORMAT_64)>* = nullptr>
	static void write(const host_device_image<sample_image_type, is_lod, is_lod_float, is_bias>* img, Args&&... args) {
		const auto runtime_base_type = img->runtime_image_type & (COMPUTE_IMAGE_TYPE::__FORMAT_MASK |
																  COMPUTE_IMAGE_TYPE::__CHANNELS_MASK |
																  COMPUTE_IMAGE_TYPE::__DATA_TYPE_MASK);
		switch(runtime_base_type) {
			FLOOR_RT_WRITE_IMAGE_CASE(COMPUTE_IMAGE_TYPE::FORMAT_64 | COMPUTE_IMAGE_TYPE::CHANNELS_1 | fixed_data_type)
			FLOOR_RT_WRITE_IMAGE_CASE(COMPUTE_IMAGE_TYPE::FORMAT_64 | COMPUTE_IMAGE_TYPE::CHANNELS_2 | fixed_data_type)
			FLOOR_RT_WRITE_IMAGE_CASE(COMPUTE_IMAGE_TYPE::FORMAT_64 | COMPUTE_IMAGE_TYPE::CHANNELS_3 | fixed_data_type)
			FLOOR_RT_WRITE_IMAGE_CASE(COMPUTE_IMAGE_TYPE::FORMAT_64 | COMPUTE_IMAGE_TYPE::CHANNELS_4 | fixed_data_type)
			
			default: floor_unreachable();
		}
	}
#endif

FLOOR_POP_WARNINGS()
#undef FLOOR_RT_READ_IMAGE_CASE
#undef FLOOR_RT_WRITE_IMAGE_CASE
	
};

FLOOR_POP_WARNINGS()

#endif

#endif
