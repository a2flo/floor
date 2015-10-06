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

#ifndef __FLOOR_COMPUTE_DEVICE_CUDA_IMAGE_HPP__
#define __FLOOR_COMPUTE_DEVICE_CUDA_IMAGE_HPP__

#if defined(FLOOR_COMPUTE_CUDA)

template <COMPUTE_IMAGE_TYPE image_type> struct image {
	// NOTE: this needs to packed like this, so that clang/llvm don't do struct expansion when only one is used
	union {
		struct {
			const uint64_t tex[4];
			const uint64_t surf;
		};
		const uint64_t ids[5];
	};
};

#define ro_image read_only cuda_ro_image
template <COMPUTE_IMAGE_TYPE image_type> using cuda_ro_image =
image<image_type & (~COMPUTE_IMAGE_TYPE::WRITE) | COMPUTE_IMAGE_TYPE::READ>;

#define wo_image write_only cuda_wo_image
template <COMPUTE_IMAGE_TYPE image_type> using cuda_wo_image =
image<image_type & (~COMPUTE_IMAGE_TYPE::READ) | COMPUTE_IMAGE_TYPE::WRITE>;

#define rw_image read_write cuda_rw_image
template <COMPUTE_IMAGE_TYPE image_type> using cuda_rw_image =
image<image_type | COMPUTE_IMAGE_TYPE::READ_WRITE>;

namespace cuda_image {
	// convert any coordinate vector type to int* or float*
	template <typename coord_type, typename ret_coord_type = vector_n<conditional_t<is_integral<typename coord_type::decayed_scalar_type>::value, int, float>, coord_type::dim>>
	static auto convert_coord(const coord_type& coord) {
		return ret_coord_type { coord };
	}
	// convert any fundamental (single value) coordinate type to int1 or float1
	template <typename coord_type, typename ret_coord_type = conditional_t<is_integral<coord_type>::value, int1, float1>, enable_if_t<is_fundamental<coord_type>::value>>
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
	
	// returns the texture index corresponding to the used coordinate type
	// with: int/uint coords -> non-normalized coordinates texture+sampler, float coords -> normalized coordinates texture+sampler
	template <typename coord_type, enable_if_t<is_floor_vector<coord_type>::value>* = nullptr,
			  uint32_t ret_value = (is_integral<typename coord_type::decayed_scalar_type>::value ? 0 : 1)>
	constexpr uint32_t tex_select() {
		return ret_value;
	}
	// returns the texture index corresponding to the used coordinate type
	// with: int/uint coords -> non-normalized coordinates texture+sampler, float coords -> normalized coordinates texture+sampler
	template <typename coord_type, enable_if_t<is_fundamental<coord_type>::value>* = nullptr,
			  uint32_t ret_value = (is_integral<coord_type>::value ? 0 : 1)>
	constexpr uint32_t tex_select() {
		return ret_value;
	}
	
	const_func clang_float4 read_imagef(uint64_t tex, COMPUTE_IMAGE_TYPE type, clang_int1 coord, uint32_t layer = 0, uint32_t sample = 0) asm("floor.read_image.float.i1");
	const_func clang_float4 read_imagef(uint64_t tex, COMPUTE_IMAGE_TYPE type, clang_float1 coord, uint32_t layer = 0, uint32_t sample = 0) asm("floor.read_image.float.f1");
	const_func clang_float4 read_imagef(uint64_t tex, COMPUTE_IMAGE_TYPE type, clang_int2 coord, uint32_t layer = 0, uint32_t sample = 0) asm("floor.read_image.float.i2");
	const_func clang_float4 read_imagef(uint64_t tex, COMPUTE_IMAGE_TYPE type, clang_float2 coord, uint32_t layer = 0, uint32_t sample = 0) asm("floor.read_image.float.f2");
	const_func clang_float4 read_imagef(uint64_t tex, COMPUTE_IMAGE_TYPE type, clang_int3 coord, uint32_t layer = 0, uint32_t sample = 0) asm("floor.read_image.float.i3");
	const_func clang_float4 read_imagef(uint64_t tex, COMPUTE_IMAGE_TYPE type, clang_float3 coord, uint32_t layer = 0, uint32_t sample = 0) asm("floor.read_image.float.f3");
	
