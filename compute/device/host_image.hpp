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

#ifndef __FLOOR_COMPUTE_DEVICE_HOST_IMAGE_HPP__
#define __FLOOR_COMPUTE_DEVICE_HOST_IMAGE_HPP__

#if defined(FLOOR_COMPUTE_HOST)

template <COMPUTE_IMAGE_TYPE image_type, bool readable, bool writable>
struct host_device_image {
	static constexpr bool is_readable() { return readable; }
	static constexpr bool is_writable() { return writable; }
	static constexpr bool is_read_only() { return readable && !writable; }
	
	typedef conditional_t<is_read_only(), const uint8_t, uint8_t> storage_type;
	__attribute__((align_value(128))) storage_type* data;
	const int4 image_dim;
	
	//!
	size_t coord_to_offset(const int& coord) const {
		return const_math::clamp(coord, 0, image_dim.x - 1) * image_bytes_per_pixel(image_type);
	}
	
	//!
	size_t coord_to_offset(const int2& coord) const {
		return (image_dim.x * const_math::clamp(coord.y, 0, image_dim.y - 1) +
				const_math::clamp(coord.x, 0, image_dim.x - 1)) * image_bytes_per_pixel(image_type);
	}
	
	//!
	size_t coord_to_offset(const int3& coord) const {
		return (image_dim.x * image_dim.y * const_math::clamp(coord.z, 0, image_dim.z - 1) +
				image_dim.x * const_math::clamp(coord.y, 0, image_dim.y - 1) +
				const_math::clamp(coord.x, 0, image_dim.x - 1)) * image_bytes_per_pixel(image_type);
	}
	
	//!
	size_t coord_to_offset(const int4& coord) const {
		return (image_dim.x * image_dim.y * image_dim.z * const_math::clamp(coord.w, 0, image_dim.w - 1) +
				image_dim.x * image_dim.y * const_math::clamp(coord.z, 0, image_dim.z - 1) +
				image_dim.x * const_math::clamp(coord.y, 0, image_dim.y - 1) +
				const_math::clamp(coord.x, 0, image_dim.x - 1)) * image_bytes_per_pixel(image_type);
	}
};

template <COMPUTE_IMAGE_TYPE type> using ro_image = const host_device_image<type, true, false>*;
template <COMPUTE_IMAGE_TYPE type> using wo_image = host_device_image<type, false, true>*;
template <COMPUTE_IMAGE_TYPE type> using rw_image = host_device_image<type, true, true>*;

// helper functions
template <COMPUTE_IMAGE_TYPE image_type> static constexpr auto compute_image_bpc() {
	return const_array<uint32_t, 4> {{
		image_bits_of_channel(image_type, 0),
		image_bits_of_channel(image_type, 1),
		image_bits_of_channel(image_type, 2),
		image_bits_of_channel(image_type, 3)
	}};
}

template <COMPUTE_IMAGE_TYPE image_type, size_t N,
		  enable_if_t<(image_type & COMPUTE_IMAGE_TYPE::__DATA_TYPE_MASK) != COMPUTE_IMAGE_TYPE::FLOAT>* = nullptr>
