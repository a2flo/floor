/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2019 Florian Ziesche
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

#ifndef __FLOOR_GRAPHICS_GRAPHICS_PIPELINE_HPP__
#define __FLOOR_GRAPHICS_GRAPHICS_PIPELINE_HPP__

#include <floor/compute/compute_kernel.hpp>
#include <floor/compute/device/image_types.hpp>

enum class PRIMITIVE {
	POINT,
	LINE,
	TRIANGLE,
	TRIANGLE_STRIP,
};

enum class CULL_MODE {
	NONE,
	BACK,
	FRONT,
};

enum class FRONT_FACE {
	CLOCKWISE,
	COUNTER_CLOCKWISE,
};

enum class DEPTH_COMPARE {
	NEVER,
	LESS,
	EQUAL,
	LESS_OR_EQUAL,
	GREATER,
	NOT_EQUAL,
	GREATER_OR_EQUAL,
	ALWAYS,
};

enum class BLEND_FACTOR {
	//! constants
	ZERO,
	ONE,
	
	//! color modes
	SRC_COLOR,
	ONE_MINUS_SRC_COLOR,
	DST_COLOR,
	ONE_MINUS_DST_COLOR,
	
	//! alpha modes
	SRC_ALPHA,
	ONE_MINUS_SRC_ALPHA,
	DST_ALPHA,
	ONE_MINUS_DST_ALPHA,
	SRC_ALPHA_SATURATE,
	
	//! with constant values
	BLEND_COLOR,
	ONE_MINUS_BLEND_COLOR,
	BLEND_ALPHA,
	ONE_MINUE_BLEND_ALPHA,
};

enum class BLEND_OP {
	ADD,
	SUB,
	REV_SUB,
	MIN,
	MAX,
};

//! full pipeline description used to create pipeline objects
struct render_pipeline_description {
	//! shaders for this pipeline
	const compute_kernel* vertex_shader { nullptr };
	const compute_kernel* fragment_shader { nullptr };
	
	//! to be rendered primitive type
	PRIMITIVE primitive { PRIMITIVE::TRIANGLE };
	
	//! geometry culling mode
	CULL_MODE cull_mode { CULL_MODE::BACK };
	
	//! geometry front facing order
	FRONT_FACE front_face { FRONT_FACE::COUNTER_CLOCKWISE };
	
	//! render viewport
	//! NOTE: if set to ~0u, will cover the whole screen
	uint2 viewport { ~0u, ~0u };
	
	//! scissor testing / only render within an area of the screen
	struct scissor_t {
		//! offset within the viewport, (0, 0) is left top
		uint2 offset { 0, 0 };
		//! (width, height) extent of the scissor area
		//! NOTE: if set to ~0u, extent will be set to cover the whole viewport
		uint2 extent { ~0u, ~0u };
	} scissor;
	
	//! depth testing
	//! NOTE: depth testing is implicitly always enabled, set to DEPTH_COMPARE::ALWAYS to disable
	struct depth_t {
		//! flag if depth should be written to the depth attachment
		bool write { true };
		
		//! [min, max] range of the depth stored in the depth attachment
		//! NOTE: depth will be clamped to this range
		float2 range { 0.0f, 1.0f };
		
		//! depth test compare mode, i.e. pass fragment if "current depth <compare> stored depth" is true
		//! NOTE: "NEVER" will never pass any fragment regardless of stored depth, "ALWAYS" will always pass regardless of stored depth
		DEPTH_COMPARE compare { DEPTH_COMPARE::LESS };
	} depth;
	
	//! global blend constants (apply for all attachments that have blending enabled)
	struct blend_t {
		//! constant color used when BLEND_FACTOR is BLEND_COLOR or ONE_MINUS_BLEND_COLOR
		float3 constant_color;
		//! constant alpha used when BLEND_FACTOR is BLEND_ALPHA or ONE_MINUS_BLEND_ALPHA
		float constant_alpha { 0.0f };
	} blend;
	
	//! per-attachment blend state
	struct attachment_blend_t {
		//! flag if blending should be performed
		bool enable { false };
		//! RGBA write mask (default: enable all channels)
		bool4 write_mask { true, true, true, true };
		
		//! blend factor applied to the source color
		BLEND_FACTOR src_color_factor { BLEND_FACTOR::ONE };
		//! blend factor applied to the destination color
		BLEND_FACTOR dst_color_factor { BLEND_FACTOR::ONE };
		//! blend operation performed on the color results
		BLEND_OP color_blend_op { BLEND_OP::ADD };
		
		//! blend factor applied to the source alpha
		BLEND_FACTOR src_alpha_factor { BLEND_FACTOR::ONE };
		//! blend factor applied to the destination alpha
		BLEND_FACTOR dst_alpha_factor { BLEND_FACTOR::ONE };
		//! blend operation performed on the alpha results
		BLEND_OP alpha_blend_op { BLEND_OP::ADD };
	};
	
	//! color attachment state
	struct color_attachment_t {
		//! base pixel format of the attachment
		//! requires: FORMAT, CHANNELS, DATA_TYPE
		//! optional: LAYOUT, COMPRESSION, FLAG_NORMALIZED, FLAG_SRGB
		//! e.g.: specify BGRA8UI_NORM or RGBA16F
		COMPUTE_IMAGE_TYPE format { COMPUTE_IMAGE_TYPE::NONE };
		//! blend state of this attachment
		attachment_blend_t blend;
	};
	vector<color_attachment_t> color_attachments;
	
	//! depth attachment state
	struct depth_attachment_t {
		//! base pixel format of the depth attachment
		//! requires: FLAG_DEPTH, FORMAT, CHANNELS, DATA_TYPE
		//! optional: FLAG_STENCIL (not supported yet)
		//! NOTE: no depth attachment when NONE (default)
		//! e.g.: specify D32F or D24
		COMPUTE_IMAGE_TYPE format { COMPUTE_IMAGE_TYPE::NONE };
	};
	depth_attachment_t depth_attachment;
	
};

//! pipeline object used for rendering with graphics_renderer
//! NOTE: this is costly to create, try to avoid doing this at run-time, prefer creation during init
class graphics_pipeline {
public:
	graphics_pipeline(const render_pipeline_description& pipeline_desc_);
	virtual ~graphics_pipeline();
	
	//! returns the description of this pipeline
	const render_pipeline_description& get_description() const {
		return pipeline_desc;
	}
	
	//! returns true if this pipeline is in a valid state
	bool is_valid() const {
		return valid;
	}
	
protected:
	const render_pipeline_description pipeline_desc;
	bool valid { false };
	
};

#endif