	const_func clang_int4 read_imagei(uint64_t tex, COMPUTE_IMAGE_TYPE type, clang_int1 coord, uint32_t layer = 0, uint32_t sample = 0) asm("floor.read_image.int.i1");
	const_func clang_int4 read_imagei(uint64_t tex, COMPUTE_IMAGE_TYPE type, clang_float1 coord, uint32_t layer = 0, uint32_t sample = 0) asm("floor.read_image.int.f1");
	const_func clang_int4 read_imagei(uint64_t tex, COMPUTE_IMAGE_TYPE type, clang_int2 coord, uint32_t layer = 0, uint32_t sample = 0) asm("floor.read_image.int.i2");
	const_func clang_int4 read_imagei(uint64_t tex, COMPUTE_IMAGE_TYPE type, clang_float2 coord, uint32_t layer = 0, uint32_t sample = 0) asm("floor.read_image.int.f2");
	const_func clang_int4 read_imagei(uint64_t tex, COMPUTE_IMAGE_TYPE type, clang_int3 coord, uint32_t layer = 0, uint32_t sample = 0) asm("floor.read_image.int.i3");
	const_func clang_int4 read_imagei(uint64_t tex, COMPUTE_IMAGE_TYPE type, clang_float3 coord, uint32_t layer = 0, uint32_t sample = 0) asm("floor.read_image.int.f3");
	
	const_func clang_uint4 read_imageui(uint64_t tex, COMPUTE_IMAGE_TYPE type, clang_int1 coord, uint32_t layer = 0, uint32_t sample = 0) asm("floor.read_image.uint.i1");
	const_func clang_uint4 read_imageui(uint64_t tex, COMPUTE_IMAGE_TYPE type, clang_float1 coord, uint32_t layer = 0, uint32_t sample = 0) asm("floor.read_image.uint.f1");
	const_func clang_uint4 read_imageui(uint64_t tex, COMPUTE_IMAGE_TYPE type, clang_int2 coord, uint32_t layer = 0, uint32_t sample = 0) asm("floor.read_image.uint.i2");
	const_func clang_uint4 read_imageui(uint64_t tex, COMPUTE_IMAGE_TYPE type, clang_float2 coord, uint32_t layer = 0, uint32_t sample = 0) asm("floor.read_image.uint.f2");
	const_func clang_uint4 read_imageui(uint64_t tex, COMPUTE_IMAGE_TYPE type, clang_int3 coord, uint32_t layer = 0, uint32_t sample = 0) asm("floor.read_image.uint.i3");
	const_func clang_uint4 read_imageui(uint64_t tex, COMPUTE_IMAGE_TYPE type, clang_float3 coord, uint32_t layer = 0, uint32_t sample = 0) asm("floor.read_image.uint.f3");
	
	void write_imagef(uint64_t surf, COMPUTE_IMAGE_TYPE type, clang_int1 coord, uint32_t layer, clang_float4 data) asm("floor.write_image.float.i1");
	void write_imagef(uint64_t surf, COMPUTE_IMAGE_TYPE type, clang_int2 coord, uint32_t layer, clang_float4 data) asm("floor.write_image.float.i2");
	void write_imagef(uint64_t surf, COMPUTE_IMAGE_TYPE type, clang_int3 coord, uint32_t layer, clang_float4 data) asm("floor.write_image.float.i3");
	
	void write_imagei(uint64_t surf, COMPUTE_IMAGE_TYPE type, clang_int1 coord, uint32_t layer, clang_int4 data) asm("floor.write_image.int.i1");
	void write_imagei(uint64_t surf, COMPUTE_IMAGE_TYPE type, clang_int2 coord, uint32_t layer, clang_int4 data) asm("floor.write_image.int.i2");
	void write_imagei(uint64_t surf, COMPUTE_IMAGE_TYPE type, clang_int3 coord, uint32_t layer, clang_int4 data) asm("floor.write_image.int.i3");
	
