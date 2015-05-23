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

// NOTE: cuda surf call return type is an untyped ("binary") 8-bit, 16-bit, 32-bit or 64-bit value
template <COMPUTE_IMAGE_TYPE image_type, typename = void> struct cuda_surf_texel_data_type {};

// standard types
template <COMPUTE_IMAGE_TYPE image_type>
struct cuda_surf_texel_data_type<image_type, enable_if_t<(!has_flag<COMPUTE_IMAGE_TYPE::FLAG_DEPTH>(image_type) &&
														  ((image_type & COMPUTE_IMAGE_TYPE::__FORMAT_MASK) == COMPUTE_IMAGE_TYPE::FORMAT_2 ||
														   (image_type & COMPUTE_IMAGE_TYPE::__FORMAT_MASK) == COMPUTE_IMAGE_TYPE::FORMAT_4 ||
														   (image_type & COMPUTE_IMAGE_TYPE::__FORMAT_MASK) == COMPUTE_IMAGE_TYPE::FORMAT_8 ||
														   (image_type & COMPUTE_IMAGE_TYPE::__FORMAT_MASK) == COMPUTE_IMAGE_TYPE::FORMAT_3_3_2 ||
														   (image_type & COMPUTE_IMAGE_TYPE::__FORMAT_MASK) == COMPUTE_IMAGE_TYPE::FORMAT_5_5_5 ||
														   (image_type & COMPUTE_IMAGE_TYPE::__FORMAT_MASK) == COMPUTE_IMAGE_TYPE::FORMAT_5_5_5_1 ||
														   (image_type & COMPUTE_IMAGE_TYPE::__FORMAT_MASK) == COMPUTE_IMAGE_TYPE::FORMAT_5_6_5)
														  )>> {
	typedef vector_n<typename image_sized_data_type<image_type, 8u>::type, image_channel_count(image_type)> type;
};
template <COMPUTE_IMAGE_TYPE image_type>
struct cuda_surf_texel_data_type<image_type, enable_if_t<(!has_flag<COMPUTE_IMAGE_TYPE::FLAG_DEPTH>(image_type) &&
														  ((image_type & COMPUTE_IMAGE_TYPE::__FORMAT_MASK) == COMPUTE_IMAGE_TYPE::FORMAT_16 ||
														   (image_type & COMPUTE_IMAGE_TYPE::__FORMAT_MASK) == COMPUTE_IMAGE_TYPE::FORMAT_9_9_9_5 ||
														   (image_type & COMPUTE_IMAGE_TYPE::__FORMAT_MASK) == COMPUTE_IMAGE_TYPE::FORMAT_10 ||
														   (image_type & COMPUTE_IMAGE_TYPE::__FORMAT_MASK) == COMPUTE_IMAGE_TYPE::FORMAT_10_10_10_2 ||
														   (image_type & COMPUTE_IMAGE_TYPE::__FORMAT_MASK) == COMPUTE_IMAGE_TYPE::FORMAT_11_11_10 ||
														   (image_type & COMPUTE_IMAGE_TYPE::__FORMAT_MASK) == COMPUTE_IMAGE_TYPE::FORMAT_12_12_12 ||
														   (image_type & COMPUTE_IMAGE_TYPE::__FORMAT_MASK) == COMPUTE_IMAGE_TYPE::FORMAT_12_12_12_12)
														  )>> {
	typedef vector_n<typename image_sized_data_type<image_type, 16u>::type, image_channel_count(image_type)> type;
};
template <COMPUTE_IMAGE_TYPE image_type>
struct cuda_surf_texel_data_type<image_type, enable_if_t<(!has_flag<COMPUTE_IMAGE_TYPE::FLAG_DEPTH>(image_type) &&
														  (image_type & COMPUTE_IMAGE_TYPE::__FORMAT_MASK) == COMPUTE_IMAGE_TYPE::FORMAT_32)>> {
	typedef vector_n<typename image_sized_data_type<image_type, 32u>::type, image_channel_count(image_type)> type;
};
template <COMPUTE_IMAGE_TYPE image_type>
struct cuda_surf_texel_data_type<image_type, enable_if_t<(!has_flag<COMPUTE_IMAGE_TYPE::FLAG_DEPTH>(image_type) &&
														  (image_type & COMPUTE_IMAGE_TYPE::__FORMAT_MASK) == COMPUTE_IMAGE_TYPE::FORMAT_64)>> {
	typedef vector_n<typename image_sized_data_type<image_type, 64u>::type, image_channel_count(image_type)> type;
};
// depth types
template <COMPUTE_IMAGE_TYPE image_type>
struct cuda_surf_texel_data_type<image_type, enable_if_t<(has_flag<COMPUTE_IMAGE_TYPE::FLAG_DEPTH>(image_type) &&
														  (image_type & COMPUTE_IMAGE_TYPE::__DATA_TYPE_MASK) == COMPUTE_IMAGE_TYPE::UINT &&
														  (image_type & COMPUTE_IMAGE_TYPE::__FORMAT_MASK) == COMPUTE_IMAGE_TYPE::FORMAT_16)>> {
	typedef uint16_t type;
};
template <COMPUTE_IMAGE_TYPE image_type>
struct cuda_surf_texel_data_type<image_type, enable_if_t<(has_flag<COMPUTE_IMAGE_TYPE::FLAG_DEPTH>(image_type) &&
														  (image_type & COMPUTE_IMAGE_TYPE::__DATA_TYPE_MASK) == COMPUTE_IMAGE_TYPE::UINT &&
														  ((image_type & COMPUTE_IMAGE_TYPE::__FORMAT_MASK) == COMPUTE_IMAGE_TYPE::FORMAT_24 ||
														   (image_type & COMPUTE_IMAGE_TYPE::__FORMAT_MASK) == COMPUTE_IMAGE_TYPE::FORMAT_32)
														  )>> {
	typedef uint32_t type;
};
template <COMPUTE_IMAGE_TYPE image_type>
struct cuda_surf_texel_data_type<image_type, enable_if_t<(has_flag<COMPUTE_IMAGE_TYPE::FLAG_DEPTH>(image_type) &&
														  (image_type & COMPUTE_IMAGE_TYPE::__DATA_TYPE_MASK) == COMPUTE_IMAGE_TYPE::FLOAT &&
														  (image_type & COMPUTE_IMAGE_TYPE::__FORMAT_MASK) == COMPUTE_IMAGE_TYPE::FORMAT_32)>> {
	typedef float type;
};
// depth + stencil types
template <COMPUTE_IMAGE_TYPE image_type>
struct cuda_surf_texel_data_type<image_type, enable_if_t<(has_flag<COMPUTE_IMAGE_TYPE::FLAG_DEPTH>(image_type) &&
														  has_flag<COMPUTE_IMAGE_TYPE::FLAG_STENCIL>(image_type) &&
														  (image_type & COMPUTE_IMAGE_TYPE::__DATA_TYPE_MASK) == COMPUTE_IMAGE_TYPE::UINT &&
														  (image_type & COMPUTE_IMAGE_TYPE::__FORMAT_MASK) == COMPUTE_IMAGE_TYPE::FORMAT_24_8)>> {
	typedef pair<uint32_t, uint8_t> type;
};
template <COMPUTE_IMAGE_TYPE image_type>
struct cuda_surf_texel_data_type<image_type, enable_if_t<(has_flag<COMPUTE_IMAGE_TYPE::FLAG_DEPTH>(image_type) &&
														  has_flag<COMPUTE_IMAGE_TYPE::FLAG_STENCIL>(image_type) &&
														  (image_type & COMPUTE_IMAGE_TYPE::__DATA_TYPE_MASK) == COMPUTE_IMAGE_TYPE::FLOAT &&
														  (image_type & COMPUTE_IMAGE_TYPE::__FORMAT_MASK) == COMPUTE_IMAGE_TYPE::FORMAT_32_8)>> {
	typedef pair<float, uint8_t> type;
};

