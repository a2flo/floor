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

#include <floor/compute/device/host_limits.hpp>
#include <floor/constexpr/soft_f16.hpp>

// ignore vectorization/optimization/etc. hints and infos
FLOOR_PUSH_WARNINGS()
#if defined(__clang__)
#pragma clang diagnostic ignored "-Rpass-analysis"
#pragma clang diagnostic ignored "-Rpass"
#endif

template <COMPUTE_IMAGE_TYPE, bool is_lod, bool is_lod_float, bool is_bias> struct host_device_image;

namespace host_image_impl {
	struct image_level_info {
		const uint4 dim;
		const int4 clamp_dim_int;
		const float4 clamp_dim_float;
		const uint32_t offset;
		const uint32_t _unused[3];
	};
	static_assert(sizeof(image_level_info) == 64, "invalid level info size");
	
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
		//! int/uint coordinates, non cube map
		template <typename coord_type, COMPUTE_IMAGE_TYPE type = fixed_image_type,
				  enable_if_t<(!is_floating_point<typename coord_type::decayed_scalar_type>() &&
							   !has_flag<COMPUTE_IMAGE_TYPE::FLAG_CUBE>(type))>* = nullptr>
		floor_inline_always static auto process_coord(const image_level_info& level_info,
													  const coord_type& coord,
													  const vector_n<int32_t, coord_type::dim()> offset = {}) {
			// clamp to [0, image_dim - 1]
			return vector_n<uint32_t, coord_type::dim()> {
				(coord + offset).clamped(image_dim_to_coord_dim<coord_type::dim()>(level_info.clamp_dim_int))
			};
		}
		
		//! int/uint coordinates, cube map
		template <typename coord_type, COMPUTE_IMAGE_TYPE type = fixed_image_type,
				  enable_if_t<(!is_floating_point<typename coord_type::decayed_scalar_type>() &&
							   has_flag<COMPUTE_IMAGE_TYPE::FLAG_CUBE>(type))>* = nullptr>
		floor_inline_always static auto process_coord(const image_level_info& level_info,
													  const coord_type& coord,
													  const int2 offset = {}) {
			// clamp to [0, image_dim - 1]
			return vector_n<uint32_t, coord_type::dim()> {
				int3(int(coord.x) + offset.x, int(coord.y) + offset.y, int(coord.z)).clamped(image_dim_to_coord_dim<coord_type::dim()>(level_info.clamp_dim_int))
			};
		}
		
		//! float coordinates, non cube map
		template <typename coord_type, COMPUTE_IMAGE_TYPE type = fixed_image_type,
				  enable_if_t<(is_floating_point<typename coord_type::decayed_scalar_type>::value &&
							   !has_flag<COMPUTE_IMAGE_TYPE::FLAG_CUBE>(type))>* = nullptr>
		floor_inline_always static auto process_coord(const image_level_info& level_info,
													  const coord_type& coord,
													  const vector_n<int32_t, coord_type::dim()> offset = {}) {
			// with normalized coords:
			// * scale and add [0, 1] to [0, image dim]
			// * apply offset (now in [-min offset, image dim + max offset])
			// * clamp to [0, image dim - eps), taking care of out-of-bounds access and the case that
			//   floor(image_dim) != image_dim - 1, but floor(nexttoward(image_dim), 0.0) == image_dim -1;
			//   eps is 0.5f here just to be sure (__FLT_MIN__ might still cause issues)
			// * convert to uint (equivalent to floor(x))
			const auto fdim = image_dim_to_coord_dim<coord_type::dim()>(level_info.clamp_dim_float);
			return vector_n<uint32_t, coord_type::dim()> { (coord * fdim + coord_type(offset)).clamp(fdim - 0.5f) };
		}
		