	void write_imageui(uint64_t surf, COMPUTE_IMAGE_TYPE type, clang_int1 coord, uint32_t layer, clang_uint4 data) asm("floor.write_image.uint.i1");
	void write_imageui(uint64_t surf, COMPUTE_IMAGE_TYPE type, clang_int2 coord, uint32_t layer, clang_uint4 data) asm("floor.write_image.uint.i2");
	void write_imageui(uint64_t surf, COMPUTE_IMAGE_TYPE type, clang_int3 coord, uint32_t layer, clang_uint4 data) asm("floor.write_image.uint.i3");
}

// image read functions
template <COMPUTE_IMAGE_TYPE image_type, typename coord_type,
		  enable_if_t<((has_flag<COMPUTE_IMAGE_TYPE::FLAG_NORMALIZED>(image_type) ||
						(image_type & COMPUTE_IMAGE_TYPE::__DATA_TYPE_MASK) == COMPUTE_IMAGE_TYPE::FLOAT) &&
					   has_flag<COMPUTE_IMAGE_TYPE::READ>(image_type) &&
					   !has_flag<COMPUTE_IMAGE_TYPE::FLAG_MSAA>(image_type))>* = nullptr>
const_func floor_inline_always auto read(const image<image_type>& img, const coord_type& coord, const uint32_t layer = 0) {
	return image_vec_ret_type<image_type, float>::fit(float4::from_clang_vector(cuda_image::read_imagef(img.tex[cuda_image::tex_select<coord_type>()], image_type, cuda_image::convert_coord(coord), layer)));
}

template <COMPUTE_IMAGE_TYPE image_type, typename coord_type,
		  enable_if_t<(!has_flag<COMPUTE_IMAGE_TYPE::FLAG_NORMALIZED>(image_type) &&
					   (image_type & COMPUTE_IMAGE_TYPE::__DATA_TYPE_MASK) == COMPUTE_IMAGE_TYPE::INT &&
					   has_flag<COMPUTE_IMAGE_TYPE::READ>(image_type) &&
					   !has_flag<COMPUTE_IMAGE_TYPE::FLAG_MSAA>(image_type))>* = nullptr>
const_func floor_inline_always auto read(const image<image_type>& img, const coord_type& coord, const uint32_t layer = 0) {
	return image_vec_ret_type<image_type, int32_t>::fit(int4::from_clang_vector(cuda_image::read_imagei(img.tex[cuda_image::tex_select<coord_type>()], image_type, cuda_image::convert_coord(coord), layer)));
}

template <COMPUTE_IMAGE_TYPE image_type, typename coord_type,
		  enable_if_t<(!has_flag<COMPUTE_IMAGE_TYPE::FLAG_NORMALIZED>(image_type) &&
					   (image_type & COMPUTE_IMAGE_TYPE::__DATA_TYPE_MASK) == COMPUTE_IMAGE_TYPE::UINT &&
					   has_flag<COMPUTE_IMAGE_TYPE::READ>(image_type) &&
					   !has_flag<COMPUTE_IMAGE_TYPE::FLAG_MSAA>(image_type))>* = nullptr>
const_func floor_inline_always auto read(const image<image_type>& img, const coord_type& coord, const uint32_t layer = 0) {
	return image_vec_ret_type<image_type, uint32_t>::fit(uint4::from_clang_vector(cuda_image::read_imageui(img.tex[cuda_image::tex_select<coord_type>()], image_type, cuda_image::convert_coord(coord), layer)));
}

template <COMPUTE_IMAGE_TYPE image_type, typename coord_type,
		  enable_if_t<((has_flag<COMPUTE_IMAGE_TYPE::FLAG_NORMALIZED>(image_type) ||
						(image_type & COMPUTE_IMAGE_TYPE::__DATA_TYPE_MASK) == COMPUTE_IMAGE_TYPE::FLOAT) &&
					   has_flag<COMPUTE_IMAGE_TYPE::READ>(image_type) &&
					   has_flag<COMPUTE_IMAGE_TYPE::FLAG_MSAA>(image_type))>* = nullptr>