// cuda specific
template <COMPUTE_IMAGE_TYPE itype>
static constexpr bool is_surf() { return (itype & COMPUTE_IMAGE_TYPE::WRITE) == COMPUTE_IMAGE_TYPE::WRITE; }

template <COMPUTE_IMAGE_TYPE itype>
static constexpr bool is_tex() { return !is_surf<itype>(); }

template <COMPUTE_IMAGE_TYPE itype>
static constexpr bool is_uint() { return (itype & COMPUTE_IMAGE_TYPE::__DATA_TYPE_MASK) == COMPUTE_IMAGE_TYPE::UINT; }

template <COMPUTE_IMAGE_TYPE itype>
static constexpr bool is_int() { return (itype & COMPUTE_IMAGE_TYPE::__DATA_TYPE_MASK) == COMPUTE_IMAGE_TYPE::INT; }

template <COMPUTE_IMAGE_TYPE itype>
static constexpr bool is_float() { return (itype & COMPUTE_IMAGE_TYPE::__DATA_TYPE_MASK) == COMPUTE_IMAGE_TYPE::FLOAT; }


template <COMPUTE_IMAGE_TYPE image_type, typename = void> struct image {};
template <COMPUTE_IMAGE_TYPE image_type>
struct image<image_type, enable_if_t<has_flag<COMPUTE_IMAGE_TYPE::READ>(image_type) && !has_flag<COMPUTE_IMAGE_TYPE::READ_WRITE>(image_type)>> {
	static constexpr COMPUTE_IMAGE_TYPE type { image_type };
	typedef typename image_texel_data_type<image_type>::type tex_data_type;
	
