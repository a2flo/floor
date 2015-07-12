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
	
	typedef conditional_t<is_readable(), const uint8_t, uint8_t> storage_type;
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

template <COMPUTE_IMAGE_TYPE image_type, size_t N>
auto extract_channels(const uint8_t (&raw_data)[N]) {
	// get bits of each channel (0 if channel doesn't exist)
	constexpr const auto bpc = compute_image_bpc<image_type>();
	// figure out which type we need to store each channel (max size is used)
	typedef typename image_sized_data_type<image_type, max(max(bpc[0], bpc[1]), max(bpc[2], bpc[3]))>::type channel_type;
	// and the appropriate unsigned format
	typedef typename image_sized_data_type<COMPUTE_IMAGE_TYPE::UINT, max(max(bpc[0], bpc[1]), max(bpc[2], bpc[3]))>::type uchannel_type;
	
	// and now for bit voodoo:
	constexpr const uint32_t channel_count = image_channel_count(image_type);
	array<channel_type, channel_count> ret;
	switch(image_type & COMPUTE_IMAGE_TYPE::__FORMAT_MASK) {
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
			for(uint32_t i = 0; i < channel_count; ++i) {
				*((uchannel_type*)&ret[i]) = raw_data[i];
			}
			break;
		case COMPUTE_IMAGE_TYPE::FORMAT_16:
			for(uint32_t i = 0; i < channel_count; ++i) {
				*((uchannel_type*)&ret[i]) = raw_data[i * 2u] + (raw_data[i * 2u + 1u] << 8u);
			}
			break;
		case COMPUTE_IMAGE_TYPE::FORMAT_32:
			for(uint32_t i = 0; i < channel_count; ++i) {
				*((uchannel_type*)&ret[i]) = (raw_data[i * 4u] +
											  (raw_data[i * 4u + 1u] << 8u) +
											  (raw_data[i * 4u + 2u] << 16u) +
											  (raw_data[i * 4u + 3u] << 24u));
			}
			break;
		case COMPUTE_IMAGE_TYPE::FORMAT_64:
			for(uint32_t i = 0; i < channel_count; ++i) {
				*((uchannel_type*)&ret[i]) = (uint64_t(raw_data[i * 8u]) +
											  (uint64_t(raw_data[i * 8u + 1u]) << 8ull) +
											  (uint64_t(raw_data[i * 8u + 2u]) << 16ull) +
											  (uint64_t(raw_data[i * 8u + 3u]) << 24ull) +
											  (uint64_t(raw_data[i * 8u + 4u]) << 32ull) +
											  (uint64_t(raw_data[i * 8u + 5u]) << 40ull) +
											  (uint64_t(raw_data[i * 8u + 6u]) << 48ull) +
											  (uint64_t(raw_data[i * 8u + 7u]) << 56ull));
			}
			break;
			
		// special formats (TODO: !)
		case COMPUTE_IMAGE_TYPE::FORMAT_3_3_2:
			break;
		case COMPUTE_IMAGE_TYPE::FORMAT_5_5_5:
			break;
		case COMPUTE_IMAGE_TYPE::FORMAT_5_5_5_1:
			break;
		case COMPUTE_IMAGE_TYPE::FORMAT_5_6_5:
			break;
		case COMPUTE_IMAGE_TYPE::FORMAT_9_9_9_5:
			break;
		case COMPUTE_IMAGE_TYPE::FORMAT_10:
			break;
		case COMPUTE_IMAGE_TYPE::FORMAT_10_10_10_2:
			break;
		case COMPUTE_IMAGE_TYPE::FORMAT_11_11_10:
			break;
		case COMPUTE_IMAGE_TYPE::FORMAT_12_12_12:
			break;
		case COMPUTE_IMAGE_TYPE::FORMAT_12_12_12_12:
			break;
		case COMPUTE_IMAGE_TYPE::FORMAT_24:
			break;
		case COMPUTE_IMAGE_TYPE::FORMAT_24_8:
			break;
		case COMPUTE_IMAGE_TYPE::FORMAT_32_8:
			break;
	}
	
	// need to fix up the sign bit for non-8/16/32/64-bit signed formats
	if((image_type & COMPUTE_IMAGE_TYPE::__DATA_TYPE_MASK) == COMPUTE_IMAGE_TYPE::INT) {
		constexpr const const_array<uchannel_type, 4> high_bits {{
			uchannel_type(1) << (bpc[0] - 1u),
			uchannel_type(1) << (bpc[1] - 1u),
			uchannel_type(1) << (bpc[2] - 1u),
			uchannel_type(1) << (bpc[3] - 1u)
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

// image read functions
template <COMPUTE_IMAGE_TYPE image_type, bool readable, bool writable,
		  enable_if_t<(has_flag<COMPUTE_IMAGE_TYPE::FLAG_NORMALIZED>(image_type) ||
					   (image_type & COMPUTE_IMAGE_TYPE::__DATA_TYPE_MASK) == COMPUTE_IMAGE_TYPE::FLOAT)>* = nullptr>
floor_inline_always auto read(const host_device_image<image_type, readable, writable>* img, int2 coord) {
	static_assert(readable, "image must be readable!");
	
	// read/copy raw data
	constexpr const size_t bpp = image_bytes_per_pixel(image_type);
	uint8_t raw_data[bpp];
	memcpy(raw_data, &img->data[img->coord_to_offset(coord)], bpp);
	
	// extract channel bits/bytes
	vector_n<float, image_channel_count(image_type)> ret;
	if((image_type & COMPUTE_IMAGE_TYPE::__DATA_TYPE_MASK) == COMPUTE_IMAGE_TYPE::FLOAT) {
		// for float formats, just pass-through raw data
		ret = *(decltype(ret)*)raw_data;
	}
	else {
		// extract all channels from the raw data
		const auto channels = extract_channels<image_type>(raw_data);
		
		// normalize + convert to float
		constexpr const auto bpc = compute_image_bpc<image_type>();
		constexpr const auto channel_count = image_channel_count(image_type);
		if((image_type & COMPUTE_IMAGE_TYPE::__DATA_TYPE_MASK) == COMPUTE_IMAGE_TYPE::UINT) {
			// normalized unsigned-integer formats, normalized to [0, 1]
#pragma clang loop unroll(full) vectorize(enable) interleave(enable)
			for(uint32_t i = 0; i < channel_count; ++i) {
				// normalized = channel / (2^bpc_i - 1)
				ret[i] = float(channels[i]) / float((1ull << bpc[i]) - 1ull);
			}
		}
		else if((image_type & COMPUTE_IMAGE_TYPE::__DATA_TYPE_MASK) == COMPUTE_IMAGE_TYPE::INT) {
			// normalized integer formats, normalized to [-1, 1]
#pragma clang loop unroll(full) vectorize(enable) interleave(enable)
			for(uint32_t i = 0; i < channel_count; ++i) {
				// normalized = channel / (2^(bpc_i - 1) - 1)
				ret[i] = float(channels[i]) / float((1ull << (bpc[i] - 1u)) - 1ull);
			}
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
	
	// TODO: handle formats with different channel counts / special formats
	constexpr const auto channel_count = image_channel_count(image_type);
	vector_n<int32_t, channel_count> ret =
		*(vector_n<typename image_sized_data_type<image_type, image_bits_of_channel(image_type, 0)>::type, channel_count>*)raw_data;
	return ret;
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
	
	// TODO: handle formats with different channel counts / special formats
	constexpr const auto channel_count = image_channel_count(image_type);
	vector_n<uint32_t, channel_count> ret =
		*(vector_n<typename image_sized_data_type<image_type, image_bits_of_channel(image_type, 0)>::type, channel_count>*)raw_data;
	return ret;
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