const_func floor_inline_always auto read(const image<image_type>& img, const coord_type& coord, const uint32_t sample, const uint32_t layer = 0) {
	return image_vec_ret_type<image_type, float>::fit(float4::from_clang_vector(cuda_image::read_imagef(img.tex[cuda_image::tex_select<coord_type>()], image_type, cuda_image::convert_coord(coord), layer, sample)));
}

template <COMPUTE_IMAGE_TYPE image_type, typename coord_type,
		  enable_if_t<(!has_flag<COMPUTE_IMAGE_TYPE::FLAG_NORMALIZED>(image_type) &&
					   (image_type & COMPUTE_IMAGE_TYPE::__DATA_TYPE_MASK) == COMPUTE_IMAGE_TYPE::INT &&
					   has_flag<COMPUTE_IMAGE_TYPE::READ>(image_type) &&
					   has_flag<COMPUTE_IMAGE_TYPE::FLAG_MSAA>(image_type))>* = nullptr>
const_func floor_inline_always auto read(const image<image_type>& img, const coord_type& coord, const uint32_t sample, const uint32_t layer = 0) {
	return image_vec_ret_type<image_type, int32_t>::fit(int4::from_clang_vector(cuda_image::read_imagei(img.tex[cuda_image::tex_select<coord_type>()], image_type, cuda_image::convert_coord(coord), layer, sample)));
}

template <COMPUTE_IMAGE_TYPE image_type, typename coord_type,
		  enable_if_t<(!has_flag<COMPUTE_IMAGE_TYPE::FLAG_NORMALIZED>(image_type) &&
					   (image_type & COMPUTE_IMAGE_TYPE::__DATA_TYPE_MASK) == COMPUTE_IMAGE_TYPE::UINT &&
					   has_flag<COMPUTE_IMAGE_TYPE::READ>(image_type) &&
					   has_flag<COMPUTE_IMAGE_TYPE::FLAG_MSAA>(image_type))>* = nullptr>
const_func floor_inline_always auto read(const image<image_type>& img, const coord_type& coord, const uint32_t sample, const uint32_t layer = 0) {
	return image_vec_ret_type<image_type, uint32_t>::fit(uint4::from_clang_vector(cuda_image::read_imageui(img.tex[cuda_image::tex_select<coord_type>()], image_type, cuda_image::convert_coord(coord), layer, sample)));
}

// image write functions
template <COMPUTE_IMAGE_TYPE image_type, typename coord_type,
		  typename color_type = conditional_t<image_channel_count(image_type) == 1, float, vector_n<float, image_channel_count(image_type)>>,
		  enable_if_t<((has_flag<COMPUTE_IMAGE_TYPE::FLAG_NORMALIZED>(image_type) ||
						(image_type & COMPUTE_IMAGE_TYPE::__DATA_TYPE_MASK) == COMPUTE_IMAGE_TYPE::FLOAT) &&
					   has_flag<COMPUTE_IMAGE_TYPE::WRITE>(image_type) &&
					   !has_flag<COMPUTE_IMAGE_TYPE::FLAG_ARRAY>(image_type))>* = nullptr>
floor_inline_always void write(const image<image_type>& img, const coord_type& coord, const color_type& data) {
	cuda_image::write_imagef(img.surf, image_type, cuda_image::convert_coord(coord), 0, cuda_image::convert_data<float>(data));
}
template <COMPUTE_IMAGE_TYPE image_type, typename coord_type,
		  typename color_type = conditional_t<image_channel_count(image_type) == 1, float, vector_n<float, image_channel_count(image_type)>>,
		  enable_if_t<((has_flag<COMPUTE_IMAGE_TYPE::FLAG_NORMALIZED>(image_type) ||
						(image_type & COMPUTE_IMAGE_TYPE::__DATA_TYPE_MASK) == COMPUTE_IMAGE_TYPE::FLOAT) &&
					   has_flag<COMPUTE_IMAGE_TYPE::WRITE>(image_type) &&
					   has_flag<COMPUTE_IMAGE_TYPE::FLAG_ARRAY>(image_type))>* = nullptr>
floor_inline_always void write(const image<image_type>& img, const coord_type& coord, const uint32_t layer, const color_type& data) {
	cuda_image::write_imagef(img.surf, image_type, cuda_image::convert_coord(coord), layer, cuda_image::convert_data<float>(data));
}