	const uint64_t tex;
};
template <COMPUTE_IMAGE_TYPE image_type>
struct image<image_type, enable_if_t<!has_flag<COMPUTE_IMAGE_TYPE::READ_WRITE>(image_type) && has_flag<COMPUTE_IMAGE_TYPE::WRITE>(image_type)>> {
	static constexpr COMPUTE_IMAGE_TYPE type { image_type };
	typedef typename cuda_surf_texel_data_type<image_type>::type surf_data_type;
	
	const uint64_t surf;
};
template <COMPUTE_IMAGE_TYPE image_type>
struct image<image_type, enable_if_t<has_flag<COMPUTE_IMAGE_TYPE::READ_WRITE>(image_type)>> {
	static constexpr COMPUTE_IMAGE_TYPE type { image_type };
	typedef typename image_texel_data_type<image_type>::type tex_data_type;
	typedef typename cuda_surf_texel_data_type<image_type>::type surf_data_type;
	
	// NOTE: this needs to packed like this, so that we don't get weird optimization behavior when one isn't used
	union {
		struct {
			const uint64_t tex;
			const uint64_t surf;
		};
		const uint2 surf_tex_id;
	};
};

#define ro_image read_only cuda_ro_image
template <COMPUTE_IMAGE_TYPE image_type> using cuda_ro_image =
image<image_type & (~COMPUTE_IMAGE_TYPE::FLAG_NO_SAMPLER) | COMPUTE_IMAGE_TYPE::READ>;

#define wo_image write_only cuda_wo_image
template <COMPUTE_IMAGE_TYPE image_type> using cuda_wo_image =
image<image_type & (~COMPUTE_IMAGE_TYPE::READ) | COMPUTE_IMAGE_TYPE::WRITE>;

#define rw_image read_write cuda_rw_image
template <COMPUTE_IMAGE_TYPE image_type> using cuda_rw_image =
image<image_type & (~COMPUTE_IMAGE_TYPE::WRITE) | COMPUTE_IMAGE_TYPE::READ_WRITE>;

#if 0
//
template <typename ret_type,
		  COMPUTE_IMAGE_TYPE image_type,
		  enable_if_t<((is_same<ret_type, uchar4>::value ||
					   is_same<ret_type, char4>::value) &&
					   is_surf<image_type>())>* = nullptr>
floor_inline_always auto read(const image<image_type>& img, const int2& coord) {
	ret_type ret;
	asm("suld.b.2d.v4.b8.clamp { %0, %1, %2, %3 }, [%4, { %5, %6 }];" :
		"=r"(ret.x), "=r"(ret.y), "=r"(ret.z), "=r"(ret.w) :
		"l"(img.surf), "r"(coord.x * 4), "r"(coord.y));
	return ret;
}

// TODO: half float?
template <typename ret_type,
		  COMPUTE_IMAGE_TYPE image_type,
		  enable_if_t<is_same<ret_type, float4>::value && is_surf<image_type>()>* = nullptr>
