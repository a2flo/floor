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

#ifndef __FLOOR_COMPUTE_MIP_MAP_MINIFY_HPP__
#define __FLOOR_COMPUTE_MIP_MAP_MINIFY_HPP__

// only add depth image types if these are supported (both read and write)
#if defined(FLOOR_COMPUTE_INFO_HAS_IMAGE_DEPTH_SUPPORT_1) && \
	defined(FLOOR_COMPUTE_INFO_HAS_IMAGE_DEPTH_WRITE_SUPPORT_1) && \
	!defined(FLOOR_COMPUTE_VULKAN) // TODO: vulkan support
#define FLOOR_MINIFY_DEPTH_IMAGE_TYPES(F) \
F(IMAGE_DEPTH, FLOAT) \
F(IMAGE_DEPTH_ARRAY, FLOAT)
#else
#define FLOOR_MINIFY_DEPTH_IMAGE_TYPES(F)
#endif

// list of all supported minification image types (base + sample type)
#define FLOOR_MINIFY_IMAGE_TYPES(F) \
F(IMAGE_1D, FLOAT) \
F(IMAGE_1D, INT) \
F(IMAGE_1D, UINT) \
F(IMAGE_1D_ARRAY, FLOAT) \
F(IMAGE_1D_ARRAY, INT) \
F(IMAGE_1D_ARRAY, UINT) \
F(IMAGE_2D, FLOAT) \
F(IMAGE_2D, INT) \
F(IMAGE_2D, UINT) \
F(IMAGE_2D_ARRAY, FLOAT) \
F(IMAGE_2D_ARRAY, INT) \
F(IMAGE_2D_ARRAY, UINT) \
F(IMAGE_3D, FLOAT) \
F(IMAGE_3D, INT) \
F(IMAGE_3D, UINT) \
FLOOR_MINIFY_DEPTH_IMAGE_TYPES(F)

floor_inline_always static constexpr COMPUTE_IMAGE_TYPE minify_image_base_type(COMPUTE_IMAGE_TYPE image_type) {
	const auto image_base_type = ((image_type & (COMPUTE_IMAGE_TYPE::__DIM_MASK |
												 COMPUTE_IMAGE_TYPE::FLAG_ARRAY |
												 COMPUTE_IMAGE_TYPE::FLAG_DEPTH |
												 COMPUTE_IMAGE_TYPE::FLAG_CUBE |
												 COMPUTE_IMAGE_TYPE::FLAG_MSAA |
												 COMPUTE_IMAGE_TYPE::FLAG_STENCIL)) |
								  // use float sample type if normalized
								  (has_flag<COMPUTE_IMAGE_TYPE::FLAG_NORMALIZED>(image_type) ?
								   COMPUTE_IMAGE_TYPE::FLOAT :
								   (image_type & COMPUTE_IMAGE_TYPE::__DATA_TYPE_MASK)) |
								  // must also attach channel count for depth (IMAGE_DEPTH uses it)
								  (!has_flag<COMPUTE_IMAGE_TYPE::FLAG_DEPTH>(image_type) ?
								   COMPUTE_IMAGE_TYPE::NONE :
								   (image_type & COMPUTE_IMAGE_TYPE::__CHANNELS_MASK)));
	return image_base_type;
}

// only enable the following code on devices or when FLOOR_COMPUTE_HOST_MINIFY is explicitly defined (-> host_image)
#if (defined(FLOOR_COMPUTE) && !defined(FLOOR_COMPUTE_HOST)) || defined(FLOOR_COMPUTE_HOST_MINIFY)

template <bool is_array, typename image_type, typename coord_type, typename int_coord_type, enable_if_t<!is_array>* = nullptr>
floor_inline_always void image_mip_level_read_write(image_type& img, const uint32_t level, const uint32_t,
													const coord_type& coord, const int_coord_type& int_coord) {
	img.write_lod(int_coord, level, img.read_lod_linear(coord, level - 1u));
}
template <bool is_array, typename image_type, typename coord_type, typename int_coord_type, enable_if_t<is_array>* = nullptr>
floor_inline_always void image_mip_level_read_write(image_type& img, const uint32_t level, const uint32_t layer,
													const coord_type& coord, const int_coord_type& int_coord) {
	img.write_lod(int_coord, layer, level, img.read_lod_linear(coord, layer, level - 1u));
}

template <COMPUTE_IMAGE_TYPE image_type, uint32_t image_dim = image_dim_count(image_type)>
floor_inline_always void image_mip_map_minify(image<image_type> img,
											  const uint3& level_size,
											  const float3& inv_prev_level_size,
											  const uint32_t& level,
											  const uint32_t& layer) {
	static_assert(!has_flag<COMPUTE_IMAGE_TYPE::FLAG_MSAA>(image_type), "msaa is not supported!");
	// TODO: support this (can't do this right now, because there is no (2d float coord, face) read function)
	static_assert(!has_flag<COMPUTE_IMAGE_TYPE::FLAG_CUBE>(image_type), "cube map is not supported!");
	
	const auto trimmed_global_id = global_id.trim<image_dim>();
	if((trimmed_global_id >= level_size.trim<image_dim>()).any()) return;
	
	// we generally want to directly sample in between pixels of the previous level
	// e.g., in 1D for a previous level of [0 .. 7] px, global id is in [0 .. 3],
	// and we want to sample between [0, 1] -> 0, [2, 3] -> 1, [4, 5] -> 2, [6, 7] -> 3,
	// which is normalized (to [0, 1]) equal to 1/8, 3/8, 5/8, 7/8
	const auto coord = vector_n<float, image_dim>(trimmed_global_id * 2u + 1u) * inv_prev_level_size.trim<image_dim>();
	image_mip_level_read_write<has_flag<COMPUTE_IMAGE_TYPE::FLAG_ARRAY>(image_type)>(img, level, layer, coord, trimmed_global_id);
}

#define FLOOR_MINIFY_KERNEL(image_type, sample_type) \
kernel void libfloor_mip_map_minify_ ## image_type ## _ ## sample_type (image<(COMPUTE_IMAGE_TYPE::image_type | COMPUTE_IMAGE_TYPE::sample_type | \
																			   ((COMPUTE_IMAGE_TYPE::image_type & COMPUTE_IMAGE_TYPE::__CHANNELS_MASK) == COMPUTE_IMAGE_TYPE::NONE ? \
																				COMPUTE_IMAGE_TYPE::CHANNELS_4 : COMPUTE_IMAGE_TYPE::NONE))> img, \
																		param<uint3> level_size, \
																		param<float3> inv_prev_level_size, \
																		param<uint32_t> level, \
																		param<uint32_t> layer) { \
	image_mip_map_minify<(COMPUTE_IMAGE_TYPE::image_type | \
						  COMPUTE_IMAGE_TYPE::sample_type | \
						  ((COMPUTE_IMAGE_TYPE::image_type & COMPUTE_IMAGE_TYPE::__CHANNELS_MASK) == COMPUTE_IMAGE_TYPE::NONE ? \
						   COMPUTE_IMAGE_TYPE::CHANNELS_4 : COMPUTE_IMAGE_TYPE::NONE))>(img, level_size, inv_prev_level_size, level, layer); \
}

FLOOR_MINIFY_IMAGE_TYPES(FLOOR_MINIFY_KERNEL)

#endif

#endif