template <COMPUTE_IMAGE_TYPE image_type, typename coord_type,
		  typename color_type = conditional_t<image_channel_count(image_type) == 1, int32_t, vector_n<int32_t, image_channel_count(image_type)>>,
		  enable_if_t<(!has_flag<COMPUTE_IMAGE_TYPE::FLAG_NORMALIZED>(image_type) &&
					   (image_type & COMPUTE_IMAGE_TYPE::__DATA_TYPE_MASK) == COMPUTE_IMAGE_TYPE::INT &&
					   has_flag<COMPUTE_IMAGE_TYPE::WRITE>(image_type) &&
					   !has_flag<COMPUTE_IMAGE_TYPE::FLAG_ARRAY>(image_type))>* = nullptr>
floor_inline_always void write(const image<image_type>& img, const coord_type& coord, const color_type& data) {
	cuda_image::write_imagei(img.surf, image_type, cuda_image::convert_coord(coord), 0, cuda_image::convert_data<int32_t>(data));
}
template <COMPUTE_IMAGE_TYPE image_type, typename coord_type,
		  typename color_type = conditional_t<image_channel_count(image_type) == 1, int32_t, vector_n<int32_t, image_channel_count(image_type)>>,
		  enable_if_t<(!has_flag<COMPUTE_IMAGE_TYPE::FLAG_NORMALIZED>(image_type) &&
					   (image_type & COMPUTE_IMAGE_TYPE::__DATA_TYPE_MASK) == COMPUTE_IMAGE_TYPE::INT &&
					   has_flag<COMPUTE_IMAGE_TYPE::WRITE>(image_type) &&
					   has_flag<COMPUTE_IMAGE_TYPE::FLAG_ARRAY>(image_type))>* = nullptr>
floor_inline_always void write(const image<image_type>& img, const coord_type& coord, const uint32_t layer, const color_type& data) {
	cuda_image::write_imagei(img.surf, image_type, cuda_image::convert_coord(coord), layer, cuda_image::convert_data<int32_t>(data));
}

template <COMPUTE_IMAGE_TYPE image_type, typename coord_type,
		  typename color_type = conditional_t<image_channel_count(image_type) == 1, uint32_t, vector_n<uint32_t, image_channel_count(image_type)>>,
		  enable_if_t<(!has_flag<COMPUTE_IMAGE_TYPE::FLAG_NORMALIZED>(image_type) &&
					   (image_type & COMPUTE_IMAGE_TYPE::__DATA_TYPE_MASK) == COMPUTE_IMAGE_TYPE::UINT &&
					   has_flag<COMPUTE_IMAGE_TYPE::WRITE>(image_type) &&
					   !has_flag<COMPUTE_IMAGE_TYPE::FLAG_ARRAY>(image_type))>* = nullptr>
floor_inline_always void write(const image<image_type>& img, const coord_type& coord, const color_type& data) {
	cuda_image::write_imageui(img.surf, image_type, cuda_image::convert_coord(coord), 0, cuda_image::convert_data<uint32_t>(data));
}
template <COMPUTE_IMAGE_TYPE image_type, typename coord_type,
		  typename color_type = conditional_t<image_channel_count(image_type) == 1, uint32_t, vector_n<uint32_t, image_channel_count(image_type)>>,
		  enable_if_t<(!has_flag<COMPUTE_IMAGE_TYPE::FLAG_NORMALIZED>(image_type) &&
					   (image_type & COMPUTE_IMAGE_TYPE::__DATA_TYPE_MASK) == COMPUTE_IMAGE_TYPE::UINT &&
					   has_flag<COMPUTE_IMAGE_TYPE::WRITE>(image_type) &&
					   has_flag<COMPUTE_IMAGE_TYPE::FLAG_ARRAY>(image_type))>* = nullptr>
floor_inline_always void write(const image<image_type>& img, const coord_type& coord, const uint32_t layer, const color_type& data) {
	cuda_image::write_imageui(img.surf, image_type, cuda_image::convert_coord(coord), layer, cuda_image::convert_data<uint32_t>(data));
}

#endif

#endif