floor_inline_always auto read(const image<image_type>& img, const int2& coord) {
	ret_type ret;
	asm("suld.b.2d.v4.b32.clamp { %0, %1, %2, %3 }, [%4, { %5, %6 }];" :
		"=f"(ret.x), "=f"(ret.y), "=f"(ret.z), "=f"(ret.w) :
		"l"(img.surf), "r"(coord.x * 32), "r"(coord.y));
	return ret;
}

template <COMPUTE_IMAGE_TYPE image_type,
		  typename ret_type = typename cuda_surf_texel_data_type<image_type>::type,
		  enable_if_t<is_same<ret_type, float4>::value && is_surf<image_type>()>* = nullptr>
floor_inline_always auto read(const image<image_type>& img, const int2& coord) {
	ret_type ret;
	asm("suld.b.2d.v4.b32.clamp { %0, %1, %2, %3 }, [%4, { %5, %6 }];" :
		"=f"(ret.x), "=f"(ret.y), "=f"(ret.z), "=f"(ret.w) :
		"l"(img.surf), "r"(coord.x * 32), "r"(coord.y));
	return ret;
}
#endif

template <COMPUTE_IMAGE_TYPE image_type, typename surf_data_type = uchar4,
enable_if_t<is_same<surf_data_type, uchar4>::value>* = nullptr>
floor_inline_always void write(const image<image_type>& img, const int2& coord, const surf_data_type& data) {
	asm("sust.b.2d.v4.b8.clamp [%0, { %1, %2 }], { %3, %4, %5, %6 };" : :
		"l"(img.surf), "r"(coord.x * 4), "r"(coord.y),
		"r"(data.x), "r"(data.y), "r"(data.z), "r"(data.w));
}

template <COMPUTE_IMAGE_TYPE image_type, typename surf_data_type,
enable_if_t<is_same<surf_data_type, float4>::value>* = nullptr>
floor_inline_always void write(const image<image_type>& img, const int2& coord, const surf_data_type& unscaled_data) {
	uchar4 data = unscaled_data * 255.0f;
	asm("sust.b.2d.v4.b8.clamp [%0, { %1, %2 }], { %3, %4, %5, %6 };" : :
		"l"(img.surf), "r"(coord.x * 4), "r"(coord.y),
		"r"(data.x), "r"(data.y), "r"(data.z), "r"(data.w));
}

#if 0
// will only work with 32-bit uint types?
template <typename ret_type = tex_data_type, COMPUTE_IMAGE_TYPE itype = image_type, enable_if_t<is_uint<itype>() && !has_flag<COMPUTE_IMAGE_TYPE::FLAG_NO_SAMPLER>(itype)>* = nullptr>
floor_inline_always auto read(int2 coord) const {
	uint4 ret;
	asm("tex.2d.v4.u32.s32 { %0, %1, %2, %3 }, [%4, { %5, %6 }];" :
		"=r"(ret.x), "=r"(ret.y), "=r"(ret.z), "=r"(ret.w) :
		"l"(surf), "r"(coord.x), "r"(coord.y));
	return ret;
}
#endif

template <typename ret_type = float4, COMPUTE_IMAGE_TYPE image_type,
		  enable_if_t<is_same<ret_type, float4>::value && has_flag<COMPUTE_IMAGE_TYPE::READ>(image_type)>* = nullptr>
floor_inline_always ret_type read(const image<image_type>& img, int2 coord) {
	ret_type ret;
	asm("tex.2d.v4.f32.s32 { %0, %1, %2, %3 }, [%4, { %5, %6 }];" :
		"=f"(ret.x), "=f"(ret.y), "=f"(ret.z), "=f"(ret.w) :
		"l"(img.tex), "r"(coord.x), "r"(coord.y));
	return ret;
}
template <typename ret_type, COMPUTE_IMAGE_TYPE image_type,
		  enable_if_t<is_same<ret_type, uchar4>::value && has_flag<COMPUTE_IMAGE_TYPE::READ>(image_type)>* = nullptr>
floor_inline_always ret_type read(const image<image_type>& img, int2 coord) {
	float4 ret;
	asm("tex.2d.v4.f32.s32 { %0, %1, %2, %3 }, [%4, { %5, %6 }];" :
		"=f"(ret.x), "=f"(ret.y), "=f"(ret.z), "=f"(ret.w) :
		"l"(img.tex), "r"(coord.x), "r"(coord.y));
	return uchar4 { ret * 255.0f };
}

#endif

#endif
