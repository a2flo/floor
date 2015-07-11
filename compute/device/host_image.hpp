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

template <COMPUTE_IMAGE_TYPE type, bool readable, bool writable>
struct host_device_image {
	static constexpr bool is_readable() { return readable; }
	static constexpr bool is_writable() { return writable; }
	static constexpr bool is_read_only() { return readable && !writable; }
	
	typedef conditional_t<is_readable(), const uint8_t, uint8_t> storage_type;
	__attribute__((align_value(128))) storage_type* data;
	const int4 image_dim;
	
	//!
	size_t coord_to_offset(const int& coord) const {
		return const_math::clamp(coord, 0, image_dim.x - 1);
	}
	
	//!
	size_t coord_to_offset(const int2& coord) const {
		return image_dim.x * const_math::clamp(coord.y, 0, image_dim.y - 1) + const_math::clamp(coord.x, 0, image_dim.x - 1);
	}
	
	//!
	size_t coord_to_offset(const int3& coord) const {
		return (image_dim.x * image_dim.y * const_math::clamp(coord.z, 0, image_dim.z - 1) +
				image_dim.x * const_math::clamp(coord.y, 0, image_dim.y - 1) +
				const_math::clamp(coord.x, 0, image_dim.x - 1));
	}
};

template <COMPUTE_IMAGE_TYPE type> using ro_image = const host_device_image<type, true, false>;
template <COMPUTE_IMAGE_TYPE type> using wo_image = host_device_image<type, false, true>;
template <COMPUTE_IMAGE_TYPE type> using rw_image = host_device_image<type, true, true>;

// image read functions
template <COMPUTE_IMAGE_TYPE image_type, bool readable, bool writable,
		  enable_if_t<(has_flag<COMPUTE_IMAGE_TYPE::FLAG_NORMALIZED>(image_type) ||
					   (image_type & COMPUTE_IMAGE_TYPE::__DATA_TYPE_MASK) == COMPUTE_IMAGE_TYPE::FLOAT)>* = nullptr>
floor_inline_always auto read(const host_device_image<image_type, readable, writable>& img, int2 coord) {
	static_assert(readable, "image must be readable!");
	
	// read/copy raw data
	constexpr const size_t bpp = image_bytes_per_pixel(image_type);
	uint8_t raw_data[bpp];
	memcpy(raw_data, &img.data[img.coord_to_offset(coord)], bpp);
	
	// extract channel bits/bytes
	vector_n<float, image_channel_count(image_type)> ret;
	if((image_type & COMPUTE_IMAGE_TYPE::__DATA_TYPE_MASK) == COMPUTE_IMAGE_TYPE::FLOAT) {
		// for float formats, just pass-through raw data
		ret = *(decltype(ret)*)raw_data;
	}
	else if((image_type & COMPUTE_IMAGE_TYPE::__DATA_TYPE_MASK) == COMPUTE_IMAGE_TYPE::UINT) {
		// normalized unsigned-integer formats, normalized to [0, 1]
		// TODO: !
	}
	else if((image_type & COMPUTE_IMAGE_TYPE::__DATA_TYPE_MASK) == COMPUTE_IMAGE_TYPE::INT) {
		// normalized integer formats, normalized to [-1, 1]
		// TODO: !
	}
	return ret;
}

template <COMPUTE_IMAGE_TYPE image_type, bool readable, bool writable,
		  enable_if_t<(!has_flag<COMPUTE_IMAGE_TYPE::FLAG_NORMALIZED>(image_type) &&
					   (image_type & COMPUTE_IMAGE_TYPE::__DATA_TYPE_MASK) == COMPUTE_IMAGE_TYPE::INT)>* = nullptr>
floor_inline_always auto read(const host_device_image<image_type, readable, writable>& img, int2 coord) {
	static_assert(readable, "image must be readable!");
	
	// read/copy raw data
	constexpr const size_t bpp = image_bytes_per_pixel(image_type);
	uint8_t raw_data[bpp];
	memcpy(raw_data, &img.data[img.coord_to_offset(coord)], bpp);
	
	// TODO: handle formats with different channel counts / special formats
	constexpr const auto channel_count = image_channel_count(image_type);
	vector_n<int32_t, channel_count> ret =
		*(vector_n<typename image_sized_data_type<image_type, image_bits_of_channel(image_type, 0)>::type, channel_count>*)raw_data;
	return ret;
}

template <COMPUTE_IMAGE_TYPE image_type, bool readable, bool writable,
		  enable_if_t<(!has_flag<COMPUTE_IMAGE_TYPE::FLAG_NORMALIZED>(image_type) &&
					   (image_type & COMPUTE_IMAGE_TYPE::__DATA_TYPE_MASK) == COMPUTE_IMAGE_TYPE::UINT)>* = nullptr>
floor_inline_always auto read(const host_device_image<image_type, readable, writable>& img, int2 coord) {
	static_assert(readable, "image must be readable!");
	
	// read/copy raw data
	constexpr const size_t bpp = image_bytes_per_pixel(image_type);
	uint8_t raw_data[bpp];
	memcpy(raw_data, &img.data[img.coord_to_offset(coord)], bpp);
	
	// TODO: handle formats with different channel counts / special formats
	constexpr const auto channel_count = image_channel_count(image_type);
	vector_n<uint32_t, channel_count> ret =
		*(vector_n<typename image_sized_data_type<image_type, image_bits_of_channel(image_type, 0)>::type, channel_count>*)raw_data;
	return ret;
}

// image write functions
template <COMPUTE_IMAGE_TYPE type, bool readable, bool writable>
void write(host_device_image<type, readable, writable>& img, const int2& coord, const float4& color) {
	static_assert(writable, "image must be writable!");
	
	const auto offset = img.coord_to_offset(coord);
	// TODO: !
}

#endif

#endif
