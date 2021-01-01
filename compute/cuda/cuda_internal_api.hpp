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

#ifndef __FLOOR_CUDA_INTERNAL_API_HPP__
#define __FLOOR_CUDA_INTERNAL_API_HPP__

#include <floor/core/essentials.hpp>

#if !defined(FLOOR_NO_CUDA)

typedef struct _cu_device_obj* cu_device_obj;
typedef struct _cu_sampler_pool* cu_sampler_pool;

// NOTE: do *not* make use of this, this is just for informational purposes
//       struct contents/sizes are different for each cuda version on each os
#if defined(__APPLE__)
// cuda 7.5 on os x: 0xB0 bytes
struct _cu_sampler_pool {
	cu_context ctx;
	uint32_t max_sampler_count;
	int32_t _unknown_1;
	int64_t _unknown_2;
	int64_t _unknown_3;
	uint32_t samplers_in_use;
	int32_t _unknown_4;
	void* tex_index_pool;
	void* data[16];
};
static_assert(sizeof(_cu_sampler_pool) == 0xB0, "invalid _cu_sampler_pool size");

// cuda 7.5 on os x: 0x1F10 bytes
struct _cu_context {
	int32_t ctx_state;
	int32_t _unknown_1;
	void* _unknown_2;
	pthread_mutex_t mtx;
	void* _unknown_3;
	void* _unknown_4;
	void* _unknown_5;
	pthread_mutex_t mtx2;
	void* _unknown_6;
	void* _unknown_7;
	void* _unknown_8;
	void* _unknown_9;
	void* _unknown_10;
	void* _unknown_11;
	cu_device_obj device;
	void* data_1[94];
	cu_sampler_pool sampler_pool;
	void* data_2[871];
};
static_assert(sizeof(_cu_context) == 0x1F10, "invalid _cu_context size");
#endif

union CU_SAMPLER_TYPE {
	// same as host-side metal and vulkan
	enum class COMPARE_FUNCTION : uint32_t {
		NEVER				= 0,
		LESS				= 1,
		EQUAL				= 2,
		LESS_OR_EQUAL		= 3,
		GREATER				= 4,
		NOT_EQUAL			= 5,
		GREATER_OR_EQUAL	= 6,
		ALWAYS				= 7,
	};
	
	struct {
		struct {
			// TODO: coord_mode? (0 = non-normalized, 1 = normalized)
			uint32_t address_mode : 9;
			uint32_t _unknown_1 : 1;
			COMPARE_FUNCTION compare_function : 3;
			uint32_t has_anisotropic : 1;
			uint32_t _unknown_3 : 6;
			uint32_t anisotropic : 3;
			uint32_t _unknown_4 : 9;
		};
		struct {
			uint32_t filter_1 : 2; // 1 = nearest, 2 = linear
			uint32_t _unknown_5 : 2;
			uint32_t filter_2 : 2; // 1 = nearest, 2 = linear
			uint32_t _unknown_6 : 26;
		};
	};
	struct {
		uint32_t low { 0 };
		uint32_t high { 0 };
	};
};
static_assert(sizeof(CU_SAMPLER_TYPE) == sizeof(uint64_t), "invalid sampler size");
static_assert(alignof(CU_SAMPLER_TYPE) == 4, "invalid sampler alignment");

template <uint32_t cuda_version>
struct cu_texture_ref_template {
	size_t _init_unknown_1;
	cu_context ctx;
	int32_t _init_unknown_2;
	// -- x64: 4 bytes padding
	const char* identifier_str;
	uint32_t is_tex_object;
	int32_t _unknown_4;
	uint32_t type;
	int32_t _unknown_5;
	cu_device_ptr device_ptr;
	int32_t slice_size_2d;
	int32_t _unknown_6;
	cu_array array_ptr;
	cu_texture_ref array_next_texture;
	cu_texture_ref array_prev_texture;
	cu_mip_mapped_array mip_array_ptr;
	uint32_t format;
	uint32_t channel_count;
	uint32_t dim_x;
	uint32_t dim_y;
	uint32_t dim_z;
	uint32_t pitch_in_bytes;
	uint32_t has_no_gather;
	// -- x64: 4 bytes padding
	uint64_t array_offset;
	uint32_t first_mip_level;
	uint32_t last_mip_level;
	uint32_t has_rsrc_view;
	// -- x64: 4 bytes padding
	cu_resource_view_descriptor view_desc;
	uint32_t address_mode[3];
	uint32_t filter_mode;
	uint32_t mip_filter_mode;
	float mip_level_bias;
	float mip_level_clamp_min;
	float mip_level_clamp_max;
	uint32_t max_anisotropic;
	float4 border_color[cuda_version >= 8000 ? 1 : 0]; // cuda 8.0+
	// -- x64: 4 bytes padding
	size_t _init_unknown_3;
	uint32_t flags;
	uint32_t is_dirty;
	uint32_t _sampler1[8]; // always 32 bytes
	CU_SAMPLER_TYPE sampler_enum; // with _sampler2 always 32 bytes
	uint32_t _sampler2[6];
	uint32_t texture_object[2]; // low = texture id, high = sampler id
	size_t _unknown_11;
	size_t _unknown_12;
	void* function_ptr;
	void* _unknown_obj_ref_dep2;
	void* _unknown_obj_ref_dep;
};
using cu_texture_ref_75 = cu_texture_ref_template<7050>;
using cu_texture_ref_80 = cu_texture_ref_template<8000>;

// check if sizes/offsets are correct
static_assert(sizeof(cu_texture_ref_75) == 0x1B0, "invalid cu_texture_ref size");
static_assert(sizeof(cu_texture_ref_80) == 0x1C0, "invalid cu_texture_ref size");
static_assert(offsetof(cu_texture_ref_75, sampler_enum) == 352, "invalid sampler enum offset");
static_assert(offsetof(cu_texture_ref_80, sampler_enum) == 368, "invalid sampler enum offset");

#endif

#endif