floor_inline_always auto extract_channels(const uint8_t (&raw_data)[N]) {
	constexpr const auto image_format = (image_type & COMPUTE_IMAGE_TYPE::__FORMAT_MASK);
	
	// validity checking: depth reading is handled elsewhere
	static_assert(image_format != COMPUTE_IMAGE_TYPE::FORMAT_24 &&
				  image_format != COMPUTE_IMAGE_TYPE::FORMAT_24_8 &&
				  image_format != COMPUTE_IMAGE_TYPE::FORMAT_32_8, "invalid integer format!");
	
	// validity checking: unsupported formats
	static_assert(image_format != COMPUTE_IMAGE_TYPE::FORMAT_3_3_2 &&
				  image_format != COMPUTE_IMAGE_TYPE::FORMAT_5_5_5 &&
				  image_format != COMPUTE_IMAGE_TYPE::FORMAT_5_5_5_1 &&
				  image_format != COMPUTE_IMAGE_TYPE::FORMAT_5_6_5 &&
				  image_format != COMPUTE_IMAGE_TYPE::FORMAT_9_9_9_5 &&
				  image_format != COMPUTE_IMAGE_TYPE::FORMAT_10 &&
				  image_format != COMPUTE_IMAGE_TYPE::FORMAT_10_10_10_2 &&
				  image_format != COMPUTE_IMAGE_TYPE::FORMAT_11_11_10 &&
				  image_format != COMPUTE_IMAGE_TYPE::FORMAT_12_12_12 &&
				  image_format != COMPUTE_IMAGE_TYPE::FORMAT_12_12_12_12, "unsupported format!");
	
	// get bits of each channel (0 if channel doesn't exist)
	constexpr const auto bpc = compute_image_bpc<image_type>();
	// figure out which type we need to store each channel (max size is used)
	typedef typename image_sized_data_type<image_type, max(max(bpc[0], bpc[1]), max(bpc[2], bpc[3]))>::type channel_type;
	// and the appropriate unsigned format
	typedef typename image_sized_data_type<COMPUTE_IMAGE_TYPE::UINT, max(max(bpc[0], bpc[1]), max(bpc[2], bpc[3]))>::type uchannel_type;
	
	// and now for bit voodoo:
	constexpr const uint32_t channel_count = image_channel_count(image_type);
	array<channel_type, channel_count> ret;
	switch(image_format) {
		// uniform formats
		case COMPUTE_IMAGE_TYPE::FORMAT_2:
			for(uint32_t i = 0; i < channel_count; ++i) {
				*((uchannel_type*)&ret[i]) = (raw_data[0] >> (6u - 2u * i)) & 0b11u;
			}
			break;
		case COMPUTE_IMAGE_TYPE::FORMAT_4:
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
			
		// special formats (TODO: implement this when needed, opencl/cuda don't support this either)
		case COMPUTE_IMAGE_TYPE::FORMAT_3_3_2:
		case COMPUTE_IMAGE_TYPE::FORMAT_5_5_5:
		case COMPUTE_IMAGE_TYPE::FORMAT_5_5_5_1:
		case COMPUTE_IMAGE_TYPE::FORMAT_5_6_5:
		case COMPUTE_IMAGE_TYPE::FORMAT_9_9_9_5:
		case COMPUTE_IMAGE_TYPE::FORMAT_10:
		case COMPUTE_IMAGE_TYPE::FORMAT_10_10_10_2:
		case COMPUTE_IMAGE_TYPE::FORMAT_11_11_10:
		case COMPUTE_IMAGE_TYPE::FORMAT_12_12_12:
		case COMPUTE_IMAGE_TYPE::FORMAT_12_12_12_12:
		default:
			floor_unreachable();
	}
	
	// need to fix up the sign bit for non-8/16/32/64-bit signed formats
	if((image_type & COMPUTE_IMAGE_TYPE::__DATA_TYPE_MASK) == COMPUTE_IMAGE_TYPE::INT) {
		constexpr const const_array<uchannel_type, 4> high_bits {{
			uchannel_type(1) << (max(bpc[0], 1u) - 1u),
			uchannel_type(1) << (max(bpc[1], 1u) - 1u),
			uchannel_type(1) << (max(bpc[2], 1u) - 1u),
			uchannel_type(1) << (max(bpc[3], 1u) - 1u)
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
template <COMPUTE_IMAGE_TYPE image_type, size_t N,
		  enable_if_t<(image_type & COMPUTE_IMAGE_TYPE::__DATA_TYPE_MASK) == COMPUTE_IMAGE_TYPE::FLOAT>* = nullptr>
floor_inline_always auto extract_channels(const uint8_t (&raw_data)[N]) {
	// this is a dummy function and never called, but necessary for compilation
	return vector_n<uint8_t, image_channel_count(image_type)> {};
}

// 1D "vectors" must return a scalar value, 2D/3D/4D must return a vector
template <typename scalar_type, size_t N, enable_if_t<N == 1>* = nullptr>
floor_inline_always auto host_image_fit_return_type(const vector_n<scalar_type, N>& vec) {
	return vec.x;
}
template <typename scalar_type, size_t N, enable_if_t<N >= 2>* = nullptr>
floor_inline_always auto host_image_fit_return_type(const vector_n<scalar_type, N>& vec) {
	return vec;
}

// image read functions
template <COMPUTE_IMAGE_TYPE image_type, bool readable, bool writable,
		  enable_if_t<((has_flag<COMPUTE_IMAGE_TYPE::FLAG_NORMALIZED>(image_type) ||
						(image_type & COMPUTE_IMAGE_TYPE::__DATA_TYPE_MASK) == COMPUTE_IMAGE_TYPE::FLOAT) &&
					   !has_flag<COMPUTE_IMAGE_TYPE::FLAG_DEPTH>(image_type))>* = nullptr>
floor_inline_always auto read(const host_device_image<image_type, readable, writable>* img, int2 coord) {
	static_assert(readable, "image must be readable!");
	
	// read/copy raw data
	constexpr const size_t bpp = image_bytes_per_pixel(image_type);
	uint8_t raw_data[bpp];
	memcpy(raw_data, &img->data[img->coord_to_offset(coord)], bpp);
	
	// extract channel bits/bytes
	constexpr const auto data_type = (image_type & COMPUTE_IMAGE_TYPE::__DATA_TYPE_MASK);
	constexpr const auto channel_count = image_channel_count(image_type);
	vector_n<float, channel_count> ret;
	if(data_type == COMPUTE_IMAGE_TYPE::FLOAT) {
		// for float formats, just pass-through raw data
		ret = *(decltype(ret)*)raw_data;
	}
	else {
		// extract all channels from the raw data
		const auto channels = extract_channels<image_type>(raw_data);
		
		// normalize + convert to float
		constexpr const auto bpc = compute_image_bpc<image_type>();
		if(data_type == COMPUTE_IMAGE_TYPE::UINT) {
			// normalized unsigned-integer formats, normalized to [0, 1]
#pragma clang loop unroll(full) vectorize(enable) interleave(enable)
			for(uint32_t i = 0; i < channel_count; ++i) {
				// normalized = channel / (2^bpc_i - 1)
				ret[i] = float(channels[i]) * float(1.0 / double((1ull << bpc[i]) - 1ull));
			}
		}
		else if(data_type == COMPUTE_IMAGE_TYPE::INT) {
			// normalized integer formats, normalized to [-1, 1]
#pragma clang loop unroll(full) vectorize(enable) interleave(enable)
			for(uint32_t i = 0; i < channel_count; ++i) {
				// normalized = channel / (2^(bpc_i - 1) - 1)
				ret[i] = float(channels[i]) * float(1.0 / double((1ull << (bpc[i] - 1u)) - 1ull));
			}
		}
	}
	return host_image_fit_return_type<float, channel_count>(ret);
}

template <COMPUTE_IMAGE_TYPE image_type, bool readable, bool writable,
		  enable_if_t<has_flag<COMPUTE_IMAGE_TYPE::FLAG_DEPTH>(image_type)>* = nullptr>
floor_inline_always auto read(const host_device_image<image_type, readable, writable>* img, int2 coord) {
	static_assert(readable, "image must be readable!");
	
	// validate all the things
	constexpr const auto data_type = (image_type & COMPUTE_IMAGE_TYPE::__DATA_TYPE_MASK);
	static_assert(data_type == COMPUTE_IMAGE_TYPE::FLOAT ||
				  data_type == COMPUTE_IMAGE_TYPE::UINT, "invalid depth data type!");
	
	constexpr const auto image_format = (image_type & COMPUTE_IMAGE_TYPE::__FORMAT_MASK);
	constexpr const bool has_stencil = has_flag<COMPUTE_IMAGE_TYPE::FLAG_STENCIL>(image_type);
	static_assert((!has_stencil &&
				   ((image_format == COMPUTE_IMAGE_TYPE::FORMAT_16 && data_type == COMPUTE_IMAGE_TYPE::UINT) ||
					(image_format == COMPUTE_IMAGE_TYPE::FORMAT_24 && data_type == COMPUTE_IMAGE_TYPE::UINT) ||
					image_format == COMPUTE_IMAGE_TYPE::FORMAT_32 /* supported by both */)) ||
				  (has_stencil &&
				   ((image_format == COMPUTE_IMAGE_TYPE::FORMAT_24_8 && data_type == COMPUTE_IMAGE_TYPE::UINT) ||
					(image_format == COMPUTE_IMAGE_TYPE::FORMAT_32_8 && data_type == COMPUTE_IMAGE_TYPE::FLOAT))),
				  "invalid depth format!");
	
	constexpr const auto channel_count = image_channel_count(image_type);
	static_assert((!has_stencil && channel_count == 1) ||
				  (has_stencil && channel_count == 2), "invalid channel count for depth format!");
	
	// depth is always returned as a float, with stencil a a <float, 8-bit uint> pair
	// NOTE: neither opencl, nor cuda support reading depth+stencil images, so a proper return type is unclear right now
	typedef conditional_t<!has_stencil, float, pair<float, uint8_t>> ret_type;
	ret_type ret;
	const auto offset = img->coord_to_offset(coord);
	if(image_format == COMPUTE_IMAGE_TYPE::FLOAT) {
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

template <COMPUTE_IMAGE_TYPE image_type, bool readable, bool writable,
		  enable_if_t<(!has_flag<COMPUTE_IMAGE_TYPE::FLAG_NORMALIZED>(image_type) &&
					   (image_type & COMPUTE_IMAGE_TYPE::__DATA_TYPE_MASK) == COMPUTE_IMAGE_TYPE::INT)>* = nullptr>
floor_inline_always auto read(const host_device_image<image_type, readable, writable>* img, int2 coord) {
	static_assert(readable, "image must be readable!");
	
	// read/copy raw data
	constexpr const size_t bpp = image_bytes_per_pixel(image_type);
	uint8_t raw_data[bpp];
	memcpy(raw_data, &img->data[img->coord_to_offset(coord)], bpp);
	
	constexpr const auto channel_count = image_channel_count(image_type);
	vector_n<int32_t, channel_count> ret =
		*(vector_n<typename image_sized_data_type<image_type, image_bits_of_channel(image_type, 0)>::type, channel_count>*)raw_data;
	return host_image_fit_return_type<int32_t, channel_count>(ret);
}

template <COMPUTE_IMAGE_TYPE image_type, bool readable, bool writable,
		  enable_if_t<(!has_flag<COMPUTE_IMAGE_TYPE::FLAG_NORMALIZED>(image_type) &&
					   (image_type & COMPUTE_IMAGE_TYPE::__DATA_TYPE_MASK) == COMPUTE_IMAGE_TYPE::UINT)>* = nullptr>
floor_inline_always auto read(const host_device_image<image_type, readable, writable>* img, int2 coord) {
	static_assert(readable, "image must be readable!");
	
	// read/copy raw data
	constexpr const size_t bpp = image_bytes_per_pixel(image_type);
	uint8_t raw_data[bpp];
	memcpy(raw_data, &img->data[img->coord_to_offset(coord)], bpp);
	
	constexpr const auto channel_count = image_channel_count(image_type);
	vector_n<uint32_t, channel_count> ret =
		*(vector_n<typename image_sized_data_type<image_type, image_bits_of_channel(image_type, 0)>::type, channel_count>*)raw_data;
	return host_image_fit_return_type<uint32_t, channel_count>(ret);
}

// image write functions
template <COMPUTE_IMAGE_TYPE image_type, bool readable, bool writable>
void write(host_device_image<image_type, readable, writable>* img, const int2& coord, const float4& color) {
	static_assert(writable, "image must be writable!");
	
	constexpr const auto channel_count = image_channel_count(image_type);
	const auto offset = img->coord_to_offset(coord);
	// TODO: same as read, but in reverse!
	
	// for testing purposes:
	if((image_type & COMPUTE_IMAGE_TYPE::__FORMAT_MASK) == COMPUTE_IMAGE_TYPE::FORMAT_8 &&
	   (image_type & COMPUTE_IMAGE_TYPE::__DATA_TYPE_MASK) == COMPUTE_IMAGE_TYPE::UINT &&
	   has_flag<COMPUTE_IMAGE_TYPE::FLAG_NORMALIZED>(image_type)) {
#pragma clang loop unroll(full) vectorize(enable) interleave(enable)
		for(uint32_t i = 0; i < channel_count; ++i) {
			img->data[offset + i] = const_math::clamp((uint32_t)(color[i] * 255.0f), 0u, 255u);
		}
	}
}

#endif

#endif