		//! float coordinates, cube map
		template <typename coord_type, COMPUTE_IMAGE_TYPE type = fixed_image_type,
				  enable_if_t<(is_floating_point<typename coord_type::decayed_scalar_type>() &&
							   has_flag<COMPUTE_IMAGE_TYPE::FLAG_CUBE>(type))>* = nullptr>
		floor_inline_always static auto process_coord(const image_level_info& level_info,
													  const coord_type& coord,
													  const int2 offset = {}) {
			// normalized coords, see above for explanation
			const auto coord_layer = compute_cube_coord_and_layer(coord);
			const auto fdim = image_dim_to_coord_dim<coord_type::dim() - 1u>(level_info.clamp_dim_float);
			return vector_n<uint32_t, coord_type::dim()> {
				(coord_layer.first * fdim + float2(offset)).clamp(fdim - 0.5f),
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
		floor_inline_always static size_t coord_to_offset(const image_level_info& level_info, const uint1 coord) {
			return level_info.offset + size_t(coord.x) * image_bytes_per_pixel(fixed_image_type);
		}
		
		//! 1D array
		floor_inline_always static size_t coord_to_offset(const image_level_info& level_info, const uint1 coord, const uint32_t layer) {
			return coord_to_offset(level_info, coord) + image_slice_data_size_from_types(level_info.dim, fixed_image_type) * layer;
		}
		
		//! 2D, 2D depth, 2D depth+stencil
		floor_inline_always static size_t coord_to_offset(const image_level_info& level_info, const uint2 coord) {
			return level_info.offset + size_t(level_info.dim.x * coord.y + coord.x) * image_bytes_per_pixel(fixed_image_type);
		}
		
		//! 2D array, 2D depth array
		floor_inline_always static size_t coord_to_offset(const image_level_info& level_info, const uint2 coord, const uint32_t layer) {
			return coord_to_offset(level_info, coord) + image_slice_data_size_from_types(level_info.dim, fixed_image_type) * layer;
		}
		
		//! 3D
		template <COMPUTE_IMAGE_TYPE type = fixed_image_type, enable_if_t<!has_flag<COMPUTE_IMAGE_TYPE::FLAG_CUBE>(type)>* = nullptr>
		floor_inline_always static size_t coord_to_offset(const image_level_info& level_info, const uint3 coord) {
			return level_info.offset + size_t(level_info.dim.x * level_info.dim.y * coord.z +
											  level_info.dim.x * coord.y +
											  coord.x) * image_bytes_per_pixel(type);
		}
		
		//! cube, depth cube
		template <COMPUTE_IMAGE_TYPE type = fixed_image_type, enable_if_t<has_flag<COMPUTE_IMAGE_TYPE::FLAG_CUBE>(type)>* = nullptr>
		floor_inline_always static size_t coord_to_offset(const image_level_info& level_info, const uint3 coord) {
			return coord_to_offset(level_info, coord.xy, coord.z);
		}
		
		//! cube array, depth cube array
		floor_inline_always static size_t coord_to_offset(const image_level_info& level_info, const uint3 coord, const uint32_t layer) {
			return coord_to_offset(level_info, coord.xy, layer * 6u + coord.z);
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
					uchannel_type(1) << (max(bpc[0], uint64_t(1)) - 1u),
					uchannel_type(1) << (max(bpc[1], uint64_t(1)) - 1u),
					uchannel_type(1) << (max(bpc[2], uint64_t(1)) - 1u),
					uchannel_type(1) << (max(bpc[3], uint64_t(1)) - 1u)
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
																						<< uchannel_type(max(bpc[i], uint64_t(1))
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
		
		// determines which lod/bias value to use and clamps it to [0, max #mip-levels - 1]
		template <bool is_lod_ = is_lod, bool is_bias_ = is_bias, enable_if_t<is_lod_ || is_bias_>* = nullptr>
		floor_inline_always static constexpr uint32_t select_lod(const int32_t lod_i,
																 const float lod_or_bias_f) {
			return min(host_limits::max_mip_levels - 1u, (is_lod_float || is_bias ?
														  uint32_t(max(0.0f, roundf(lod_or_bias_f))) :
														  uint32_t(max(0, lod_i))));
		}
		// clamps lod to [0, max #mip-levels - 1]
		template <bool is_lod_ = is_lod, enable_if_t<is_lod_>* = nullptr>
		floor_inline_always static constexpr uint32_t select_lod(const uint32_t lod) {
			return min(host_limits::max_mip_levels - 1u, lod);
		}
		// no lod/bias -> always return 0
		template <bool is_lod_ = is_lod, bool is_bias_ = is_bias, enable_if_t<!is_lod_ && !is_bias_>* = nullptr>
		floor_inline_always static constexpr uint32_t select_lod(const int32_t, const float) { return 0; }
		// no lod/bias -> always return 0
		template <bool is_lod_ = is_lod, enable_if_t<!is_lod_>* = nullptr>
		floor_inline_always static constexpr uint32_t select_lod(const uint32_t) { return 0; }


FLOOR_PUSH_WARNINGS()
FLOOR_IGNORE_WARNING(cast-align) // kill "cast needs 4 byte alignment" warning in here (it is 4 byte aligned)

		// image read functions
		template <typename coord_type, typename offset_type, COMPUTE_IMAGE_TYPE type = fixed_image_type,
				  enable_if_t<((has_flag<COMPUTE_IMAGE_TYPE::FLAG_NORMALIZED>(type) ||
								(type & COMPUTE_IMAGE_TYPE::__DATA_TYPE_MASK) == COMPUTE_IMAGE_TYPE::FLOAT) &&
							   !has_flag<COMPUTE_IMAGE_TYPE::FLAG_DEPTH>(type))>* = nullptr>
		static auto read(const host_device_image<type, is_lod, is_lod_float, is_bias>* img,
						 const coord_type& coord,
						 const offset_type& coord_offset,
						 const uint32_t layer,
						 const int32_t lod_i,
						 const float lod_or_bias_f) {
			// read/copy raw data
			constexpr const bool is_array = has_flag<COMPUTE_IMAGE_TYPE::FLAG_ARRAY>(type);
			constexpr const size_t bpp = image_bytes_per_pixel(type);
			const auto lod = select_lod(lod_i, lod_or_bias_f);
#if defined(FLOOR_CXX17)
			size_t offset;
			if constexpr(!is_array) offset = coord_to_offset(img->level_info[lod], process_coord(img->level_info[lod], coord, coord_offset));
			else offset = coord_to_offset(img->level_info[lod], process_coord(img->level_info[lod], coord, coord_offset), layer);
#else
			const auto offset = __builtin_choose_expr(!is_array,
													  coord_to_offset(img->level_info[lod], process_coord(img->level_info[lod], coord, coord_offset)),
													  coord_to_offset(img->level_info[lod], process_coord(img->level_info[lod], coord, coord_offset), layer));
#endif
			typedef uint8_t raw_data_type[bpp];
			const raw_data_type& raw_data = *(const raw_data_type*)&img->data[offset];
			
			// extract channel bits/bytes
			constexpr const auto data_type = (type & COMPUTE_IMAGE_TYPE::__DATA_TYPE_MASK);
			constexpr const auto image_format = (type & COMPUTE_IMAGE_TYPE::__FORMAT_MASK);
			constexpr const auto channel_count = image_channel_count(type);
			vector4<conditional_t<(image_bits_of_channel(type, 0) <= 32 ||
								   data_type != COMPUTE_IMAGE_TYPE::FLOAT), float, double>> ret;
			if(data_type == COMPUTE_IMAGE_TYPE::FLOAT) {
				if(image_format == COMPUTE_IMAGE_TYPE::FORMAT_32 ||
				   image_format == COMPUTE_IMAGE_TYPE::FORMAT_64) {
					// for 32-bit/64-bit float formats, just pass-through raw data
					ret = *(decltype(ret)*)raw_data;
				}
				else if(image_format == COMPUTE_IMAGE_TYPE::FORMAT_16) {
					// 16-bit half float data must be converted to 32-bit float data
#pragma unroll
					for(uint32_t i = 0; i < channel_count; ++i) {
						ret[i] = (float)*(soft_f16*)(raw_data + i * 2);
					}
				}
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
						 const offset_type& coord_offset,
						 const uint32_t layer,
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
			typedef conditional_t<!has_stencil, float1, pair<float1, uint8_t>> ret_type;
			ret_type ret;
			const auto lod = select_lod(lod_i, lod_or_bias_f);
#if defined(FLOOR_CXX17)
			size_t offset;
			if constexpr(!is_array) offset = coord_to_offset(img->level_info[lod], process_coord(img->level_info[lod], coord, coord_offset));
			else offset = coord_to_offset(img->level_info[lod], process_coord(img->level_info[lod], coord, coord_offset), layer);
#else
			const auto offset = __builtin_choose_expr(!is_array,
													  coord_to_offset(img->level_info[lod], process_coord(img->level_info[lod], coord, coord_offset)),
													  coord_to_offset(img->level_info[lod], process_coord(img->level_info[lod], coord, coord_offset), layer));
#endif
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
						 const offset_type& coord_offset,
						 const uint32_t layer,
						 const int32_t lod_i,
						 const float lod_or_bias_f) {
			// read/copy raw data
			constexpr const bool is_array = has_flag<COMPUTE_IMAGE_TYPE::FLAG_ARRAY>(type);
			constexpr const size_t bpp = image_bytes_per_pixel(type);
			const auto lod = select_lod(lod_i, lod_or_bias_f);
#if defined(FLOOR_CXX17)
			size_t offset;
			if constexpr(!is_array) offset = coord_to_offset(img->level_info[lod], process_coord(img->level_info[lod], coord, coord_offset));
			else offset = coord_to_offset(img->level_info[lod], process_coord(img->level_info[lod], coord, coord_offset), layer);
#else
			const auto offset = __builtin_choose_expr(!is_array,
													  coord_to_offset(img->level_info[lod], process_coord(img->level_info[lod], coord, coord_offset)),
													  coord_to_offset(img->level_info[lod], process_coord(img->level_info[lod], coord, coord_offset), layer));
#endif
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
			return vector4<ret_type> { ret_widened };
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
						  const uint32_t lod_input,
						  const float4& color) {
			constexpr const bool is_array = has_flag<COMPUTE_IMAGE_TYPE::FLAG_ARRAY>(type);
			constexpr const auto data_type = (type & COMPUTE_IMAGE_TYPE::__DATA_TYPE_MASK);
			constexpr const auto image_format = (type & COMPUTE_IMAGE_TYPE::__FORMAT_MASK);
			const auto lod = select_lod(lod_input);
#if defined(FLOOR_CXX17)
			size_t offset;
			if constexpr(!is_array) offset = coord_to_offset(img->level_info[lod], process_coord(img->level_info[lod], coord));
			else offset = coord_to_offset(img->level_info[lod], process_coord(img->level_info[lod], coord), layer);
#else
			const auto offset = __builtin_choose_expr(!is_array,
													  coord_to_offset(img->level_info[lod], process_coord(img->level_info[lod], coord)),
													  coord_to_offset(img->level_info[lod], process_coord(img->level_info[lod], coord), layer));
#endif
			
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
				else if(image_format == COMPUTE_IMAGE_TYPE::FORMAT_16) {
					// for 16-bit half float formats, data must be converted to 32-bit float data
					soft_f16 half_vals[4];
#pragma clang loop unroll(FLOOR_CLANG_UNROLL_FULL) vectorize(enable) interleave(enable)
					for(uint32_t i = 0; i < channel_count; ++i) {
						half_vals[i] = (soft_f16)color[i];
					}
					memcpy(&img->data[offset], half_vals, sizeof(soft_f16) * channel_count);
				}
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
						  const uint32_t lod_input,
						  const color_type& color) {
			depth_format_validity_check();
			
			constexpr const bool is_array = has_flag<COMPUTE_IMAGE_TYPE::FLAG_ARRAY>(type);
			constexpr const auto data_type = (type & COMPUTE_IMAGE_TYPE::__DATA_TYPE_MASK);
			constexpr const auto image_format = (type & COMPUTE_IMAGE_TYPE::__FORMAT_MASK);
			constexpr const bool has_stencil = has_flag<COMPUTE_IMAGE_TYPE::FLAG_STENCIL>(type);
			
			// depth value input is always a float -> convert it to the correct output format
			const auto lod = select_lod(lod_input);
#if defined(FLOOR_CXX17)
			size_t offset;
			if constexpr(!is_array) offset = coord_to_offset(img->level_info[lod], process_coord(img->level_info[lod], coord));
			else offset = coord_to_offset(img->level_info[lod], process_coord(img->level_info[lod], coord), layer);
#else
			const auto offset = __builtin_choose_expr(!is_array,
													  coord_to_offset(img->level_info[lod], process_coord(img->level_info[lod], coord)),
													  coord_to_offset(img->level_info[lod], process_coord(img->level_info[lod], coord), layer));
#endif
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
				auto depth_val = uint32_t(scale_factor * ((long double)*(float*)&color));
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
							   !has_flag<COMPUTE_IMAGE_TYPE::FLAG_DEPTH>(type) &&
							   ((type & COMPUTE_IMAGE_TYPE::__DATA_TYPE_MASK) == COMPUTE_IMAGE_TYPE::INT ||
								(type & COMPUTE_IMAGE_TYPE::__DATA_TYPE_MASK) == COMPUTE_IMAGE_TYPE::UINT))>* = nullptr>
		static void write(const host_device_image<type, is_lod, is_lod_float, is_bias>* img,
						  const coord_type& coord,
						  const uint32_t layer,
						  const uint32_t lod_input,
						  const vector4<scalar_type>& color) {
			// figure out the storage type/format of the image and create (cast to) the correct storage type from the input
			constexpr const bool is_array = has_flag<COMPUTE_IMAGE_TYPE::FLAG_ARRAY>(type);
			constexpr const auto channel_count = image_channel_count(type);
			typedef typename image_sized_data_type<type, image_bits_of_channel(type, 0)>::type storage_scalar_type;
			typedef vector_n<storage_scalar_type, channel_count> storage_type;
			static_assert(sizeof(storage_type) == image_bytes_per_pixel(type), "invalid storage type size!");
			// cast down to storage scalar type, then trim the vector to the image channel count
			const storage_type raw_data = color.template cast<storage_scalar_type>().template trim<channel_count>();
			const auto lod = select_lod(lod_input);
#if defined(FLOOR_CXX17)
			size_t offset;
			if constexpr(!is_array) offset = coord_to_offset(img->level_info[lod], process_coord(img->level_info[lod], coord));
			else offset = coord_to_offset(img->level_info[lod], process_coord(img->level_info[lod], coord), layer);
#else
			const auto offset = __builtin_choose_expr(!is_array,
													  coord_to_offset(img->level_info[lod], process_coord(img->level_info[lod], coord)),
													  coord_to_offset(img->level_info[lod], process_coord(img->level_info[lod], coord), layer));
#endif
			memcpy(&img->data[offset], &raw_data, sizeof(raw_data));
		}
	};
}

template <COMPUTE_IMAGE_TYPE sample_image_type, bool is_lod = false, bool is_lod_float = false, bool is_bias = false>
struct host_device_image {
	typedef conditional_t<(has_flag<COMPUTE_IMAGE_TYPE::READ>(sample_image_type) &&
						   !has_flag<COMPUTE_IMAGE_TYPE::WRITE>(sample_image_type)), const uint8_t, uint8_t> storage_type;
	typedef host_device_image<sample_image_type, is_lod, is_lod_float, is_bias> host_device_image_type;
	
	storage_type* data;
	const COMPUTE_IMAGE_TYPE runtime_image_type;
	alignas(16) const host_image_impl::image_level_info level_info[host_limits::max_mip_levels];
	
	// image read with linear interpolation (1D images)
	template <typename coord_type, typename offset_type,
			  COMPUTE_IMAGE_TYPE type = sample_image_type, typename... Args,
			  enable_if_t<(image_dim_count(type) == 1 &&
						   is_same<typename coord_type::decayed_scalar_type, float>::value)>* = nullptr>
	static auto read_linear(const host_device_image_type* img,
							const coord_type& coord,
							const offset_type& coord_offset,
							const uint32_t layer,
							const int32_t lod_i,
							const float lod_or_bias_f) {
		typedef decltype(host_device_image_type::read(img, coord, coord_offset, layer, lod_i, lod_or_bias_f)) color_type;
		
		// * normalize coord to [0, 1]
		// * scale it to [0, image dim]
		// * get fractional [0, 1) in this texel
		const auto lod = host_image_impl::fixed_image<type, is_lod, is_lod_float, is_bias>::select_lod(lod_i, lod_or_bias_f);
		const auto frac_coord = (coord.wrapped(1.0f) * img->level_info[lod].clamp_dim_float.x).fractional();
		// texel A is always the outside pixel, texel B is always the active one
		const color_type colors[2] {
			read(img, coord, coord_offset + int1 { frac_coord < 0.5f ? -1 : 1 }, layer, lod_i, lod_or_bias_f),
			read(img, coord, coord_offset, layer, lod_i, lod_or_bias_f),
		};
		// linear interpolation with t == 0 -> A, t == 1 -> B
		// for coord < 0.5: t = [0, 0.5) + 0.5 -> [0.5, 1)
		// for coord >= 0.5: t = 1.5 - [0.5, 1) -> "[1, 0.5)"
		return colors[0].interpolated(colors[1], (frac_coord < 0.5f ? frac_coord + 0.5f : 1.5f - frac_coord));
	}
	
	// image read with bilinear interpolation (2D images)
	template <typename coord_type, typename offset_type,
			  COMPUTE_IMAGE_TYPE type = sample_image_type, typename... Args,
			  enable_if_t<(image_dim_count(type) == 2 &&
						   is_same<typename coord_type::decayed_scalar_type, float>::value)>* = nullptr>
	static auto read_linear(const host_device_image_type* img,
							const coord_type& coord,
							const offset_type& coord_offset,
							const uint32_t layer,
							const int32_t lod_i,
							const float lod_or_bias_f) {
		typedef decltype(host_device_image_type::read(img, coord, coord_offset, layer, lod_i, lod_or_bias_f)) color_type;
		
		const auto lod = host_image_impl::fixed_image<type, is_lod, is_lod_float, is_bias>::select_lod(lod_i, lod_or_bias_f);
		const auto frac_coord = (coord.wrapped(1.0f) * img->level_info[lod].clamp_dim_float.xy).fractional();
		const int2 sample_offset { frac_coord.x < 0.5f ? -1 : 1, frac_coord.y < 0.5f ? -1 : 1 };
		color_type colors[4] {
			read(img, coord, coord_offset + int2 { sample_offset.x, sample_offset.y }, layer, lod_i, lod_or_bias_f),
			read(img, coord, coord_offset + int2 { 0, sample_offset.y }, layer, lod_i, lod_or_bias_f),
			read(img, coord, coord_offset + int2 { sample_offset.x, 0 }, layer, lod_i, lod_or_bias_f),
			read(img, coord, coord_offset, layer, lod_i, lod_or_bias_f),
		};
		// interpolate in x first, then y
		const float2 weights {
			(frac_coord.x < 0.5f ? frac_coord.x + 0.5f : 1.5f - frac_coord.x),
			(frac_coord.y < 0.5f ? frac_coord.y + 0.5f : 1.5f - frac_coord.y)
		};
		return colors[0].interpolate(colors[1], weights.x).interpolate(colors[2].interpolate(colors[3], weights.x), weights.y);
	}
	
	// image read with trilinear interpolation (3D images)
	template <typename coord_type, typename offset_type,
			  COMPUTE_IMAGE_TYPE type = sample_image_type, typename... Args,
			  enable_if_t<(image_dim_count(type) == 3 &&
						   is_same<typename coord_type::decayed_scalar_type, float>::value)>* = nullptr>
	static auto read_linear(const host_device_image_type* img,
							const coord_type& coord,
							const offset_type& coord_offset,
							const uint32_t layer,
							const int32_t lod_i,
							const float lod_or_bias_f) {
		typedef decltype(host_device_image_type::read(img, coord, coord_offset, layer, lod_i, lod_or_bias_f)) color_type;
		
		const auto lod = host_image_impl::fixed_image<type, is_lod, is_lod_float, is_bias>::select_lod(lod_i, lod_or_bias_f);
		const auto frac_coord = (coord.wrapped(1.0f) * img->level_info[lod].clamp_dim_float.xyz).fractional();
		const int3 sample_offset { frac_coord.x < 0.5f ? -1 : 1, frac_coord.y < 0.5f ? -1 : 1, frac_coord.z < 0.5f ? -1 : 1 };
		color_type colors[8] {
			read(img, coord, coord_offset + int3 { sample_offset.x, sample_offset.y, sample_offset.z }, layer, lod_i, lod_or_bias_f),
			read(img, coord, coord_offset + int3 { 0, sample_offset.y, sample_offset.z }, layer, lod_i, lod_or_bias_f),
			read(img, coord, coord_offset + int3 { sample_offset.x, 0, sample_offset.z }, layer, lod_i, lod_or_bias_f),
			read(img, coord, coord_offset + int3 { 0, 0, sample_offset.z }, layer, lod_i, lod_or_bias_f),
			read(img, coord, coord_offset + int3 { sample_offset.x, sample_offset.y, 0 }, layer, lod_i, lod_or_bias_f),
			read(img, coord, coord_offset + int3 { 0, sample_offset.y, 0 }, layer, lod_i, lod_or_bias_f),
			read(img, coord, coord_offset + int3 { sample_offset.x, 0, 0 }, layer, lod_i, lod_or_bias_f),
			read(img, coord, coord_offset, layer, lod_i, lod_or_bias_f),
		};
		// interpolate in x first, then y, then z
		const float3 weights {
			(frac_coord.x < 0.5f ? frac_coord.x + 0.5f : 1.5f - frac_coord.x),
			(frac_coord.y < 0.5f ? frac_coord.y + 0.5f : 1.5f - frac_coord.y),
			(frac_coord.z < 0.5f ? frac_coord.z + 0.5f : 1.5f - frac_coord.z)
		};
		// chain calls ftw
		return colors[0].interpolate(colors[1], weights.x).interpolate(colors[2].interpolate(colors[3], weights.x), weights.y).interpolate(
			   colors[4].interpolate(colors[5], weights.x).interpolate(colors[6].interpolate(colors[7], weights.x), weights.y), weights.z);
	}
	
	// image read with *linear interpolation, but integer coordinates -> just forward to nearest/point read
	template <typename coord_type, typename offset_type,
			  COMPUTE_IMAGE_TYPE type = sample_image_type, typename... Args,
			  enable_if_t<is_same<typename coord_type::decayed_scalar_type, int>::value>* = nullptr>
	static auto read_linear(const host_device_image_type* img,
							const coord_type& coord,
							const offset_type& coord_offset,
							const uint32_t layer,
							const int32_t lod_i,
							const float lod_or_bias_f) {
		return read(img, coord, coord_offset, layer, lod_i, lod_or_bias_f);
	}

// ignore fp comparison warnings
FLOOR_PUSH_WARNINGS()
FLOOR_IGNORE_WARNING(float-equal)
	
	// depth compare function (depth / no stencil)
	floor_inline_always static constexpr float perform_compare(const COMPARE_FUNCTION compare_function,
															   const float compare_value,
															   const float1& depth_value) {
		switch(compare_function) {
			case COMPARE_FUNCTION::NONE:
			case COMPARE_FUNCTION::NEVER: return 0.0f;
			case COMPARE_FUNCTION::ALWAYS: return 1.0f;
			case COMPARE_FUNCTION::LESS_OR_EQUAL: return (compare_value <= depth_value.x ? 1.0f : 0.0f);
			case COMPARE_FUNCTION::GREATER_OR_EQUAL: return (compare_value >= depth_value.x ? 1.0f : 0.0f);
			case COMPARE_FUNCTION::LESS: return (compare_value < depth_value.x ? 1.0f : 0.0f);
			case COMPARE_FUNCTION::GREATER: return (compare_value > depth_value.x ? 1.0f : 0.0f);
			case COMPARE_FUNCTION::EQUAL: return (compare_value == depth_value.x ? 1.0f : 0.0f);
			case COMPARE_FUNCTION::NOT_EQUAL: return (compare_value != depth_value.x ? 1.0f : 0.0f);
		}
		floor_unreachable();
	}
	
	// depth compare function (depth / stencil)
	floor_inline_always static constexpr float perform_compare(const COMPARE_FUNCTION compare_function,
															   const float compare_value,
															   const pair<float1, uint8_t>& depth_stencil_value) {
		return perform_compare(compare_function, compare_value, depth_stencil_value.first);
	}

FLOOR_POP_WARNINGS()
	
	// depth compare read (sample image, compare sampled depth with compare value according to compare function)
	template <typename coord_type, typename offset_type, COMPUTE_IMAGE_TYPE type = sample_image_type,
			  enable_if_t<(has_flag<COMPUTE_IMAGE_TYPE::FLAG_DEPTH>(type))>* = nullptr>
	static auto compare(const host_device_image_type* img,
						const coord_type& coord,
						const offset_type& coord_offset,
						const uint32_t layer,
						const int32_t lod_i,
						const float lod_or_bias_f,
						const COMPARE_FUNCTION compare_function,
						const float compare_value) {
		return perform_compare(compare_function, compare_value, read(img, coord, coord_offset, layer, lod_i, lod_or_bias_f));
	}
	
	// depth compare read with linear interpolation of the results / 1D images (sample image, compare each sampled depth value with the compare value according to compare function, then blend the result)
	template <typename coord_type, typename offset_type, COMPUTE_IMAGE_TYPE type = sample_image_type,
			  enable_if_t<(image_dim_count(type) == 1 &&
						   has_flag<COMPUTE_IMAGE_TYPE::FLAG_DEPTH>(type) &&
						   is_same<typename coord_type::decayed_scalar_type, float>::value)>* = nullptr>
	static auto compare_linear(const host_device_image_type* img,
							   const coord_type& coord,
							   const offset_type& coord_offset,
							   const uint32_t layer,
							   const int32_t lod_i,
							   const float lod_or_bias_f,
							   const COMPARE_FUNCTION compare_function,
							   const float compare_value) {
		typedef decltype(host_device_image_type::read(img, coord, coord_offset, layer, lod_i, lod_or_bias_f)) color_type;
		
		const auto lod = host_image_impl::fixed_image<type, is_lod, is_lod_float, is_bias>::select_lod(lod_i, lod_or_bias_f);
		const auto frac_coord = (coord.wrapped(1.0f) * img->level_info[lod].clamp_dim_float.x).fractional();
		const color_type depth_values[2] {
			read(img, coord, coord_offset + int1 { frac_coord < 0.5f ? -1 : 1 }, layer, lod_i, lod_or_bias_f),
			read(img, coord, coord_offset, layer, lod_i, lod_or_bias_f),
		};
		float1 cmp_results[2] {
			perform_compare(compare_function, compare_value, depth_values[0]),
			perform_compare(compare_function, compare_value, depth_values[1]),
		};
		return cmp_results[0].interpolated(cmp_results[1], (frac_coord < 0.5f ? frac_coord + 0.5f : 1.5f - frac_coord));
	}
	
	// depth compare read with linear interpolation of the results / 2D images (sample image, compare each sampled depth value with the compare value according to compare function, then blend the result)
	template <typename coord_type, typename offset_type, COMPUTE_IMAGE_TYPE type = sample_image_type,
			  enable_if_t<(image_dim_count(type) == 2 &&
						   has_flag<COMPUTE_IMAGE_TYPE::FLAG_DEPTH>(type) &&
						   is_same<typename coord_type::decayed_scalar_type, float>::value)>* = nullptr>
	static auto compare_linear(const host_device_image_type* img,
							   const coord_type& coord,
							   const offset_type& coord_offset,
							   const uint32_t layer,
							   const int32_t lod_i,
							   const float lod_or_bias_f,
							   const COMPARE_FUNCTION compare_function,
							   const float compare_value) {
		typedef decltype(host_device_image_type::read(img, coord, coord_offset, layer, lod_i, lod_or_bias_f)) color_type;
		
		const auto lod = host_image_impl::fixed_image<type, is_lod, is_lod_float, is_bias>::select_lod(lod_i, lod_or_bias_f);
		const auto frac_coord = (coord.wrapped(1.0f) * img->level_info[lod].clamp_dim_float.xy).fractional();
		const int2 sample_offset { frac_coord.x < 0.5f ? -1 : 1, frac_coord.y < 0.5f ? -1 : 1 };
		const color_type depth_values[4] {
			read(img, coord, coord_offset + int2 { sample_offset.x, sample_offset.y }, layer, lod_i, lod_or_bias_f),
			read(img, coord, coord_offset + int2 { 0, sample_offset.y }, layer, lod_i, lod_or_bias_f),
			read(img, coord, coord_offset + int2 { sample_offset.x, 0 }, layer, lod_i, lod_or_bias_f),
			read(img, coord, coord_offset, layer, lod_i, lod_or_bias_f),
		};
		// interpolate in x first, then y
		const float2 weights {
			(frac_coord.x < 0.5f ? frac_coord.x + 0.5f : 1.5f - frac_coord.x),
			(frac_coord.y < 0.5f ? frac_coord.y + 0.5f : 1.5f - frac_coord.y)
		};
		float1 cmp_results[4] {
			perform_compare(compare_function, compare_value, depth_values[0]),
			perform_compare(compare_function, compare_value, depth_values[1]),
			perform_compare(compare_function, compare_value, depth_values[2]),
			perform_compare(compare_function, compare_value, depth_values[3]),
		};
		return cmp_results[0].interpolate(cmp_results[1], weights.x).interpolate(cmp_results[2].interpolate(cmp_results[3], weights.x), weights.y);
	}
	
	// depth compare read with linear interpolation of the results / 3D images (sample image, compare each sampled depth value with the compare value according to compare function, then blend the result)
	template <typename coord_type, typename offset_type, COMPUTE_IMAGE_TYPE type = sample_image_type,
			  enable_if_t<(image_dim_count(type) == 3 &&
						   has_flag<COMPUTE_IMAGE_TYPE::FLAG_DEPTH>(type) &&
						   is_same<typename coord_type::decayed_scalar_type, float>::value)>* = nullptr>
	static auto compare_linear(const host_device_image_type* img,
							   const coord_type& coord,
							   const offset_type& coord_offset,
							   const uint32_t layer,
							   const int32_t lod_i,
							   const float lod_or_bias_f,
							   const COMPARE_FUNCTION compare_function,
							   const float compare_value) {
		typedef decltype(host_device_image_type::read(img, coord, coord_offset, layer, lod_i, lod_or_bias_f)) color_type;
		
		const auto lod = host_image_impl::fixed_image<type, is_lod, is_lod_float, is_bias>::select_lod(lod_i, lod_or_bias_f);
		const auto frac_coord = (coord.wrapped(1.0f) * img->level_info[lod].clamp_dim_float.xyz).fractional();
		const int3 sample_offset { frac_coord.x < 0.5f ? -1 : 1, frac_coord.y < 0.5f ? -1 : 1, frac_coord.z < 0.5f ? -1 : 1 };
		const color_type depth_values[8] {
			read(img, coord, coord_offset + int3 { sample_offset.x, sample_offset.y, sample_offset.z }, layer, lod_i, lod_or_bias_f),
			read(img, coord, coord_offset + int3 { 0, sample_offset.y, sample_offset.z }, layer, lod_i, lod_or_bias_f),
			read(img, coord, coord_offset + int3 { sample_offset.x, 0, sample_offset.z }, layer, lod_i, lod_or_bias_f),
			read(img, coord, coord_offset + int3 { 0, 0, sample_offset.z }, layer, lod_i, lod_or_bias_f),
			read(img, coord, coord_offset + int3 { sample_offset.x, sample_offset.y, 0 }, layer, lod_i, lod_or_bias_f),
			read(img, coord, coord_offset + int3 { 0, sample_offset.y, 0 }, layer, lod_i, lod_or_bias_f),
			read(img, coord, coord_offset + int3 { sample_offset.x, 0, 0 }, layer, lod_i, lod_or_bias_f),
			read(img, coord, coord_offset, layer, lod_i, lod_or_bias_f),
		};
		// interpolate in x first, then y, then z
		const float3 weights {
			(frac_coord.x < 0.5f ? frac_coord.x + 0.5f : 1.5f - frac_coord.x),
			(frac_coord.y < 0.5f ? frac_coord.y + 0.5f : 1.5f - frac_coord.y),
			(frac_coord.z < 0.5f ? frac_coord.z + 0.5f : 1.5f - frac_coord.z)
		};
		float1 cmp_results[8] {
			perform_compare(compare_function, compare_value, depth_values[0]),
			perform_compare(compare_function, compare_value, depth_values[1]),
			perform_compare(compare_function, compare_value, depth_values[2]),
			perform_compare(compare_function, compare_value, depth_values[3]),
			perform_compare(compare_function, compare_value, depth_values[4]),
			perform_compare(compare_function, compare_value, depth_values[5]),
			perform_compare(compare_function, compare_value, depth_values[6]),
			perform_compare(compare_function, compare_value, depth_values[7]),
		};
		// chain calls ftw
		return cmp_results[0].interpolate(cmp_results[1], weights.x).interpolate(cmp_results[2].interpolate(cmp_results[3], weights.x), weights.y).interpolate(
			   cmp_results[4].interpolate(cmp_results[5], weights.x).interpolate(cmp_results[6].interpolate(cmp_results[7], weights.x), weights.y), weights.z);
	}
	
	// technically invalid to call this with integer coordinates, but still forward this to the nearest sampling function
	template <typename coord_type, typename offset_type, COMPUTE_IMAGE_TYPE type = sample_image_type,
			  enable_if_t<(has_flag<COMPUTE_IMAGE_TYPE::FLAG_DEPTH>(type) &&
						   is_same<typename coord_type::decayed_scalar_type, int>::value)>* = nullptr>
	static auto compare_linear(const host_device_image_type* img,
							   const coord_type& coord,
							   const offset_type& coord_offset,
							   const uint32_t layer,
							   const int32_t lod_i,
							   const float lod_or_bias_f,
							   const COMPARE_FUNCTION compare_function,
							   const float compare_value) {
		return compare(img, coord, coord_offset, layer, lod_i, lod_or_bias_f, compare_function, compare_value);
	}
	
	// depth compare is not supported for non-depth images
	template <typename... Args, COMPUTE_IMAGE_TYPE type = sample_image_type,
			  enable_if_t<(!has_flag<COMPUTE_IMAGE_TYPE::FLAG_DEPTH>(type))>* = nullptr>
	static auto compare(Args&&...) {
		return 0.0f;
	}
	template <typename... Args, COMPUTE_IMAGE_TYPE type = sample_image_type,
			  enable_if_t<(!has_flag<COMPUTE_IMAGE_TYPE::FLAG_DEPTH>(type))>* = nullptr>
	static auto compare_linear(Args&&...) {
		return 0.0f;
	}
	
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
	static auto read(const host_device_image_type* img, Args&&... args) {
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
	auto read(const host_device_image_type* img, Args&&... args) {
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
	static auto read(const host_device_image_type* img, Args&&... args) {
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
	static auto read(const host_device_image_type* img, Args&&... args) {
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
	static auto read(const host_device_image_type* img, Args&&... args) {
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
	auto read(const host_device_image_type* img, Args&&... args) {
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
	static void write(const host_device_image_type* img, Args&&... args) {
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
	static void write(const host_device_image_type* img, Args&&... args) {
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
	static void write(const host_device_image_type* img, Args&&... args) {
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
	static void write(const host_device_image_type* img, Args&&... args) {
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
	static void write(const host_device_image_type* img, Args&&... args) {
		const auto runtime_base_type = img->runtime_image_type & (COMPUTE_IMAGE_TYPE::__FORMAT_MASK |
																  COMPUTE_IMAGE_TYPE::__CHANNELS_MASK |
																  COMPUTE_IMAGE_TYPE::__DATA_TYPE_MASK);
		switch(runtime_base_type) {
			// TODO: fix this
			/*FLOOR_RT_WRITE_IMAGE_CASE(COMPUTE_IMAGE_TYPE::FORMAT_2 | COMPUTE_IMAGE_TYPE::CHANNELS_1 | fixed_data_type)
			FLOOR_RT_WRITE_IMAGE_CASE(COMPUTE_IMAGE_TYPE::FORMAT_2 | COMPUTE_IMAGE_TYPE::CHANNELS_2 | fixed_data_type)
			FLOOR_RT_WRITE_IMAGE_CASE(COMPUTE_IMAGE_TYPE::FORMAT_2 | COMPUTE_IMAGE_TYPE::CHANNELS_3 | fixed_data_type)
			FLOOR_RT_WRITE_IMAGE_CASE(COMPUTE_IMAGE_TYPE::FORMAT_2 | COMPUTE_IMAGE_TYPE::CHANNELS_4 | fixed_data_type)
			
			FLOOR_RT_WRITE_IMAGE_CASE(COMPUTE_IMAGE_TYPE::FORMAT_4 | COMPUTE_IMAGE_TYPE::CHANNELS_1 | fixed_data_type)
			FLOOR_RT_WRITE_IMAGE_CASE(COMPUTE_IMAGE_TYPE::FORMAT_4 | COMPUTE_IMAGE_TYPE::CHANNELS_2 | fixed_data_type)
			FLOOR_RT_WRITE_IMAGE_CASE(COMPUTE_IMAGE_TYPE::FORMAT_4 | COMPUTE_IMAGE_TYPE::CHANNELS_3 | fixed_data_type)
			FLOOR_RT_WRITE_IMAGE_CASE(COMPUTE_IMAGE_TYPE::FORMAT_4 | COMPUTE_IMAGE_TYPE::CHANNELS_4 | fixed_data_type)*/
			
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
	static void write(const host_device_image_type* img, Args&&... args) {
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
